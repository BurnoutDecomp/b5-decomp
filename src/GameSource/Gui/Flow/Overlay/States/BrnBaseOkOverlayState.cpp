#include "GameSource/Gui/Flow/Overlay/States/BrnBaseOkOverlayState.h"

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // GetLanguageManager
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"           // FormatAndAddText
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // the 18432 in-queue view

// BrnGui::BaseOkOverlayState -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here:
//   BaseOkOverlayState::UpdateRunning @0x824B2770  (this TU's ledger function)
//   BaseOkOverlayState::SetupOverlay  @0x824B1BC0  (ledger-drift recovery: DWARF-
//     misattributed to CgsGuiStateInterface.h, marked reviewed with no committed
//     body; landed at its real home)

namespace BrnGui
{
namespace
{
    typedef CgsModule::VariableEventQueue<18432, 16> OverlayStateInQueue;

    const s32 KI_EVENT_CONTROLLER_ACTION   = 6;
    const s32 KI_EVENT_OVERLAY_WAIT_FINISH = 188;

    // The controller-action sub-id that accepts a popup (the same id 49 the
    // credits/boot states treat as their advance action).
    const s32 KI_ACTION_POPUP_OK = 49;

    // The dynamic loc-string key the button label is (re)published under, and its
    // "$"-prefixed lookup form the help item consumes.
    const char* KPC_TEMP_POPUP_STRING1        = "TEMP_POPUP_STRING1";
    const char* KPC_TEMP_POPUP_STRING1_LOOKUP = "$TEMP_POPUP_STRING1";
}

// @ 0x824B1BC0
void BaseOkOverlayState::SetupOverlay(const GuiOverlayFullInfoResponse* lpResponse)
{
    BaseOverlayState::SetupOverlay(lpResponse);

    // Help item 1 is blanked (empty string, both glyphs invisible).
    mHelpItem1Component.SetItem("", FlaptButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                FlaptButtonIconComponent::E_PADBUTTON_INVISIBLE, true);

    // Publish the button-1 label under TEMP_POPUP_STRING1: resolve macButton1Id as a
    // loc-string id (format 9), formatting in the button parameter when present. The
    // X360 passes the parameter's raw meParamType word as the vararg format type
    // (lwz 0xEC -> r9), NOT the KE_POPUP_PARAM_LOOKUP mapping the message path uses --
    // preserved as-is.
    CgsLanguage::LanguageManager* lpLanguageManager = mpStateInterface->GetLanguageManager();
    if (lpResponse->mbButon1ParamUsed)
    {
        lpLanguageManager->FormatAndAddText(
            KPC_TEMP_POPUP_STRING1, lpResponse->macButton1Id,
            CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP, 1,
            lpResponse->mButton1Param.macParameter,
            static_cast<s32>(lpResponse->mButton1Param.meParamType));
    }
    else
    {
        lpLanguageManager->FormatAndAddText(
            KPC_TEMP_POPUP_STRING1, lpResponse->macButton1Id,
            CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
    }

    // Help item 2 carries the label with the SELECT (accept) glyph.
    mHelpItem2Component.SetItem(KPC_TEMP_POPUP_STRING1_LOOKUP,
                                FlaptButtonIconComponent::E_PADBUTTON_SELECT,
                                FlaptButtonIconComponent::E_PADBUTTON_INVISIBLE, true);
}

// @ 0x824B2770
bool BaseOkOverlayState::UpdateRunning()
{
    OverlayStateInQueue* lpInQueue = reinterpret_cast<OverlayStateInQueue*>(mpInGuiEventQueue);

    const CgsModule::Event* lpEvent = NULL;
    s32 liSize = 0;
    s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
    while (lpEvent != NULL)
    {
        if (liEventId == KI_EVENT_CONTROLLER_ACTION)
        {
            // The action sub-id rides the payload's second word.
            const s32 liAction =
                *reinterpret_cast<const s32*>(reinterpret_cast<const u8*>(lpEvent) + 4);
            if (liAction == KI_ACTION_POPUP_OK)
            {
                meLeaveMethod = GuiOverlayCompleteEvent::E_LEAVEMETHOD_OK;
                return true;
            }
        }
        else if (liEventId == KI_EVENT_OVERLAY_WAIT_FINISH &&
                 *reinterpret_cast<const CgsID*>(lpEvent) == mCurrentOverlayId)
        {
            meLeaveMethod = GuiOverlayCompleteEvent::E_LEAVEMETHOD_NONE;
            return true;
        }

        liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
    }

    return false;
}
}
