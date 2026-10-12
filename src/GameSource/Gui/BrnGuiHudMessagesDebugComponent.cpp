// BrnGuiHudMessagesDebugComponent.cpp
// The two out-of-line virtual identity overrides of BrnGui::GuiHudMessagesDebugComponent the
// X360 ARTIST build emits:
//
//   GetName @ 0x827DD220 -> "Hud Messages"
//   GetPath @ 0x824ED348 -> "Gui"
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (both are constant-string returns). The rest of
// the component bodied here is TriggerMessage, which posts the selected message with stand-in
// parameters.

#include "GameSource/Gui/BrnGuiHudMessagesDebugComponent.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"           // BrnGui::GuiHudMessage
#include "GameSource/Gui/BrnGuiHudMessageDirector.h"      // BrnGui::HudMessageDirector::AddMessage
#include "SharedClasses/DataLists/BrnHudMessageController.h" // BrnResource::HudMessageController
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT

namespace BrnGui
{
    namespace
    {
        // ARTIST 0x8206F290 (KAC_HUD_MESSAGE_GROUP_NAMES). The table values and exact display
        // strings come from the original four StringList records.
        const CgsDev::DebugUI::StringList KAC_HUD_MESSAGE_GROUP_NAMES[4] = {
            {0, "All Messages"},
            {1, "Live Revenge"},
            {2, "Dirty Tricks"},
            {3, "Main Game Mode Messages"},
        };
    }

    // ARTIST 0x82523B90: expose the selected message id as read-only, register the four-valued
    // message-group selector, then register next/previous and timed/untimed trigger actions.
    void GuiHudMessagesDebugComponent::OnActivate()
    {
        RegisterVariable(&miMessageId, "Message Id to send");
        SetReadOnly(&miMessageId, true);

        RegisterVariable(&miMessageGroup, "Group Of Messages");
        SetRange(&miMessageGroup, 0, 3);
        SetOptions(&miMessageGroup, KAC_HUD_MESSAGE_GROUP_NAMES);

        RegisterFunction(&SelectNextMessageCallback, this, "Select the next message");
        RegisterFunction(&TriggerMessageCallback, this, "Trigger the current message");
        RegisterFunction(&TriggerMessageWithoutTimerCallback, this,
                         "Trigger the current message ignoring time");
        RegisterFunction(&SelectPreviousMessageCallback, this,
                         "Select the previous message");
    }

    // ARTIST 0x824FD540 / 0x824FD5F8. The registered user-data is the component; when its
    // controller exists, advance the selected message within the selected group.
    void GuiHudMessagesDebugComponent::SelectNextMessageCallback(void* lpUserData)
    {
        CGS_ASSERT(lpUserData != 0, "Invalid data in the callback");
        GuiHudMessagesDebugComponent* lpThis =
            static_cast<GuiHudMessagesDebugComponent*>(lpUserData);
        if (lpThis->mpMessageController)
        {
            lpThis->miMessageId = lpThis->mpMessageController->GetNextMessageIdInGroup(
                lpThis->miMessageId,
                static_cast<CgsGui::HudMessageGroups>(lpThis->miMessageGroup));
        }
    }

    void GuiHudMessagesDebugComponent::SelectPreviousMessageCallback(void* lpUserData)
    {
        CGS_ASSERT(lpUserData != 0, "Invalid data in the callback");
        GuiHudMessagesDebugComponent* lpThis =
            static_cast<GuiHudMessagesDebugComponent*>(lpUserData);
        if (lpThis->mpMessageController)
        {
            lpThis->miMessageId = lpThis->mpMessageController->GetPreviousMessageIdInGroup(
                lpThis->miMessageId,
                static_cast<CgsGui::HudMessageGroups>(lpThis->miMessageGroup));
        }
    }

    // ARTIST 0x82517E00 / 0x82517EA8. Both action callbacks validate their user-data, then
    // delegate with the timer-inclusion flag observed at their call sites.
    void GuiHudMessagesDebugComponent::TriggerMessageCallback(void* lpUserData)
    {
        CGS_ASSERT(lpUserData != 0,
                   "Invalid data pointer sent to GuiHudMessagesDebugComponent::TriggerMessageCallback");
        static_cast<GuiHudMessagesDebugComponent*>(lpUserData)->TriggerMessage(false);
    }

    void GuiHudMessagesDebugComponent::TriggerMessageWithoutTimerCallback(void* lpUserData)
    {
        CGS_ASSERT(lpUserData != 0,
                   "Invalid data sent to GuiHudMessagesDebugComponent::TriggerMessageWithoutTimerCallback");
        static_cast<GuiHudMessagesDebugComponent*>(lpUserData)->TriggerMessage(true);
    }

    // @ 0x827DD220
    const char* GuiHudMessagesDebugComponent::GetName() const
    {
        return "Hud Messages";
    }

    // @ 0x824ED348
    const char* GuiHudMessagesDebugComponent::GetPath() const
    {
        return "Gui";
    }

    // Build the selected message (miMessageId) with one stand-in value per declared parameter of
    // each of its display strings -- "param" for a string, "4" for an int, the
    // PLACEHOLDER_TEMP_STRING string id for a string id, "Unrecognized Param" for any other type
    // (posted under its own type) -- and hand it to the director. Nothing happens until a
    // controller has been set.
    void GuiHudMessagesDebugComponent::TriggerMessage(bool lbWithTimer)
    {
        if (mpMessageController == 0)
            return;

        GuiHudMessage lMessage;
        lMessage.Construct(mpMessageController->GetMessageHashFromIndex(miMessageId));

        for (s32 liString = 0; liString < GuiHudMessage::KI_NUMBER_OF_STRINGS; ++liString)
        {
            const s32 liParamCount = mpMessageController->GetMessageParamCount(miMessageId, liString);
            for (s32 liParam = 0; liParam < liParamCount; ++liParam)
            {
                const CgsGui::HudMessageParamTypes leType =
                    mpMessageController->GetMessageParamType(miMessageId, liString, liParam);
                switch (leType)
                {
                case CgsGui::E_HUDMESSAGEPARAMTYPES_STRING:
                    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, liString, "param");
                    break;
                case CgsGui::E_HUDMESSAGEPARAMTYPES_INT:
                    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, liString, "4");
                    break;
                case CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID:
                    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, liString,
                                      "PLACEHOLDER_TEMP_STRING");
                    break;
                default:
                    lMessage.AddParam(leType, liString, "Unrecognized Param");
                    break;
                }
            }
        }

        mpMessageDirector->AddMessage(&lMessage, lbWithTimer);
    }
}
