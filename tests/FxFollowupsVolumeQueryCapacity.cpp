// FX-FOLLOWUPS (crash parity 2026-09-25): the HOST capacity of VolumeVolumeQuery's report region.
//
// GetPrimitiveBBoxOverlaps @0x82BB3AB0 meters its 1xN staging in CONSOLE words -- a budget of 8 bytes per
// result (`slwi r27, r11, 3`), 12 per group header, 4 per staged pair -- and Construct @0x82BB38F0 carves
// exactly 8 * results for it; GetResourceDescriptor @0x82BB3A20's `2072 * a3 + 192` leaves 192 * (results + 1)
// for the GPInstance scratch PrimitiveBatchIntersect @0x82BABC78 fills (slot 0 + the group's N instances,
// console stride 0xC0). On x64 a staged group is 16 + 8n bytes and a GPInstance 0xC8, so with the console
// sizes:
//   * ten 10-pair groups (520 console bytes, inside the 800 budget) take 960 host bytes: the tail lands in the
//     result array, which PrimitiveBatchIntersect writes while later groups are still unread;
//   * one 100-pair group needs 101 instances = 20200 host bytes in a 19392-byte region: the last 808 bytes go
//     past the end of the descriptor.
// run_fxfollowups_volume_query_capacity.py compiles the revision's VolumeQuery.cpp and VolumeBBoxQuery.cpp (and
// the working tree's VolRef.cpp) beside this file, with the PRODUCTION PrimitiveBatchIntersect pasted in
// (fxfu_vqcap.inc). The volume descriptors, the aggregate and the two narrow-phase kernels are fixtures: the
// aggregate stages its children through the production AddPrimitiveRef, CreateGPInstance fills a whole host
// GPInstance, and each kernel writes one whole result record (a narrow phase that found a contact).
#include "types.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <excpt.h>
#include <malloc.h>
#include <vector>

#include "vendor/renderware/collision/VolumeQuery.hpp"
#include "vendor/renderware/collision/VolumeBBoxQuery.hpp"
#include "vendor/renderware/collision/CollisionVolume.hpp"
#include "vendor/renderware/collision/AABBox.hpp"
#include "vendor/renderware/collision/VolRef.hpp"
#include "vendor/renderware/collision/GPInstance.hpp"
#include "vendor/renderware/collision/Aggregate.hpp"

using namespace rw::collision;

// ---- fixtures -------------------------------------------------------------------------------------------------
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
namespace CgsDev { namespace Log { DebugPrint* gpDebugPrint = nullptr; } }
// VolumeQuery.cpp's line walk announces its two [PC TRAP]s through WriteToLog and asserts through CgsDev::Assert
// (FX-FOLLOWUPS stage a); nothing here reaches them, so any that fires is printed.
namespace CgsDev { namespace Log { void WriteToLog(const char* lpcText) { std::printf("LOG (collision TU): %s", lpcText); } } }
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) { std::printf("ASSERT (collision TU): %s\n", lpcMessage); return 0; }
void* EndAssert() { return nullptr; }
} }

namespace rw { namespace collision {
    Volume::VTable* gVolumeVTable[E_VOLUMETYPE_NUMINTERNALTYPES] = {};
} }

// What the fixture kernels and CreateGPInstance saw.
static const u8*                  gpInstanceHighWater = nullptr;   // one past the last GPInstance byte written
static std::vector<const Volume*> gaSlot0Volumes;                  // the "1"-side volume of every group
static std::vector<s32>           gaGroupSizes;                    // N of every group
static std::vector<const void*>   gaSlotVolume;                    // the volume instanced into each slot, by slot

static RwBool FixtureGetBBox(const Volume* lpVolume, const Vec4*, RwBool, AABBox& arBox)
{
    const Vec4& lrCentre = lpVolume->maTransform[3];
    const f32 lfR = lpVolume->mfRadius;
    const f32 laf[8] = { lrCentre.x - lfR, lrCentre.y - lfR, lrCentre.z - lfR, 0.0f,
                         lrCentre.x + lfR, lrCentre.y + lfR, lrCentre.z + lfR, 0.0f };
    std::memcpy(&arBox, laf, sizeof(laf));
    return 1;
}

static GPInstance* gpInstancingBase = nullptr;
static RwBool FixtureCreateGPInstance(const Volume* lpVolume, GPInstance& arInstance, const Vec4*)
{
    std::memset(&arInstance, 0xAB, sizeof(GPInstance));                 // a whole host instance, as the real ones
    const u8* lpEnd = reinterpret_cast<const u8*>(&arInstance) + sizeof(GPInstance);
    if (lpEnd > gpInstanceHighWater) gpInstanceHighWater = lpEnd;
    const size_t luSlot = static_cast<size_t>(&arInstance - gpInstancingBase);
    if (gaSlotVolume.size() <= luSlot) gaSlotVolume.resize(luSlot + 1, nullptr);
    gaSlotVolume[luSlot] = lpVolume;
    return 1;
}

static Volume::VTable gSphereDescriptor    = { E_VOLUMETYPE_SPHERE,    FixtureGetBBox, nullptr, nullptr, nullptr,
                                               FixtureCreateGPInstance };
static Volume::VTable gAggregateDescriptor = { E_VOLUMETYPE_AGGREGATE, FixtureGetBBox };

namespace rw { namespace collision {
    // The narrow phase is not under test: record the group and write ONE whole result record, as a
    // kernel that found a contact does (the pre-fix staging tail lives where this lands).
    static s32 RecordGroup(PrimitivePairIntersectResult* lapResults, s32 aiResBufMaxSize, s32 aiNum)
    {
        gaSlot0Volumes.push_back(gaSlotVolume.empty() ? nullptr : static_cast<const Volume*>(gaSlotVolume[0]));
        gaGroupSizes.push_back(aiNum);
        if (aiResBufMaxSize <= 0) return 0;
        std::memset(lapResults, 0xEE, sizeof(PrimitivePairIntersectResult));
        return 1;
    }
    s32 GPInstanceBatchIntersect1xN(PrimitivePairIntersectResult* lapResults, s32 aiResBufMaxSize,
                                    const GPInstance&, const GPInstance*, s32 aiNum, f32)
    { return RecordGroup(lapResults, aiResBufMaxSize, aiNum); }
    s32 GPInstanceBatchIntersectNx1(PrimitivePairIntersectResult* lapResults, s32 aiResBufMaxSize,
                                    const GPInstance*, s32 aiNum, const GPInstance&, f32)
    { return RecordGroup(lapResults, aiResBufMaxSize, aiNum); }

#include "fxfu_vqcap.inc"
} }

// ---- the fixture aggregate: its BBoxOverlapQuery stages every child that meets the query box ------------------
// Mirrors VolumeBBoxQuery.cpp's AggregateVTableView (host layout; the query reaches the table through the
// pointer at Aggregate+0x20 and calls +0x18 == m_BBoxOverlapQuery on the console).
typedef RwBool (*FixtureBBoxOverlapQueryFn)(Aggregate*, VolumeBBoxQuery*, const rw::math::vpu::Matrix44Affine*);
struct FixtureAggregateVTable
{
    u32                       muType;
    void*                     mpGetSize;
    u32                       muAlignment;
    RwBool                    mbIsProcedural;
    void*                     mpUpdate;
    void*                     mpLineIntersectionQuery;
    FixtureBBoxOverlapQueryFn mpBBoxOverlapQuery;
};
struct FixtureAggregate
{
    u8                            mau8AABB[0x20];   // +0x00 m_AABB
    const FixtureAggregateVTable* mpVTable;         // +0x20 m_vTable
    const Volume*                 mpChildren;
    u32                           muNumChildren;
};
static_assert(offsetof(FixtureAggregate, mpVTable) == 0x20, "the query reads the aggregate's table at +0x20");

static RwBool FixtureBBoxOverlapQuery(Aggregate* lpAggregate, VolumeBBoxQuery* lpQuery,
                                      const rw::math::vpu::Matrix44Affine* lpMtx)
{
    const FixtureAggregate* lpAgg = reinterpret_cast<const FixtureAggregate*>(lpAggregate);
    const f32* lafMin = reinterpret_cast<const f32*>(&lpQuery->m_aabb);
    const f32* lafMax = lafMin + 4;
    for (u32 li = 0; li < lpAgg->muNumChildren; ++li)
    {
        AABBox lBox;
        FixtureGetBBox(&lpAgg->mpChildren[li], nullptr, 0, lBox);
        const f32* lafBox = reinterpret_cast<const f32*>(&lBox);
        bool lbSeparated = false;
        for (int la = 0; la < 3; ++la)
            if (lafBox[la] > lafMax[la] || lafMin[la] > lafBox[4 + la]) lbSeparated = true;
        if (lbSeparated) continue;
        if (!lpQuery->AddPrimitiveRef(&lpAgg->mpChildren[li], lpMtx, lBox, 0, 0)) return 0;
    }
    return 1;
}
static const FixtureAggregateVTable gFixtureAggregateVTable = { 0, nullptr, 16, 0, nullptr, nullptr, FixtureBBoxOverlapQuery };

static Volume MakeSphere(f32 x, f32 y, f32 z, f32 r)
{
    Volume lVolume;
    std::memset(&lVolume, 0, sizeof(lVolume));
    lVolume.maTransform[0].x = 1.0f; lVolume.maTransform[1].y = 1.0f; lVolume.maTransform[2].z = 1.0f;
    lVolume.maTransform[3].x = x; lVolume.maTransform[3].y = y; lVolume.maTransform[3].z = z; lVolume.maTransform[3].w = 1.0f;
    lVolume.muVTableSlot = E_VOLUMETYPE_SPHERE;
    lVolume.mfRadius = r;
    lVolume.muFlags = KU_VOLUMEFLAG_ISENABLED;
    return lVolume;
}

// An aggregate volume of radius 50 at the origin (so it is the LARGER side and its children are the first
// pass) whose children are lu children of radius 0.1 spread inside +/-4 (all inside the query sphere's box).
static void MakeAggregate(Volume& arVolume, FixtureAggregate& arAgg, std::vector<Volume>& arChildren, u32 luChildren)
{
    arChildren.clear();
    for (u32 li = 0; li < luChildren; ++li)
        arChildren.push_back(MakeSphere(-4.0f + 8.0f * static_cast<f32>(li % 10) / 9.0f,
                                        -4.0f + 8.0f * static_cast<f32>((li / 10) % 10) / 9.0f, 0.0f, 0.1f));
    std::memset(&arAgg, 0, sizeof(arAgg));
    arAgg.mpVTable = &gFixtureAggregateVTable;
    arAgg.mpChildren = arChildren.data();
    arAgg.muNumChildren = luChildren;
    arVolume = MakeSphere(0.0f, 0.0f, 0.0f, 50.0f);
    arVolume.muVTableSlot = E_VOLUMETYPE_AGGREGATE;
    const FixtureAggregate* lpAgg = &arAgg;
    std::memcpy(reinterpret_cast<u8*>(&arVolume) + 0x44, &lpAgg, sizeof(lpAgg));   // Volume::aggregateData.agg
}

static int RunGuarded(VolumeVolumeQuery* lpQuery, bool lbIntersect, int* lpiResult)
{
    __try
    {
        *lpiResult = lbIntersect ? lpQuery->GetPrimitiveIntersections() : lpQuery->GetPrimitiveBBoxOverlaps();
        return 1;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

static unsigned guChecks = 0, guFailures = 0;
static void Check(bool lbPass, const char* lpcLabel)
{
    ++guChecks;
    if (!lbPass) ++guFailures;
    std::printf("%s  %s\n", lbPass ? "PASS" : "FAIL", lpcLabel);
}

static const u8  KU8_GUARD      = 0xCD;
static const u32 KU_GUARD_BYTES = 4096;
static bool GuardIntact(const u8* lpFrom, const u8* lpTo)
{
    for (const u8* lp = lpFrom; lp < lpTo; ++lp)
        if (*lp != KU8_GUARD) return false;
    return true;
}

struct Scenario
{
    u8*                lpMem;
    u8*                lpBase;
    u32                luSize;
    VolumeVolumeQuery* lpQuery;
};

static Scenario Build()
{
    u32 lauDesc[12] = {};
    VolumeVolumeQuery::GetResourceDescriptor(lauDesc, 100, 100);
    Scenario lS;
    lS.luSize = lauDesc[0];
    lS.lpMem  = static_cast<u8*>(_aligned_malloc(lS.luSize + 2 * KU_GUARD_BYTES, 16));
    std::memset(lS.lpMem, KU8_GUARD, lS.luSize + 2 * KU_GUARD_BYTES);
    lS.lpBase = lS.lpMem + KU_GUARD_BYTES;
    void* lapBuffer[5] = { lS.lpBase, nullptr, nullptr, nullptr, nullptr };
    lS.lpQuery = static_cast<VolumeVolumeQuery*>(VolumeVolumeQuery::Initialize(lapBuffer, 100, 100));
    gpInstancingBase = lS.lpQuery->m_instancingSPR;
    return lS;
}

static void Prime(VolumeVolumeQuery* lpQuery, const Volume** lapInputs, u32 luInputs, const Volume* lpQueryVol)
{
    lpQuery->m_padding         = 0.0f;
    lpQuery->m_currInput       = 0;
    lpQuery->m_inputVols       = lapInputs;
    lpQuery->m_inputMats       = nullptr;
    lpQuery->m_numInputs       = luInputs;
    lpQuery->m_volRefPairCount = 0;
    lpQuery->m_queryVol        = lpQueryVol;
    lpQuery->m_queryMtx        = nullptr;
    lpQuery->m_cullTable       = nullptr;
}

int main()
{
    gVolumeVTable[E_VOLUMETYPE_SPHERE]    = &gSphereDescriptor;
    gVolumeVTable[E_VOLUMETYPE_AGGREGATE] = &gAggregateDescriptor;

    const Volume lQuerySphere = MakeSphere(0.0f, 0.0f, 0.0f, 5.0f);

    // ---- the descriptor: the host total for the (100, 100) queries ------------------------------------------------
    {
        u32 lauDesc[12] = {};
        VolumeVolumeQuery::GetResourceDescriptor(lauDesc, 100, 100);
        std::printf("descriptor(100, 100) = 0x%X; host sizeof GPInstance 0x%zX, VolRef1xN header %zu\n",
                    lauDesc[0], sizeof(GPInstance), offsetof(VolRef1xN, vRefsN));
        Check(lauDesc[0] == 0x495D8u,
              "D1 GetResourceDescriptor(100, 100) is the host total 0x495D8 (console 0x48F30; header-only fix 0x48F90)");
        Check(lauDesc[0] <= 0x62000u && lauDesc[0] > 0x49000u,
              "D2 it fits OverlapCullingModule's 0x62000 and outgrows the console's 0x49000 octree buffer");
    }

    // ---- A: one aggregate input with 100 children -> one 100-pair group -------------------------------------------
    {
        Scenario lS = Build();
        Volume lAggVolume; FixtureAggregate lAgg; std::vector<Volume> laChildren;
        MakeAggregate(lAggVolume, lAgg, laChildren, 100);
        const Volume* lapInputs[1] = { &lAggVolume };
        Prime(lS.lpQuery, lapInputs, 1, &lQuerySphere);

        int liStaged = -1;
        const int liRan = RunGuarded(lS.lpQuery, false, &liStaged);
        const u32 luGroups = lS.lpQuery->m_volRef1xNCount, luPairs = lS.lpQuery->m_volRefPairCount;
        const size_t luHostBytes = 16u * luGroups + 8u * luPairs;
        const size_t luRegion = reinterpret_cast<const u8*>(lS.lpQuery->m_intersectionBuffer)
                              - reinterpret_cast<const u8*>(lS.lpQuery->m_volRefPairBuffer);
        std::printf("A: staged %d (groups %u pairs %u) host bytes %zu region %zu\n", liStaged, luGroups, luPairs,
                    luHostBytes, luRegion);
        Check(liRan == 1 && liStaged == 100 && luGroups == 1 && luPairs == 100,
              "A1 a 100-child aggregate against one sphere stages ONE group of 100 pairs (console metering: 412 of 800)");
        Check(luHostBytes <= luRegion, "A2 the staged group (816 host bytes) fits the host staging region");

        gpInstanceHighWater = nullptr; gaSlot0Volumes.clear(); gaGroupSizes.clear(); gaSlotVolume.clear();
        lS.lpQuery->m_currInput = 0;
        int liHits = -1;
        const int liRan2 = RunGuarded(lS.lpQuery, true, &liHits);
        std::printf("A: intersections %d, instance high water +0x%zX of 0x%X\n", liHits,
                    gpInstanceHighWater ? static_cast<size_t>(gpInstanceHighWater - lS.lpBase) : 0, lS.luSize);
        Check(liRan2 == 1 && liHits == 1 && gaGroupSizes.size() == 1 && gaGroupSizes[0] == 100
              && gaSlot0Volumes.size() == 1 && gaSlot0Volumes[0] == &lQuerySphere,
              "A3 GetPrimitiveIntersections instances the group (slot 0 = the query sphere, 100 children) and runs it once");
        Check(gpInstanceHighWater != nullptr && gpInstanceHighWater <= lS.lpBase + lS.luSize,
              "A4 all 101 GPInstances lie inside the descriptor (console region 192 * 101)");
        Check(GuardIntact(lS.lpBase + lS.luSize, lS.lpBase + lS.luSize + KU_GUARD_BYTES)
              && GuardIntact(lS.lpMem, lS.lpBase),
              "A5 the guards before and after the descriptor are intact");
        _aligned_free(lS.lpMem);
    }

    // ---- B: ten aggregate inputs with 10 children each -> ten 10-pair groups --------------------------------------
    {
        Scenario lS = Build();
        Volume laAggVolume[10]; FixtureAggregate laAgg[10]; std::vector<Volume> laChildren[10];
        const Volume* lapInputs[10];
        for (int li = 0; li < 10; ++li)
        {
            MakeAggregate(laAggVolume[li], laAgg[li], laChildren[li], 10);
            lapInputs[li] = &laAggVolume[li];
        }
        Prime(lS.lpQuery, lapInputs, 10, &lQuerySphere);

        int liStaged = -1;
        const int liRan = RunGuarded(lS.lpQuery, false, &liStaged);
        const u32 luGroups = lS.lpQuery->m_volRef1xNCount, luPairs = lS.lpQuery->m_volRefPairCount;
        const size_t luHostBytes = 16u * luGroups + 8u * luPairs;
        const size_t luRegion = reinterpret_cast<const u8*>(lS.lpQuery->m_intersectionBuffer)
                              - reinterpret_cast<const u8*>(lS.lpQuery->m_volRefPairBuffer);
        std::printf("B: staged %d (groups %u pairs %u) host bytes %zu region %zu\n", liStaged, luGroups, luPairs,
                    luHostBytes, luRegion);
        Check(liRan == 1 && liStaged == 100 && luGroups == 10 && luPairs == 100,
              "B1 ten 10-child aggregates stage ten groups of 10 pairs (console metering: 520 of 800)");
        Check(luHostBytes <= luRegion, "B2 the staged groups (960 host bytes) fit the host staging region");

        gpInstanceHighWater = nullptr; gaSlot0Volumes.clear(); gaGroupSizes.clear(); gaSlotVolume.clear();
        lS.lpQuery->m_currInput = 0;
        int liHits = -1;
        const int liRan2 = RunGuarded(lS.lpQuery, true, &liHits);
        bool lbGroupsRight = gaGroupSizes.size() == 10 && gaSlot0Volumes.size() == 10;
        for (size_t li = 0; lbGroupsRight && li < gaGroupSizes.size(); ++li)
            lbGroupsRight = gaGroupSizes[li] == 10 && gaSlot0Volumes[li] == &lQuerySphere;
        std::printf("B: ran %d intersections %d groups seen %zu\n", liRan2, liHits, gaGroupSizes.size());
        Check(liRan2 == 1, "B3 GetPrimitiveIntersections completes (pre-fix: result 0 lands on the unread staged groups)");
        Check(liRan2 == 1 && liHits == 10 && lbGroupsRight,
              "B4 every one of the ten groups reaches the narrow phase intact (slot 0 = the query sphere, N = 10)");
        Check(GuardIntact(lS.lpBase + lS.luSize, lS.lpBase + lS.luSize + KU_GUARD_BYTES),
              "B5 the guard after the descriptor is intact");
        _aligned_free(lS.lpMem);
    }

    std::printf("FxFollowupsVolumeQueryCapacity: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
