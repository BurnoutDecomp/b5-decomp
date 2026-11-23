#include "GameSource/Gui/Flow/Overlay/States/BrnBaseOkOverlayState.h"

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // GetLanguageManager
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"           // FormatAndAddText
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // InputBuffer::GuiEventQueue (the in-queue type)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiOverlayWaitFinishRequest

// BrnGui::BaseOkOverlayState -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here:
//   BaseOkOverlayState::UpdateRunning @0x824B2770  (this TU's ledger function)
//   BaseOkOverlayState::SetupOverlay  @0x824B1BC0  (ledger-drift recovery: DWARF-
//     misattributed to CgsGuiStateInterface.h, marked reviewed with no committed
//     body; landed at its real home)

// The dynamic loc-string key the button label is (re)published under ("TEMP_POPUP_STRING1",
// @0x82064154) and its "$"-prefixed lookup form the help item consumes (@0x82064140) are
// literals: ARTIST addresses both directly (lis/addi), with no file-scope pointer behind them.
namespace BrnGui
{

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
    // DWARF (BrnGuiOverlayUnity CU) lists GetLanguageManager twice: one call per arm.
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

    // Help item 2 carries the label with the SELECT (accept) glyph.
    mHelpItem2Component.SetItem("$TEMP_POPUP_STRING1",
                                FlaptButtonIconComponent::E_PADBUTTON_SELECT,
                                FlaptButtonIconComponent::E_PADBUTTON_INVISIBLE, true);
}

// @ 0x824B2770
bool BaseOkOverlayState::UpdateRunning()
{
    // DWARF locals (BrnGuiOverlayUnity CU): lpEvent cpp:43, liEventSize :44, liEventId :45,
    // lpEventPressed :53 (a GuiEventControllerInputPressed). Neither out-param is initialised
    // and the queue member is re-read for each call (ARTIST 0x824B2770).
    const CgsModule::Event* lpEvent;
    s32 liEventSize;
    s32 liEventId = mpInGuiEventQueue->GetFirstEvent(&lpEvent, &liEventSize);
    while (lpEvent != NULL)
    {
        // 6: GuiEventControllerInputPressed (GuiEvent<6>); 49: the accept button.
        if (liEventId == 6)
        {
            // FLAG: miButtonId is read at payload +4 (ARTIST `lwz r11, 4(r4)`); the tree's
            // GuiEvent<N> base still carries a 12-byte header, so the DWARF member access
            // (lpEventPressed->miButtonId) would read +0x10 until that base is emptied.
            const s32 liButtonId =
                *reinterpret_cast<const s32*>(reinterpret_cast<const u8*>(lpEvent) + 4);
            if (liButtonId == 49)
            {
                meLeaveMethod = GuiOverlayCompleteEvent::E_LEAVEMETHOD_OK;
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
