// FX-FOLLOWUPS (crash parity 2026-09-25, item 2 step E2): SceneManagerModule::ProcessVolumeTestDeepest @0x828D4460 (a
// CGS_ASSERT(false) trap before) -- the scene manager's deepest-volume query, the director camera's "inside geometry"
// sphere (flags 30).
// run_fxfollowups_process_volume_test_deepest.py extracts the revision's body and compiles it here against fakes of
// its collaborators: the collision generator's sphere-vs-world test, the spatial partition's VolumeTest and coarse
// result buffer, the entity table, the fine module's ComputeVolumeTestDeepest, and the results queue.
//
// The console, from the ARTIST listing (asserts are CgsSceneManagerBridgeFunctions.cpp):
//   * the event is read at +0x00 transform, +0x40 query id, +0x44 flags, +0x48 exclude id, +0x4C exclusion mode,
//     +0x50 the 128-byte volume, +0xD0 the volume-type flags;
//   * flags & 2 (the world): the volume must be a sphere (else the :1557 stream assert; the test still runs);
//     sphere = {event translation + the volume's relative translation, the volume's radius};
//     TestSphereAgainstPolySoupList(&sphere, the triangle manager's map, 0, 0), Finish, and a non-zero result count
//     posts {id, 0.0, 1} (type 5) and returns -- a miss falls through;
//   * the entities: "lpVolume != NULL" (:1594), the batch around partition slot 8 VolumeTest(flags, volume,
//     transform, buffer); the exclude id resolved (not found -> :1626); :1629 / :1632; no candidate or only the
//     excluded one -> {id, 0.0, 0}; else the fine query -> ComputeVolumeTestDeepest -> its {id, depth, hit};
//     both post type 5. The TriCacheQueryBuffer argument is never read.
#include "types.hpp"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneQueryId.h"   // SceneQueryId (real; the event's mQueryId since REVIEW-K)
#include "GameShared/GameClasses/SceneManager/CgsVolumeStore.h"    // VolumeSlot (real; the event's mVolumeBuffer since REVIEW-K)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventVolumeTestDeepest.h"   // the 224-byte event (the revision's)

static std::vector<std::string> gaAsserts;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) { gaAsserts.push_back(lpcMessage ? lpcMessage : ""); return 0; }
void* EndAssert() { return nullptr; }
} }
static bool AssertedWith(const char* lpcNeedle)
{
    for (const std::string& s : gaAsserts) if (s.find(lpcNeedle) != std::string::npos) return true;
    return false;
}

struct Vector4 { f32 x, y, z, w; };
struct Matrix44Affine { Vector4 xAxis, yAxis, zAxis, wAxis; };

namespace rw { namespace collision {
    enum VolumeType { E_VOLUMETYPE_SPHERE = 1, E_VOLUMETYPE_BBOX = 4 };
    // The SDK view the scene module includes (volume_debug_access.h): +0x00 relative transform, +0x40 the type
    // (the descriptor's typeID on the console), +0x50 radius.
    class Volume
    {
    public:
        u32 GetType() const { u32 lu; std::memcpy(&lu, maPayload + 0x40, 4); return lu; }
        f32 GetRadius() const { f32 lf; std::memcpy(&lf, maPayload + 0x50, 4); return lf; }
        const Matrix44Affine& GetRelativeTransform() const { return *reinterpret_cast<const Matrix44Affine*>(maPayload); }
        alignas(16) u8 maPayload[96];
    };
} }

namespace CgsGeometric {
    struct PolygonSoupListSpatialMap { int miTag; };
    struct Sphere { Vector4 mPositionRadius; };
}

namespace CgsCollision {
    struct CollisionResultList { u16 mu16NumResults; };
    struct BaseCollisionGenerator
    {
        int miTests = 0, miFinishes = 0;
        CgsGeometric::Sphere mLastSphere = {};
        const CgsGeometric::PolygonSoupListSpatialMap* mpLastMap = nullptr;
        u32 muLastTagA = 99; u16 mu16LastTagB = 99;
        u16 mu16ScriptCount = 0;
        u16 TestSphereAgainstPolySoupList(const CgsGeometric::Sphere* lpSphere, CgsGeometric::PolygonSoupListSpatialMap* lpMap,
                                          u32 luTagA, u16 lu16TagB)
        {
            ++miTests; mLastSphere = *lpSphere; mpLastMap = lpMap; muLastTagA = luTagA; mu16LastTagB = lu16TagB;
            return 3;
        }
        void Finish() { ++miFinishes; }
        CollisionResultList GetResultList(u16 lu16Index) const
        {
            CollisionResultList l; l.mu16NumResults = (lu16Index == 3) ? mu16ScriptCount : 0xEEEE; return l;
        }
    };
}

namespace CgsSceneManager
{
    namespace VolRef { struct Volume; }
    // SceneQueryId and VolumeSlot are the real ones (included above): since REVIEW-K the event names its members with them.

    template <s32 N>
    struct CoarseQueryResultBuffer
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

    namespace SpatialPartitionIO {
        struct OutputBuffer
        {
            CoarseQueryResultBuffer<16384> mCoarse;
            CoarseQueryResultBuffer<16384>* GetCoarseResultBuffer() { return &mCoarse; }
        };
    }

    namespace SceneManagerIO {
        enum EExclusionMode { E_EXCLUDE_ENTITY_ONLY = 0, E_EXCLUDE_ALL_CHILD_PARTS = 1 };
        struct OutEventVolumeTestDeepestResult { SceneQueryId mQueryId; f32 mfDepth; bool mbIntersection; };
        struct Posted { s32 miType; OutEventVolumeTestDeepestResult mRecord; };
        template <s32 SizeBytes>
        struct OutSceneQueryResultsQueue
        {
            std::vector<Posted> maPosted;
            template <typename T> void AddEvent(const T* lpEvent, s32 liType)
            {
                Posted l; l.miType = liType; std::memcpy(&l.mRecord, lpEvent, sizeof(OutEventVolumeTestDeepestResult));
                maPosted.push_back(l);
            }
        };
        struct OutputBuffer
        {
            OutSceneQueryResultsQueue<32768> mResults;
            OutSceneQueryResultsQueue<32768>* GetResultsQueue() { return &mResults; }
        };
        struct TriCacheQueryBuffer { int miUnused; };
    }

    struct FakePartition
    {
        int miCalls = 0;
        u32 muFlags = 0;
        const VolRef::Volume* mpVolume = nullptr;
        const Matrix44Affine* mpTransform = nullptr;
        std::vector<u16> maScript;
        s32 miScriptAttempted = -1;   // -1: == written
        bool VolumeTest(u32 luFlags, const VolRef::Volume* lpVolume, const Matrix44Affine* lpTransform,
                        CoarseQueryResultBuffer<16384>* lpBuffer)
        {
            ++miCalls; muFlags = luFlags; mpVolume = lpVolume; mpTransform = lpTransform;
            lpBuffer->maResults = maScript;
            lpBuffer->miAttempted = miScriptAttempted < 0 ? static_cast<s32>(maScript.size()) : miScriptAttempted;
            return !maScript.empty();
        }
    };
    struct FakeSpatialPartitionManager { FakePartition mPartition; FakePartition* GetSpatialPartition() { return &mPartition; } };

    struct FakeEntityManager
    {
        u32 muKnownId = 0; s32 miKnownIndex = -1;
        s32 GetEntityIndexByID(EntityId lId) const { return static_cast<u32>(lId) == muKnownId ? miKnownIndex : -1; }
    };

    namespace FineIntersectionTestIO {
        struct InEventVolumeTestDeepest
        {
            Matrix44Affine mTransform; VolumeSlot mVolumeBuffer; SceneQueryId mQueryId; const u16* mpau16EntityIndices;
            u16 mu16NumEntities; u16 mu16ExcludeEntityIndex; u8 mxVolumeTypeFlags; bool mbExcludeParts;
        };
        struct OutEventVolumeTestDeepestResult { SceneQueryId mQueryId; f32 mfDepth; bool mbIntersection; };
    }

    struct FakeFineModule
    {
        int miCalls = 0;
        FineIntersectionTestIO::InEventVolumeTestDeepest mLast = {};
        bool mbHit = false; f32 mfDepth = 0.0f; u32 muAnswerId = 0;
        void ComputeVolumeTestDeepest(const FineIntersectionTestIO::InEventVolumeTestDeepest* lpQuery,
                                      FineIntersectionTestIO::OutEventVolumeTestDeepestResult* lpOut)
        {
            ++miCalls; mLast = *lpQuery;
            lpOut->mbIntersection = false;                 // as the console: +8, +0 up front, +4 only on a hit
            lpOut->mQueryId.mId   = muAnswerId ? muAnswerId : lpQuery->mQueryId.mId;
            if (mbHit) { lpOut->mfDepth = mfDepth; lpOut->mbIntersection = true; }
        }
    };

    struct FakeTriangleCollisionManager
    {
        CgsGeometric::PolygonSoupListSpatialMap mMap = { 42 };
        CgsGeometric::PolygonSoupListSpatialMap* GetPolySoupListSpacialMap() { return &mMap; }
    };

    inline EntityId InvalidEntityId() { EntityId l; l.SetInvalid(); return l; }   // the production helper, verbatim
    // The production [DIAG] witness (BRN_SCENE_QUERY_DIAG, default off) -- a no-op here.
    inline void NoteVolumeTestDeepest(u32, u32, s32, s32, u16, bool, const SceneManagerIO::OutEventVolumeTestDeepestResult&) {}

    class SceneManagerModule
    {
    public:
        void ProcessVolumeTestDeepest(CgsCollision::BaseCollisionGenerator*, SceneManagerIO::TriCacheQueryBuffer*,
                                      const SceneManagerIO::InEventVolumeTestDeepest*, SpatialPartitionIO::OutputBuffer*,
                                      SceneManagerIO::OutputBuffer*);
        FakeSpatialPartitionManager  mSpatialPartitionManager;
        FakeEntityManager            mEntityManager;
        FakeFineModule               mFineIntersectionTestModule;
        FakeTriangleCollisionManager mTriangleCollisionManager;
    };
#include "fxfu_pvtd_body.inc"   // SceneManagerModule::ProcessVolumeTestDeepest from the revision
}

using namespace CgsSceneManager;

static unsigned guChecks = 0, guFailures = 0;
static void Check(bool lbPass, const char* lpcLabel)
{
    ++guChecks;
    if (!lbPass) ++guFailures;
    std::printf("%s  %s\n", lbPass ? "PASS" : "FAIL", lpcLabel);
}

struct World
{
    SceneManagerModule                     mModule;
    CgsCollision::BaseCollisionGenerator   mGenerator;
    SpatialPartitionIO::OutputBuffer       mSpatial;
    SceneManagerIO::OutputBuffer           mOut;
    SceneManagerIO::TriCacheQueryBuffer    mTriCache = { 0x7777 };
    SceneManagerIO::InEventVolumeTestDeepest mEvent;
};

static void Put32(SceneManagerIO::InEventVolumeTestDeepest& lrEvent, u32 luOffset, u32 luValue)
{
    std::memcpy(lrEvent.macOpaquePayload + luOffset, &luValue, 4);
}
static void PutF(SceneManagerIO::InEventVolumeTestDeepest& lrEvent, u32 luOffset, f32 lfValue)
{
    std::memcpy(lrEvent.macOpaquePayload + luOffset, &lfValue, 4);
}

// flags, exclude id, mode, volume type; transform translation (10, 20, 30); the volume's relative translation
// (1, 2, 3), radius 0.1; volume-type flags 0x5A; distinctive bytes through the rest of the volume image.
static void MakeEvent(World& lrWorld, u32 luFlags, u32 luExclude, u32 luMode, u32 luType)
{
    SceneManagerIO::InEventVolumeTestDeepest& e = lrWorld.mEvent;
    std::memset(e.macOpaquePayload, 0, sizeof(e.macOpaquePayload));
    for (u32 lu = 0; lu < 16; ++lu) PutF(e, lu * 4, static_cast<f32>(lu) * 0.5f);   // rows 0..2
    PutF(e, 0x30, 10.0f); PutF(e, 0x34, 20.0f); PutF(e, 0x38, 30.0f); PutF(e, 0x3C, 1.0f);
    Put32(e, 0x40, 0x10077u);
    Put32(e, 0x44, luFlags);
    Put32(e, 0x48, luExclude);
    Put32(e, 0x4C, luMode);
    for (u32 lu = 0; lu < 0x80; ++lu) e.macOpaquePayload[0x50 + lu] = static_cast<u8>(0xA0 + lu);
    for (u32 lu = 0; lu < 12; ++lu) PutF(e, 0x50 + lu * 4, 0.25f);                    // relative rows 0..2
    PutF(e, 0x50 + 0x30, 1.0f); PutF(e, 0x50 + 0x34, 2.0f); PutF(e, 0x50 + 0x38, 3.0f); PutF(e, 0x50 + 0x3C, 1.0f);
    Put32(e, 0x50 + 0x40, luType);
    PutF(e, 0x50 + 0x50, 0.1f);
    e.macOpaquePayload[0xD0] = 0x5A;
}

static void Run(World& lrWorld)
{
    lrWorld.mModule.ProcessVolumeTestDeepest(&lrWorld.mGenerator, &lrWorld.mTriCache, &lrWorld.mEvent, &lrWorld.mSpatial,
                                             &lrWorld.mOut);
}

static bool PostedOnce(const World& lrWorld, u32 luId, f32 lfDepth, bool lbHit)
{
    if (lrWorld.mOut.mResults.maPosted.size() != 1) return false;
    const SceneManagerIO::Posted& p = lrWorld.mOut.mResults.maPosted[0];
    return p.miType == 5 && p.mRecord.mQueryId.mId == luId && p.mRecord.mfDepth == lfDepth && p.mRecord.mbIntersection == lbHit;
}

int main()
{
    // ---- W. the world arm -----------------------------------------------------------------------------------------
    {
        World* w = new World(); gaAsserts.clear();
        MakeEvent(*w, 30u, 0xFFFFFFFFu, 0u, 1u);
        w->mGenerator.mu16ScriptCount = 1;
        Run(*w);
        const CgsGeometric::Sphere& s = w->mGenerator.mLastSphere;
        std::printf("W1: sphere (%g, %g, %g, %g), %d tests, %zu posted\n", static_cast<double>(s.mPositionRadius.x),
                    static_cast<double>(s.mPositionRadius.y), static_cast<double>(s.mPositionRadius.z),
                    static_cast<double>(s.mPositionRadius.w), w->mGenerator.miTests, w->mOut.mResults.maPosted.size());
        Check(w->mGenerator.miTests == 1 && w->mGenerator.miFinishes == 1
              && s.mPositionRadius.x == 11.0f && s.mPositionRadius.y == 22.0f && s.mPositionRadius.z == 33.0f
              && s.mPositionRadius.w == 0.1f && w->mGenerator.mpLastMap == &w->mModule.mTriangleCollisionManager.mMap
              && w->mGenerator.muLastTagA == 0u && w->mGenerator.mu16LastTagB == 0u,
              "W1 world bit: TestSphereAgainstPolySoupList on {event translation + the volume's relative translation, "
              "radius}, the triangle manager's map, tags 0 / 0, then Finish (0x828D4514..0x828D456C)");
        Check(PostedOnce(*w, 0x10077u, 0.0f, true) && w->mModule.mSpatialPartitionManager.mPartition.miCalls == 0
              && w->mSpatial.mCoarse.miBegins == 0 && w->mModule.mFineIntersectionTestModule.miCalls == 0 && gaAsserts.empty(),
              "W2 a world hit posts {id, 0.0 (flt_82001CC0), 1} as type 5 and RETURNS -- no coarse or fine pass");
        delete w;
    }
    {
        World* w = new World(); gaAsserts.clear();
        MakeEvent(*w, 30u, 0xFFFFFFFFu, 0u, 4u);   // a box: the stream assert, and the test still runs
        w->mGenerator.mu16ScriptCount = 0;          // and misses -> the entities
        Run(*w);
        Check(AssertedWith("Currently only support sphere tests against world\n") && w->mGenerator.miTests == 1
              && w->mModule.mSpatialPartitionManager.mPartition.miCalls == 1,
              "W3 a non-sphere volume with the world bit fires :1557 (0x820F6DB8) and still runs the world test; a world "
              "miss falls through to the entity pass");
        delete w;
    }

    // ---- E. the entity arm ----------------------------------------------------------------------------------------
    {
        World* w = new World(); gaAsserts.clear();
        MakeEvent(*w, 0x1Cu, 0xFFFFFFFFu, 0u, 1u);
        Run(*w);
        const FakePartition& p = w->mModule.mSpatialPartitionManager.mPartition;
        Check(w->mGenerator.miTests == 0 && p.miCalls == 1 && p.muFlags == 0x1Cu
              && p.mpVolume == reinterpret_cast<const VolRef::Volume*>(w->mEvent.macOpaquePayload + 0x50)
              && p.mpTransform == reinterpret_cast<const Matrix44Affine*>(w->mEvent.macOpaquePayload)
              && w->mSpatial.mCoarse.miBegins == 1 && w->mSpatial.mCoarse.miEnds == 1,
              "E1 no world bit: partition slot 8 VolumeTest(flags, event+0x50, event+0x00, the coarse buffer) inside one "
              "Begin/EndResultsBatch; no sphere test");
        Check(PostedOnce(*w, 0x10077u, 0.0f, false) && w->mModule.mFineIntersectionTestModule.miCalls == 0 && gaAsserts.empty(),
              "E2 no candidate -> {id, 0.0, 0} as type 5, no fine call (0x828D4848)");
        delete w;
    }
    {
        World* w = new World(); gaAsserts.clear();
        MakeEvent(*w, 0x1Cu, 0xFFFFFFFFu, 0u, 1u);
        w->mModule.mSpatialPartitionManager.mPartition.maScript = { 4, 9 };
        w->mModule.mFineIntersectionTestModule.mbHit = true;
        w->mModule.mFineIntersectionTestModule.mfDepth = 0.37f;
        Run(*w);
        const FineIntersectionTestIO::InEventVolumeTestDeepest& q = w->mModule.mFineIntersectionTestModule.mLast;
        const bool lbTransform = std::memcmp(&q.mTransform, w->mEvent.macOpaquePayload, 64) == 0;
        const bool lbVolume = std::memcmp(&q.mVolumeBuffer, w->mEvent.macOpaquePayload + 0x50, 0x80) == 0;
        Check(w->mModule.mFineIntersectionTestModule.miCalls == 1 && lbTransform && lbVolume && q.mQueryId.mId == 0x10077u
              && q.mpau16EntityIndices == w->mSpatial.mCoarse.maResults.data() && q.mu16NumEntities == 2
              && q.mu16ExcludeEntityIndex == 0xFFFF && q.mxVolumeTypeFlags == 0x5A && !q.mbExcludeParts,
              "E3 the fine query: the transform, the whole 0x80-byte volume image, the id, the candidates and their "
              "count, exclude index 0xFFFF, the volume-type flags (+0xD0), exclude parts off (0x828D47AC..0x828D4820)");
        Check(PostedOnce(*w, 0x10077u, 0.37f, true) && gaAsserts.empty(),
              "E4 the fine answer is posted as it came back: {id, depth, hit} as type 5");
        delete w;
    }
    {
        World* w = new World(); gaAsserts.clear();
        MakeEvent(*w, 0x1Cu, 0x02001405u, 0u, 1u);   // exclude a known entity, entity-only
        w->mModule.mEntityManager.muKnownId = 0x02001405u; w->mModule.mEntityManager.miKnownIndex = 4;
        w->mModule.mSpatialPartitionManager.mPartition.maScript = { 4 };
        Run(*w);
        Check(PostedOnce(*w, 0x10077u, 0.0f, false) && w->mModule.mFineIntersectionTestModule.miCalls == 0 && gaAsserts.empty(),
              "E5 the only candidate is the excluded entity -> {id, 0.0, 0}, no fine call (0x828D4794..0x828D47A8)");
        delete w;
    }
    {
        World* w = new World(); gaAsserts.clear();
        MakeEvent(*w, 0x1Cu, 0x02001405u, 1u, 1u);   // exclude all child parts
        w->mModule.mEntityManager.muKnownId = 0x02001405u; w->mModule.mEntityManager.miKnownIndex = 4;
        w->mModule.mSpatialPartitionManager.mPartition.maScript = { 4, 9 };
        Run(*w);
        const FineIntersectionTestIO::InEventVolumeTestDeepest& q = w->mModule.mFineIntersectionTestModule.mLast;
        Check(w->mModule.mFineIntersectionTestModule.miCalls == 1 && q.mu16ExcludeEntityIndex == 4 && q.mbExcludeParts
              && PostedOnce(*w, 0x10077u, 0.0f, false) && gaAsserts.empty(),
              "E6 exclusion mode 1: the resolved exclude index and exclude parts on (`cntlzw(mode - 1)`); a fine miss "
              "posts depth 0.0 (NOT X360: the console's record there is stack garbage) and hit 0");
        delete w;
    }
    {
        World* w = new World(); gaAsserts.clear();
        MakeEvent(*w, 0x1Cu, 0x03000000u, 0u, 1u);   // an exclude id the table does not know
        w->mModule.mSpatialPartitionManager.mPartition.maScript = { 4 };
        Run(*w);
        const FineIntersectionTestIO::InEventVolumeTestDeepest& q = w->mModule.mFineIntersectionTestModule.mLast;
        Check(AssertedWith("Entity not found (VolumeTestDeepest): ") && w->mModule.mFineIntersectionTestModule.miCalls == 1
              && q.mu16ExcludeEntityIndex == 0xFFFF,
              "E7 an unknown exclude id fires :1626 (0x820F6D90) and the exclude index stays 0xFFFF");
        delete w;
    }
    {
        World* w = new World(); gaAsserts.clear();
        MakeEvent(*w, 0x1Cu, 0xFFFFFFFFu, 0u, 1u);
        w->mModule.mSpatialPartitionManager.mPartition.maScript = { 4 };
        w->mModule.mSpatialPartitionManager.mPartition.miScriptAttempted = 3;   // the buffer overflowed
        Run(*w);
        Check(AssertedWith("lpCoarseResult->miActualNumResults == lpCoarseResult->miNumResultsStored")
              && !AssertedWith("lpInputQuery->mQueryId == lpCoarseResult->mQueryId"),
              "E8 attempted != written fires :1632 (0x820F60E0); the query id check (:1629) holds");
        delete w;
    }

    std::printf("FxFollowupsProcessVolumeTestDeepest: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
