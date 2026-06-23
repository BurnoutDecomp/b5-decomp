#pragma once

// Crash-module network IO event payloads. Reconstructed from the DecFIGS DWARF.
#include "BrnCommonTypes.h"                                  // Matrix44Affine
#include "GameShared/GameClasses/Module/CgsEventQueue.h"     // CgsModule::EventQueue<T, N>
#include "GameShared/GameClasses/Containers/CgsBitArray.h"   // CgsContainers::BitArray<N>

namespace BrnWorld
{
// Max simultaneously active race cars in a network game; the per-car indexing bound
// for the crash network input interface (the X360 asserts spell
// "liRaceCarId < BrnWorld::KI_MAX_ACTIVE_RACE_CARS", == 8 on this build). Modelled as a
// file-visible constant rather than pulling in a shared BrnWorld header for a single
// bound (BrnBaseOnlineModeScoring / BrnGameModeParams precedent).
const s32 KI_MAX_ACTIVE_RACE_CARS = 8;

namespace CrashIO
{
    // Per-player / total crashing-traffic update caps (DecFIGS DWARF
    // BrnCrashModuleNetworkIOInterfaces.h:36/:37). The per-player cap is the
    // EventQueue capacity used by NetworkInputInterface / NetworkOutputInterface.
    const s32 KI_MAX_NETWORK_TRAFFIC_UPDATES_PER_PLAYER = 24;
    const s32 KI_MAX_NETWORK_TRAFFIC_UPDATES            = 192;

    // Network event: a crashing traffic vehicle's updated transform.
    struct alignas(16) CrashingTrafficUpdateEvent
    {
        u16            muVehicleId;
        Matrix44Affine mTransform;
    };

    // Per-player network input view of crashing traffic. Holds an active-race-car bitset
    // and one fixed-capacity (24) crashing-traffic update queue per race car. Layout and
    // member names from the DecFIGS DWARF (BrnCrashModuleNetworkIOInterfaces.h:72-110);
    // the queue array follows the (alignas-16) bitset with 16-byte alignment, giving the
    // 0x790-byte queue stride and 0x3C90-byte struct size the X360 asm attests.
    struct NetworkInputInterface
    {
        typedef CgsContainers::BitArray<8u>                                  ActiveRaceCarBitArray;
        typedef CgsModule::EventQueue<CrashingTrafficUpdateEvent,
                                      KI_MAX_NETWORK_TRAFFIC_UPDATES_PER_PLAYER> CrashingTrafficUpdateQueue;

        // Reset the bitset and (re)construct every per-car queue against its inline storage.
        void Construct();

        // Mark a race car as having a pending network update (sets its bit).
        void MarkRaceCarForUpdate(s32 liRaceCarId);

        // Per-instance copy: copy the active-car bitset, then merge each source queue's
        // live events onto the matching (freshly cleared) destination queue (X360 operator=).
        NetworkInputInterface& operator=(const NetworkInputInterface& lOther);

    private:
        ActiveRaceCarBitArray      mActiveRaceCars;
        CrashingTrafficUpdateQueue maCrashingTrafficUpdateQueues[KI_MAX_ACTIVE_RACE_CARS];
    };

    // Network OUTPUT view of locally-owned crashing traffic. A single fixed-capacity (24)
    // crashing-traffic update queue (the same EventQueue<CrashingTrafficUpdateEvent, 24>
    // type the input interface uses per car). Layout and member names from the DecFIGS DWARF
    // (BrnCrashModuleNetworkIOInterfaces.h:125-150): the lone queue lives at offset 0, so the
    // X360 AddOwnedTrafficUpdate passes `this` straight to the queue's AddEvent.
    struct NetworkOutputInterface
    {
        typedef CgsModule::EventQueue<CrashingTrafficUpdateEvent,
                                      KI_MAX_NETWORK_TRAFFIC_UPDATES_PER_PLAYER> CrashingTrafficUpdateQueue;

        // (Re)construct the queue against its inline storage.
        void Construct();

        // Drop all queued updates (queue length reset).
        void Clear();

        // Build a CrashingTrafficUpdateEvent{ (u16)luVehicleIndex, lTransform } and append it
        // to the queue. The X360 body asserts the vehicle index is in range and that the
        // transform is valid (both non-gating tripwires), then forwards to the queue's AddEvent.
        void AddOwnedTrafficUpdate(u32 luVehicleIndex, Matrix44Affine lTransform);

        // Read-only access to the queued updates.
        const CrashingTrafficUpdateQueue* GetCrashingTrafficUpdateQueue() const;

    private:
        CrashingTrafficUpdateQueue mCrashingTrafficUpdateQueue;
    };
}
}
