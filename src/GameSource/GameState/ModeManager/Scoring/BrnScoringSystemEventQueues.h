#pragma once

// ----------------------------------------------------------------------------
// ScoringSystem event-queue parameter types (BrnScoringSystem_finale_queues work item).
//
// Two of the per-frame ScoringSystem update passes take these queues by const pointer:
//   UpdateTakedowns        (const InputBuffer::TakedownEventQueue*)                 X360 0x8232AC88
//   UpdateCrashes          (const VehicleManagerOutputInterface::RaceCarCrashEventQueue*) 0x8231F9B8
// (UpdatePaybackTakedowns takes the network interface's own DirtyTrickQueue typedef; the
// keystone reaches it through BrnNetworkModuleGameStateIOInterfaces.h.)
//
// The BrnScoringSystem.h keystone forward-declares each of these as a bare local
// `struct` under BrnGameState::{InputBuffer, VehicleManagerOutputInterface}. In the original source
// those names are namespace-imported typedefs to the FIXED-STRIDE CgsModule::EventQueue<T,N>
// (DWARF-authoritative -- NOT the variable-stride VariableEventQueue the work item premise
// guessed at):
//   InputBuffer::TakedownEventQueue
//        = EventQueue<BrnGameState::TakedownEvent,8>                  (BrnAIModuleIO.h:54)
//   VehicleManagerOutputInterface::RaceCarCrashEventQueue
//        = EventQueue<BrnPhysics::Vehicle::RaceCarCrashEvent,8>        (BrnVehicleOutputInterface.h:154)
//
// MINIMAL MODEL -- NO DEEP CASCADE. The generic CgsModule::EventQueue<T,N> +
// CgsModule::BaseEventQueue<T> are already fully modelled (CgsEventQueue.h /
// CgsBaseEventQueue.h) with the complete iteration interface the bodies need
// (GetLength @+8, GetEvent(i)). Both element types already have committed homes
// (TakedownEvent: BrnTakedownManagerTypes.h; RaceCarCrashEvent: BrnVehicleEvents.h).
// So the only modelling left is to give the
// keystone's forward-declared queue structs a complete definition.
//
// We complete each as an EMPTY struct deriving from its canonical EventQueue<T,N>
// instantiation (it adds no members, so its layout == the EventQueue's; semantic
// parity is by named access, not byte-exact). This keeps the keystone's `struct`
// forward declarations consistent (a `struct X;` cannot be completed by a `typedef`),
// while inheriting the full iteration interface. The empty-derived-over-EventQueue
// pattern is already used in-tree (BrnContactSpyRunList.h:45).
// ----------------------------------------------------------------------------

#include "GameShared/GameClasses/Module/CgsEventQueue.h"                              // CgsModule::EventQueue<T,N>
#include "GameSource/GameState/TakedownManager/BrnTakedownManagerTypes.h"            // BrnGameState::TakedownEvent
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"             // BrnPhysics::Vehicle::RaceCarCrashEvent

namespace BrnGameState
{
    // InputBuffer::TakedownEventQueue == EventQueue<TakedownEvent,8>.
    // Completes the keystone forward-decl `namespace InputBuffer { struct TakedownEventQueue; }`.
    namespace InputBuffer
    {
        struct TakedownEventQueue
            : public CgsModule::EventQueue<BrnGameState::TakedownEvent, 8>
        {
        };
    }

    // VehicleManagerOutputInterface::RaceCarCrashEventQueue == EventQueue<RaceCarCrashEvent,8>.
    namespace VehicleManagerOutputInterface
    {
        struct RaceCarCrashEventQueue
            : public CgsModule::EventQueue<BrnPhysics::Vehicle::RaceCarCrashEvent, 8>
        {
        };
    }
}
