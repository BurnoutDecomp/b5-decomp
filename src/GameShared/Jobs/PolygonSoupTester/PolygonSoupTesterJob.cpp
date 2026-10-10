// ============================================================================
// GameShared/Jobs/PolygonSoupTester/PolygonSoupTesterJob.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The console's own path for this TU is
// baked into every assert here:
//   D:\P4\B5_MAIN\Burnout\MAIN\Code\GameShared\Jobs\PolygonSoupTester\PolygonSoupTesterJob.cpp
//
//   Execute                        @0x82915930  (107)  :112
//   ExecuteFillTriangleCacheStream @0x82915D88  (145)  :185 :186 :187
//   FillTriangleCache              @0x82915FD0  (219)  :240 :241
//   AllocateMemory                 @0x82916B98   (99)  :987
//   RunBoxQuery                    @0x82916D28   (46)
//   LoadPrimitive                  @0x82916AB8    (8)
//   ExecuteLineTest
//   LineTestNearestSS
//   LineTestSS / LineTestDS / LineTestNearestDS / RunLineQuery
// ============================================================================

#include "GameShared/Jobs/PolygonSoupTester/PolygonSoupTesterJob.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                    // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                            // gpDebugPrint (the two named gates)
#include "GameShared/GameClasses/Containers/CgsReadOnlyObjectCache.h"                 // ReadOnlyObjectCache<>
#include "GameShared/GameClasses/Geometric/Intersection/CgsLineTests.h"               // TestAxisAlignedBoxAxisAlignedBox
#include "GameShared/GameClasses/Geometric/Intersection/CgsPolygonSoupTests.h"        // ExtractTriangle4ListIntersectingSphere
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoup.h"   // PolygonSoup
#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle4.h"                 // Triangle4
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"            // AxisAlignedBox
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupListSpatialMap.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupSpacialNode.h"
#include "GameShared/GameClasses/Memory/DataStream/CgsSimpleDataStreamConsumer.h"
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsCollisionJobDescription.h"
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsFillTriangleCacheStreamJobDesc.h"
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsLineWithPolySoupListJobDesc.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsLine.h"                                    // Line (the line arm's command)

#include <cstring>   // std::memcpy (the line arm's 14-qword hit copy)

// The six per-job-thread contexts (X360 unk_83123940, stride 0x19040).
PolygonSoupTesterJob gaPolygonSoupTesterJobs[KI_NUM_POLYGON_SOUP_TESTER_JOBS];

using CgsSceneManager::CgsCollision::CollisionJobDescription;
using CgsSceneManager::CgsCollision::FillTriangleCacheStreamJobDesc;
using CgsSceneManager::CgsCollision::LineWithPolySoupStreamJobDesc;

namespace
{
    // The two job-side query buffers RunBoxQuery and RunLineQuery carve: 2048 ENTRIES each,
    // so 4096 BYTES (the console passes both numbers; 4096 == 2048 * sizeof(u16)).
    const s32 KI_QUERY_BUFFER_ENTRIES = 2048;
    const s32 KI_QUERY_BUFFER_BYTES   = KI_QUERY_BUFFER_ENTRIES * static_cast<s32>(sizeof(u16));

    // The nearest-hit seed the line arm stores into its result's line-parameter lane
    // (rw::math::vpu::GetVecFloat_Two, splatted): anything the kernel accepts (t <= 1) beats it.
    const f32 KF_LINE_PARAM_NO_HIT = 2.0f;
}

// The line arm carves its result as one 128-byte arena slot and the stream copies a whole
// aligned result per line, so the one-record block must be exactly that slot.
static_assert(sizeof(LineWithPolySoupStreamJobDesc::StreamResult) == 128,
              "StreamResult is the 16-byte header plus one 112-byte hit record");

// ----------------------------------------------------------------------------
// Execute @0x82915930 (107)
//
//   a1[25609..25615] = -1        -> maiFetchTags, all seven, in the console's own
//                                   scrambled store order (25613 before 25612 --
//                                   scheduling, not semantics)
//   a1[4]            = a2        -> mpJobDescription
//   a1[25608]        = 0         -> miAllocCursor
//   switch (CollisionJobDescription::GetType(a1[4])) { 2 / 3 / 4 }
//   default: assert "false" at :112
// ----------------------------------------------------------------------------
void PolygonSoupTesterJob::Execute(void* lpvJobData)
{
    for (s32 liTag = 0; liTag < 7; ++liTag)
    {
        maiFetchTags[liTag] = -1;
    }

    mpJobDescription = static_cast<CollisionJobDescription*>(lpvJobData);
    miAllocCursor    = 0;

    switch (mpJobDescription->GetType())
    {
    case CgsSceneManager::CgsCollision::E_COLLISIONJOB_FILL_TRIANGLE_CACHE:
        ExecuteFillTriangleCache();
        return;

    case CgsSceneManager::CgsCollision::E_COLLISIONJOB_FILL_TRIANGLE_CACHE_STREAM:
        ExecuteFillTriangleCacheStream();
        return;

    case CgsSceneManager::CgsCollision::E_COLLISIONJOB_LINE_WITH_POLYSOUP_STREAM:
        ExecuteLineTest();
        return;

    default:
        break;
    }

    CGS_ASSERT(false, "false");   // PolygonSoupTesterJob.cpp:112
}

// ----------------------------------------------------------------------------
// ExecuteFillTriangleCacheStream @0x82915D88 (145)
//
// Three entry tripwires (:185 "No job description\n", :186 "No spacial map\n",
// :187 "No stream producer\n" -- the misspelling is the console's), then:
//   LoadPrimitive(GetSpacialMap(), &lpMap)
//   SimpleDataStreamConsumer::Construct(&lConsumer, GetStreamProducer(), 0, 0)
//   lpCommand = AllocateMemory(128, 128)
//   lpResult  = AllocateMemory(128, 128)
//   while (!lConsumer.ReadCo(lpCommand, &luIndex)) {
//       lpResult->batches = FillTriangleCache(lpMap, lpCommand,
//                                             lpCommand->mpDestinationTriangles,
//                                             lpCommand->mu16MaxNumTriangleBatches,
//                                             &lpResult->overflow);
//       lConsumer.AddResult(lpResult, luIndex);
//   }
//   lConsumer.Destruct()
//
// ⭐ The result's overflow slot is `v14 + 2` in `_WORD*` units == +4 bytes, i.e.
// StreamResult::miOverflow -- and the batch count goes to +0, StreamResult::
// mu16NumCachedTriangleBatches. Both match the record the committed
// EndUpdateTriangleCaches already reads back.
// ----------------------------------------------------------------------------
void PolygonSoupTesterJob::ExecuteFillTriangleCacheStream()
{
    FillTriangleCacheStreamJobDesc* lpDesc =
        static_cast<FillTriangleCacheStreamJobDesc*>(mpJobDescription);

    CGS_ASSERT(lpDesc != NULL, "No job description\n");                     // :185
    CGS_ASSERT(lpDesc->GetSpatialMapPointer() != NULL, "No spacial map\n");
    CGS_ASSERT(lpDesc->GetDataStreamProducer() != NULL, "No stream producer\n");

    // LoadPrimitive: on SPU this DMAs the map in; on a shared-memory host it is the
    // pointer. Kept as the call the console makes rather than folded away.
    const void* lpvSpatialMap = NULL;
    LoadPrimitive(lpDesc->GetSpatialMapPointer(), &lpvSpatialMap);
    const CgsGeometric::PolygonSoupListSpatialMap* lpSpatialMap =
        static_cast<const CgsGeometric::PolygonSoupListSpatialMap*>(lpvSpatialMap);

    CgsMemory::SimpleDataStreamConsumer lConsumer;
    lConsumer.Construct(lpDesc->GetDataStreamProducer(), NULL, 0);

    // Both scratch records are a whole 128-byte aligned arena slice, as the console
    // asks (`AllocateMemory(a1, 128, 128)` twice) -- not sizeof(record).
    FillTriangleCacheStreamJobDesc::StreamCommand* lpCommand =
        static_cast<FillTriangleCacheStreamJobDesc::StreamCommand*>(AllocateMemory(128, 128));
    FillTriangleCacheStreamJobDesc::StreamResult* lpResult =
        static_cast<FillTriangleCacheStreamJobDesc::StreamResult*>(AllocateMemory(128, 128));

    u32 luResultIndex = 0;
    while (lConsumer.ReadCo(lpCommand, &luResultIndex) == 0)
    {
        s32 liOverflow = 0;

        lpResult->mu16NumCachedTriangleBatches =
            FillTriangleCache(lpSpatialMap,
                              &lpCommand->mCacheSphere,
                              lpCommand->mpDestinationTriangles,
                              lpCommand->mu16MaxNumTriangleBatches,
                              &liOverflow);

        lpResult->miOverflow = liOverflow;

        lConsumer.AddResult(lpResult, static_cast<s32>(luResultIndex));
    }

    lConsumer.Destruct();
}

// ----------------------------------------------------------------------------
// FillTriangleCache @0x82915FD0 (219)
//
//   :240 "No spacial map\n" / :241 "No triangle list destination\n"
//   save miAllocCursor
//   box = { sphere.pos - splat(sphere.radius), sphere.pos + splat(sphere.radius) }
//         (GetRadius/GetPosition then rw::math::vpu::operator+ and operator-, then
//          AxisAlignedBox::Set(v1 = min, v2 = max) -- the register order at
//          0x82915FD0's `lvx128 v2,[v50]` / `lvx128 v1,[v53]` pins which is which)
//   leafCacheStorage = AllocateMemory(8, 128)
//   RunBoxQuery(map, &box, &lpaLeafIndices, &liNumLeafIndices)
//   leafCache.Construct(map->GetPolygonSoup(0), map->GetNumLeafNodes(), 0, 1)
//   for each returned leaf index:
//       leaf = leafCache.Get(index)
//       if (TestAxisAlignedBoxAxisAlignedBox(leaf->mBox, box))
//           written += ExtractTriangle4ListIntersectingSphere(
//                          leaf->mpPolygonSoup, sphere,
//                          &dest[written], maxBatches - written, &overflow)
//       leafCache.Release(leaf)
//   restore miAllocCursor; *lpiOutOverflow = overflow; return written
// ----------------------------------------------------------------------------
u16 PolygonSoupTesterJob::FillTriangleCache(
    const CgsGeometric::PolygonSoupListSpatialMap* lpSpatialMap,
    const CgsGeometric::Sphere*                    lpCacheSphere,
    CgsGeometric::Triangle4*                       lpaDestinationTriangles,
    u16                                            lu16MaxNumTriangleBatches,
    s32*                                           lpiOutOverflow)
{
    CGS_ASSERT(lpSpatialMap != NULL, "No spacial map\n");                       // :240
    CGS_ASSERT(lpaDestinationTriangles != NULL, "No triangle list destination\n"); // :241

    const s32 liSavedCursor = miAllocCursor;

    // The query sphere, copied out of the command record by value (the console does it
    // as two 8-byte moves into a stack Sphere).
    const CgsGeometric::Sphere lQuerySphere = *lpCacheSphere;

    // sphere -> AABB. GetRadius broadcasts the w lane across all four; the two vpu
    // operators are whole-vector, so the w lane of the box is pos.w +/- radius and is
    // never read (CgsAxisAlignedBox.h: "xyz used, w lane spare").
    const Vector4  lvPosition = lQuerySphere.GetPosition();
    const VecFloat lvRadius   = lQuerySphere.GetRadius();

    Vector4 lvBoxMax;
    lvBoxMax.x = lvPosition.x + lvRadius.x;
    lvBoxMax.y = lvPosition.y + lvRadius.y;
    lvBoxMax.z = lvPosition.z + lvRadius.z;
    lvBoxMax.w = lvPosition.w + lvRadius.w;

    Vector4 lvBoxMin;
    lvBoxMin.x = lvPosition.x - lvRadius.x;
    lvBoxMin.y = lvPosition.y - lvRadius.y;
    lvBoxMin.z = lvPosition.z - lvRadius.z;
    lvBoxMin.w = lvPosition.w - lvRadius.w;

    CgsGeometric::AxisAlignedBox lQueryBox;
    lQueryBox.Set(lvBoxMin, lvBoxMax);

    // The leaf cache lives in the arena (8 bytes at 128 alignment), scoped to this call
    // by the cursor save/restore above and below.
    CgsContainers::ReadOnlyObjectCache<CgsGeometric::PolygonSoupLeafNode>* lpLeafCache =
        static_cast<CgsContainers::ReadOnlyObjectCache<CgsGeometric::PolygonSoupLeafNode>*>(
            AllocateMemory(static_cast<s32>(sizeof(CgsContainers::ReadOnlyObjectCache<
                                                       CgsGeometric::PolygonSoupLeafNode>)),
                           128));

    s32 liTotalOverflow = 0;

    u16* lpaLeafIndices    = NULL;
    s32  liNumLeafIndices  = 0;
    RunBoxQuery(lpSpatialMap, &lQueryBox, &lpaLeafIndices, &liNumLeafIndices);

    lpLeafCache->Construct(lpSpatialMap->GetPolygonSoup(0),
                           lpSpatialMap->GetNumLeafNodes(), 0, 1);

    u16       lu16BatchesWritten = 0;
    const u16 lu16MaxBatches     = lu16MaxNumTriangleBatches;

    for (s32 liResult = 0; liResult < liNumLeafIndices; ++liResult)
    {
        const CgsGeometric::PolygonSoupLeafNode* lpLeaf =
            lpLeafCache->Get(static_cast<s32>(lpaLeafIndices[liResult]));

        if (CgsGeometric::TestAxisAlignedBoxAxisAlignedBox(lpLeaf->mBox, lQueryBox))
        {
            // ⭐⭐⭐ 2026-08-11: THE GATE IS GONE AND THE EXTRACTOR IS REAL.
            //   0x829162A0  subf r6, r10, r11       ; liBufferSize = max - written
            //   0x829162A8  mulli r11, r11, 0xE0    ; &dest[written]
            //   0x829162BC  bl ExtractTriangle4ListIntersectingSphere
            //   0x829162C4  add r11, r11, r3        ; written += return
            //   0x829162D4  add r11, r11, var_3C    ; overrun += out-param
            s32 liLeafOverrun = 0;
            lu16BatchesWritten = static_cast<u16>(
                lu16BatchesWritten
                + CgsGeometric::ExtractTriangle4ListIntersectingSphere(
                      *lpLeaf->mpPolygonSoup,
                      lQuerySphere,
                      lpaDestinationTriangles + lu16BatchesWritten,
                      static_cast<s32>(lu16MaxBatches) - static_cast<s32>(lu16BatchesWritten),
                      &liLeafOverrun));
            liTotalOverflow += liLeafOverrun;
        }

        lpLeafCache->Release(lpLeaf);
    }

    miAllocCursor = liSavedCursor;

    if (lpiOutOverflow)
    {
        *lpiOutOverflow = liTotalOverflow;
    }

    return lu16BatchesWritten;
}

// ----------------------------------------------------------------------------
// AllocateMemory @0x82916B98 (99)
//
//   if (align(cursor,al) + align(size,al) >= 102400) assert :987
//   cursor  = align(cursor, al)
//   result  = this + 32 + cursor            <- the arena, reached BY NAME here
//   cursor += align(size, al)
//
// ⚠️ The console's overflow test uses the PRE-ADVANCE cursor and does NOT return null
// on failure -- it asserts and then allocates anyway. Reproduced as shipped.
// ----------------------------------------------------------------------------
void* PolygonSoupTesterJob::AllocateMemory(s32 liNumBytes, s32 liAlignment)
{
    const s32 liAlignedCursor = (miAllocCursor + liAlignment - 1) & ~(liAlignment - 1);
    const s32 liAlignedSize   = (liNumBytes    + liAlignment - 1) & ~(liAlignment - 1);

    CGS_ASSERT((liAlignedCursor + liAlignedSize) < KI_ARENA_BYTES,
               "Trying to use too much memory");   // :987

    miAllocCursor = liAlignedCursor;

    void* lpvResult = &maArena[miAllocCursor];

    miAllocCursor += liAlignedSize;

    return lpvResult;
}

// ----------------------------------------------------------------------------
// RunBoxQuery @0x82916D28 (46)
//
//   params = { AllocateMemory(4096,128), AllocateMemory(4096,128), 2048 }
//   saved  = miAllocCursor
//   cache  = AllocateMemory(8,128)
//   result = map->RunJobQuery(box, &params, out, count, cache)
//   miAllocCursor = saved
//
// ⚠️ NOTE THE ASYMMETRY, IT IS THE CONSOLE'S: the cursor is captured AFTER the two
// 4 KB buffers are carved and restored at the end, so the NODE CACHE is scoped to the
// call but the two query buffers are NOT -- they stay live in the caller's arena
// window, which is exactly what lets RunJobQuery hand one of them back as the result
// array. Reproduced verbatim; getting this backwards would return a dangling buffer.
//
// ⚠️ 4096 is a byte size and 2048 is an ENTRY count -- 4096 == 2048 * sizeof(u16).
// The relationship is written out rather than reproduced as two magic numbers.
// ----------------------------------------------------------------------------
s32 PolygonSoupTesterJob::RunBoxQuery(
    const CgsGeometric::PolygonSoupListSpatialMap* lpSpatialMap,
    const CgsGeometric::AxisAlignedBox*            lpQueryBox,
    u16**                                          lppaOutResults,
    s32*                                           lpiOutNumResults)
{
    CgsGeometric::PolygonSoupListSpatialMap::PolygonSoupJobQueryParams lParams;
    lParams.mpaQueryBufferA   = static_cast<u16*>(AllocateMemory(KI_QUERY_BUFFER_BYTES, 128));
    lParams.mpaQueryBufferB   = static_cast<u16*>(AllocateMemory(KI_QUERY_BUFFER_BYTES, 128));
    lParams.miQueryBufferSize = KI_QUERY_BUFFER_ENTRIES;

    const s32 liSavedCursor = miAllocCursor;

    CgsContainers::ReadOnlyObjectCache<CgsGeometric::PolygonSoupSpacialNode>* lpNodeCache =
        static_cast<CgsContainers::ReadOnlyObjectCache<CgsGeometric::PolygonSoupSpacialNode>*>(
            AllocateMemory(static_cast<s32>(sizeof(CgsContainers::ReadOnlyObjectCache<
                                                       CgsGeometric::PolygonSoupSpacialNode>)),
                           128));

    const s32 liResult = lpSpatialMap->RunJobQuery(*lpQueryBox, &lParams,
                                                   lppaOutResults, lpiOutNumResults,
                                                   lpNodeCache);

    miAllocCursor = liSavedCursor;

    return liResult;
}

// LoadPrimitive @0x82916AB8 (8): `*a3 = a2`.
void PolygonSoupTesterJob::LoadPrimitive(const void* lpvSource, const void** lppvOut)
{
    *lppvOut = lpvSource;
}

// ----------------------------------------------------------------------------
// The synchronous fill arm -- a NAMED BOOT GATE, not a silent return.
// ----------------------------------------------------------------------------
void PolygonSoupTesterJob::ExecuteFillTriangleCache()
{
    do { static bool s_bLogged = false;
    if (!s_bLogged) { s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint
                << "conductor gate: PolygonSoupTesterJob::ExecuteFillTriangleCache @0x82915AE0 "
                   "(170) not reconstructed -- the SYNCHRONOUS twin of the stream arm; nothing "
                   "posts a type-2 descriptor today [FLAG PC boot gate]\n"; } } while (0);
}

// ----------------------------------------------------------------------------
// ExecuteLineTest -- the streamed line arm (descriptor type 4).
//
//    "No job description\n" "No spacial map\n"
//   LoadPrimitive(desc->GetSpatialMapPointer(), &map)
//   consumer.Construct(desc->GetStream(), 0, 0)
//   command = AllocateMemory(128, 128)          (one whole aligned slot, not sizeof)
//   nearest ? (doubleSided ? NearestDS : NearestSS) : (doubleSided ? DS : SS), each as
//       while (!consumer.ReadCo(command, &index)) worker(&consumer, index, map, command)
//   consumer.Destruct()
// ----------------------------------------------------------------------------
void PolygonSoupTesterJob::ExecuteLineTest()
{
    const LineWithPolySoupStreamJobDesc* lpDesc =
        static_cast<const LineWithPolySoupStreamJobDesc*>(mpJobDescription);

    CGS_ASSERT(lpDesc != NULL, "No job description\n");
    CGS_ASSERT(lpDesc->GetSpatialMapPointer() != NULL, "No spacial map\n");

    const void* lpvSpatialMap = NULL;
    LoadPrimitive(lpDesc->GetSpatialMapPointer(), &lpvSpatialMap);
    const CgsGeometric::PolygonSoupListSpatialMap* lpSpatialMap =
        static_cast<const CgsGeometric::PolygonSoupListSpatialMap*>(lpvSpatialMap);

    CgsMemory::SimpleDataStreamConsumer lConsumer;
    lConsumer.Construct(lpDesc->GetStream(), NULL, 0);

    LineStreamCommand* lpCommand = static_cast<LineStreamCommand*>(AllocateMemory(128, 128));

    u32 luResultIndex = 0;
    if (lpDesc->GetTestNearest())
    {
        if (lpDesc->GetTestDoubleSided())
        {
            while (lConsumer.ReadCo(lpCommand, &luResultIndex) == 0)
            {
                LineTestNearestDS(&lConsumer, luResultIndex, lpSpatialMap, lpCommand);
            }
        }
        else
        {
            while (lConsumer.ReadCo(lpCommand, &luResultIndex) == 0)
            {
                LineTestNearestSS(&lConsumer, luResultIndex, lpSpatialMap, lpCommand);
            }
        }
    }
    else if (lpDesc->GetTestDoubleSided())
    {
        while (lConsumer.ReadCo(lpCommand, &luResultIndex) == 0)
        {
            LineTestDS(&lConsumer, luResultIndex, lpSpatialMap, lpCommand);
        }
    }
    else
    {
        while (lConsumer.ReadCo(lpCommand, &luResultIndex) == 0)
        {
            LineTestSS(&lConsumer, luResultIndex, lpSpatialMap, lpCommand);
        }
    }

    lConsumer.Destruct();
}

// ----------------------------------------------------------------------------
// LineTestSS / LineTestDS / LineTestNearestDS -- on the console each is the argument
// spill and a `blr`: these three arms compile to nothing in this build, so a stream run
// with either flag combination posts no result for its commands. Reproduced as shipped.
// The one live arm is LineTestNearestSS below (the only one the scene manager asks for:
// nearest, single-sided).
// ----------------------------------------------------------------------------
void PolygonSoupTesterJob::LineTestSS(CgsMemory::SimpleDataStreamConsumer* lpConsumer, u32 luResultIndex,
                                      const CgsGeometric::PolygonSoupListSpatialMap* lpSpatialMap,
                                      const LineStreamCommand* lpCommand)
{
    (void)lpConsumer;
    (void)luResultIndex;
    (void)lpSpatialMap;
    (void)lpCommand;
}

void PolygonSoupTesterJob::LineTestDS(CgsMemory::SimpleDataStreamConsumer* lpConsumer, u32 luResultIndex,
                                      const CgsGeometric::PolygonSoupListSpatialMap* lpSpatialMap,
                                      const LineStreamCommand* lpCommand)
{
    (void)lpConsumer;
    (void)luResultIndex;
    (void)lpSpatialMap;
    (void)lpCommand;
}

void PolygonSoupTesterJob::LineTestNearestDS(CgsMemory::SimpleDataStreamConsumer* lpConsumer, u32 luResultIndex,
                                             const CgsGeometric::PolygonSoupListSpatialMap* lpSpatialMap,
                                             const LineStreamCommand* lpCommand)
{
    (void)lpConsumer;
    (void)luResultIndex;
    (void)lpSpatialMap;
    (void)lpCommand;
}

// ----------------------------------------------------------------------------
// LineTestNearestSS -- one line against the world, nearest single-sided hit.
//
//    "No spacial map\n"
//   save miAllocCursor
//   leafCache = AllocateMemory(8, 128)
//   start = line +0x00, end = line +0x10
//   RunLineQuery(map, line, &leafIndices, &numLeafIndices)
//   result  = AllocateMemory(128, 128)  -> StreamResult (16-byte header + one record)
//   scratch = AllocateMemory(112, 128)  -> one PolySoupLineNearestResult
//   result.muNumResults = 0; result.maResults[0] line param = {2,2,2,2}
//   if (numLeafIndices > 0 && map->GetPolygonSoup(0)) {
//       leafCache.Construct(map->GetPolygonSoup(0), map->GetNumLeafNodes(), 0, 1)
//       for each leaf index: leaf = leafCache.Get(index)
//           if (TestLineStartEndAxisAlignedBox(leaf box, start, end)
//               && IntersectLinePolygonSoupNearestSingleSided(leaf soup, scratch, start, end)
//               && scratch.param < best.param)          (`vcmpgtfp. best, scratch`, all lanes)
//               { result.muNumResults = 1; best = scratch (14 qwords) }
//           leafCache.Release(leaf)
//   }
//   consumer.AddResult(result, index); restore miAllocCursor
//
// The hit count is set by the copy, not by a final `1.0 >= t` test (the kernel only
// reports a hit with t <= 1, so a copied record is always a real one).
// ----------------------------------------------------------------------------
void PolygonSoupTesterJob::LineTestNearestSS(CgsMemory::SimpleDataStreamConsumer* lpConsumer, u32 luResultIndex,
                                             const CgsGeometric::PolygonSoupListSpatialMap* lpSpatialMap,
                                             const LineStreamCommand* lpCommand)
{
    CGS_ASSERT(lpSpatialMap != NULL, "No spacial map\n");

    const s32 liSavedCursor = miAllocCursor;

    CgsContainers::ReadOnlyObjectCache<CgsGeometric::PolygonSoupLeafNode>* lpLeafCache =
        static_cast<CgsContainers::ReadOnlyObjectCache<CgsGeometric::PolygonSoupLeafNode>*>(
            AllocateMemory(static_cast<s32>(sizeof(CgsContainers::ReadOnlyObjectCache<
                                                       CgsGeometric::PolygonSoupLeafNode>)),
                           128));

    // The line's two 16-byte lanes, w included (the kernel's position lane multiplies all four).
    const CgsGeometric::Line& lrLine = lpCommand->mLine;
    const Vector4 lLineStart  = { lrLine.mStart.x, lrLine.mStart.y, lrLine.mStart.z, lrLine.mStart.w };
    const Vector4 lLineEnd    = { lrLine.mEnd.x,   lrLine.mEnd.y,   lrLine.mEnd.z,   lrLine.mEnd.w };
    const Vector3 lLineStart3 = { lrLine.mStart.x, lrLine.mStart.y, lrLine.mStart.z, lrLine.mStart.w };
    const Vector3 lLineEnd3   = { lrLine.mEnd.x,   lrLine.mEnd.y,   lrLine.mEnd.z,   lrLine.mEnd.w };

    u16* lpaLeafIndices   = NULL;
    s32  liNumLeafIndices = 0;
    RunLineQuery(lpSpatialMap, lrLine, &lpaLeafIndices, &liNumLeafIndices);

    LineWithPolySoupStreamJobDesc::StreamResult* lpResult =
        static_cast<LineWithPolySoupStreamJobDesc::StreamResult*>(
            AllocateMemory(static_cast<s32>(sizeof(LineWithPolySoupStreamJobDesc::StreamResult)), 128));
    CgsGeometric::PolySoupLineNearestResult* lpScratch =
        static_cast<CgsGeometric::PolySoupLineNearestResult*>(
            AllocateMemory(static_cast<s32>(sizeof(CgsGeometric::PolySoupLineNearestResult)), 128));

    lpResult->muNumResults = 0;

    CgsGeometric::PolySoupLineNearestResult* lpBest = &lpResult->maResults[0];
    lpBest->mLineParam.x = KF_LINE_PARAM_NO_HIT;
    lpBest->mLineParam.y = KF_LINE_PARAM_NO_HIT;
    lpBest->mLineParam.z = KF_LINE_PARAM_NO_HIT;
    lpBest->mLineParam.w = KF_LINE_PARAM_NO_HIT;

    if (liNumLeafIndices > 0 && lpSpatialMap->GetPolygonSoup(0) != NULL)
    {
        lpLeafCache->Construct(lpSpatialMap->GetPolygonSoup(0), lpSpatialMap->GetNumLeafNodes(), 0, 1);

        for (s32 liResult = 0; liResult < liNumLeafIndices; ++liResult)
        {
            const CgsGeometric::PolygonSoupLeafNode* lpLeaf =
                lpLeafCache->Get(static_cast<s32>(lpaLeafIndices[liResult]));

            if (CgsGeometric::TestLineStartEndAxisAlignedBox(lLineStart, lLineEnd, lpLeaf->mBox)
                && CgsGeometric::IntersectLinePolygonSoupNearestSingleSided(*lpLeaf->mpPolygonSoup, lpScratch,
                                                                            lLineStart3, lLineEnd3)
                && lpBest->mLineParam.x > lpScratch->mLineParam.x)
            {
                lpResult->muNumResults = 1;
                std::memcpy(lpBest, lpScratch, sizeof(CgsGeometric::PolySoupLineNearestResult));   // 14 qwords
            }

            lpLeafCache->Release(lpLeaf);
        }
    }

    lpConsumer->AddResult(lpResult, static_cast<s32>(luResultIndex));

    miAllocCursor = liSavedCursor;
}

// ----------------------------------------------------------------------------
// RunLineQuery -- RunBoxQuery's segment twin, the same carve:
//   params = { AllocateMemory(4096,128), AllocateMemory(4096,128), 2048 }
//   saved  = miAllocCursor
//   cache  = AllocateMemory(8,128)
//   map->RunJobQuery(line, &params, out, count, cache)
//   miAllocCursor = saved
// The two query buffers stay live in the caller's arena window (one of them is the
// result array); only the node cache is scoped to the call.
// ----------------------------------------------------------------------------
void PolygonSoupTesterJob::RunLineQuery(const CgsGeometric::PolygonSoupListSpatialMap* lpSpatialMap,
                                        const CgsGeometric::Line&                      lrLine,
                                        u16**                                          lppaOutResults,
                                        s32*                                           lpiOutNumResults)
{
    CgsGeometric::PolygonSoupListSpatialMap::PolygonSoupJobQueryParams lParams;
    lParams.miQueryBufferSize = KI_QUERY_BUFFER_ENTRIES;
    lParams.mpaQueryBufferA   = static_cast<u16*>(AllocateMemory(KI_QUERY_BUFFER_BYTES, 128));
    lParams.mpaQueryBufferB   = static_cast<u16*>(AllocateMemory(KI_QUERY_BUFFER_BYTES, 128));

    const s32 liSavedCursor = miAllocCursor;

    CgsContainers::ReadOnlyObjectCache<CgsGeometric::PolygonSoupSpacialNode>* lpNodeCache =
        static_cast<CgsContainers::ReadOnlyObjectCache<CgsGeometric::PolygonSoupSpacialNode>*>(
            AllocateMemory(static_cast<s32>(sizeof(CgsContainers::ReadOnlyObjectCache<
                                                       CgsGeometric::PolygonSoupSpacialNode>)),
                           128));

    lpSpatialMap->RunJobQuery(lrLine, &lParams, lppaOutResults, lpiOutNumResults, lpNodeCache);

    miAllocCursor = liSavedCursor;
}
