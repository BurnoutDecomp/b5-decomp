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

    // Traffic-input event: a crashed traffic vehicle to remove from the world. A single u16
    // traffic-vehicle index (0..KU_MAX_TOTAL_TRAFFIC). The producer
    // (TrafficEntityModule::GenerateRemovedVehicleEvents @0x827206E8) stores the index into a
    // local __int16 and forwards it to AddEvent; the X360 AddEvent (@0x8271A568) writes a single
    // 2-byte element at a 2-byte stride. The queue base subobject stays 12 bytes (4-byte element
    // alignment), giving the maEvents offset 0xC the EventQueue<...,160>::Construct attests
    // (@0x827608A0).
    struct RemoveCrashedTrafficEvent
    {
        u16 muVehicleId;
    };

    // Traffic-input event: a slam-recovered traffic vehicle to remove. Identical single-u16
    // payload to RemoveCrashedTrafficEvent; the producer
    // (TrafficEntityModule::GenerateSlamRecoveryEvents @0x827207E0) forwards a traffic index and
    // the X360 AddEvent (@0x8271A430) writes one 2-byte element at a 2-byte stride. The queue
    // base subobject stays 12 bytes (4-byte element alignment), giving the maEvents offset 0xC
    // the EventQueue<...,160>::Construct attests (@0x82760830).
    struct RemoveSlammedTrafficEvent
    {
        u16 muVehicleId;
    };
}
}
