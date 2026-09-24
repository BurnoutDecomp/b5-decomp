// FX-RCEM4 (crash parity 2026-09-24): ActiveRaceCar::AddToScene's handling-box Y pad (ARTIST
// 0x822EB80C..0x822EB854). run_fxrcem4_add_to_scene_pad.py extracts the statements from
// `const f32 lfComY` through `lfHalfDelta` VERBATIM and runs them against:
//   lvx128 +0xC0 ; vspltw 1           COM.y (mCentreOfMassTransform.wAxis.y)
//   vandc <sign mask>                 fabs
//   vsubfp                            - spec->mHandlingBodyDimensions.y
//   vmaxfp128 v13, v13, v127 (= 0)    NOT (x > 0) ? x : 0 -- a NaN operand gives a QNaN (AltiVec PEM)
//   vaddfp unk_82FAD550 (0.05) ; vmulfp128 0.5
#include "types.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace Fixture {
static const f32 KF_HANDLING_BODY_Y_PAD = 0.05f;   // unk_82FAD550, CRT splat of flt_8201497C
struct Row { f32 x, y, z, w; };
struct Transform { Row wAxis; };
static f32 HalfDelta(f32 lfComYIn, f32 lfDimsY)
{
    const Transform mCentreOfMassTransform = { { 0.0f, lfComYIn, 0.0f, 1.0f } };
    const Row lrDims = { 0.0f, lfDimsY, 0.0f, 0.0f };
    const Row& lrHandlingBodyDimensions = lrDims;
#include "fxrcem4_ats_block.inc"
    return lfHalfDelta;
}
}   // namespace Fixture

// vmaxfp lane semantics: NaN in either operand -> QNaN; +0 is the larger of +-0.
static f32 Vmaxfp(f32 a, f32 b) {
    if (a != a) return a;
    if (b != b) return b;
    if (a == b) return std::signbit(a) ? b : a;
    return a > b ? a : b;
}
static f32 Console(f32 lfComY, f32 lfDimsY) {
    return 0.5f * (Vmaxfp(std::fabs(lfComY) - lfDimsY, 0.0f) + 0.05f);
}

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName) {
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}
static bool Same(f32 a, f32 b) { u32 x, y; std::memcpy(&x, &a, 4); std::memcpy(&y, &b, 4); return x == y || (std::isnan(a) && std::isnan(b)); }

int main() {
    using Fixture::HalfDelta;
    const f32 lfNan = std::numeric_limits<f32>::quiet_NaN();
    Check(Same(HalfDelta(0.8f, 0.6f), 0.5f * ((0.8f - 0.6f) + 0.05f)), "an overhang grows the box by half of (overhang + 0.05)");
    Check(Same(HalfDelta(-0.8f, 0.6f), HalfDelta(0.8f, 0.6f)), "the COM height is taken as fabs (vandc sign mask)");
    Check(Same(HalfDelta(0.3f, 0.6f), 0.025f), "no overhang -> vmaxfp against 0 -> the bare 0.05 pad, halved");
    Check(std::isnan(HalfDelta(lfNan, 0.6f)), "a NaN COM height stays NaN through vmaxfp (not the 0.025 of `(x > 0) ? x : 0`)");
    Check(std::isnan(HalfDelta(0.8f, lfNan)), "a NaN handling-body height stays NaN too");
    const f32 laCom[] = { 0.0f, -0.0f, 0.6f, 0.60001f, 1.5f, -2.0f, 1.0e-30f, lfNan };
    const f32 laDims[] = { 0.6f, 0.0f, -0.0f, 2.0f };
    bool lbAll = true;
    for (f32 c : laCom) for (f32 d : laDims) {
        if (!Same(HalfDelta(c, d), Console(c, d))) {
            lbAll = false; std::fprintf(stderr, "  com %g dims %g: production %g console %g\n", c, d, HalfDelta(c, d), Console(c, d));
        }
    }
    Check(lbAll, "bit for bit against the console's vandc / vsubfp / vmaxfp / vaddfp / vmulfp lane");
    std::printf("FxRcem4AddToScenePad: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
