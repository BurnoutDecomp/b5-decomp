#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManager.h"

#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h" // InSceneUpdateInterface (mAddToCacheQueue / mRemoveFromCacheQueue)
#include "GameShared/GameClasses/Core/CgsAssert.h"                             // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                     // [DIAG] gpDebugPrint

#include <math.h>   // sqrtf -- the portable lowering of the X360 vrsqrtefp + 2 Newton-Raphson steps
#include <stdlib.h> // [DIAG] getenv -- BRN_PROP_DIAG latch only

// ============================================================================
// The TriangleCacheManager's per-frame SLOT BOOKKEEPING, reconstructed from
// BURNOUT_X360_ARTIST.XEX. Four functions, all driven (in this order) by
// SceneManagerModule::StartUpdateTriangleCache @0x828C73D8:
//
//   CgsSceneManager::TriangleCacheManager::ProcessRemoveFromCacheEvents      @0x828B2710 (346)
//   CgsSceneManager::TriangleCacheManager::ProcessAddToCacheEvents           @0x828B2C78 (222)
//   CgsSceneManager::TriangleCacheManager::ProcessUpdateCachedPositionEvents @0x828BE898 (279)
//   CgsSceneManager::CacheSlot::UpdateCachedObject                           @0x828BE660 (54)
//
// This is the WRITE side of the cache's slot table: which of the 298 cache slots
// are claimed, where each claimed object is, and which of them need their triangle
// set rebuilt this frame. The READ side (GetTrianglesForCachedObject / the
// SceneManagerIO::TriangleCacheInterface accessors) was already reconstructed and is
// what AddRaceCarTractionLineTests calls.
//
// ⭐⭐ REACHABILITY -- SUPERSEDED TWICE, so the whole history is here rather than a bare claim.
// This banner used to say "the sole caller, SceneManagerModule::StartUpdateTriangleCache, is
// still a link stub ... These four bodies therefore run ZERO times today." Both halves are now
// false, and each was corrected by a measurement rather than by reading:
//   * 2026-08-10 (spatial-partition wave): StartUpdateTriangleCache @0x828C73D8 got its real body
//     in CgsSceneManagerModule.cpp, so all three Process* run EVERY FRAME.
//   * 2026-08-10 (producer wave): `xrefs_to` showed each of them has a SECOND console caller --
//     SceneManagerModule::BridgeInputSceneUpdateInterfaceToSubModules @0x828D1F88 -- which the PC
//     bridge did not wire. That is the caller the PREPARE path uses (the bridge runs these three
//     only when its lbPrepare argument is set), and it is the one that drains
//     VehicleManager::PrepareTriangleCache's 28 InEventAddToCache out of a scene input buffer that
//     is destroyed in the same call. Wired now.
// RUNTIME-WITNESSED (2026-08-10): ProcessAddToCacheEvents claims 28 slots (8 race cars + 20
// traffic) during WorldModule::Prepare's physics stage.
// ⚠️⚠️ THE OLD TAIL OF THAT SENTENCE -- "`usedSlots=28` thereafter, every frame" -- IS RETIRED,
// wave Q6 round 2. It was measured before the prop subsystem had any producer and it went stale in
// the HELPFUL direction. PROPS CLAIM SLOTS TOO, and by name:
//   * BrnPhysics::Props::PropManager::ProcessAddPropInstanceEvents (PropManager_wQ2_06.cpp:560)
//     posts one InEventAddToCache per whole prop at KI_PROP_CACHE_START_INDEX + slot (28..42);
//   * ...::CreatePart (PropManager_wQ2_04.cpp:361) posts one per part at
//     KI_PROP_PART_CACHE_START_INDEX + slot (43..72);
//   * ...::RemoveProp / RemovePart (PropManager_wQ2_04.cpp:639 / :558) post the matching
//     InEventRemoveFromCache, and ...::RemoveAllPropsAndParts (PropManager_wQ6_01.cpp:250 / :264,
//     landed wave Q6) posts one for every live prop AND every live part on world unload.
// All three TUs are mounted.
// The steady-state used count is therefore 28 + (live physical props) + (live physical parts) and
// is NOT statically re-derivable. It is now MEASURED at run time by the one-shot
// `[Q6-tcache] first prop cache slot seated` line in ProcessAddToCacheEvents below -- read that,
// do not inherit a number from this banner.
// ⚠️ Nothing here fills the cache with TRIANGLES -- that is StartUpdateTriangleCaches @0x828BECF8
// (bodied) + the PolygonSoupTesterJob fill path (still absent), and it only runs for slots marked
// DIRTY, which needs the position half (PhysicsModule::UpdateCachedPositions, still gated).
//
// ⭐⭐ WAVE Q6 ROUND 2 (2026-08-19) -- WHAT THE FOUR "Trying to remove unused triangle cache slot"
// ASSERTS IN scratch/flow_run/20260819_115004/BrnGame.log:11541/:11558/:11575/:11592 ACTUALLY WERE.
// The round-1 follow-up handed to this cluster read "the prop triangle-cache ADD leg is not seating
// slots". MEASURED, and it is NOT: the four asserts are cross-frame DOUBLE REMOVES, and this
// manager was reporting the truth. The proof is static and does not need a boot:
//   1. All four fired from SceneManagerModule::BridgeInputSceneUpdateInterfaceToSubModules (leg 6,
//      the `lbPrepare` arm) on the log's own callstacks, i.e. off BrnWorldModule.cpp:3018's
//      post-physics UpdateScene(..., lbPrepare = true) -- the pass that drains the scene input
//      buffer WorldBridgePhysicsToScene.cpp:53 filled from the PHYSICS module's staged interface.
//   2. In that same log there are exactly FOUR `mUsedProps.IsBitSet( luPropIndex )` asserts
//      (PropManager_wQ2_04.cpp:611) from RemoveProp <- ProcessRemovePropInstanceEvents, and exactly
//      FOUR `Physics: Bad search for a Rigid body`. One-to-one-to-one: twelve asserts, four events.
//   3. RemoveProp posts its InEventRemoveFromCache FIRST (PropManager_wQ2_04.cpp:640),
//      UNCONDITIONALLY, and only then asserts that the prop's own slot was still in use. So a
//      SECOND RemoveProp on an already-retired prop necessarily posts a second remove for a cache
//      slot this manager had already freed -- which is precisely `!mUsedCacheSlots.IsBitSet(slot)`
//      with the add nowhere in this frame's queue.
//   4. ProcessAddPropInstanceEvents posts the ADD through the SAME `lpSceneInput` pointer, in the
//      SAME PropManager::ProcessInputsPreScene call, as the REMOVE that demonstrably arrived. If
//      the remove reaches this manager, the add does too; there is no route on which one survives
//      the buffer hop and the other does not.
// The upstream cause is the one wave Q6 round 1 closed (PropManager::OutputUpdatedProps was an
// inert gate, so ReadUpdatedBodies retired props silently and the world later re-removed them).
// The newest drive run scratch/flow_run/20260819_135113/BrnGame.log has ZERO asserts of all three
// families -- but it also ENDS ~300 log lines before the point at which the 115004 run first fired
// them, so treat that as consistent-with, NOT as proof. The `[Q6-tcache]` lines added below are the
// proof; they name the slot on both legs.
// NOTHING WAS "FIXED" HERE TO SILENCE THOSE ASSERTS. The two changes to the remove leg below make
// it fire MORE, not less, because the console fires more (see the ⭐ decode on that function).
//
// ---------------------------------------------------------------------------
// SOURCES / METHOD (per the standing discipline: read the ASM, not the pseudocode)
// The Hex-Rays output for all three Process* functions is ~80% CgsDev::StrStreamBase
// assert-message formatting, which buries the work; every decode below is off the
// disassembly. The three event record layouts were NOT inferred here -- they are the
// DWARF-authoritative structs already committed in CgsTriangleCacheManagerIO.h, and
// they independently predicted the exact fields the asm reads (mfCacheSphereRadius at
// +4 for Add, mNewPositionAndRadius at +0x10 for Update). The queue member offsets the
// caller folds in (+0xC4930 / +0xC5290) and the one this TU re-derives (+0xC77E0) match
// the offsets already documented on InSceneUpdateInterface's members, so every queue
// here is reached BY NAME.
//
// ⚠️ VMX LOWERING, flagged rather than hidden: the X360 touches CacheSlot's two
// 16-byte sphere fields with lvx128/vrlimi128/stvx128 lane merges. A `vrlimi128 vD,vS,1,0`
// inserts one lane (the w lane) of vS into vD, i.e. it is a single-component write that
// the compiler expressed as a whole-vector read-modify-write. Those are lowered here to
// plain scalar assignments of the field concerned; that is faithful to the source shape
// (Sphere::SetRadius / SetPosition) and observationally identical, but it is NOT a
// store-for-store transcription of the SIMD and is called out at each site.
// ============================================================================

namespace CgsSceneManager
{
    // ========================================================================
    // [DIAG] NOT IN THE X360 BINARY -- wave Q6 round 2 slot-ownership probe.
    //
    // Everything in this block is file-static: the TriangleCacheManager's own layout is pinned by
    // CgsTriangleCacheManager_embed_check.cpp and by the console offsets this TU decodes
    // (mpaCachedObjectSlots at +4, mUsedCacheSlots at +8), so NOT ONE BYTE of diagnostic state may
    // live on the class. The counters below therefore sit beside the code, not inside the object.
    // There is one TriangleCacheManager in the process (SceneManagerModule::mTriangleCacheManager);
    // if a second one is ever constructed these counters merge the two, which is stated here rather
    // than defended, because a merged count still answers the only question they are asked.
    //
    // WHY THEY EXIST: the question "did the prop ADD leg ever seat a slot?" cannot be answered from
    // the used bitmap at the moment the remove asserts -- by then the bit is gone either way. The
    // seat/free tallies distinguish "never seated" (seated == 0) from "seated then freed already"
    // (seated >= 1 && freed >= 1, i.e. a DOUBLE REMOVE upstream), which is exactly the fork the
    // round-1 follow-up left open.
    //
    // The window labels are DIAGNOSTIC LABELS ONLY. GameShared must not depend on GameSource, so
    // the two boundaries are spelled here as diag-local literals rather than by including the prop
    // headers; their provenance is the DWARF (BrnTriangleCacheConstants.h:36 KI_PROP_CACHE_START_
    // INDEX == 28, :37 KI_PROP_PART_CACHE_START_INDEX == 43) and the producers cited in the banner.
    // If a future wave re-homes those constants, this label mapping is the thing to re-check -- a
    // wrong label here would be a wrong comment, not just a cosmetic one.
    // ========================================================================
    namespace
    {
        const s32 KI_DIAG_PROP_WINDOW_FIRST = 28;   // DWARF BrnTriangleCacheConstants.h:36
        const s32 KI_DIAG_PART_WINDOW_FIRST = 43;   // DWARF BrnTriangleCacheConstants.h:37

        bool DiagEnabled()
        {
            // One getenv for the life of the process; a getenv per event would be a syscall on the
            // per-frame slot-bookkeeping path.
            static const bool sbPropDiag = ( getenv( "BRN_PROP_DIAG" ) != 0 );
            return sbPropDiag && ( CgsDev::Log::gpDebugPrint != 0 );
        }

        const char* DiagSlotOwner( s32 liCacheSlot )
        {
            if ( liCacheSlot >= KI_DIAG_PART_WINDOW_FIRST ) { return "part"; }
            if ( liCacheSlot >= KI_DIAG_PROP_WINDOW_FIRST ) { return "prop"; }
            return "vehicle/traffic";
        }

        // Per-slot lifetime tallies. u16 wraps after 65535 claims of one slot; the counters are only
        // ever read as "zero / non-zero" plus a rough magnitude, so wrap is harmless and is not
        // guarded (guarding it would put a branch on the diag arm for nothing).
        u16 sauDiagSeatCount[KU_MAX_CACHED_OBJECTS] = { 0 };
        u16 sauDiagFreeCount[KU_MAX_CACHED_OBJECTS] = { 0 };

        // The console indexes its 48-byte slot array with an event field whose bounds check is a
        // CGS_ASSERT -- i.e. a tripwire, not a guard. Diagnostics must not inherit that: an
        // out-of-range slot would make this file WRITE out of bounds for no reason other than
        // logging. These three keep the diag arm total, and are the only place in this TU where a
        // bound is enforced rather than asserted.
        bool DiagSlotInRange(s32 liCacheSlot)
        {
            return static_cast<u32>(liCacheSlot) < KU_MAX_CACHED_OBJECTS;
        }
        void DiagCount(u16* lpaCounters, s32 liCacheSlot)
        {
            if (DiagSlotInRange(liCacheSlot)) { ++lpaCounters[liCacheSlot]; }
        }
        s32 DiagRead(const u16* lpaCounters, s32 liCacheSlot)
        {
            return DiagSlotInRange(liCacheSlot) ? static_cast<s32>(lpaCounters[liCacheSlot]) : -1;
        }
    }

    // ------------------------------------------------------------------------
    // CgsSceneManager::CacheSlot::UpdateCachedObject @ 0x828BE660 (54 insns)
    //
    // Move this slot's cached object to the event's new position/radius and decide
    // whether the cached triangle set is now stale. The X360 body:
    //   1. mInnerSpherePositionAndRadius <- {lrEvent.mNewPositionAndRadius.xyz, .w}.
    //      Emitted as TWO stvx128 to the same address (0x828BE6A8 then 0x828BE6B0):
    //      the first merges in the OLD inner-sphere w, the second overwrites it with
    //      the event's radius. That is an inlined SetPosition() immediately followed by
    //      SetRadius() -- the first store is dead. Written here as the two field
    //      assignments the source shape implies; the dead intermediate is not reproduced
    //      (it has no observable effect and no address is published between the stores).
    //   2. mbIsDirty = false                                   (stb r10, 0x2E(r3))
    //   3. lfDistance = |lrEvent.mNewPositionAndRadius.xyz - mLastCachedSphere.xyz|
    //      X360: vsubfp -> vmsum3fp128 (3-lane dot) -> vrsqrtefp + TWO Newton-Raphson
    //      refinements (the vcfsx v9,v0,0 == 1.0f and vcfsx v8,v0,1 == 0.5f constants),
    //      then vmulfp128 by the squared length to turn 1/|d| into |d|, with a
    //      vcmpeqfp/vsel guard that yields 0 when |d|^2 is 0. That whole sequence is the
    //      standard rw::math::vpu length lowering; expressed here as sqrtf(dot3).
    //      ⚠️ Faithful, NOT bit-exact: the refined reciprocal-square-root estimate and
    //      sqrtf can differ in the last ulp. Flagged for review.
    //   4. if (lfDistance + newRadius > mLastCachedSphere.w) OR the slot holds no
    //      batches yet, re-seat the cached sphere's CENTRE on the new position (its w,
    //      the cached radius, is deliberately KEPT -- `vrlimi128 v12, v7, 1, 0` merges
    //      the OLD w back into the new position vector before the store) and mark dirty.
    //      The X360 spells the second half as a fall-through: `bne` to the dirty block
    //      when the distance test passes, else `lwz 0x28` / `cmpwi 0` / `bnelr` -- i.e.
    //      return early only when the slot already has batches. Same truth table.
    // ------------------------------------------------------------------------
    void CacheSlot::UpdateCachedObject(const TriangleCacheManagerIO::InEventUpdateCachedPosition& lrEvent)
    {
        const f32 lfNewRadius = lrEvent.mNewPositionAndRadius.w;

        // Step 1 -- the inner sphere always follows the object exactly.
        // (VMX: two stvx128 lane merges at +0x10; see the banner note.)
        mInnerSpherePositionAndRadius.x = lrEvent.mNewPositionAndRadius.x;
        mInnerSpherePositionAndRadius.y = lrEvent.mNewPositionAndRadius.y;
        mInnerSpherePositionAndRadius.z = lrEvent.mNewPositionAndRadius.z;
        mInnerSpherePositionAndRadius.w = lfNewRadius;

        // Step 2.
        mbIsDirty = false;

        // Step 3 -- distance from the object's new centre to the centre the triangles
        // were last cached around.
        const f32 lfDeltaX = lrEvent.mNewPositionAndRadius.x - mLastCachedSphere.x;
        const f32 lfDeltaY = lrEvent.mNewPositionAndRadius.y - mLastCachedSphere.y;
        const f32 lfDeltaZ = lrEvent.mNewPositionAndRadius.z - mLastCachedSphere.z;
        const f32 lfDistanceSquared = (lfDeltaX * lfDeltaX) + (lfDeltaY * lfDeltaY) + (lfDeltaZ * lfDeltaZ);
        const f32 lfDistance = (lfDistanceSquared == 0.0f) ? 0.0f : sqrtf(lfDistanceSquared);

        // Step 4 -- has the object's bound escaped the sphere the triangles cover, or
        // was it never cached at all?
        if (((lfDistance + lfNewRadius) > mLastCachedSphere.w) || (miNumCachedTriangleBatches == 0))
        {
            // Re-centre on the new position; the cached RADIUS (w) is kept.
            mLastCachedSphere.x = lrEvent.mNewPositionAndRadius.x;
            mLastCachedSphere.y = lrEvent.mNewPositionAndRadius.y;
            mLastCachedSphere.z = lrEvent.mNewPositionAndRadius.z;
            mbIsDirty = true;
        }
    }

    // ------------------------------------------------------------------------
    // CgsSceneManager::TriangleCacheManager::ProcessRemoveFromCacheEvents @ 0x828B2710 (346)
    //
    // Release every slot named by this frame's remove queue. The X360 walks
    // lrSceneUpdate.mRemoveFromCacheQueue (re-derived in the callee at +0xC77E0 --
    // `addis r30, r4, 0xC ; addi r30, r30, 0x77E0`) and per event:
    //   1. bounds-assert the slot index against KU_MAX_CACHED_OBJECTS;
    //   2. if the slot's used bit is NOT set, run the three dev cross-checks below;
    //   3. clear mbIsDirty (stb 0x2E) and miOverflow (**sth** 0x2C -- 16-bit),
    //      ZERO THE WHOLE cached sphere (see the ⭐ VMX decode below),
    //      then bounds-assert again and clear the used bit (`andc`, 0x828B2C54).
    //
    // ⭐ WHY THIS ONE TAKES THE WHOLE INTERFACE while its two siblings take a bare
    // queue: the not-set branch scans mAddToCacheQueue as well -- the asm loads the ADD
    // queue's length at +0xC4938 (0x828B2A24) and its base at +0xC4930 (0x828B2A3C) to
    // report "Trying to add and remove triangle cache slot in same frame". So the extra
    // reach is the function's own requirement, not a caller inconsistency.
    //
    // ⭐⭐ FOUR DIVERGENCES FIXED, wave Q6 round 2 (2026-08-19), all re-derived off the raw
    // disassembly of 0x828B2710..0x828B2C74 (dump: scratchpad/waveQ6/asm_828B2710.txt). The
    // previous revision of this body was a plausible *shape* of the console's dev block rather
    // than its control flow, and one of the four changed what an assert MEANS. (a)-(c) are the
    // dev block and are described here; (d), the one with a runtime effect, is the whole-sphere
    // zero and is documented at its own store site further down.
    //
    //  (a) 🔴 "Trying to remove unused triangle cache slot" IS UNCONDITIONAL inside the not-set
    //      block. The console's ADD-queue scan ends at 0x828B2B18 `blt cr6, loc_828B2A40` and
    //      FALLS THROUGH at 0x828B2B1C straight into the `bl BeginAssert` at 0x828B2B20; the
    //      empty-add-queue path reaches the same label directly (0x828B2A30 `ble cr6,
    //      loc_828B2B20`). There is no branch anywhere that can skip it. This TU used to guard it
    //      with `lbAddedThisFrame || lbRemovedAlready`, which INVERTED the assert's meaning: it
    //      made "unused slot" the *residual* message ("and neither cross-check matched") when on
    //      the console it is the *headline* message and the other two are extra detail. That
    //      inverted reading is what wave Q6 round 1 reasoned from when it concluded the prop ADD
    //      leg was not seating slots; see the ⭐⭐ block at the top of this file for the
    //      measurement that settles it the other way.
    //
    //  (b) THE SCAN ORDER IS REVERSED. The console scans THIS QUEUE'S EARLIER EVENTS FIRST
    //      (0x828B2938..0x828B2A14, `bl BaseEventQueue::GetEvent` on r30 == mRemoveFromCacheQueue,
    //      bounded by the outer loop counter) and fires "Trying to remove triangle cache slot
    //      twice" (baked source line 0xFF == 255) from inside it; only then does it scan
    //      mAddToCacheQueue (0x828B2A40..0x828B2B18) and fire "Trying to add and remove triangle
    //      cache slot in same frame" (baked line 0x105 == 261). The unused-slot assert is baked
    //      line 0x107 == 263. Restored to that order. (The baked line numbers are the CONSOLE's
    //      CgsTriangleCacheManager.cpp; this file's CGS_ASSERT carries its own __LINE__.)
    //
    //  (c) NEITHER SCAN BREAKS ON A MATCH. Both console loops fire the assert and keep iterating
    //      (0x828B29FC FireAssert -> 0x828B2A08 increment -> 0x828B2A14 `blt` back; likewise
    //      0x828B2B04 -> 0x828B2B0C -> 0x828B2B18). So N duplicate entries produce N asserts, not
    //      one. This TU used to `break`. Restored -- the assert COUNT is a wave signal and must
    //      not be quietly compressed.
    //
    // ⚠️ The three baked messages are reproduced verbatim from the X360 rodata. They are DEV-ONLY
    // diagnosis of a caller that double-frees or races a slot; the O(n^2) duplicate scans exist to
    // ADD detail to the unconditional third message, not to choose between the three. The scans
    // are kept -- they are the shipped behaviour of this build -- but they compute nothing the
    // release path consumes.
    // ------------------------------------------------------------------------
    void TriangleCacheManager::ProcessRemoveFromCacheEvents(
        const SceneManagerIO::InSceneUpdateInterface& lrSceneUpdate)
    {
        const CgsModule::EventQueue<TriangleCacheManagerIO::InEventRemoveFromCache,
                                    KU_MAX_CACHED_OBJECTS>& lrRemoveQueue =
            lrSceneUpdate.mRemoveFromCacheQueue;

        const s32 liNumRemoveEvents = lrRemoveQueue.GetLength();
        for (s32 liEvent = 0; liEvent < liNumRemoveEvents; ++liEvent)
        {
            // const overload: the caller holds this buffer under IOBuffer::LockForRead,
            // and the mutable GetEvent twin trips the write-lock tripwire.
            const TriangleCacheManagerIO::InEventRemoveFromCache& lrEvent = lrRemoveQueue.GetEvent(liEvent);
            const s32 liCacheSlot = lrEvent.miCacheSlot;

            CGS_ASSERT(static_cast<u32>(liCacheSlot) < KU_MAX_CACHED_OBJECTS, "invalid index : ");

            if (!mUsedCacheSlots.IsBitSet(static_cast<u32>(liCacheSlot)))
            {
                // Scan 1 (X360 0x828B2938..0x828B2A14) -- the EARLIER entries of this same remove
                // queue. Fires per match and keeps scanning; see divergence (b)/(c) above.
                bool lbRemovedAlready = false;
                for (s32 liEarlier = 0; liEarlier < liEvent; ++liEarlier)
                {
                    if (lrRemoveQueue.GetEvent(liEarlier).miCacheSlot == liCacheSlot)
                    {
                        lbRemovedAlready = true;
                        CGS_ASSERT(false, "Trying to remove triangle cache slot twice");
                    }
                }

                // Scan 2 (X360 0x828B2A40..0x828B2B18) -- this frame's ADD queue, reached at
                // +0xC4930 with its length at +0xC4938. Same per-match, no-break shape.
                bool lbAddedThisFrame = false;
                const CgsModule::EventQueue<TriangleCacheManagerIO::InEventAddToCache,
                                            KU_MAX_CACHED_OBJECTS>& lrAddQueue =
                    lrSceneUpdate.mAddToCacheQueue;
                const s32 liNumAddEvents = lrAddQueue.GetLength();
                for (s32 liAdd = 0; liAdd < liNumAddEvents; ++liAdd)
                {
                    if (lrAddQueue.GetEvent(liAdd).miCacheSlot == liCacheSlot)
                    {
                        lbAddedThisFrame = true;
                        CGS_ASSERT(false,
                                   "Trying to add and remove triangle cache slot in same frame");
                    }
                }

                // [DIAG] NOT IN THE X360 BINARY -- fires BEFORE the console's own unconditional
                // assert so the record survives even when the assert manager stops the world.
                // Rate-limited to the first 8; the two tallies are what settle "never seated"
                // (seated=0) versus "already freed" (seated>=1 && freed>=1 == a DOUBLE REMOVE
                // upstream, which is what the four 2026-08-19 asserts were).
                if (DiagEnabled())
                {
                    static u32 suDiagUnusedRemoves = 0;
                    if (suDiagUnusedRemoves < 8u)
                    {
                        ++suDiagUnusedRemoves;
                        *CgsDev::Log::gpDebugPrint
                            << "[Q6-tcache] UNUSED-SLOT REMOVE slot=" << liCacheSlot
                            << " owner=" << DiagSlotOwner(liCacheSlot)
                            << " seatedSoFar=" << DiagRead(sauDiagSeatCount, liCacheSlot)
                            << " freedSoFar=" << DiagRead(sauDiagFreeCount, liCacheSlot)
                            << " addedThisFrame=" << (lbAddedThisFrame ? 1 : 0)
                            << " removedEarlierThisFrame=" << (lbRemovedAlready ? 1 : 0)
                            << " usedNow=" << static_cast<s32>(mUsedCacheSlots.CountSetBits())
                            << "\n";
                    }
                }

                // X360 0x828B2B20 -- UNCONDITIONAL. The two booleans above are consumed only by
                // the diag line; they exist because the console computes them, not because this
                // assert reads them. See divergence (a).
                (void)lbAddedThisFrame;
                (void)lbRemovedAlready;
                CGS_ASSERT(false, "Trying to remove unused triangle cache slot");
            }

            CacheSlot& lrSlot = mpaCachedObjectSlots[liCacheSlot];
            lrSlot.mbIsDirty  = false;
            lrSlot.miOverflow = 0;
            // ⭐ CORRECTED wave Q6 round 2 -- THE WHOLE SPHERE IS ZEROED, not just its radius.
            // X360 0x828B2BDC..0x828B2C08, decoded lane by lane:
            //     vmr128    v0, v127          ; v127 == vspltisw128 0 (hoisted at 0x828B2754)
            //     lvx128    v13, r0, r11      ; the OLD mLastCachedSphere
            //     vrlimi128 v0, v13, 1, 0     ; v0.w <- old w      (DEAD -- overwritten below)
            //     vrlimi128 v0, v127, 1, 0    ; v0.w <- 0
            //     stvx128   v0, r0, r11       ; store {0,0,0,0}
            // v0's x/y/z lanes never come from v13 at all: they are the zero register's, because
            // both merges have mask 1 (the w lane only). The previous revision of this line read
            // "the net effect on the stored vector is the w lane taken from v127" and wrote only
            // `.w = 0` -- that dropped the base vector and left a released slot's CENTRE stale.
            // It matters: ProcessAddToCacheEvents re-stamps only the radius (its store is a
            // read-modify-write that keeps x/y/z), so on the console a recycled slot starts at the
            // origin, while in the old tree it inherited the previous owner's position and kept it
            // until something posted an InEventUpdateCachedPosition for it. Props post NONE today
            // (PropManager::UpdateTriangleCache is still the inert gate at
            // BrnPhysicsConductorGates.cpp:522-527), so for every prop slot that stale centre was
            // permanent -- exactly the input StartUpdateTriangleCaches hands the fill job.
            lrSlot.mLastCachedSphere.x = 0.0f;
            lrSlot.mLastCachedSphere.y = 0.0f;
            lrSlot.mLastCachedSphere.z = 0.0f;
            lrSlot.mLastCachedSphere.w = 0.0f;

            // CgsBitArray.h:241 -- UnSetBit's OWN bound, a different rodata entry from the
            // IsBitSet :203 tripwire above: X360 0x828B2C18 `bl BeginAssert` / 0x828B2C1C
            // `li r5, 0xF1` (== 241) / 0x828B2C24 `lwz r3, var_FC` where var_FC was stored at
            // 0x828B276C from aLuindexNumbits == "luIndex < NUMBITS" -- a plain baked string,
            // no stream formatting.
            CGS_ASSERT(static_cast<u32>(liCacheSlot) < KU_MAX_CACHED_OBJECTS, "luIndex < NUMBITS");
            mUsedCacheSlots.UnSetBit(static_cast<u32>(liCacheSlot));

            // [DIAG] NOT IN THE X360 BINARY -- one bounds-checked increment, no print.
            if (DiagEnabled())
            {
                DiagCount(sauDiagFreeCount, liCacheSlot);
            }
        }
    }

    // ------------------------------------------------------------------------
    // CgsSceneManager::TriangleCacheManager::ProcessAddToCacheEvents @ 0x828B2C78 (222)
    //
    // Claim a slot for every object that entered the cache this frame. Per event:
    //   1. bounds-assert the slot index (`cmplwi r29, 0x12A` == 298);
    //   2. assert the slot is NOT already claimed -- the baked message names the member
    //      and the field verbatim: "!mUsedCacheSlots.IsBitSet( lEvent.miCacheSlot )";
    //   3. stamp the slot's cached-sphere RADIUS from the event
    //      (lvlx at event+4 -> vspltw -> vrlimi128 into the w lane) and zero its batch
    //      count (`stw r26, 0x28(r11)` with r26 == 0);
    //   4. set the used bit (`ldx`/`or`/`stdx` off this+8, 0x828B2FCC..0x828B2FD8).
    // The slot address is `idx*3 << 4` == idx * 48, the committed CacheSlot stride.
    //
    // Note the ORDER of the two writes in step 3 matches the X360 (`stw` of the batch
    // count is emitted between the lvx128 and the stvx128 of the sphere) -- they touch
    // disjoint fields, so the interleave is scheduling, not semantics.
    // ------------------------------------------------------------------------
    void TriangleCacheManager::ProcessAddToCacheEvents(
        const CgsModule::EventQueue<TriangleCacheManagerIO::InEventAddToCache,
                                    KU_MAX_CACHED_OBJECTS>& lrQueue)
    {
        const s32 liNumEvents = lrQueue.GetLength();
        for (s32 liEvent = 0; liEvent < liNumEvents; ++liEvent)
        {
            const TriangleCacheManagerIO::InEventAddToCache& lrEvent = lrQueue.GetEvent(liEvent);
            const s32 liCacheSlot = lrEvent.miCacheSlot;

            CGS_ASSERT(static_cast<u32>(liCacheSlot) < KU_MAX_CACHED_OBJECTS, "invalid index : ");
            CGS_ASSERT(!mUsedCacheSlots.IsBitSet(static_cast<u32>(liCacheSlot)),
                       "!mUsedCacheSlots.IsBitSet( lEvent.miCacheSlot )");

            CacheSlot& lrSlot = mpaCachedObjectSlots[liCacheSlot];
            // VMX lane merge into w, and ONLY into w: 0x828B2EA0 `lvx128 v13` loads the existing
            // sphere and 0x828B2EA8 `vrlimi128 v13, v0, 1, 0` inserts the splatted radius into its
            // w lane before the store, so x/y/z survive the claim. (Contrast the remove leg, whose
            // merge base is the ZERO register -- that asymmetry is the console's, and it is why
            // zeroing the sphere on release is load-bearing rather than cosmetic.)
            lrSlot.mLastCachedSphere.w      = lrEvent.mfCacheSphereRadius;
            lrSlot.miNumCachedTriangleBatches = 0;

            // CgsBitArray.h:222 -- SetBit's OWN bound, again distinct from the :203 IsBitSet
            // tripwire above: X360 0x828B2FA4 `li r5, 0xDE` (== 222) with the message STREAMED
            // from r20 == aIndex_0 ("Index: ") + the index + aNumberOfBits (", Number of bits: ")
            // + 0x12A (298). Reduced to the project's static expression form, as at
            // CgsOverlapCullingModule.cpp:511 and BrnCameraValidityAccount.cpp:45.
            CGS_ASSERT(static_cast<u32>(liCacheSlot) < KU_MAX_CACHED_OBJECTS, "Index < Number of bits");
            mUsedCacheSlots.SetBit(static_cast<u32>(liCacheSlot));

            // [DIAG] NOT IN THE X360 BINARY -- wave Q6 round 2. One-shot for the first slot claimed
            // OUTSIDE the vehicle/traffic window (0..27), i.e. the first slot a PROP or a PROP PART
            // ever owns. This is the line that answers round 1's open question directly: if it
            // prints, the prop triangle-cache ADD leg reaches this manager and seats its bit, and
            // any later "Trying to remove unused triangle cache slot" is an upstream double remove,
            // not a lost add. If it never prints across a drive that breaks props, the add leg IS
            // the hole and the producers in PropManager_wQ2_06.cpp / _wQ2_04.cpp are where to look.
            if (DiagEnabled())
            {
                DiagCount(sauDiagSeatCount, liCacheSlot);

                static bool sbDiagFirstPropSlotReported = false;
                if (!sbDiagFirstPropSlotReported && (liCacheSlot >= KI_DIAG_PROP_WINDOW_FIRST))
                {
                    sbDiagFirstPropSlotReported = true;
                    *CgsDev::Log::gpDebugPrint
                        << "[Q6-tcache] first prop cache slot seated: slot=" << liCacheSlot
                        << " owner=" << DiagSlotOwner(liCacheSlot)
                        << " radius=" << lrEvent.mfCacheSphereRadius
                        << " usedNow=" << static_cast<s32>(mUsedCacheSlots.CountSetBits())
                        << "\n";
                }
            }
        }
    }

    // ------------------------------------------------------------------------
    // CgsSceneManager::TriangleCacheManager::ProcessUpdateCachedPositionEvents @ 0x828BE898 (279)
    //
    // Move every cached object that reported a new position this frame. Per event:
    //   1. bounds-assert the slot index;
    //   2. assert the slot IS claimed -- baked message "Bit not set for event index "
    //      (the X360 then appends the index, "\n lEvent.miCacheSlot = " and
    //      "\n lEvent.mPositionAndRadius = " through StrStreamBase; this tree's
    //      CGS_ASSERT carries the leading literal, as elsewhere);
    //   3. delegate to CacheSlot::UpdateCachedObject -- a real `bl` at 0x828BEC38, NOT
    //      inlined, so it is called here rather than open-coded;
    //   4. for slots below KI_MAX_RADIUS_TRACKED_SLOTS only (`cmpwi r11, 8 ; bge`),
    //      fold the event's radius into mvfMaxRadiusSoFar if it is larger. The X360
    //      keeps mvfMaxRadiusSoFar SPLATTED across all four lanes (vspltw v0,v0,3 then
    //      stvx128 of the whole vector), so all four are written, not just w. The
    //      compare is vcmpgtfp. on the splatted lanes -- a scalar `>` on the w lane is
    //      the same predicate.
    // ⚠️ ONE THING IS DELIBERATELY NOT REPRODUCED, and it is a console PRINT, not a store:
    //    after the splat store the X360 tests `CgsDev::Message::gxMessageFilterFlags & 1`
    //    (0x828BEC84 `ld` + `clrldi r10,r10,63`) and, when set, streams
    //    "\nNew largest cache radius: " followed by the new radius through gpDebugPrint
    //    (0x828BEC94..0x828BECC0). It is behaviourally inert -- it reads only the value just
    //    stored -- and this tree has no committed gxMessageFilterFlags home, so reproducing it
    //    would mean inventing the gate. Recorded here as a known, bounded omission rather than
    //    left unmentioned; the store above IS the whole of the function's effect.
    // ------------------------------------------------------------------------
    void TriangleCacheManager::ProcessUpdateCachedPositionEvents(
        const CgsModule::EventQueue<TriangleCacheManagerIO::InEventUpdateCachedPosition,
                                    KU_MAX_CACHED_OBJECTS>& lrQueue)
    {
        const s32 liNumEvents = lrQueue.GetLength();
        for (s32 liEvent = 0; liEvent < liNumEvents; ++liEvent)
        {
            const TriangleCacheManagerIO::InEventUpdateCachedPosition& lrEvent = lrQueue.GetEvent(liEvent);
            const s32 liCacheSlot = lrEvent.miCacheSlot;

            CGS_ASSERT(static_cast<u32>(liCacheSlot) < KU_MAX_CACHED_OBJECTS, "invalid index : ");
            CGS_ASSERT(mUsedCacheSlots.IsBitSet(static_cast<u32>(liCacheSlot)),
                       "Bit not set for event index ");

            mpaCachedObjectSlots[liCacheSlot].UpdateCachedObject(lrEvent);

            if (liCacheSlot < KI_MAX_RADIUS_TRACKED_SLOTS)
            {
                const f32 lfRadius = lrEvent.mNewPositionAndRadius.w;
                if (lfRadius > mvfMaxRadiusSoFar.w)
                {
                    // Splatted across all four lanes, as the X360 stores it.
                    mvfMaxRadiusSoFar.x = lfRadius;
                    mvfMaxRadiusSoFar.y = lfRadius;
                    mvfMaxRadiusSoFar.z = lfRadius;
                    mvfMaxRadiusSoFar.w = lfRadius;
                }
            }
        }
    }
}
