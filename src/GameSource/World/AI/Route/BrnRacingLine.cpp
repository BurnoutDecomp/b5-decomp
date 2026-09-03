#include "BrnRacingLine.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x8276E090
//   BrnAI::RacingLine::ClearSectionCache
//
// Walks the 16-entry section cache (r11 = this + 0xA8, stride 0xB0) writing four sentinels per
// entry -- named against the DWARF SectionData layout (entry 0 @ this+0x10, HardNoGoMap @
// entry+0x50):
//   stw 0x270F (9999) @ r11-0x08  -> entry+0x90 == mHardNoGoMap.miSectionIndex
//   stb 0             @ r11+0x00  -> entry+0x98 == mHardNoGoMap.mbReady
//   sth 0x3E7  (999)  @ r11+0x10  -> entry+0xA8 == mCachedSectionIndex
//   stb 0             @ r11+0x12  -> entry+0xAA == mbTargetUpToDate
// and (inside the loop on the console, hoisted here -- same final state) clears the spread
// cursor triple at this+0xBC0/0xBC4/0xBC8.

namespace BrnAI
{
RacingLine* RacingLine::ClearSectionCache()
{
    for (s32 li = 0; li < KI_SECTION_CACHE_COUNT; ++li)
    {
        SectionData& lrEntry = maSectionCache[li];
        lrEntry.mCachedSectionIndex        = static_cast<s16>(KU_COST_EMPTY);  // sth 999
        lrEntry.mbTargetUpToDate           = false;                            // stb 0
        lrEntry.mHardNoGoMap.miSectionIndex = KI_DISTANCE_EMPTY;               // stw 9999
        lrEntry.mHardNoGoMap.mbReady       = false;                            // stb 0
    }

    miSectionToSpread = 0;
    miBackwardsStep   = -1;
    miHNGLineStart    = 0;

    return this;
}
}
