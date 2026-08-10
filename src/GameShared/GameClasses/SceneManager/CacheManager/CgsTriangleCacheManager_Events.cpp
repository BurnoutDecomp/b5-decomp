#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManager.h"

#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h" // InSceneUpdateInterface (mAddToCacheQueue / mRemoveFromCacheQueue)
#include "GameShared/GameClasses/Core/CgsAssert.h"                             // CGS_ASSERT

#include <math.h>   // sqrtf -- the portable lowering of the X360 vrsqrtefp + 2 Newton-Raphson steps

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
// ⛔ REACHABILITY, stated plainly so nobody reads more into this TU than is there:
// the sole caller, SceneManagerModule::StartUpdateTriangleCache, is still a link stub
// (WorldLinkStubs.cpp:3605), as is TriangleCacheManager::EndUpdateTriangleCaches
// (WorldLinkStubs.cpp:2379). These four bodies therefore run ZERO times today. They
// are mounted so the link closure over them is enforced (the shadowing-redeclaration
// rule: a per-TU compile gate cannot catch a mismatched redeclaration, only a LINK
// can), and /OPT:REF keeps their bytes out of the exe until something reaches them.
// Nothing here fills the cache with triangles either -- that is StartUpdateTriangleCaches
// @0x828BECF8 + the PolygonSoupTesterJob fill path, neither of which exists yet.
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
    //      drop the cached sphere's radius to 0 (vrlimi128 lane merge + one stvx128),
    //      then bounds-assert again and clear the used bit (`andc`, 0x828B2C54).
    //
    // ⭐ WHY THIS ONE TAKES THE WHOLE INTERFACE while its two siblings take a bare
    // queue: the not-set branch scans mAddToCacheQueue as well -- the asm loads the ADD
    // queue's length at +0xC4938 (0x828B2A24) and its base at +0xC4930 (0x828B2A3C) to
    // report "Trying to add and remove triangle cache slot in same frame". So the extra
    // reach is the function's own requirement, not a caller inconsistency.
    //
    // ⚠️ The three baked messages are reproduced verbatim from the X360 rodata. They are
    // DEV-ONLY diagnosis of a caller that double-frees or races a slot; the O(n^2)
    // duplicate scans exist purely to choose between them. The scans are kept -- they are
    // the shipped behaviour of this build and CGS_ASSERT compiles out with them -- but
    // they compute nothing the release path consumes.
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
                // Which kind of bad remove is it? The X360 asks the ADD queue first,
                // then the earlier entries of this same remove queue, then gives up.
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
                        break;
                    }
                }

                bool lbRemovedAlready = false;
                for (s32 liEarlier = 0; liEarlier < liEvent; ++liEarlier)
                {
                    if (lrRemoveQueue.GetEvent(liEarlier).miCacheSlot == liCacheSlot)
                    {
                        lbRemovedAlready = true;
                        break;
                    }
                }

                CGS_ASSERT(!lbAddedThisFrame,
                           "Trying to add and remove triangle cache slot in same frame");
                CGS_ASSERT(!lbRemovedAlready, "Trying to remove triangle cache slot twice");
                CGS_ASSERT(lbAddedThisFrame || lbRemovedAlready,
                           "Trying to remove unused triangle cache slot");
            }

            CacheSlot& lrSlot = mpaCachedObjectSlots[liCacheSlot];
            lrSlot.mbIsDirty  = false;
            lrSlot.miOverflow = 0;
            // VMX: one lvx128 + two vrlimi128 + one stvx128 at 0x828B2BF4..0x828B2C08,
            // whose net effect on the stored vector is the w lane taken from the zero
            // register v127. A zeroed cached radius is what stops the freed slot matching
            // any later sphere test. Lowered to the single field the merge lands in.
            lrSlot.mLastCachedSphere.w = 0.0f;

            CGS_ASSERT(static_cast<u32>(liCacheSlot) < KU_MAX_CACHED_OBJECTS, "invalid index : ");
            mUsedCacheSlots.UnSetBit(static_cast<u32>(liCacheSlot));
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
            // VMX lane merge into w; see the banner note.
            lrSlot.mLastCachedSphere.w      = lrEvent.mfCacheSphereRadius;
            lrSlot.miNumCachedTriangleBatches = 0;

            CGS_ASSERT(static_cast<u32>(liCacheSlot) < KU_MAX_CACHED_OBJECTS, "invalid index : ");
            mUsedCacheSlots.SetBit(static_cast<u32>(liCacheSlot));
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
    //      the same predicate. The baked message for this branch is
    //      "\nNew largest cache radius: ".
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
