#include "GameSource/Gui/Flow/Overlay/States/BrnBaseOkCancelOverlayState.h"

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // GetLanguageManager
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"           // FormatAndAddText
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // InputBuffer::GuiEventQueue (the in-queue type)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiOverlayWaitFinishRequest

// BrnGui::BaseOkCancelOverlayState -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here:
//   BaseOkCancelOverlayState::UpdateRunning @0x824B2840  (this TU's ledger function)
//   BaseOkCancelOverlayState::SetupOverlay  @0x824B1C78  (ledger-drift recovery:
//     DWARF-misattributed to CgsGuiStateInterface.h, marked reviewed with no
//     committed body; landed at its real home)

// The two dynamic loc-string keys the button labels are published under
// ("TEMP_POPUP_STRING1" @0x82064154, "TEMP_POPUP_STRING2" @0x8206417C) and their "$"-prefixed
// lookup forms the help items consume (@0x82064140 / @0x82064168) are literals: ARTIST
// addresses them directly (lis/addi), with no file-scope pointer behind them.
namespace BrnGui
{

// @ 0x824B1C78
void BaseOkCancelOverlayState::SetupOverlay(const GuiOverlayFullInfoResponse* lpResponse)
{
    BaseOverlayState::SetupOverlay(lpResponse);

    // DWARF (BrnGuiOverlayUnity CU) lists GetLanguageManager per arm. The PS3 body's extra
    // lacBuffer[1024]/lpcString block (cpp:107/108) has no X360 counterpart (ARTIST 0x824B1C78).
    // Publish the button-1 label (see BaseOkOverlayState::SetupOverlay for the raw
    // meParamType vararg-format note; preserved as-is).
    if (lpResponse->mbButon1ParamUsed)
    {
        mpStateInterface->GetLanguageManager()->FormatAndAddText(
            "TEMP_POPUP_STRING1", lpResponse->macButton1Id,
            CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP, 1,
            lpResponse->mButton1Param.macParameter,
            static_cast<s32>(lpResponse->mButton1Param.meParamType));
    }
    else
    {
        mpStateInterface->GetLanguageManager()->FormatAndAddText(
            "TEMP_POPUP_STRING1", lpResponse->macButton1Id,
            CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
    }

    // Help item 1: the OK label with the SELECT glyph.
    mHelpItem1Component.SetItem("$TEMP_POPUP_STRING1",
                                FlaptButtonIconComponent::E_PADBUTTON_SELECT,
                                FlaptButtonIconComponent::E_PADBUTTON_INVISIBLE, true);

    // Publish the button-2 label the same way.
    if (lpResponse->mbButon2ParamUsed)
    {
        mpStateInterface->GetLanguageManager()->FormatAndAddText(
            "TEMP_POPUP_STRING2", lpResponse->macButton2Id,
            CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP, 1,
            lpResponse->mButton2Param.macParameter,
            static_cast<s32>(lpResponse->mButton2Param.meParamType));
    }
    else
    {
        mpStateInterface->GetLanguageManager()->FormatAndAddText(
            "TEMP_POPUP_STRING2", lpResponse->macButton2Id,
            CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
    }

    // Help item 2: the Cancel label with the BACK glyph.
    mHelpItem2Component.SetItem("$TEMP_POPUP_STRING2",
                                FlaptButtonIconComponent::E_PADBUTTON_BACK,
                                FlaptButtonIconComponent::E_PADBUTTON_INVISIBLE, true);
}

// @ 0x824B2840
bool BaseOkCancelOverlayState::UpdateRunning()
{
    // DWARF locals (BrnGuiOverlayUnity CU): lpEvent cpp:43, liEventSize :44, liEventId :45,
    // lpEventPressed :53 (a GuiEventControllerInputPressed). Neither out-param is initialised
    // and the queue member is re-read for each call (ARTIST 0x824B2840).
    const CgsModule::Event* lpEvent;
    s32 liEventSize;
    s32 liEventId = mpInGuiEventQueue->GetFirstEvent(&lpEvent, &liEventSize);
    while (lpEvent != NULL)
    {
        // 6: GuiEventControllerInputPressed (GuiEvent<6>); 49 accept, 50 back.
        if (liEventId == 6)
        {
            // FLAG: miButtonId read at payload +4 (ARTIST `lwz r11, 4(r4)`); see
            // BaseOkOverlayState::UpdateRunning for why not lpEventPressed->miButtonId yet.
            const s32 liButtonId =
                *reinterpret_cast<const s32*>(reinterpret_cast<const u8*>(lpEvent) + 4);
            if (liButtonId == 49)
            {
                meLeaveMethod = GuiOverlayCompleteEvent::E_LEAVEMETHOD_OK;
                return true;
            }
            if (liButtonId == 50)
            {
                meLeaveMethod = GuiOverlayCompleteEvent::E_LEAVEMETHOD_CANCEL;
                return true;
            }
        }
        // 188: GuiOverlayWaitFinishRequest's wire id (its GetEventType()).
        else if (liEventId == 188 &&
                 reinterpret_cast<const GuiOverlayWaitFinishRequest*>(lpEvent)->mOverlayId == mCurrentOverlayId)
        {
            meLeaveMethod = GuiOverlayCompleteEvent::E_LEAVEMETHOD_NONE;
            return true;
        }

        liEventId = mpInGuiEventQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize);
    }

    return false;
}
}
