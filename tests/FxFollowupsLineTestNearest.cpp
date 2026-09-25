// FX-FOLLOWUPS (crash parity 2026-09-25, stage a): the fine module's nearest-line test and the line walk under it --
//   FineIntersectionTestModule::ComputeLineTestNearest @0x828C8CC8   (a LOUD trap before)
//   rw::collision::VolumeLineQuery::AddPrimitiveRef     @0x82BB3230   (no body before)
//   rw::collision::VolumeLineQuery::AddVolumeRef        @0x82BB3300   (no body before)
//   rw::collision::VolumeLineQuery::GetIntersections    @0x82BB3470   (an export hole; a `return 0` link stub before)
//   rw::collision::VolumeLineQuery::GetAllIntersections @0x82BB3820   (no body before)
//   and the header inlines InitQuery (volumelinequery.h:297) / Finished (:363)
// run_fxfollowups_line_test_nearest.py compiles the revision's walk region of VolumeQuery.cpp and its
// ComputeLineTestNearest here, against the revision's VolumeQuery.hpp / LineSegIntersect.hpp, the real VolRef.cpp,
// and fakes of the scene manager's entity / volume managers. The rwcollision descriptors are fixture records whose
// lineSegIntersect slot answers from a script, so the walk and the nearest pick are checked against the console's
// words (see the runner's docstring for the list).
#include "types.hpp"
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include "BrnCommonTypes.h"                                        // Vector3 / Matrix44Affine (the vpu vocabulary)
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"         // WriteToLog (the walk's trap announcements)
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"
#include "vendor/renderware/collision/CollisionVolume.hpp"
#include "vendor/renderware/collision/LineSegIntersect.hpp"
#include "vendor/renderware/collision/VolumeQuery.hpp"

static std::vector<std::string> gaAsserts;
static std::vector<std::string> gaAnnouncements;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) { gaAsserts.push_back(lpcMessage ? lpcMessage : ""); return 0; }
void* EndAssert() { return nullptr; }
} }
namespace CgsDev { namespace Log {
void WriteToLog(const char* lpcText) { gaAnnouncements.push_back(lpcText ? lpcText : ""); }
} }

namespace rw { namespace collision {
Volume::VTable* gVolumeVTable[E_VOLUMETYPE_NUMINTERNALTYPES];
#include "fxfu_vlq_walk.inc"   // the revision's line-walk region of VolumeQuery.cpp
} }

using rw::collision::Volume;
typedef rw::collision::VolRef RwVolRef;   // (CgsSceneManager has a VolRef namespace too)
using rw::collision::VolumeLineQuery;
using rw::collision::VolumeLineSegIntersectResult;

// ---- the scripted lineSegIntersect slot -------------------------------------------------------------------------
struct SlotCall
{
    const Volume* mpVolume; const rw::collision::Vec4* mpPt1; const rw::collision::Vec4* mpPt2;
    rw::collision::Vec4 mPt1; rw::collision::Vec4 mPt2; const rw::collision::Vec4* mpTransform;
    rw::collision::Vec4 maRows[4]; VolumeLineSegIntersectResult* mpResult; f32 mfFatness;
};
struct SlotAnswer { bool mbHit; f32 mfLineParam; f32 mfTag; };   // position / normal derive from mfTag
static std::vector<SlotCall> gaCalls;
static std::map<const Volume*, SlotAnswer> gaAnswers;

static rw::collision::RwBool ScriptedLineSegIntersect(const Volume* lpVolume, const rw::collision::Vec4& arPt1,
                                                      const rw::collision::Vec4& arPt2,
                                                      const rw::collision::Vec4* lpTransform,
                                                      VolumeLineSegIntersectResult& arResult, f32 afFatness)
{
    SlotCall lCall = {};
    lCall.mpVolume = lpVolume; lCall.mpPt1 = &arPt1; lCall.mpPt2 = &arPt2; lCall.mPt1 = arPt1; lCall.mPt2 = arPt2;
    lCall.mpTransform = lpTransform; lCall.mpResult = &arResult; lCall.mfFatness = afFatness;
    if (lpTransform != nullptr)
        for (int li = 0; li < 4; ++li) lCall.maRows[li] = lpTransform[li];
    gaCalls.push_back(lCall);
    std::map<const Volume*, SlotAnswer>::const_iterator lIt = gaAnswers.find(lpVolume);
    if (lIt == gaAnswers.end() || !lIt->second.mbHit)
        return 0;
    const f32 lfTag = lIt->second.mfTag;
    arResult.lineParam = lIt->second.mfLineParam;
    arResult.position.x = lfTag; arResult.position.y = lfTag + 1.0f; arResult.position.z = lfTag + 2.0f;
    arResult.position.w = lfTag + 3.0f;
    arResult.normal.x = -lfTag; arResult.normal.y = -lfTag - 1.0f; arResult.normal.z = -lfTag - 2.0f;
    arResult.normal.w = -lfTag - 3.0f;
    return 1;
}

static Volume::VTable gTriangleDesc;    // type 3, scripted slot
static Volume::VTable gBoxDesc;         // type 4, NO slot (the host's parked BOX)
static Volume::VTable gAggregateDesc;   // type 6, slot 0 (as in the image)

static void InstallDescriptors()
{
    std::memset(&gTriangleDesc, 0, sizeof(gTriangleDesc));
    std::memset(&gBoxDesc, 0, sizeof(gBoxDesc));
    std::memset(&gAggregateDesc, 0, sizeof(gAggregateDesc));
    gTriangleDesc.muTypeID = 3;  gTriangleDesc.mpfnLineSegIntersect = ScriptedLineSegIntersect;
    gBoxDesc.muTypeID = 4;
    gAggregateDesc.muTypeID = 6;
    for (int li = 0; li < rw::collision::E_VOLUMETYPE_NUMINTERNALTYPES; ++li) rw::collision::gVolumeVTable[li] = nullptr;
    rw::collision::gVolumeVTable[3] = &gTriangleDesc;
    rw::collision::gVolumeVTable[4] = &gBoxDesc;
    rw::collision::gVolumeVTable[6] = &gAggregateDesc;
}

static void MakeVolume(Volume& arVolume, u32 luType, bool lbEnabled, u32 luGroup = 0, u32 luSurface = 0)
{
    std::memset(&arVolume, 0, sizeof(arVolume));
    arVolume.muVTableSlot = luType;
    arVolume.muFlags      = lbEnabled ? 1u : 0u;
    arVolume.muGroupID    = luGroup;
    arVolume.muSurfaceID  = luSurface;
}

// ---- the scene manager fakes ComputeLineTestNearest reads ---------------------------------------------------------
namespace CgsSceneManager
{
    namespace VolRef { struct Volume; }
    struct SceneQueryId { u32 mId; };

    struct VolumeInstance
    {
        Matrix44Affine mWorldSpaceTransform;
        s32            miVolumeIndex;
        s32            miNextEntityVolumeInstance;
    };

    struct VolumeManagerVolume { typedef u8 VolumeTypeFlags; VolumeTypeFlags mxFlags; };

    class VolumeManager
    {
    public:
        VolumeManagerVolume::VolumeTypeFlags GetVolumeTypeFlags(s32 liVolumeIndex) const { return mau8Flags[liVolumeIndex]; }
        const VolRef::Volume* GetRwVolume(s32 liVolumeIndex) const
        {
            return reinterpret_cast<const VolRef::Volume*>(mapRwVolumes[liVolumeIndex]);
        }
        u8                           mau8Flags[16];
        const rw::collision::Volume* mapRwVolumes[16];
    };

    class EntityManager
    {
    public:
        EntityId GetEntityIdByIndex(u16 lu16Index) const { return EntityId(mauIds[lu16Index]); }
        const VolumeInstance* GetFirstEntityVolumeInstance(u16 lu16EntityIndex, s32* lpiFirst) const
        {
            if (lpiFirst != nullptr) *lpiFirst = maiFirst[lu16EntityIndex];
            return GetVolumeInstance(maiFirst[lu16EntityIndex]);
        }
        const VolumeInstance* GetVolumeInstance(s32 liIndex) const
        {
            ++miConstWalks;
            return (liIndex >= 0 && liIndex < 16) ? &maInstances[liIndex] : nullptr;
        }
        VolumeInstance* GetVolumeInstance(s32 liIndex)
        {
            ++miNonConstWalks;
            return (liIndex >= 0 && liIndex < 16) ? &maInstances[liIndex] : nullptr;
        }
        u32            mauIds[16];
        s32            maiFirst[16];
        VolumeInstance maInstances[16];
        mutable int    miConstWalks;
        int            miNonConstWalks;
    };

    namespace FineIntersectionTestIO
    {
        struct alignas(16) InEventLineTestNearest
        {
            Vector3      mLineStart;
            Vector3      mLineEnd;
            SceneQueryId mQueryId;
            const u16*   mpau16EntityIndices;
            u16          mu16NumEntities;
            u16          mu16ExcludeEntityIndex;
            u8           mxVolumeTypeFlags;
            bool         mbExcludeParts;
        };
        struct alignas(16) OutEventLineTestNearestResult
        {
            SceneQueryId mQueryId;
            u32          muVolumeInstanceIndex;
            Vector3      mPosition;
            Vector3      mNormal;
            f32          mfLineParam;
            u16          mu16EntityIndex;
            u16          mu16MaterialTag;
            u16          mu16GroupTag;
            bool         mbIntersection;
        };
    }

    struct FineIntersectionTestModule
    {
        typedef FineIntersectionTestIO::InEventLineTestNearest        InEventLineTestNearest;
        typedef FineIntersectionTestIO::OutEventLineTestNearestResult OutEventLineTestNearestResult;
        void ComputeLineTestNearest(const InEventLineTestNearest* lpQuery, OutEventLineTestNearestResult* lpOutResult);
        VolumeManager*                  mpVolumeManager;
        EntityManager*                  mpEntityManager;
        rw::collision::VolumeLineQuery* mpVolumeLineQuery;
    };

#include "fxfu_cltn_body.inc"   // FineIntersectionTestModule::ComputeLineTestNearest from the revision
}

using namespace CgsSceneManager;

static unsigned guChecks = 0, guFailures = 0;
static void Check(bool lbPass, const char* lpcLabel)
{
    ++guChecks;
    if (!lbPass) ++guFailures;
    std::printf("%s  %s\n", lbPass ? "PASS" : "FAIL", lpcLabel);
}

static bool SameBits(f32 a, f32 b) { u32 x, y; std::memcpy(&x, &a, 4); std::memcpy(&y, &b, 4); return x == y; }
static bool SameVec(const rw::collision::Vec4& a, f32 x, f32 y, f32 z, f32 w)
{
    return SameBits(a.x, x) && SameBits(a.y, y) && SameBits(a.z, z) && SameBits(a.w, w);
}
static bool SameRow(const RwVolRef::Vec4& a, const rw::collision::Vec4& b)
{
    return SameBits(a.x, b.x) && SameBits(a.y, b.y) && SameBits(a.z, b.z) && SameBits(a.w, b.w);
}

// ---- one line query over fixture buffers (what Initialize carves) --------------------------------------------------
static const u32 KU_CAP = 8;
static RwVolRef                        gaStack[KU_CAP];
static RwVolRef                        gaPrims[KU_CAP];
static VolumeLineSegIntersectResult  gaResults[KU_CAP];
static VolumeLineQuery               gQuery;

static void ResetQuery(u32 luStackMax, u32 luPrimMax, u32 luResMax)
{
    std::memset(&gQuery, 0, sizeof(gQuery));
    std::memset(gaStack, 0xAB, sizeof(gaStack));
    std::memset(gaPrims, 0xAB, sizeof(gaPrims));
    std::memset(gaResults, 0xAB, sizeof(gaResults));
    gQuery.m_stackVRefBuffer = gaStack;  gQuery.m_stackMax       = luStackMax;
    gQuery.m_primVRefBuffer  = gaPrims;  gQuery.m_primBufferSize = luPrimMax;
    gQuery.m_resBuffer       = gaResults; gQuery.m_resBufferSize = luResMax;
    gaCalls.clear(); gaAnswers.clear(); gaAsserts.clear(); gaAnnouncements.clear();
}

static Matrix44Affine MakeMatrix(f32 lfBase)
{
    Matrix44Affine lM;
    lM.xAxis = { lfBase + 0.25f, lfBase + 0.5f, lfBase + 0.75f, 0.0f };
    lM.yAxis = { lfBase + 1.25f, lfBase + 1.5f, lfBase + 1.75f, 0.0f };
    lM.zAxis = { lfBase + 2.25f, lfBase + 2.5f, lfBase + 2.75f, 0.0f };
    lM.wAxis = { lfBase + 3.25f, lfBase + 3.5f, lfBase + 3.75f, 1.0f };
    return lM;
}
static const rw::collision::Vec4* Rows(const Matrix44Affine& arM) { return reinterpret_cast<const rw::collision::Vec4*>(&arM); }

static void TestWalk()
{
    InstallDescriptors();
    Volume laVol[6];
    const Matrix44Affine lM0 = MakeMatrix(10.0f), lM1 = MakeMatrix(20.0f);

    // W1 AddPrimitiveRef: fill, tm copy aimed at the entry's own rows, full buffer answers 0 and writes nothing.
    {
        ResetQuery(2, 2, 2);
        MakeVolume(laVol[0], 3, true);
        const s32 liA = gQuery.AddPrimitiveRef(&laVol[0], reinterpret_cast<const rw::math::vpu::Matrix44Affine*>(&lM0), 0x1234u, 7);
        const s32 liB = gQuery.AddPrimitiveRef(&laVol[0], nullptr, 5u, 2);
        RwVolRef lUntouched; std::memset(&lUntouched, 0xAB, sizeof(lUntouched));
        const u32 luPrimBefore = gQuery.m_primNext;
        const s32 liC = gQuery.AddPrimitiveRef(&laVol[0], nullptr, 9u, 9);
        const bool lbEntry0 = gaPrims[0].muVolumePtr == reinterpret_cast<uintptr_t>(&laVol[0]) &&
                              gaPrims[0].muTransformPtr == reinterpret_cast<uintptr_t>(&gaPrims[0].mRow0) &&
                              SameRow(gaPrims[0].mRow0, Rows(lM0)[0]) && SameRow(gaPrims[0].mRow3, Rows(lM0)[3]) &&
                              gaPrims[0].muTag == 0x1234u && gaPrims[0].muNumTagBits == 7;
        const bool lbEntry1 = gaPrims[1].muTransformPtr == 0 && gaPrims[1].muTag == 5u && gaPrims[1].muNumTagBits == 2;
        Check(liA == 1 && liB == 1 && liC == 0 && luPrimBefore == 2 && gQuery.m_primNext == 2 && lbEntry0 && lbEntry1 &&
              std::memcmp(&gaPrims[2], &lUntouched, sizeof(RwVolRef)) == 0,
              "W1 AddPrimitiveRef @0x82BB3230: volume, the four rows copied with the transform word aimed at them (0 without), "
              "tag, bit count; full (primNext >= primBufferSize) -> 0, nothing written");
    }
    // W2 AddVolumeRef: a primitive tail-calls AddPrimitiveRef; an aggregate goes on the stack; a full stack -> 0.
    {
        ResetQuery(1, 4, 2);
        MakeVolume(laVol[0], 3, true);
        MakeVolume(laVol[1], 6, true);
        const s32 liPrim = gQuery.AddVolumeRef(&laVol[0], nullptr, 0, 0);
        const s32 liAgg  = gQuery.AddVolumeRef(&laVol[1], reinterpret_cast<const rw::math::vpu::Matrix44Affine*>(&lM1), 3u, 4);
        const s32 liFull = gQuery.AddVolumeRef(&laVol[1], nullptr, 0, 0);
        Check(liPrim == 1 && liAgg == 1 && liFull == 0 && gQuery.m_primNext == 1 && gQuery.m_stackNext == 1 &&
              gaPrims[0].muVolumePtr == reinterpret_cast<uintptr_t>(&laVol[0]) &&
              gaStack[0].muVolumePtr == reinterpret_cast<uintptr_t>(&laVol[1]) &&
              gaStack[0].muTransformPtr == reinterpret_cast<uintptr_t>(&gaStack[0].mRow0) &&
              SameRow(gaStack[0].mRow2, Rows(lM1)[2]) && gaStack[0].muTag == 3u && gaStack[0].muNumTagBits == 4,
              "W2 AddVolumeRef @0x82BB3300: non-aggregate -> AddPrimitiveRef; aggregate (type 6) -> the stack (stackMax gate)");
    }
    // W3/W4 GetAllIntersections over three inputs (one disabled): staging, LIFO test order, the result record.
    {
        ResetQuery(4, 4, 4);
        MakeVolume(laVol[0], 3, true);
        MakeVolume(laVol[1], 3, true);
        MakeVolume(laVol[2], 3, false);
        gaAnswers[&laVol[0]] = { true, 0.75f, 100.0f };
        gaAnswers[&laVol[1]] = { true, 0.25f, 200.0f };
        const Volume* lapInputs[3] = { &laVol[0], &laVol[1], &laVol[2] };
        const Matrix44Affine lM2 = MakeMatrix(30.0f);
        const rw::math::vpu::Matrix44Affine* lapMats[3] = { &lM0, &lM1, &lM2 };
        const RwVolRef::Vec4 lP1 = { 1.0f, 2.0f, 3.0f, 4.0f }, lP2 = { 5.0f, 6.0f, 7.0f, 8.0f };
        gQuery.InitQuery(lapInputs, lapMats, 3, lP1, lP2, 0.125f);
        gQuery.m_resultsSet = VolumeLineQuery::NEARESTLINEINTERSECTION;   // GetAllIntersections resets it
        gQuery.m_resMax = 1;                                             // ... and this
        gQuery.m_tag = 77; gQuery.m_numTagBits = 5; gQuery.m_instVolCount = 9;
        const u32 luCount = gQuery.GetAllIntersections();
        Check(luCount == 2 && gQuery.m_resCount == 2 && gQuery.m_resultsSet == VolumeLineQuery::ALLLINEINTERSECTIONS &&
              gQuery.m_resMax == 4 && gQuery.m_tag == 0 && gQuery.m_numTagBits == 0 && gQuery.m_instVolCount == 0,
              "W3 GetAllIntersections @0x82BB3820: results-set 0, m_resMax = m_resBufferSize; the walk zeroes resCount, "
              "instVolCount, tag, tag bits; two hits counted");
        Check(gaCalls.size() == 2 && gaCalls[0].mpVolume == &laVol[1] && gaCalls[1].mpVolume == &laVol[0],
              "W4a the disabled input is never tested; the staged primitives are tested LAST STAGED FIRST (--m_primNext)");
        Check(gaCalls.size() == 2 && gaCalls[0].mpPt1 == reinterpret_cast<const rw::collision::Vec4*>(&gQuery.m_pt1) &&
              gaCalls[0].mpPt2 == reinterpret_cast<const rw::collision::Vec4*>(&gQuery.m_pt2) &&
              SameVec(gaCalls[0].mPt1, 1.0f, 2.0f, 3.0f, 4.0f) && SameVec(gaCalls[0].mPt2, 5.0f, 6.0f, 7.0f, 8.0f) &&
              SameBits(gaCalls[0].mfFatness, 0.125f) &&
              gaCalls[0].mpTransform == reinterpret_cast<const rw::collision::Vec4*>(&gaPrims[1].mRow0) &&
              SameRow(gaPrims[1].mRow1, Rows(lM1)[1]) && gaCalls[0].mpResult == &gaResults[0] &&
              gaCalls[1].mpResult == &gaResults[1],
              "W4b the slot gets (volume, &m_pt1, &m_pt2, the staged entry's own transform copy, m_resBuffer[m_resCount], "
              "m_fatness)");
        const VolumeLineSegIntersectResult& lr0 = gaResults[0];
        Check(lr0.vRef.muVolumePtr == reinterpret_cast<uintptr_t>(&laVol[1]) &&
              lr0.v == reinterpret_cast<uintptr_t>(&laVol[2]) &&
              lr0.vRef.muTransformPtr == reinterpret_cast<uintptr_t>(&lr0.vRef.mRow0) &&
              SameRow(lr0.vRef.mRow0, Rows(lM1)[0]) && SameRow(lr0.vRef.mRow3, Rows(lM1)[3]) &&
              lr0.vRef.muTag == 0u && lr0.vRef.muNumTagBits == 0xAB && SameBits(lr0.lineParam, 0.25f),
              "W4c the result: vRef volume, v = m_inputVols[m_currInput - 1] (the LAST input once all are staged), "
              "the rows copied and +0x54 aimed at them, the entry's tag; the tag-bit byte +0xC4 is NOT written");
        Check(SameBits(gQuery.m_endClipVal, 1.0f) && gQuery.Finished() == 1 && gQuery.m_currInput == 3,
              "W4d with results-set 0 m_endClipVal stays 1.0 (InitQuery, flt_82001C98); the query is Finished");
    }
    // W5 no input matrices -> no transform word anywhere.
    {
        ResetQuery(4, 4, 4);
        MakeVolume(laVol[0], 3, true);
        gaAnswers[&laVol[0]] = { true, 0.5f, 1.0f };
        const Volume* lapInputs[1] = { &laVol[0] };
        const RwVolRef::Vec4 lP = { 0.0f, 0.0f, 0.0f, 0.0f };
        gQuery.InitQuery(lapInputs, nullptr, 1, lP, lP, 0.0f);
        const u32 luCount = gQuery.GetAllIntersections();
        Check(luCount == 1 && gaCalls.size() == 1 && gaCalls[0].mpTransform == nullptr && gaResults[0].vRef.muTransformPtr == 0,
              "W5 m_inputMats == 0: the staged entry and the result carry a zero transform word");
    }
    // W6 a full result buffer returns and the next call resumes.
    {
        ResetQuery(4, 4, 1);
        MakeVolume(laVol[0], 3, true);
        MakeVolume(laVol[1], 3, true);
        gaAnswers[&laVol[0]] = { true, 0.5f, 1.0f };
        gaAnswers[&laVol[1]] = { true, 0.6f, 2.0f };
        const Volume* lapInputs[2] = { &laVol[0], &laVol[1] };
        const RwVolRef::Vec4 lP = { 0.0f, 0.0f, 0.0f, 0.0f };
        gQuery.InitQuery(lapInputs, nullptr, 2, lP, lP, 0.0f);
        const u32 luFirst = gQuery.GetAllIntersections();
        const s32 liFinishedAfterFirst = gQuery.Finished();
        const bool lbFirstIsLast = gaResults[0].vRef.muVolumePtr == reinterpret_cast<uintptr_t>(&laVol[1]);
        const u32 luSecond = gQuery.GetAllIntersections();
        const bool lbSecondIsFirst = gaResults[0].vRef.muVolumePtr == reinterpret_cast<uintptr_t>(&laVol[0]);
        Check(luFirst == 1 && liFinishedAfterFirst == 0 && lbFirstIsLast && luSecond == 1 && lbSecondIsFirst &&
              gQuery.Finished() == 1 && gaCalls.size() == 2,
              "W6 m_resMax 1: the walk returns with one result and the rest staged (not Finished); the next call resumes");
    }
    // W7 a nearest-style results set clips m_endClipVal to a smaller lineParam; a NaN never clips.
    {
        ResetQuery(4, 4, 4);
        MakeVolume(laVol[0], 3, true);
        MakeVolume(laVol[1], 3, true);
        MakeVolume(laVol[2], 3, true);
        gaAnswers[&laVol[0]] = { true, 0.4f, 1.0f };
        gaAnswers[&laVol[1]] = { true, std::numeric_limits<f32>::quiet_NaN(), 2.0f };
        gaAnswers[&laVol[2]] = { true, 0.7f, 3.0f };
        const Volume* lapInputs[3] = { &laVol[0], &laVol[1], &laVol[2] };
        const RwVolRef::Vec4 lP = { 0.0f, 0.0f, 0.0f, 0.0f };
        gQuery.InitQuery(lapInputs, nullptr, 3, lP, lP, 0.0f);
        gQuery.m_resultsSet = VolumeLineQuery::NEARESTLINEINTERSECTION;
        const u32 luCount = gQuery.GetIntersections();
        Check(luCount == 3 && SameBits(gQuery.m_endClipVal, 0.4f),
              "W7 results-set != 0: m_endClipVal = lineParam when below it (0.7 then NaN then 0.4 -> 0.4)");
    }
    // W8 a primitive whose descriptor has no lineSegIntersect body: announced once, asserted, not tested; the walk goes on.
    {
        ResetQuery(4, 4, 4);
        MakeVolume(laVol[0], 4, true);   // BOX -- no host slot
        MakeVolume(laVol[1], 3, true);
        MakeVolume(laVol[2], 4, true);
        gaAnswers[&laVol[1]] = { true, 0.5f, 1.0f };
        const Volume* lapInputs[3] = { &laVol[0], &laVol[1], &laVol[2] };
        const RwVolRef::Vec4 lP = { 0.0f, 0.0f, 0.0f, 0.0f };
        gQuery.InitQuery(lapInputs, nullptr, 3, lP, lP, 0.0f);
        const u32 luCount = gQuery.GetAllIntersections();
        size_t luBoxAnnouncements = 0;
        for (size_t li = 0; li < gaAnnouncements.size(); ++li)
            if (gaAnnouncements[li].find("[vlq] TRAP") != std::string::npos &&
                gaAnnouncements[li].find("type 4") != std::string::npos) ++luBoxAnnouncements;
        Check(luCount == 1 && gaCalls.size() == 1 && gaCalls[0].mpVolume == &laVol[1] && gaAsserts.size() == 2 &&
              luBoxAnnouncements <= 1 && gQuery.Finished() == 1,
              "W8 [PC TRAP] a staged BOX (no lineSegIntersect body) asserts each time, is NOT tested, the walk continues");
    }
    // W9 an aggregate input: stacked, popped, trapped, spent -- the walk ends.
    {
        ResetQuery(4, 4, 4);
        MakeVolume(laVol[0], 6, true);
        const Volume* lapInputs[1] = { &laVol[0] };
        const RwVolRef::Vec4 lP = { 0.0f, 0.0f, 0.0f, 0.0f };
        gQuery.InitQuery(lapInputs, nullptr, 1, lP, lP, 0.0f);
        gQuery.m_aggIndex = 5;
        gQuery.m_curSpatialMapQuery = &gQuery;
        const u32 luCount = gQuery.GetAllIntersections();
        bool lbAnnounced = false;
        for (size_t li = 0; li < gaAnnouncements.size(); ++li)
            if (gaAnnouncements[li].find("AGGREGATE") != std::string::npos) lbAnnounced = true;
        Check(luCount == 0 && gaAsserts.size() == 1 && gQuery.m_currVRef.muVolumePtr == 0 && gQuery.m_stackNext == 0 &&
              gQuery.m_aggIndex == 0 && gQuery.m_curSpatialMapQuery == nullptr && gQuery.Finished() == 1 &&
              (lbAnnounced || gaAnnouncements.empty()),
              "W9 [PC TRAP] an aggregate is stacked, popped into m_currVRef, asserted and SPENT (aggIndex / spatial-map "
              "query 0) -- no hang, no silent test");
    }
    // W10 the console's input path ignores AddVolumeRef's answer: a full primitive buffer drops the input.
    {
        ResetQuery(4, 1, 4);
        MakeVolume(laVol[0], 3, true);
        MakeVolume(laVol[1], 3, true);
        gaAnswers[&laVol[0]] = { true, 0.5f, 1.0f };
        gaAnswers[&laVol[1]] = { true, 0.6f, 2.0f };
        const Volume* lapInputs[2] = { &laVol[0], &laVol[1] };
        const RwVolRef::Vec4 lP = { 0.0f, 0.0f, 0.0f, 0.0f };
        gQuery.InitQuery(lapInputs, nullptr, 2, lP, lP, 0.0f);
        const u32 luCount = gQuery.GetAllIntersections();
        Check(luCount == 1 && gaCalls.size() == 1 && gaCalls[0].mpVolume == &laVol[0] && gQuery.m_currInput == 2 &&
              gQuery.Finished() == 1,
              "W10 primBufferSize 1, two inputs: the second input's AddVolumeRef fails and m_currInput still advances "
              "(the result at 0x82BB3584 is never read) -- it is never tested");
    }
    // W11 a disabled input is skipped BEFORE staging (0x82BB3544): it takes no primitive slot.
    {
        ResetQuery(4, 1, 4);
        MakeVolume(laVol[0], 3, false);
        MakeVolume(laVol[1], 3, true);
        gaAnswers[&laVol[1]] = { true, 0.5f, 1.0f };
        const Volume* lapInputs[2] = { &laVol[0], &laVol[1] };
        const RwVolRef::Vec4 lP = { 0.0f, 0.0f, 0.0f, 0.0f };
        gQuery.InitQuery(lapInputs, nullptr, 2, lP, lP, 0.0f);
        const u32 luCount = gQuery.GetAllIntersections();
        Check(luCount == 1 && gaCalls.size() == 1 && gaCalls[0].mpVolume == &laVol[1],
              "W11 a disabled input (m_flags bit 0 clear) only advances m_currInput -- it takes no primitive slot");
    }
}

// ---- ComputeLineTestNearest ------------------------------------------------------------------------------------------
static EntityManager              gEntities;
static VolumeManager              gVolumes;
static FineIntersectionTestModule gModule;
static rw::collision::Volume      gaRw[8];

static void ResetScene()
{
    std::memset(&gEntities, 0, sizeof(gEntities));
    std::memset(&gVolumes, 0, sizeof(gVolumes));
    for (int li = 0; li < 16; ++li) { gEntities.maiFirst[li] = -1; gEntities.maInstances[li].miNextEntityVolumeInstance = -1; }
    gModule.mpVolumeManager = &gVolumes;
    gModule.mpEntityManager = &gEntities;
    gModule.mpVolumeLineQuery = &gQuery;
    ResetQuery(4, 4, 4);
}

// entity e with instances (instance index, volume index) chained in order.
static void AddEntity(u16 lu16Entity, u32 luId, const s32* laiInstances, const s32* laiVolumes, int liCount)
{
    gEntities.mauIds[lu16Entity] = luId;
    gEntities.maiFirst[lu16Entity] = liCount > 0 ? laiInstances[0] : -1;
    for (int li = 0; li < liCount; ++li)
    {
        VolumeInstance& lr = gEntities.maInstances[laiInstances[li]];
        lr.mWorldSpaceTransform = MakeMatrix(static_cast<f32>(laiInstances[li]) * 100.0f);
        lr.miVolumeIndex = laiVolumes[li];
        lr.miNextEntityVolumeInstance = (li + 1 < liCount) ? laiInstances[li + 1] : -1;
    }
}

static FineIntersectionTestIO::OutEventLineTestNearestResult Poisoned()
{
    FineIntersectionTestIO::OutEventLineTestNearestResult lOut;
    std::memset(&lOut, 0xCD, sizeof(lOut));
    return lOut;
}

static FineIntersectionTestIO::InEventLineTestNearest MakeQuery(const u16* lpau16, u16 lu16Count, u16 lu16Exclude,
                                                                u8 lu8Flags, bool lbParts)
{
    FineIntersectionTestIO::InEventLineTestNearest lQuery;
    std::memset(&lQuery, 0, sizeof(lQuery));
    lQuery.mLineStart = { 1.0f, 2.0f, 3.0f, 9.0f };
    lQuery.mLineEnd   = { 4.0f, 5.0f, 6.0f, 10.0f };
    lQuery.mQueryId.mId = 0x5150u;
    lQuery.mpau16EntityIndices = lpau16;
    lQuery.mu16NumEntities = lu16Count;
    lQuery.mu16ExcludeEntityIndex = lu16Exclude;
    lQuery.mxVolumeTypeFlags = lu8Flags;
    lQuery.mbExcludeParts = lbParts;
    return lQuery;
}

static void TestNearest()
{
    InstallDescriptors();

    // C1 no candidates: the up-front writes only.
    {
        ResetScene();
        const FineIntersectionTestIO::InEventLineTestNearest lQuery = MakeQuery(nullptr, 0, 0xFFFF, 0xFF, false);
        FineIntersectionTestIO::OutEventLineTestNearestResult lOut = Poisoned();
        gModule.ComputeLineTestNearest(&lQuery, &lOut);
        Check(lOut.mbIntersection == false && lOut.mQueryId.mId == 0x5150u && lOut.mu16MaterialTag == 0 &&
              lOut.mu16GroupTag == 0 && lOut.muVolumeInstanceIndex == 0xCDCDCDCDu && lOut.mu16EntityIndex == 0xCDCD,
              "C1 up front: +0x3A = 0, +0x36 / +0x38 = 0, +0x00 = the query id; nothing else written without a hit");
    }
    // C2..C6 one scene: three candidates, four instances.
    //   entity 0 (id 0x100): instance 1 -> volume 1 (hit 0.8)
    //   entity 1 (id 0x200): instance 2 -> volume 2 (hit 0.3), instance 3 -> volume 3 (hit 0.5)
    //   entity 2 (id 0x300): instance 4 -> volume 4 (hit 0.1, but its volume-type flags miss the query's)
    ResetScene();
    const s32 laiI0[1] = { 1 }, laiV0[1] = { 1 };
    const s32 laiI1[2] = { 2, 3 }, laiV1[2] = { 2, 3 };
    const s32 laiI2[1] = { 4 }, laiV2[1] = { 4 };
    AddEntity(0, 0x1000u, laiI0, laiV0, 1);
    AddEntity(1, 0x2000u, laiI1, laiV1, 2);
    AddEntity(2, 0x3000u, laiI2, laiV2, 1);
    for (int li = 1; li <= 4; ++li)
    {
        MakeVolume(gaRw[li], 3, true, 0x10000u + static_cast<u32>(li), 0x20000u + static_cast<u32>(li) * 0x11u);
        gVolumes.mapRwVolumes[li] = &gaRw[li];
        gVolumes.mau8Flags[li] = (li == 4) ? 0x02 : 0x05;
    }
    gaAnswers[&gaRw[1]] = { true, 0.8f, 10.0f };
    gaAnswers[&gaRw[2]] = { true, 0.3f, 20.0f };
    gaAnswers[&gaRw[3]] = { true, 0.5f, 30.0f };
    gaAnswers[&gaRw[4]] = { true, 0.1f, 40.0f };
    const u16 lau16All[3] = { 0, 1, 2 };
    {
        gaCalls.clear();
        const FineIntersectionTestIO::InEventLineTestNearest lQuery = MakeQuery(lau16All, 3, 0xFFFF, 0x04, false);
        FineIntersectionTestIO::OutEventLineTestNearestResult lOut = Poisoned();
        gModule.ComputeLineTestNearest(&lQuery, &lOut);
        Check(lOut.mbIntersection && lOut.mu16EntityIndex == 1 && lOut.muVolumeInstanceIndex == 2 &&
              SameBits(lOut.mfLineParam, 0.3f),
              "C2 the nearest over instances and entities wins (0.8, 0.3, 0.5 -> 0.3: entity 1, instance 2); the "
              "type-flag miss (entity 2, 0.1) is never tested");
        Check(lOut.mPosition.x == 20.0f && lOut.mPosition.y == 21.0f && lOut.mPosition.z == 22.0f &&
              lOut.mPosition.w == 23.0f && lOut.mNormal.x == -20.0f && lOut.mNormal.w == -23.0f,
              "C3 +0x10 / +0x20: the winning result's position and normal, all four lanes");
        Check(lOut.mu16MaterialTag == static_cast<u16>(0x20000u + 2 * 0x11u) &&
              lOut.mu16GroupTag == static_cast<u16>(0x10000u + 2),
              "C4 +0x36 = the hit volume's surfaceID (+0x58), +0x38 = its groupID (+0x54), low halves");
        bool lbNoFour = true;
        for (size_t li = 0; li < gaCalls.size(); ++li) if (gaCalls[li].mpVolume == &gaRw[4]) lbNoFour = false;
        const bool lbPrimed = gaCalls.size() == 3 && SameVec(gaCalls[0].mPt1, 1.0f, 2.0f, 3.0f, 9.0f) &&
                              SameVec(gaCalls[0].mPt2, 4.0f, 5.0f, 6.0f, 10.0f) && SameBits(gaCalls[0].mfFatness, 0.0f) &&
                              gaCalls[0].mpVolume == &gaRw[1];
        const Matrix44Affine lInst1 = MakeMatrix(100.0f);
        Check(lbNoFour && lbPrimed && SameVec(gaCalls[0].maRows[0], Rows(lInst1)[0].x, Rows(lInst1)[0].y,
                                              Rows(lInst1)[0].z, Rows(lInst1)[0].w) &&
              gEntities.miNonConstWalks == 0 && gEntities.miConstWalks > 0,
              "C5 InitQuery: the one input volume GetRwVolume(index), the one matrix the instance's transform, the "
              "segment's four lanes each, fatness 0.0 (flt_82001CC0); instances walked through the CONST overload");
    }
    // C6 the exclude rule is an EQUALITY under the mask: a bit-superset id is NOT excluded.
    {
        gEntities.mauIds[3] = 0x2000u;   // the excluded entity carries entity 1's id
        gEntities.mauIds[0] = 0x3000u;   // 0x3000 is a bit-superset of 0x2000
        gaCalls.clear();
        const FineIntersectionTestIO::InEventLineTestNearest lQuery = MakeQuery(lau16All, 2, 3, 0x04, false);
        FineIntersectionTestIO::OutEventLineTestNearestResult lOut = Poisoned();
        gModule.ComputeLineTestNearest(&lQuery, &lOut);
        Check(lOut.mbIntersection && lOut.mu16EntityIndex == 0 && SameBits(lOut.mfLineParam, 0.8f) && gaCalls.size() == 1,
              "C6 exclude (mask ~0): only an id EQUAL to the excluded one is skipped (0x2000 skipped, superset 0x3000 kept)");
        gEntities.mauIds[0] = 0x1000u;
    }
    // C7 parts: the mask is 0xFFFFFC00, ids equal above bit 10 are skipped.
    {
        gEntities.mauIds[3] = 0x2000u | 0x155u;   // entity 1's entity bits, other part bits
        gaCalls.clear();
        const FineIntersectionTestIO::InEventLineTestNearest lQuery = MakeQuery(lau16All, 2, 3, 0x04, true);
        FineIntersectionTestIO::OutEventLineTestNearestResult lOut = Poisoned();
        gModule.ComputeLineTestNearest(&lQuery, &lOut);
        Check(lOut.mbIntersection && lOut.mu16EntityIndex == 0 && gaCalls.size() == 1,
              "C7 exclude with parts (mask 0xFFFFFC00): entity 1 (0x2000) equals 0x2155 under the mask and is skipped");
    }
    // C8 no exclude index: only an id equal to 0xFFFFFFFF (dword_82F33F64) would be skipped.
    {
        gEntities.mauIds[1] = 0xFFFFFFFFu;
        gaCalls.clear();
        const FineIntersectionTestIO::InEventLineTestNearest lQuery = MakeQuery(lau16All, 2, 0xFFFF, 0x04, false);
        FineIntersectionTestIO::OutEventLineTestNearestResult lOut = Poisoned();
        gModule.ComputeLineTestNearest(&lQuery, &lOut);
        Check(lOut.mbIntersection && lOut.mu16EntityIndex == 0 && gaCalls.size() == 1,
              "C8 no exclude: exclude = 0xFFFFFFFF, mask -1 -- an entity whose id is 0xFFFFFFFF is skipped");
        gEntities.mauIds[1] = 0x2000u;
    }
    // C9 strict less and NaN: a tie never replaces, a NaN never wins, and the nearest carries across entities.
    {
        gaAnswers[&gaRw[1]] = { true, 0.3f, 10.0f };
        gaAnswers[&gaRw[2]] = { true, std::numeric_limits<f32>::quiet_NaN(), 20.0f };
        gaAnswers[&gaRw[3]] = { true, 0.3f, 30.0f };
        gaCalls.clear();
        const FineIntersectionTestIO::InEventLineTestNearest lQuery = MakeQuery(lau16All, 2, 0xFFFF, 0x04, false);
        FineIntersectionTestIO::OutEventLineTestNearestResult lOut = Poisoned();
        gModule.ComputeLineTestNearest(&lQuery, &lOut);
        Check(lOut.mbIntersection && lOut.mu16EntityIndex == 0 && lOut.muVolumeInstanceIndex == 1 &&
              lOut.mPosition.x == 10.0f && gaCalls.size() == 3,
              "C9 `fcmpu ; bge`: the first 0.3 stands against a later equal 0.3 and a NaN (FLT_MAX start, never reset)");
        gaAnswers[&gaRw[2]] = { true, 0.3f, 20.0f };
    }
    // C10 misses everywhere leave only the up-front writes.
    {
        gaAnswers.clear();
        const FineIntersectionTestIO::InEventLineTestNearest lQuery = MakeQuery(lau16All, 3, 0xFFFF, 0x07, false);
        FineIntersectionTestIO::OutEventLineTestNearestResult lOut = Poisoned();
        gModule.ComputeLineTestNearest(&lQuery, &lOut);
        Check(!lOut.mbIntersection && lOut.mu16EntityIndex == 0xCDCD && lOut.muVolumeInstanceIndex == 0xCDCDCDCDu &&
              lOut.mu16MaterialTag == 0 && lOut.mu16GroupTag == 0,
              "C10 every volume tested, none hit: mbIntersection 0 and the rest of the record untouched");
    }
    // C11 a BOX instance volume (no host slot) traps inside the walk; the other instances still answer.
    {
        MakeVolume(gaRw[2], 4, true, 0x10002u, 0x20022u);
        gaAnswers[&gaRw[1]] = { true, 0.9f, 10.0f };
        gaAnswers[&gaRw[3]] = { true, 0.6f, 30.0f };
        gaAsserts.clear();
        const FineIntersectionTestIO::InEventLineTestNearest lQuery = MakeQuery(lau16All, 2, 0xFFFF, 0x04, false);
        FineIntersectionTestIO::OutEventLineTestNearestResult lOut = Poisoned();
        gModule.ComputeLineTestNearest(&lQuery, &lOut);
        Check(lOut.mbIntersection && lOut.mu16EntityIndex == 1 && lOut.muVolumeInstanceIndex == 3 &&
              SameBits(lOut.mfLineParam, 0.6f) && gaAsserts.size() == 1,
              "C11 [PC TRAP] a BOX volume asserts in the walk and is not tested; instance 3 (0.6) wins over 0.9");
    }
}

int main()
{
    TestWalk();
    TestNearest();
    std::printf("FxFollowupsLineTestNearest: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
