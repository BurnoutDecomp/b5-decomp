// FX-TAILS-A item 6 (crash parity 2026-09-24): the ResetOnTrack Strategies TU's Normalise3D has the console's
// arithmetic -- NO zero guard. run_fxtailsa_reset_normalise.py extracts verbatim from
// BrnResetOnTrackManager_Strategies.cpp the TU helper namespace and ResetOnTrackManager::ResetNearRoutelessPlayer
// @0x827844D8, and GetAICar from BrnResetOnTrackManager.cpp.
//
// Normalise3D's three console sites are all `vmsum3fp128 ; vrsqrtefp ; two Newton-Raphson steps ; vmulfp128` with
// no vcmpeqfp/vsel: ResetNearRoutelessPlayer 0x8278462C..0x82784684 (the reset direction, next portal - prev
// portal) and 0x827846CC..0x82784704 (the player offset, screened by the FLT_EPSILON IsZero at 0x827846AC..
// 0x827846C8 first), ConvertNodesToPositionAndDirection 0x827905A8..0x827905EC (screened by IsSimilar2D). The chain:
//   y0 = vrsqrtefp(lenSq) ; e = -(lenSq * (y*y) - 1.0) (vnmsubfp, fused) ; y' = (y * 0.5) * e + y (vmaddfp, fused),
//   twice ; out = v * y2.
// For lenSq = +0, vrsqrtefp gives +inf, 0 * inf = NaN, so every output lane is NaN. The old helper answered
// (0, 0, 0) and credited the guard to ResetAwayFromPlayer 0x827842C8/0x827842F4 -- that vcmpeqfp128/vsel pair guards
// a MAGNITUDE (d2 * rsqrt(d2)), not a normalise.
//
// ResetNearRoutelessPlayer with a degenerate portal pair (start portal == end portal -- the console's own
// ":1199 !IsSimilar(lNextPortal, lPrevPortal)" assert fires and does not gate): the console stores
// Normalise(0) = NaN lanes as lpResetData->mDirection (+0x20, 0x82784688); the old body stored (0, 0, 0).
// Only degenerate portal data reaches it (FX-AINAN2 / FX-AIRESET), so it is unreachable with the shipped AI.DAT.
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/BrnAIPortal.h"
#include "GameSource/World/AI/Route/BrnRoute.h"
#include "GameSource/World/AI/Route/BrnRouteMapModule.h"
#include "GameSource/World/AI/RacingLine/BrnRacingLineGenerator.h"
#include "GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.h"
#include "SharedClasses/AI/AISectionsResourceType.h"
#include "GameSource/Math/BrnMathUtils.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

static unsigned gAssertions = 0;
namespace CgsDev {
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { u64 gxMessageFilterFlags = 0; }
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++gAssertions; return 0; }
void* EndAssert() { return nullptr; }
} }
namespace CgsResource {
BaseResourcePtr::BaseResourcePtr() {}
BaseResourcePtr::~BaseResourcePtr() {}
}

namespace BrnAI {
Vector3 AICar::GetPosition() const { return mPosition; }
Vector3 AICar::GetDirection() const { return mDirection; }
const AISection* AISectionsData::GetAISection(u32 luIndex) const { return &mpaSections[luIndex]; }
const Portal* AISection::GetPortal(u8 luPortalIndex) const { return &mpaPortals[luPortalIndex]; }
f32 Portal::GetPositionX() const { return mPositionX; }
f32 Portal::GetPositionY() const { return mPositionY; }
f32 Portal::GetPositionZ() const { return mPositionZ; }
}

#include "restored_methods.inc"

using namespace BrnAI;

namespace
{
    unsigned guChecks = 0, guFailures = 0;
    void Check(bool lbPass, const char* lpcLabel)
    {
        ++guChecks;
        std::printf("%s  %s\n", lbPass ? "PASS" : "FAIL", lpcLabel);
        if (!lbPass) { ++guFailures; }
    }
    bool IsNaN(f32 lfValue) { return lfValue != lfValue; }
    bool Same(f32 lfA, f32 lfB) { return std::memcmp(&lfA, &lfB, sizeof(f32)) == 0; }

    // The console chain, operation for operation (0x82784658..0x82784684). A nonzero lenSq's estimate is taken as
    // the exact 1/sqrt; only NaN-ness is compared.
    f32 ConsoleRsqrtNewton(f32 lfLenSq)
    {
        f32 lfY = (lfLenSq == 0.0f) ? std::numeric_limits<f32>::infinity() : 1.0f / std::sqrt(lfLenSq);
        for (int li = 0; li < 2; ++li)
        {
            const f32 lfYY   = lfY * lfY;
            const f32 lfHalf = lfY * 0.5f;
            const f32 lfE    = -std::fmaf(lfLenSq, lfYY, -1.0f);
            lfY = std::fmaf(lfHalf, lfE, lfY);
        }
        return lfY;
    }

    AISection      saSections[1];
    Portal         saPortals[2];
    AISectionsData sData;
    AICar          saCars[1];
    ResetOnTrackManager sManager;

    // One section, two portals; the player car at the origin facing +Z.
    void World(Vector3 lPrev, Vector3 lNext)
    {
        std::memset(saSections, 0, sizeof(saSections));
        std::memset(saPortals, 0, sizeof(saPortals));
        saSections[0].mpaPortals = saPortals;
        saSections[0].mu8NumPortals = 2;
        saPortals[0].mPositionX = lPrev.x; saPortals[0].mPositionY = lPrev.y; saPortals[0].mPositionZ = lPrev.z;
        saPortals[1].mPositionX = lNext.x; saPortals[1].mPositionY = lNext.y; saPortals[1].mPositionZ = lNext.z;
        sData.mpaSections = saSections;
        sData.muNumSections = 1;
        sManager.mpAISectionData.mpResourceMemory = &sData;
        sManager.mpaAICars = saCars;
        sManager.mePlayerGlobalRaceCarIndex = E_GLOBAL_RACE_CAR_INDEX_0;
        AICar& lrCar = saCars[0];
        lrCar.mPosition  = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
        lrCar.mDirection = Vector3{ 0.0f, 0.0f, 1.0f, 0.0f };
        lrCar.muResetOnTrackSectionIndex = 0;
        lrCar.muResetOnTrackStartPortal  = 0;
        lrCar.muResetOnTrackEndPortal    = 1;
        gAssertions = 0;
    }
}

int main()
{
    // ---- Normalise3D itself ---------------------------------------------------------------------------------------
    {
        Check(IsNaN(0.0f * ConsoleRsqrtNewton(0.0f)), "model: the console chain turns lenSq = +0 into NaN");
        const Vector3 lA = TestNormalise3D(Vector3{ 0.0f, 0.0f, 0.0f, 0.0f });
        Check(IsNaN(lA.x) && IsNaN(lA.y) && IsNaN(lA.z), "Normalise3D(0, 0, 0) is NaN in every lane, as the console");
        const Vector3 lB = TestNormalise3D(Vector3{ -0.0f, 0.0f, -0.0f, 0.0f });
        Check(IsNaN(lB.x) && IsNaN(lB.y) && IsNaN(lB.z), "Normalise3D(-0, 0, -0) squares to +0 and is NaN too");

        const f32 laInputs[][3] = { { 0.0f, 0.0f, 10.0f }, { 3.0f, -4.0f, 12.0f }, { 1.0e-3f, 2.0e-3f, -5.0e-4f },
                                    { 2208.7f, -8.4f, -2369.8f } };
        bool lbSame = true;
        for (const auto& lrIn : laInputs)
        {
            const Vector3 lV = { lrIn[0], lrIn[1], lrIn[2], 0.0f };
            const f32 lfInv = 1.0f / sqrtf(lV.x * lV.x + lV.y * lV.y + lV.z * lV.z);   // the pre-fix nonzero arm
            const Vector3 lN = TestNormalise3D(lV);
            lbSame = lbSame && Same(lN.x, lV.x * lfInv) && Same(lN.y, lV.y * lfInv) && Same(lN.z, lV.z * lfInv);
        }
        Check(lbSame, "nonzero vectors: v * (1 / sqrtf(lenSq)) bit for bit, as before the fix");
    }

    // ---- ResetNearRoutelessPlayer @0x827844D8: a degenerate portal pair -------------------------------------------
    {
        World(Vector3{ 50.0f, 0.0f, 60.0f, 0.0f }, Vector3{ 50.0f, 0.0f, 60.0f, 0.0f });
        ResetOnTrackManager::ResetOnTrackCoords lCoords;
        std::memset(&lCoords, 0, sizeof(lCoords));
        const bool lbOk = sManager.ResetNearRoutelessPlayer(&lCoords);
        Check(lbOk, "degenerate pair: the strategy still succeeds (the :1199 IsSimilar assert does not gate)");
        Check(gAssertions == 1, "degenerate pair: the console's :1199 assert fires once (0x8278460C)");
        Check(IsNaN(lCoords.mDirection.x) && IsNaN(lCoords.mDirection.y) && IsNaN(lCoords.mDirection.z),
              "degenerate pair: mDirection = Normalise(0) = NaN lanes (0x8278462C..0x82784688), was (0, 0, 0)");
        Check(Same(lCoords.mPosition.x, 0.0f) && Same(lCoords.mPosition.y, 0.0f) && Same(lCoords.mPosition.z, -20.0f),
              "degenerate pair: the portal is ahead of the player -> forced 20 m (flt_820C4890) behind (control)");
    }
    {
        World(Vector3{ 0.0f, 0.0f, -40.0f, 0.0f }, Vector3{ 0.0f, 0.0f, -30.0f, 0.0f });
        ResetOnTrackManager::ResetOnTrackCoords lCoords;
        std::memset(&lCoords, 0, sizeof(lCoords));
        Check(sManager.ResetNearRoutelessPlayer(&lCoords) && gAssertions == 0,
              "control: an ordinary portal pair succeeds with no assert");
        Check(Same(lCoords.mDirection.x, 0.0f) && Same(lCoords.mDirection.y, 0.0f) && Same(lCoords.mDirection.z, 1.0f),
              "control: mDirection = (0, 0, 1)");
        Check(Same(lCoords.mPosition.z, -40.0f), "control: behind the player -> the start portal is kept");
    }

    std::printf("FxTailsAResetNormalise: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
