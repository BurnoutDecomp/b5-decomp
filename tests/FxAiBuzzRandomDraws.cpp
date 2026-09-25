// FX-AIBUZZ reviewer-G item (a) (crash parity 2026-09-25): CgsNumeric::Random's two BOUNDED draws fuse their range
// map like the console, run on the PRODUCTION bodies (RandomFloat(min, max) and RandomVector(Vector3, Vector3),
// extracted from CgsRandom.cpp by run_fxaibuzz_random_draws.py) on the real Random layout.
//
// The console inlines both at every call site; the combine is one multiply-add that rounds ONCE:
//   scalar  fsubs (max - min) ; fmadds D, range, t, min   (D = range * t + min)
//             CameraShake::Update          @0x822213BC   BehaviourFixedCam (dutch)       @0x8222A0F8
//             ParticleModule (rot. speed)  @0x82281AD0   ClutchControl  @0x826CDFC0 and @0x826CE07C
//   vector  vsubfp v6, v10, v12 @0x82221570 ; vmaddfp v12, v8, v12, v6 @0x822215CC (CameraShake::Update).
//             IDA prints classic vmaddfp in raw field order D, A, B, C = A * C + B, so this is
//             v12 = t * (max - min) + min on all four lanes (w included).
// t = ring slot - 1.0 (exact: the slot holds a [1, 2) float). The PC used to write `(max - min) * t + min`, which
// rounds the product first. Every counterexample below is an input where one rounding and two differ. The expected
// bits are the exactly rounded range * t + min (rational arithmetic, round-to-nearest-even, checked against numpy on
// 200000 doubles); the scratch generator is gen_random_fma.py / gen_random_rows.py (FX-AIBUZZ scratchpad). The two
// scalar controls (a power-of-two range, a zero minimum) round the same either way. Every draw also re-checks the
// ring / seed / cursor it leaves behind, so the untouched part of each body stays pinned.
#include "types.hpp"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "rw/math/vpu/types.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace CgsNumeric
{
// The production bounded draws, extracted verbatim.
#include "fxaibuzz_random_draws.inc"
}

static unsigned gChecks = 0, gFailures = 0;

static void Check(bool lbPassed, const char* lpcLabel)
{
    ++gChecks;
    if (!lbPassed)
    {
        ++gFailures;
        std::printf("FAIL  %s\n", lpcLabel);
    }
}

static f32 Float(u32 lu) { f32 lf; std::memcpy(&lf, &lu, 4); return lf; }
static u32 Bits(f32 lf) { u32 lu; std::memcpy(&lu, &lf, 4); return lu; }

// { min bits, max bits, ring slot bits (t = slot - 1.0), exactly rounded range * t + min }
struct DrawCase { u32 muMin, muMax, muRing, muExpected; };

static const DrawCase kaScalarCases[] = {
    { 0xC0A00000u, 0x40A00000u, 0x3FB33335u, 0xBF7FFFDCu },  // (-5, 5) t 0.400000215: one rounding 0xBF7FFFDC, two 0xBF7FFFE0
    { 0x3F800000u, 0x3FA66666u, 0x3FA468C0u, 0x3F8AEC39u },  // (1, 1.3) t 0.284446716: one 0x3F8AEC39, two 0x3F8AEC3A
    { 0xBEB33333u, 0x3EB33333u, 0x3FB69D05u, 0xBD5242C3u },  // (-0.35, 0.35) t 0.426666856: one 0xBD5242C3, two 0xBD5242C0
    { 0x3F800000u, 0x3F933333u, 0x3FC8D180u, 0x3F8AEC39u },  // (1, 1.15) t 0.568893433: one 0x3F8AEC39, two 0x3F8AEC3A
    { 0x40000000u, 0x40E9999Au, 0x3FDB05B3u, 0x40B89AC1u },  // (2, 7.3) t 0.711111426: one 0x40B89AC1, two 0x40B89AC0
    { 0xC1490FDBu, 0x41C90FDBu, 0x3FED3A0Au, 0x419CD413u },  // (-4pi, 8pi) t 0.853333712: one 0x419CD413, two 0x419CD412
    // The one bounded draw whose value reaches sim time: MomentHardStop's ultra slo-mo timestep scale,
    // RandomFloat(KF_ULTRA_SLOMO_TIMESTEP_MIN_SCALE 0.005, _MAX_SCALE 0.01) (BrnMomentHardStop.cpp:315 -> the
    // camera effects' mfSimTimeScale, :416). 1,363,583 of the 2^23 ring values round differently there. These two
    // sit at the dtSim ratios FX-WITNESS measured on a forced ultra slo-mo crash (0.0058, 0.0079).
    { 0x3BA3D70Au, 0x3C23D70Au, 0x3F947AE1u, 0x3BBE0DEDu },  // HardStop t 0.159999967 -> 0.0058: one 0x3BBE0DED, two 0x3BBE0DEC
    { 0x3BA3D70Au, 0x3C23D70Au, 0x3FCA3D7Cu, 0x3C016F07u },  // HardStop t 0.580001354 -> 0.0079: one 0x3C016F07, two 0x3C016F08
    { 0xBF800000u, 0x3F800000u, 0x3FA468ACu, 0xBEDCBAA0u },  // control (-1, 1) t 0.284444332: both 0xBEDCBAA0
    { 0x00000000u, 0x3DCCCCCDu, 0x3FC8D158u, 0x3D69044Du },  // control (0, 0.1) t 0.568888664: both 0x3D69044D
};
static const u32 KU_SCALAR_COUNTEREXAMPLES = 8;

static const DrawCase kaVectorCases[3][4] = {
    { { 0xC0400000u, 0x40400000u, 0x3FD55557u, 0x3F80000Au },    // x (-3, 3): one 0x3F80000A, two 0x3F800008
      { 0xC0200000u, 0x40200000u, 0x3FB33335u, 0xBEFFFFDCu },    // y (-2.5, 2.5): one 0xBEFFFFDC, two 0xBEFFFFE0
      { 0xBF333333u, 0x3F333333u, 0x3F8B6DBBu, 0xBF133327u },    // z (-0.7, 0.7): one 0xBF133327, two 0xBF133328
      { 0x3F800000u, 0x3FA66666u, 0x3F9AAABAu, 0x3F880005u } },  // w (1, 1.3): one 0x3F880005, two 0x3F880004
    { { 0x3DCCCCCDu, 0x3F666666u, 0x3F8F6014u, 0x3E48CD4Du },    // x (0.1, 0.9): one 0x3E48CD4D, two 0x3E48CD4C
      { 0xC0E80000u, 0x40600000u, 0x3F92735Bu, 0xC0B669FBu },    // y (-7.25, 3.5): one 0xC0B669FB, two 0xC0B669FC
      { 0x40800000u, 0x41100000u, 0x3FE6666Fu, 0x41000005u },    // z (4, 9): one 0x41000005, two 0x41000006
      { 0xBE99999Au, 0x3E800000u, 0x3F9899B8u, 0xBE46F53Du } },  // w (-0.3, 0.25): one 0xBE46F53D, two 0xBE46F53E
    { { 0xBFC00000u, 0x3FC00000u, 0x3FD55557u, 0x3F00000Au },    // x (-1.5, 1.5): one 0x3F00000A, two 0x3F000008
      { 0x3F000000u, 0x40100000u, 0x3FEDB6DDu, 0x40000001u },    // y (0.5, 2.25): one 0x40000001, two 0x40000002
      { 0xC2200000u, 0x42200000u, 0x3FB33335u, 0xC0FFFFDCu },    // z (-40, 40): one 0xC0FFFFDC, two 0xC0FFFFE0
      { 0x40400000u, 0x406CCCCDu, 0x3FA4E697u, 0x404CEA4Fu } },  // w (3, 3.7): one 0x404CEA4F, two 0x404CEA4E
};

static const u64 KU_K = 0x5851F42D4C957F2Dull;   // the LCG multiplier every inlined site builds (lis/ori/insrdi)
static const u32 KU_ONE = 0x3F800000u;

// A ring of distinct [1, 2) words, so a stray write to the wrong slot is visible.
static void Fill(CgsNumeric::Random& lrRandom, u32 luSalt)
{
    for (u32 i = 0; i < 8; ++i)
        lrRandom.mauIntegerBuffer[i] = KU_ONE | ((0x00012345u * (i + 1) + luSalt) & 0x7FFFFFu);
}

int main()
{
    // ---- RandomFloat(min, max): fmadds D, (max - min), t, min ------------------------------------------------------
    for (u32 c = 0; c < sizeof(kaScalarCases) / sizeof(kaScalarCases[0]); ++c)
    {
        const DrawCase& lrCase = kaScalarCases[c];
        CgsNumeric::Random lRandom;
        Fill(lRandom, c * 0x1111u);
        const u32 luIndex = (c * 3u + 1u) & 7u;
        const u64 luSeed = 0x0123456789ABCDEFull + 0x9E3779B97F4A7C15ull * c;
        lRandom.muSeed = luSeed;
        lRandom.muOldestBufferIndex = luIndex;
        lRandom.mauIntegerBuffer[luIndex] = lrCase.muRing;
        u32 lauBefore[8];
        std::memcpy(lauBefore, lRandom.mauIntegerBuffer, sizeof(lauBefore));

        const f32 lfResult = lRandom.RandomFloat(Float(lrCase.muMin), Float(lrCase.muMax));

        char lacLabel[200];
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "RandomFloat(%.9g, %.9g), t %.9g %s: got 0x%08X, want 0x%08X (fmadds, one rounding)",
                      Float(lrCase.muMin), Float(lrCase.muMax), Float(lrCase.muRing) - 1.0f,
                      c < KU_SCALAR_COUNTEREXAMPLES ? "[counterexample]" : "[control]", Bits(lfResult),
                      lrCase.muExpected);
        Check(Bits(lfResult) == lrCase.muExpected, lacLabel);

        bool lbState = lRandom.mauIntegerBuffer[luIndex] == (KU_ONE | (static_cast<u32>(luSeed >> 32) >> 9))
                       && lRandom.muSeed == luSeed * KU_K + 1u
                       && lRandom.muOldestBufferIndex == ((luIndex + 1u) & 7u);
        for (u32 i = 0; i < 8; ++i)
            lbState = lbState && (i == luIndex || lRandom.mauIntegerBuffer[i] == lauBefore[i]);
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "RandomFloat(min, max) case %u: refills the CURRENT slot from the old seed, steps the LCG, "
                      "bumps the cursor, touches nothing else", c);
        Check(lbState, lacLabel);
    }

    // ---- RandomVector(min, max): vmaddfp v12, v8, v12, v6 == t * (max - min) + min, per lane -----------------------
    for (u32 c = 0; c < 3; ++c)
    {
        CgsNumeric::Random lRandom;
        Fill(lRandom, 0x5A5A5u + c);
        const u32 luIndex = (c * 5u + 1u) & 7u;
        const u32 luSlot = (luIndex + 3u) & 4u;
        const u64 luSeed0 = 0xFEDCBA9876543210ull + 0x2545F4914F6CDD1Dull * c;
        lRandom.muSeed = luSeed0;
        lRandom.muOldestBufferIndex = luIndex;
        rw::math::vpu::Vector3 lMin, lMax;
        f32* lpfMin = &lMin.x;
        f32* lpfMax = &lMax.x;
        for (u32 l = 0; l < 4; ++l)
        {
            lRandom.mauIntegerBuffer[luSlot + l] = kaVectorCases[c][l].muRing;
            lpfMin[l] = Float(kaVectorCases[c][l].muMin);
            lpfMax[l] = Float(kaVectorCases[c][l].muMax);
        }
        u32 lauBefore[8];
        std::memcpy(lauBefore, lRandom.mauIntegerBuffer, sizeof(lauBefore));

        const rw::math::vpu::Vector3 lResult = lRandom.RandomVector(lMin, lMax);

        const f32* lpfResult = &lResult.x;
        for (u32 l = 0; l < 4; ++l)
        {
            char lacLabel[200];
            std::snprintf(lacLabel, sizeof(lacLabel),
                          "RandomVector case %u lane %c: (%.9g, %.9g), t %.9g: got 0x%08X, want 0x%08X (vmaddfp, "
                          "one rounding) [counterexample]", c, "xyzw"[l], lpfMin[l], lpfMax[l],
                          Float(kaVectorCases[c][l].muRing) - 1.0f, Bits(lpfResult[l]), kaVectorCases[c][l].muExpected);
            Check(Bits(lpfResult[l]) == kaVectorCases[c][l].muExpected, lacLabel);
        }

        const u64 luSeed1 = luSeed0 * KU_K + 1u;
        const u32 luHi0 = static_cast<u32>(luSeed0 >> 32), luHi1 = static_cast<u32>(luSeed1 >> 32);
        bool lbState = lRandom.mauIntegerBuffer[luSlot + 0] == (KU_ONE | ((luHi1 << 2) & 0x7FFFFCu))
                       && lRandom.mauIntegerBuffer[luSlot + 1] == (KU_ONE | ((luHi0 << 13) & 0x7FE000u) | (luHi1 >> 19))
                       && lRandom.mauIntegerBuffer[luSlot + 2] == (KU_ONE | (luHi0 >> 9))
                       && lRandom.muSeed == luSeed1 * KU_K + 1u
                       && lRandom.muOldestBufferIndex == luSlot + 3u;
        for (u32 i = 0; i < 8; ++i)
            lbState = lbState && ((i >= luSlot && i < luSlot + 3u) || lRandom.mauIntegerBuffer[i] == lauBefore[i]);
        char lacLabel[200];
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "RandomVector case %u: two LCG steps packed across slot+0..2, lane w's word left alone, "
                      "cursor slot + 3", c);
        Check(lbState, lacLabel);
    }

    std::printf("FxAiBuzzRandomDraws: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
