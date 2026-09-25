// FX-AIBUZZ reviewer-G item (b) (crash parity 2026-09-25): the PathLine output write rounds ONCE, as the console's
// `fmadds f0, f1, f31, f0` does -- value = GetOutput(f, curve) * (finish - start) + maStart[mnCurrentStage]:
//   PathLine<2>::Update @0x8268F2D0   fsubs f31 (finish - start) @0x8268F3F4, fmadds @0x8268F428, stfs +0x30
//   PathLine<3>::Update               fsubs f31 (finish - start) @0x8268F144, fmadds @0x8268F178, stfs +0x40
// (fmadds frD, frA, frC, frB is D = A * C + B). The PC multiplied, rounded, then added. Every case below is an input
// where one rounding and two differ; the expected bits are the exactly rounded f * range + start (rational
// arithmetic, ties-to-even; scratch generator gen_pathline.py). A 1000 ms stage has maLength = 1000 * 0.001f, which
// rounds to exactly 1.0f, so on E_LINEAR (GetOutput returns f) the stage position f IS the elapsed time.
// The control (a power-of-two range) rounds the same either way.
//
// run_fxaibuzz_path_line_fma.py extracts the PRODUCTION PathLine<2> AddStage / AddLinkedStage / Update, the generic
// PathLine<3> AddStage / Update(f32) / Update(f32, f32) with their two stage-length constants, and Curve::GetOutput
// (+ its table, constant and read helper) from CgsSoundUtils.cpp; PathLine is the real CgsSoundUtils.h struct.
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"
#include "rw/math/fpu/scalar_operation.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

static unsigned guAsserts = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++guAsserts; return 0; }
void* EndAssert() { return nullptr; }
} }

namespace CgsSound { namespace Utils {
#include "fxaibuzz_path_curve_bodies.inc"
#include "fxaibuzz_path_line_bodies.inc"
} }

using CgsSound::Utils::Curve;
using CgsSound::Utils::PathLine;

static int giChecks = 0;
static int giFailures = 0;
static void Check(bool lbCondition, const char* lpcLabel)
{
    ++giChecks;
    if (!lbCondition)
    {
        ++giFailures;
        std::printf("FAIL  %s\n", lpcLabel);
    }
}

static f32 Float(u32 lu) { f32 lf; std::memcpy(&lf, &lu, 4); return lf; }
static u32 Bits(f32 lf) { u32 lu; std::memcpy(&lu, &lf, 4); return lu; }

// { start bits, finish bits, stage position f bits, exactly rounded f * (finish - start) + start }
struct OutputCase { u32 muStart, muFinish, muFraction, muExpected; };

static const OutputCase kaTwo[] = {
    { 0x3E4CCCCDu, 0x3F733333u, 0x3E999A2Bu, 0x3ED99A07u },  // 0.2 -> 0.95, f 0.300004333: one 0x3ED99A07, two 0x3ED99A06
    { 0x3F800000u, 0x3FACCCCDu, 0x3E999E73u, 0x3F8D7111u },  // 1 -> 1.35, f 0.300036997: one 0x3F8D7111, two 0x3F8D7110
    { 0x40000000u, 0x40C9999Au, 0x3E9999A9u, 0x40528F65u },  // 2 -> 6.3, f 0.300000459: one 0x40528F65, two 0x40528F64
    { 0xC0600000u, 0x40100000u, 0x3EB21674u, 0xBFBFFFB9u },  // -3.5 -> 2.25, f 0.347827554: one 0xBFBFFFB9, two 0xBFBFFFBA
};
// Stage 1 linked from stage 0's finish 6.0 to 10.3, f in [0.25, 0.5): one 0x40E266D3, two 0x40E266D2
static const OutputCase kTwoLinked = { 0x40C00000u, 0x4124CCCDu, 0x3E800192u, 0x40E266D3u };
static const OutputCase kaThree[] = {
    { 0x3E19999Au, 0x3F59999Au, 0x3E999A3Du, 0x3EB8525Fu },  // 0.15 -> 0.85, f 0.30000487: one 0x3EB8525F, two 0x3EB8525E
    { 0x3F800000u, 0x3FD9999Au, 0x3E999A21u, 0x3F9AE15Fu },  // 1 -> 1.7, f 0.300004035: one 0x3F9AE15F, two 0x3F9AE160
    { 0x40400000u, 0x40F66666u, 0x3E999BEEu, 0x408D1F67u },  // 3 -> 7.7, f 0.300017774: one 0x408D1F67, two 0x408D1F68
    { 0xC0C00000u, 0x3FA66666u, 0x3F0C462Au, 0xBFFFFFE6u },  // -6 -> 1.3, f 0.547945619: one 0xBFFFFFE6, two 0xBFFFFFE4
};
// Update(dt, 0.9) on a 0.3 -> 0.6 stage: the live finish replaces 0.6 first. one 0x3EF5C2BB, two 0x3EF5C2BC
static const OutputCase kThreeTwoArg = { 0x3E99999Au, 0x3F666666u, 0x3E9999E3u, 0x3EF5C2BBu };

int main()
{
    char lacLabel[200];

    // ---- PathLine<2>::Update, fmadds @0x8268F428 -------------------------------------------------------------------
    for (const OutputCase& lrCase : kaTwo)
    {
        PathLine<2> l;
        l.AddStage(Float(lrCase.muStart), Float(lrCase.muFinish), 1000.0f, Curve::E_LINEAR);
        const bool lbUnitStage = l.maLength[0] == 1.0f;
        l.Update(Float(lrCase.muFraction));
        std::snprintf(lacLabel, sizeof(lacLabel), "PathLine<2> %.9g -> %.9g at f %.9g: got 0x%08X, want 0x%08X (fmadds "
                      "@0x8268F428, one rounding)", Float(lrCase.muStart), Float(lrCase.muFinish),
                      Float(lrCase.muFraction), Bits(l.mfCurrentValue), lrCase.muExpected);
        Check(lbUnitStage && !l.mbComplete && Bits(l.mfCurrentValue) == lrCase.muExpected, lacLabel);
    }
    {
        // Stage 0 (2 -> 6) completes on Update(1.25): stage 1 starts at 6.0 with t = 0.25. Then t + dt = f exactly
        // (f in [0.25, 0.5), so f - 0.25 and 0.25 + (f - 0.25) are both exact), and the addend is maStart[1].
        PathLine<2> l;
        l.AddStage(2.0f, 6.0f, 1000.0f, Curve::E_LINEAR);
        l.AddLinkedStage(Float(kTwoLinked.muFinish), 1000.0f, Curve::E_LINEAR);
        l.Update(1.25f);
        const f32 lfFraction = Float(kTwoLinked.muFraction);
        l.Update(lfFraction - 0.25f);
        const bool lbSetUp = l.mnCurrentStage == 1 && l.maStart[1] == Float(kTwoLinked.muStart)
                             && l.mfElapsedTime == lfFraction && l.maLength[1] == 1.0f;
        std::snprintf(lacLabel, sizeof(lacLabel), "PathLine<2> linked stage 1 (6 -> 10.3) at f %.9g: got 0x%08X, want "
                      "0x%08X (the addend is maStart[mnCurrentStage])", lfFraction, Bits(l.mfCurrentValue),
                      kTwoLinked.muExpected);
        Check(lbSetUp && Bits(l.mfCurrentValue) == kTwoLinked.muExpected, lacLabel);
    }
    {
        PathLine<2> l;
        l.AddStage(2.0f, 6.0f, 1000.0f, Curve::E_LINEAR);
        l.Update(0.3141592f);
        Check(Bits(l.mfCurrentValue) == 0x40506CBDu,
              "control: PathLine<2> 2 -> 6 (range 4, an exact product) at f 0.3141592 -> 0x40506CBD either way");
    }

    // ---- PathLine<3>::Update, fmadds @0x8268F178 -------------------------------------------------------------------
    for (const OutputCase& lrCase : kaThree)
    {
        PathLine<3> l;
        l.AddStage(Float(lrCase.muStart), Float(lrCase.muFinish), 1000.0f, Curve::E_LINEAR);
        const bool lbUnitStage = l.maLength[0] == 1.0f;
        l.Update(Float(lrCase.muFraction));
        std::snprintf(lacLabel, sizeof(lacLabel), "PathLine<3> %.9g -> %.9g at f %.9g: got 0x%08X, want 0x%08X (fmadds "
                      "@0x8268F178, one rounding)", Float(lrCase.muStart), Float(lrCase.muFinish),
                      Float(lrCase.muFraction), Bits(l.mfCurrentValue), lrCase.muExpected);
        Check(lbUnitStage && !l.mbComplete && Bits(l.mfCurrentValue) == lrCase.muExpected, lacLabel);
    }
    {
        PathLine<3> l;
        l.AddStage(Float(kThreeTwoArg.muStart), 0.6f, 1000.0f, Curve::E_LINEAR);
        l.Update(Float(kThreeTwoArg.muFraction), Float(kThreeTwoArg.muFinish));
        std::snprintf(lacLabel, sizeof(lacLabel), "PathLine<3>::Update(dt, 0.9) on a 0.3 -> 0.6 stage at f %.9g: got "
                      "0x%08X, want 0x%08X (the live finish, then the fused write)", Float(kThreeTwoArg.muFraction),
                      Bits(l.mfCurrentValue), kThreeTwoArg.muExpected);
        Check(l.maFinish[0] == Float(kThreeTwoArg.muFinish) && !l.mbComplete
              && Bits(l.mfCurrentValue) == kThreeTwoArg.muExpected, lacLabel);
    }

    Check(guAsserts == 0, "no tripwire fires on these ordinary stages");

    std::printf("FxAiBuzzPathLineFma: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
