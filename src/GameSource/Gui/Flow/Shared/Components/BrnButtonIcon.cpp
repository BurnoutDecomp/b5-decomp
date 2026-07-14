// ===================================================================================
// BrnGui::ButtonIconComponent  -- implementation
//   class:BrnGui::ButtonIconComponent
//
// Construct  @ 0x824E20E8
// SetButton  @ 0x824E2138
//   Control flow and stores reconstructed store-for-store from the X360 pseudocode/asm.
//
// FLAG (opaque rodata): the two identifier tables below are file-scope statics owned by
// this TU (DWARF: maButtonIdentifiers @ BrnButtonIcon.cpp:28, maStateIdentifiers @ :48).
// The dossier asm attests index 0 of each -- maButtonIdentifiers[0] = "up" (off_8206B790@l)
// and maStateIdentifiers[0] = "Active" (off_8206B7D0@l). maButtonIdentifiers is now fully
// RECOVERED from the B5ControllerButtons Apt clip's frame labels (see the table note) --
// these strings ARE the values SetButton's apt_button drives the clip with, so the Apt data
// is the authoritative source. maStateIdentifiers[1..3] remain honest empty placeholders
// (the popup only uses [0]="Active"; the other visual states are not exercised yet and their
// frame-label bytes are not attested). The store/branch/indexing logic is exact.
// ===================================================================================
#include "GameSource/Gui/Flow/Shared/Components/BrnButtonIcon.h"

namespace BrnGui
{
    namespace
    {
        // off_8206B790[E_PADBUTTON_COUNT] -- glyph identifier per pad button. [0]="up" is
        // asm-attested; the rest are RECOVERED from the B5ControllerButtons Apt clip's frame
        // labels (the strings SetButton's apt_button drives the clip's gotoAndStop to -- the
        // authoritative source, since these values ARE consumed by the Apt data). Confirmed present
        // as frame labels in build/game/GUIAPT/B5CONTROLLERBUTTONS.bundle: up/down/left/right/
        // select/back/option0/option1/lshoulder/rshoulder/ltrigger/rtrigger/start/lthumb/rthumb/
        // invisible -- an exact lowercase match to the enum order, with [15]="invisible" also seen
        // in SaveLoadComponent.bundle (== the Construct default glyph).
        const char* const maButtonIdentifiers[ButtonIconComponent::E_PADBUTTON_COUNT] =
        {
            "up",         // E_PADBUTTON_UP (asm-attested @ off_8206B790)
            "down",       // E_PADBUTTON_DOWN
            "left",       // E_PADBUTTON_LEFT
            "right",      // E_PADBUTTON_RIGHT
            "select",     // E_PADBUTTON_SELECT
            "back",       // E_PADBUTTON_BACK
            "option0",    // E_PADBUTTON_OPTION0
            "option1",    // E_PADBUTTON_OPTION1
            "lshoulder",  // E_PADBUTTON_LSHOULDER
            "rshoulder",  // E_PADBUTTON_RSHOULDER
            "ltrigger",   // E_PADBUTTON_LTRIGGER
            "rtrigger",   // E_PADBUTTON_RTRIGGER
            "start",      // E_PADBUTTON_START
            "lthumb",     // E_PADBUTTON_LTHUMB
            "rthumb",     // E_PADBUTTON_RTHUMB
            "invisible",  // E_PADBUTTON_INVISIBLE (== Construct's glyph)
        };

        // off_8206B7D0[E_PADBUTTON_STATE_COUNT] -- visual-state identifier. Only [0] is
        // attested ("Active"); the rest are unrecoverable placeholders.
        const char* const maStateIdentifiers[ButtonIconComponent::E_PADBUTTON_STATE_COUNT] =
        {
            "Active", // E_PADBUTTON_STATE_ACTIVE (attested @ off_8206B7D0)
            "",       // E_PADBUTTON_STATE_HIGLIGHTED   (placeholder: unrecoverable)
            "",       // E_PADBUTTON_STATE_PRESSED      (placeholder: unrecoverable)
            "",       // E_PADBUTTON_STATE_UNSELECTABLE (placeholder: unrecoverable)
        };
    }

    // @ 0x824E20E8
    void ButtonIconComponent::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                                        const char* lpacParentName)
    {
        // bl CgsGui__GuiComponent__Construct
        CgsGui::GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);

        meButton = E_PADBUTTON_INVISIBLE;  // li r10, 0xF ; stw r10, 0x8C(r31)

        // AddOutputAptViewState(this, "apt_button", &unk_820046A7, 0) -- the invisible glyph.
        AddOutputAptViewState("apt_button", maButtonIdentifiers[E_PADBUTTON_INVISIBLE], false);
    }

    // @ 0x824E2138
    void ButtonIconComponent::SetButton(EPadButton leButton, EPadButtonState leState)
    {
        meButton = leButton;  // stw r11, 0x8C(r31)

        // AddOutputAptViewState(this, "apt_button", off_8206B790[button], 0)
        AddOutputAptViewState("apt_button", maButtonIdentifiers[leButton], false);

        // AddOutputAptViewState(this, "apt_state", off_8206B7D0[state], 0)
        AddOutputAptViewState("apt_state", maStateIdentifiers[leState], false);
    }
}
