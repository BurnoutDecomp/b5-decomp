#include "GameSource/Gui/Flow/Overlay/States/BrnBaseOkCancelOverlayState.h"

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // GetLanguageManager
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"           // FormatAndAddText
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // the 18432 in-queue view

// BrnGui::BaseOkCancelOverlayState -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here:
//   BaseOkCancelOverlayState::UpdateRunning @0x824B2840  (this TU's ledger function)
//   BaseOkCancelOverlayState::SetupOverlay  @0x824B1C78  (ledger-drift recovery:
//     DWARF-misattributed to CgsGuiStateInterface.h, marked reviewed with no
//     committed body; landed at its real home)

namespace BrnGui
{
namespace
{
    typedef CgsModule::VariableEventQueue<18432, 16> OverlayStateInQueue;

    const s32 KI_EVENT_CONTROLLER_ACTION   = 6;
    const s32 KI_EVENT_OVERLAY_WAIT_FINISH = 188;

    // The accept / back controller-action sub-ids.
    const s32 KI_ACTION_POPUP_OK     = 49;
    const s32 KI_ACTION_POPUP_CANCEL = 50;

    // The dynamic loc-string keys the two button labels are published under, and
    // their "$"-prefixed lookup forms the help items consume.
    const char* KPC_TEMP_POPUP_STRING1        = "TEMP_POPUP_STRING1";
    const char* KPC_TEMP_POPUP_STRING1_LOOKUP = "$TEMP_POPUP_STRING1";
    const char* KPC_TEMP_POPUP_STRING2        = "TEMP_POPUP_STRING2";
    const char* KPC_TEMP_POPUP_STRING2_LOOKUP = "$TEMP_POPUP_STRING2";
}

// @ 0x824B1C78
void BaseOkCancelOverlayState::SetupOverlay(const GuiOverlayFullInfoResponse* lpResponse)
{
    BaseOverlayState::SetupOverlay(lpResponse);

    CgsLanguage::LanguageManager* lpLanguageManager = mpStateInterface->GetLanguageManager();

    // Publish the button-1 label (see BaseOkOverlayState::SetupOverlay for the raw
    // meParamType vararg-format note; preserved as-is).
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

    // Help item 1: the OK label with the SELECT glyph.
    mHelpItem1Component.SetItem(KPC_TEMP_POPUP_STRING1_LOOKUP,
                                FlaptButtonIconComponent::E_PADBUTTON_SELECT,
                                FlaptButtonIconComponent::E_PADBUTTON_INVISIBLE, true);

    // Publish the button-2 label the same way.
    lpLanguageManager = mpStateInterface->GetLanguageManager();
    if (lpResponse->mbButon2ParamUsed)
    {
        lpLanguageManager->FormatAndAddText(
            KPC_TEMP_POPUP_STRING2, lpResponse->macButton2Id,
            CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP, 1,
            lpResponse->mButton2Param.macParameter,
            static_cast<s32>(lpResponse->mButton2Param.meParamType));
    }
    else
    {
        lpLanguageManager->FormatAndAddText(
            KPC_TEMP_POPUP_STRING2, lpResponse->macButton2Id,
            CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
    }

    // Help item 2: the Cancel label with the BACK glyph.
    mHelpItem2Component.SetItem(KPC_TEMP_POPUP_STRING2_LOOKUP,
                                FlaptButtonIconComponent::E_PADBUTTON_BACK,
                                FlaptButtonIconComponent::E_PADBUTTON_INVISIBLE, true);
}

// @ 0x824B2840
bool BaseOkCancelOverlayState::UpdateRunning()
{
    OverlayStateInQueue* lpInQueue = reinterpret_cast<OverlayStateInQueue*>(mpInGuiEventQueue);

    const CgsModule::Event* lpEvent = NULL;
    s32 liSize = 0;
    s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
    while (lpEvent != NULL)
    {
        if (liEventId == KI_EVENT_CONTROLLER_ACTION)
        {
            const s32 liAction =
                *reinterpret_cast<const s32*>(reinterpret_cast<const u8*>(lpEvent) + 4);
            if (liAction == KI_ACTION_POPUP_OK)
            {
                meLeaveMethod = GuiOverlayCompleteEvent::E_LEAVEMETHOD_OK;
                return true;
            }
            if (liAction == KI_ACTION_POPUP_CANCEL)
            {
                meLeaveMethod = GuiOverlayCompleteEvent::E_LEAVEMETHOD_CANCEL;
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
