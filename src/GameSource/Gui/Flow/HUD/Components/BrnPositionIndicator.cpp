#include "GameSource/Gui/Flow/hud/Components/BrnPositionIndicator.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the SetPosition bounds tripwires)

// BrnGui::PositionIndicator -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (4 ledger functions, DWARF-free; the asserts name the original
// source GameSource/Gui/Flow/hud/Components/BrnPositionIndicator.cpp):
//   PositionIndicator::Construct   @0x82412340
//   PositionIndicator::SetVisible  @0x824123B8
//   PositionIndicator::SetLoaded   @0x824123F0
//   PositionIndicator::SetPosition @0x82412400

namespace BrnGui
{

// The clip frame labels (XEX .data @0x82F249BC -> .rodata strings) and the
// per-position localisation keys (@0x82F24A8C; slot 0 is the un-prefixed
// invalid sentinel).
const char* const PositionIndicator::KAC_APTSTATE_NAMES[5] =
{
    "TransIn", "Showing", "ChangePosition", "TransOut", "Invisible",
};
const char* const PositionIndicator::KAC_POSITION_NAMES[9] =
{
    "EVENT_POSITION_INVALID",
    "$EVENT_POSITION_FIRST",  "$EVENT_POSITION_SECOND", "$EVENT_POSITION_THIRD",
    "$EVENT_POSITION_FOURTH", "$EVENT_POSITION_FIFTH",  "$EVENT_POSITION_SIXTH",
    "$EVENT_POSITION_SEVENTH", "$EVENT_POSITION_EIGHTH",
};

// @ 0x82412340 -- base Construct, the embedded field's (virtual on the X360)
// Construct, then the latch seeds: visible/loaded CLEAR, pending-trans-in RAISED
// (the asm's stb 0,0x1B4 / stb 0,0x1B5 / stb 1,0x1B6 -- the Hex-Rays view mis-reads
// the +0x1B5 store as 1; the asm stores r11 == 0).
void PositionIndicator::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                                  const char* lpacParentName)
{
    CgsGui::GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);
    mRacePositionTextField.Construct("Text_txt", lpStateInterface, "PositionIndicator_mc");
    mbVisible        = false;
    mbLoaded         = false;
    mbFirstFrame = true;
}

// @ 0x824123B8 -- latch and push the matching frame.
void PositionIndicator::SetVisible(bool lbVisible)
{
    mbVisible = lbVisible;
    if (lbVisible)
        AddOutputAptViewState("apt_state", KAC_APTSTATE_NAMES[0], false);   // "TransIn"
    else
        AddOutputAptViewState("apt_state", KAC_APTSTATE_NAMES[4], false);   // "Invisible"
}

// @ 0x824123F0 -- the load-complete latch.
void PositionIndicator::SetLoaded()
{
    mbLoaded = true;
}

// @ 0x82412400 -- cpp:145/:146. No-op until loaded. The bounds tripwires are
// non-gating (the X360 indexes the text table regardless); the first push after
// Construct steps "TransIn" (consuming the pending latch), later pushes step
// "ChangePosition".
void PositionIndicator::SetPosition(s32 liPosition)
{
    if (!mbLoaded)
        return;

    CGS_ASSERT(liPosition <= 8,
               "Position Out of Bounds (too high) in PositionIndicator::SetPosition");   // :145 (streamed; folded)
    CGS_ASSERT(liPosition > 0,
               "Position out of bounds (too low) in PositionIndicator::SetPosition");    // :146 (streamed; folded)

    mRacePositionTextField.SetText(KAC_POSITION_NAMES[liPosition]);

    if (mbFirstFrame)
    {
        mbFirstFrame = false;
        AddOutputAptViewState("apt_state", KAC_APTSTATE_NAMES[0], false);   // "TransIn"
    }
    else
    {
        AddOutputAptViewState("apt_state", KAC_APTSTATE_NAMES[2], false);   // "ChangePosition"
    }
}

}
