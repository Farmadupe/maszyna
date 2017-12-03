﻿/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#include "stdafx.h"

#include "audiorenderer.h"
#include "sound.h"
#include "globals.h"
#include "logs.h"

namespace audio {

openal_renderer renderer;

float const EU07_SOUND_CUTOFFRANGE { 3000.f }; // 2750 m = max expected emitter spawn range, plus safety margin

// starts playback of queued buffers
void
openal_source::play() {

    if( id == audio::null_resource ) { return; } // no implementation-side source to match, no point

    ::alGetError(); // pop the error stack
    ::alSourcePlay( id );
    is_playing = (
        ::alGetError() == AL_NO_ERROR ?
            true :
            false );
}

// stops the playback
void
openal_source::stop() {

    if( id == audio::null_resource ) { return; } // no implementation-side source to match, no point

    loop( false );
    // NOTE: workaround for potential edge cases where ::alSourceStop() doesn't set source which wasn't yet started to AL_STOPPED
    int state;
    ::alGetSourcei( id, AL_SOURCE_STATE, &state );
    if( state == AL_INITIAL ) {
        play();
    }
    ::alSourceStop( id );
    is_playing = false;
}

// updates state of the source
void
openal_source::update( double const Deltatime ) {

    update_deltatime = Deltatime; // cached for time-based processing of data from the controller

    if( id != audio::null_resource ) {

        ::alGetSourcei( id, AL_BUFFERS_PROCESSED, &buffer_index );
        // for multipart sounds trim away processed sources until only one remains, the last one may be set to looping by the controller
        ALuint bufferid;
        while( ( buffer_index > 0 )
            && ( buffers.size() > 1 ) ) {
            ::alSourceUnqueueBuffers( id, 1, &bufferid );
            buffers.erase( std::begin( buffers ) );
            --buffer_index;
        }

        int state;
        ::alGetSourcei( id, AL_SOURCE_STATE, &state );
        is_playing = ( state == AL_PLAYING );
    }

    // request instructions from the controller
    controller->update( *this );
}

// configures state of the source to match the provided set of properties
void
openal_source::sync_with( sound_properties const &State ) {

    if( id == audio::null_resource ) {
        // no implementation-side source to match, return sync error so the controller can clean up on its end
        is_synced = false;
        return;
    }
/*
    // velocity
    // not used yet
    glm::vec3 const velocity { ( State.location - properties.location ) / update_deltatime };
*/
    // location
    properties.location = State.location;
    sound_distance = properties.location - glm::dvec3 { Global::pCameraPosition };
    if( sound_range > 0 ) {
        // range cutoff check
        auto const cutoffrange = (
            is_multipart ?
                EU07_SOUND_CUTOFFRANGE : // we keep multi-part sounds around longer, to minimize restarts as the sounds get out and back in range
                sound_range * 7.5f );
        if( glm::length2( sound_distance ) > std::min( ( cutoffrange * cutoffrange ), ( EU07_SOUND_CUTOFFRANGE * EU07_SOUND_CUTOFFRANGE ) ) ) {
            stop();
            is_synced = false; // flag sync failure for the controller
            return;
        }
    }
    if( sound_range >= 0 ) {
        ::alSourcefv( id, AL_POSITION, glm::value_ptr( sound_distance ) );
    }
    else {
        // sounds with 'unlimited' range are positioned on top of the listener
        ::alSourcefv( id, AL_POSITION, glm::value_ptr( glm::vec3() ) );
    }
    // gain
    if( ( State.placement_stamp != properties.placement_stamp )
     || ( State.base_gain != properties.base_gain ) ) {
        // gain value has changed
        properties.base_gain = State.base_gain;
        properties.placement_gain = State.placement_gain;
        properties.placement_stamp = State.placement_stamp;

        ::alSourcef( id, AL_GAIN, properties.base_gain * properties.placement_gain * Global::AudioVolume );
    }
    // pitch
    if( State.base_pitch != properties.base_pitch ) {
        // pitch value has changed
        properties.base_pitch = State.base_pitch;

        ::alSourcef( id, AL_PITCH, properties.base_pitch * pitch_variation );
    }
    is_synced = true;
}

// sets max audible distance for sounds emitted by the source
void
openal_source::range( float const Range ) {

    // NOTE: we cache actual specified range, as we'll be giving 'unlimited' range special treatment
    sound_range = Range;

    if( id == audio::null_resource ) { return; } // no implementation-side source to match, no point

    auto const range { (
        Range >= 0 ?
            Range :
            5 ) }; // range of -1 means sound of unlimited range, positioned at the listener
    ::alSourcef( id, AL_REFERENCE_DISTANCE, range * ( 1.f / 16.f ) );
    ::alSourcef( id, AL_ROLLOFF_FACTOR, 1.5f );
}

// sets modifier applied to the pitch of sounds emitted by the source
void
openal_source::pitch( float const Pitch ) {

    pitch_variation = Pitch;
    // invalidate current pitch value to enforce change of next syns
    properties.base_pitch = -1.f;
}

// toggles looping of the sound emitted by the source
void
openal_source::loop( bool const State ) {

    if( id == audio::null_resource ) { return; } // no implementation-side source to match, no point
    if( is_looping == State ) { return; }

    is_looping = State;
    ::alSourcei(
        id,
        AL_LOOPING,
        ( State ?
            AL_TRUE :
            AL_FALSE ) );
}

// releases bound buffers and resets state of the class variables
// NOTE: doesn't release allocated implementation-side source
void
openal_source::clear() {

    if( id != audio::null_resource ) {
        // unqueue bound buffers:
        // ensure no buffer is in use...
        stop();
        // ...prepare space for returned ids of unqueued buffers (not that we need that info)...
        std::vector<ALuint> bufferids;
        bufferids.resize( buffers.size() );
        // ...release the buffers...
        ::alSourceUnqueueBuffers( id, bufferids.size(), bufferids.data() );
    }
    // ...and reset reset the properties, except for the id of the allocated source
    // NOTE: not strictly necessary since except for the id the source data typically get discarded in next step
    auto const sourceid { id };
    *this = openal_source();
    id = sourceid;
}



openal_renderer::~openal_renderer() {

    ::alcMakeContextCurrent( nullptr );

    if( m_context != nullptr ) { ::alcDestroyContext( m_context ); }
    if( m_device != nullptr )  { ::alcCloseDevice( m_device ); }
}

audio::buffer_handle
openal_renderer::fetch_buffer( std::string const &Filename ) {

    return m_buffers.create( Filename );
}

// provides direct access to a specified buffer
audio::openal_buffer const &
openal_renderer::buffer( audio::buffer_handle const Buffer ) const {

    return m_buffers.buffer( Buffer );
}

// initializes the service
bool
openal_renderer::init() {

    if( true == m_ready ) {
        // already initialized and enabled
        return true;
    }
    if( false == init_caps() ) {
        // basic initialization failed
        return false;
    }
    //
//    ::alDistanceModel( AL_LINEAR_DISTANCE );
    ::alDistanceModel( AL_INVERSE_DISTANCE_CLAMPED );
    ::alListenerf( AL_GAIN, clamp( Global::AudioVolume, 1.f, 4.f ) );
    // all done
    m_ready = true;
    return true;
}

// schedules playback of specified sample, under control of the specified emitter
void
openal_renderer::insert( sound_source *Controller, audio::buffer_handle const Sound ) {

    audio::openal_source::buffer_sequence buffers { Sound };
    return
        insert(
            Controller,
            std::begin( buffers ), std::end( buffers ) );
}

// removes from the queue all sounds controlled by the specified sound emitter
void
openal_renderer::erase( sound_source const *Controller ) {

    auto source { std::begin( m_sources ) };
    while( source != std::end( m_sources ) ) {
        if( source->controller == Controller ) {
            // if the controller is the one specified, kill it
            source->clear();
            if( source->id != audio::null_resource ) {
                // keep around functional sources, but no point in doing it with the above-the-limit ones
                m_sourcespares.push( source->id );
            }
            source = m_sources.erase( source );
        }
        else {
            // otherwise proceed through the list normally
            ++source;
        }
    }
}

// updates state of all active emitters
void
openal_renderer::update( double const Deltatime ) {

    // update listener
    glm::dmat4 cameramatrix;
    Global::pCamera->SetMatrix( cameramatrix );
    auto rotationmatrix { glm::mat3{ cameramatrix } };
    glm::vec3 const orientation[] = {
        glm::vec3{ 0, 0,-1 } * rotationmatrix ,
        glm::vec3{ 0, 1, 0 } * rotationmatrix };
    ::alListenerfv( AL_ORIENTATION, reinterpret_cast<ALfloat const *>( orientation ) );
/*
    glm::dvec3 const listenerposition { Global::pCameraPosition };
    // not used yet
    glm::vec3 const velocity { ( listenerposition - m_listenerposition ) / Deltatime };
    m_listenerposition = listenerposition;
*/

    // update active emitters
    auto source { std::begin( m_sources ) };
    while( source != std::end( m_sources ) ) {
        // update each source
        source->update( Deltatime );
        // if after the update the source isn't playing, put it away on the spare stack, it's done
        if( false == source->is_playing ) {
            source->clear();
            if( source->id != audio::null_resource ) {
                // keep around functional sources, but no point in doing it with the above-the-limit ones
                m_sourcespares.push( source->id );
            }
            source = m_sources.erase( source );
        }
        else {
            // otherwise proceed through the list normally
            ++source;
        }
    }
}

// returns an instance of implementation-side part of the sound emitter
audio::openal_source
openal_renderer::fetch_source() {

    audio::openal_source newsource;
    if( false == m_sourcespares.empty() ) {
        // reuse (a copy of) already allocated source
        newsource.id = m_sourcespares.top();
        m_sourcespares.pop();
    }
    if( newsource.id == audio::null_resource ) {
        // if there's no source to reuse, try to generate a new one
        ::alGenSources( 1, &( newsource.id ) );
    }
    if( newsource.id == audio::null_resource ) {
        // if we still don't have a working source, see if we can sacrifice an already active one
        // under presumption it's more important to play new sounds than keep the old ones going
        // TBD, TODO: for better results we could use range and/or position for the new sound
        // to better weight whether the new sound is really more important
        auto leastimportantsource { std::end( m_sources ) };
        auto leastimportantweight { std::numeric_limits<float>::max() };

        for( auto source { std::begin( m_sources ) }; source != std::cend( m_sources ); ++source ) {

            if( ( source->id == audio::null_resource )
             || ( true == source->is_multipart )
             || ( false == source->is_playing ) ) {

                continue;
            }
            auto const sourceweight { (
                source->sound_range > 0 ?
                    ( source->sound_range * source->sound_range ) / ( glm::length2( source->sound_distance ) + 1 ) :
                    std::numeric_limits<float>::max() ) };
            if( sourceweight < leastimportantweight ) {
                leastimportantsource = source;
                leastimportantweight = sourceweight;
            }
        }
        if( ( leastimportantsource != std::end( m_sources ) )
         && ( leastimportantweight < 1.f ) ) {
            // only accept the candidate if it's outside of its nominal hearing range
            leastimportantsource->stop();
            leastimportantsource->update( 0 ); // HACK: a roundabout way to notify the controller its emitter has stopped
            leastimportantsource->clear();
            // we should be now free to grab the id and get rid of the remains
            newsource.id = leastimportantsource->id;
            m_sources.erase( leastimportantsource );
        }
    }

    return newsource;
}

bool
openal_renderer::init_caps() {

    // NOTE: default value of audio renderer variable is empty string, meaning argument of NULL i.e. 'preferred' device
    m_device = ::alcOpenDevice( Global::AudioRenderer.c_str() );
    if( m_device == nullptr ) {
        ErrorLog( "Failed to obtain audio device" );
        return false;
    }

    ALCint versionmajor, versionminor;
    ::alcGetIntegerv( m_device, ALC_MAJOR_VERSION, 1, &versionmajor );
    ::alcGetIntegerv( m_device, ALC_MINOR_VERSION, 1, &versionminor );
    auto const oalversion { std::to_string( versionmajor ) + "." + std::to_string( versionminor ) };

    WriteLog(
        "Audio Renderer: " + std::string { (char *)::alcGetString( m_device, ALC_DEVICE_SPECIFIER ) }
        + " OpenAL Version: " + oalversion );

    WriteLog( "Supported extensions: " + std::string{ (char *)::alcGetString( m_device, ALC_EXTENSIONS ) } );

    m_context = ::alcCreateContext( m_device, nullptr );
    if( m_context == nullptr ) {
        ErrorLog( "Failed to create audio context" );
        return false;
    }

    return ( ::alcMakeContextCurrent( m_context ) == AL_TRUE );
}

} // audio

//---------------------------------------------------------------------------
