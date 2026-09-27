#pragma once

// ----------------------------------------------------------------------------
// The event-queue parameter types the scorers, the HUD message logic and the challenge manager
// take by const pointer:
//   InputBuffer::TakedownEventQueue                    ScoringSystem::UpdateTakedowns,
//                                                      HUDMessageLogic, ModeManager
//   VehicleManagerOutputInterface::RaceCarCrashEventQueue  ScoringSystem::UpdateCrashes,
//                                                      ChallengeManager::PostWorldUpdate,
//                                                      HUDMessageLogic
//   VehicleOutputInterface::PhysicalTrafficStateQueue  CrashModeScoring::Update,
//                                                      ScoringSystem::UpdateCrashModeScore
//
// In the original source these names are namespace-imported typedefs to the fixed-stride
// CgsModule::EventQueue<T,N>:
//   InputBuffer::TakedownEventQueue                     = EventQueue<TakedownEvent,8>
//   VehicleManagerOutputInterface::RaceCarCrashEventQueue = EventQueue<RaceCarCrashEvent,8>
//   VehicleOutputInterface::PhysicalTrafficStateQueue   = EventQueue<PhysicalTrafficState,20>
// so they are typedefs here too: the same types the post-world input buffer, the vehicle output
// interface and the module's takedown cache hold, handed over with no cast.
//
// This is the one home of the three names; the headers that take them include this one.
// ----------------------------------------------------------------------------

#include "GameShared/GameClasses/Module/CgsEventQueue.h"                              // CgsModule::EventQueue<T,N>
#include "GameSource/GameState/TakedownManager/BrnTakedownManagerTypes.h"            // BrnGameState::TakedownEvent
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"             // RaceCarCrashEvent, PhysicalTrafficState

namespace BrnGameState
{
    namespace InputBuffer
    {
        typedef CgsModule::EventQueue<BrnGameState::TakedownEvent, 8> TakedownEventQueue;
    }

    namespace VehicleManagerOutputInterface
    {
        typedef CgsModule::EventQueue<BrnPhysics::Vehicle::RaceCarCrashEvent, 8> RaceCarCrashEventQueue;
    }

    namespace VehicleOutputInterface
    {
        typedef CgsModule::EventQueue<BrnPhysics::Vehicle::PhysicalTrafficState, 20> PhysicalTrafficStateQueue;
    }
}
