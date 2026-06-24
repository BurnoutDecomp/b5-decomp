#pragma once

// ===================================================================================
// BrnWorld::AirTimeManager -- owning header
//   b5-decomp/src/GameSource/World/EntityModules/RaceCarEntityModule/AirTime/BrnAirTimeManager.h
//
// Per-race-car airborne-time state machine. Each PostPhysics frame it watches the car's
// airborne indicator (RaceCarState +0x404) and steps a four-state machine that accumulates
// the in-air duration, pushing InAir events (type 69 / 'E') onto the game-event queue.
//
// Layout/method set from the DecFIGS DWARF (BrnAirTimeManager.h:70/:98/:108-112). The
// X360 lays the object out as { meState@+0, mfTimeSinceLastAir@+4, mfPreviousAirTime@+8,
// mfTotalAirTime@+0xC } (the Update stores prove +0/+4/+8/+0xC).
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // CgsModule::VariableEventQueue<1536,16>

namespace BrnPhysics { namespace Vehicle { struct RaceCarState; } }

namespace BrnWorld
{
    struct AirTimeManager
    {
        // The game-event queue Update pushes InAir events onto. The X360 dispatches to
        // CgsModule::VariableEventQueue<1536,16>::AddEvent (== GameStateModuleIO::GameEventQueue).
        typedef CgsModule::VariableEventQueue<1536, 16> GameEventQueue;

        // DWARF BrnAirTimeManager.h:98.
        enum EAirManagerState
        {
            E_AIR_STATE_CRASHING       = 0,
            E_AIR_STATE_IN_BETWEEN_AIR = 1,
            E_AIR_STATE_IN_AIR         = 2,
            E_AIR_STATE_NONE           = 3,
            E_AIR_STATE_COUNT          = 4,
        };

        // @ 0x822F8C88 -- step the air-time state machine for one PostPhysics frame and emit
        // InAir events. (The X360 fold of the OutputAirEvent helper is inlined here.)
        void Update(const BrnPhysics::Vehicle::RaceCarState* lpRaceCarState,
                    f32 lfSimTimerTimeStep,
                    GameEventQueue* lpEventQueue);

    private:
        EAirManagerState meState;             // +0x0
        f32              mfTimeSinceLastAir;   // +0x4
        f32              mfPreviousAirTime;    // +0x8
        f32              mfTotalAirTime;       // +0xC
    };
}
