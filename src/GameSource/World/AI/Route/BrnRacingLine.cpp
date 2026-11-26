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
// and clears the spread cursor triple at this+0xBC0/0xBC4/0xBC8 on every pass (ARTIST stores
// them inside the loop, before the bne at 0x8276E0D0).
//
// DWARF (_compile/BrnWorldAIUnity.cpp:4486): void, defined at BrnRacingLine.h:452 with the local
// `int32_t lu32SectionIndex` and, in the loop's block, `const int32_t KI_CACHE_ENTRY_UNUSED = 999`
// (h:456). Kept in this .cpp: ARTIST's out-of-line copy is what AIDriver::Prepare 0x82792CA8 and
// InitialiseRacingLine 0x82792DF0 call.
// The HardNoGoMap's two members are private (DWARF BrnHardNoGoMap.h:239/241): the loop calls
// HardNoGoMap::Prepare(KI_NO_SECTION) -- PS3 0x9B4DC0 (Prepare) lists AIDriver::Prepare and
// InitialiseRacingLine, the inliners of ClearSectionCache, among its callers. ARTIST's unity TU
// inlines Prepare here (stw 9999 / stb 0 at 0x8276E0B4 / 0x8276E0BC); a lone compile calls it.

namespace BrnAI
{
void RacingLine::ClearSectionCache()
{
    int32_t lu32SectionIndex;
    for (lu32SectionIndex = 0; lu32SectionIndex < KI_RACING_LINE_MAX_AVAILABLE_SECTIONS; ++lu32SectionIndex)
    {
        const int32_t KI_CACHE_ENTRY_UNUSED = 999;
        maSectionCache[lu32SectionIndex].mCachedSectionIndex = KI_CACHE_ENTRY_UNUSED;
        maSectionCache[lu32SectionIndex].mbTargetUpToDate = false;
        maSectionCache[lu32SectionIndex].mHardNoGoMap.Prepare(KI_NO_SECTION);
        miSectionToSpread = 0;
        miBackwardsStep = -1;
        miHNGLineStart = 0;
    }
}
}
