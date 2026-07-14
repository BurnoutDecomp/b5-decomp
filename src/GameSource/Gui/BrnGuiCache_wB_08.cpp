#include "GameSource/Gui/BrnGuiCache.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX (GuiCache accessor wave, part 08).
// Three snapshot/index accessors, each a thin read of one named far member guarded
// by the game's debug assert (CGS_ASSERT is a no-op in this build, matching the X360
// release assert machinery). Offsets / branch senses are taken straight from the ARTIST
// asm; members are accessed BY NAME against the recovered GuiCache layout in BrnGuiCache.h.

namespace BrnGui
{
    // @ 0x82472E78 -- maRoadRuleActiveByType[liRoadRuleType] (@0xAC44, idx 0..1). The X360
    // brackets the read with two debug asserts (>= 0 and < E_SCORE_TYPE_COUNT == 2), each
    // building an "Invalid score type : <n>" message that the no-op assert discards.
    bool GuiCache::IsRoadRuleActive(s32 liRoadRuleType) const
    {
        CGS_ASSERT(liRoadRuleType >= 0, "Invalid score type");
        CGS_ASSERT(liRoadRuleType < 2, "Invalid score type");
        return maRoadRuleActiveByType[liRoadRuleType];
    }

    // @ 0x82472FD0 -- bump the sat-nav zoom level (miSatNavZoomLevel @0x803C / result[8207]),
    // assert it did not overrun E_SAT_NAV_ZOOM_COUNT (2), then clamp the stored value to a max
    // of 1. The X360 re-reads the member after the increment and writes back the clamped value.
    void GuiCache::ZoomSatNavOut()
    {
        ++miSatNavZoomLevel;
        CGS_ASSERT(miSatNavZoomLevel <= 2, "leEnumIndex <= E_SAT_NAV_ZOOM_COUNT");
        if (miSatNavZoomLevel >= 1)
        {
            miSatNavZoomLevel = 1;
        }
    }

    // @ 0x824827D8 -- resolve the fly-by pre-event record at liIndex: assert the index is in
    // [0, mPreRaceData.miNumMessages), then return &maPreEventInfo[liIndex] (stride 580 storage
    // @0x12F0C, count miNumMessages @0x135D8). X360: return 580 * liIndex + this + 77580.
    const PreEventInfo* GuiCache::GetPreEventInfo(s32 liIndex) const
    {
        CGS_ASSERT(liIndex >= 0, "liIndex >= 0");
        CGS_ASSERT(liIndex < miNumMessages, "liIndex < mPreRaceData.miNumMessages");
        return reinterpret_cast<const PreEventInfo*>(maPreEventInfoStorage[liIndex]);
    }
}
