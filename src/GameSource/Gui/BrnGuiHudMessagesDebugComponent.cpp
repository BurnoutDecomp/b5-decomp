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

#include "GameShared/GameClasses/Development/CgsStrStream.h"

namespace BrnGui
{
    // Original C++ initialization emits only the derived vtable.
    // No active/next/controller/director/id/group field is initialized here.
    GuiHudMessagesDebugComponent::GuiHudMessagesDebugComponent()
        : CgsDev::DebugComponent(CgsDev::DebugComponent::GuiConstructorTag{})
    {
    }

    // ARTIST825187E8..82518804 inlines this actual owner; DecFIGS534114
    // supplies its named declaration/body. The folded base call is empty.
    void GuiHudMessagesDebugComponent::Construct(HudMessageDirector* lpMessageDirector)
    {
        mpMessageController = 0;
        mpMessageDirector = lpMessageDirector;
        miMessageId = 0;
        miMessageGroup = 0;
    }

    // Actual module Destruct82507714..1C resolves this owner to8284CB38.
    void GuiHudMessagesDebugComponent::Destruct()
    {
        // Original folded empty body; no active/controller reset.
    }

    // Target table820CE800 slot5 resolves to false-returning827E2F38.
    bool GuiHudMessagesDebugComponent::IsSimple() const
    {
        return false;
    }

    bool GuiHudMessagesDebugComponent::ControllerIsValid() const
    {
        return mpMessageController != 0;
    }

    // ARTIST82523B90. The observed base activation call is empty8284CB38.
    void GuiHudMessagesDebugComponent::OnActivate()
    {
        RegisterVariable(&miMessageId, "Message Id to send");
        SetReadOnly(&miMessageId, true);
        RegisterVariable(&miMessageGroup, "Group Of Messages");
        SetRange(&miMessageGroup, 0, 3);
        SetOptions(&miMessageGroup, CgsGui::KAC_HUD_MESSAGE_GROUP_NAMES);
        RegisterFunction(&GuiHudMessagesDebugComponent::SelectNextMessageCallback,
            this, "Select the next message");
        RegisterFunction(&GuiHudMessagesDebugComponent::TriggerMessageCallback,
            this, "Trigger the current message");
        RegisterFunction(&GuiHudMessagesDebugComponent::TriggerMessageWithoutTimerCallback,
            this, "Trigger the current message ignoring time");
        RegisterFunction(&GuiHudMessagesDebugComponent::SelectPreviousMessageCallback,
            this, "Select the previous message");
    }

    // ARTIST824FD5D0..E8 /824FD688..6A0 inline these real helpers.
    void GuiHudMessagesDebugComponent::SelectNextMessage()
    {
        if (ControllerIsValid())
            miMessageId = mpMessageController->GetNextMessageIdInGroup(miMessageId,
                static_cast<CgsGui::HudMessageGroups>(miMessageGroup));
    }

    void GuiHudMessagesDebugComponent::SelectPreviousMessage()
    {
        if (ControllerIsValid())
            miMessageId = mpMessageController->GetPreviousMessageIdInGroup(miMessageId,
                static_cast<CgsGui::HudMessageGroups>(miMessageGroup));
    }

    void GuiHudMessagesDebugComponent::TriggerMessageCallback(void* lpUserData)
    {
        if (!lpUserData)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::MessageBufferPC lMessage;
            CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
            lStream << "Invalid data pointer sent to GuiHudMessagesDebugComponent::TriggerMessageCallback";
            CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
                "..\\..\\..\\GameSource\\Gui/BrnGuiHudMessagesDebugComponent.cpp", 238);
            CgsDev::Assert::EndAssert();
        }
        static_cast<GuiHudMessagesDebugComponent*>(lpUserData)->TriggerMessage(false);
    }

    void GuiHudMessagesDebugComponent::TriggerMessageWithoutTimerCallback(void* lpUserData)
    {
        if (!lpUserData)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::MessageBufferPC lMessage;
            CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
            lStream << "Invalid data sent to GuiHudMessagesDebugComponent::TriggerMessageWithoutTimerCallback";
            CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
                "..\\..\\..\\GameSource\\Gui/BrnGuiHudMessagesDebugComponent.cpp", 252);
            CgsDev::Assert::EndAssert();
        }
        static_cast<GuiHudMessagesDebugComponent*>(lpUserData)->TriggerMessage(true);
    }

    void GuiHudMessagesDebugComponent::SelectNextMessageCallback(void* lpUserData)
    {
        if (!lpUserData)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::MessageBufferPC lMessage;
            CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
            lStream << "Invalid data in the callback";
            CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
                "..\\..\\..\\GameSource\\Gui/BrnGuiHudMessagesDebugComponent.cpp", 267);
            CgsDev::Assert::EndAssert();
        }
        static_cast<GuiHudMessagesDebugComponent*>(lpUserData)->SelectNextMessage();
    }

    void GuiHudMessagesDebugComponent::SelectPreviousMessageCallback(void* lpUserData)
    {
        if (!lpUserData)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::MessageBufferPC lMessage;
            CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
            lStream << "Invalid data in the callback";
            CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
                "..\\..\\..\\GameSource\\Gui/BrnGuiHudMessagesDebugComponent.cpp", 282);
            CgsDev::Assert::EndAssert();
        }
        static_cast<GuiHudMessagesDebugComponent*>(lpUserData)->SelectPreviousMessage();
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
    void GuiHudMessagesDebugComponent::TriggerMessage(bool lbForce)
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

        mpMessageDirector->AddMessage(&lMessage, lbForce);
    }
}
