#include "GameSource/Gui/Flow/Screen/Components/BrnCrashNavLegend.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnGui::CrashNavLegend -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (4 ledger functions, DWARF primary file
// GameSource/Gui/Flow/Screen/Components/BrnCrashNavLegend.cpp):
//   CrashNavLegend::Construct         @0x824189A0
//   CrashNavLegend::UpdateIcons       @0x82418B38
//   CrashNavLegend::HighlightNext     @0x82425F68
//   CrashNavLegend::HighlightPrevious @0x82425FD0

namespace BrnGui
{

// The legend clip/state/help tables (XEX .data: icon names @0x82F2520C, state
// names @0x82F25220, help strings @0x82F25244).
const char CrashNavLegend::KAC_HELP_STRING_TEXT_FIELD_NAME[14] = "HelpText_text";
const char* const CrashNavLegend::KAPC_ICON_NAMES[CrashNavLegend::KI_NUM_VISIBLE_ICONS] =
{
    "icon0", "icon1", "icon2", "icon3", "icon4",
};
const char* const CrashNavLegend::KAPC_LEGEND_ICON_NAMES[9] =
{
    "Legend_AllOn",       "Legend_Landmark",   "Legend_Rival",
    "Legend_Blackspot",   "Legend_SpeedCam",   "Legend_DriveThough",
    "Legend_AlloysFound", "Legend_SigTakedown",
    "Invisible",   // the out-of-window slot state (UpdateIcons index 8)
};
const char* const CrashNavLegend::KAPC_LEGEND_HELP_STRINGS[9] =
{
    "$CN_LEGEND_ALL_ON",      "$CN_LEGEND_LANDMARK",    "$CN_LEGEND_RIVAL",
    "$CN_LEGEND_BLACKSPOT",   "$CN_LEGEND_SPEEDCAM",    "$CN_LEGEND_DRIVETHROUGH",
    "$CN_LEGEND_ALLOYSFOUND", "$CN_LEGEND_SIGTAKEDOWN",
    "",   // the 9th slot is the empty string on the console
};

// @ 0x824189A0 -- cpp:84/:85. Both argument tripwires are non-gating (streamed on
// the X360; folded static). The help text field is constructed through its vtable
// slot 0; the five icon slots each bind the shared legend state-name table and are
// parented under this component's own name.
void CrashNavLegend::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                               const char* lpacParentName)
{
    CGS_ASSERT(lpacName != 0, "Invalid name sent to CrashNavLegend::Construct");                     // :84
    CGS_ASSERT(lpStateInterface != 0, "Invalid state interface sent to CrashNavLegend::Construct");  // :85

    CgsGui::GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);
    mHelpStringTextField.Construct(KAC_HELP_STRING_TEXT_FIELD_NAME, lpStateInterface, macName);
    for (s32 liIcon = 0; liIcon < KI_NUM_VISIBLE_ICONS; ++liIcon)
    {
        mIcons[liIcon].Construct(KAPC_ICON_NAMES[liIcon], lpStateInterface,
                                 KAPC_LEGEND_ICON_NAMES, macName);
    }
    mbIsDirty = false;
    meAnimState = E_ANIMSTATE_INVISIBLE;
    mi8CurrentlySelectedIcon = 0;
}

// @ 0x82418B38 -- window the five slots around the cursor: slot i shows category
// (cursor + i - 2); anything outside 0..7 shows the Invisible state (index 8).
void CrashNavLegend::UpdateIcons()
{
    for (s32 liIcon = 0; liIcon < KI_NUM_VISIBLE_ICONS; ++liIcon)
    {
        u32 luState = static_cast<u32>(mi8CurrentlySelectedIcon + liIcon - 2);
        if (luState > 7)
            luState = 8;
        mIcons[liIcon].SetState(luState);
    }
}

// @ 0x82425F68 -- advance the highlight cursor (max category 7), refresh the help
// text + the icon window. Always reports handled.
bool CrashNavLegend::HighlightNext()
{
    if (mi8CurrentlySelectedIcon < 7)
    {
        ++mi8CurrentlySelectedIcon;
        mHelpStringTextField.SetText(KAPC_LEGEND_HELP_STRINGS[mi8CurrentlySelectedIcon]);
        UpdateIcons();
    }
    return true;
}

// @ 0x82425FD0 -- retreat the highlight cursor (min 0); otherwise as HighlightNext.
bool CrashNavLegend::HighlightPrevious()
{
    if (mi8CurrentlySelectedIcon > 0)
    {
        --mi8CurrentlySelectedIcon;
        mHelpStringTextField.SetText(KAPC_LEGEND_HELP_STRINGS[mi8CurrentlySelectedIcon]);
        UpdateIcons();
    }
    return true;
}

}
