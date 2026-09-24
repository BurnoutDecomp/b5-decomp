// FX-SCENEMGR (crash parity 2026-09-24, item 3b): the scene manager's fine line test.
//   SceneManagerModule::ProcessLineTestFine            @0x828CDCD0 (was an assert-false stub)
//   SceneManagerModule::ProcessTriangleCollisionLineTests @0x828C6FB0 (was a trap on a non-empty queue)
//   OutSceneQueryResultsQueue::AddLineTestFineResult  @0x828C4A08 (was AllocateLineTestFineResult)
//   OutSceneQueryResultsQueue::AddTriangleCollisionLineTestResult @0x828C4A60 (absent)
//   SceneManagerIO::OutEventLineTestFineResult (DWARF CgsSceneManagerModuleIO.h:217; records at +0x10)
// all extracted VERBATIM by run_fxscenemgr_line_test_fine.py and driven through scripted fakes of the
// octree, the entity manager, the fine module and the collision generator. The expected values are the
// ARTIST asm's (see each check).
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneQueryId.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerTypes.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventLineTest.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventTriangleCollisionLineTest.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsLine.h"
#include "GameShared/GameClasses/Geometric/Intersection/CgsPolygonSoupTests.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static std::vector<std::string> gaAsserts;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) { gaAsserts.push_back(lpcMessage ? lpcMessage : ""); return 0; }
void* EndAssert() { return nullptr; }
}
}
static bool AssertedWith(const char* lpcNeedle)
{
    for (const std::string& s : gaAsserts) if (s.find(lpcNeedle) != std::string::npos) return true;
    return false;
}

namespace CgsModule {
template <typename T, s32 N>
struct EventQueue
{
    std::vector<T> maEvents;
    s32 GetLength() const { return static_cast<s32>(maEvents.size()); }
    const T& GetEvent(s32 liIndex) const { return maEvents[static_cast<size_t>(liIndex)]; }
    bool AddEvent(const T& lrEvent) { maEvents.push_back(lrEvent); return true; }
};
}

namespace CgsGeometric { struct PolygonSoupListSpatialMap { int miTag; }; }

namespace CgsSceneManager {
namespace SceneManagerIO {
#include "fxsm_ltf_result_ns.inc"   // the SceneManagerIO block of CgsSceneManagerIO_LineTestFineResult.hpp

template <s32 SizeBytes>
class OutSceneQueryResultsQueue
{
public:
    struct Allocation { s32 miType; s32 miSize; u8* mpData; };
    alignas(16) u8 maBuffer[SizeBytes];
    s32 miUsed = 0;
    std::vector<Allocation> maAllocations;
    void* AllocateEvent(s32 liType, s32 liSize)
    {
        u8* lpData = maBuffer + miUsed;
        miUsed += (liSize + 15) & ~15;
        std::memset(lpData, 0xEE, static_cast<size_t>(liSize));   // stale bytes, as a reused queue would hold
        maAllocations.push_back(Allocation{ liType, liSize, lpData });
        return lpData;
    }
    LineTestIntersection* AddLineTestFineResult(SceneQueryId lQueryId, s32 liNumIntersections);
    LineTestIntersection* AddTriangleCollisionLineTestResult(SceneQueryId lQueryId, EntityId lEntityId,
                                                             VolumeInstanceId lVolumeInstanceId,
                                                             const CgsGeometric::PolySoupLineNearestResult* lpaResults,
                                                             s32 liNumResults);
};
#include "fxsm_ltf_queue_bodies.inc"   // the two template definitions (the header under test, or stand-ins)

struct OutputBuffer
{
    OutSceneQueryResultsQueue<32768> mResults;
    OutSceneQueryResultsQueue<32768>* GetResultsQueue() { return &mResults; }
};

struct TriCacheQueryBuffer
{
    CgsModule::EventQueue<InEventTriangleCollisionLineTest, 256> mLineQueue;
    CgsModule::EventQueue<InEventTriangleCollisionLineTest, 256>* GetTriangleCollisionLineTestQueue() { return &mLineQueue; }
};
}   // SceneManagerIO

namespace SpatialPartitionIO {
struct CoarseBuffer
{
    int miBegins = 0, miEnds = 0;
    std::vector<u16> maResults;
    s32 miAttempted = 0;
    void BeginResultsBatch() { ++miBegins; }
    void EndResultsBatch() { ++miEnds; }
    s32 GetNumResultsWritten() const { return static_cast<s32>(maResults.size()); }
    s32 GetNumResultsAttempted() const { return miAttempted; }
    const u16* GetResultsBatch() const { return maResults.data(); }
};
struct OutputBuffer
{
    CoarseBuffer mCoarse;
    CoarseBuffer* GetCoarseResultBuffer() { return &mCoarse; }
};
}

struct FakePartition
{
    int miCalls = 0;
    u32 muFlags = 0;
    Vector3 mStart = {}, mEnd = {};
    std::vector<u16> maScript;
    s32 miScriptAttempted = -1;   // -1: == written
    void LineTest(u32 luFlags, const Vector3& lrStart, const Vector3& lrEnd, SpatialPartitionIO::CoarseBuffer* lpBuffer)
    {
        ++miCalls; muFlags = luFlags; mStart = lrStart; mEnd = lrEnd;
        lpBuffer->maResults = maScript;
        lpBuffer->miAttempted = miScriptAttempted < 0 ? static_cast<s32>(maScript.size()) : miScriptAttempted;
    }
};
struct FakeSpatialPartitionManager { FakePartition mPartition; FakePartition* GetSpatialPartition() { return &mPartition; } };

struct FakeEntityManager
{
    u32 muKnownId = 0; s32 miKnownIndex = -1;
    u16 mu16InvalidIndex = 0xFFFF;
    s32 GetEntityIndexByID(EntityId lId) const { return static_cast<u32>(lId) == muKnownId ? miKnownIndex : -1; }
    EntityId GetEntityIdByIndex(u16 lu16Index) const
    {
        return lu16Index == mu16InvalidIndex ? EntityId(0xFFFFFFFFu) : EntityId(0x02000000u | (static_cast<u32>(lu16Index) << 10));
    }
};

namespace FineIntersectionTestIO {
struct InEventLineTestFine
{
    Vector3 mLineStart, mLineEnd; SceneQueryId mQueryId; const u16* mpau16EntityIndices;
    u16 mu16NumEntities; u16 mu16ExcludeEntityIndex; u8 mxVolumeTypeFlags; bool mbExcludeParts;
};
struct OutEventLineTestFineResult { SceneQueryId mQueryId; s32 miNumResults; const LineTestIntersection* mpaResults; };
struct OutputBuffer { int miGets = 0; int maArray[4]; void* GetLineTestIntersectionArray() { ++miGets; return maArray; } };
}

struct FakeFineModule
{
    int miCalls = 0;
    FineIntersectionTestIO::InEventLineTestFine mLast = {};
    void* mpLastArray = nullptr;
    std::vector<LineTestIntersection> maScript;
    void ComputeLineTestFine(const FineIntersectionTestIO::InEventLineTestFine* lpQuery,
                             FineIntersectionTestIO::OutEventLineTestFineResult* lpOut, void* lpArray)
    {
        ++miCalls; mLast = *lpQuery; mpLastArray = lpArray;
        lpOut->mQueryId = lpQuery->mQueryId;
        lpOut->miNumResults = static_cast<s32>(maScript.size());
        lpOut->mpaResults = maScript.data();
    }
};

namespace CgsCollision {
struct CollisionResult { u8 maRaw[80]; };
struct CollisionResultList
{
    CollisionResult* mpResults; u32 mu32UserTagA; u16 mu16UserTagB; u16 mu16MaxNumResults; u16 mu16NumResults; u8 meResultType;
    CollisionResult* GetResult(u16 lu16Index)
    {
        CGS_ASSERT(lu16Index < mu16NumResults, "lu16Index < mu16NumResults");
        return reinterpret_cast<CollisionResult*>(reinterpret_cast<u8*>(mpResults) + 112 * lu16Index);
    }
};
struct BaseCollisionGenerator
{
    int miCollides = 0, miFinishes = 0;
    std::vector<Vector3> maStarts, maEnds;
    std::vector<const void*> mapMaps;
    std::vector<u16> mau16Max; std::vector<u32> mauTagA; std::vector<u16> mau16TagB;
    std::vector<std::vector<CgsGeometric::PolySoupLineNearestResult>> maScripts;   // one per collide
    std::vector<CgsGeometric::PolySoupLineNearestResult> mCurrent;
    CollisionResultList mList = {};
    u16 CollideLineAgainstPolySoupList(const CgsGeometric::Line& lrLine, CgsGeometric::PolygonSoupListSpatialMap* lpMap,
                                       u16 lu16Max, u32 luTagA, u16 lu16TagB)
    {
        maStarts.push_back(Vector3{ lrLine.mStart.x, lrLine.mStart.y, lrLine.mStart.z, lrLine.mStart.w });
        maEnds.push_back(Vector3{ lrLine.mEnd.x, lrLine.mEnd.y, lrLine.mEnd.z, lrLine.mEnd.w });
        mapMaps.push_back(lpMap); mau16Max.push_back(lu16Max); mauTagA.push_back(luTagA); mau16TagB.push_back(lu16TagB);
        mCurrent = static_cast<size_t>(miCollides) < maScripts.size() ? maScripts[static_cast<size_t>(miCollides)]
                                                                     : std::vector<CgsGeometric::PolySoupLineNearestResult>();
        ++miCollides;
        mList.mpResults = reinterpret_cast<CollisionResult*>(mCurrent.data());
        mList.mu16NumResults = static_cast<u16>(mCurrent.size());
        return 7;
    }
    void Finish() { ++miFinishes; }
    CollisionResultList GetResultList(u16 luIndex) const
    {
        CGS_ASSERT(luIndex == 7, "luIndex < mu16NumUsedResultLists");
        return mList;
    }
};
}

struct FakeTriangleCollisionManager
{
    CgsGeometric::PolygonSoupListSpatialMap mMap = { 42 };
    CgsGeometric::PolygonSoupListSpatialMap* GetPolySoupListSpacialMap() { return &mMap; }
};

// ---- the module's file-local plumbing (unchanged by the fix; the production helpers are identical) ----
static s32 siProcessTriCollisionLineTestsPerfMon = -1;
static int giMonitorStarts = 0, giMonitorStops = 0;
inline void StartPassMonitor(s32) { ++giMonitorStarts; }
inline void StopPassMonitor(s32) { ++giMonitorStops; }
static s32 siStat_MaxTriColLineTest = 0;
inline EntityId InvalidEntityId() { EntityId l; l.SetInvalid(); return l; }
inline VolumeInstanceId InvalidVolumeInstanceId() { VolumeInstanceId l; l.SetInvalid(); return l; }
inline VolumeInstanceId ZeroVolumeInstanceId() { VolumeInstanceId l; l.muId = 0; return l; }
inline Vector3Plus LaneCopy(const Vector3& lrLane) { Vector3Plus l; l.x = lrLane.x; l.y = lrLane.y; l.z = lrLane.z; l.w = lrLane.w; return l; }
#include "fxsm_ltf_consts.inc"   // KU16_MAX_WORLD_LINE_TEST_RESULTS / KU_FINE_LINE_TEST_WORLD_TYPE_FLAG (from the source)

class SceneManagerModule
{
public:
    void ProcessLineTestFine(CgsCollision::BaseCollisionGenerator*, SceneManagerIO::TriCacheQueryBuffer*,
                             SpatialPartitionIO::OutputBuffer*, const SceneManagerIO::InEventLineTestFine*,
                             SceneManagerIO::OutputBuffer*, FineIntersectionTestIO::OutputBuffer*);
    void ProcessTriangleCollisionLineTests(CgsCollision::BaseCollisionGenerator*,
                                           CgsModule::EventQueue<SceneManagerIO::InEventTriangleCollisionLineTest, 256>*,
                                           SceneManagerIO::OutputBuffer*);
    FakeSpatialPartitionManager  mSpatialPartitionManager;
    FakeEntityManager            mEntityManager;
    FakeFineModule               mFineIntersectionTestModule;
    FakeTriangleCollisionManager mTriangleCollisionManager;
};
#include "fxsm_ltf_process_fine.inc"
#include "fxsm_ltf_process_tri.inc"
}   // CgsSceneManager

#include "fxsm_ltf_form.inc"   // FXSM_LTF_RECORDS_AT_16 (does the result record expose GetIntersections at +0x10?)

using namespace CgsSceneManager;

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}

static bool Same3(const Vector3& a, const Vector3& b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
static CgsGeometric::PolySoupLineNearestResult WorldHit(f32 lfX, f32 lfT, u32 luTag)
{
    CgsGeometric::PolySoupLineNearestResult r;
    std::memset(&r, 0, sizeof(r));
    r.mNormal = Vector3{ 0.0f, 1.0f, 0.0f, 0.0f };
    r.mPosition = Vector3{ lfX, -3.5f, 7.0f, 1.0f };
    r.mLineParam = Vector4{ lfT, lfT, lfT, lfT };
    for (int i = 0; i < 4; ++i) r.mau32Tag[i] = luTag;
    return r;
}
static LineTestIntersection EntityHit(u32 luIdWord, f32 lfT)
{
    LineTestIntersection r;
    std::memset(&r, 0, sizeof(r));
    r.mPosition = Vector3{ 10.0f + lfT, 1.0f, 2.0f, 1.0f };
    r.mNormal = Vector3{ 1.0f, 0.0f, 0.0f, 0.0f };
    r.mVolumeInstanceId.muId = 0x1122334455667788ull;
    r.mEntityId = CgsSceneManager::EntityId(luIdWord);
    r.mfLineParam = lfT;
    r.mu16MaterialTag = 0x0A0B; r.mu16GroupTag = 0x0C0D;
    return r;
}
static SceneManagerIO::InEventLineTestFine Query(u32 luFlags, u32 luQueryId, u32 luExclude, SceneManagerIO::EExclusionMode leMode, u8 lu8Vol)
{
    SceneManagerIO::InEventLineTestFine q;
    std::memset(&q, 0, sizeof(q));
    q.mLineStart = Vector3{ 1.0f, 60.0f, 3.0f, 0.5f };
    q.mLineEnd = Vector3{ 1.0f, -40.0f, 3.0f, 0.25f };
    q.mQueryId.mId = luQueryId;
    q.mx32EntityTypeFlags = luFlags;
    q.mExcludeEntityId = CgsSceneManager::EntityId(luExclude);
    q.meExclusionMode = leMode;
    q.mxVolumeTypeFlags = lu8Vol;
    return q;
}
static const SceneManagerIO::OutEventLineTestFineResult* Posted(SceneManagerIO::OutputBuffer& lrOut, size_t liIndex)
{
    if (liIndex >= lrOut.mResults.maAllocations.size()) return nullptr;
    return reinterpret_cast<const SceneManagerIO::OutEventLineTestFineResult*>(lrOut.mResults.maAllocations[liIndex].mpData);
}
static const LineTestIntersection* Record(SceneManagerIO::OutputBuffer& lrOut, size_t liEvent, int liRecord)
{
    if (liEvent >= lrOut.mResults.maAllocations.size()) return nullptr;
    return reinterpret_cast<const LineTestIntersection*>(lrOut.mResults.maAllocations[liEvent].mpData + 16 + 64 * liRecord);
}

int main()
{
    // ================= the result record (DWARF CgsSceneManagerModuleIO.h:217) =================
#if FXSM_LTF_RECORDS_AT_16
    {
        alignas(16) static u8 saImage[16 + 2 * 64];
        std::memset(saImage, 0, sizeof(saImage));
        const SceneManagerIO::OutEventLineTestFineResult* lpResult =
            reinterpret_cast<const SceneManagerIO::OutEventLineTestFineResult*>(saImage);
        Check(sizeof(SceneManagerIO::OutEventLineTestFineResult) == 16
              && reinterpret_cast<const u8*>(lpResult->GetIntersections()) == saImage + 16,
              "the 16-byte header is followed by the records (GetIntersections == +0x10)");
        Check(reinterpret_cast<const u8*>(&lpResult->GetIntersection(1).mEntityId) == saImage + 0x78,
              "record 1's EntityId is event + 0x78 -- the word the trigger consumer reads (+0x38 + 64)");
    }
#else
    Check(false, "the result record has GetIntersections() at +0x10 (DWARF :227)");
    Check(false, "(record layout not checkable)");
#endif

    // ================= the two results-queue helpers =================
    {
        SceneManagerIO::OutputBuffer* lpOut = new SceneManagerIO::OutputBuffer();
        SceneQueryId lId; lId.mId = 0x00050003u;
        LineTestIntersection* lpRecords = lpOut->mResults.AddLineTestFineResult(lId, 3);
        const bool lbOk = lpRecords != nullptr && lpOut->mResults.maAllocations.size() == 1;
        Check(lbOk && lpOut->mResults.maAllocations[0].miType == 1 && lpOut->mResults.maAllocations[0].miSize == 16 + 3 * 64,
              "AddLineTestFineResult(id, 3): ONE type-1 event of 16 + 3 * 64 bytes (0x828C4A24..30)");
        Check(lbOk && Posted(*lpOut, 0)->mQueryId.mId == 0x00050003u && Posted(*lpOut, 0)->miNumIntersections == 3
              && reinterpret_cast<u8*>(lpRecords) == lpOut->mResults.maAllocations[0].mpData + 16,
              "... header {id, 3} and the record area (event + 0x10) returned");

        CgsGeometric::PolySoupLineNearestResult laWorld[2] = { WorldHit(1.5f, 0.25f, 0x12345678u), WorldHit(2.5f, 0.75f, 0x0000FFFFu) };
        lId.mId = 0x00050006u;
        VolumeInstanceId lVolume; lVolume.muId = 0;
        lpRecords = lpOut->mResults.AddTriangleCollisionLineTestResult(lId, CgsSceneManager::EntityId(0u), lVolume, laWorld, 2);
        const bool lbOk2 = lpRecords != nullptr && lpOut->mResults.maAllocations.size() == 2;
        Check(lbOk2 && lpOut->mResults.maAllocations[1].miType == 1 && lpOut->mResults.maAllocations[1].miSize == 16 + 2 * 64
              && Posted(*lpOut, 1)->mQueryId.mId == 0x00050006u && Posted(*lpOut, 1)->miNumIntersections == 2,
              "AddTriangleCollisionLineTestResult: one type-1 event {id, n} of 16 + n * 64 bytes (0x828C4A74..A0)");
        Check(lbOk2 && Same3(lpRecords[0].mPosition, laWorld[0].mPosition) && Same3(lpRecords[0].mNormal, laWorld[0].mNormal)
              && lpRecords[0].mfLineParam == 0.25f && lpRecords[1].mfLineParam == 0.75f && Same3(lpRecords[1].mPosition, laWorld[1].mPosition),
              "... +0x40 -> position, +0x30 -> normal, lane 0 of +0x50 -> line param, 112-byte source stride");
        Check(lbOk2 && lpRecords[0].mu16MaterialTag == 0x1234 && lpRecords[0].mu16GroupTag == 0x5678
              && lpRecords[1].mu16MaterialTag == 0x0000 && lpRecords[1].mu16GroupTag == 0xFFFF,
              "... the +0x60 tag splits into material (high half, lhz) and group (low half)");
        Check(lbOk2 && static_cast<u32>(lpRecords[1].mEntityId) == 0u && lpRecords[1].mVolumeInstanceId.muId == 0ull,
              "... every record carries the caller's two ids (stw r29 / std r28)");
        delete lpOut;
    }

    // ================= ProcessLineTestFine =================
    // S1: world only (flags == 2) -> the triangle-collision queue, nothing else.
    {
        gaAsserts.clear();
        SceneManagerModule m; CgsCollision::BaseCollisionGenerator gen; SceneManagerIO::TriCacheQueryBuffer tri;
        SpatialPartitionIO::OutputBuffer sp; SceneManagerIO::OutputBuffer* out = new SceneManagerIO::OutputBuffer(); FineIntersectionTestIO::OutputBuffer fine;
        const SceneManagerIO::InEventLineTestFine q = Query(2u, 0x00050002u, 0xFFFFFFFFu, SceneManagerIO::E_EXCLUDE_ENTITY_ONLY, 0xFF);
        m.ProcessLineTestFine(&gen, &tri, &sp, &q, out, &fine);
        Check(tri.mLineQueue.GetLength() == 1 && Same3(tri.mLineQueue.GetEvent(0).mLineStart, q.mLineStart)
              && Same3(tri.mLineQueue.GetEvent(0).mLineEnd, q.mLineEnd) && tri.mLineQueue.GetEvent(0).mQueryId.mId == 0x00050002u,
              "S1 world-only (flags 2): one {start, end, id} parked on the triangle-collision line queue (0x828CDD48)");
        Check(sp.mCoarse.miBegins == 0 && m.mSpatialPartitionManager.mPartition.miCalls == 0 && out->mResults.maAllocations.empty()
              && gen.miCollides == 0 && gaAsserts.empty(),
              "S1 ... and no octree pass, no result, no world test, no assert");
        delete out;
    }
    // S2: entity flags, the octree finds nothing -> the EMPTY result {id, 0}.
    {
        gaAsserts.clear();
        SceneManagerModule m; CgsCollision::BaseCollisionGenerator gen; SceneManagerIO::TriCacheQueryBuffer tri;
        SpatialPartitionIO::OutputBuffer sp; SceneManagerIO::OutputBuffer* out = new SceneManagerIO::OutputBuffer(); FineIntersectionTestIO::OutputBuffer fine;
        const SceneManagerIO::InEventLineTestFine q = Query(32u, 0x00040011u, 0xFFFFFFFFu, SceneManagerIO::E_EXCLUDE_ENTITY_ONLY, 0x03);
        m.ProcessLineTestFine(&gen, &tri, &sp, &q, out, &fine);
        Check(sp.mCoarse.miBegins == 1 && sp.mCoarse.miEnds == 1 && m.mSpatialPartitionManager.mPartition.miCalls == 1
              && m.mSpatialPartitionManager.mPartition.muFlags == 32u && Same3(m.mSpatialPartitionManager.mPartition.mStart, q.mLineStart),
              "S2 the octree LineTest runs once, bracketed by Begin/EndResultsBatch, with the query's flags and line");
        Check(out->mResults.maAllocations.size() == 1 && out->mResults.maAllocations[0].miSize == 16
              && Posted(*out, 0)->mQueryId.mId == 0x00040011u && Posted(*out, 0)->miNumIntersections == 0
              && m.mFineIntersectionTestModule.miCalls == 0 && tri.mLineQueue.GetLength() == 0 && gaAsserts.empty(),
              "S2 no candidate: AddLineTestFineResult(id, 0) (0x828CE048), no fine test");
        delete out;
    }
    // S3: the only candidate is the excluded entity -> no fine test, empty result.
    {
        gaAsserts.clear();
        SceneManagerModule m; CgsCollision::BaseCollisionGenerator gen; SceneManagerIO::TriCacheQueryBuffer tri;
        SpatialPartitionIO::OutputBuffer sp; SceneManagerIO::OutputBuffer* out = new SceneManagerIO::OutputBuffer(); FineIntersectionTestIO::OutputBuffer fine;
        m.mSpatialPartitionManager.mPartition.maScript = { 5 };
        m.mEntityManager.muKnownId = 0x01000400u; m.mEntityManager.miKnownIndex = 5;
        const SceneManagerIO::InEventLineTestFine q = Query(32u, 0x00040012u, 0x01000400u, SceneManagerIO::E_EXCLUDE_ENTITY_ONLY, 0x03);
        m.ProcessLineTestFine(&gen, &tri, &sp, &q, out, &fine);
        Check(m.mFineIntersectionTestModule.miCalls == 0 && out->mResults.maAllocations.size() == 1
              && Posted(*out, 0)->miNumIntersections == 0 && gaAsserts.empty(),
              "S3 one candidate == the excluded entity's index: skipped (`cmpwi 1 ; lhz batch[0] == index`), empty result");
        delete out;
    }
    // S4: two candidates -> the fine module's record, and its hits published with their public ids.
    {
        gaAsserts.clear();
        SceneManagerModule m; CgsCollision::BaseCollisionGenerator gen; SceneManagerIO::TriCacheQueryBuffer tri;
        SpatialPartitionIO::OutputBuffer sp; SceneManagerIO::OutputBuffer* out = new SceneManagerIO::OutputBuffer(); FineIntersectionTestIO::OutputBuffer fine;
        m.mSpatialPartitionManager.mPartition.maScript = { 3, 9 };
        m.mEntityManager.muKnownId = 0x01000C00u; m.mEntityManager.miKnownIndex = 3;
        m.mFineIntersectionTestModule.maScript = { EntityHit(0xABCD0009u, 0.3f), EntityHit(0x00000003u, 0.6f) };
        const SceneManagerIO::InEventLineTestFine q = Query(32u, 0x00040013u, 0x01000C00u, SceneManagerIO::E_EXCLUDE_ALL_CHILD_PARTS, 0x0C);
        m.ProcessLineTestFine(&gen, &tri, &sp, &q, out, &fine);
        const FineIntersectionTestIO::InEventLineTestFine& f = m.mFineIntersectionTestModule.mLast;
        Check(m.mFineIntersectionTestModule.miCalls == 1 && Same3(f.mLineStart, q.mLineStart) && Same3(f.mLineEnd, q.mLineEnd)
              && f.mQueryId.mId == 0x00040013u && f.mpau16EntityIndices == sp.mCoarse.GetResultsBatch() && f.mu16NumEntities == 2
              && f.mu16ExcludeEntityIndex == 3 && f.mxVolumeTypeFlags == 0x0C && f.mbExcludeParts,
              "S4 the fine record {start, end, id, candidates, 2, exclude index 3, volume flags 0x0C, excludeParts (mode 1)}");
        Check(m.mFineIntersectionTestModule.mpLastArray == fine.maArray && fine.miGets == 1,
              "S4 ... into lpFineTestOutputBuffer->GetLineTestIntersectionArray() (0x828B09E0)");
        const LineTestIntersection* r0 = Record(*out, 0, 0); const LineTestIntersection* r1 = Record(*out, 0, 1);
        Check(out->mResults.maAllocations.size() == 1 && out->mResults.maAllocations[0].miSize == 16 + 2 * 64
              && Posted(*out, 0)->mQueryId.mId == 0x00040013u && Posted(*out, 0)->miNumIntersections == 2,
              "S4 one type-1 event {id, 2}");
        Check(r0 && r1 && static_cast<u32>(r0->mEntityId) == (0x02000000u | (9u << 10))
              && static_cast<u32>(r1->mEntityId) == (0x02000000u | (3u << 10)),
              "S4 each record's id = GetEntityIdByIndex(the id word's LOW half) (`clrlwi r31, r11, 16`)");
        Check(r0 && r0->mfLineParam == 0.3f && r0->mVolumeInstanceId.muId == 0x1122334455667788ull && r0->mu16MaterialTag == 0x0A0B
              && r0->mu16GroupTag == 0x0C0D && Same3(r0->mPosition, m.mFineIntersectionTestModule.maScript[0].mPosition),
              "S4 ... the rest of the 64-byte record copied whole (8 x ld/std)");
        Check(gaAsserts.empty() && tri.mLineQueue.GetLength() == 0 && gen.miCollides == 0, "S4 no world test (world bit clear), no assert");
        delete out;
    }
    // S5: world + entity, both answer -> world hits FIRST (invalid ids), then the entity hits.
    {
        gaAsserts.clear();
        SceneManagerModule m; CgsCollision::BaseCollisionGenerator gen; SceneManagerIO::TriCacheQueryBuffer tri;
        SpatialPartitionIO::OutputBuffer sp; SceneManagerIO::OutputBuffer* out = new SceneManagerIO::OutputBuffer(); FineIntersectionTestIO::OutputBuffer fine;
        m.mSpatialPartitionManager.mPartition.maScript = { 4 };
        m.mFineIntersectionTestModule.maScript = { EntityHit(0x00000004u, 0.5f) };
        gen.maScripts = { { WorldHit(1.0f, 0.2f, 0x00070009u), WorldHit(2.0f, 0.9f, 0x00080001u) } };
        const SceneManagerIO::InEventLineTestFine q = Query(34u, 0x00040014u, 0xFFFFFFFFu, SceneManagerIO::E_EXCLUDE_ENTITY_ONLY, 0x01);
        m.ProcessLineTestFine(&gen, &tri, &sp, &q, out, &fine);
        Check(gen.miCollides == 1 && gen.mau16Max[0] == 0x20 && gen.mauTagA[0] == 0u && gen.mau16TagB[0] == 0
              && gen.mapMaps[0] == &m.mTriangleCollisionManager.mMap && Same3(gen.maStarts[0], q.mLineStart)
              && gen.maStarts[0].w == q.mLineStart.w && Same3(gen.maEnds[0], q.mLineEnd) && gen.miFinishes == 1,
              "S5 CollideLineAgainstPolySoupList(whole-lane line, this+0x3A82C0, 0x20, 0, 0) then Finish (0x828CDFD8/E4)");
        const LineTestIntersection* w0 = Record(*out, 0, 0); const LineTestIntersection* w1 = Record(*out, 0, 1);
        const LineTestIntersection* e0 = Record(*out, 0, 2);
        Check(out->mResults.maAllocations.size() == 1 && Posted(*out, 0)->miNumIntersections == 3
              && out->mResults.maAllocations[0].miSize == 16 + 3 * 64,
              "S5 ONE event holding 2 world + 1 entity records");
        Check(w0 && w1 && w0->mfLineParam == 0.2f && w1->mfLineParam == 0.9f && Same3(w0->mPosition, gen.maScripts[0][0].mPosition)
              && Same3(w0->mNormal, gen.maScripts[0][0].mNormal) && w0->mu16MaterialTag == 0x0007 && w0->mu16GroupTag == 0x0009,
              "S5 the world hits come first, field for field (112-byte stride)");
        Check(w0 && w1 && static_cast<u32>(w0->mEntityId) == 0xFFFFFFFFu && w0->mVolumeInstanceId.muId == 0xFFFFFFFFFFFFFFFFull
              && static_cast<u32>(w1->mEntityId) == 0xFFFFFFFFu,
              "S5 ... stamped K_INVALID_ENTITY_ID (dword_82F33F64) / K_INVALID_VOLUME_INSTANCE_ID (qword_82F33F70)");
        Check(e0 && e0->mfLineParam == 0.5f && static_cast<u32>(e0->mEntityId) == (0x02000000u | (4u << 10)),
              "S5 then the entity hit, id resolved");
        Check(gaAsserts.empty() && tri.mLineQueue.GetLength() == 0, "S5 nothing parked, no assert");
        delete out;
    }
    // S6: world + entity, no entity hit -> parked on the triangle-collision queue, no result now.
    {
        gaAsserts.clear();
        SceneManagerModule m; CgsCollision::BaseCollisionGenerator gen; SceneManagerIO::TriCacheQueryBuffer tri;
        SpatialPartitionIO::OutputBuffer sp; SceneManagerIO::OutputBuffer* out = new SceneManagerIO::OutputBuffer(); FineIntersectionTestIO::OutputBuffer fine;
        const SceneManagerIO::InEventLineTestFine q = Query(34u, 0x00040015u, 0xFFFFFFFFu, SceneManagerIO::E_EXCLUDE_ENTITY_ONLY, 0x01);
        m.ProcessLineTestFine(&gen, &tri, &sp, &q, out, &fine);
        Check(tri.mLineQueue.GetLength() == 1 && tri.mLineQueue.GetEvent(0).mQueryId.mId == 0x00040015u
              && out->mResults.maAllocations.empty() && gen.miCollides == 0 && sp.mCoarse.miBegins == 1,
              "S6 world bit with no entity hit: the octree ran, then the test is parked (0x828CDF78..A0), nothing published");
        delete out;
    }
    // S7: world + entity, the world finds nothing -> the entity hit alone.
    {
        gaAsserts.clear();
        SceneManagerModule m; CgsCollision::BaseCollisionGenerator gen; SceneManagerIO::TriCacheQueryBuffer tri;
        SpatialPartitionIO::OutputBuffer sp; SceneManagerIO::OutputBuffer* out = new SceneManagerIO::OutputBuffer(); FineIntersectionTestIO::OutputBuffer fine;
        m.mSpatialPartitionManager.mPartition.maScript = { 6 };
        m.mFineIntersectionTestModule.maScript = { EntityHit(0x00000006u, 0.4f) };
        const SceneManagerIO::InEventLineTestFine q = Query(34u, 0x00040016u, 0xFFFFFFFFu, SceneManagerIO::E_EXCLUDE_ENTITY_ONLY, 0x01);
        m.ProcessLineTestFine(&gen, &tri, &sp, &q, out, &fine);
        const LineTestIntersection* e0 = Record(*out, 0, 0);
        Check(gen.miCollides == 1 && out->mResults.maAllocations.size() == 1 && Posted(*out, 0)->miNumIntersections == 1
              && e0 && static_cast<u32>(e0->mEntityId) == (0x02000000u | (6u << 10)) && gaAsserts.empty(),
              "S7 an empty world answer publishes the entity hit alone (no GetResult on an empty list)");
        delete out;
    }
    // S8..S10: the three tripwires.
    {
        gaAsserts.clear();
        SceneManagerModule m; CgsCollision::BaseCollisionGenerator gen; SceneManagerIO::TriCacheQueryBuffer tri;
        SpatialPartitionIO::OutputBuffer sp; SceneManagerIO::OutputBuffer* out = new SceneManagerIO::OutputBuffer(); FineIntersectionTestIO::OutputBuffer fine;
        m.mSpatialPartitionManager.mPartition.maScript = { 1, 2 };
        m.mSpatialPartitionManager.mPartition.miScriptAttempted = 5;
        m.mEntityManager.mu16InvalidIndex = 2;
        m.mFineIntersectionTestModule.maScript = { EntityHit(0x00000002u, 0.1f) };
        const SceneManagerIO::InEventLineTestFine q = Query(32u, 0x00040017u, 0x0100FFFFu, SceneManagerIO::E_EXCLUDE_ENTITY_ONLY, 0x01);
        m.ProcessLineTestFine(&gen, &tri, &sp, &q, out, &fine);
        Check(AssertedWith("Entity not found (LineTestFine)"), "S8 an exclude id the entity manager does not know fires :1048");
        Check(AssertedWith("miActualNumResults == lpCoarseResult->miNumResultsStored"), "S9 attempted != written fires :1054");
        Check(AssertedWith("lID != K_INVALID_ENTITY_ID"), "S10 a hit whose index resolves to the invalid id fires :1134");
        Check(m.mFineIntersectionTestModule.miCalls == 1 && m.mFineIntersectionTestModule.mLast.mu16ExcludeEntityIndex == 0xFFFF,
              "S8 ... the tripwires do not gate: the fine test still runs, exclude index 0xFFFF");
        delete out;
    }

    // ================= ProcessTriangleCollisionLineTests =================
    {
        gaAsserts.clear();
        giMonitorStarts = giMonitorStops = 0;
        SceneManagerModule m; CgsCollision::BaseCollisionGenerator gen;
        SceneManagerIO::OutputBuffer* out = new SceneManagerIO::OutputBuffer();
        CgsModule::EventQueue<SceneManagerIO::InEventTriangleCollisionLineTest, 256> queue;
        SceneManagerIO::InEventTriangleCollisionLineTest t;
        std::memset(&t, 0, sizeof(t));
        t.mLineStart = Vector3{ 5.0f, 50.0f, 6.0f, 0.0f }; t.mLineEnd = Vector3{ 5.0f, -50.0f, 6.0f, 0.0f };
        t.mQueryId.mId = 0x00050001u; queue.AddEvent(t);
        t.mLineStart.x = 9.0f; t.mLineEnd.x = 9.0f; t.mQueryId.mId = 0x00050004u; queue.AddEvent(t);
        gen.maScripts = { { WorldHit(5.0f, 0.535f, 0xAAAA0001u), WorldHit(5.0f, 0.1f, 0xBBBB0002u), WorldHit(5.0f, 0.9f, 0xCCCC0003u) }, {} };
        m.ProcessTriangleCollisionLineTests(&gen, &queue, out);
        Check(gen.miCollides == 2 && gen.miFinishes == 2 && gen.mau16Max[0] == 0x20 && gen.mau16Max[1] == 0x20
              && gen.mauTagA[0] == 0u && gen.mau16TagB[1] == 0 && gen.mapMaps[1] == &m.mTriangleCollisionManager.mMap
              && gen.maStarts[1].x == 9.0f && gen.maEnds[1].y == -50.0f,
              "T1 every queued test is run: CollideLineAgainstPolySoupList(line, the tri map, 0x20, 0, 0) + Finish");
        Check(out->mResults.maAllocations.size() == 2 && Posted(*out, 0) && Posted(*out, 0)->mQueryId.mId == 0x00050001u
              && Posted(*out, 0)->miNumIntersections == 3 && out->mResults.maAllocations[0].miSize == 16 + 3 * 64,
              "T1 one type-1 answer per test: {0x00050001, 3}");
        Check(out->mResults.maAllocations.size() == 2 && Posted(*out, 1) && Posted(*out, 1)->mQueryId.mId == 0x00050004u
              && Posted(*out, 1)->miNumIntersections == 0 && out->mResults.maAllocations[1].miSize == 16,
              "T1 an empty world answer is still published: {0x00050004, 0}");
        const LineTestIntersection* r0 = Record(*out, 0, 0); const LineTestIntersection* r2 = Record(*out, 0, 2);
        Check(r0 && r2 && r0->mfLineParam == 0.535f && r2->mfLineParam == 0.9f && static_cast<u32>(r0->mEntityId) == 0u
              && r0->mVolumeInstanceId.muId == 0ull && r0->mu16MaterialTag == 0xAAAA && r2->mu16GroupTag == 0x0003,
              "T1 records via AddTriangleCollisionLineTestResult with ids 0 / 0 (r26 / r25), in the kernel's order");
        Check(siStat_MaxTriColLineTest == 2 && giMonitorStarts == 1 && giMonitorStops == 1 && gaAsserts.empty(),
              "T1 high-water mark 2 (dword_83084888), one monitor bracket, no assert");
        delete out;
    }

    std::printf("FxScenemgrLineTestFine: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
