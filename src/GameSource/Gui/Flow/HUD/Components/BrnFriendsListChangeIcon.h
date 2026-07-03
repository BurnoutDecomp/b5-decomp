#pragma once

#include "types.hpp"
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptIconComponent.h"   // FlaptIconComponent (base)

// BrnGui::FriendsListChangeIconComponent - the little "friends list changed"
// HUD icon: a FlaptIconComponent plus one visibility latch, stepped between the
// "transin" / "idle" / "invisible" timeline labels by the HUD states
// (FBurnMainHudState / RaceMainHudState drive AnimateIn / ShowNow / Hide).
// Shape asm-derived from the five exported bodies (no DWARF hints for this TU);
// X360 layout: the FlaptIconComponent base (vptr +0x00, iface +0x04, clip +0x08,
// state hash +0x10) plus mbShowing @+0x14.
namespace BrnGui
{
    class FriendsListChangeIconComponent : public FlaptIconComponent
    {
    public:
        // @0x82423CB0 (this TU) -- the base Construct (the X360 re-inlines the
        // FlaptComponent bind + clip clear + hash reset) plus the latch clear.
        virtual void Construct(const void* lpDEBUGName,
                               CgsGui::StateInterface* lpStateInterface,
                               const void* lpcParentName);

        // @0x82423D10 (this TU) -- the base Prepare re-inlined verbatim (bind the
        // clip, reset its timeline, reset the state hash); no derived state touched.
        void Prepare(const char* lacName, const BrnFlapt::FileRef& lFile);

        // @0x82414C50 (this TU) -- animate the icon in ("transin" through the live
        // SetState virtual) exactly once until hidden again.
        void AnimateIn();

        // @0x82414CA8 (this TU) -- pop the icon straight to its "idle" frame (the
        // init-wait path), same latch.
        void ShowNow();

        // @0x82414D00 (this TU) -- drop to "invisible" and clear the latch.
        void Hide();

    private:
        bool mbShowing;   // X360 +0x14 (DWARF name; the show/hide latch)
    };
}
