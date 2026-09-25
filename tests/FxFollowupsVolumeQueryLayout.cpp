// FX-FOLLOWUPS (crash parity 2026-09-25): the HOST layout of the two in-place rw::collision query objects.
//
// VolumeVolumeQuery::Construct @0x82BB38F0 carves its two VolumeBBoxQuery sub-queries right behind the object
// (console `addi r11, r31, 0x50` @0x82BB390C == the console object size 0x48 rounded to 16), and
// VolumeBBoxQuery::Initialize @0x82BBBD90 carves its stack VolRef records right behind ITS object (`addi r10,
// r11, 0x100` @0x82BBBD9C == 0xF1 rounded to 16). The port kept both CONSOLE sizes, but the host objects are
// 0x88 and 0x110 bytes:
//   * sub-query A sat ON the parent's m_intersectionBuffer .. m_bBoxQueryBtoA, so GetPrimitiveBBoxOverlaps'
//     first-pass priming overwrote them, and its second-pass read of m_bBoxQueryBtoA then wrote through the
//     AABB's float bits (here: null -> an access violation, caught below);
//   * an aggregate's stack VolRef (AddVolumeRef) sat ON the sub-query's own m_curSpatialMapQuery / m_tag /
//     m_numTagBits.
// run_fxfollowups_volume_query_layout.py compiles the revision's VolumeQuery.cpp and VolumeBBoxQuery.cpp (and
// the working tree's VolRef.cpp) beside this file. The volume descriptors are fixtures: a sphere type (1) whose
// getBBox returns centre -/+ radius, and an aggregate type (6) for the stack push.
#include "types.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <excpt.h>
#include <malloc.h>

#include "vendor/renderware/collision/VolumeQuery.hpp"
#include "vendor/renderware/collision/VolumeBBoxQuery.hpp"
#include "vendor/renderware/collision/CollisionVolume.hpp"
#include "vendor/renderware/collision/AABBox.hpp"
#include "vendor/renderware/collision/VolRef.hpp"
#include "vendor/renderware/collision/GPInstance.hpp"

using namespace rw::collision;

// ---- fixtures -------------------------------------------------------------------------------------------------
// The [vvq] DIAG in GetPrimitiveIntersections (not called here) logs through gpDebugPrint; no log in this harness.
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
    // Not reached: the test stops at GetPrimitiveBBoxOverlaps (the narrow phase is not under test).
    s32 PrimitiveBatchIntersect(PrimitivePairIntersectResult*, s32, GPInstance*, VolRef1xN*, s32, f32) { return 0; }
    Volume::VTable* gVolumeVTable[E_VOLUMETYPE_NUMINTERNALTYPES] = {};
} }

static RwBool FixtureGetBBox(const Volume* lpVolume, const Vec4*, RwBool, AABBox& arBox)
{
    const Vec4& lrCentre = lpVolume->maTransform[3];
    const f32 lfR = lpVolume->mfRadius;
    const f32 laf[8] = { lrCentre.x - lfR, lrCentre.y - lfR, lrCentre.z - lfR, 0.0f,
                         lrCentre.x + lfR, lrCentre.y + lfR, lrCentre.z + lfR, 0.0f };
    std::memcpy(&arBox, laf, sizeof(laf));
    return 1;
}

static Volume::VTable gSphereDescriptor    = { E_VOLUMETYPE_SPHERE,    FixtureGetBBox };
static Volume::VTable gAggregateDescriptor = { E_VOLUMETYPE_AGGREGATE, FixtureGetBBox };

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

// GetPrimitiveBBoxOverlaps under SEH: on the pre-fix layout its second-pass write goes through the clobbered
// m_bBoxQueryBtoA. (No C++ objects with destructors in this frame -- required for __try.)
static int RunOverlapsGuarded(VolumeVolumeQuery* lpQuery, int* lpiResult)
{
    __try
    {
        *lpiResult = lpQuery->GetPrimitiveBBoxOverlaps();
        return 1;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

static int RunAddVolumeRefGuarded(VolumeBBoxQuery* lpQuery, const Volume* lpVolume, const AABBox& arBox, RwBool* lpbResult)
{
    __try
    {
        *lpbResult = lpQuery->AddVolumeRef(lpVolume, nullptr, arBox, 0x5A5A5A5Au, 7);
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

static const u8 KU8_GUARD = 0xCD;
static bool GuardIntact(const u8* lpFrom, const u8* lpTo)
{
    if (lpTo < lpFrom) return false;    // a negative gap is an overlapping carve (an empty one is fine)
    for (const u8* lp = lpFrom; lp < lpTo; ++lp)
        if (*lp != KU8_GUARD) return false;
    return true;
}

static const u32 KU_GUARD_BYTES = 256;

int main()
{
    gVolumeVTable[E_VOLUMETYPE_SPHERE]    = &gSphereDescriptor;
    gVolumeVTable[E_VOLUMETYPE_AGGREGATE] = &gAggregateDescriptor;

    // ---- the carve (the module call sites' (100, 100)) -----------------------------------------------------------
    u32 lauDesc[12] = {};
    VolumeVolumeQuery::GetResourceDescriptor(lauDesc, 100, 100);
    u32 lauBBoxDesc[12] = {};
    VolumeBBoxQuery::GetResourceDescriptor(lauBBoxDesc, 100, 100);
    const u32 luSize = lauDesc[0];
    std::printf("descriptor 0x%X (bbox 0x%X); host sizeof VolumeVolumeQuery 0x%zX, VolumeBBoxQuery 0x%zX\n",
                luSize, lauBBoxDesc[0], sizeof(VolumeVolumeQuery), sizeof(VolumeBBoxQuery));

    u8* lpMem = static_cast<u8*>(_aligned_malloc(luSize + 2 * KU_GUARD_BYTES, 16));
    std::memset(lpMem, KU8_GUARD, luSize + 2 * KU_GUARD_BYTES);
    u8* lpBase = lpMem + KU_GUARD_BYTES;
    void* lapBuffer[5] = { lpBase, nullptr, nullptr, nullptr, nullptr };
    VolumeVolumeQuery* lpQuery = static_cast<VolumeVolumeQuery*>(VolumeVolumeQuery::Initialize(lapBuffer, 100, 100));

    const u8* lpA = reinterpret_cast<const u8*>(lpQuery->m_bBoxQueryAtoB);
    const u8* lpB = reinterpret_cast<const u8*>(lpQuery->m_bBoxQueryBtoA);
    Check(reinterpret_cast<u8*>(lpQuery) == lpBase, "L1 the query is built in place at the base of its store");
    Check(lpA >= lpBase + sizeof(VolumeVolumeQuery),
          "L2 sub-query A starts at or after the END of the host object (console: +0x50 == its own size)");
    Check((reinterpret_cast<uintptr_t>(lpA) & 15u) == 0, "L3 sub-query A is 16-byte aligned");
    Check(lpB == lpA + lauBBoxDesc[0] && lpB >= lpA + sizeof(VolumeBBoxQuery),
          "L4 sub-query B follows A by A's descriptor and does not overlap A's object");
    Check(reinterpret_cast<const u8*>(lpQuery->m_bBoxQueryAtoB->m_stackVRefBuffer) >= lpA + sizeof(VolumeBBoxQuery) &&
          reinterpret_cast<const u8*>(lpQuery->m_bBoxQueryBtoA->m_stackVRefBuffer) >= lpB + sizeof(VolumeBBoxQuery),
          "L5 each sub-query's stack VolRefs start at or after the END of its host object (console: +0x100)");
    Check(reinterpret_cast<const u8*>(lpQuery->m_instancingSPR) < lpBase + luSize &&
          reinterpret_cast<const u8*>(lpQuery->m_volRefPairBuffer) >= lpB + lauBBoxDesc[0],
          "L6 the report buffers lie after both sub-queries and inside the descriptor");

    // ---- the runtime pass: one overlapping sphere pair, as DoPairQuery primes it -----------------------------------
    const Volume lInput = MakeSphere(0.0f, 0.0f, 1.0f, 1.0f);   // box min.z == 0, min.w == 0 (the clobber is null)
    const Volume lQueryVol = MakeSphere(0.5f, 0.0f, 1.0f, 1.0f);
    const Volume* lapInputs[1] = { &lInput };

    lpQuery->m_padding         = 0.0f;
    lpQuery->m_currInput       = 0;
    lpQuery->m_inputVols       = lapInputs;
    lpQuery->m_inputMats       = nullptr;
    lpQuery->m_numInputs       = 1;
    lpQuery->m_volRefPairCount = 0;
    lpQuery->m_queryVol        = &lQueryVol;
    lpQuery->m_queryMtx        = nullptr;
    lpQuery->m_cullTable       = nullptr;

    const VolumeVolumeQuery lBefore = *lpQuery;   // every parent field, as the pass must leave them
    int liStaged = -1;
    const int liRan = RunOverlapsGuarded(lpQuery, &liStaged);
    Check(liRan == 1, "R1 GetPrimitiveBBoxOverlaps completes (pre-fix: the second pass writes through the clobbered m_bBoxQueryBtoA)");
    Check(liRan == 1 && liStaged == 1, "R2 the overlapping pair is staged once");
    Check(lpQuery->m_intersectionBuffer == lBefore.m_intersectionBuffer &&
          lpQuery->m_intersectionBufferMaxSize == lBefore.m_intersectionBufferMaxSize &&
          lpQuery->m_volRefPairBuffer == lBefore.m_volRefPairBuffer &&
          lpQuery->m_volRefPairBufferSize == lBefore.m_volRefPairBufferSize &&
          lpQuery->m_instancingSPR == lBefore.m_instancingSPR,
          "R3 the parent's report-buffer fields survive the pass");
    Check(lpQuery->m_bBoxQueryAtoB == lBefore.m_bBoxQueryAtoB && lpQuery->m_bBoxQueryBtoA == lBefore.m_bBoxQueryBtoA,
          "R4 the parent's sub-query handles survive the pass");
    Check(lpQuery->m_queryVol == &lQueryVol && lpQuery->m_queryMtx == nullptr && lpQuery->m_padding == 0.0f,
          "R5 the parent's per-call query fields survive the pass");
    Check(GuardIntact(lpBase + sizeof(VolumeVolumeQuery), lpA),
          "R6 the guard between the host object and sub-query A is intact");
    Check(GuardIntact(lpMem, lpBase) && GuardIntact(lpBase + luSize, lpBase + luSize + KU_GUARD_BYTES),
          "R7 the guards before and after the descriptor are intact");

    // ---- a separated pair stages nothing (sanity of the pass itself) ------------------------------------------------
    if (liRan == 1)
    {
        const Volume lFar = MakeSphere(10.0f, 0.0f, 1.0f, 1.0f);
        lpQuery->m_currInput = 0;
        lpQuery->m_queryVol  = &lFar;
        int liFar = -1;
        const int liFarRan = RunOverlapsGuarded(lpQuery, &liFar);
        Check(liFarRan == 1 && liFar == 0, "R8 a separated pair stages nothing");
    }
    else
    {
        Check(false, "R8 a separated pair stages nothing (not run: R1 failed)");
    }

    // ---- the sub-query's stack: an aggregate push must not touch its own tail --------------------------------------
    {
        u8* lpMem2 = static_cast<u8*>(_aligned_malloc(lauBBoxDesc[0] + 2 * KU_GUARD_BYTES, 16));
        std::memset(lpMem2, KU8_GUARD, lauBBoxDesc[0] + 2 * KU_GUARD_BYTES);
        void* lapBuffer2[5] = { lpMem2 + KU_GUARD_BYTES, nullptr, nullptr, nullptr, nullptr };
        VolumeBBoxQuery* lpBBox = static_cast<VolumeBBoxQuery*>(VolumeBBoxQuery::Initialize(lapBuffer2, 100, 100));
        lpBBox->m_stackNext          = 0;
        lpBBox->m_curSpatialMapQuery = reinterpret_cast<void*>(static_cast<uintptr_t>(0x1234567890ull));
        lpBBox->m_tag                = 0xC0FFEEu;
        lpBBox->m_numTagBits         = 3;

        Volume lAggregate = MakeSphere(0.0f, 0.0f, 0.0f, 1.0f);
        lAggregate.muVTableSlot = E_VOLUMETYPE_AGGREGATE;
        alignas(16) u8 lau8Box[32];
        const f32 laf[8] = { -1.0f, -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f };
        std::memcpy(lau8Box, laf, sizeof(laf));

        RwBool lbPushed = 0;
        const int liPushRan = RunAddVolumeRefGuarded(lpBBox, &lAggregate, *reinterpret_cast<const AABBox*>(lau8Box), &lbPushed);
        Check(liPushRan == 1 && lbPushed == 1 && lpBBox->m_stackNext == 1, "S1 the aggregate is pushed onto the traversal stack");
        Check(lpBBox->m_curSpatialMapQuery == reinterpret_cast<void*>(static_cast<uintptr_t>(0x1234567890ull)) &&
              lpBBox->m_tag == 0xC0FFEEu && lpBBox->m_numTagBits == 3,
              "S2 the push leaves the sub-query's m_curSpatialMapQuery / m_tag / m_numTagBits alone (pre-fix: the record sat on them)");
        Check(GuardIntact(lpMem2 + KU_GUARD_BYTES + sizeof(VolumeBBoxQuery),
                          reinterpret_cast<const u8*>(lpBBox->m_stackVRefBuffer)),
              "S3 the guard between the sub-query object and its stack records is intact");
        Check(GuardIntact(lpMem2 + KU_GUARD_BYTES + lauBBoxDesc[0], lpMem2 + 2 * KU_GUARD_BYTES + lauBBoxDesc[0]),
              "S4 the guard after the sub-query's descriptor is intact");
    }

    std::printf("FxFollowupsVolumeQueryLayout: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
