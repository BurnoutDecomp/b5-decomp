#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                        // CGS_ASSERT
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

// =================================================================================================
// GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManager_Update.cpp
//
// ⭐ THE TRIANGLE CACHE'S FILL HALF, reconstructed from BURNOUT_X360_ARTIST.XEX:
//   CgsSceneManager::TriangleCacheManager::StartUpdateTriangleCaches @0x828BECF8 (278)
//   CgsSceneManager::TriangleCacheManager::EndUpdateTriangleCaches   @0x828BF150 (475)
// Both replace WorldLinkStubs.cpp gates, which are deleted with this TU.
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
// ⛔ REACHABILITY, stated plainly -- the two halves are NOT in the same state.
//
//  * EndUpdateTriangleCaches IS ON THE LIVE PATH. SceneManagerModule::EndUpdateTriangleCache
//    @0x828C7500 is a real body and WorldModule::Update calls it every frame
//    (BrnWorldModule.cpp:2471). This body therefore RUNS, and takes its own opening null guard on
//    mpUpdateTriangleCacheStream -- exactly as the console does whenever a frame posted no fill.
//
//  * StartUpdateTriangleCaches IS NOW REACHED TOO -- ⭐ UPDATED 2026-08-10 (spatial-partition
//    wave); the paragraph that used to sit here is superseded and two of its claims were wrong.
//    Its sole caller SceneManagerModule::StartUpdateTriangleCache @0x828C73D8 is no longer a
//    WorldLinkStubs gate: it has its real body in CgsSceneManagerModule.cpp, and the chain it
//    needed first -- TriangleCollisionManager::Prepare, ProcessAddPolySoupListEvents
//    @0x828B3160, and PolygonSoupListSpatialMap::BuildSpacialPartition @0x82841740 (2,255) --
//    is all present and mounted. RUNTIME-WITNESSED entering this function every frame.
//    ⚠️ TWO CORRECTIONS to what this banner used to assert:
//      - "TriangleCollisionManager::Prepare @0x828D0C40" -- NO EXPORT LIVES AT 0x828D0C40
//        (checked against all 30,084 X360 export JSONs). The real address is 0x828B2FF0 (91).
//      - "Prepare is still a WorldLinkStubs gate ... BuildSpacialPartition absent from this
//        tree" -- Prepare and ProcessAddPolySoupListEvents were in fact FULLY RECONSTRUCTED the
//        whole time and merely UNMOUNTED; only BuildSpacialPartition was genuinely missing.
//
//  * ⛔ WHAT STILL STARVES IT, measured not guessed: the partition is only built when an
//    InEventAddPolySoupList arrives, whose one producer chain
//    (WorldEntityModule::AddCollisionZoneToSceneManager @0x822D8130 <- PrepareWorldCollision
//    @0x823068F8, both already bodied) is never entered, because the scripted boot spine defers
//    the stage that starts it -- BrnGameMainFlowStates.cpp:532 logs
//    `ScriptedLoad: stage 7 (LoadWorldCollision [deferred])` and jumps to stage 8. Remaining:
//    LoadingScriptedState::LoadWorldCollision @0x823E73E0 (55) + WorldModule::
//    PrepareWorldCollision @0x827C9478 (152, an IO-buffer + streaming round trip).
//    ⭐ Until then this function's OWN first act after its two asserts takes the same exit the
//    console would: `if (lpPolySoupListSpacialMap->mpLeafNodes == NULL) return;` (0x828BED68
//    `lwz r11, 0x48(r29)` / `beq` straight to the epilogue) -- a fact about the world DATA not
//    being loaded, not about this reconstruction.
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


        // 0x828BED68: no spatial partition built -> nothing to query against, so no stream is
        // created and no command is posted. The matching End then takes ITS null guard.
        if (lpPolySoupListSpacialMap->GetLeafNodes() == NULL)
        {
            return;
        }

        if (s_miBuildCacheCommandsPerfMon > -1)
        {
            CgsDev::PerfMonCpu::StartMonitor(s_miBuildCacheCommandsPerfMon);
        }

        // 0x828BED8C: one producer per frame, sized for the whole slot pool.
        mpUpdateTriangleCacheStream =
            lpCollisionGenerator->CreateStreamProducer(static_cast<s32>(KU_MAX_CACHED_OBJECTS));

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

            CgsDev::DebugManager::ThreadSafeRelease(&lDebugInterface.GetDebugManager());
        }

        if (s_miReadCacheResultsPerfMon > -1)
        {
            CgsDev::PerfMonCpu::StopMonitor(s_miReadCacheResultsPerfMon);
        }
    }
}
