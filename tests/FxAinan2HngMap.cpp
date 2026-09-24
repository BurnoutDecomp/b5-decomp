// FX-AINAN2 regression (crash parity 2026-09-24): the hard-no-go map's NaN polarity, extracted
// verbatim from RacingLine/BrnHardNoGoMap.cpp by run_fxainan2_hng_map.py.
//
//   WriteIntoMap @0x82782BC0: the four square coordinates are clamped by `fneg ; fsel` (low, to 0)
//     then `fsubs ; fsel` (high, to 31 / 7) at 0x82782C48..0x82782CA4. fsel takes its THIRD operand
//     on an unordered test: a NaN survives the low clamp and becomes the MAXIMUM at the high one,
//     so the console always rasterises an in-grid square. The old if/if kept the NaN: no square,
//     or (s32)NaN into SetMapSquare.
//   SpreadHNGAlongTrack @0x82782E08: after paying the row step, `fcmpu budget, 0.0 ; bgt -> next
//     column` @0x82782EE8/0x82782EEC -- a NaN budget falls into the neighbour fill (and is refilled
//     from a positive neighbour); the old `<= 0` skipped the fill and left the NaN.
//   SpreadHNGIntoPreviousSection @0x82777958: `fcmpu budget, 0.0 ; ble -> skip the column`
//     @0x82777AC0/0x82777AC4 (a NaN is skipped as an exhausted column) and the same bgt fill test
//     @0x82777B08/0x82777B0C.
//   RacingLineGenerator::HasSpreadHardNoGoLinesFinished (inlined into SpreadHNGBackOneStep
//     @0x8278F680): `fcmpu stretch, 0.0 ; bge -> not finished` @0x8278F804/0x8278F808 -- a NaN
//     column still counts as budget; the old `>= 0` let it read as exhausted.
// GetHNGInterpXY is a fixture (the interpolants are set directly); the ordered controls pass on
// both spellings.
#include "GameSource/World/AI/RacingLine/BrnHardNoGoMap.h"
#include "GameSource/World/AI/RacingLine/BrnRacingLineGenerator.h"
#include "GameSource/World/AI/Route/BrnRacingLine.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/math/vpu/vector2_operation.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

static unsigned gAssertions = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++gAssertions; return 0; }
void* EndAssert() { return nullptr; }
} }

namespace
{
    f32 gafInterp[4];   // start x, start y, end x, end y
    int giInterpCall = 0;
}
namespace BrnAI {
void HardNoGoMap::GetHNGInterpXY(Vector2, f32& lfInterpX, f32& lfInterpY)
{
    lfInterpX = gafInterp[giInterpCall * 2 + 0];
    lfInterpY = gafInterp[giInterpCall * 2 + 1];
    giInterpCall ^= 1;
}
}

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

    alignas(16) unsigned char gaMap[sizeof(HardNoGoMap)];
    HardNoGoMap& Map() { return *reinterpret_cast<HardNoGoMap*>(gaMap); }

    bool Bit(s32 liWidth, s32 liHeight) { return (Map().mHNGMap[liHeight] & (1 << liWidth)) != 0; }

    // A 32 x 8 section: left edge x = 0, right edge x = 32, current row at z = 0, previous row at
    // the given z (the row step is |z| / 7 per row).
    void Section(f32 lfPreviousLeftZ, f32 lfPreviousRightZ)
    {
        std::memset(gaMap, 0, sizeof(gaMap));
        Map().SetCorners(Vector2{ 0.0f, 0.0f, 0.0f, 0.0f }, Vector2{ 32.0f, 0.0f, 0.0f, 0.0f },
                         Vector2{ 0.0f, lfPreviousLeftZ, 0.0f, 0.0f },
                         Vector2{ 32.0f, lfPreviousRightZ, 0.0f, 0.0f });
        Map().ClearMap();   // the two outer columns of every row start occupied; mbReady
    }

    // WriteIntoMap on an EMPTY grid (the outer columns ClearMap sets would hide column 31).
    void Write(f32 lfStartX, f32 lfStartY, f32 lfEndX, f32 lfEndY)
    {
        for (s32 li = 0; li < KI_HNG_MAP_HEIGHT; ++li) Map().mHNGMap[li] = 0;
        gafInterp[0] = lfStartX; gafInterp[1] = lfStartY; gafInterp[2] = lfEndX; gafInterp[3] = lfEndY;
        giInterpCall = 0;
        Map().WriteIntoMap(Vector2{ 0.0f, 0.0f, 0.0f, 0.0f }, Vector2{ 0.0f, 0.0f, 0.0f, 0.0f });
    }
}

int main()
{
    // ---- WriteIntoMap ------------------------------------------------------------------------
    Section(-14.0f, -14.0f);
    gAssertions = 0;
    Write(0.5f, 0.5f, 0.5f, 0.5f);
    Check(Bit(16, 4) && Map().mHNGMap[4] == (1 << 16) && gAssertions == 0,
          "control: interp (0.5, 0.5) marks square (16, 4) and nothing else");

    Section(-14.0f, -14.0f);
    gAssertions = 0;
    Write(KF_NAN, KF_NAN, KF_NAN, KF_NAN);
    Check(Bit(31, 7), "all-NaN interps -> the high fsels give (31, 7) -> that square is marked");
    Check(gAssertions == 0, "... with no out-of-grid assert");

    Section(-14.0f, -14.0f);
    gAssertions = 0;
    Write(KF_NAN, 0.5f, 0.5f, 0.5f);
    Check(Bit(31, 4) && Bit(20, 4) && Bit(16, 4),
          "a NaN start x becomes column 31 -> row 4 is rasterised from 31 down to 16");
    Check(gAssertions == 0, "... every square inside the grid (no SetMapSquare assert)");

    // ---- SpreadHNGAlongTrack -----------------------------------------------------------------
    {
        f32 lafStretch[KI_HNG_MAP_WIDTH];
        Section(-14.0f, -14.0f);   // ordered: 2 m per row
        Map().SetMapSquare(5, 7);
        Map().SpreadHNGAlongTrack(lafStretch, 5.0f);
        Check(Bit(5, 6) && Bit(5, 5) && Bit(5, 4) && !Bit(5, 3),
              "control: a 5 m budget at 2 m per row covers rows 6, 5 and 4 (tested before paying), not 3");

        Section(KF_NAN, KF_NAN);   // NaN row step
        Map().SetMapSquare(1, 7);
        Map().SetMapSquare(2, 7);
        Map().SpreadHNGAlongTrack(lafStretch, 5.0f);
        Check(Bit(1, 6), "control: a NaN row step still marks the first row under the seed");
        Check(Bit(1, 5), "the NaN budget falls through bgt 0x82782EEC into the fill, is refilled from "
                         "its neighbours and marks row 5 too");
    }

    // ---- SpreadHNGIntoPreviousSection ---------------------------------------------------------
    {
        f32 lafStretch[KI_HNG_MAP_WIDTH];
        for (s32 li = 0; li < KI_HNG_MAP_WIDTH; ++li) lafStretch[li] = 0.0f;
        Section(-14.0f, -14.0f);
        lafStretch[3] = KF_NAN;
        lafStretch[8] = 5.0f;
        Map().SpreadHNGIntoPreviousSection(lafStretch);
        Check(Bit(8, 7) && Bit(8, 6) && Bit(8, 5),
              "control: an ordered 5 m budget marks column 8 on rows 7, 6 and 5");
        Check(!Bit(3, 7), "a NaN budget takes ble 0x82777AC4 -> the column is skipped, nothing marked");

        for (s32 li = 0; li < KI_HNG_MAP_WIDTH; ++li) lafStretch[li] = 5.0f;
        Section(KF_NAN, KF_NAN);
        for (s32 li = 0; li < KI_HNG_MAP_HEIGHT; ++li) Map().mHNGMap[li] = 0;   // no outer columns
        Map().SpreadHNGIntoPreviousSection(lafStretch);
        Check(lafStretch[0] == 5.0f, "a NaN row step -> bgt 0x82777B0C not taken -> the fill hands "
                                     "column 0 the budget of column 1 back (5.0); the old body left NaN");
    }

    // ---- RacingLineGenerator::HasSpreadHardNoGoLinesFinished -----------------------------------
    {
        alignas(16) static unsigned char saLine[sizeof(RacingLine)];
        alignas(16) static unsigned char saGenerator[sizeof(RacingLineGenerator)];
        RacingLine& lrLine = *reinterpret_cast<RacingLine*>(saLine);
        RacingLineGenerator& lrGenerator = *reinterpret_cast<RacingLineGenerator*>(saGenerator);
        for (s32 li = 0; li < 32; ++li) lrLine.maStretchDistanceForHNG[li] = -1.0f;
        Check(lrGenerator.HasSpreadHardNoGoLinesFinished(&lrLine), "control: every column negative -> finished");
        lrLine.maStretchDistanceForHNG[9] = 0.0f;
        Check(!lrGenerator.HasSpreadHardNoGoLinesFinished(&lrLine), "control: a 0.0 column is still budget (bge)");
        lrLine.maStretchDistanceForHNG[9] = KF_NAN;
        Check(!lrGenerator.HasSpreadHardNoGoLinesFinished(&lrLine),
              "a NaN column takes bge 0x8278F808 -> not finished");
    }

    std::printf("FxAinan2HngMap: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
