#pragma once

// Crash-module traffic IO event payloads (DWARF home BrnCrashModuleTrafficIOInterfaces.h).
// These are the per-element types embedded in the crash module's traffic input/output
// interface event queues (EventQueue<T, 160>). Layout and member names from the DecFIGS
// DWARF (BrnCrashModuleTrafficIOInterfaces.h:60-131); element sizes/strides confirmed
// against the X360 ARTIST queue Construct/AddEvent spine.
#include "BrnCommonTypes.h"                                            // EntityId
#include "GameShared/GameClasses/Module/CgsEventQueue.h"               // CgsModule::EventQueue<T, N>
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"   // CgsSceneManager::VolumeInstanceId
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"     // BrnPhysics::Vehicle::eCrashTrafficType

namespace BrnWorld
{
namespace CrashIO
{
    // Traffic-input event: a traffic vehicle has started crashing. 16 bytes
    // (VolumeInstanceId 8 + EntityId 4 + eCrashTrafficType 4); the X360 AddEvent copies the
    // element as two 8-byte stores at a 16-byte stride (@0x8271A2E8).
    struct AddCrashingTrafficEvent
    {
        CgsSceneManager::VolumeInstanceId mVolumeInstanceId;
        EntityId                          mCrasherEntityId;
        BrnPhysics::Vehicle::eCrashTrafficType meCrashTrafficType;
    };

    // Traffic-output event: a crashed traffic volume to clean up. A single 8-byte
    // VolumeInstanceId; the queue base subobject pads to 16 (8-byte element alignment),
    // giving the maEvents offset 0x10 the EventQueue<...,160>::Construct attests (@0x82760910).
    struct CleanupTrafficEvent
    {
        CgsSceneManager::VolumeInstanceId mVolumeInstanceId;
    };

    // Traffic-output event: a network-replicated traffic vehicle to start crashing. A single
    // u16 vehicle id; the queue base subobject stays 12 bytes (4-byte element alignment),
    // giving the maEvents offset 0xC the EventQueue<...,160>::Construct attests (@0x82760980).
    struct NetworkTrafficCrashingEvent
    {
        u16 muVehicleId;
    };
}
}
