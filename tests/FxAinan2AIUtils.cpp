// FX-AINAN2 regression (crash parity 2026-09-24): BrnAI::IsInsideSectionFast @0x82768680, extracted
// verbatim from BrnAIUtils.cpp by run_fxainan2_ai_utils.py.
//
// Console: per edge lane, `vcmpgefp v13, cross, 0` then `vnot` (0x827686EC / 0x827686F0) -- the lane
// is "inside" when cross is NOT >= 0 -- then the vperm AND-reduce, vcmpequw against ~0 and the
// vcmpeqfp./not/extrwi tail (0x827686F4..0x82768708). A NaN cross fails vcmpgefp, so its lane counts
// as INSIDE; only an ORDERED cross >= 0 puts the point outside. The old `!(cross < 0) -> outside`
// sent a NaN outside. The ordered controls pass on both spellings.
#include "GameSource/World/AI/BrnAIUtils.h"
#include <cstdio>
#include <limits>

#include "restored_methods.inc"

using namespace BrnAI;

namespace
{
    unsigned guChecks = 0, guFailures = 0;
    void Check(bool lbPass, const char* lpcLabel)
    {
        ++guChecks;
        if (!lbPass) { ++guFailures; std::printf("FAIL %s\n", lpcLabel); }
    }
    const f32 KF_NAN = std::numeric_limits<f32>::quiet_NaN();

    // The SoA edge block: edgeX0[4], edgeY0[4], coefA[4], coefB[4], 16-byte aligned. Four edges of
    // the axis-aligned square [0,10] x [0,10], wound so an interior point gives four NEGATIVE
    // crosses (cross = A * (y - y0) - B * (x - x0)).
    //   left   (x0 0,  y0 0):  A 0,  B 1   -> cross = -x        (< 0 for x > 0)
    //   right  (x0 10, y0 0):  A 0,  B -1  -> cross = x - 10    (< 0 for x < 10)
    //   bottom (x0 0,  y0 0):  A -1, B 0   -> cross = -y        (< 0 for y > 0)
    //   top    (x0 0,  y0 10): A 1,  B 0   -> cross = y - 10    (< 0 for y < 10)
    alignas(16) f32 gafSquare[16] = {
        0.0f, 10.0f, 0.0f, 0.0f,         // edge start x
        0.0f, 0.0f, 0.0f, 10.0f,         // edge start y
        0.0f, 0.0f, -1.0f, 1.0f,         // A
        1.0f, -1.0f, 0.0f, 0.0f,         // B
    };
}

int main()
{
    // cross_i = A_i * (y - y0_i) - B_i * (x - x0_i): at (5, 5) every edge gives -5.
    Check(IsInsideSectionFast(gafSquare, 5.0f, 5.0f), "control: the centre of the square is inside");
    Check(!IsInsideSectionFast(gafSquare, 15.0f, 5.0f), "control: a point right of the square is outside");
    Check(!IsInsideSectionFast(gafSquare, 5.0f, -3.0f), "control: a point below the square is outside");
    Check(!IsInsideSectionFast(gafSquare, 0.0f, 5.0f), "control: a point ON an edge (cross 0) is outside");

    Check(IsInsideSectionFast(gafSquare, KF_NAN, KF_NAN),
          "a NaN probe point makes every cross NaN -> every lane 'not >= 0' -> inside");
    alignas(16) f32 lafBad[16];
    for (int li = 0; li < 16; ++li) lafBad[li] = gafSquare[li];
    lafBad[8 + 1] = KF_NAN;             // the right edge's A coefficient
    Check(IsInsideSectionFast(lafBad, 5.0f, 5.0f),
          "a NaN edge coefficient only flips that lane to 'inside' -- the centre stays inside");
    Check(!IsInsideSectionFast(lafBad, 5.0f, -3.0f),
          "... while an ordered outside lane still puts a point below the square outside");

    std::printf("FxAinan2AIUtils: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
