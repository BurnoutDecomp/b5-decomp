#include "GameSource/Gui/Flow/Screen/Components/BrnEATraxMenuComponent.h"

// BrnGui::EATraxMenuComponent - per-track mode get/set over two parallel FastBitArray<128>
// bit sets. See BrnEATraxMenuComponent.h for the recovered layout and X360 addresses.

namespace BrnGui
{
    // @0x82426A60 - decode the 2-bit mode for liTrackIndex. The X360 asserts the index
    // against KI_MAX_TRAX_IN_GAME first (BrnEATraxMenuComponent.h:281), then reads the
    // high bit (set at +0xB0) and the low bit (set at +0xA0). Both contribute INVERTED:
    //   result = (high ? 0 : 2) + (low ? 0 : 1)
    // (the X360 repeated 128-bound checks are FastBitArray's own inlined bounds asserts,
    // owned by the committed container; not duplicated here).
    s32 EATraxMenuComponent::GetTrackMode(s32 liTrackIndex) const
    {
        CGS_ASSERT(liTrackIndex >= 0 && liTrackIndex < KI_MAX_TRAX_IN_GAME,
                   "(liTrackIndex >= 0) && (liTrackIndex < KI_MAX_TRAX_IN_GAME)");

        const u32 luIndex = static_cast<u32>(liTrackIndex);
        const bool lbHigh = maTrackModeHighBits.IsBitSet(luIndex);
        const bool lbLow  = maTrackModeLowBits.IsBitSet(luIndex);

        const s32 liHighPart = lbHigh ? 0 : 2;
        const s32 liLowPart  = lbLow  ? 0 : 1;
        return liHighPart + liLowPart;
    }

    // @0x82426DA0 - encode leMode into the high/low bit sets (inverted, see GetTrackMode):
    //   mode 0 -> high=set,   low=set
    //   mode 1 -> high=set,   low=clear
    //   mode 2 -> high=clear, low=set
    //   mode 3 -> high=clear, low=clear
    // Each X360 case writes the high set (+0xB0) then the low set (+0xA0); an out-of-range
    // mode fires "Invalid mode" (BrnEATraxMenuComponent.h:341).
    EATraxMenuComponent& EATraxMenuComponent::SetTrackMode(s32 liTrackIndex, s32 leMode)
    {
        const u32 luIndex = static_cast<u32>(liTrackIndex);

        switch (leMode)
        {
            case E_TRACKMODE_0:   // high=set, low=set
                maTrackModeHighBits.SetBit(luIndex);
                maTrackModeLowBits.SetBit(luIndex);
                break;
            case E_TRACKMODE_1:   // high=set, low=clear
                maTrackModeHighBits.SetBit(luIndex);
                maTrackModeLowBits.UnSetBit(luIndex);
                break;
            case E_TRACKMODE_2:   // high=clear, low=set
                maTrackModeHighBits.UnSetBit(luIndex);
                maTrackModeLowBits.SetBit(luIndex);
                break;
            case E_TRACKMODE_3:   // high=clear, low=clear
                maTrackModeHighBits.UnSetBit(luIndex);
                maTrackModeLowBits.UnSetBit(luIndex);
                break;
            default:
                CGS_ASSERT(false, "Invalid mode");
                break;
        }
        return *this;
    }
}
