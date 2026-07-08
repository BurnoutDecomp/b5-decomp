#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"

#include <cstring>   // std::memcpy (the delayed-message adopt)

#include "GameShared/GameClasses/Core/CgsAssert.h"     // CGS_ASSERT
#include "GameSource/Gui/BrnGuiCache.h"                // GuiCache accessors
#include "GameSource/Gui/BrnGuiFreeburnChallengeManager.h" // IsRunning/IsShowingResults
#include "GameSource/Gui/BrnGuiHudMessageDirector.h"   // HudMessageDirector::AddMessage
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h" // InGamePlayerStatusData (online name / ARCI)

// BrnGui::HudMessageAnalyzer -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (3 ledger functions + one unnamed sibling, DWARF primary file
// GameSource/Gui/BrnGuiHudMessageAnalyzer.h / .cpp):
//   HudMessageAnalyzer::TriggerMessage(const char*)       @0x825179E8  (h:~737)
//   HudMessageAnalyzer::TriggerMessage(const GuiHudMessage*) @0x82517AF8 (h:~764;
//     the ledger row is unnamed -- recovered alongside its caller)
//   HudMessageAnalyzer::HandlePlayerEliminated            @0x8251B058
//   HudMessageAnalyzer::HandleLiveRevengeUpdate           @0x8251E1F0  (cpp:3590s)

namespace BrnGui
{

// @ 0x825179E8
void HudMessageAnalyzer::TriggerMessage(const char* lpcMessageId)
{
    // Non-gating tripwires (h:737/738).
    CGS_ASSERT(NULL != lpcMessageId, "NULL != lpcMessageId");
    CGS_ASSERT('\0' != lpcMessageId[0], "'\\0' != lpcMessageId[0]");

    GuiHudMessage lMessage;
    lMessage.Construct(lpcMessageId);

    // h:747 -- the X360 streams "Invalid HUD message director"; folded static.
    CGS_ASSERT(mpHudMessageDirector != NULL, "Invalid HUD message director");
    mpHudMessageDirector->AddMessage(&lMessage, false);
}

// @ 0x82517AF8
void HudMessageAnalyzer::TriggerMessage(const GuiHudMessage* lpMessage)
{
    // Non-gating tripwires (h:764 "Invalid HUD message" / h:~770 director check;
    // both streamed on the X360, folded static).
    CGS_ASSERT(NULL != lpMessage, "Invalid HUD message");
    CGS_ASSERT(mpHudMessageDirector != NULL, "Invalid HUD message director");
    mpHudMessageDirector->AddMessage(lpMessage, false);
}

// @ 0x8251B058
void HudMessageAnalyzer::HandlePlayerEliminated(const s32* lpiEventPayload)
{
    // The road-rage elimination message, tagged with the eliminated player's name
    // (the payload's leading word is the eliminated player's ARCI).
    GuiHudMessage lMessage;
    lMessage.Construct("OnlRRPlyElim");

    bool lbNameValid = true;
    const char* lpcOnlineName =
        GetOnlineName(static_cast<EActiveRaceCarIndex>(*lpiEventPayload), &lbNameValid);
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0, lpcOnlineName);

    if (lbNameValid)
        TriggerMessage(&lMessage);
}

// @ 0x8251E1F0
void HudMessageAnalyzer::HandleLiveRevengeUpdate(const GuiLiveRevengeUpdateEvent* lpEvent)
{
    // Non-gating tripwires (cpp:3593-3597; the first streamed, folded static).
    CGS_ASSERT(lpEvent != NULL, "Invalid Live Revenge Update Event");
    CGS_ASSERT(lpEvent->meAggressorActiveRaceCarIndex >= 0,
               "lpEvent->meAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(lpEvent->meAggressorActiveRaceCarIndex < 8,
               "lpEvent->meAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    CGS_ASSERT(lpEvent->meVictimActiveRaceCarIndex >= 0,
               "lpEvent->meVictimActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(lpEvent->meVictimActiveRaceCarIndex < 8,
               "lpEvent->meVictimActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    // A running freeburn challenge suppresses the revenge chatter (the accessor
    // carries the inlined "mpChallengeManager" assert, BrnGuiCache.h:2390).
    const FreeburnChallengeManager* lpChallengeManager =
        mpGuiCache->GetFreeburnChallengeManager();
    if (lpChallengeManager->IsRunning() || lpChallengeManager->IsShowingResults())
        return;

    const EActiveRaceCarIndex leLocalPlayer =
        static_cast<EActiveRaceCarIndex>(mpGuiCache->GetPlayerActiveRaceCarIndex());

    GuiHudMessage lMessage;
    bool lbNameValid = true;
    EActiveRaceCarIndex leNamedPlayer;

    if (lpEvent->meAggressorActiveRaceCarIndex == leLocalPlayer)
    {
        // The local player dealt the takedown.
        switch (lpEvent->meNewStatus)
        {
        case 0: lMessage.Construct("LRTkdnNoRel"); break;
        case 1: lMessage.Construct("LRTkdnReig");  break;
        case 2: lMessage.Construct("LRTkdnScrSt"); break;
        case 3:
            lMessage.Construct("LRTkdnAhead");
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 0, lpEvent->miDifference);
            break;
        default:
            return;
        }
        leNamedPlayer = lpEvent->meVictimActiveRaceCarIndex;
    }
    else if (lpEvent->meVictimActiveRaceCarIndex == leLocalPlayer)
    {
        // The local player was taken down.
        switch (lpEvent->meNewStatus)
        {
        case 0: lMessage.Construct("LRTkndnNoRel"); break;
        case 1: lMessage.Construct("LRTkndnReig");  break;
        case 2: lMessage.Construct("LRTkndnScrSt"); break;
        case 4:
            lMessage.Construct("LRTkndnBehnd");
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 0, lpEvent->miDifference);
            break;
        default:
            return;
        }
        leNamedPlayer = lpEvent->meAggressorActiveRaceCarIndex;
    }
    else
    {
        return;
    }

    const char* lpcOnlineName = GetOnlineName(leNamedPlayer, &lbNameValid);
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1, lpcOnlineName);

    if (lbNameValid)
    {
        // While a wreck is on screen (crash-bar states 0/2) the message is parked
        // for the after-crash flow instead of firing immediately (the X360 copies
        // the whole 840-byte record).
        if (meCrashEntryState == 0 || meCrashEntryState == 2)
        {
            std::memcpy(&mDelayedForAfterCrashHudMessage, &lMessage, sizeof(GuiHudMessage));
            mbDelayedForAfterCrashHudMessagePending = true;
        }
        else
        {
            TriggerMessage(&lMessage);
        }
    }
}

// @ 0x824ECE10
const char* HudMessageAnalyzer::GetOnlineName(EActiveRaceCarIndex leActiveRaceCarIndex,
                                              bool* lpbValid) const
{
    // Non-gating range tripwires (h:792-794).
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");

    // Linear-search the cache's online-player records for the one whose active-race-car index
    // matches; return that player's online name and flag the result valid.
    for (s32 liIndex = 0; liIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liIndex)
    {
        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerInfo =
            mpGuiCache->GetOnlinePlayerInfo(liIndex);

        if (static_cast<EActiveRaceCarIndex>(lpPlayerInfo->meActiveRaceCarIndex) == leActiveRaceCarIndex)
        {
            *lpbValid = true;
            return lpPlayerInfo->mPlayerName.macName;
        }
    }

    // No matching player: empty name, flagged invalid (callers skip the message).
    *lpbValid = false;
    return "";
}

// The event payloads carry the remote player's network id; -1 is the "no player" sentinel
// (CgsNetwork::K_INVALID_PLAYER_ID).
static const s32 KI_INVALID_PLAYER_ID = -1;

// @ 0x8251FA90
void HudMessageAnalyzer::HandleOnlineStuntRunElimination(
        const GuiOnlineStuntRunEliminationEvent* lpEliminationEvent)
{
    CGS_ASSERT(lpEliminationEvent != NULL, "lpEliminationEvent != NULL");

    GuiHudMessage lMessage;

    if (lpEliminationEvent->mbIsLocalPlayer)
    {
        CGS_ASSERT(lpEliminationEvent->mPlayerId == KI_INVALID_PLAYER_ID,
                   "lpEliminationEvent->mPlayerId == CgsNetwork::K_INVALID_PLAYER_ID");

        if (lpEliminationEvent->mbIsTeammate)
            lMessage.Construct("OnlSrPtElim");
        else
            lMessage.Construct("OnlSrRtElim");

        TriggerMessage(&lMessage);
        return;
    }

    CGS_ASSERT(lpEliminationEvent->mPlayerId != KI_INVALID_PLAYER_ID,
               "lpEliminationEvent->mPlayerId != CgsNetwork::K_INVALID_PLAYER_ID");

    if (lpEliminationEvent->mbIsLastPlayer)
    {
        lMessage.Construct("OnlSrLpElim");
    }
    else
    {
        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpEliminatedPlayer =
            mpGuiCache->GetOnlinePlayerInfoFromPlayerId(lpEliminationEvent->mPlayerId);

        lMessage.Construct(lpEliminationEvent->mbIsTeammate ? "OnlSRTpElim" : "OnlSrRiElim");
        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0,
                          lpEliminatedPlayer->mPlayerName.macName);
    }

    TriggerMessage(&lMessage);
}

// @ 0x8251FC08
void HudMessageAnalyzer::HandleOnlineStuntRunLeading(
        const GuiOnlineStuntRunLeadingEvent* lpLeadingEvent)
{
    CGS_ASSERT(lpLeadingEvent != NULL, "lpLeadingEvent != NULL");

    GuiHudMessage lMessage;

    if (lpLeadingEvent->mbIsLocalPlayer)
    {
        CGS_ASSERT(lpLeadingEvent->mPlayerId == KI_INVALID_PLAYER_ID,
                   "lpLeadingEvent->mPlayerId == CgsNetwork::K_INVALID_PLAYER_ID");

        if (lpLeadingEvent->mbIsTeammate)
            lMessage.Construct("OnlSrPtLead");
        else
            lMessage.Construct("OnlSrRtLead");
    }
    else
    {
        CGS_ASSERT(lpLeadingEvent->mPlayerId != KI_INVALID_PLAYER_ID,
                   "lpLeadingEvent->mPlayerId != CgsNetwork::K_INVALID_PLAYER_ID");

        if (lpLeadingEvent->mbIsLastPlayer)
        {
            lMessage.Construct("OnlSrLpLead");
        }
        else
        {
            const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpLeadingPlayer =
                mpGuiCache->GetOnlinePlayerInfoFromPlayerId(lpLeadingEvent->mPlayerId);

            lMessage.Construct("OnlSrRiLead");
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0,
                              lpLeadingPlayer->mPlayerName.macName);
        }
    }

    TriggerMessage(&lMessage);
}

// @ 0x8251FD68
void HudMessageAnalyzer::HandleOnlineStuntRunVictory(
        const GuiOnlineStuntRunVictoryEvent* lpVictoryEvent)
{
    CGS_ASSERT(lpVictoryEvent != NULL, "lpVictoryEvent != NULL");

    GuiHudMessage lMessage;

    if (lpVictoryEvent->mbIsLocalPlayer)
    {
        CGS_ASSERT(lpVictoryEvent->mPlayerId == KI_INVALID_PLAYER_ID,
                   "lpVictoryEvent->mPlayerId == CgsNetwork::K_INVALID_PLAYER_ID");

        if (lpVictoryEvent->mbIsTeammate)
            lMessage.Construct("OnlSrPtWin");
        else
            lMessage.Construct("OnlSrRtWin");
    }
    else
    {
        CGS_ASSERT(lpVictoryEvent->mPlayerId != KI_INVALID_PLAYER_ID,
                   "lpVictoryEvent->mPlayerId != CgsNetwork::K_INVALID_PLAYER_ID");

        if (lpVictoryEvent->mbIsLastPlayer)
        {
            lMessage.Construct("OnlSrLpWin");
        }
        else
        {
            const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpVictoryPlayer =
                mpGuiCache->GetOnlinePlayerInfoFromPlayerId(lpVictoryEvent->mPlayerId);

            lMessage.Construct("OnlSrRiWin");
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0,
                              lpVictoryPlayer->mPlayerName.macName);
        }
    }

    TriggerMessage(&lMessage);
}

// @ 0x8251FEC8
void HudMessageAnalyzer::HandleOnlineStuntRunLastRun(
        const GuiOnlineStuntRunLastRunEvent* lpLastRunEvent)
{
    CGS_ASSERT(lpLastRunEvent != NULL, "lpLastRunEvent");
    (void)lpLastRunEvent;   // pointer-only tripwire; no field is read.

    GuiHudMessage lMessage;
    lMessage.Construct("OnlSRLastRun");
    TriggerMessage(&lMessage);
}

// @ 0x8251FF38
void HudMessageAnalyzer::HandleOnlineStuntRunMessage(
        const GuiOnlineStuntRunMessageEvent* lpMessageEvent)
{
    CGS_ASSERT(lpMessageEvent != NULL, "lpMessageEvent");

    GuiHudMessage lMessage;

    if (lpMessageEvent->meMessageType == GuiOnlineStuntRunMessageEvent::E_ONLINE_STUNT_RUN_MESSAGE_SCORE)
    {
        // A team-scored / rival-scored notification: pick the message by comparing the local
        // player's online team to the scoring player's online team, and tag it with that
        // player's name.
        const s32 liLocalPlayerTeam = mpGuiCache->GetCurrentOnlinePlayerTeam(
            static_cast<EActiveRaceCarIndex>(mpGuiCache->GetPlayerActiveRaceCarIndex()));

        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpEliminatedPlayer =
            mpGuiCache->GetOnlinePlayerInfoFromPlayerId(lpMessageEvent->mPlayerId);
        CGS_ASSERT(lpEliminatedPlayer != NULL, "lpEliminatedPlayer");

        const s32 liScoringPlayerTeam = mpGuiCache->GetCurrentOnlinePlayerTeam(
            static_cast<EActiveRaceCarIndex>(lpEliminatedPlayer->meActiveRaceCarIndex));

        lMessage.Construct(liLocalPlayerTeam == liScoringPlayerTeam ? "OnlSrTpScore" : "OnlSrRiScore");
        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0,
                          lpEliminatedPlayer->mPlayerName.macName);
    }
    else if (lpMessageEvent->meMessageType == GuiOnlineStuntRunMessageEvent::E_ONLINE_STUNT_RUN_MESSAGE_TIME)
    {
        lMessage.Construct("OnlSRTime");
    }
    else
    {
        CGS_ASSERT(false, "Unknown stunt run message type");
        return;
    }

    // Both surviving branches append the score/time value as an int param, then fire.
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 0, lpMessageEvent->miValue);
    TriggerMessage(&lMessage);
}

}
