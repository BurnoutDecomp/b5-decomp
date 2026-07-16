#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"   // brings BrnGuiEventTypeDefs + PlayerName

#include "GameShared/GameClasses/Core/CgsAssert.h"      // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"          // CgsIDConvertToString
#include "GameShared/GameClasses/Core/CgsStringUtils.h" // CgsCore::SPrintf
#include "GameSource/Gui/BrnGuiCache.h"                 // GetOpponentsInEvent / IsOnlineStartInProgress

// BrnGui::HudMessageAnalyzer -- rival event-flow trio (wave-C group wC_01),
// reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX.  All three handlers
// are const and tail-call the const TriggerMessage(const GuiHudMessage*).

namespace BrnGui
{

// @ 0x8251D078 -- a rival joined your event.  The joining rival id (a CgsID/u64 at
// record+0x00) is printed straight into the "RVL_%llu" string id.
void HudMessageAnalyzer::HandleJoinedEvent(const GuiEventCarJoinedEvent* lpEvent) const
{
    char lacRivalName[32];
    CgsCore::SPrintf(lacRivalName, 32, "RVL_%llu", lpEvent->mJoiningRivalID);

    GuiHudMessage lMessage;
    lMessage.Construct("EventJoined");
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 1, lacRivalName);
    TriggerMessage(&lMessage);
}

// @ 0x8251D0E8 -- "you were eliminated" with the finish-position string id.  The X360
// body ignores the (payload-less) event entirely and reads the opponents-in-event
// count straight off the cache; GetOpponentsInEvent() carries the `-1 < ...` assert
// the asm inlines here (BrnGuiCache.h:3030), so it is not duplicated.  The message id
// indexes the SECOND slot of KAPC_FINISH_POSITION_MESSAGES ([1 + position]).
void HudMessageAnalyzer::HandleEliminatedEvent(const GuiEventCarEliminatedFromEvent* /*lpEvent*/) const
{
    const s32 liPosition = mpGuiCache->GetOpponentsInEvent();

    GuiHudMessage lMessage;
    lMessage.Construct("EventRvlElim");
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 0,
                      KAPC_FINISH_POSITION_MESSAGES[1 + liPosition]);
    TriggerMessage(&lMessage);
}

// @ 0x8251CCD8 -- "a rival is on your tail".  Online events name the tailing player
// via the shared gamer-tag path (bailing without firing when the tag will not
// resolve); offline events fall back to the rival car id printed into "RVL_%s".
void HudMessageAnalyzer::HandleNetworkTailing(const GuiNetworkPlayerOnTailEvent* lpEvent) const
{
    CGS_ASSERT(lpEvent != NULL, "lpOnTailEvent");

    GuiHudMessage lMessage;
    lMessage.Construct("AggDrOnTail");

    if (mpGuiCache->IsOnlineStartInProgress())
    {
        if (!AddGamerTagToMessage(&lMessage, "TEMP_ONLINE_NAME", lpEvent->meOnTailActiveRaceCarIndex))
            return;
    }
    else
    {
        char lacRivalCarID[16];
        CgsIDConvertToString(lpEvent->mOfflineRivalCarID, lacRivalCarID);

        // asm @0x8251CD74: the "%s" source is the buffer+1 slot (var_38F), one byte
        // past the CgsIDConvertToString destination (var_390).
        char lacRivalName[32];
        CgsCore::SPrintf(lacRivalName, 32, "RVL_%s", &lacRivalCarID[1]);
        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 0, lacRivalName);
    }

    TriggerMessage(&lMessage);
}

} // namespace BrnGui
