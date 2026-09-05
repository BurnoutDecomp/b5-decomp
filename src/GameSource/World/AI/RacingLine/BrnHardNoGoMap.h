#ifndef BRN_HARD_NO_GO_MAP_H
#define BRN_HARD_NO_GO_MAP_H

// BrnAI::HardNoGoMap -- a fixed 32x8 bit grid marking "hard no-go" squares over one AI
// section of track, used by the racing-line generator and the AI steering fan. Each of the
// 8 rows (height index) is one 32-bit word; each of the 32 columns (width index) is one bit.
//
// The grid is not axis-aligned in world space: it is a bilinear patch spanned by four
// corner points -- the section's entry portal edge ("previous" left/right) and its exit
// portal edge ("current" left/right) -- which SetCorners packs two-per-Vector4:
//   mCurrentAndPreviousLeft  = (currentLeft.x,  currentLeft.y,  previousLeft.x,  previousLeft.y)
//   mCurrentAndPreviousRight = (currentRight.x, currentRight.y, previousRight.x, previousRight.y)
// A world position is mapped to the grid by GetHNGInterpXY (interpX across the road,
// interpY along it) and then to integer indices by ConvertInterpToIndex. The map's 2D
// (x, y) IS the world (x, z) ground plane -- GetSquareTopLeft/TopRight/Centre rebuild a
// Vector3 as (x, lfHeight, y), and FindMaximalEdges copies lane 2 (z) of that Vector3 into
// lane 1 (y) of its Vector2 outs.
//
// SHAPE: DecFIGS DWARF GameSource/World/AI/RacingLine/BrnHardNoGoMap.h (172 lines, ~38
// methods). Every declaration below is gated on the X360 ledger; the ones the X360 has no
// export for are marked. The DWARF declares NO const method on this class (0 hits for
// "const;" in the dump, against 4 in the sibling BrnRacingLine.h), so nothing here is const.
//
// LAYOUT pinned from the X360 asm, unchanged by the 2026-09-05 grow:
//   +0x00 mCurrentAndPreviousLeft   (lvx128 v*, r0, this      -- SetCorners @0x82777840)
//   +0x10 mCurrentAndPreviousRight  (lvx128 v*, this, 0x10    -- ditto)
//   +0x20 mHNGMap[8]                (lwzx/stwx @ 4*(height+8) -- MapSquareOccupiedFast
//                                    @0x82764898 / SetMapSquare @0x82764A80)
//   +0x40 miSectionIndex            (stw @0x40 of the map base -- RacingLineGenerator::
//                                    SetUpHardNoGoMap @0x82780574; RacingLine::
//                                    ClearSectionCache writes 9999 at entry+0x90)
//   +0x44 mfAverageWidth            (stfs 0x44(r3)            -- SetCorners)
//   +0x48 mbReady                   (lbz/stb 0x48             -- every ready assert)
// 16-byte SIMD alignment of the Vector4 head pads the object to 0x50, the stride
// SectionData attests (mHardNoGoMap @ +0x50 .. +0xA0).
//
// NOTE ON THE ROW MEMBER: the pre-grow header spelled the row array `u32 mauMap[8]` with
// class-scope KU_WIDTH/KU_HEIGHT. The DWARF names it `MapData mHNGMap[8]` with
// `typedef int32_t MapData` and the two counts as namespace constants
// KI_HNG_MAP_WIDTH/KI_HNG_MAP_HEIGHT, and the SIGNEDNESS IS LOAD-BEARING:
// DistanceToHardNoGoEdge @0x82777D60 grows its search mask with `srawi` (an ARITHMETIC
// right shift), which only reproduces on a signed row word. The old spellings had no user
// outside this TU (RacingLine.cpp reaches only miSectionIndex / mbReady), so they are
// renamed to the DWARF names rather than kept as aliases.

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector2 / Vector3 / Vector4 (rw::math::vpu aliases)

// DWARF BrnHardNoGoMap.h:24 -- one map row word. int32_t, not uint32_t (see the note above).
typedef s32 MapData;

// DWARF BrnHardNoGoMap.h:26 -- declared at GLOBAL scope in the dump (namespace ::). The
// "this map is not built for any section" sentinel; RacingLine::ClearSectionCache writes it
// into every cached section's miSectionIndex (stw 0x270F @ entry+0x90, @0x8276E090).
const s32 KI_NO_SECTION = 9999;

namespace BrnAI
{
struct AISection;      // SharedClasses/AI/AISectionsResourceType.h

// DWARF BrnHardNoGoMap.h:30 / :31. Attested by the asm: the range asserts compare against
// 0x20 and 8, the interp scales are the rodata 32.0f / 8.0f (flt_82013FA8 / flt_820C41E0)
// and the WriteIntoMap clamps are 31.0f / 7.0f (flt_820323F4 / flt_820054D0).
const s32 KI_HNG_MAP_WIDTH  = 32;
const s32 KI_HNG_MAP_HEIGHT = 8;

class HardNoGoMap
{
public:
    // ---- section lifecycle -------------------------------------------------------------
    // :38 -- point the map at a section and mark it not-yet-built. NO IDA export: inlined by
    // RacingLineGenerator::SetUpHardNoGoMap @0x82780574..0x82780578
    // (`stw <sectionIndex>, 0x40(map)` ; `stb 0, 0x48(map)`).
    void Prepare(s32 liSectionIndex);

    // :45 @0x82777840 -- pack the section's four portal corners into the two Vector4s and
    // set mfAverageWidth to the mean of the two portal widths.
    void SetCorners(Vector2 lCurrentLeft, Vector2 lCurrentRight,
                    Vector2 lPreviousLeft, Vector2 lPreviousRight);

    // :52 @0x82782F80 -- rasterise the section's no-go lines [liStart, liEnd) into the grid.
    // liEnd is clamped to the section's line count; returns true once the whole section has
    // been placed, so the caller can stop budgeting frames at it.
    bool MakeMap(const AISection* lpSection, s32 liStart, s32 liEnd);

    // :54 -- fill every row with the two outer-column bits and mark the map ready. NO IDA
    // export: inlined by RacingLineGenerator::SetUpHardNoGoMap @0x82780608..0x8278062C
    // (`li 0x80000001` stored over 8 words from map+0x20, then `stb 1, 0x48`).
    void ClearMap();

    // :58 @0x82783740 -- debug render of the whole grid (walks MapSquareOccupiedFast +
    // GetSquareCentre + RenderHNGSquare). PARKED: presentation-only on this host.
    void Render(Vector3 lPosition);

    // :61 -- NO IDA export (inlined; e.g. RacingLineGenerator::SpreadHNGBackOneStep
    // @0x8278F79C reads `lbz 0x98(sectionData)` == map+0x48).
    bool IsReady() { return mbReady; }

    // ---- spreading ---------------------------------------------------------------------
    // :66 @0x82782E08 -- walk the grid from the top row down, flooding lfSpreadDistance out
    // of every occupied square into lpafStretchDistance[32] and marking the squares it
    // reaches. Zero-fills lpafStretchDistance first.
    void SpreadHNGAlongTrack(f32* lpafStretchDistance, f32 lfSpreadDistance);

    // :70 @0x82777958 -- continue an in-flight spread into THIS (earlier) section's grid,
    // consuming the per-column budget left in lpafStretchDistance[32].
    void SpreadHNGIntoPreviousSection(f32* lpafStretchDistance);

    // :103 -- NO IDA export, not reached by any recovered caller.
    // [FLAG PC bring-up] declared-only; see the banner in the .cpp.
    void CheckForContinuedSpreading(f32* lpafStretchDistance, s32 liSectionIndex);

    // ---- queries -----------------------------------------------------------------------
    // :98 @0x82777E58 -- length of the section along the lfInterp'th line across it:
    // Lerp(|currentLeft - previousLeft|, |currentRight - previousRight|, lfInterp).
    f32 SectionLength(f32 lfInterp);

    // :109 @0x82783AA8 -- the outermost free-square boundaries on the row at lfHeightInterp.
    // lLeftEdge  <- GetSquareTopLeft  of the first free square scanning DOWN from index 31,
    // lRightEdge <- GetSquareTopRight of the first free square scanning UP   from index 0;
    // when a side finds no free square it falls back to index 31 / 0 respectively.
    void FindMaximalEdges(Vector2& lLeftEdge, Vector2& lRightEdge, f32 lfHeightInterp);

    // :113 @0x82783C28 -- |lRightEdge - lLeftEdge| of the FindMaximalEdges pair.
    f32 GetEstimatedRoadWidth(f32 lfHeightInterp);

    // :131 @0x82777D60 -- occupancy of the square lPos falls in, plus (through lfDistance)
    // the ring distance in map-width units to the nearest square of the OPPOSITE occupancy.
    bool DistanceToHardNoGoEdge(Vector2 lPos, f32& lfDistance);

    // :126 @0x82764898 -- raw bit test, no range clamping (it asserts instead).
    bool MapSquareOccupiedFast(s32 liWidth, s32 liHeight);

    // :75 / :83 / :87 / :94 -- NO IDA export, not reached by any recovered caller.
    // [FLAG PC bring-up] declared-only; see the banner in the .cpp.
    bool MoveTargetToLegalPosition(Vector2& lTarget, f32 lfRadius);
    bool GetNearestEdges(Vector2 lPos, Vector2& lLeftEdge, Vector2& lRightEdge, f32 lfHeight);
    bool IsSquareOccupied(Vector2 lPos);
    bool FindNearestSpaceForTarget(s32 liWidth, s32 liHeight, Vector2& lTarget, f32 lfRadius);

    // ---- trivial accessors (all inlined on the console; DWARF :116..:151) ---------------
    s32  GetSectionIndex()                   { return miSectionIndex; }           // :116
    void SetSectionIndex(s32 liSectionIndex) { miSectionIndex = liSectionIndex; } // :120

    // :139 / :142 / :145 / :148 -- the four packed corners. The X360 unpacks the "previous"
    // pair with `vpermwi128 v, v, 0xBF` (lanes z,w -> x,y); only lanes x/y are ever read, so
    // the z/w lanes (which the console leaves as a copy of w) are published as zero here.
    Vector2 GetPreviousLeft()  { return Vector2{ mCurrentAndPreviousLeft.z,  mCurrentAndPreviousLeft.w,  0.0f, 0.0f }; }
    Vector2 GetPreviousRight() { return Vector2{ mCurrentAndPreviousRight.z, mCurrentAndPreviousRight.w, 0.0f, 0.0f }; }
    Vector2 GetCurrentLeft()   { return Vector2{ mCurrentAndPreviousLeft.x,  mCurrentAndPreviousLeft.y,  0.0f, 0.0f }; }
    Vector2 GetCurrentRight()  { return Vector2{ mCurrentAndPreviousRight.x, mCurrentAndPreviousRight.y, 0.0f, 0.0f }; }

    f32 GetAverageRoadWidth()  { return mfAverageWidth; }                         // :151

    // RacingLine::ClearSectionCache @0x8276E090 resets miSectionIndex / mbReady by name.
    friend class RacingLine;

private:
    // :163 @0x82768858 -- range-tolerant bit test: OUT OF RANGE READS AS OCCUPIED (the asm
    // folds the four signed bounds into two `cmplwi` unsigned compares and returns 1).
    bool MapSquareOccupied(s32 liWidth, s32 liHeight);

    // :169 / :175 / :181 -- world position of a grid square's corner / centre at world
    // height lfHeight. Returned as (x, lfHeight, y) -- the map plane is world (x, z).
    Vector3 GetSquareTopLeft(u32 luWidth, u32 luHeight, f32 lfHeight);   // @0x82783088
    Vector3 GetSquareTopRight(u32 luWidth, u32 luHeight, f32 lfHeight);  // @0x82783228
    Vector3 GetSquareCentre(u32 luWidth, u32 luHeight, f32 lfHeight);    // @0x827833D0

    // :191 @0x82764A80 -- set the (width, height) bit.
    void SetMapSquare(s32 liWidth, s32 liHeight);

    // :196 @0x82782BC0 -- DDA-rasterise one 2D segment into the grid.
    void WriteIntoMap(Vector2 lStart, Vector2 lEnd);

    // :201 @0x82777BA0 -- world position -> (across, along) grid interpolants.
    void GetHNGInterpXY(Vector2 lPos, f32& lfInterpX, f32& lfInterpY);

    // :207 @0x827687D0 -- interpolants -> clamped integer grid indices.
    void ConvertInterpToIndex(f32 lfInterpX, f32 lfInterpY, s32& liWidth, s32& liHeight);

    // :158 / :186 / :212 / :220 / :226 -- NO IDA export, not reached by any recovered
    // caller. [FLAG PC bring-up] declared-only; see the banner in the .cpp.
    // (:234 RenderHNGSquare(s32, s32, s32, f32, RGBA) is deliberately NOT declared -- it is
    //  debug-render-only AND its RGBA parameter has no single canonical home in this tree.)
    bool    MapSquareOccupied(f32 lfInterpX, f32 lfInterpY);
    void    SetMapSquare(f32 lfInterpX, f32 lfInterpY);
    bool    IsInRange(f32 lfInterpX, f32 lfInterpY);
    bool    FindSpaceForTarget(s32 liWidth, s32 liHeight, s32 liRange, Vector2& lTarget, f32 lfRadius);
    Vector3 ConvertInterpToPosition(f32 lfInterpX, f32 lfInterpY, f32 lfHeight);

    // ---- storage (DWARF declaration order; offsets pinned by the asm, see the banner) ----
    Vector4 mCurrentAndPreviousLeft;      // :135 (+0x00) current in x/y, previous in z/w
    Vector4 mCurrentAndPreviousRight;     // :136 (+0x10) current in x/y, previous in z/w
    MapData mHNGMap[KI_HNG_MAP_HEIGHT];   // :238 (+0x20) one signed row word per height
    s32     miSectionIndex;               // :239 (+0x40) KI_NO_SECTION when unbuilt
    f32     mfAverageWidth;               // :240 (+0x44)
    bool    mbReady;                      // :241 (+0x48)
};

static_assert(sizeof(HardNoGoMap) == 0x50, "HardNoGoMap is 0x50 bytes (SectionData +0x50 .. +0xA0)");
}

#endif
