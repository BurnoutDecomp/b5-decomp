#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"   // CgsGui::GuiComponent (base)
#include "GameSource/Gui/BrnGuiTextField.h"                     // BrnGui::TextField (embedded)

// ============================================================================
// GameSource/Gui/Flow/hud/Components/BrnPositionIndicator.h
//
// BrnGui::PositionIndicator - the race HUD's "1st/2nd/..." position readout: a
// GuiComponent wrapping one TextField ("Text_txt" under "PositionIndicator_mc")
// plus the visible/loaded/trans-in latches driving the clip's "apt_state" frame.
// NO DecFIGS DWARF for this TU; shape asm-derived from the four exported bodies
// (the SetPosition asserts name the original source
// GameSource/Gui/Flow/hud/Components/BrnPositionIndicator.cpp:145/:146).
// X360 offsets in comments (mRacePositionTextField +0x8C, the latches +0x1B4/+0x1B5/+0x1B6);
// access is BY NAME.
// ============================================================================

namespace BrnGui
{
    class PositionIndicator : public CgsGui::GuiComponent
    {
    public:
        // @0x82412340 (this TU) -- base Construct, then the embedded text field
        // ("Text_txt" under the "PositionIndicator_mc" parent -- the X360 calls the
        // field's virtual Construct through its live vtable), then clear the
        // visible/loaded latches and RAISE the pending-trans-in latch.
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpacParentName);

        // @0x824123B8 (this TU) -- latch visibility and push the matching frame
        // ("TransIn" / "Invisible") as the clip's "apt_state".
        void SetVisible(bool lbVisible);

        // @0x824123F0 (this TU) -- raise the loaded latch (the HUD state calls this
        // from the clip's load-complete apt event).
        void SetLoaded();

        // @0x82412400 (this TU, cpp:145/:146) -- once loaded: bounds-assert the
        // 1..8 position (streamed on the X360; folded static, non-gating), push the
        // position's database key into the text field, and step the clip frame --
        // "TransIn" the first time (consuming the pending latch), "ChangePosition"
        // after.
        void SetPosition(s32 liPosition);

    private:
        // The clip frame labels this component drives (XEX .data table @0x82F249BC;
        // slots 1/3 are not consumed by this TU's bodies but belong to the same
        // 5-entry table) and the per-position localisation keys (@0x82F24A8C).
        static const char* const KAC_APTSTATE_NAMES[5];
        static const char* const KAC_POSITION_NAMES[9];

        TextField mRacePositionTextField;            // X360 +0x8C  ("Text_txt")
        bool      mbVisible;        // X360 +0x1B4
        bool      mbLoaded;         // X360 +0x1B5 (gates SetPosition)
        bool      mbFirstFrame;     // X360 +0x1B6 (DWARF name; raised by Construct,
                                    //   consumed by the first SetPosition)
    };
}
