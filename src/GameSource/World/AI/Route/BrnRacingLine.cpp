#include "BrnRacingLine.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x8276E090
//   BrnAI::RacingLine::ClearSectionCache
//
// Walks the 16-entry section cache (r11 = this + 0xA8, stride 0xB0) writing the
// four per-entry sentinels each pass:
//   sth 0x3E7 (999)  @ r11+0x10  -> entry.muCost   (halfword)
//   stb 0    (0)     @ r11+0x12  -> entry.muFlagB  (byte)
//   stw 0x270F(9999) @ r11-0x08  -> entry.muDistance (word)
//   stb 0    (0)     @ r11+0x00  -> entry.muFlagA  (byte)
// (r11 == entry + 0xA8, so r11-0x08 == entry+0xA0 == entry base +0x00, etc.)
// and finally clears the trailing scalar triple at this+0xBC0/0xBC4/0xBC8.

namespace BrnAI
{
RacingLine* RacingLine::ClearSectionCache()
{
    for (s32 li = 0; li < KI_SECTION_CACHE_COUNT; ++li)
    {
        SectionCacheEntry& lrEntry = maSectionCache[li];
        lrEntry.muCost     = KU_COST_EMPTY;     // sth 999
        lrEntry.muFlagB    = 0;                 // stb 0
        lrEntry.muDistance = KU_DISTANCE_EMPTY; // stw 9999
        lrEntry.muFlagA    = 0;                 // stb 0
    }

    miCursor      = 0;
    miActiveIndex = -1;
    miFlags       = 0;

    return this;
}
}
