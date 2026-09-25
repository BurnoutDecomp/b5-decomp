// FX-FOLLOWUPS (crash parity 2026-09-25, item 2 step E1): the fine module's deepest-volume test --
//   FineIntersectionTestModule::ComputeVolumeTestDeepest @0x828C90D0 (a LOUD trap before)
//   VolumeManager::GetVolumeTypeFlags (DWARF CgsVolumeManager.h:138; no body before -- its inline copy at
//     0x828C9250..0x828C92C4 attests it: the h:203 / h:204 asserts, then the record's mxFlags)
// run_fxfollowups_volume_test_deepest.py extracts both bodies from the revision and compiles them here against fakes
// of their collaborators: the entity manager (ids, first instances, the CONST GetVolumeInstance), the volume manager
// (a pool of flag records, the rw volumes) and a VolumeVolumeQuery whose GetPrimitiveIntersections records how it was
// primed and answers scripted contact results.
//
// The console, from the ARTIST listing:
//   * deepest starts at 0.0f (flt_82001CC0) and carries across every instance and entity;
//   * exclude index != 0xFFFF: r14 = (mbExcludeParts ? 0xFFFFFC00 : ~0) & the excluded id; else r14 = 0xFFFFFFFF;
//     a candidate is skipped when (r14 & id) == r14 (a bit-superset test, as compiled);
//   * out+8 = 0, out+0 = the query id up front; out+4 is written only with a hit;
//   * per instance: GetVolumeTypeFlags(volume) & query flags == 0 -> skip; else prime the VVQ (padding 0.0, the one
//     input volume / matrix, numInputs 1, currInput 0, volRefPairCount 0, the query volume and transform, no cull
//     table) and take -distance > deepest (NaN never wins) with numPoints != 0;
//   * the chain walks GetVolumeInstance(miNextEntityVolumeInstance) -- the CONST overload (0x828B9F28).
#include "types.hpp"
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"

static std::vector<std::string> gaAsserts;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) { gaAsserts.push_back(lpcMessage ? lpcMessage : ""); return 0; }
void* EndAssert() { return nullptr; }
} }

struct Matrix44Affine { f32 mafRows[16]; };

namespace rw { namespace collision {
    struct Volume { u8 maBytes[96]; };
    struct PrimitivePairIntersectResult { f32 distance; u32 numPoints; };
    struct VolumeVolumeQuery
    {
        const Volume**                m_inputVols;
        const Matrix44Affine**        m_inputMats;
        u32                           m_numInputs;
        u32                           m_currInput;
        const void*                   m_cullTable;
        f32                           m_padding;
        u32                           m_volRefPairCount;
        PrimitivePairIntersectResult* m_intersectionBuffer;
        const Volume*                 m_queryVol;
        const Matrix44Affine*         m_queryMtx;
        int GetPrimitiveIntersections();
    };
} }

// ---- the VolumeVolumeQuery double: records each priming, answers the script for the input volume --------------------
struct Priming
{
    const rw::collision::Volume* mpInputVol; const Matrix44Affine* mpInputMat; u32 muNumInputs; u32 muCurrInput;
    const void* mpCullTable; f32 mfPadding; u32 muVolRefPairCount; const rw::collision::Volume* mpQueryVol;
    const Matrix44Affine* mpQueryMtx;
};
static std::vector<Priming> gaPrimings;
static std::map<const rw::collision::Volume*, std::vector<rw::collision::PrimitivePairIntersectResult> > gaScript;
static rw::collision::PrimitivePairIntersectResult gaResultBuffer[16];

int rw::collision::VolumeVolumeQuery::GetPrimitiveIntersections()
{
    Priming lPriming = { m_numInputs ? m_inputVols[0] : nullptr, m_numInputs ? m_inputMats[0] : nullptr, m_numInputs,
                         m_currInput, m_cullTable, m_padding, m_volRefPairCount, m_queryVol, m_queryMtx };
    gaPrimings.push_back(lPriming);
    const std::vector<rw::collision::PrimitivePairIntersectResult>& lrResults = gaScript[lPriming.mpInputVol];
    for (size_t li = 0; li < lrResults.size() && li < 16; ++li)
        gaResultBuffer[li] = lrResults[li];
    m_intersectionBuffer = gaResultBuffer;
    return static_cast<int>(lrResults.size());
}

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

    const s32 KI_MAX_NUM_VOLUMES = 5048;

    struct VolumeManagerVolume
    {
        typedef u8 VolumeTypeFlags;
        VolumeTypeFlags mxFlags;
    };

    struct FakeVolumePool
    {
        bool                mabAllocated[8];
        VolumeManagerVolume maRecords[8];
        bool IsObjectAllocated(s32 li) const { return li >= 0 && li < 8 && mabAllocated[li]; }
        const VolumeManagerVolume& operator[](s32 li) const { return maRecords[li < 0 ? 0 : (li > 7 ? 7 : li)]; }
    };

    class VolumeManager
    {
    public:
        VolumeManagerVolume::VolumeTypeFlags GetVolumeTypeFlags(s32 liVolumeIndex) const;
        const VolRef::Volume* GetRwVolume(s32 liVolumeIndex) const
        {
            return reinterpret_cast<const VolRef::Volume*>(mapRwVolumes[liVolumeIndex]);
        }
        FakeVolumePool               mVolumePool;
        const rw::collision::Volume* mapRwVolumes[8];
    };

#include "fxfu_cvtd_flags.inc"   // VolumeManager::GetVolumeTypeFlags from the revision (or a stand-in)

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
        struct InEventVolumeTestDeepest
        {
            Matrix44Affine mTransform;
            u8             mVolumeBuffer[128];
            SceneQueryId   mQueryId;
            const u16*     mpau16EntityIndices;
            u16            mu16NumEntities;
            u16            mu16ExcludeEntityIndex;
            u8             mxVolumeTypeFlags;
            bool           mbExcludeParts;
        };
        struct OutEventVolumeTestDeepestResult { SceneQueryId mQueryId; f32 mfDepth; bool mbIntersection; };
    }

    struct FineIntersectionTestModule
    {
        typedef FineIntersectionTestIO::InEventVolumeTestDeepest        InEventVolumeTestDeepest;
        typedef FineIntersectionTestIO::OutEventVolumeTestDeepestResult OutEventVolumeTestDeepestResult;
        void ComputeVolumeTestDeepest(const InEventVolumeTestDeepest* lpQuery, OutEventVolumeTestDeepestResult* lpOutResult);
        VolumeManager*                     mpVolumeManager;
        EntityManager*                     mpEntityManager;
        rw::collision::VolumeVolumeQuery*  mpVolumeVolumeQuery;
    };

#include "fxfu_cvtd_body.inc"    // FineIntersectionTestModule::ComputeVolumeTestDeepest from the revision
}

using namespace CgsSceneManager;

static unsigned guChecks = 0, guFailures = 0;
static void Check(bool lbPass, const char* lpcLabel)
{
    ++guChecks;
    if (!lbPass) ++guFailures;
    std::printf("%s  %s\n", lbPass ? "PASS" : "FAIL", lpcLabel);
}

// ---- one world per case -------------------------------------------------------------------------------------------
static EntityManager                   gEntities;
static VolumeManager                   gVolumes;
static rw::collision::VolumeVolumeQuery gVvq;
static rw::collision::Volume           gaRw[8];
static FineIntersectionTestModule      gModule;

static void Reset()
{
    std::memset(&gEntities, 0, sizeof(gEntities));
    std::memset(&gVolumes, 0, sizeof(gVolumes));
    std::memset(&gVvq, 0xCD, sizeof(gVvq));   // every priming field starts as garbage
    gaPrimings.clear();
    gaScript.clear();
    gaAsserts.clear();
    for (int li = 0; li < 16; ++li) { gEntities.maiFirst[li] = -1; gEntities.maInstances[li].miNextEntityVolumeInstance = -1; }
    for (int li = 0; li < 8; ++li) gVolumes.mapRwVolumes[li] = &gaRw[li];
    gModule.mpVolumeManager = &gVolumes;
    gModule.mpEntityManager = &gEntities;
    gModule.mpVolumeVolumeQuery = &gVvq;
}

// entity slot -> id, first instance; instance -> volume, next; volume -> allocated, flags
static void Entity(u16 lu16Index, u32 luId, s32 liFirst) { gEntities.mauIds[lu16Index] = luId; gEntities.maiFirst[lu16Index] = liFirst; }
static void Instance(s32 liIndex, s32 liVolume, s32 liNext)
{
    for (int li = 0; li < 16; ++li) gEntities.maInstances[liIndex].mWorldSpaceTransform.mafRows[li] = static_cast<f32>(liIndex * 100 + li);
    gEntities.maInstances[liIndex].miVolumeIndex = liVolume;
    gEntities.maInstances[liIndex].miNextEntityVolumeInstance = liNext;
}
static void VolumeRecord(s32 liIndex, u8 lxFlags) { gVolumes.mVolumePool.mabAllocated[liIndex] = true; gVolumes.mVolumePool.maRecords[liIndex].mxFlags = lxFlags; }
static void Results(s32 liVolume, std::vector<rw::collision::PrimitivePairIntersectResult> laResults) { gaScript[&gaRw[liVolume]] = laResults; }

static FineIntersectionTestIO::InEventVolumeTestDeepest Query(const u16* lpau16, u16 lu16Count, u16 lu16Exclude, bool lbParts, u8 lxFlags)
{
    FineIntersectionTestIO::InEventVolumeTestDeepest lQuery;
    std::memset(&lQuery, 0, sizeof(lQuery));
    for (int li = 0; li < 16; ++li) lQuery.mTransform.mafRows[li] = static_cast<f32>(li);
    lQuery.mQueryId.mId = 0x10042u;
    lQuery.mpau16EntityIndices = lpau16;
    lQuery.mu16NumEntities = lu16Count;
    lQuery.mu16ExcludeEntityIndex = lu16Exclude;
    lQuery.mbExcludeParts = lbParts;
    lQuery.mxVolumeTypeFlags = lxFlags;
    return lQuery;
}

static FineIntersectionTestIO::OutEventVolumeTestDeepestResult Run(const FineIntersectionTestIO::InEventVolumeTestDeepest& lrQuery)
{
    FineIntersectionTestIO::OutEventVolumeTestDeepestResult lOut;
    lOut.mQueryId.mId = 0xDEADu;   // garbage the body must overwrite
    lOut.mfDepth = 123.0f;         // the sentinel a miss must leave alone (the console never writes +4 on a miss)
    lOut.mbIntersection = true;    // garbage the body must clear
    gModule.ComputeVolumeTestDeepest(&lrQuery, &lOut);
    return lOut;
}

int main()
{
    const f32 lfNaN = std::numeric_limits<f32>::quiet_NaN();

    // ---- A. one candidate, one instance: the priming and the deepest pick ------------------------------------------
    {
        Reset();
        Entity(3, 0x00000C00u, 5);
        Instance(5, 2, -1);
        VolumeRecord(2, 0x03);
        Results(2, { { -0.5f, 1 }, { -0.8f, 0 }, { lfNaN, 1 }, { -0.3f, 2 } });
        const u16 lau16[] = { 3 };
        const FineIntersectionTestIO::InEventVolumeTestDeepest lQuery = Query(lau16, 1, 0xFFFF, false, 0x01);
        const FineIntersectionTestIO::OutEventVolumeTestDeepestResult lOut = Run(lQuery);
        std::printf("A: out {id 0x%X, depth %g, hit %d}, %zu primings, %zu asserts\n", lOut.mQueryId.mId,
                    static_cast<double>(lOut.mfDepth), lOut.mbIntersection ? 1 : 0, gaPrimings.size(), gaAsserts.size());
        Check(lOut.mQueryId.mId == 0x10042u && lOut.mbIntersection && lOut.mfDepth == 0.5f && gaAsserts.empty(),
              "A1 the deepest -distance with points wins: {-0.5 n1, -0.8 n0, NaN n1, -0.3 n2} -> depth 0.5, hit, the query id "
              "(numPoints 0 is skipped, a NaN never wins)");
        const Priming* lp = gaPrimings.empty() ? nullptr : &gaPrimings[0];
        Check(gaPrimings.size() == 1 && lp->mpInputVol == &gaRw[2]
              && lp->mpInputMat == &gEntities.maInstances[5].mWorldSpaceTransform && lp->muNumInputs == 1u
              && lp->muCurrInput == 0u && lp->mpCullTable == nullptr && lp->mfPadding == 0.0f && lp->muVolRefPairCount == 0u
              && lp->mpQueryVol == reinterpret_cast<const rw::collision::Volume*>(&lQuery.mVolumeBuffer)
              && lp->mpQueryMtx == &lQuery.mTransform,
              "A2 the VolumeVolumeQuery is primed as at 0x828C92FC..0x828C931C: padding 0.0, {GetRwVolume}, {the instance's "
              "transform}, 1 input, currInput 0, volRefPairCount 0, the query's volume (+0x40) and transform (+0x00), no cull table");
    }

    // ---- B. misses leave the depth alone -------------------------------------------------------------------------------
    {
        Reset();
        const FineIntersectionTestIO::InEventVolumeTestDeepest lQuery = Query(nullptr, 0, 0xFFFF, false, 0x01);
        const FineIntersectionTestIO::OutEventVolumeTestDeepestResult lOut = Run(lQuery);
        Check(lOut.mQueryId.mId == 0x10042u && !lOut.mbIntersection && lOut.mfDepth == 123.0f && gaPrimings.empty()
              && gaAsserts.empty(),
              "B1 no candidates: out+0 = the query id, out+8 = 0, out+4 NOT written (0x828C9194), no query run");
    }
    {
        Reset();
        Entity(3, 0x00000C00u, 5);
        Instance(5, 2, -1);
        VolumeRecord(2, 0x01);
        Results(2, { { 0.25f, 3 }, { 0.0f, 1 } });
        const u16 lau16[] = { 3 };
        const FineIntersectionTestIO::OutEventVolumeTestDeepestResult lOut = Run(Query(lau16, 1, 0xFFFF, false, 0x01));
        Check(!lOut.mbIntersection && lOut.mfDepth == 123.0f && gaPrimings.size() == 1,
              "B2 separations only (distance 0.25, 0.0): -distance never beats the 0.0 start -> no hit, depth untouched");
    }

    // ---- C. the volume-type gate ---------------------------------------------------------------------------------------
    {
        Reset();
        Entity(3, 0x00000C00u, 5);
        Instance(5, 2, -1);
        VolumeRecord(2, 0x04);
        Results(2, { { -1.0f, 1 } });
        const u16 lau16[] = { 3 };
        const FineIntersectionTestIO::OutEventVolumeTestDeepestResult lOut = Run(Query(lau16, 1, 0xFFFF, false, 0x03));
        Check(!lOut.mbIntersection && gaPrimings.empty() && gaAsserts.empty(),
              "C1 GetVolumeTypeFlags(volume) & query+0xCC == 0 -> the instance is skipped, no query run (0x828C92C4)");
    }

    // ---- D / E / F. the exclude rule ------------------------------------------------------------------------------------
    {
        Reset();
        Entity(3, 0x00000C05u, 0);   // the excluded entity itself (entity 3, part 5)
        Entity(4, 0x00000C07u, 1);   // bits 0xC07 include every bit of 0xC05 -> skipped (a superset)
        Entity(5, 0x00001405u, 2);   // 0x1405 & 0xC05 = 0x405 -> tested
        Instance(0, 0, -1); Instance(1, 1, -1); Instance(2, 2, -1);
        VolumeRecord(0, 1); VolumeRecord(1, 1); VolumeRecord(2, 1);
        const u16 lau16[] = { 3, 4, 5 };
        Run(Query(lau16, 3, 3, false, 0x01));
        Check(gaPrimings.size() == 1 && gaPrimings[0].mpInputVol == &gaRw[2],
              "D1 exclude without parts: r14 = the excluded id; (r14 & id) == r14 skips the id itself AND a bit-superset of "
              "it (0xC07 vs 0xC05) -- only 0x1405 is queried");
    }
    {
        Reset();
        Entity(3, 0x00000C05u, 0);   // excluded: entity 3, part 5
        Entity(6, 0x00000C09u, 1);   // entity 3, part 9 -> skipped with parts
        Entity(7, 0x00001000u, 2);   // entity 4 -> tested
        Instance(0, 0, -1); Instance(1, 1, -1); Instance(2, 2, -1);
        VolumeRecord(0, 1); VolumeRecord(1, 1); VolumeRecord(2, 1);
        const u16 lau16[] = { 3, 6, 7 };
        Run(Query(lau16, 3, 3, true, 0x01));
        Check(gaPrimings.size() == 1 && gaPrimings[0].mpInputVol == &gaRw[2],
              "E1 exclude with parts: r14 = 0xFFFFFC00 & the excluded id -> every part of entity 3 is skipped, entity 4 is queried");
    }
    {
        Reset();
        Entity(8, 0xFFFFFFFFu, 0);   // the invalid id -> skipped when nothing is excluded
        Entity(9, 0x00002400u, 1);
        Instance(0, 0, -1); Instance(1, 1, -1);
        VolumeRecord(0, 1); VolumeRecord(1, 1);
        const u16 lau16[] = { 8, 9 };
        Run(Query(lau16, 2, 0xFFFF, false, 0x01));
        Check(gaPrimings.size() == 1 && gaPrimings[0].mpInputVol == &gaRw[1],
              "F1 no exclude (0xFFFF): r14 = dword_82F33F64 (0xFFFFFFFF) -> only an entity whose id is 0xFFFFFFFF is skipped");
    }

    // ---- G. the deepest carries across instances and entities; the chain walk ----------------------------------------
    {
        Reset();
        Entity(10, 0x00002800u, 11);
        Entity(11, 0x00002C00u, 13);
        Instance(11, 1, 12);          // entity 10: instance 11 -> 12
        Instance(12, 2, -1);
        Instance(13, 3, -1);          // entity 11: instance 13
        VolumeRecord(1, 1); VolumeRecord(2, 1); VolumeRecord(3, 1);
        Results(1, { { -0.6f, 1 } });
        Results(2, { { -0.4f, 1 } });
        Results(3, { { -0.2f, 1 } });
        const u16 lau16[] = { 10, 11 };
        const FineIntersectionTestIO::OutEventVolumeTestDeepestResult lOut = Run(Query(lau16, 2, 0xFFFF, false, 0x01));
        Check(gaPrimings.size() == 3 && lOut.mbIntersection && lOut.mfDepth == 0.6f,
              "G1 deepest carries across instances and entities (0.6, then 0.4, then 0.2 -> 0.6): f31 is never reset");
        // 5 const lookups: first(11), next 12, next -1 | first(13), next -1 (the fake's GetFirstEntityVolumeInstance
        // resolves its first instance through the same const overload, as the console's @0x828C5E58 does).
        Check(gEntities.miNonConstWalks == 0 && gEntities.miConstWalks == 5,
              "G2 the chain walks miNextEntityVolumeInstance with the CONST GetVolumeInstance (0x828B9F28), to the -1 end");
    }

    // ---- H. GetVolumeTypeFlags' asserts (its inline copy: h:203 / h:204) ----------------------------------------------
    {
        Reset();
        VolumeRecord(2, 0x5A);
        const u8 lxFlags = gVolumes.GetVolumeTypeFlags(2);
        Check(lxFlags == 0x5A && gaAsserts.empty(), "H1 GetVolumeTypeFlags(allocated) = the record's mxFlags (+0x08), no assert");
        gVolumes.GetVolumeTypeFlags(-1);
        gVolumes.GetVolumeTypeFlags(4);   // in range, not allocated
        Check(gaAsserts.size() == 3 && gaAsserts[0] == "liVolumeIndex >=0 && liVolumeIndex < KI_MAX_NUM_VOLUMES"
              && gaAsserts[1] == "mVolumePool.IsObjectAllocated( liVolumeIndex )"
              && gaAsserts[2] == "mVolumePool.IsObjectAllocated( liVolumeIndex )",
              "H2 GetVolumeTypeFlags(-1) fires the range assert (0x820F51CC) and, unallocated, the pool assert (0x820F519C); "
              "an in-range unallocated slot fires the pool assert");
    }

    std::printf("FxFollowupsVolumeTestDeepest: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
