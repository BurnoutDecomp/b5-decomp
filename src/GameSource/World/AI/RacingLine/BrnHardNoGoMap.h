#ifndef BRN_HARD_NO_GO_MAP_H
#define BRN_HARD_NO_GO_MAP_H

// BrnAI::HardNoGoMap -- a fixed 32x8 bit grid marking "hard no-go" squares along
// a stretch of track for the racing-line / AI route code. Each of the 8 rows
// (height index) is one 32-bit word; each of the 32 columns (width index) is one
// bit. The map is filled by Spread*/WriteIntoMap and queried by
// MapSquareOccupiedFast; SetMapSquare marks a single square occupied.
//
// LAYOUT pinned from the X360 asm of MapSquareOccupiedFast (@0x82764898) and
// SetMapSquare (@0x82764A80):
//   * the row words are read/written at byte 4*(height+8), i.e. mauMap[height]
//     sits at +0x20 (row words 8..15 of the object), one u32 per row;
//   * the "section ready" flag is a byte at +0x48 (lbz 0x48; asserted non-zero
//     with "Hard No Go Secton not ready").
// The 0x20-byte head and the two-word gap between the map and the ready flag are
// other members of this class (Spread*/WriteIntoMap/Render live in their own
// TUs and are not modelled here); they are kept as honest opaque padding sized
// only to the offsets the bodied asm attests. Members reached by name.

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector2 / Vector4 (rw::math::vpu aliases)

namespace BrnAI
{
struct AISection;   // fwd (SharedClasses/AI/AISectionsResourceType.h)

class HardNoGoMap
{
public:
    static const u32 KU_WIDTH  = 32; // columns: one bit per width index (0..31)
    static const u32 KU_HEIGHT = 8;  // rows:    one 32-bit word per height index (0..7)

    // Bodied in the (not-yet-committed) rest of the HardNoGoMap TU; declared here
    // so the RacingLineGenerator callers (SetupSectionExit / DropHardNoGoLinesIntoMap)
    // resolve. Shapes are DWARF-authoritative (BrnHardNoGoMap.h:50 / :86).
    //   @0x82782F80 -- build the map from an AISection over the [start,end) line
    //   budget; returns true once the whole section has been placed.
    bool MakeMap(const AISection* lpAISection, s32 liStart, s32 liEnd);
    //   @0x82783AA8 -- return the two maximal (left/right) edge points of the
    //   occupied region at the given height interpolant.
    void FindMaximalEdges(Vector2& lLeftEdge, Vector2& lRightEdge, f32 lfHeightInterp);

    // @0x82764898 -- is the (width,height) square marked occupied? Returns the
    // bit (mauMap[height] & (1<<width)) as a bool (the X360 spells the return
    // BOOL; the asm yields 1/0 from a single-bit test).
    bool MapSquareOccupiedFast(u32 luWidth, u32 luHeight) const;

    // @0x82764A80 -- mark the (width,height) square occupied:
    // mauMap[height] |= (1<<width).
    void SetMapSquare(u32 luWidth, u32 luHeight);

    // RacingLine::ClearSectionCache @0x8276E090 resets miSectionIndex / mbReady by name.
    friend class RacingLine;

private:
    // DWARF BrnHardNoGoMap.h -- the two edge-history vectors that head the object (two
    // Vector4 slots, +0x00 / +0x10); the map row words begin at +0x20.
    Vector4 mCurrentAndPreviousLeft;      // +0x00
    Vector4 mCurrentAndPreviousRight;     // +0x10

    // One 32-bit row per height index; bit (1<<width) per column.
    u32 mauMap[KU_HEIGHT];               // +0x20 .. +0x3F

    // DWARF BrnHardNoGoMap.h -- the section this map was built for (ClearSectionCache
    // resets it to 9999 == "no section") and its average width.
    s32 miSectionIndex;                  // +0x40
    f32 mfAverageWidth;                  // +0x44

    // Set once the section's map has been built; MapSquareOccupiedFast /
    // SetMapSquare assert this is non-zero ("Hard No Go Secton not ready").
    bool mbReady;                        // +0x48
    // (16-byte SIMD alignment of the Vector4 head pads the object to 0x50 -- the stride
    //  SectionData @0x50 .. @0xA0 attests.)
};

static_assert(sizeof(HardNoGoMap) == 0x50, "HardNoGoMap is 0x50 bytes (SectionData +0x50 .. +0xA0)");
}

#endif
