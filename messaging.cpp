﻿/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#include "stdafx.h"
#include "messaging.h"

#include "globals.h"
#include "simulation.h"
#include "ground.h"
#include "mtable.h"
#include "logs.h"

extern "C"
{
    GLFWAPI HWND glfwGetWin32Window( GLFWwindow* window ); //m7todo: potrzebne do directsound
}

namespace multiplayer {

#ifdef _WINDOWS
void
Navigate(std::string const &ClassName, UINT Msg, WPARAM wParam, LPARAM lParam) {
    // wysłanie komunikatu do sterującego
    HWND h = FindWindow(ClassName.c_str(), 0); // można by to zapamiętać
    if (h == 0)
        h = FindWindow(0, ClassName.c_str()); // można by to zapamiętać
    SendMessage(h, Msg, wParam, lParam);
}

void
WyslijEvent(const std::string &e, const std::string &d)
{ // Ra: jeszcze do wyczyszczenia
    DaneRozkaz r;
    r.iSygn = MAKE_ID4( 'E', 'U', '0', '7' );
    r.iComm = 2; // 2 - event
    size_t i = e.length(), j = d.length();
    r.cString[0] = char(i);
    strcpy(r.cString + 1, e.c_str()); // zakończony zerem
    r.cString[i + 2] = char(j); // licznik po zerze kończącym
    strcpy(r.cString + 3 + i, d.c_str()); // zakończony zerem
    COPYDATASTRUCT cData;
    cData.dwData = MAKE_ID4( 'E', 'U', '0', '7' ); // sygnatura
    cData.cbData = (DWORD)(12 + i + j); // 8+dwa liczniki i dwa zera kończące
    cData.lpData = &r;
    Navigate( "TEU07SRK", WM_COPYDATA, (WPARAM)glfwGetWin32Window( Global::window ), (LPARAM)&cData );
	CommLog( Now() + " " + std::to_string(r.iComm) + " " + e + " sent" );
}

void
WyslijUszkodzenia(const std::string &t, char fl)
{ // wysłanie informacji w postaci pojedynczego tekstu
	DaneRozkaz r;
    r.iSygn = MAKE_ID4( 'E', 'U', '0', '7' );
	r.iComm = 13; // numer komunikatu
	size_t i = t.length();
	r.cString[0] = char(fl);
	r.cString[1] = char(i);
	strcpy(r.cString + 2, t.c_str()); // z zerem kończącym
	COPYDATASTRUCT cData;
    cData.dwData = MAKE_ID4( 'E', 'U', '0', '7' ); // sygnatura
	cData.cbData = (DWORD)(11 + i); // 8+licznik i zero kończące
	cData.lpData = &r;
    Navigate( "TEU07SRK", WM_COPYDATA, (WPARAM)glfwGetWin32Window( Global::window ), (LPARAM)&cData );
	CommLog( Now() + " " + std::to_string(r.iComm) + " " + t + " sent");
}

void
WyslijString(const std::string &t, int n)
{ // wysłanie informacji w postaci pojedynczego tekstu
    DaneRozkaz r;
    r.iSygn = MAKE_ID4( 'E', 'U', '0', '7' );
    r.iComm = n; // numer komunikatu
    size_t i = t.length();
    r.cString[0] = char(i);
    strcpy(r.cString + 1, t.c_str()); // z zerem kończącym
    COPYDATASTRUCT cData;
    cData.dwData = MAKE_ID4( 'E', 'U', '0', '7' ); // sygnatura
    cData.cbData = (DWORD)(10 + i); // 8+licznik i zero kończące
    cData.lpData = &r;
    Navigate( "TEU07SRK", WM_COPYDATA, (WPARAM)glfwGetWin32Window( Global::window ), (LPARAM)&cData );
	CommLog( Now() + " " + std::to_string(r.iComm) + " " + t + " sent");
}

void
WyslijWolny(const std::string &t)
{ // Ra: jeszcze do wyczyszczenia
    WyslijString(t, 4); // tor wolny
}

void
WyslijNamiary(TGroundNode *t)
{ // wysłanie informacji o pojeździe - (float), długość ramki będzie zwiększana w miarę potrzeby
    // WriteLog("Wysylam pojazd");
    DaneRozkaz r;
    r.iSygn = MAKE_ID4( 'E', 'U', '0', '7' );
    r.iComm = 7; // 7 - dane pojazdu
	int i = 32;
	size_t j = t->asName.length();
    r.iPar[0] = i; // ilość danych liczbowych
    r.fPar[1] = Global::fTimeAngleDeg / 360.0; // aktualny czas (1.0=doba)
    r.fPar[2] = t->DynamicObject->MoverParameters->Loc.X; // pozycja X
    r.fPar[3] = t->DynamicObject->MoverParameters->Loc.Y; // pozycja Y
    r.fPar[4] = t->DynamicObject->MoverParameters->Loc.Z; // pozycja Z
    r.fPar[5] = t->DynamicObject->MoverParameters->V; // prędkość ruchu X
    r.fPar[6] = t->DynamicObject->MoverParameters->nrot * M_PI *
                t->DynamicObject->MoverParameters->WheelDiameter; // prędkość obrotowa kóŁ
    r.fPar[7] = 0; // prędkość ruchu Z
    r.fPar[8] = t->DynamicObject->MoverParameters->AccS; // przyspieszenie X
    r.fPar[9] = t->DynamicObject->MoverParameters->AccN; // przyspieszenie Y //na razie nie
    r.fPar[10] = t->DynamicObject->MoverParameters->AccV; // przyspieszenie Z
    r.fPar[11] = t->DynamicObject->MoverParameters->DistCounter; // przejechana odległość w km
    r.fPar[12] = t->DynamicObject->MoverParameters->PipePress; // ciśnienie w PG
    r.fPar[13] = t->DynamicObject->MoverParameters->ScndPipePress; // ciśnienie w PZ
    r.fPar[14] = t->DynamicObject->MoverParameters->BrakePress; // ciśnienie w CH
    r.fPar[15] = t->DynamicObject->MoverParameters->Compressor; // ciśnienie w ZG
    r.fPar[16] = t->DynamicObject->MoverParameters->Itot; // Prąd całkowity
    r.iPar[17] = t->DynamicObject->MoverParameters->MainCtrlPos; // Pozycja NJ
    r.iPar[18] = t->DynamicObject->MoverParameters->ScndCtrlPos; // Pozycja NB
    r.iPar[19] = t->DynamicObject->MoverParameters->MainCtrlActualPos; // Pozycja jezdna
    r.iPar[20] = t->DynamicObject->MoverParameters->ScndCtrlActualPos; // Pozycja bocznikowania
    r.iPar[21] = t->DynamicObject->MoverParameters->ScndCtrlActualPos; // Pozycja bocznikowania
    r.iPar[22] = t->DynamicObject->MoverParameters->ResistorsFlag * 1 +
                 t->DynamicObject->MoverParameters->ConverterFlag * 2 +
                 +t->DynamicObject->MoverParameters->CompressorFlag * 4 +
                 t->DynamicObject->MoverParameters->Mains * 8 +
                 +t->DynamicObject->MoverParameters->DoorLeftOpened * 16 +
                 t->DynamicObject->MoverParameters->DoorRightOpened * 32 +
                 +t->DynamicObject->MoverParameters->FuseFlag * 64 +
                 t->DynamicObject->MoverParameters->DepartureSignal * 128;
    // WriteLog("Zapisalem stare");
    // WriteLog("Mam patykow "+IntToStr(t->DynamicObject->iAnimType[ANIM_PANTS]));
    for (int p = 0; p < 4; p++)
    {
        //   WriteLog("Probuje pant "+IntToStr(p));
        if (p < t->DynamicObject->iAnimType[ANIM_PANTS])
        {
            r.fPar[23 + p] = t->DynamicObject->pants[p].fParamPants->PantWys; // stan pantografów 4
            //     WriteLog("Zapisalem pant "+IntToStr(p));
        }
        else
        {
            r.fPar[23 + p] = -2;
            //     WriteLog("Nie mam pant "+IntToStr(p));
        }
    }
    // WriteLog("Zapisalem pantografy");
    for (int p = 0; p < 3; p++)
        r.fPar[27 + p] =
            t->DynamicObject->MoverParameters->ShowCurrent(p + 1); // amperomierze kolejnych grup
    // WriteLog("zapisalem prady");
    r.iPar[30] = t->DynamicObject->MoverParameters->WarningSignal; // trabienie
    r.fPar[31] = t->DynamicObject->MoverParameters->RunningTraction.TractionVoltage; // napiecie WN
    // WriteLog("Parametry gotowe");
    i <<= 2; // ilość bajtów
    r.cString[i] = char(j); // na końcu nazwa, żeby jakoś zidentyfikować
    strcpy(r.cString + i + 1, t->asName.c_str()); // zakończony zerem
    COPYDATASTRUCT cData;
    cData.dwData = MAKE_ID4( 'E', 'U', '0', '7' ); // sygnatura
    cData.cbData = (DWORD)(10 + i + j); // 8+licznik i zero kończące
    cData.lpData = &r;
    // WriteLog("Ramka gotowa");
    Navigate( "TEU07SRK", WM_COPYDATA, (WPARAM)glfwGetWin32Window( Global::window ), (LPARAM)&cData );
    // WriteLog("Ramka poszla!");
	CommLog( Now() + " " + std::to_string(r.iComm) + " " + t->asName + " sent");
}

void
WyslijObsadzone()
{   // wysłanie informacji o pojeździe
	DaneRozkaz2 r;
    r.iSygn = MAKE_ID4( 'E', 'U', '0', '7' );
	r.iComm = 12;   // kod 12
	for (int i=0; i<1984; ++i) r.cString[i] = 0;

    // TODO: clean this up, we shouldn't be relying on direct list access
    auto &vehiclelist = simulation::Vehicles.sequence();

	int i = 0;
    for( auto *vehicle : vehiclelist ) {
        if( vehicle->Mechanik ) {
            strcpy( r.cString + 64 * i, vehicle->asName.c_str() );
            r.fPar[ 16 * i + 4 ] = vehicle->GetPosition().x;
            r.fPar[ 16 * i + 5 ] = vehicle->GetPosition().y;
            r.fPar[ 16 * i + 6 ] = vehicle->GetPosition().z;
            r.iPar[ 16 * i + 7 ] = vehicle->Mechanik->GetAction();
            strcpy( r.cString + 64 * i + 32, vehicle->GetTrack()->IsolatedName().c_str() );
            strcpy( r.cString + 64 * i + 48, vehicle->Mechanik->Timetable()->TrainName.c_str() );
            i++;
            if( i > 30 ) break;
        }
    }
	while (i <= 30)
	{
		strcpy(r.cString + 64 * i, "none");
		r.fPar[16 * i + 4] = 1;
		r.fPar[16 * i + 5] = 2;
		r.fPar[16 * i + 6] = 3;
		r.iPar[16 * i + 7] = 0;
		strcpy(r.cString + 64 * i + 32, "none");
		strcpy(r.cString + 64 * i + 48, "none");
		i++;
	}

	COPYDATASTRUCT cData;
    cData.dwData = MAKE_ID4( 'E', 'U', '0', '7' );     // sygnatura
	cData.cbData = 8 + 1984; // 8+licznik i zero kończące
	cData.lpData = &r;
	// WriteLog("Ramka gotowa");
    Navigate( "TEU07SRK", WM_COPYDATA, (WPARAM)glfwGetWin32Window( Global::window ), (LPARAM)&cData );
	CommLog( Now() + " " + std::to_string(r.iComm) + " obsadzone" + " sent");
}

void
WyslijParam(int nr, int fl)
{ // wysłanie parametrów symulacji w ramce (nr) z flagami (fl)
    DaneRozkaz r;
    r.iSygn = MAKE_ID4( 'E', 'U', '0', '7' );
    r.iComm = nr; // zwykle 5
    r.iPar[0] = fl; // flagi istotności kolejnych parametrów
    int i = 0; // domyślnie brak danych
    switch (nr)
    { // można tym przesyłać różne zestawy parametrów
    case 5: // czas i pauza
        r.fPar[1] = Global::fTimeAngleDeg / 360.0; // aktualny czas (1.0=doba)
        r.iPar[2] = Global::iPause; // stan zapauzowania
        i = 8; // dwa parametry po 4 bajty każdy
        break;
    }
    COPYDATASTRUCT cData;
    cData.dwData = MAKE_ID4( 'E', 'U', '0', '7' ); // sygnatura
    cData.cbData = 12 + i; // 12+rozmiar danych
    cData.lpData = &r;
    Navigate( "TEU07SRK", WM_COPYDATA, (WPARAM)glfwGetWin32Window( Global::window ), (LPARAM)&cData );
}
#endif

} // multiplayer

//---------------------------------------------------------------------------
