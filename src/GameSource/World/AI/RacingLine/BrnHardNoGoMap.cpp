// BrnAI::HardNoGoMap -- the whole class, reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// The map is a 32 x 8 bit grid laid over one AI section as a bilinear patch between the
// section's entry portal edge ("previous" left/right) and its exit portal edge ("current"
// left/right); see the header banner for the packing and the pinned offsets.
//
// EXPORTED BODIES RECONSTRUCTED HERE
//   0x82777840 SetCorners            0x82782F80 MakeMap            0x82782E08 SpreadHNGAlongTrack
//   0x82777958 SpreadHNGIntoPreviousSection    0x82777E58 SectionLength
//   0x82783AA8 FindMaximalEdges      0x82783C28 GetEstimatedRoadWidth
//   0x82777D60 DistanceToHardNoGoEdge          0x82768858 MapSquareOccupied(s32,s32)
//   0x82783088 GetSquareTopLeft      0x82783228 GetSquareTopRight  0x827833D0 GetSquareCentre
//   0x82782BC0 WriteIntoMap          0x82777BA0 GetHNGInterpXY     0x827687D0 ConvertInterpToIndex
//   0x82764898 MapSquareOccupiedFast 0x82764A80 SetMapSquare(s32,s32)
// RECOVERED FROM AN INLINED BLOCK (no export of their own)
//   Prepare / ClearMap  -- RacingLineGenerator::SetUpHardNoGoMap @0x82780574..0x8278062C
//   IsReady             -- inlined at RacingLineGenerator::SpreadHNGBackOneStep @0x8278F79C
//                          (`lbz 0x98(sectionData)` == map+0x48); bodied in the header.
//   the Get/Set trivial accessors -- bodied in the header.
//
// PARKS (declared in the header, deliberately NOT bodied here)
//   [FLAG PC bring-up] HardNoGoMap::Render @0x82783740 -- debug render (walks
//     MapSquareOccupiedFast + GetSquareCentre + RenderHNGSquare). Presentation-only on this
//     host per the wave rule on Render*/Draw* bodies. DELETE-WHEN a PC debug-render lane
//     brings up RacingLineGenerator::RenderHardNoGoMap @0x82790278.
//   [FLAG PC bring-up] MoveTargetToLegalPosition, GetNearestEdges, IsSquareOccupied,
//     FindNearestSpaceForTarget, CheckForContinuedSpreading, FindSpaceForTarget,
//     ConvertInterpToPosition, IsInRange, MapSquareOccupied(f32,f32), SetMapSquare(f32,f32)
//     -- DWARF-declared, NO IDA export, and not reached by any caller recovered in this wave
//     (the only X360 xrefs into this class are RacingLineGenerator::{SetUpHardNoGoMap,
//     DropHardNoGoLinesIntoMap, SpreadHNGBackOneStep, SetupSectionExit, GetRouteCentre,
//     SetUpIncomingPortalTarget, CalculateIntersectionWithProjectedRoute, GetHalfRoadWidthHere,
//     GetPointFarAhead} and SteeringFan::IncludeHardNoGo, all of which are covered above).
//     Declared-only so the DWARF surface is recorded; DELETE-WHEN a caller that inlines one
//     of them is decompiled and the block can be read out of its asm.
//   [FLAG PC bring-up] RenderHNGSquare (DWARF :234) is not even declared: it is
//     debug-render-only AND its `RGBA` parameter has no single canonical home in this tree
//     (three different `typedef u32 RGBA` live in three namespaces). DELETE-WHEN the debug
//     render lane lands with an agreed RGBA home.
//
// THE PSEUDOCODE LIES IN THREE PLACES (all resolved against the asm):
//   * 0x82768858 is MapSquareOccupied(int32_t, int32_t), NOT the (f32, f32) overload: its two
//     arguments arrive in r4/r5 and are compared with `cmplwi`, not in f1/f2.
//   * ConvertInterpToIndex / GetSquareTop* / SectionLength take their float arguments in
//     f1(/f2) while the following integer/pointer arguments still start at r6 -- the PPC
//     float-arg GPR skip. Hex-Rays renders that as a long tail of phantom int parameters.
//   * every `vmaddfp vD, vA, vB, vC` is vD = vA*vC + vB (encoding order A, B, C), which is
//     what turns the lerps below into (b - a) * t + a rather than the pseudocode's shape.

#include "GameSource/World/AI/RacingLine/BrnHardNoGoMap.h"

#include "SharedClasses/AI/AISectionsResourceType.h"   // BrnAI::AISection (mpaNoGoLines, muNumNoGoLines)
#include "GameSource/World/AI/BrnAIBoundaryLine.h"     // BrnAI::BoundaryLine (16-byte packed 2D segment)
#include "GameShared/GameClasses/Core/CgsAssert.h"     // CGS_ASSERT
#include "rw/math/vpu/vector2_operation.h"             // rw::math::vpu::Magnitude (2-lane)

#include <cmath>                                       // fabsf

namespace BrnAI
{
namespace
{
    namespace vpu = rw::math::vpu;

    // ---- rodata constants, read out of the image (VA -> value -> how) -------------------
    // flt_82013FA8 = 32.0f        == (f32)KI_HNG_MAP_WIDTH        ConvertInterpToIndex/WriteIntoMap
    // flt_820C41E0 =  8.0f        == (f32)KI_HNG_MAP_HEIGHT       ditto
    // flt_820323F4 = 31.0f        == KI_HNG_MAP_WIDTH  - 1        WriteIntoMap high clamp
    // flt_820054D0 =  7.0f        == KI_HNG_MAP_HEIGHT - 1        ditto
    // flt_820C665C = 0.03125f     == 1 / KI_HNG_MAP_WIDTH         square width / ring distance
    // flt_82004010 = 0.125f       == 1 / KI_HNG_MAP_HEIGHT        square height
    // flt_82013AB4 = 0.14285715f  == 1 / (KI_HNG_MAP_HEIGHT - 1)  spread row step
    // flt_820C6658 = 0.032258064f == 1 / (KI_HNG_MAP_WIDTH  - 1)  spread column step
    // flt_820C4168 = 0.5f   flt_82001C98 = 1.0f   flt_82001CC0 = 0.0f
    // flt_820C3B70 = 1.1920929e-07f == FLT_EPSILON (GetHNGInterpXY degenerate-section guard)
    const f32 KF_HNG_SQUARE_WIDTH  = 0.03125f;      // flt_820C665C
    const f32 KF_HNG_SQUARE_HEIGHT = 0.125f;        // flt_82004010
    const f32 KF_HNG_ROW_STEP      = 0.14285715f;   // flt_82013AB4
    const f32 KF_HNG_COLUMN_STEP   = 0.032258064f;  // flt_820C6658
    const f32 KF_EPSILON           = 1.1920929e-07f;// flt_820C3B70

    // GetHNGInterpXY's binary search: `li r11, 5` @0x82777BAC, seeded at t = 0.5 with a
    // 0.25 step (vcfsx v13, 1, 1 == 0.5; v12 = 0.5 * 0.5) that halves every pass.
    const s32 KI_HNG_INTERP_ITERATIONS = 5;

    // ClearMap's fill word: `lis r10, -0x8000 ; ori r10, r10, 1` @0x82780608/0x82780610 --
    // the two outermost columns of every row start occupied (the road's own edges).
    const MapData KI_HNG_MAP_OUTER_COLUMNS = static_cast<MapData>(0x80000001u);

    // The X360 forms every 2D length in this TU as the SDK pair `Magnitude(lA - lB)` over the
    // x/y lanes only -- vmulfp128 (square) / vspltw 1 / vspltw 0 / vaddfp (x^2 + y^2), then
    // vrsqrtefp + two Newton-Raphson steps * the dot, with a vcmpeqfp-vs-zero vsel guard that
    // falls out of sqrt(0) == 0. The vendor Vector2 header carries Magnitude but no
    // operator-, so the subtraction is spelled here rather than growing a shared vendor
    // header another lane may also be growing this wave.
    inline f32 Distance2D(Vector2 lA, Vector2 lB)
    {
        return vpu::Magnitude(Vector2{ lA.x - lB.x, lA.y - lB.y, 0.0f, 0.0f });
    }
}

// ================================================================================================
// Section lifecycle
// ================================================================================================

// No export of its own -- inlined by RacingLineGenerator::SetUpHardNoGoMap:
//   0x82780574  stw  r31, 0x40(r28)   -> miSectionIndex = liSectionIndex
//   0x82780578  stb  r11(0), 0x48(r28)-> mbReady = false
// (r28 == sectionData + 0x50 == the embedded HardNoGoMap.)
void HardNoGoMap::Prepare(s32 liSectionIndex)
{
    miSectionIndex = liSectionIndex;
    mbReady        = false;
}

// @0x82777840
// Four vrlimi128 pairs pack the corners two-per-Vector4 (mask 8 = lane x, 4 = lane y,
// 2 = lane z with a 2-word rotate, 1 = lane w with a 2-word rotate), then the average
// portal width is accumulated into mfAverageWidth with three separate stfs to 0x44 --
// i.e. the source assigns, adds and halves in three statements.
void HardNoGoMap::SetCorners(Vector2 lCurrentLeft, Vector2 lCurrentRight,
                             Vector2 lPreviousLeft, Vector2 lPreviousRight)
{
    mCurrentAndPreviousLeft.x = lCurrentLeft.x;    // vrlimi128 v12, v1, 8, 0
    mCurrentAndPreviousLeft.y = lCurrentLeft.y;    // vrlimi128 v12, v1, 4, 0
    mCurrentAndPreviousLeft.z = lPreviousLeft.x;   // vrlimi128 v12, v3, 2, 2
    mCurrentAndPreviousLeft.w = lPreviousLeft.y;   // vrlimi128 v12, v3, 1, 2

    mCurrentAndPreviousRight.x = lCurrentRight.x;  // vrlimi128 v10, v2, 8, 0
    mCurrentAndPreviousRight.y = lCurrentRight.y;  // vrlimi128 v10, v2, 4, 0
    mCurrentAndPreviousRight.z = lPreviousRight.x; // vrlimi128 v10, v4, 2, 2
    mCurrentAndPreviousRight.w = lPreviousRight.y; // vrlimi128 v10, v4, 1, 2

    mfAverageWidth  = Distance2D(lCurrentLeft, lCurrentRight);    // stfs 0x44 @0x827778F0
    mfAverageWidth += Distance2D(lPreviousLeft, lPreviousRight);  // stfs 0x44 @0x82777940
    mfAverageWidth *= 0.5f;                                       // stfs 0x44 @0x82777950 (flt_820C4168)
}

// @0x82782F80
// Rasterise the section's no-go lines [liStart, liEnd) into the grid, one WriteIntoMap per
// line. liEnd is clamped to the section's line count (`cmpw r27, r23 ; blt ; mr r27, r23`);
// the return value is that clamp -- true means the caller has now placed the WHOLE section.
// The per-iteration bounds assert is the inlined AISection::GetHNGLine guard, which the
// retail build still emits here (FireAssert with the literal expression string, no stream).
bool HardNoGoMap::MakeMap(const AISection* lpSection, s32 liStart, s32 liEnd)
{
    const s32 liNumNoGoLines = lpSection->muNumNoGoLines;   // lhz 0x12(section)

    if (liEnd >= liNumNoGoLines)
    {
        liEnd = liNumNoGoLines;
    }

    for (s32 liLine = liStart; liLine < liEnd; ++liLine)
    {
        // clrlwi r30, r31, 16 -- the index is narrowed to the u16 GetHNGLine takes.
        const u16 luHNGLineIndex = static_cast<u16>(liLine);

        CGS_ASSERT(luHNGLineIndex < lpSection->muNumNoGoLines,
                   "luHNGLineIndex < muNumNoGoLines");

        const BoundaryLine& lrLine = lpSection->mpaNoGoLines[luHNGLineIndex];

        // The whole 16-byte line is loaded once (lvx128) and re-lane-selected into the two
        // Vector2 arguments, whose z/w lanes are explicitly zeroed (the two `std r29, 0(rN)`).
        const Vector2 lStart = { lrLine.mfStartX, lrLine.mfStartY, 0.0f, 0.0f };
        const Vector2 lEnd   = { lrLine.mfEndX,   lrLine.mfEndY,   0.0f, 0.0f };

        WriteIntoMap(lStart, lEnd);
    }

    return liEnd == liNumNoGoLines;   // subf/cntlzw/extrwi of (liNumNoGoLines - liEnd)
}

// No export of its own -- inlined by RacingLineGenerator::SetUpHardNoGoMap
// @0x82780608..0x8278062C: `li 0x80000001` stored over 8 consecutive words from map+0x20
// (mtctr 8 loop), then `li r11, 1 ; stb r11, 0x48` marking the map ready.
void HardNoGoMap::ClearMap()
{
    for (s32 liHeight = 0; liHeight < KI_HNG_MAP_HEIGHT; ++liHeight)
    {
        mHNGMap[liHeight] = KI_HNG_MAP_OUTER_COLUMNS;
    }

    mbReady = true;
}

// ================================================================================================
// Spreading
// ================================================================================================

// @0x82782E08
// Flood the spread budget down the grid. Each row is walked left to right; an occupied square
// re-seeds its column's budget to lfSpreadDistance, a free column with budget left is marked
// occupied and pays the row's step out of its budget. A column that runs out inherits from a
// neighbour that still has some, so the no-go region widens as it travels back along the track.
//
// The per-column step grows linearly across the row from the left edge's row height to the
// right edge's: SectionLength(0) / 7 at column 0, SectionLength(1) / 7 at column 31.
void HardNoGoMap::SpreadHNGAlongTrack(f32* lpafStretchDistance, f32 lfSpreadDistance)
{
    // mtctr 32 ; stw 0 -- the caller's RacingLine::maStretchDistanceForHNG[32] is reset.
    for (s32 liColumn = 0; liColumn < KI_HNG_MAP_WIDTH; ++liColumn)
    {
        lpafStretchDistance[liColumn] = 0.0f;
    }

    const f32 lfLeftRowStep  = SectionLength(0.0f) * KF_HNG_ROW_STEP;   // bl SectionLength, f1 = 0.0
    const f32 lfRightRowStep = SectionLength(1.0f) * KF_HNG_ROW_STEP;   // bl SectionLength, f1 = 1.0
    const f32 lfRowStepDelta = (lfRightRowStep - lfLeftRowStep) * KF_HNG_COLUMN_STEP;

    for (s32 liHeight = KI_HNG_MAP_HEIGHT - 1; liHeight >= 0; --liHeight)
    {
        f32 lfRowStep = lfLeftRowStep;   // fmr f30, f28 at the top of every row

        for (s32 liWidth = 0; liWidth < KI_HNG_MAP_WIDTH; ++liWidth)
        {
            if (MapSquareOccupiedFast(liWidth, liHeight))
            {
                lpafStretchDistance[liWidth] = lfSpreadDistance;
            }
            else if (lpafStretchDistance[liWidth] > 0.0f)
            {
                SetMapSquare(liWidth, liHeight);

                lpafStretchDistance[liWidth] -= lfRowStep;

                if (lpafStretchDistance[liWidth] <= 0.0f)
                {
                    // Exhausted: inherit whichever neighbouring column still has budget.
                    if (liWidth == 0)
                    {
                        if (lpafStretchDistance[1] > 0.0f)
                        {
                            lpafStretchDistance[0] = lpafStretchDistance[1];
                        }
                    }
                    else if (liWidth == KI_HNG_MAP_WIDTH - 1)
                    {
                        if (lpafStretchDistance[KI_HNG_MAP_WIDTH - 2] > 0.0f)
                        {
                            lpafStretchDistance[KI_HNG_MAP_WIDTH - 1] =
                                lpafStretchDistance[KI_HNG_MAP_WIDTH - 2];
                        }
                    }
                    else
                    {
                        const f32 lfBelow = lpafStretchDistance[liWidth - 1];
                        const f32 lfAbove = lpafStretchDistance[liWidth + 1];

                        if (lfBelow > 0.0f && lfAbove > 0.0f)
                        {
                            lpafStretchDistance[liWidth] =
                                (lfBelow >= lfAbove) ? lfAbove : lfBelow;
                        }
                    }
                }
            }

            lfRowStep += lfRowStepDelta;
        }
    }
}

// @0x82777958
// The mirror of SpreadHNGAlongTrack for the section BEHIND the one the spread started in: the
// budget array arrives already populated, an occupied square KILLS that column's budget (the
// no-go region it would have spread into is already solid), and -- unlike SpreadHNGAlongTrack
// -- a column with no budget left does not advance the row step at all (the `fadds f30, f30,
// f29` at 0x82777B74 sits inside the `> 0.0f` arm, not after it).
//
// The two row steps are computed OPEN-CODED here (0x8277798C..0x82777A8C), not through a call
// to SectionLength -- there is no `bl` and no ready-assert in this body, so the source did the
// two magnitudes directly rather than calling SectionLength(0) / SectionLength(1).
void HardNoGoMap::SpreadHNGIntoPreviousSection(f32* lpafStretchDistance)
{
    const f32 lfLeftRowStep  = Distance2D(GetCurrentLeft(),  GetPreviousLeft())  * KF_HNG_ROW_STEP;
    const f32 lfRightRowStep = Distance2D(GetCurrentRight(), GetPreviousRight()) * KF_HNG_ROW_STEP;
    const f32 lfRowStepDelta = (lfRightRowStep - lfLeftRowStep) * KF_HNG_COLUMN_STEP;

    for (s32 liHeight = KI_HNG_MAP_HEIGHT - 1; liHeight >= 0; --liHeight)
    {
        f32 lfRowStep = lfLeftRowStep;

        for (s32 liWidth = 0; liWidth < KI_HNG_MAP_WIDTH; ++liWidth)
        {
            if (lpafStretchDistance[liWidth] <= 0.0f)
            {
                continue;   // ble -> the loop increment, skipping the step advance
            }

            if (MapSquareOccupiedFast(liWidth, liHeight))
            {
                lpafStretchDistance[liWidth] = 0.0f;
            }
            else
            {
                SetMapSquare(liWidth, liHeight);

                lpafStretchDistance[liWidth] -= lfRowStep;

                if (lpafStretchDistance[liWidth] <= 0.0f)
                {
                    if (liWidth == 0)
                    {
                        if (lpafStretchDistance[1] > 0.0f)
                        {
                            lpafStretchDistance[0] = lpafStretchDistance[1];
                        }
                    }
                    else if (liWidth == KI_HNG_MAP_WIDTH - 1)
                    {
                        if (lpafStretchDistance[KI_HNG_MAP_WIDTH - 2] > 0.0f)
                        {
                            lpafStretchDistance[KI_HNG_MAP_WIDTH - 1] =
                                lpafStretchDistance[KI_HNG_MAP_WIDTH - 2];
                        }
                    }
                    else
                    {
                        const f32 lfBelow = lpafStretchDistance[liWidth - 1];
                        const f32 lfAbove = lpafStretchDistance[liWidth + 1];

                        if (lfBelow > 0.0f && lfAbove > 0.0f)
                        {
                            lpafStretchDistance[liWidth] =
                                (lfBelow >= lfAbove) ? lfAbove : lfBelow;
                        }
                    }
                }
            }

            lfRowStep += lfRowStepDelta;
        }
    }
}

// ================================================================================================
// Queries
// ================================================================================================

// @0x82777E58
// Distance travelled through the section along the line at lfInterp across it: the left edge's
// length at interp 0, the right edge's at interp 1, lerped. The X360 spells the lerp
// `vmaddfp v0, (right - left), left, interp` == (right - left) * interp + left.
f32 HardNoGoMap::SectionLength(f32 lfInterp)
{
    CGS_ASSERT(mbReady, "Section cannot give length\n");   // BrnHardNoGoMap.cpp:1493

    const f32 lfLeftLength  = Distance2D(GetCurrentLeft(),  GetPreviousLeft());
    const f32 lfRightLength = Distance2D(GetCurrentRight(), GetPreviousRight());

    return (lfRightLength - lfLeftLength) * lfInterp + lfLeftLength;
}

// @0x82783AA8
// Walk the row at lfHeightInterp in from both ends and report the boundary of the free span:
// scanning UP from column 0 the first free square's TOP-RIGHT corner is the lRightEdge, and
// scanning DOWN from column 31 the first free square's TOP-LEFT corner is the lLeftEdge (so
// column 0 is the world-right side of the road). When a side finds no free square at all it
// falls back to the extreme column, which is what keeps the pair usable on a fully-blocked row.
//
// MapSquareOccupied (not ...Fast) is deliberate: it reports OUT OF RANGE as occupied, so the
// two scans terminate on their own bounds.
//
// The two Vector2 outs take lane 0 and lane 2 of the returned Vector3 (`vrlimi128 v13, v0, 8, 0`
// then `vrlimi128 v13, v0, 4, 1`) -- i.e. world (x, z), because the map plane IS world (x, z).
// Lanes z/w of each out are left untouched, exactly as the asm leaves them.
void HardNoGoMap::FindMaximalEdges(Vector2& lLeftEdge, Vector2& lRightEdge, f32 lfHeightInterp)
{
    s32 liWidthIndex  = 0;
    s32 liHeightIndex = 0;
    ConvertInterpToIndex(0.0f, lfHeightInterp, liWidthIndex, liHeightIndex);

    bool lbFoundRight = false;
    for (s32 liWidth = 0; liWidth < KI_HNG_MAP_WIDTH; ++liWidth)
    {
        if (MapSquareOccupied(liWidth, liHeightIndex))
        {
            continue;
        }

        const Vector3 lTopRight =
            GetSquareTopRight(static_cast<u32>(liWidth), static_cast<u32>(liHeightIndex), 0.0f);
        lRightEdge.x = lTopRight.x;
        lRightEdge.y = lTopRight.z;
        lbFoundRight = true;
        break;
    }

    if (!lbFoundRight)
    {
        const Vector3 lTopRight = GetSquareTopRight(0u, static_cast<u32>(liHeightIndex), 0.0f);
        lRightEdge.x = lTopRight.x;
        lRightEdge.y = lTopRight.z;
    }

    bool lbFoundLeft = false;
    for (s32 liWidth = KI_HNG_MAP_WIDTH - 1; liWidth >= 0; --liWidth)
    {
        if (MapSquareOccupied(liWidth, liHeightIndex))
        {
            continue;
        }

        const Vector3 lTopLeft =
            GetSquareTopLeft(static_cast<u32>(liWidth), static_cast<u32>(liHeightIndex), 0.0f);
        lLeftEdge.x = lTopLeft.x;
        lLeftEdge.y = lTopLeft.z;
        lbFoundLeft = true;
        break;
    }

    if (!lbFoundLeft)
    {
        const Vector3 lTopLeft = GetSquareTopLeft(static_cast<u32>(KI_HNG_MAP_WIDTH - 1),
                                                  static_cast<u32>(liHeightIndex), 0.0f);
        lLeftEdge.x = lTopLeft.x;
        lLeftEdge.y = lTopLeft.z;
    }
}

// @0x82783C28
// Width of the free span FindMaximalEdges reports on the row at lfHeightInterp. Both out
// vectors are zeroed first (vspltisw128 v127, 0 into each), so the trailing lanes contribute
// nothing to the magnitude.
f32 HardNoGoMap::GetEstimatedRoadWidth(f32 lfHeightInterp)
{
    Vector2 lLeftEdge;
    Vector2 lRightEdge;
    lLeftEdge.SetZero();
    lRightEdge.SetZero();

    FindMaximalEdges(lLeftEdge, lRightEdge, lfHeightInterp);

    return Distance2D(lRightEdge, lLeftEdge);   // vsubfp v13, v12(right), v13(left)
}

// @0x82777D60
// Occupancy of the square lPos falls in, plus how far (in map-width units) the nearest square
// of the OPPOSITE occupancy is. The row word is INVERTED when the query square is occupied, so
// one search finds "nearest free" and "nearest blocked" alike; a mask that starts as the two
// immediate neighbours of the query bit is then grown one ring per step until it hits a set
// bit. A row with nothing to find (all bits set after the optional inversion) reports 1.0.
//
// This is where the row word's SIGNEDNESS bites: the mask grows with `srawi r11, r11, 1`, an
// ARITHMETIC right shift, so a mask that has reached bit 31 keeps its sign bit filling in from
// the top. MapData is int32_t (DWARF BrnHardNoGoMap.h:24) precisely for this.
bool HardNoGoMap::DistanceToHardNoGoEdge(Vector2 lPos, f32& lfDistance)
{
    f32 lfInterpX = 0.0f;
    f32 lfInterpY = 0.0f;
    GetHNGInterpXY(lPos, lfInterpX, lfInterpY);

    s32 liWidth  = 0;
    s32 liHeight = 0;
    ConvertInterpToIndex(lfInterpX, lfInterpY, liWidth, liHeight);

    const MapData lMapBit  = static_cast<MapData>(1u << liWidth);   // li 1 ; slw
    MapData       lMapRow  = mHNGMap[liHeight];                     // lwzx @ 4*(height+8)
    const bool    lbOccupied = ((lMapRow & lMapBit) == lMapBit);

    if (lbOccupied)
    {
        lMapRow = ~lMapRow;   // not r10, r10 -- now search for the nearest FREE square
    }

    if (lMapRow == -1)
    {
        lfDistance = 1.0f;    // flt_82001C98
        return lbOccupied;
    }

    f32     lfRings = 1.0f;
    MapData lMapMask = (lMapBit >> 1) | static_cast<MapData>(static_cast<u32>(lMapBit) << 1);

    while ((lMapMask & lMapRow) == 0)
    {
        if (lMapMask == -1)
        {
            break;            // the mask has saturated the whole row
        }

        lfRings += 1.0f;
        lMapMask |= (lMapMask >> 1) | static_cast<MapData>(static_cast<u32>(lMapMask) << 1);
    }

    lfDistance = lfRings * KF_HNG_SQUARE_WIDTH;   // flt_820C665C == 1/32
    return lbOccupied;
}

// ================================================================================================
// Square access
// ================================================================================================

// @0x82764898 (inline body at BrnHardNoGoMap.h:255..270 on the console; the three asserts are
// the header's lines 264/265/266, and the file/line pair the retail build bakes in is replaced
// by CGS_ASSERT's own __FILE__/__LINE__).
// The range checks are SIGNED (`cmpwi ..,0 ; blt` then `cmpwi ..,0x20 ; blt`), which is what
// pins the parameters to int32_t rather than the pseudocode's unsigned.
bool HardNoGoMap::MapSquareOccupiedFast(s32 liWidth, s32 liHeight)
{
    CGS_ASSERT(mbReady, "Hard No Go Secton not ready\n");
    CGS_ASSERT(liWidth  >= 0 && liWidth  < KI_HNG_MAP_WIDTH,  "Bad width index ");
    CGS_ASSERT(liHeight >= 0 && liHeight < KI_HNG_MAP_HEIGHT, "Bad height index ");

    // DWARF local BrnHardNoGoMap.h:270. The asm's subf/cntlzw/extrwi tail is the compiler's
    // way of yielding the == comparison as a BOOL.
    const MapData lMapMask = static_cast<MapData>(1u << liWidth);
    return (mHNGMap[liHeight] & lMapMask) == lMapMask;
}

// @0x82768858 (out of line, BrnHardNoGoMap.cpp:868).
// The range-tolerant sibling: OUT OF RANGE READS AS OCCUPIED, and the ready assert only runs
// once the indices are known good. The asm folds the four signed bounds into two unsigned
// compares (`cmplwi ..,0x1F ; bgt` / `cmplwi ..,7 ; bgt`), which is exactly the pair of
// `< 0 || >= N` tests below.
bool HardNoGoMap::MapSquareOccupied(s32 liWidth, s32 liHeight)
{
    if (liWidth  < 0 || liWidth  >= KI_HNG_MAP_WIDTH ||
        liHeight < 0 || liHeight >= KI_HNG_MAP_HEIGHT)
    {
        return true;
    }

    CGS_ASSERT(mbReady, "Hard No Go Secton not ready\n");

    const MapData lMapMask = static_cast<MapData>(1u << liWidth);
    return (mHNGMap[liHeight] & lMapMask) == lMapMask;
}

// @0x82764A80 (inline body at BrnHardNoGoMap.h:280..291; asserts at the header's 287/288/289 --
// note the range asserts run BEFORE the ready assert here, the opposite order to
// MapSquareOccupiedFast, and the bit set afterwards is unconditional).
void HardNoGoMap::SetMapSquare(s32 liWidth, s32 liHeight)
{
    CGS_ASSERT(liWidth  >= 0 && liWidth  < KI_HNG_MAP_WIDTH,  "Bad Width of ");
    CGS_ASSERT(liHeight >= 0 && liHeight < KI_HNG_MAP_HEIGHT, "Bad Height of ");
    CGS_ASSERT(mbReady, "Hard No Go Secton not ready\n");

    const MapData lMapBit = static_cast<MapData>(1u << liWidth);   // DWARF local, h:291
    mHNGMap[liHeight] |= lMapBit;
}

// ================================================================================================
// Grid <-> world
// ================================================================================================

// @0x82783088 (assert at BrnHardNoGoMap.cpp:896)
// The world point at the square's TOP-LEFT corner: interpolate the section's two side edges to
// the row's top (luHeight / 8), then interpolate across between them at the column's left edge
// (luWidth / 32). The result is assembled as (x, lfHeight, y) because the map plane is world
// (x, z): `vrlimi128 v12, v0, 2, 3` drops the 2D y into lane 2 after lane 1 was set to lfHeight.
Vector3 HardNoGoMap::GetSquareTopLeft(u32 luWidth, u32 luHeight, f32 lfHeight)
{
    CGS_ASSERT(mbReady, "Hard No Go Secton not ready");

    const f32 lfHeightInterp = static_cast<f32>(luHeight) * KF_HNG_SQUARE_HEIGHT;
    const f32 lfWidthInterp  = static_cast<f32>(luWidth)  * KF_HNG_SQUARE_WIDTH;

    const Vector2 lCurrentLeft   = GetCurrentLeft();
    const Vector2 lPreviousLeft  = GetPreviousLeft();
    const Vector2 lCurrentRight  = GetCurrentRight();
    const Vector2 lPreviousRight = GetPreviousRight();

    const Vector3 lLeft = {
        (lCurrentLeft.x - lPreviousLeft.x) * lfHeightInterp + lPreviousLeft.x,
        lfHeight,
        (lCurrentLeft.y - lPreviousLeft.y) * lfHeightInterp + lPreviousLeft.y,
        0.0f };
    const Vector3 lRight = {
        (lCurrentRight.x - lPreviousRight.x) * lfHeightInterp + lPreviousRight.x,
        lfHeight,
        (lCurrentRight.y - lPreviousRight.y) * lfHeightInterp + lPreviousRight.y,
        0.0f };

    return Vector3{ (lRight.x - lLeft.x) * lfWidthInterp + lLeft.x,
                    (lRight.y - lLeft.y) * lfWidthInterp + lLeft.y,
                    (lRight.z - lLeft.z) * lfWidthInterp + lLeft.z,
                    0.0f };
}

// @0x82783228 (assert at BrnHardNoGoMap.cpp:918)
// Identical to GetSquareTopLeft but for the column's RIGHT edge: the only difference in the two
// bodies is the `addi r8, r26, 1` at 0x827832F0, i.e. (luWidth + 1) / 32.
Vector3 HardNoGoMap::GetSquareTopRight(u32 luWidth, u32 luHeight, f32 lfHeight)
{
    CGS_ASSERT(mbReady, "Hard No Go Secton not ready");

    const f32 lfHeightInterp = static_cast<f32>(luHeight)     * KF_HNG_SQUARE_HEIGHT;
    const f32 lfWidthInterp  = static_cast<f32>(luWidth + 1u) * KF_HNG_SQUARE_WIDTH;

    const Vector2 lCurrentLeft   = GetCurrentLeft();
    const Vector2 lPreviousLeft  = GetPreviousLeft();
    const Vector2 lCurrentRight  = GetCurrentRight();
    const Vector2 lPreviousRight = GetPreviousRight();

    const Vector3 lLeft = {
        (lCurrentLeft.x - lPreviousLeft.x) * lfHeightInterp + lPreviousLeft.x,
        lfHeight,
        (lCurrentLeft.y - lPreviousLeft.y) * lfHeightInterp + lPreviousLeft.y,
        0.0f };
    const Vector3 lRight = {
        (lCurrentRight.x - lPreviousRight.x) * lfHeightInterp + lPreviousRight.x,
        lfHeight,
        (lCurrentRight.y - lPreviousRight.y) * lfHeightInterp + lPreviousRight.y,
        0.0f };

    return Vector3{ (lRight.x - lLeft.x) * lfWidthInterp + lLeft.x,
                    (lRight.y - lLeft.y) * lfWidthInterp + lLeft.y,
                    (lRight.z - lLeft.z) * lfWidthInterp + lLeft.z,
                    0.0f };
}

// @0x827833D0 (assert at BrnHardNoGoMap.cpp:939)
// Same again for the square's centre: both indices gain the 0.5f at flt_820C4168 before the
// scale (`fadds f12, f12, f0` / `fadds f13, f13, f0` at 0x827834E8/0x827834EC).
Vector3 HardNoGoMap::GetSquareCentre(u32 luWidth, u32 luHeight, f32 lfHeight)
{
    CGS_ASSERT(mbReady, "Hard No Go Secton not ready");

    const f32 lfHeightInterp = (static_cast<f32>(luHeight) + 0.5f) * KF_HNG_SQUARE_HEIGHT;
    const f32 lfWidthInterp  = (static_cast<f32>(luWidth)  + 0.5f) * KF_HNG_SQUARE_WIDTH;

    const Vector2 lCurrentLeft   = GetCurrentLeft();
    const Vector2 lPreviousLeft  = GetPreviousLeft();
    const Vector2 lCurrentRight  = GetCurrentRight();
    const Vector2 lPreviousRight = GetPreviousRight();

    const Vector3 lLeft = {
        (lCurrentLeft.x - lPreviousLeft.x) * lfHeightInterp + lPreviousLeft.x,
        lfHeight,
        (lCurrentLeft.y - lPreviousLeft.y) * lfHeightInterp + lPreviousLeft.y,
        0.0f };
    const Vector3 lRight = {
        (lCurrentRight.x - lPreviousRight.x) * lfHeightInterp + lPreviousRight.x,
        lfHeight,
        (lCurrentRight.y - lPreviousRight.y) * lfHeightInterp + lPreviousRight.y,
        0.0f };

    return Vector3{ (lRight.x - lLeft.x) * lfWidthInterp + lLeft.x,
                    (lRight.y - lLeft.y) * lfWidthInterp + lLeft.y,
                    (lRight.z - lLeft.z) * lfWidthInterp + lLeft.z,
                    0.0f };
}

// @0x82782BC0
// Rasterise one 2D segment into the grid with a DDA: map both endpoints to (column, row) in
// float square space, clamped to [0, 31] x [0, 7] (the clamps are float, and 31.0f / 7.0f, not
// the 32 / 8 the interp scale uses -- flt_820323F4 / flt_820054D0), then step from one to the
// other in max(|dx|, |dy|) + 1 whole steps, marking the square under the cursor each time.
//
// The degenerate case is explicit in the asm: if |dx| <= |dy| AND dy == 0 the step count is 1
// and the increment scale stays 0.0f (f0 is never divided), so exactly one square is marked.
void HardNoGoMap::WriteIntoMap(Vector2 lStart, Vector2 lEnd)
{
    f32 lfStartInterpX = 0.0f;
    f32 lfStartInterpY = 0.0f;
    GetHNGInterpXY(lStart, lfStartInterpX, lfStartInterpY);

    f32 lfEndInterpX = 0.0f;
    f32 lfEndInterpY = 0.0f;
    GetHNGInterpXY(lEnd, lfEndInterpX, lfEndInterpY);

    const f32 lfMaxWidth  = static_cast<f32>(KI_HNG_MAP_WIDTH  - 1);   // flt_820323F4 == 31.0f
    const f32 lfMaxHeight = static_cast<f32>(KI_HNG_MAP_HEIGHT - 1);   // flt_820054D0 ==  7.0f

    // fsel(-v, 0, v) then fsel(C - v, v, C) -- clamp low to 0, then high to C.
    f32 lfStartSquareX = lfStartInterpX * static_cast<f32>(KI_HNG_MAP_WIDTH);
    f32 lfStartSquareY = lfStartInterpY * static_cast<f32>(KI_HNG_MAP_HEIGHT);
    f32 lfEndSquareX   = lfEndInterpX   * static_cast<f32>(KI_HNG_MAP_WIDTH);
    f32 lfEndSquareY   = lfEndInterpY   * static_cast<f32>(KI_HNG_MAP_HEIGHT);

    if (lfStartSquareX <= 0.0f) { lfStartSquareX = 0.0f; }
    if (lfStartSquareY <= 0.0f) { lfStartSquareY = 0.0f; }
    if (lfEndSquareX   <= 0.0f) { lfEndSquareX   = 0.0f; }
    if (lfEndSquareY   <= 0.0f) { lfEndSquareY   = 0.0f; }

    if (lfStartSquareX > lfMaxWidth)  { lfStartSquareX = lfMaxWidth;  }
    if (lfEndSquareX   > lfMaxWidth)  { lfEndSquareX   = lfMaxWidth;  }
    if (lfStartSquareY > lfMaxHeight) { lfStartSquareY = lfMaxHeight; }
    if (lfEndSquareY   > lfMaxHeight) { lfEndSquareY   = lfMaxHeight; }

    const f32 lfDeltaX = lfEndSquareX - lfStartSquareX;
    const f32 lfDeltaY = lfEndSquareY - lfStartSquareY;

    s32 liSteps     = 1;
    f32 lfStepScale = 0.0f;

    if (fabsf(lfDeltaX) > fabsf(lfDeltaY))
    {
        liSteps     = static_cast<s32>(fabsf(lfDeltaX)) + 1;   // fctiwz truncates toward zero
        lfStepScale = 1.0f / fabsf(lfDeltaX);
    }
    else if (lfDeltaY != 0.0f)
    {
        liSteps     = static_cast<s32>(fabsf(lfDeltaY)) + 1;
        lfStepScale = 1.0f / fabsf(lfDeltaY);
    }

    const f32 lfStepX = lfDeltaX * lfStepScale;
    const f32 lfStepY = lfDeltaY * lfStepScale;

    f32 lfSquareX = lfStartSquareX;
    f32 lfSquareY = lfStartSquareY;

    for (s32 liStep = 0; liStep < liSteps; ++liStep)
    {
        SetMapSquare(static_cast<s32>(lfSquareX), static_cast<s32>(lfSquareY));

        lfSquareX += lfStepX;
        lfSquareY += lfStepY;
    }
}

// @0x82777BA0
// World position -> the map's two interpolants.
//
// lfInterpX (across the road) is solved by a FIVE-STEP BINARY SEARCH, not a projection: for a
// candidate t, lerp the exit edge and the entry edge to t and ask which side of the segment
// joining those two points lPos lies on (the 2D cross product), then step t half as far again
// toward the answer. That is the only way to invert the bilinear patch cheaply, and it is why
// the body is one unrolled-looking VMX block with `li r11, 5`.
//
// lfInterpY (along the road) is then just how far lPos is from the entry-edge point as a
// fraction of the distance between the two lerped points. A degenerate section -- the two
// points coincident to within FLT_EPSILON -- reports 0.0 rather than dividing.
void HardNoGoMap::GetHNGInterpXY(Vector2 lPos, f32& lfInterpX, f32& lfInterpY)
{
    const Vector2 lCurrentLeft   = GetCurrentLeft();
    const Vector2 lCurrentRight  = GetCurrentRight();
    const Vector2 lPreviousLeft  = GetPreviousLeft();
    const Vector2 lPreviousRight = GetPreviousRight();

    // vcfsx v13, 1, 1 == 0.5 seeds both the parameter and (squared) the first step.
    f32 lfInterp = 0.5f;
    f32 lfStep   = 0.5f * 0.5f;

    // Kept outside the loop: the post-loop length uses the LAST iteration's two points, which
    // are the ones computed before the final parameter update (vsubfp v11, v9, v11 @0x82777C20).
    f32 lfCurrentX = 0.0f;
    f32 lfCurrentY = 0.0f;
    f32 lfPreviousX = 0.0f;
    f32 lfPreviousY = 0.0f;

    for (s32 liIteration = 0; liIteration < KI_HNG_INTERP_ITERATIONS; ++liIteration)
    {
        // vmaddfp v11, (right - left), left, t  == (right - left) * t + left
        lfCurrentX = (lCurrentRight.x - lCurrentLeft.x) * lfInterp + lCurrentLeft.x;
        lfCurrentY = (lCurrentRight.y - lCurrentLeft.y) * lfInterp + lCurrentLeft.y;

        lfPreviousX = (lPreviousRight.x - lPreviousLeft.x) * lfInterp + lPreviousLeft.x;
        lfPreviousY = (lPreviousRight.y - lPreviousLeft.y) * lfInterp + lPreviousLeft.y;

        const f32 lfNextUp   = lfInterp + lfStep;
        const f32 lfNextDown = lfInterp - lfStep;
        lfStep *= 0.5f;

        // A = lPos - current point, B = previous point - current point; the side test is
        // cross(B, A) = B.x * A.y - B.y * A.x.
        const f32 lfAx = lPos.x - lfCurrentX;
        const f32 lfAy = lPos.y - lfCurrentY;
        const f32 lfBx = lfPreviousX - lfCurrentX;
        const f32 lfBy = lfPreviousY - lfCurrentY;

        const f32 lfCross = lfBx * lfAy - lfBy * lfAx;

        lfInterp = (lfCross > 0.0f) ? lfNextUp : lfNextDown;   // vcmpgtfp + vsel
    }

    lfInterpX = lfInterp;   // stfs 0(r4) @0x82777C40, before the length branch

    const f32 lfSectionLength =
        vpu::Magnitude(Vector2{ lfPreviousX - lfCurrentX, lfPreviousY - lfCurrentY, 0.0f, 0.0f });

    if (fabsf(lfSectionLength) > KF_EPSILON)   // vandc (fabs) then vcmpgtfp vs flt_820C3B70
    {
        const f32 lfAlong =
            vpu::Magnitude(Vector2{ lPos.x - lfPreviousX, lPos.y - lfPreviousY, 0.0f, 0.0f });

        lfInterpY = lfAlong / lfSectionLength;   // vrefp + 2x Newton-Raphson == the divide
    }
    else
    {
        lfInterpY = 0.0f;   // flt_82001CC0
    }
}

// @0x827687D0
// Interpolants -> integer grid indices, each scaled by its axis' square count and clamped to
// the last valid index. fctiwz truncates toward zero, which is a plain C cast. The asm stores
// the unclamped value first and overwrites it in the two clamp arms -- one assignment here.
void HardNoGoMap::ConvertInterpToIndex(f32 lfInterpX, f32 lfInterpY, s32& liWidth, s32& liHeight)
{
    liWidth = static_cast<s32>(lfInterpX * static_cast<f32>(KI_HNG_MAP_WIDTH));  // flt_82013FA8
    if (liWidth > KI_HNG_MAP_WIDTH - 1)
    {
        liWidth = KI_HNG_MAP_WIDTH - 1;
    }
    else if (liWidth < 0)
    {
        liWidth = 0;
    }

    liHeight = static_cast<s32>(lfInterpY * static_cast<f32>(KI_HNG_MAP_HEIGHT)); // flt_820C41E0
    if (liHeight > KI_HNG_MAP_HEIGHT - 1)
    {
        liHeight = KI_HNG_MAP_HEIGHT - 1;
    }
    else if (liHeight < 0)
    {
        liHeight = 0;
    }
}
}
