#pragma once

// ===========================================================================
// CgsSceneManager::SceneSweeper
//   Home: GameShared/GameClasses/SceneManager/ContactGen/CgsSceneSweeper.{h,cpp}
//
// The sweep-and-prune broadphase that the OverlapGenerationModule drives. It owns
// an on-screen debug component, the per-axis interval lists, and the overlapping-
// interval-pair output queue. The OverlapGenerationModule embeds it BY VALUE
// (CgsSceneManager::OverlapGenerationModule::mSweeper, X360 +0x230).
//
// LAYOUT NOTE (semantic parity, not byte-matching). The OverlapGenerationModule
// Construct asm (0x828D0460) attests the sweeper's two leading regions:
//   +0x00  SceneSweeperDebugComponent  (self-ptr @+0x0C, draw distance 50.0f @+0x10,
//                                        two render flags @+0x14/+0x15; then Register()ed)
//   +0x18  EventQueue<OverlappingIntervalPair, 131072>  mOverlappingIntervalPairs
// The debug component's real home header (CgsSceneSweeperDebugComponent.h) pulls the
// debug-system + rw colour cascade and does not compile standalone in this TU's
// include set, so its 0x18-byte leading slice is modelled here as a documented opaque
// block (maDebugComponentSlice) -- the by-value member only needs its size + the
// (debug-only) Construct/Register side effect, both owned by the sweeper's own .cpp TU.
//
// The bulk of the sweeper (its CgsIntervalList per-axis arrays + sort/sweep scratch
// state) likewise has its real home in CgsSceneSweeper.cpp (which delegates to the not-
// yet-reconstructed CgsSceneManager::IntervalList subsystem); it is modelled as a sized
// opaque tail (maIntervalListState). The whole SceneSweeper is sized to the X360 module-
// member delta (mSweeper spans +0x230..+0xDC6F0 in OverlapGenerationModule => 902336
// bytes) so the embedding layout is correct. FLAG: the internal split between the
// interval queue and the interval lists is not individually pinned.
//
// Method declarations only -- Clear / Prepare / SortLists / SweepLists bodies live in
// CgsSceneSweeper.cpp; AddObject / RemoveObject / UpdateObject are the broadphase
// mutators the OverlapGenerationModule's Process*BodyQueue passes call (their bodies
// live with the sweeper TU / are inlined there). This header is the declaration surface
// those call sites compile against.
// ===========================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"  // Vector3 (UpdateObject position lane)
#include "GameShared/GameClasses/Module/CgsEventQueue.h"  // CgsModule::EventQueue
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsInterval.h"  // OverlappingIntervalPair

namespace CgsSceneManager
{
    class SceneSweeper
    {
    public:
        // The overlapping-interval-pair output queue capacity (X360 build budget; the
        // queue is constructed inline by Construct via the 131072-element instantiation).
        static const s32 KI_MAX_OVERLAPPING_INTERVAL_PAIRS = 131072;

        typedef CgsModule::EventQueue<OverlappingIntervalPair, KI_MAX_OVERLAPPING_INTERVAL_PAIRS>
            OverlappingIntervalPairQueue;

        // Construct the sweeper. Reconstructed inline from the OverlapGenerationModule::
        // Construct asm at 0x828D0460, which carries this sweeper-construction inlined:
        //   * set up + register the debug component (self-ptr, 50.0f draw distance, render
        //     flags cleared) -- a debug-only side effect owned by the sweeper's own TU, so
        //     it is documented here rather than poked through the opaque debug slice;
        //   * construct the overlapping-interval-pair output queue;
        //   * Clear() the interval state.
        void Construct()
        {
            mOverlappingIntervalPairs.Construct();
            Clear();
        }

        // Clear the interval/pair state (X360 SceneSweeper::Clear @ 0x828B57A0; a memset of
        // the interval-list state). Body in CgsSceneSweeper.cpp.
        void Clear();

        // Prepare the sweeper against the scene's culling-group manager + culling table
        // (X360 SceneSweeper::Prepare @ 0x828C2000). Returns success. Body in CgsSceneSweeper.cpp.
        bool Prepare(void* lpCullingGroupManager, void* lpCullingTable);

        // Add a swept body to the broadphase. Called from
        // OverlapGenerationModule::ProcessAddBodyQueue per queued add-body request.
        //   luObjectIndex   = the sweeper object index (queue element +0x24),
        //   lpAABBox        = the world AABB to sweep (the queue element itself),
        //   luVolumeHandle  = the volume-instance index (queue element +0x28),
        //   lu64Body        = packed body id/cull-group word (queue element +0x30).
        void AddObject(u32 luObjectIndex, const void* lpAABBox, u32 luVolumeHandle, u64 lu64Body);

        // Remove a swept body (OverlapGenerationModule::ProcessRemoveBodyQueue).
        //   luObjectIndex = the sweeper object index (queue element +0x08).
        void RemoveObject(u32 luObjectIndex);

        // Update a swept body's bounds/position (OverlapGenerationModule::ProcessUpdateBodyQueue).
        //   luObjectIndex = queue element +0x30, lpAABBox = the queue element +0x38 box,
        //   lu64Body = the packed body word, lrPosition = the new world position lane (+0x20).
        void UpdateObject(u32 luObjectIndex, const void* lpAABBox, u64 lu64Body, const Vector3& lrPosition);

    private:
        // SceneSweeperDebugComponent leading slice (+0x00..+0x18). Opaque to avoid the
        // debug-system/rw cascade its real header pulls; see LAYOUT NOTE above.
        u8 maDebugComponentSlice[0x18];                          // +0x00

        OverlappingIntervalPairQueue mOverlappingIntervalPairs;  // +0x18

        // Per-axis interval lists + sort/sweep scratch state (real home: this class's
        // CgsSceneSweeper.cpp TU + the CgsSceneManager::IntervalList subsystem). Sized so
        // the whole SceneSweeper spans the OverlapGenerationModule member delta
        // +0x230..+0xDC6F0 (902336 bytes total). FLAG: opaque layout stand-in.
        u8 maIntervalListState[902336 - 0x18 - sizeof(OverlappingIntervalPairQueue)];
    };
}
