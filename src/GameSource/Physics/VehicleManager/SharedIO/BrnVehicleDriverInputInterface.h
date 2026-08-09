#pragma once

// BrnPhysics::Vehicle::VehicleDriverInputInterface -- the driver-input seam between the
// entity/AI/network/traffic layers and the vehicle physics solver. Each frame the producer
// modules push driver-control records (player/AI/network/traffic) into the inline
// VariableEventQueue; the VehicleManager later drains the queue to drive the cars. The same
// interface also carries the per-frame "target assist" stomped-car list (used by Showtime /
// crash target lock-on) and a per-active-race-car base-deformation snapshot.
//
// LAYOUT (X360-authoritative; byte offsets recovered from the leaf functions in
// BrnVehicleDriverInputInterface.cpp):
//   +0      mDriverUpdateQueue          VariableEventQueue<5040,16>  (sizeof 5053, padded to 5056)
//   +5056   mTargetAssistPositions      Vector3[8]   (16-byte SIMD stride; AddTargetAssist stvx128)
//   +5184   mTargetAssistIDs            EntityId[8]  (u32 each)
//   +5216   miTargetAssistCount         s32
//   +5220   maBaseDeformationAmounts    f32[8]       (per active race car; Construct inits to 0.0f, flt_82001CC0)
//   +5252   maBaseDeformationFrames     s32[8]       (per active race car; Construct inits to -1)
// Total reaches +5284; the DirectorModuleIO embeds it in a 0x338 (824)-byte slot.
//
// The X360 queue template parameter is <5040,16> (the leaf Construct calls
// CgsModule::VariableEventQueue<5040,16>::Construct). The DecFIGS DWARF lists the queue as
// <4920,16> and shows only the target-assist members -- that is an older PS3/Feb-2007
// revision; this X360 build widened the queue and added the base-deformation snapshot arrays.
// The X360 ledger is authoritative, so we reconstruct the <5040,16> queue and both array sets.
//
// alignas(16): the type leads with a SIMD VariableEventQueue and holds a Vector3[8] (the X360
// stores positions with stvx128, i.e. the array must be 16-byte aligned).
#include "types.hpp"                                                  // u8/s32/f32
#include "BrnCommonTypes.h"                                           // Vector3 (rw::math::vpu::Vector3), EntityId
#include "GameSource/BurnoutConstants.h"                              // EActiveRaceCarIndex, E_ACTIVE_RACE_CAR_INDEX_COUNT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"      // CgsModule::VariableEventQueue<BUFSIZE,ALIGN>

namespace BrnPhysics
{
namespace Vehicle
{
    // Inline driver-update event queue capacity (X360 Construct -> VariableEventQueue<5040,16>).
    const s32 KI_UPDATE_DRIVER_EVENT_QUEUE_SIZE = 5040;

    // Max simultaneous target-assist (stomped) cars tracked per frame == the active-race-car
    // count (the leaf range-guards the count against 8, E_ACTIVE_RACE_CAR_INDEX_COUNT).
    const s32 KI_MAX_TARGET_ASSIST_CARS = E_ACTIVE_RACE_CAR_INDEX_COUNT;

    class VehicleDriverInputInterface
    {
    public:
        typedef CgsModule::VariableEventQueue<KI_UPDATE_DRIVER_EVENT_QUEUE_SIZE, 16> UpdateDriverEventQueue;

        // @0x822CBF60  Clears the event queue and resets the per-active-race-car snapshot
        // (target-assist count -> 0, base-deformation amounts -> 0.0f, frames -> -1).
        void Construct();

        // @0x822A0398  Append one stomped/target car to this frame's target-assist list.
        void AddTargetAssist(Vector3 lTargetAssistPosition, EntityId lTargetAssistID);

        // @0x823A7B40  Copy out the target-assist positions/IDs/count (count first, then the
        // packed position+ID arrays when the count is non-zero).
        void GetTargetAssistParams(Vector3* lpOutTargetAssistPositions,
                                   EntityId* lpOutTargetAssistIDs,
                                   s32* lpiOutTargetAssistCount) const;

        // @0x8279C290  Copy the per-active-race-car base-deformation snapshot (amount + frame
        // arrays) from another interface into this one.
        void CopyBaseDeformationParams(const VehicleDriverInputInterface* lpInterfaceToCopy);

        // @0x823DB640  Merge another interface's staged driver state into this one: append its
        // update-driver queue, then adopt its target-assist list when we have none of our own.
        // ⭐ ADDED 2026-08-09 (feed wave) -- the callee of
        // WorldModule::BridgeInputToPhysicsModule @0x827AB830.
        void Append(const VehicleDriverInputInterface* lpInterfaceToAppend);

        // @0x825B3588  Per-active-race-car base-deformation amount accessor.
        f32 GetBaseDeformationAmount(EActiveRaceCarIndex leRaceCarIndex) const;

        // @0x825B3600  Per-active-race-car base-deformation frame accessor. The exported symbol
        // is truncated to "Ge" in the X360 symbol table (no longer name is recoverable); the
        // ledger canonical for this leaf is therefore "Ge". It is the s32 counterpart to
        // GetBaseDeformationAmount (reads maBaseDeformationFrames[leRaceCarIndex]).
        s32 Ge(EActiveRaceCarIndex leRaceCarIndex) const;

        // ADDITIVE 2026-08-09: attested by the Append @0x823DB640 assert string in the X360
        // .rdata -- "miTargetAssistCount==0 || lpInterfaceToAppend->GetTargetAssistCount()==0"
        // (0x82039118). The console inlines it (Append reads `lwz r11,0x1460(r30)` directly), so
        // there is no out-of-line symbol; header-only inline keeps the read layout-correct.
        s32 GetTargetAssistCount() const { return miTargetAssistCount; }

        // Accessors for the inline driver-update queue (producers push UpdatePlayer/Network/AI/
        // Traffic driver records into it; the VehicleManager drains it).
        UpdateDriverEventQueue* GetUpdateDriverQueue() { return &mDriverUpdateQueue; }
        const UpdateDriverEventQueue* GetUpdateDriverQueue() const { return &mDriverUpdateQueue; }

    private:
        // Compile-time layout oracle (never called): pins the X360-authoritative member offsets so
        // the gate fails on any drift (queue padded to 5056 lands the SIMD arrays 16-aligned).
        static void _AssertLayout();

        UpdateDriverEventQueue mDriverUpdateQueue;                       // +0
        Vector3                mTargetAssistPositions[E_ACTIVE_RACE_CAR_INDEX_COUNT];   // +5056
        EntityId               mTargetAssistIDs[E_ACTIVE_RACE_CAR_INDEX_COUNT];         // +5184
        s32                    miTargetAssistCount;                     // +5216
        f32                    maBaseDeformationAmounts[E_ACTIVE_RACE_CAR_INDEX_COUNT]; // +5220
        s32                    maBaseDeformationFrames[E_ACTIVE_RACE_CAR_INDEX_COUNT];  // +5252
    };
}
}
