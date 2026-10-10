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

namespace BrnGui
{
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
