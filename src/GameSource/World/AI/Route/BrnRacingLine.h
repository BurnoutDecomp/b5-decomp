#ifndef BRN_RACING_LINE_H
#define BRN_RACING_LINE_H

#include "types.hpp"

// BrnAI::RacingLine -- the per-driver racing-line working state owned by
// BrnAI::AIDriver (ClearSectionCache is called from AIDriver::Prepare and
// AIDriver::InitialiseRacingLine). Reconstructed from BURNOUT_X360_ARTIST.XEX;
// no prior source and no DecFIGS DWARF for this TU, so the layout is recovered
// store-for-store from the ClearSectionCache asm (@0x8276E090) and every
// recovered field is accessed by name. Spans the binary touches but whose
// purpose is not recoverable from this TU are kept as explicit named padding
// (no raw offset casts).
//
// ClearSectionCache resets a 16-entry section cache (stride 0xB0 == 176 bytes,
// first entry's leading word at guest offset 0xA0) and three trailing
// scalar fields at guest offsets 0xBC0/0xBC4/0xBC8.

namespace BrnAI
{
// The section-cache generator reaches the trailing cursor/index/flag triple by
// name (SetupSectionExit / DropHardNoGoLinesIntoMap live in BrnRacingLineGenerator).
class RacingLineGenerator;

class RacingLine
{
public:
    static const s32 KI_SECTION_CACHE_COUNT = 16;

    // @0x8276E090  Reset every section-cache entry to its empty sentinels and
    // clear the trailing cursor/index/flag triple. Returns this (the X360
    // calling convention threads the object pointer back through r3).
    RacingLine* ClearSectionCache();

private:
    // One section-cache entry. 176 bytes (0xB0) on the guest; the recovered
    // scalar slots are named, the remainder is preserved as padding.
    struct SectionCacheEntry
    {
        u32 muDistance;     // +0x00  reset to 9999 (KU_DISTANCE_EMPTY)
        u8  mPad04[4];      // +0x04
        u8  muFlagA;        // +0x08  reset to 0
        u8  mPad09[15];     // +0x09
        u16 muCost;         // +0x18  reset to 999  (KU_COST_EMPTY)
        u8  muFlagB;        // +0x1A  reset to 0
        u8  mPad1B[149];    // +0x1B .. +0xAF  (entry is 0xB0 bytes)
    };

    static const u16 KU_COST_EMPTY     = 999;
    static const u32 KU_DISTANCE_EMPTY = 9999;

    u8                mPadHead[0xA0];                       // guest 0x00 .. 0xA0
    SectionCacheEntry maSectionCache[KI_SECTION_CACHE_COUNT]; // 0xA0 .. 0xBA0
    u8                mPadGap[0x20];                        // 0xBA0 .. 0xBC0

    // Trailing scalar triple cleared after the cache loop.
    s32 miCursor;      // 0xBC0  -> 0
    s32 miActiveIndex; // 0xBC4  -> -1
    s32 miFlags;       // 0xBC8  -> 0

    friend class RacingLineGenerator;
};
}

#endif
