// L6 AIDRIVE regression (owner list 2026-09-27): ROUNDING_RULE rule 3 in the AI steering fan. The production
// bodies are extracted verbatim by run_l6_fan_rounding.py:
//   * InterpolateTraffic -- SteeringFan::Interpolate as IncludeConstantBearing @0x827873A0 inlines it twice:
//       `fsubs f0, new, old ; fmadds f0, f0, f20 (0.2), old`  @0x82787890/0x82787894 and 0x827878A0/0x827878A4
//   * SteeringFan::IncludeHardNoGo @0x82779D98 -- the ExitHNG rescale into [0.5, 1]:
//       `fsubs f11, v, min ; fmadds f11, f11, f12 (scale), f13 (0.5)`  x17 (0x8277A108 .. 0x8277A1E0)
//   * SteeringFan::AccumulateWeightings @0x82779088 -- the fold of the 14 rows by kfBias:
//       `fmadds acc, weight, bias, acc`  x17 (0x827790FC .. 0x82779194), then the 0.5 lerp (0x82779208 ..)
//   * SteeringFan::FanIntersectsEdge @0x8277A208 -- the three 2D crosses:
//       `fmuls p, a, b ; fmsubs r, c, d, p`  (0x8277A260, 0x8277A2AC, 0x8277A2CC)
// Each fused op rounds ONCE (std::fmaf with the console's operand order); the PC used to round the product and
// the sum separately. The inputs were chosen so the two spellings differ, and every group checks that first,
// so a check that cannot discriminate fails loudly instead of passing vacuously.
// Racing-line section queries and HardNoGoMap::DistanceToHardNoGoEdge are fixtures answering configured values,
// exactly as FxAinan2SteeringFan.cpp configures them. The real CgsStrStream.cpp is linked (the [DIAG] streams).
#define BRN_AI_STEERINGFAN_HNG_PRESENT 1
#include "GameSource/World/AI/RacingLine/BrnAISteeringFan.h"
#include "GameSource/World/AI/RacingLine/BrnHardNoGoMap.h"
#include "GameSource/World/AI/RacingLine/BrnRacingLineGenerator.h"
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/BrnAIDriver.h"
#include "GameSource/World/AI/Route/BrnRacingLine.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static unsigned gAssertions = 0;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++gAssertions; return 0; }
void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
}

static f32 gafHngDistance[17] = {};
static bool gabHngInside[17] = {};
static unsigned guHngCall = 0;
namespace BrnAI {
static SectionData gSectionData;
s32 RacingLineGenerator::GetLocalSectionID(RacingLine*, Vector2, s32) { return 5; }
s32 RacingLineGenerator::GetNearSectionID(RacingLine*, Vector2, s32) { return 5; }
SectionData* RacingLineGenerator::GetSectionPointer(RacingLine*, s32) { return &gSectionData; }
bool HardNoGoMap::DistanceToHardNoGoEdge(Vector2, f32& lrfDistance)
{
    const unsigned luStep = guHngCall++ % 17u;
    lrfDistance = gafHngDistance[luStep];
    return gabHngInside[luStep];
}
}

#include "l6_fan_rounding.inc"

using namespace BrnAI;

namespace
{
    unsigned guChecks = 0, guFailures = 0;
    void Check(bool lbPass, const char* lpcLabel)
    {
        ++guChecks;
        if (!lbPass) { ++guFailures; std::printf("FAIL %s\n", lpcLabel); }
    }
    u32 Bits(f32 lfValue) { u32 luBits; std::memcpy(&luBits, &lfValue, 4); return luBits; }
    f32 FromBits(u32 luBits) { f32 lfValue; std::memcpy(&lfValue, &luBits, 4); return lfValue; }
    bool Same(f32 lfA, f32 lfB) { return Bits(lfA) == Bits(lfB); }
    Vector2 V2(f32 lfX, f32 lfY) { Vector2 lV{}; lV.x = lfX; lV.y = lfY; return lV; }

    // The PC's old spellings, kept here only to prove each input discriminates.
    f32 TwoRoundings(f32 lfA, f32 lfB, f32 lfC)
    {
        volatile f32 lfProduct = lfA * lfB;   // volatile: no contraction, one rounding per operation
        return lfProduct + lfC;
    }
    f32 CrossTwice(f32 lfC, f32 lfD, f32 lfA, f32 lfB)   // c*d - a*b, both products rounded
    {
        volatile f32 lfFirst = lfC * lfD;
        volatile f32 lfSecond = lfA * lfB;
        return lfFirst - lfSecond;
    }

    SteeringFan gFan;
    RacingLine  gLine;
}

// ---- IncludeConstantBearing's two row lerps: InterpolateTraffic(old, new, 0.2) ------------------
static void GroupInterpolateTraffic()
{
    struct Case { f32 mfFrom; f32 mfTo; const char* mpcLabel; };
    const Case laCases[] = {
        { 0.1f,  0.8f,  "InterpolateTraffic(0.1, 0.8, 0.2) is one rounding of (0.8 - 0.1) * 0.2 + 0.1 (fmadds 0x82787894)" },
        { 0.1f,  0.55f, "InterpolateTraffic(0.1, 0.55, 0.2) is one rounding (fmadds 0x82787894)" },
        { 0.9f,  0.47f, "InterpolateTraffic(0.9, 0.47, 0.2) -- a DECAYING row -- is one rounding (fmadds 0x827878A4)" },
        { 0.33f, 1.0f,  "InterpolateTraffic(0.33, 1.0, 0.2) is one rounding (fmadds 0x827878A4)" },
    };
    const f32 lfTime = 0.2f;   // f20 = flt_820C4300
    for (const Case& lrCase : laCases)
    {
        const f32 lfDelta = lrCase.mfTo - lrCase.mfFrom;   // the fsubs, rounded on its own
        const f32 lfFused = std::fmaf(lfDelta, lfTime, lrCase.mfFrom);
        Check(!Same(lfFused, TwoRoundings(lfDelta, lfTime, lrCase.mfFrom)),
              "self-check: this input separates one rounding from two");
        Check(Same(InterpolateTraffic(lrCase.mfFrom, lrCase.mfTo, lfTime), lfFused), lrCase.mpcLabel);
    }
    // controls: exact arithmetic is the same either way
    Check(Same(InterpolateTraffic(0.0f, 1.0f, lfTime), 0.2f), "control: 0 -> 1 at 0.2 is 0.2");
    Check(Same(InterpolateTraffic(0.5f, 0.5f, lfTime), 0.5f), "control: a steady row stays put");
}

// ---- IncludeHardNoGo's ExitHNG rescale ------------------------------------------------------------
static void GroupHardNoGoRescale()
{
    // every ray inside the no-go map; distances 0.3 + 0.07 i as exact f32 bit patterns
    static const u32 kauDistanceBits[17] = {
        0x3e99999a, 0x3ebd70a4, 0x3ee147ae, 0x3f028f5c, 0x3f147ae1, 0x3f266666, 0x3f3851ec, 0x3f4a3d71,
        0x3f5c28f6, 0x3f6e147b, 0x3f800000, 0x3f88f5c3, 0x3f91eb85, 0x3f9ae148, 0x3fa3d70a, 0x3faccccd,
        0x3fb5c28f };
    std::memset(&gFan, 0, sizeof(gFan));
    std::memset(&gLine, 0, sizeof(gLine));
    gLine.mbIsInitialised = true;
    gLine.miLastKnownSectionID = 5;
    gSectionData.mHardNoGoMap.mbReady = true;
    for (s32 liStep = 0; liStep < 17; ++liStep)
    {
        gafHngDistance[liStep] = FromBits(kauDistanceBits[liStep]);
        gabHngInside[liStep] = true;
    }
    guHngCall = 0;
    gFan.IncludeHardNoGo(nullptr, &gLine);

    const f32 lfMinimum = gafHngDistance[0];
    const f32 lfMaximum = gafHngDistance[16];
    const f32 lfScale   = (1.0f / (lfMaximum - lfMinimum)) * 0.5f;   // fdivs + fmuls, two roundings
    s32 liDiscriminating = 0;
    bool lbAllFused = true;
    for (s32 liStep = 0; liStep < 17; ++liStep)
    {
        const f32 lfOffset = gafHngDistance[liStep] - lfMinimum;
        const f32 lfFused  = std::fmaf(lfOffset, lfScale, 0.5f);
        if (!Same(lfFused, TwoRoundings(lfOffset, lfScale, 0.5f)))
            ++liDiscriminating;
        if (!Same(gFan.mfWeighting[eFan_ExitHNG][liStep], lfFused))
        {
            lbAllFused = false;
            std::printf("  step %d: row %.9g, one rounding %.9g\n", liStep,
                        gFan.mfWeighting[eFan_ExitHNG][liStep], lfFused);
        }
    }
    Check(liDiscriminating == 8, "self-check: 8 of the 17 rescales separate one rounding from two");
    Check(lbAllFused, "IncludeHardNoGo: every ExitHNG rescale is one rounding of (v - min) * scale + 0.5 "
                      "(fmadds 0x8277A108 .. 0x8277A1E0)");
    Check(Same(gFan.mfWeighting[eFan_ExitHNG][0], 0.5f), "control: the nearest wall rescales to exactly 0.5");
    Check(Same(gFan.mfWeighting[eFan_AvoidHNG][3], 0.0f) && Same(gFan.mfWeighting[eFan_FavourHNGDanger][3], 0.0f),
          "control: inside the map the AvoidHNG / FavourHNGDanger rows are 0");
}

// ---- AccumulateWeightings: the kfBias fold and the 0.5 lerp -----------------------------------------
static void GroupAccumulateWeightings()
{
    std::memset(&gFan, 0, sizeof(gFan));
    gFan.meBiasMode = eBiasMode_Race;
    u32 luSeed = 0x0927A11Au;
    for (s32 liRow = 0; liRow < E_FAN_CONTRIBUTORS_COUNT; ++liRow)
        for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
        {
            luSeed = luSeed * 1664525u + 1013904223u;                      // a fixed LCG: same rows old / new
            gFan.mfWeighting[liRow][liStep] = static_cast<f32>(luSeed >> 8) * (1.2f / 16777216.0f);
        }
    f32 lafCumulativeBefore[KI_FAN_STEPS];
    for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
    {
        lafCumulativeBefore[liStep] = 3.0f + 0.1f * static_cast<f32>(liStep);
        gFan.mfCumulativeWeighting[liStep] = lafCumulativeBefore[liStep];
    }
    gFan.AccumulateWeightings();

    s32 liDiscriminating = 0;
    bool lbAllFused = true;
    for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
    {
        f32 lfFused = 0.0f, lfTwice = 0.0f;
        for (s32 liRow = 0; liRow < E_FAN_CONTRIBUTORS_COUNT; ++liRow)
        {
            const f32 lfBias = kfBias[eBiasMode_Race][liRow];
            if (lfBias == 0.0f)
                continue;
            lfFused = std::fmaf(gFan.mfWeighting[liRow][liStep], lfBias, lfFused);
            lfTwice = TwoRoundings(gFan.mfWeighting[liRow][liStep], lfBias, lfTwice);
        }
        if (!Same(lfFused, lfTwice))
            ++liDiscriminating;
        const f32 lfCumulative = std::fmaf(lfFused - lafCumulativeBefore[liStep], 0.5f, lafCumulativeBefore[liStep]);
        if (!Same(gFan.mfCumulativeWeighting[liStep], lfCumulative))
        {
            lbAllFused = false;
            std::printf("  step %d: cumulative %.9g, one-rounding fold %.9g\n", liStep,
                        gFan.mfCumulativeWeighting[liStep], lfCumulative);
        }
    }
    Check(liDiscriminating > 0, "self-check: the race-mode fold of these rows separates one rounding from two");
    Check(lbAllFused, "AccumulateWeightings: every kfBias fold is one rounding of weight * bias + acc "
                      "(fmadds 0x827790FC .. 0x82779194), then the 0.5 lerp (fmadds 0x82779208 ..)");
}

// ---- FanIntersectsEdge: the three fused crosses --------------------------------------------------------
static void GroupFanIntersectsEdge()
{
    struct Case { u32 mau[8]; };   // edge0.x, edge0.y, edge1.x, edge1.y, A.x, A.y, B.x, B.y
    static const Case kaCases[] = {
        { { 0xc1ebd28e, 0x40d6aad6, 0x41b5f58b, 0xc1c02c62, 0xc2311ecb, 0xc17a044f, 0xc213004c, 0xc110bac5 } },
        { { 0xc0f149cc, 0xc1bc2d21, 0xc20e560f, 0x42181dce, 0xc1962b50, 0xc0506337, 0xc15e3f31, 0xc13fbed4 } },
        { { 0x41439ba3, 0xc2057ffd, 0x424e6c2e, 0xc20eb7b2, 0x41006754, 0x4006d490, 0x415ef352, 0xc0bebd2e } },
    };
    s32 liCase = 0;
    for (const Case& lrCase : kaCases)
    {
        f32 laf[8];
        for (s32 i = 0; i < 8; ++i) laf[i] = FromBits(lrCase.mau[i]);
        Vector2 laEdge[2] = { V2(laf[0], laf[1]), V2(laf[2], laf[3]) };
        const Vector2 lA = V2(laf[4], laf[5]);
        const Vector2 lB = V2(laf[6], laf[7]);

        // the console's sequence, rule 3 at the three crosses (the |D| root is the PC's exact sqrt either way)
        const f32 lfDx = lB.x - lA.x, lfDy = lB.y - lA.y;
        const f32 lfEx = laEdge[1].x - laEdge[0].x, lfEy = laEdge[1].y - laEdge[0].y;
        const f32 lfRx = laEdge[0].x - lA.x, lfRy = laEdge[0].y - lA.y;
        const f32 lfDen = std::fmaf(lfDy, lfEx, -(lfDx * lfEy));
        const f32 lfRecip = 1.0f / lfDen;
        const f32 lfRay = std::fmaf(lfRy, lfEx, -(lfRx * lfEy)) * lfRecip;
        const f32 lfLengthSq = lfDx * lfDx + lfDy * lfDy;
        const f32 lfExpected = lfRay * std::sqrt(lfLengthSq);

        const f32 lfDenTwice = CrossTwice(lfDy, lfEx, lfDx, lfEy);
        const f32 lfRecipTwice = 1.0f / lfDenTwice;
        const f32 lfRayTwice = CrossTwice(lfRy, lfEx, lfRx, lfEy) * lfRecipTwice;
        const f32 lfTwice = lfRayTwice * std::sqrt(lfLengthSq);

        char lacLabel[200];
        std::snprintf(lacLabel, sizeof(lacLabel), "self-check: edge case %d separates one rounding from two", liCase);
        Check(!Same(lfExpected, lfTwice), lacLabel);
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "FanIntersectsEdge case %d: the crossing distance uses the fused crosses "
                      "(fmsubs 0x8277A260 / 0x8277A2AC / 0x8277A2CC)", liCase);
        const f32 lfGot = gFan.FanIntersectsEdge(laEdge, 0, lA, lB);
        if (!Same(lfGot, lfExpected))
            std::printf("  case %d: got %.9g, one rounding %.9g, two %.9g\n", liCase, lfGot, lfExpected, lfTwice);
        Check(Same(lfGot, lfExpected), lacLabel);
        ++liCase;
    }
    // controls: a parallel edge and a crossing short of the target both answer -1
    Vector2 laParallel[2] = { V2(0.0f, 5.0f), V2(10.0f, 5.0f) };
    Check(gFan.FanIntersectsEdge(laParallel, 0, V2(0.0f, 0.0f), V2(10.0f, 0.0f)) == -1.0f,
          "control: a parallel edge is no intersection (-1)");
    Vector2 laShort[2] = { V2(-5.0f, 2.0f), V2(5.0f, 2.0f) };
    Check(gFan.FanIntersectsEdge(laShort, 0, V2(0.0f, 0.0f), V2(0.0f, 10.0f)) == -1.0f,
          "control: a crossing short of the fan target is no intersection (-1)");
    Vector2 laAhead[2] = { V2(-5.0f, 20.0f), V2(5.0f, 20.0f) };
    Check(gFan.FanIntersectsEdge(laAhead, 0, V2(0.0f, 0.0f), V2(0.0f, 10.0f)) == 20.0f,
          "control: an edge 20 m ahead along the ray answers 20 m");
}

int main()
{
    GroupInterpolateTraffic();
    GroupHardNoGoRescale();
    GroupAccumulateWeightings();
    GroupFanIntersectsEdge();
    Check(gAssertions == 0, "no assertion fired");
    std::printf("L6FanRounding: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
