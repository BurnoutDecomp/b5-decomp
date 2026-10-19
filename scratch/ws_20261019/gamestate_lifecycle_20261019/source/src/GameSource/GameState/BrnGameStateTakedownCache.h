#pragma once

// [takedown wave 2026-09-02] The per-frame takedown inputs GameStateModule keeps beside its
// TakedownManager on X360 (gsm+249936 / +250272 / +250816). Heap-allocated on this build
// (GameStateModule::mpTakedownCache) so the complete element types -- which BrnGameStateModule.h
// must not include -- stay in the takedown partfiles. See GameStateModule_gTD_00.cpp.

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/GameState/TakedownManager/BrnTakedownManagerTypes.h"            // TakedownEvent
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"                // RaceCarCrashEvent
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"       // CrashingRaceCarInterface
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficTypeInterface.h" // TrafficTypeResponse

namespace BrnGameState
{
    struct TakedownPostWorldCache
    {
        CgsModule::EventQueue<TakedownEvent, 8>                                  mTakedownEventQueue;        // gsm+249936
        CgsModule::EventQueue<BrnPhysics::Vehicle::RaceCarCrashEvent, 8>         mRaceCarCrashEventQueue;    // gsm+250272
        CgsModule::EventQueue<BrnTraffic::BrnTrafficIO::TrafficTypeResponse, 32> mTrafficTypeResponseQueue;  // "lpLastTrafficTypeResponseQueue"

        // The scratch CrashingRaceCarInterface TakedownManager::Update is handed. On the console
        // this is PreWorldUpdate's own STACK local (var_6D8, filled at by
        // SetFromVehicleOutputInterface and passed as r7 at); it lives here because the
        // pre-world leg is an extracted member function rather than PreWorldUpdate's own frame.
        BrnPhysics::Vehicle::CrashingRaceCarInterface                            mCrashingRaceCarInterface;

        // gsm+250816 -- the module's CACHED COPY of the post-world VehicleOutputInterface, and the
        // one input SetFromVehicleOutputInterface reads (mUsedRaceCars, then each in-use slot's
        // RaceCarState::mbCrashing -- console element +0x44A). Filled by
        // GameStateModule::CacheTakedownManagerPostWorldInputData: the console copies the used-cars
        // head (`ld/std` at +0x00), block-copies the eight RaceCarStates (`memcpy(+0x10, +0x10,
        // 0x2300)` -- 8960 is the CONSOLE span; the host copy is by sizeof), and Clear+Appends the
        // impact / traffic-state / game-event queues. Named member, host layout.
        BrnPhysics::Vehicle::VehicleOutputInterface                              mVehicleOutputInterface;

        void Construct()
        {
            mTakedownEventQueue.Construct();
            mRaceCarCrashEventQueue.Construct();
            mTrafficTypeResponseQueue.Construct();
            mCrashingRaceCarInterface.Clear();
            mVehicleOutputInterface.Construct();
        }
    };
}
