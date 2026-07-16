#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"   // brings BrnGuiEventTypeDefs + PlayerName
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"         // GuiRivalryStatusChange / GuiRivalIsFleeing / GuiSignatureStuntEvent
#include "GameSource/Gui/BrnGuiCache.h"                 // GuiCache (frozen-header include set)

#include "GameShared/GameClasses/Core/CgsAssert.h"      // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"          // CgsID / CgsIDConvertToString
#include "GameShared/GameClasses/Core/CgsStringUtils.h" // CgsCore::SPrintf

// BrnGui::HudMessageAnalyzer -- reconstructed from BURNOUT_X360_ARTIST.XEX
// (wave-C group 1, the rivalry trio). All three const handlers build a stack
// GuiHudMessage and fire it through TriggerMessage(const GuiHudMessage*):
//   HandleRivalryChangeEvent @0x8251D270  (cpp:~3200)
//   HandleRivalFleeingEvent  @0x8251D430
//   HandleSignatureStunt     @0x8251D7E8

namespace BrnGui
{

// @ 0x8251D270
// A rival changed level relative to the player. Format the rival's id up front
// ("RVL_<id>"), then branch on the new status: NEW announces the promotion with the
// rival name in the second string slot; RIVAL/TARGET name the rival in the first slot;
// WRECKED additionally names the wrecked car ("CAR_<id>"). An out-of-range status trips
// a (folded) streamed assert and fires nothing.
void HudMessageAnalyzer::HandleRivalryChangeEvent(const GuiRivalryStatusChange* lpEvent) const
{
    char lacRivalName[32];
    CgsCore::SPrintf(lacRivalName, 32, "RVL_%llu", lpEvent->mRivalID);

    GuiHudMessage lMessage;

    switch (lpEvent->meNewStatus)
    {
    case GuiRivalryStatusChange::E_RIVAL_LEVEL_NEW:
        lMessage.Construct("RvlNew");
        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 1, lacRivalName);
        TriggerMessage(&lMessage);
        break;

    case GuiRivalryStatusChange::E_RIVAL_LEVEL_RIVAL:
        lMessage.Construct("RvlRival");
        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 0, lacRivalName);
        TriggerMessage(&lMessage);
        break;

    case GuiRivalryStatusChange::E_RIVAL_LEVEL_TARGET:
        lMessage.Construct("RvlTarget");
        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 0, lacRivalName);
        TriggerMessage(&lMessage);
        break;

    case GuiRivalryStatusChange::E_RIVAL_LEVEL_WRECKED:
    {
        lMessage.Construct("RvlWrecked");
        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 0, lacRivalName);

        char lacCarId[16];
        CgsIDConvertToString(lpEvent->mCarID, lacCarId);

        char lacCarName[32];
        CgsCore::SPrintf(lacCarName, 32, "CAR_%s", lacCarId);

        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 0, lacCarName);
        TriggerMessage(&lMessage);
        break;
    }

    default:
        // X360 streamed assert (BeginAssert + StrStream + FireAssert @cpp:3229), folded.
        CGS_ASSERT(false, "Undefined Rivalry Status");
        break;
    }
}

// @ 0x8251D430
// "Your rival is fleeing" -- name the rival ("RVL_<id>") in the first string slot and
// fire.
void HudMessageAnalyzer::HandleRivalFleeingEvent(const GuiRivalIsFleeing* lpEvent) const
{
    char lacRivalName[32];
    CgsCore::SPrintf(lacRivalName, 32, "RVL_%llu", lpEvent->mRivalID);

    GuiHudMessage lMessage;
    lMessage.Construct("RvlFleeing");
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 0, lacRivalName);
    TriggerMessage(&lMessage);
}

// @ 0x8251D7E8
// Signature-stunt announce -- build "GameStunt" tagged with the signature-stunt id
// ("SIG_STUNT_<id>") in the first string slot and fire.
void HudMessageAnalyzer::HandleSignatureStunt(const GuiSignatureStuntEvent* lpEvent) const
{
    GuiHudMessage lMessage;
    lMessage.Construct("GameStunt");

    char lacStuntId[32];
    CgsCore::SPrintf(lacStuntId, 32, "SIG_STUNT_%llu", lpEvent->mId);

    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 0, lacStuntId);
    TriggerMessage(&lMessage);
}

} // namespace BrnGui
