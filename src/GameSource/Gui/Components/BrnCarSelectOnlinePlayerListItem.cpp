// ===================================================================================
// BrnGui::CarSelectOnlinePlayerListItem  -- implementation
//   class:BrnGui::CarSelectOnlinePlayerListItem
//
// One row of the online car-select player list. It owns two embedded BrnGui::TextField
// sub-components (the gamertag row "gamertag_mc" and the car-name row "carName_mc") plus
// the currently displayed car id and the row's visible/final-selection flags.
//
// Construct   @ 0x8241B390 -- base Construct, Construct both text fields, zero the state.
// Show        @ 0x8241A598 -- mark visible and push the "final"/"visible" apt view-state.
// OnLoad      @ 0x8241B4A0 -- route an apt load-notification to the matching child.
// SetPlayerCar@ 0x8241B420 -- push the selected car's uppercased name into the car field.
//   Reconstructed store-for-store from the X360 pseudocode/asm.
// ===================================================================================
#include "GameSource/Gui/Components/BrnCarSelectOnlinePlayerListItem.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsID.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include <cstring>

namespace BrnGui
{
    // BrnCarSelectOnlinePlayerListItem.cpp:23/24 (DWARF-named clip-name constants).
    const char CarSelectOnlinePlayerListItem::KAC_GAMERTAG_TEXTFIELD_NAME[12] = "gamertag_mc";
    const char CarSelectOnlinePlayerListItem::KAC_CAR_TEXTFIELD_NAME[11]      = "carName_mc";

    // @ 0x8241B390 -- run the base component Construct, then Construct the two embedded
    // text fields (the gamertag row "gamertag_mc" and the car-name row "carName_mc"),
    // both parented to THIS component (GetName()). Finally zero the selection state:
    // no car chosen yet, not visible, not the final selection.
    void CarSelectOnlinePlayerListItem::Construct(const char* lpacName,
                                                  CgsGui::StateInterface* lpStateInterface,
                                                  const char* lpacParentName)
    {
        CgsGui::GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);

        mGamertagTextfield.Construct(KAC_GAMERTAG_TEXTFIELD_NAME, mpStateInterface, GetName());
        mCarTextfield.Construct(KAC_CAR_TEXTFIELD_NAME, mpStateInterface, GetName());

        mbVisible        = false;   // *(this+0x2E8) = 0
        mbFinalSelection = false;   // *(this+0x2E9) = 0
        mCurrentCarID    = 0;       // *(this+0x2E0) = 0  (u64 std)
    }

    // @ 0x8241A598
    void CarSelectOnlinePlayerListItem::Show()
    {
        mbVisible = true;                                          // *(this+0x2E8) = 1

        // Always emit the base "visible" view-state.
        AddOutputAptViewState("apt_state", "visible", false);

        // Once the selection has been finalised, emit the "final" state instead.
        if (mbFinalSelection)
            AddOutputAptViewState("apt_state", "final", false);
        else
            AddOutputAptViewState("apt_state", "visible", false);
    }

    // @ 0x8241B4A0 -- route an apt load notification (by clip name) to the matching
    // sub-component. The gamertag / car-name clips re-push their field's stored text;
    // this component's own clip toggles visibility: once loaded, show it if it was
    // marked visible, otherwise force it to the "invisible" apt state. Any other clip
    // name is unhandled and asserts.
    void CarSelectOnlinePlayerListItem::OnLoad(const char* lpacName)
    {
        if (std::strcmp(mGamertagTextfield.GetName(), lpacName) == 0)
        {
            mGamertagTextfield.SetText(mGamertagTextfield.GetText());
        }
        else if (std::strcmp(mCarTextfield.GetName(), lpacName) == 0)
        {
            mCarTextfield.SetText(mCarTextfield.GetText());
        }
        else if (std::strcmp(GetName(), lpacName) == 0)
        {
            if (mbVisible)
            {
                Show();
            }
            else
            {
                mbVisible = false;
                AddOutputAptViewState("apt_state", "invisible", false);
            }
        }
        else
        {
            CGS_ASSERT(false, "Unhandled load notification : ");
        }
    }

    // @ 0x8241B420 -- update the car-name row for a newly selected car. Only acts while
    // the row is visible and the id actually changed; then it records the new id, builds
    // the "CAR_CAPS_<id>" localisation key from the printable CgsID and pushes it through
    // the car text field as an id-lookup (uppercased car name).
    void CarSelectOnlinePlayerListItem::SetPlayerCar(CgsID lCarID)
    {
        if (!mbVisible)
            return;
        if (lCarID == mCurrentCarID)
            return;

        mCurrentCarID = lCarID;                                  // std -> +0x2E0 (before formatting)

        char lacCarName[16];
        CgsIDConvertToString(lCarID, lacCarName);

        char lacKey[32];
        CgsCore::SPrintf(lacKey, 31, "CAR_CAPS_%s", lacCarName);
        lacKey[31] = '\0';

        mCarTextfield.SetLocalisedText(lacKey,
            CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);   // format 9
    }
}
