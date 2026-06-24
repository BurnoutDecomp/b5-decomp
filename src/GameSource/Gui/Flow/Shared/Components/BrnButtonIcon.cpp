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
// The dossier asm only attests index 0 of each -- maButtonIdentifiers[0] = "up"
// (off_8206B790@l) and maStateIdentifiers[0] = "Active" (off_8206B7D0@l). The remaining
// entries' exact string bytes are NOT recoverable from the provided disassembly, so they
// are honest empty-string placeholders sized to the asm-attested element counts
// (E_PADBUTTON_COUNT = 16, E_PADBUTTON_STATE_COUNT = 4). Likewise Construct's invisible
// glyph (&unk_820046A7) is represented as the placeholder slot maButtonIdentifiers[
// E_PADBUTTON_INVISIBLE]; its literal content is not attested. The store/branch/indexing
// logic is exact; only these specific rodata string contents are placeholdered.
// ===================================================================================
#include "GameSource/Gui/Flow/Shared/Components/BrnButtonIcon.h"

namespace BrnGui
{
    namespace
    {
        // off_8206B790[E_PADBUTTON_COUNT] -- glyph identifier per pad button. Only [0] is
        // attested ("up"); the rest are unrecoverable placeholders (see file header FLAG).
        const char* const maButtonIdentifiers[ButtonIconComponent::E_PADBUTTON_COUNT] =
        {
            "up", // E_PADBUTTON_UP (attested @ off_8206B790)
            "",   // E_PADBUTTON_DOWN       (placeholder: unrecoverable)
            "",   // E_PADBUTTON_LEFT       (placeholder: unrecoverable)
            "",   // E_PADBUTTON_RIGHT      (placeholder: unrecoverable)
            "",   // E_PADBUTTON_SELECT     (placeholder: unrecoverable)
            "",   // E_PADBUTTON_BACK       (placeholder: unrecoverable)
            "",   // E_PADBUTTON_OPTION0    (placeholder: unrecoverable)
            "",   // E_PADBUTTON_OPTION1    (placeholder: unrecoverable)
            "",   // E_PADBUTTON_LSHOULDER  (placeholder: unrecoverable)
            "",   // E_PADBUTTON_RSHOULDER  (placeholder: unrecoverable)
            "",   // E_PADBUTTON_LTRIGGER   (placeholder: unrecoverable)
            "",   // E_PADBUTTON_RTRIGGER   (placeholder: unrecoverable)
            "",   // E_PADBUTTON_START      (placeholder: unrecoverable)
            "",   // E_PADBUTTON_LTHUMB     (placeholder: unrecoverable)
            "",   // E_PADBUTTON_RTHUMB     (placeholder: unrecoverable)
            "",   // E_PADBUTTON_INVISIBLE  (placeholder: unrecoverable; == Construct's glyph)
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
