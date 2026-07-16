#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"           // HudMessageAnalyzer + EventTypeDefs + PlayerName

#include "GameShared/GameClasses/Core/CgsAssert.h"              // CGS_ASSERT
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                 // GuiTookLeadEvent / GuiTookLastEvent / GuiNetworkRemotePlayerDisconnectEvent
#include "GameSource/Gui/BrnGuiCache.h"                         // GuiCache accessors (IsOnlineStartInProgress / GetPlayer... / GetOnlinePlayerDisconnected)
#include "GameSource/GameState/Progression/BrnProfile.h"        // frozen-header include set
#include "GameShared/GameClasses/Language/CgsLanguageManager.h" // frozen-header include set

// BrnGui::HudMessageAnalyzer -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (wave-C group 03, the gamer-tag chatter trio, 3 ledger functions):
//   HudMessageAnalyzer::HandleTookLead               @0x8251E820
//   HudMessageAnalyzer::HandleTookLast               @0x8251E938
//   HudMessageAnalyzer::HandleRemotePlayerDisconnect @0x8251EBD0

namespace BrnGui
{

// @ 0x8251E820
// A player took the lead: build the "AggDrTkLead" chatter line. When an online start is
// in progress the line carries the leader's gamer tag -- if the tag fails to resolve the
// message is dropped (the short-circuit skips TriggerMessage), otherwise it always fires.
void HudMessageAnalyzer::HandleTookLead(const GuiTookLeadEvent* lpEvent) const
{
    CGS_ASSERT(lpEvent != NULL, "Invalid Took Lead Event");
    CGS_ASSERT(lpEvent->meLeadActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "lpEvent->meLeadActiveRaceCarIndex < KI_MAX_PLAYERS");

    GuiHudMessage lMessage;
    lMessage.Construct("AggDrTkLead");

    if (!mpGuiCache->IsOnlineStartInProgress()
        || AddGamerTagToMessage(&lMessage, "TEMP_ONLINE_NAME", lpEvent->meLeadActiveRaceCarIndex))
    {
        TriggerMessage(&lMessage);
    }
}

// @ 0x8251E938
// Mirror of HandleTookLead for the trailing player: "AggDrLast", same online gamer-tag
// short-circuit keyed off meLastActiveRaceCarIndex.
void HudMessageAnalyzer::HandleTookLast(const GuiTookLastEvent* lpEvent) const
{
    CGS_ASSERT(lpEvent != NULL, "Invalid Took Last Event");
    CGS_ASSERT(lpEvent->meLastActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "lpEvent->meLastActiveRaceCarIndex < KI_MAX_PLAYERS");

    GuiHudMessage lMessage;
    lMessage.Construct("AggDrLast");

    if (!mpGuiCache->IsOnlineStartInProgress()
        || AddGamerTagToMessage(&lMessage, "TEMP_ONLINE_NAME", lpEvent->meLastActiveRaceCarIndex))
    {
        TriggerMessage(&lMessage);
    }
}

// @ 0x8251EBD0
// A remote player dropped out: fire the "PlayerDiscnt" chatter line naming the departed
// peer. Gated on the LOCAL player's disconnect flag -- if the local player has themselves
// disconnected nothing is shown. The name is looked up by network-player-id (the s32
// GetOnlineName overload); an unresolved name suppresses the message.
void HudMessageAnalyzer::HandleRemotePlayerDisconnect(const GuiNetworkRemotePlayerDisconnectEvent* lpEvent) const
{
    CGS_ASSERT(lpEvent != NULL, "Invalid remote player disconnected event");
    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");

    if (mpGuiCache->GetOnlinePlayerDisconnected(
            static_cast<EActiveRaceCarIndex>(mpGuiCache->GetPlayerActiveRaceCarIndex())))
    {
        return;
    }

    bool lbNameValid = true;
    GuiHudMessage lMessage;
    lMessage.Construct("PlayerDiscnt");

    const char* lpcOnlineName = GetOnlineName(lpEvent->GetNetworkPlayerID(), &lbNameValid);
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0, lpcOnlineName);

    if (lbNameValid)
    {
        TriggerMessage(&lMessage);
    }
}

}
