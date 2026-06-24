#pragma once

// BrnCarouselSliderBar.h
// Home of BrnGui::CarouselSliderBar -- the scroll-bar overlay for the car-select carousel.
// Each Update publishes the bar's start/end extents (as integer percentages of the item
// count) into the "BarStart" / "BarEnd" apt view states, and -- when its visibility was
// just changed -- the "vis_state" / "apt_updatestate" view states. Derives from
// CgsGui::GuiComponent (the apt-view-state output plumbing). Layout/method set from the
// DecFIGS DWARF (BrnCarouselSliderBar.h:44/:72/:73).
//
// LAYOUT (X360 authoritative; offsets are the store displacements in Construct/SetVisible):
//   +0x8C  mbVisible        -- bool (stb at 0x8C)
//   +0x8D  mbAptUpdatestate -- bool: set whenever visibility changes so the next Update
//                             re-publishes the vis_state. (stb at 0x8D)

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h" // CgsGui::GuiComponent base

namespace CgsGui { class StateInterface; }

namespace BrnGui
{
    struct CarouselSliderBar : public CgsGui::GuiComponent
    {
        // BrnCarouselSliderBar.cpp:48 @ 0x8241BB50 -- base Construct then init both flags.
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpacParentName) override;

        // BrnCarouselSliderBar.cpp:68 @ 0x8241BB90 -- publish the bar extents (and, when
        // dirty, the visibility) into the carousel's apt view states.
        // liFirstItem is the leading visible item index; liItemCount is the total count.
        void Update(s32 liFirstItem, s32 liItemCount);

        // BrnCarouselSliderBar.cpp:113 @ 0x8241BD18 -- set visibility and flag the
        // view-state as needing a refresh.
        void SetVisible(bool lbVisible);

    private:
        bool mbVisible;          // +0x8C
        bool mbAptUpdatestate;   // +0x8D
    };
}
