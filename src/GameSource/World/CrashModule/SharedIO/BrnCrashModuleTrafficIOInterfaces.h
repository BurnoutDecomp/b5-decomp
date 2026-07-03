#pragma once

// Crash-module traffic IO event payloads (DWARF home BrnCrashModuleTrafficIOInterfaces.h).
// These are the per-element types embedded in the crash module's traffic input/output
// interface event queues (EventQueue<T, 160>). Layout and member names from the DecFIGS
// DWARF (BrnCrashModuleTrafficIOInterfaces.h:60-131); element sizes/strides confirmed
// against the X360 ARTIST queue Construct/AddEvent spine.
#include "BrnCommonTypes.h"                                            // EntityId
#include "GameShared/GameClasses/Module/CgsEventQueue.h"               // CgsModule::EventQueue<T, N>
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"         // CgsContainers::FastBitArray<N>
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

    // Crash-module traffic INPUT interface (DWARF home BrnCrashModuleTrafficIOInterfaces.h:148;
    // member layout :187-192). The crash module's per-frame view of traffic state: three
    // EventQueue<...,160> input queues plus three FastBitArray<601> per-traffic-vehicle bit
    // masks. The X360 layout (Construct @0x827609F0, operator= @0x827AA4E8) is:
    //   mAddCrashingTrafficEventQueue   @ 0x000  EventQueue<AddCrashingTrafficEvent,160>
    //                                            (16-byte element + 12-byte base, 16-aligned ->
    //                                             next member at 0xA10)
    //   mRemoveSlammedTrafficEventQueue @ 0xA10  EventQueue<RemoveSlammedTrafficEvent,160>
    //                                            (u16 element, 12-byte base -> 332 bytes,
    //                                             next member at 0xB5C)
    //   mRemoveCrashedTrafficEventQueue @ 0xB5C  EventQueue<RemoveCrashedTrafficEvent,160>
    //                                            (u16 element -> 332 bytes, next at 0xCA8)
    //   mRenderingBits                  @ 0xCA8  FastBitArray<601> (10 u64 fields == 80 bytes)
    //   mFarFromCameraBits              @ 0xCF8  FastBitArray<601> (80 bytes)
    //   mPhysicalBits                   @ 0xD48  FastBitArray<601> (80 bytes; ends at 0xD98)
    // The three 80-byte bit masks at 0xCA8/0xCF8/0xD48 are exactly the 30 qwords Construct
    // zeroes and the 30 qwords operator= copies (3 loops of 10), confirming FastBitArray<601>
    // == 80 bytes (ceil(601/64) == 10 u64 fields).
    struct TrafficInputInterface
    {
        typedef CgsModule::EventQueue<AddCrashingTrafficEvent,  160> AddCrashingTrafficEventQueue;   // :66
        typedef CgsModule::EventQueue<RemoveSlammedTrafficEvent, 160> RemoveSlammedTrafficEventQueue; // :82
        typedef CgsModule::EventQueue<RemoveCrashedTrafficEvent, 160> RemoveCrashedTrafficEventQueue; // :98
        typedef CgsContainers::FastBitArray<601>                      TrafficVehicleBits;             // :135

        // Point each input queue at its inline storage (maxLength 160, length 0) and zero the
        // three traffic-vehicle bit masks (X360 Construct @0x827609F0).
        void Construct();

        // Per-instance copy: clear each destination queue's length and Append every live event
        // from the matching source queue, then copy the three bit masks (X360 operator=
        // @0x827AA4E8).
        TrafficInputInterface& operator=(const TrafficInputInterface& lOther);

    private:
        AddCrashingTrafficEventQueue   mAddCrashingTrafficEventQueue;    // :187 @0x000
        RemoveSlammedTrafficEventQueue mRemoveSlammedTrafficEventQueue;  // :188 @0xA10
        RemoveCrashedTrafficEventQueue mRemoveCrashedTrafficEventQueue;  // :189 @0xB5C
        TrafficVehicleBits             mRenderingBits;                   // :190 @0xCA8
        TrafficVehicleBits             mFarFromCameraBits;               // :191 @0xCF8
        TrafficVehicleBits             mPhysicalBits;                    // :192 @0xD48
    };

    // Crash-module traffic OUTPUT interface (DWARF home BrnCrashModuleTrafficIOInterfaces.h:385;
    // members :391/:398). The crash module fills this each frame with the traffic vehicles the
    // traffic module must clean up (crashed traffic volumes) plus the network-replicated traffic
    // to start crashing; the traffic module's InputBuffer_PostScene::SetCrashTrafficOutputInterface
    // (X360 0x827ACDE8) refreshes an embedded copy from it. X360 layout:
    //   mCleanupTrafficEventQueue         @ 0x000  EventQueue<CleanupTrafficEvent,160>
    //   mStartCrashingNetworkTrafficQueue @ 0x510  EventQueue<NetworkTrafficCrashingEvent,160>
    // The non-const reference-returning queue accessors let the copier chain .Clear()/.Append().
    struct TrafficOutputInterface
    {
        typedef CgsModule::EventQueue<CleanupTrafficEvent, 160>         CleanupTrafficEventQueue;   // DWARF :387
        typedef CgsModule::EventQueue<NetworkTrafficCrashingEvent, 160> CrashNetworkTrafficQueue;   // DWARF :395

        void Construct();
        void AddTrafficToCleanup(CgsSceneManager::VolumeInstanceId lVolumeInstanceId);
        void StartNetworkTrafficVehicleCrashing(u32 luVehicleId);

        const CleanupTrafficEventQueue& GetCleanupTrafficEventQueue() const { return mCleanupTrafficEventQueue; }
        CleanupTrafficEventQueue&       GetCleanupTrafficEventQueue()       { return mCleanupTrafficEventQueue; }
        const CrashNetworkTrafficQueue& GetStartCrashingNetworkTrafficQueue() const { return mStartCrashingNetworkTrafficQueue; }
        CrashNetworkTrafficQueue&       GetStartCrashingNetworkTrafficQueue()       { return mStartCrashingNetworkTrafficQueue; }

    private:
        CleanupTrafficEventQueue mCleanupTrafficEventQueue;         // DWARF :391 @0x000
        CrashNetworkTrafficQueue mStartCrashingNetworkTrafficQueue; // DWARF :398 @0x510
    };
}
}
