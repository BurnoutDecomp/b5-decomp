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
        BrnPhysics::Vehicle::CrashingRaceCarInterface                            mCrashingRaceCarInterface;  // gsm+250816

        void Construct()
        {
            mTakedownEventQueue.Construct();
            mRaceCarCrashEventQueue.Construct();
            mTrafficTypeResponseQueue.Construct();
            mCrashingRaceCarInterface.Clear();
        }
    };
}
