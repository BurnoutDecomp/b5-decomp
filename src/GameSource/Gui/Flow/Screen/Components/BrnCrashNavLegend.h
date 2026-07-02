#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"     // CgsGui::GuiComponent (base)
#include "GameSource/Gui/BrnGuiTextField.h"                             // BrnGui::TextField (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnIcon.h"              // BrnGui::IconComponent (5 by value)

namespace CgsModule { class Event; }   // RecEvent payload (pointer-only)

// BrnGui::CrashNavLegend - the crash-nav map's legend strip: five icon slots
// windowing the eight legend categories around a highlight cursor, plus the help
// text for the highlighted category. Class shape / enum / member names / method
// set verbatim from the DecFIGS DWARF (BrnCrashNavLegend.h:44/:47/:107-:116).
// This TU bodies Construct/HighlightNext/HighlightPrevious/UpdateIcons;
// RecEvent/SetActive/SetInactive are their own ledger functions.
namespace BrnGui
{
    struct CrashNavLegend : public CgsGui::GuiComponent
    {
        // DWARF :47.
        enum AnimState
        {
            E_ANIMSTATE_INVISIBLE     = 0,
            E_ANIMSTATE_TRANSITIONIN  = 1,
            E_ANIMSTATE_TRANSITIONOUT = 2,
            E_ANIMSTATE_CHANGEDATA    = 3,
            E_ANIMSTATE_IDLE          = 4,
        };

        static const s32 KI_NUM_VISIBLE_ICONS = 5;   // DWARF h:107

        // @0x824189A0 (this TU, DWARF cpp:82) -- base Construct, the help text field,
        // the five icon slots (each bound to the shared legend state-name table).
        virtual void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                               const char* lpacParentName);

        // DWARF cpp:115/:145/:168 -- their own ledger functions (declaration-only).
        void RecEvent(const CgsModule::Event* lpEvent, s32 liArgA, s32 liArgB);
        void SetActive();
        void SetInactive();

        // @0x82425F68 / @0x82425FD0 (this TU, DWARF cpp:188/:210) -- move the legend
        // highlight cursor and refresh the help text + icon window. Always report
        // handled (the X360 returns 1 on both paths).
        bool HighlightNext();
        bool HighlightPrevious();

    private:
        // @0x82418B38 (this TU, DWARF cpp:232) -- re-window the five icon slots
        // around the highlight cursor (out-of-range slots show the Invisible state).
        void UpdateIcons();

        // DWARF cpp:23-:50 -- the clip names / state names / help strings
        // (XEX-recovered; definitions in the .cpp).
        static const char        KAC_HELP_STRING_TEXT_FIELD_NAME[14];
        static const char* const KAPC_ICON_NAMES[KI_NUM_VISIBLE_ICONS];
        static const char* const KAPC_LEGEND_ICON_NAMES[9];
        static const char* const KAPC_LEGEND_HELP_STRINGS[9];

        // DWARF :109-:116 (X360 offsets: +0x8C/+0x90/+0x91/+0x94/+0x1BC).
        AnimState     meAnimState;                        // :109
        bool          mbIsDirty;                          // :111
        s8            mi8CurrentlySelectedIcon;           // :113
        TextField     mHelpStringTextField;               // :115
        IconComponent mIcons[KI_NUM_VISIBLE_ICONS];       // :116
    };
}
