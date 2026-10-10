#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                        // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                                // CgsDev::Log::gpDebugPrint ([tricache] probe)
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"                 // PerfMonCpu::Start/StopMonitor
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"   // DebugInterface (overflow overlay)
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"          // DebugManager::ThreadSafeRelease
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"         // DebugRender::Draw2DText
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupListSpatialMap.h" // the leaf-node guard
#include "GameShared/GameClasses/Memory/DataStream/CgsSimpleDataStreamProducer.h"         // SimpleDataStreamProducer + its iterator
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h" // BaseCollisionGenerator
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsFillTriangleCacheStreamJobDesc.h" // the stream records

#include "SDKs/EATech/eajobs/job.h"   // EA::Jobs::Job::WaitOn

#include <cstdio>   // std::snprintf -- the console's sprintf @0x82C0CB70 (overflow overlay)
#include <cstdlib>  // getenv  -- the [tricache] probe's opt-in latch
#include <cmath>    // sqrtf   -- the [tricache] probe's car-vs-cached lag

// =================================================================================================
// GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManager_Update.cpp
//
// ⭐ THE TRIANGLE CACHE'S FILL HALF, reconstructed from BURNOUT_X360_ARTIST.XEX:
//   CgsSceneManager::TriangleCacheManager::StartUpdateTriangleCaches @0x828BECF8 (278)
//   CgsSceneManager::TriangleCacheManager::EndUpdateTriangleCaches   @0x828BF150 (475)
//
// The pair is the WRITE half of the cache: Start carves a command stream out of the frame's
// collision generator and posts one fill request per DIRTY cache slot; the job fills each slot's
// window of the shared Triangle4 cache from the polygon-soup spatial map; End waits on that job
// and folds each answer (batch count + overflow) back into the slot table. The slot BOOKKEEPING
// that decides which slots are used and dirty is the sibling TU
// CgsTriangleCacheManager_Events.cpp; the READ side (GetTrianglesForCachedObject and the
// SceneManagerIO::TriangleCacheInterface accessors) is CgsTriangleCacheManager.cpp.
//
// -------------------------------------------------------------------------------------------------
// REACHABILITY.
//
//  * EndUpdateTriangleCaches: SceneManagerModule::EndUpdateTriangleCache is called by
//    WorldModule::Update every frame. This body takes its own opening null guard on
//    mpUpdateTriangleCacheStream -- exactly as the console does whenever a frame posted no fill.
//
//  * StartUpdateTriangleCaches: its sole caller is SceneManagerModule::StartUpdateTriangleCache
//    (CgsSceneManagerModule.cpp), which first runs TriangleCollisionManager::
//    ProcessAddPolySoupListEvents and, with it, PolygonSoupListSpatialMap::BuildSpacialPartition.
//    The partition is built from the world-collision stage (LoadingScriptedState::
//    LoadWorldCollision -> WorldModule::PrepareWorldCollision -> WorldEntityModule::
//    PrepareWorldCollision/PrepareZoneCollision -> AddCollisionZoneToSceneManager); all 396
//    "TRK_CLIL<n>" zone lists are acquired 20 per frame, and the last batch's rebuild flag reaches
//    ProcessAddPolySoupListEvents.
//
//  * SLOT CLAIMS: `usedSlots` is the popcount of mUsedCacheSlots, whose only setter is
//    ProcessAddToCacheEvents draining mAddToCacheQueue -- filled on the PREPARE path by
//    VehicleManager::PrepareTriangleCache (8 race cars) -> PhysicalTrafficManager::
//    PrepareTriangleCache (20 traffic), reached through VehicleManager::Prepare.
//    UpdateCachedPositions fills a DIFFERENT queue whose consumer asserts the used bit is ALREADY
//    set, so it never moves this number.
//    28 == KI_MAX_ACTIVE_RACE_CARS(8) + KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC(20).
//
//  * THE ARENA: the shared triangle cache's backing store is CachedTriangleList::Prepare
//    (CgsCachedTriangleList.cpp); the console's own dev report confirms the size at runtime:
//        CachedTriangleList: Total triangle cache requires 2937088 bytes    (== 13112 * 224)
//
//  * DIRTY SLOTS: the loop below allocates a command only for a DIRTY slot, and a slot is dirtied
//    through exactly two doors. Door 1 is CacheSlot::UpdateCachedObject, reached only from an
//    InEventUpdateCachedPosition, and `xrefs_to` on that queue's AddEvent gives exactly FIVE
//    producers -- PhysicalTrafficManager / DetachedPartManager / DetachedWheelManager /
//    PropManager / VehicleManager ::UpdateTriangleCache -- and EVERY ONE is a loop over LIVE
//    PHYSICS OBJECTS. Door 2 is the dev switch mbDEBUGForceAllDirty below. There is no third.
//    ⇒ the triangle cache is filled AROUND live objects; it cannot be filled before one exists.
//    UpdateCachedObject dirties unconditionally while miNumCachedTriangleBatches == 0, so the
//    FIRST position event a car posts dirties its slot -- no distance threshold has to be crossed.
//
//  * ⚠️ `UpdateCachedPositions` CALLS THREE MANAGERS, NOT SIX: VehicleManager::UpdateTriangleCache,
//    PropManager:: and DeformationManager::. The other three are CALLEES (DeformationManager's 35
//    instructions are a pure conductor onto DetachedPart + DetachedWheel; PhysicalTraffic hangs off
//    the vehicle one). Its only console caller is WorldModule::Update -- NOT PhysicsModule::Update.
//
// -------------------------------------------------------------------------------------------------
// METHOD (standing discipline: read the ASM, not the pseudocode). Both Hex-Rays listings are
// dominated by CgsDev::StrStreamBase assert formatting; every decode below is off the
// disassembly. Nothing here is invented:
//   * The 32-byte command / 16-byte result geometry is NOT inferred from the field list -- the
//     factory BaseCollisionGenerator::CreateStreamProducer @0x828109F8 passes exactly
//     GetRequiredBufferSizes(298, 0x20, 298, 0x10, ...) (`li r4, 0x20` / `li r6, 0x10`).
//   * The per-slot batch cap 44 (`li r10, 0x2C ; sth r10, 0x14(cmd)`) is corroborated
//     independently: TriangleCacheManager::Prepare sizes the shared cache at
//     CachedTriangleList::Prepare(alloc, 13112) and 298 * 44 == 13112 exactly.
//   * The tail block 0x828BF0DC..0x828BF120 is an INLINED SimpleDataStreamProducer::Begin --
//     six lwz/stw pairs +0x20..+0x34 -> +0x00..+0x14, then +0x18 = &mCommandPoster, +0x100 = 1,
//     then DataStreamCommandPoster::Begin. That is store-for-store the body already committed in
//     CgsSimpleDataStreamProducer_Begin.cpp, so it is called by name here rather than re-inlined.
//   * The paired GetCurrent calls in End (0x828BF2B0 / 0x828BF2DC) with a `++` on the cursor
//     between them are `GetCurrent()` then `GetNext()` -- the committed GetNext is exactly
//     `++miResultIndex; return GetCurrent();`, and the console drops the second answer.
//   * The overflow overlay's format string, its three float constants and its two DebugManager
//     asserts were READ OUT OF THE IMAGE at the addresses the asm names (fmt @0x820F5668,
//     x/y0 @0x820049E0 = 100.0f, y step @0x820F56B8 = 22.0f, scale @0x820054CC = 20.0f,
//     colour 0xFF0000FF built by `lis -0x100 / ori 0xFF`).
// =================================================================================================

namespace CgsSceneManager
{
    // ---------------------------------------------------------------------------------------------
    // The three CPU perfmon handles this pair brackets its work with. On the X360 these are
    // file-scope statics of this TU (dword_82F33F48 / dword_82F33F4C / dword_82F33F50) and in the
    // ARTIST image all three READ -1 -- i.e. nothing in this build ever registers them, so every
    // `if (handle > -1)` guard below is false and no monitor is entered. Reproduced as shipped
    // (values read from the image, not assumed); the names are descriptive of the span each one
    // brackets, since no AddMonitor call site names them.
    // ---------------------------------------------------------------------------------------------
    static s32 s_miBuildCacheCommandsPerfMon = -1;  // dword_82F33F48
    static s32 s_miDispatchCacheFillPerfMon  = -1;  // dword_82F33F4C
    static s32 s_miReadCacheResultsPerfMon   = -1;  // dword_82F33F50

    // ---------------------------------------------------------------------------------------------
    // ---- [tricache] PC bring-up instrument -- DELETE WHEN world collision is proven map-wide ----
    //
    // OPT-IN (BRN_TRICACHE_PROBE=1) so a DEFAULT run and every golden gate stay byte-identical to a
    // build without it: the latch below reads 0 on its first call and every print is unreachable
    // thereafter. Same discipline as [motion] in BrnRaceCarEntityModule and [wall] in
    // BrnDeformationManager_Contacts.
    //
    // ⭐ WHY THIS INSTRUMENT AND NOT ANOTHER. The player reports collision only near the Junkyard.
    // A read-only audit already proved the world DATA is complete map-wide (23,645 soups, 396
    // zones, 25 simulated probes across the map all returning geometry), so the remaining question
    // is a RUNTIME one that only a DRIVEN car can answer: does the per-object triangle cache follow
    // the car? The slot already carries both numbers and nothing else has to be computed --
    //   mInnerSpherePositionAndRadius is where the car IS (written every frame by
    //   CacheSlot::UpdateCachedObject, unconditionally), and
    //   mLastCachedSphere is where the triangles WERE CACHED (moved only when the dirty test trips).
    // Printing the pair plus the fill's own answers splits the fault four ways in one run:
    //   car frozen                    -> the position leg (VehicleManager::UpdateTriangleCache /
    //                                    mUsedRaceCars / RaceCarPhysics::GetPosition) is dead;
    //   car moves, cached does not    -> UpdateCachedObject's dirty test or the Start/End pairing;
    //   cached tracks, batches -> 0   -> the spatial map/query fails at runtime, not in the data;
    //   cached tracks, batches > 0    -> the cache is healthy and the fault is downstream.
    //
    // ⚠️ `lag` and the two counts are DERIVED FOR THE PROBE, not console state: lag is
    // |car - cached| (healthy steady state is under KF_TRIANGLE_CACHE_SPHERE_RADIUS minus the car's
    // own radius), and the used/dirty counts are re-scanned here rather than accumulated in the
    // shipped loop above so that loop stays exactly as the X360 emits it. The re-scan is EXACT:
    // nothing between the loop and here writes mbIsDirty, so `dirty` == the number of commands
    // Start posted this frame.
    // ⚠️ `batches`/`ovf` are read at START, so they are the PREVIOUS frame's fold -- End is what
    // writes them. That is deliberate (Start is the only site that knows the posted count and still
    // sees mbIsDirty before End clears it) and it is stated here so nobody reads a one-frame-old
    // number as this frame's answer. At a 60-frame period the distinction cannot matter.
    // ---------------------------------------------------------------------------------------------
    static bool TriCacheProbeArmed()
    {
        static s32 siTriCacheProbe = -1;
        if (siTriCacheProbe < 0)
        {
            const char* lpcEnv = getenv("BRN_TRICACHE_PROBE");
            siTriCacheProbe = (lpcEnv != NULL && lpcEnv[0] != '0') ? 1 : 0;
        }
        return (siTriCacheProbe == 1) && (CgsDev::Log::gpDebugPrint != NULL);
    }

    // The sampling period, in entries into StartUpdateTriangleCaches. BRN_TRICACHE_PROBE=1 keeps
    // the default 60 (~once a second); BRN_TRICACHE_PROBE=<n> sets it to n.
    // ⚠️⚠️ 60 IS TOO COARSE ONCE THE CAR IS ACTUALLY DRIVING, and that is not a preference -- it is
    // a measured miss. At the 29.7 m/s this build reaches, 60 frames is 29 METRES, so the whole
    // approach to the obstacle the car fell through (~10 frames) fell between two samples and the
    // trace could not say whether the cache held the wall face at the moment of contact. A period
    // that cannot resolve the event is a probe that answers a different question than the one asked.
    static u32 TriCacheProbePeriod()
    {
        static u32 suPeriod = 0u;
        if (suPeriod == 0u)
        {
            suPeriod = 60u;
            const char* lpcEnv = getenv("BRN_TRICACHE_PROBE");
            if (lpcEnv != NULL)
            {
                const int liValue = atoi(lpcEnv);
                if (liValue > 1)
                {
                    suPeriod = static_cast<u32>(liValue);
                }
            }
        }
        return suPeriod;
    }

    // The counter is shared by both call sites below so the two lines interleave in frame order and
    // a frame that took the spatial-map early-out is visible as a GAP, not as silence.
    static u32 s_uTriCacheProbeFrame = 0u;

    // =============================================================================================
    // TriangleCacheManager::StartUpdateTriangleCaches @0x828BECF8 (278 insns)
    // =============================================================================================
    void TriangleCacheManager::StartUpdateTriangleCaches(
        CgsCollision::BaseCollisionGenerator*          lpCollisionGenerator,
        const CgsGeometric::PolygonSoupListSpatialMap* lpPolySoupListSpacialMap)
    {
        // Baked lines 0x169 / 0x16A of CgsTriangleCacheManager.cpp. The misspelling in the second
        // message is the console's own.
        CGS_ASSERT(lpCollisionGenerator != NULL, "lpCollisionGenerator != NULL");
        CGS_ASSERT(lpPolySoupListSpacialMap != NULL, "lpPolySoupListSpacialMap != NULL");


        ++s_uTriCacheProbeFrame;   // [tricache]: counted before the early-out, so a gap is visible.

        // 0x828BED68: no spatial partition built -> nothing to query against, so no stream is
        // created and no command is posted. The matching End then takes ITS null guard.
        if (lpPolySoupListSpacialMap->GetLeafNodes() == NULL)
        {
            // ---- [tricache] ----------------------------------------------------------------
            // The one branch that silently fills nothing. If the map is present at spawn and
            // absent later, the whole bug is here and no other line in this file would say so.
            if (((s_uTriCacheProbeFrame % TriCacheProbePeriod()) == 0u) && TriCacheProbeArmed())
            {
                *CgsDev::Log::gpDebugPrint
                    << "[tricache] n " << static_cast<s32>(s_uTriCacheProbeFrame)
                    << " NO SPATIAL MAP (GetLeafNodes()==NULL) -- no fill posted this frame\n";
            }
            // ---- end [tricache] ------------------------------------------------------------
            return;
        }

        if (s_miBuildCacheCommandsPerfMon > -1)
        {
            CgsDev::PerfMonCpu::StartMonitor(s_miBuildCacheCommandsPerfMon);
        }

        // 0x828BED8C: one producer per frame, sized for the whole slot pool.
        mpUpdateTriangleCacheStream =
            lpCollisionGenerator->CreateFillTriangleCacheStream(static_cast<s32>(KU_MAX_CACHED_OBJECTS));

        // Walk every USED slot. The console open-codes BitArray<298>'s first/next-set-bit scan
        // (five 64-bit fields, `w & (w-1)` lowest-bit isolate, `cntlzd`); it is called by name
        // here -- the committed GetFirstNonZeroBit / GetNextNonZeroBit reproduce that scan
        // including its ">= tuNumBits -> -1" clamp. (The console also re-asserts
        // "invalid index : <n> < 298" once per hop; that tripwire cannot fire behind the same
        // clamp and the bounds asserts are the caller's by this header family's convention.)
        for (s32 liSlotIndex = mUsedCacheSlots.GetFirstNonZeroBit();
             liSlotIndex != CgsContainers::BitArray<KU_MAX_CACHED_OBJECTS>::KI_INVALID_BITINDEX;
             liSlotIndex = mUsedCacheSlots.GetNextNonZeroBit(liSlotIndex))
        {
            CacheSlot& lrSlot = mpaCachedObjectSlots[liSlotIndex];

            // 0x828BEE48: the DEBUG override re-dirties every slot it visits.
            if (mbDEBUGForceAllDirty)
            {
                lrSlot.mbIsDirty = true;
            }

            if (!lrSlot.mbIsDirty)
            {
                continue;
            }

            // 0x828BEE84: reserve one command record in the stream.
            void* lpvCommand = NULL;
            mpUpdateTriangleCacheStream->AllocateCommand(&lpvCommand);

            CgsCollision::FillTriangleCacheStreamJobDesc::StreamCommand* lpCommand =
                static_cast<CgsCollision::FillTriangleCacheStreamJobDesc::StreamCommand*>(lpvCommand);

            // 0x828BEED0..0x828BEEE0: the slot's cached sphere is copied verbatim (two 8-byte
            // moves of the same 16 bytes) and becomes the fill query.
            lpCommand->mCacheSphere.mPositionRadius.x = lrSlot.mLastCachedSphere.x;
            lpCommand->mCacheSphere.mPositionRadius.y = lrSlot.mLastCachedSphere.y;
            lpCommand->mCacheSphere.mPositionRadius.z = lrSlot.mLastCachedSphere.z;
            lpCommand->mCacheSphere.mPositionRadius.w = lrSlot.mLastCachedSphere.w;

            // 0x828BEEBC..0x828BEECC: the destination is this slot's own window into the shared
            // cache (`mulli r11, r29, 0xE0` == index * sizeof(Triangle4)), reached through the
            // CachedTriangleList accessor rather than by raw pointer arithmetic -- that accessor
            // IS the console's inlined "mpaTriangleCache != NULL" tripwire (baked line 0xA2), so
            // it is not duplicated here.
            lpCommand->mpDestinationTriangles = const_cast<CgsGeometric::Triangle4*>(
                mTrianglesForCachedObjects.GetCachedTriangle(lrSlot.miIndexIntoTriangleCache));

            // 0x828BEEC8/0x828BEEDC: `li r10, 0x2C ; sth r10, 0x14(cmd)`.
            lpCommand->mu16MaxNumTriangleBatches = KU_TRIANGLE_BATCHES_PER_CACHED_OBJECT;
        }

        // ---- [tricache] the four-way discriminator; see the block above the perfmon handles ----
        if (((s_uTriCacheProbeFrame % TriCacheProbePeriod()) == 0u) && TriCacheProbeArmed())
        {
            // Re-scan rather than instrument the shipped loop. `liDirty` IS the command count:
            // nothing above writes mbIsDirty except the mbDEBUGForceAllDirty override, which this
            // scan sees too because it runs after it.
            s32 liUsed  = 0;
            s32 liDirty = 0;
            for (s32 liScan = mUsedCacheSlots.GetFirstNonZeroBit();
                 liScan != CgsContainers::BitArray<KU_MAX_CACHED_OBJECTS>::KI_INVALID_BITINDEX;
                 liScan = mUsedCacheSlots.GetNextNonZeroBit(liScan))
            {
                ++liUsed;
                if (mpaCachedObjectSlots[liScan].mbIsDirty)
                {
                    ++liDirty;
                }
            }

            *CgsDev::Log::gpDebugPrint
                << "[tricache] n " << static_cast<s32>(s_uTriCacheProbeFrame)
                << " used " << liUsed << " posted " << liDirty << "\n";

            // ⚠️⚠️ ALL EIGHT RACE-CAR SLOTS, NOT SLOT 0. The first cut of this probe printed slot 0
            // only, on the reasoning that VehicleManager::PrepareTriangleCache claims slots 0..7 as
            // `miCacheSlot = liRaceCar` and race car 0 is the starter car. The slot mapping is
            // right and the conclusion was wrong: this build's LOCAL PLAYER is not race car 0. One
            // booted run printed `playerIdx 2` while slot 0 sat at the spawn point for 29,280
            // frames, so the probe reported "the car never moves" about a car nobody was driving --
            // a lying diagnostic of exactly the kind this campaign keeps tripping over. The manager
            // has no notion of which slot is the player, so print them all and let the trace say
            // which one moves.
            if (mpaCachedObjectSlots != NULL)
            {
                for (u32 luSlot = 0u; luSlot < 8u; ++luSlot)
                {
                    if (!mUsedCacheSlots.IsBitSet(luSlot))
                    {
                        continue;
                    }

                    const CacheSlot& lrSlot = mpaCachedObjectSlots[luSlot];
                    const f32 lfDeltaX = lrSlot.mInnerSpherePositionAndRadius.x - lrSlot.mLastCachedSphere.x;
                    const f32 lfDeltaY = lrSlot.mInnerSpherePositionAndRadius.y - lrSlot.mLastCachedSphere.y;
                    const f32 lfDeltaZ = lrSlot.mInnerSpherePositionAndRadius.z - lrSlot.mLastCachedSphere.z;
                    const f32 lfLag = sqrtf((lfDeltaX * lfDeltaX) + (lfDeltaY * lfDeltaY)
                                            + (lfDeltaZ * lfDeltaZ));

                    *CgsDev::Log::gpDebugPrint
                        << "[tricache] n " << static_cast<s32>(s_uTriCacheProbeFrame)
                        << " s"            << static_cast<s32>(luSlot)
                        << " car "         << lrSlot.mInnerSpherePositionAndRadius.x
                        << " "             << lrSlot.mInnerSpherePositionAndRadius.y
                        << " "             << lrSlot.mInnerSpherePositionAndRadius.z
                        << " carR "        << lrSlot.mInnerSpherePositionAndRadius.w
                        << " cached "      << lrSlot.mLastCachedSphere.x
                        << " "             << lrSlot.mLastCachedSphere.y
                        << " "             << lrSlot.mLastCachedSphere.z
                        << " cachedR "     << lrSlot.mLastCachedSphere.w
                        << " lag "         << lfLag
                        << " batches "     << lrSlot.miNumCachedTriangleBatches
                        << " ovf "         << static_cast<s32>(lrSlot.miOverflow)
                        << " dirty "       << static_cast<s32>(lrSlot.mbIsDirty ? 1 : 0)
                        << "\n";
                }
            }
        }
        // ---- end [tricache] --------------------------------------------------------------------

        if (s_miBuildCacheCommandsPerfMon > -1)
        {
            CgsDev::PerfMonCpu::StopMonitor(s_miBuildCacheCommandsPerfMon);
        }
        if (s_miDispatchCacheFillPerfMon > -1)
        {
            CgsDev::PerfMonCpu::StartMonitor(s_miDispatchCacheFillPerfMon);
        }

        // 0x828BF0DC..0x828BF120 (inlined) then 0x828BF130.
        mpUpdateTriangleCacheStream->Begin();
        mpUpdateTriangleCacheJob = lpCollisionGenerator->RunFillTriangleCacheStream(
            lpPolySoupListSpacialMap, mpUpdateTriangleCacheStream);

        if (s_miDispatchCacheFillPerfMon > -1)
        {
            CgsDev::PerfMonCpu::StopMonitor(s_miDispatchCacheFillPerfMon);
        }
    }

    // =============================================================================================
    // TriangleCacheManager::EndUpdateTriangleCaches @0x828BF150 (475 insns)
    //
    // ⚠️ AS-SHIPPED: the two parameters are never read (the only `arg_` slot in 475 instructions is
    // a spill of `this`, reloaded twice). They are kept because the call site materialises them.
    // =============================================================================================
    void TriangleCacheManager::EndUpdateTriangleCaches(void* /*lpCollisionGenerator*/,
                                                       void* /*lpTriangleCollisionScene*/)
    {
        // 0x828BF168: the opening null guard. No stream was created this frame (Start either did
        // not run or took its own spatial-map guard) -> there is nothing to wait on or read back.
        if (mpUpdateTriangleCacheStream == NULL)
        {
            return;
        }

        if (s_miDispatchCacheFillPerfMon > -1)
        {
            CgsDev::PerfMonCpu::StartMonitor(s_miDispatchCacheFillPerfMon);
        }

        // 0x828BF18C: RunFillTriangleCacheStream returns null when it dispatched nothing, and the
        // console tests for exactly that before waiting.
        if (mpUpdateTriangleCacheJob != NULL)
        {
            mpUpdateTriangleCacheJob->WaitOn();
        }

        // 0x828BF1B0..0x828BF1B8: close the command stream and drop the producer's streaming flag
        // (the console pokes the byte at producer+0x100 directly, which is mbIsStreaming; reached
        // here through the producer's own End).
        mpUpdateTriangleCacheStream->End();

        if (s_miDispatchCacheFillPerfMon > -1)
        {
            CgsDev::PerfMonCpu::StopMonitor(s_miDispatchCacheFillPerfMon);
        }
        if (s_miReadCacheResultsPerfMon > -1)
        {
            CgsDev::PerfMonCpu::StartMonitor(s_miReadCacheResultsPerfMon);
        }

        // 0x828BF1F0: a BY-VALUE copy of the producer's result cursor; the walk below advances the
        // copy, leaving the producer's own cursor where the stream left it.
        CgsMemory::SimpleDataStreamResultIterator lResultIterator =
            mpUpdateTriangleCacheStream->GetResultIterator();

        // Pass 1: fold one result into every slot that was posted, in the same slot order Start
        // posted them (both walks are the same first/next-set-bit scan over mUsedCacheSlots, and
        // both test the SAME mbIsDirty flag -- which is why the flag is only cleared here).
        for (s32 liSlotIndex = mUsedCacheSlots.GetFirstNonZeroBit();
             liSlotIndex != CgsContainers::BitArray<KU_MAX_CACHED_OBJECTS>::KI_INVALID_BITINDEX;
             liSlotIndex = mUsedCacheSlots.GetNextNonZeroBit(liSlotIndex))
        {
            CacheSlot& lrSlot = mpaCachedObjectSlots[liSlotIndex];

            if (!lrSlot.mbIsDirty)
            {
                continue;
            }

            const CgsCollision::FillTriangleCacheStreamJobDesc::StreamResult* lpResult =
                static_cast<const CgsCollision::FillTriangleCacheStreamJobDesc::StreamResult*>(
                    lResultIterator.GetCurrent());

            lrSlot.mbIsDirty                 = false;
            lrSlot.miNumCachedTriangleBatches = static_cast<s32>(lpResult->mu16NumCachedTriangleBatches);
            lrSlot.miOverflow                 = static_cast<s16>(lpResult->miOverflow);

            lResultIterator.GetNext();   // 0x828BF2BC..0x828BF2DC; the answer is dropped
        }

        // Pass 2 (0x828BF4B8..0x828BF88C): the DEV overflow overlay. One red on-screen line per
        // slot whose fill did not fit, stacked down the left of the screen. Nothing here changes
        // game state; it is reconstructed rather than dropped because a dropped diagnostic is how
        // an overflowing cache would go unnoticed.
        f32 lfTextY = 100.0f;   // flt_820049E0, also the X of every line
        for (s32 liSlotIndex = mUsedCacheSlots.GetFirstNonZeroBit();
             liSlotIndex != CgsContainers::BitArray<KU_MAX_CACHED_OBJECTS>::KI_INVALID_BITINDEX;
             liSlotIndex = mUsedCacheSlots.GetNextNonZeroBit(liSlotIndex))
        {
            const CacheSlot& lrSlot = mpaCachedObjectSlots[liSlotIndex];

            if (lrSlot.miOverflow <= 0)
            {
                continue;
            }

            // The console's stack-scoped acquire/release pair (its two asserts are
            // CgsDebugManager.h:343 "mpInstance" and :353 "lpDebugManager == mpInstance", which
            // this idiom already carries).
            CgsDev::DebugInterface lDebugInterface;

            // ⚠️ FLAG: the console's scratch buffer size is not determinable from the asm (nothing
            // bounds it); 256 is this tree's convention for a formatted debug line, and snprintf
            // is used where the console used the CRT sprintf @0x82C0CB70.
            char lacMessage[256];
            std::snprintf(lacMessage, sizeof(lacMessage),
                          "TRI CACHE OVERFLOW WITH SPHERE AT %.2f, %.2f, %.2f, RADIUS %.2f, OVERFLOW %d",
                          static_cast<double>(lrSlot.mLastCachedSphere.x),
                          static_cast<double>(lrSlot.mLastCachedSphere.y),
                          static_cast<double>(lrSlot.mLastCachedSphere.z),
                          static_cast<double>(lrSlot.mLastCachedSphere.w),
                          static_cast<int>(lrSlot.miOverflow));

            lDebugInterface.Get2dRender().Draw2DText(lacMessage, 100.0f, lfTextY, 20.0f,
                                                     0xFF0000FFu);
            lfTextY += 22.0f;   // flt_820F56B8

        }

        if (s_miReadCacheResultsPerfMon > -1)
        {
            CgsDev::PerfMonCpu::StopMonitor(s_miReadCacheResultsPerfMon);
        }
    }
}
