#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"

#include <cstring>   // std::memcpy (the delayed-message adopt)

#include "GameShared/GameClasses/Core/CgsAssert.h"     // CGS_ASSERT
#include "GameSource/Gui/BrnGuiCache.h"                // GuiCache accessors
#include "GameSource/Gui/BrnGuiFreeburnChallengeManager.h" // IsRunning/IsShowingResults
#include "GameSource/Gui/BrnGuiHudMessageDirector.h"   // HudMessageDirector::AddMessage
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h" // InGamePlayerStatusData (online name / ARCI)

// includes folded in from the BrnGuiHudMessageAnalyzer_w*.cpp partfiles (2026-09-15)
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"         // CgsGui::GuiAccessPointers::mpLanguageManager
#include "GameShared/GameClasses/Language/CgsLanguageManager.h" // FormatAndAddText / E_FORMAT_ID_LOOKUP
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"        // remaining opaque event placeholders
#include "GameShared/GameClasses/Core/CgsID.h"          // CgsIDUnCompress
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // [td-msg] PC witness
#include <cstdlib>                                      // std::getenv ([td-msg] switch)
#include "GameShared/GameClasses/Core/CgsStringUtils.h"      // CgsCore::StrCpy (the parked-name copy)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"              // GuiDirtyTrick* payloads + GuiHudMessage
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"         // CgsModule::Event, GuiEventQueueBase, GuiStackEventQueue
#include "GameSource/GameState/Progression/BrnProfile.h"    // BrnProgression::Profile (100%-viewed flag)
#include "GameSource/GameState/BrnGameStateSharedIO.h"          // E_MODE_STUNT_ATTACK

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

// @ 0x825179E8 (const per DWARF h:203)
void HudMessageAnalyzer::TriggerMessage(const char* lpcMessageId) const
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

// @ 0x82517AF8 (const per DWARF h:208)
void HudMessageAnalyzer::TriggerMessage(const GuiHudMessage* lpMessage) const
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

// [gateui r2] DWARF BrnGuiHudMessageAnalyzer.h:75. The X360 has NO standalone body for
// this (it is absent from the ledger's HudMessageAnalyzer function set) because
// BrnGui::GuiModule::Prepare @0x82518D68 INLINES it as a single store at stage 3:
//     line 104:  *(v3 + 660992) = v3 + 1005332;
// i.e. analyzer + 0x00 (mpAccessPointers, DWARF h:97) := the module's shared
// GuiAccessPointers block. Reconstructed as the de-inlined method the DWARF names
// (AGENTS "inlining reversal"), since the pointer is private to the analyzer and the
// handlers dereference it (`mpAccessPointers->mpLanguageManager` in
// TriggerChallengeEndedMessage @0x82520078 and HandleStuntPerformed @0x8251B7F0).
// Construct @0x82509060 does not touch it -- the module's Prepare is its only writer.
void HudMessageAnalyzer::SetAccessPointers(CgsGui::GuiAccessPointers* lpAccessPointers)
{
    mpAccessPointers = lpAccessPointers;
}

}

// ============================================================================
// FOLDED FROM BrnGuiHudMessageAnalyzer_wB_00.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

// BrnGui::HudMessageAnalyzer -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (wave-B group 0, 3 ledger functions):
//   HudMessageAnalyzer::Construct               @0x82509060  (the store-map ctor)
//   HudMessageAnalyzer::AddGamerTagToMessage    @0x824F3500
//   HudMessageAnalyzer::HandleGenericHUDMessage @0x8251C010

namespace BrnGui
{

// @ 0x82509060
// Pure store map (Hex-Rays mislabels this one as "allocation failed" -- the asm is
// authoritative). Zeroes/defaults every analyzer slot, seeds mRandom via its inlined
// Construct (default seed + the 8-slot float ring prime), and adopts the director.
// The members the binary deliberately leaves uninitialised (macDTPendingVictim, the
// near-miss/stunt-chain statuses, miLastSignMultiplier, mChallengedEndedData,
// mRoadRuleHighScoreData, the two online name slots) are NOT touched here.
void HudMessageAnalyzer::Construct(HudMessageDirector* lpHudMessageDirector)
{
    mpHudMessageDirector = lpHudMessageDirector;

    mpGuiCache         = NULL;
    mpViewOutputQueue  = NULL;

    mbCrashBoundaryMessagePending           = false;
    meCrashEntryState                       = -1;     // E_CRASHBARSTATE_INVALID
    mbShowDrivableMessage                   = true;
    mbDelayedForAfterCrashHudMessagePending = false;

    mePlayersCurrentlyWreckState = E_WRECKED_STATE_NOT_WRECKED;
    mfCurrentWreckDuration       = 0.0f;
    mbTakedownMessagePending     = false;
    mPendingTakedownEvent        = GuiTakedownEvent();   // 40-byte record zeroed

    mShutdownPending                     = 0;
    mbTimeExtMsgPending                  = false;
    mbRoadOneMoreCrashToTotalledPending  = false;
    mbRoadRageTargetReached              = false;
    meDirtyTrickPending                  = static_cast<BrnNetwork::EPaybackType>(3); // X360 'none' sentinel

    meCurrentBoostStatus    = E_BOOST_EARNING_STATUS_OFF;
    mfBoostDuration         = 0.0f;
    meCurrentOncomingStatus = E_BOOST_EARNING_STATUS_OFF;
    meCurrentAirStatus      = E_BOOST_EARNING_STATUS_OFF;
    meCurrentDriftStatus    = E_BOOST_EARNING_STATUS_OFF;
    meCurrentSpinStatus     = E_BOOST_EARNING_STATUS_OFF;
    meCurrentCheckingStatus = E_BOOST_EARNING_STATUS_OFF;

    miPlayerStatBoost    = 0;
    miPlayerStatSpeed    = 0;
    miPlayerStatControl  = 0;
    miPlayerStatStrength = 0;

    mbShowChangeCarMessage        = true;
    mbTickerIsActive              = false;
    mbBoostOkMessageJustTriggered = false;
    mbChallengeTriggered          = false;
    mbChallengeEnded              = false;

    mfRoadRulesMessageTimeout          = 0.0f;
    mbNewRoadRulesHighScores           = false;
    mfRoadRageAchievedMessageDelayTime = 0.0f;
    mpProfile                          = NULL;
    mbOnlineWinnerWaiting              = false;

    mfTimeUntilGameCompletedHudMessage = 0.0f;
    mbGameCompletedHudMessagePending   = false;
    mbTrophyCarUnlockPending           = false;
    mTrophyCarUnlockedEvent            = GuiEventTrophyCarUnlock();  // 16-byte record zeroed
    mbOnlinePlayerLeft                 = false;

    // X360-only developer-challenge tail (stb/std of the same zero register @0x8250932C/30).
    mbDEBUGDeveloperChallengeComplete = false;   // X360 0x4F9
    mCompletedDeveloperChallenges.Construct();   // X360 0x500 (zeroes the one u64 field)

    mRandom.Construct();
}

// @ 0x824F3500
// Append the online player's gamer tag (routed through the language manager under
// lpcStringID) to the message, then reference it as a STRINGID param. Returns whether
// the tag resolved (invalid names add nothing and the caller skips the message).
bool HudMessageAnalyzer::AddGamerTagToMessage(GuiHudMessage* lpMessage, const char* lpcStringID,
                                              EActiveRaceCarIndex leActiveRaceCarIndex) const
{
    CGS_ASSERT(lpMessage != NULL, "lpMessage");
    CGS_ASSERT(lpcStringID != NULL, "lpcStringID");
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               "leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    bool lbNameValid = true;
    const char* lpcOnlineName = GetOnlineName(leActiveRaceCarIndex, &lbNameValid);

    if (lbNameValid)
    {
        // Register the resolved name as a runtime "GAMERTAG"-formatted string under
        // lpcStringID, then reference it by id from the message.
        mpAccessPointers->mpLanguageManager->FormatAndAddText(
            lpcStringID, "GAMERTAG",
            CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP, 1,
            lpcOnlineName, CgsLanguage::LanguageManager::E_FORMAT_TEXT);
        lpMessage->AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 0, lpcStringID);
    }

    return lbNameValid;
}

// @ 0x8251C010
// Pass-through title/subtitle message: build a "Generic" HUD message, guard both
// strings against the 64-byte field size, add them as display-string params, and fire.
void HudMessageAnalyzer::HandleGenericHUDMessage(const GuiGenericHUDMessage* lpEvent)
{
    GuiHudMessage lMessage;
    lMessage.Construct("Generic");

    CGS_ASSERT(std::strlen(lpEvent->mTitle) < sizeof(lpEvent->mTitle),
               "strlen(lpEvent->mTitle) < sizeof(lpEvent->mTitle)");
    CGS_ASSERT(std::strlen(lpEvent->mSubtitle) < sizeof(lpEvent->mSubtitle),
               "strlen(lpEvent->mSubtitle) < sizeof(lpEvent->mSubtitle)");

    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0, lpEvent->mTitle);
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1, lpEvent->mSubtitle);

    TriggerMessage(&lMessage);
}

}

// ============================================================================
// FOLDED FROM BrnGuiHudMessageAnalyzer_wB_01.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

// BrnGui::HudMessageAnalyzer -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Takedown family (wave-B group 1):
//   HudMessageAnalyzer::HandleTakedown              @0x8251C3C0
//   HudMessageAnalyzer::ConstructTakedownMessage    @0x824F9C28
//
// (HandleTraitorousTakedown @0x8251B0E0 is BLOCKED: the frozen header declares it
//  `const`, but its faithful body must call the non-const TriggerMessage(const
//  GuiHudMessage*) overload -- a const method cannot. Reported in funcs_blocked.)

namespace BrnGui
{

// @ 0x8251C3C0
void HudMessageAnalyzer::HandleTakedown(const GuiTakedownEvent* lpTakedown)
{
    // Non-gating tripwires (cpp:2412-2414; all streamed on the X360, folded static).
    CGS_ASSERT(lpTakedown != NULL, "Invalid takedown message");
    CGS_ASSERT(lpTakedown->meTakedownType > BrnGameState::E_TAKEDOWN_NONE, "Takedown type invalid");
    CGS_ASSERT(lpTakedown->meTakedownType < BrnGameState::E_TAKEDOWN_COUNT, "Takedown type invalid");

    const EActiveRaceCarIndex leLocalPlayer =
        static_cast<EActiveRaceCarIndex>(mpGuiCache->GetPlayerActiveRaceCarIndex());

    // [td-msg] PC witness (BRN_TD_DIAG): the GUI event reaching the analyzer, and which of the
    // three arms it will take. [FLAG PC witness]
    {
        static const bool sbTdDiag = (std::getenv("BRN_TD_DIAG") != 0);
        if (sbTdDiag && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint << "[td-msg] HandleTakedown type=" << static_cast<s32>(lpTakedown->meTakedownType)
                                       << " aggressor=" << static_cast<s32>(lpTakedown->meAggressorIndex)
                                       << " victim=" << static_cast<s32>(lpTakedown->meVictimIndex)
                                       << " player=" << static_cast<s32>(leLocalPlayer)
                                       << " crashEntryState=" << static_cast<s32>(meCrashEntryState)
                                       << " [FLAG PC witness]\n";
        }
    }

    if (leLocalPlayer == lpTakedown->meAggressorIndex)
    {
        // The local player dealt the takedown. Abort an in-progress wreck message.
        if (mePlayersCurrentlyWreckState > E_WRECKED_STATE_NOT_WRECKED &&
            mePlayersCurrentlyWreckState < E_WRECKED_STATE_WRECK_HANDLED)
        {
            mePlayersCurrentlyWreckState = E_WRECKED_STATE_WRECK_ABORTED;
        }

        if (lpTakedown->mbMarkedManTakeDown)
        {
            // Marked-man scalp collected.
            GuiHudMessage lMessage;
            bool lbNameValid = true;
            lMessage.Construct("DTGotScalp");
            const char* lpcOnlineName = GetOnlineName(lpTakedown->meVictimIndex, &lbNameValid);
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1, lpcOnlineName);
            if (lbNameValid)
                TriggerMessage(&lMessage);
        }
        else if (meCrashEntryState == 2 || mpGuiCache->IsOnlineStartInProgress())
        {
            // Fire immediately.
            GuiHudMessage lMessage;
            ConstructTakedownMessage(&lMessage, lpTakedown);
            TriggerMessage(&lMessage);
            mbTakedownMessagePending = false;
        }
        else
        {
            // A crash bar is up: park the takedown until it drops.
            mbTakedownMessagePending = true;
            mPendingTakedownEvent = *lpTakedown;
        }
    }
    else if (lpTakedown->meVictimIndex == leLocalPlayer)
    {
        // The local player was taken down. Abort an in-progress wreck message.
        if (mePlayersCurrentlyWreckState > E_WRECKED_STATE_NOT_WRECKED &&
            mePlayersCurrentlyWreckState < E_WRECKED_STATE_WRECK_HANDLED)
        {
            mePlayersCurrentlyWreckState = E_WRECKED_STATE_WRECK_ABORTED;
        }

        if (lpTakedown->mbMarkedManTakeDown)
        {
            // Scalped by a marked man.
            GuiHudMessage lMessage;
            bool lbNameValid = true;
            lMessage.Construct("DTScalped");
            const char* lpcOnlineName = GetOnlineName(lpTakedown->meAggressorIndex, &lbNameValid);
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1, lpcOnlineName);
            if (lbNameValid)
                TriggerMessage(&lMessage);
        }
        else
        {
            char lacAggressorName[16];
            CgsIDUnCompress(lpTakedown->mAggressorCarID, lacAggressorName);
            GuiHudMessage lMessage;
            lMessage.Construct("TDBdGeneral");
            TriggerMessage(&lMessage);
        }
    }
    else if (!mpGuiCache->IsOnlineStartInProgress() || mpGuiCache->GetNumActivePlayers() != 0)
    {
        // A third party's takedown seen from a spectating local player.
        const char* lpcMessageId;
        if (lpTakedown->meTakedownType == BrnGameState::E_TAKEDOWN_VERTICAL)
            lpcMessageId = "AggDrFlttns";
        else if (lpTakedown->mbSettledScore)
            lpcMessageId = "AggDrGtRevOn";
        else
            lpcMessageId = "AggDrTksDown";

        GuiHudMessage lMessage;
        lMessage.Construct(lpcMessageId);

        CGS_ASSERT(lpTakedown->meAggressorIndex != lpTakedown->meVictimIndex,
                   "lpTakedown->meAggressorIndex != lpTakedown->meVictimIndex");

        if (mpGuiCache->IsOnlineStartInProgress())
        {
            if (AddGamerTagToMessage(&lMessage, "TEMP_ONLINE_NAME", lpTakedown->meAggressorIndex) &&
                AddGamerTagToMessage(&lMessage, "TEMP_ONLINE_NAME2", lpTakedown->meVictimIndex))
            {
                TriggerMessage(&lMessage);
            }
        }
    }
}

// @ 0x824F9C28
void HudMessageAnalyzer::ConstructTakedownMessage(GuiHudMessage* lpHudMessage,
                                                  const GuiTakedownEvent* lpTakedown)
{
    // Non-gating tripwires (cpp:2576/2577).
    CGS_ASSERT(lpHudMessage != NULL, "lpHudMessage");
    CGS_ASSERT(lpTakedown != NULL, "lpTakedown");

    // [td-msg] PC witness (BRN_TD_DIAG): which string id this takedown resolves to. [FLAG PC witness]
    {
        static const bool sbTdDiag = (std::getenv("BRN_TD_DIAG") != 0);
        if (sbTdDiag && CgsDev::Log::gpDebugPrint != 0)
        {
            const s32 liType = static_cast<s32>(lpTakedown->meTakedownType);
            *CgsDev::Log::gpDebugPrint << "[td-msg] type=" << liType
                                       << " chain=" << lpTakedown->miTakedownChainCount
                                       << " multiple=" << lpTakedown->miMultipleTakedownCount
                                       << " id="
                                       << ((liType >= 0 && liType < 13) ? KAPC_TAKEDOWN_TYPES[liType] : "<out-of-range>")
                                       << " [FLAG PC witness]\n";
        }
    }

    // Multi-takedowns get their own dedicated message elsewhere.
    if (lpTakedown->miMultipleTakedownCount <= 1)
    {
        const s32 liChainCount = lpTakedown->miTakedownChainCount;
        if (liChainCount < 2 || lpTakedown->meTakedownType != BrnGameState::E_TAKEDOWN_STANDARD)
        {
            lpHudMessage->Construct(KAPC_TAKEDOWN_TYPES[lpTakedown->meTakedownType]);
        }
        else if (liChainCount - 2 < 9)
        {
            // cpp:2631 (streamed on the X360 @0x824F9CE8 -> folded static; a provably-dead
            // guard -- this lane is only reached when liChainCount >= 2).
            CGS_ASSERT(liChainCount - 2 >= 0, "liChainIndex >= 0 && liChainIndex < liChainCount");
            lpHudMessage->Construct(KAPC_TAKEDOWN_CHAIN[liChainCount - 2]);
        }
        else
        {
            lpHudMessage->Construct(KAPC_TAKEDOWN_CHAIN[8]);
        }
    }
}
}   // namespace BrnGui

// ============================================================================
// FOLDED FROM BrnGuiHudMessageAnalyzer_wB_02.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

// BrnGui::HudMessageAnalyzer -- reconstructed from BURNOUT_X360_ARTIST.XEX (wave-B).
//
// Bodied here (ledger group 2):
//   HudMessageAnalyzer::HandleCrashedEvent              @0x8251C7C0
//   HudMessageAnalyzer::HandleNetworkPlayerCrashedEvent @0x8251CBF0
//
// (HandleWreckedEvent @0x8251CA88 is NOT bodied here -- its case
// E_WRECKED_STATE_NOT_WRECKED reads a GuiCache field at +0x9FD4 that the frozen
// BrnGuiCache.h leaves unnamed (mPad_9FD4[12]); reported as blocked.)

namespace BrnGui
{

// @ 0x8251C7C0
// Crash-bar state machine: when the crash bar drops the parked takedown / dirty-trick
// / delayed-revenge messages are fired. Param is the new crash-bar state by value.
void HudMessageAnalyzer::HandleCrashedEvent(
        GuiPlayerCrashingStateChangeEvent::CrashBarState leCrashBarState)
{
    // Non-gating range/pointer tripwires (cpp:2719-2721).
    CGS_ASSERT(leCrashBarState >= 0, "lCrashingState >= 0");
    CGS_ASSERT(leCrashBarState < GuiPlayerCrashingStateChangeEvent::E_CRASHBARSTATE_COUNT,
               "lCrashingState < BrnGui::GuiPlayerCrashingStateChangeEvent::E_CRASHBARSTATE_COUNT");
    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");

    GuiHudMessage lMessage;

    switch (leCrashBarState)
    {
    case GuiPlayerCrashingStateChangeEvent::E_CRASHBARSTATE_LEAVE_CRASHED:   // 1
    case GuiPlayerCrashingStateChangeEvent::E_CRASHBARSTATE_LEAVE_TAKEDOWN:  // 3
        // Leaving the crash bar: flush the message that was parked for after the
        // crash cam, then the road-rule time / critical-damage follow-ups.
        if (mbDelayedForAfterCrashHudMessagePending)
        {
            TriggerMessage(&mDelayedForAfterCrashHudMessage);
            mbDelayedForAfterCrashHudMessagePending = false;
        }
        if (mbTimeExtMsgPending)
        {
            mbTimeExtMsgPending = false;
            lMessage.Construct("EventRRTime");
            TriggerMessage(&lMessage);
        }
        if (mbRoadOneMoreCrashToTotalledPending)
        {
            mbRoadOneMoreCrashToTotalledPending = false;
            lMessage.Construct("RRDamCrit");
            TriggerMessage(&lMessage);
        }
        break;

    case GuiPlayerCrashingStateChangeEvent::E_CRASHBARSTATE_START_TAKEDOWN:  // 2
        // Entering a takedown crash bar: announce the shutdown line, then fire the
        // takedown message parked while the bar was up, and clear the parked record.
        if (mShutdownPending != 0)
            TriggerMessage("TDGdShutD");

        if (mbTakedownMessagePending)
        {
            ConstructTakedownMessage(&lMessage, &mPendingTakedownEvent);
            TriggerMessage(&lMessage);
        }

        mbTakedownMessagePending = false;
        std::memset(&mPendingTakedownEvent, 0, sizeof(GuiTakedownEvent));
        break;

    case GuiPlayerCrashingStateChangeEvent::E_CRASHBARSTATE_START_CRASHED:   // 0
        if (meDirtyTrickPending == static_cast<BrnNetwork::EPaybackType>(3))  // X360 'none' sentinel
        {
            // No dirty trick pending: on a wreck-in-progress, announce the wreck.
            if (mePlayersCurrentlyWreckState == E_WRECKED_STATE_WRECKED_STUNT)
            {
                lMessage.Construct("StuntWrecked");
                TriggerMessage(&lMessage);
                mePlayersCurrentlyWreckState = E_WRECKED_STATE_WRECK_HANDLED;
            }
            else if (mePlayersCurrentlyWreckState == E_WRECKED_STATE_WRECKED)
            {
                lMessage.Construct("GameWrecked");
                TriggerMessage(&lMessage);
                mePlayersCurrentlyWreckState = E_WRECKED_STATE_WRECK_HANDLED;
            }
        }
        else
        {
            // A dirty trick was parked: fire its "bad" payback line tagged with the
            // stored victim name, then reset the pending trick to the 'none' sentinel.
            CGS_ASSERT(meDirtyTrickPending >= BrnNetwork::E_PAYBACK_TYPE_START,
                       "meDirtyTrickPending >= BrnNetwork::E_PAYBACK_TYPE_START");
            CGS_ASSERT(meDirtyTrickPending < 3,
                       "meDirtyTrickPending < BrnNetwork::E_PAYBACK_TYPE_COUNT");

            lMessage.Construct(KAPC_PAYBACK_TAKEDOWN_BD_MESSAGES[meDirtyTrickPending]);

            CGS_ASSERT(macDTPendingVictim != NULL, "macDTPendingVictim");
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1, macDTPendingVictim);
            TriggerMessage(&lMessage);

            meDirtyTrickPending = static_cast<BrnNetwork::EPaybackType>(3);  // X360 'none' sentinel
        }
        break;

    default:
        break;
    }
}

// @ 0x8251CBF0
// "A rival is crashing" chatter -- only while an online session is starting, tagged
// with the crashing player's gamer tag.
void HudMessageAnalyzer::HandleNetworkPlayerCrashedEvent(
        const GuiNetworkPlayerCrashingEvent* lpEvent)
{
    // Non-gating pointer/range tripwires (cpp:2920-2922).
    CGS_ASSERT(lpEvent != NULL, "lpNetworkPlayerCrashedEvent");
    CGS_ASSERT(lpEvent->meNetworkPlayerActiveRaceCarIndex >= 0,
               "lpNetworkPlayerCrashedEvent->meNetworkPlayerActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(lpEvent->meNetworkPlayerActiveRaceCarIndex < 8,
               "lpNetworkPlayerCrashedEvent->meNetworkPlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    GuiHudMessage lMessage;
    lMessage.Construct("AggDrCrashes");

    if (mpGuiCache->IsOnlineStartInProgress())
    {
        if (AddGamerTagToMessage(&lMessage, "TEMP_ONLINE_NAME",
                                 lpEvent->meNetworkPlayerActiveRaceCarIndex))
        {
            TriggerMessage(&lMessage);
        }
    }
}

}

// ============================================================================
// FOLDED FROM BrnGuiHudMessageAnalyzer_wB_03.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

// BrnGui::HudMessageAnalyzer -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (wave-B group 3, the online dirty-trick / payback trio):
//   HudMessageAnalyzer::HandleDirtyTrickNew       @0x8251D868
//   HudMessageAnalyzer::HandleDirtyTrickTriggered @0x8251D988
//   HudMessageAnalyzer::HandleDirtyTrickEnded     @0x8251DF08

namespace BrnGui
{

namespace
{
    // The X360 payback handlers resolve an online-player record by active-race-car
    // index (the compiler inlined the lookup as a maPlayerInfo scan whose asserts name
    // "mpGuiCache->GetOnlinePlayerInfo( ... )"). It is the same scan GetOnlineName runs;
    // reproduced here from the committed GuiCache::GetOnlinePlayerInfo(index) accessor
    // + the record's meActiveRaceCarIndex, returning the matching record or NULL.
    const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData*
    GetOnlinePlayerInfoByActiveRaceCarIndex(const GuiCache* lpGuiCache,
                                            EActiveRaceCarIndex leActiveRaceCarIndex)
    {
        for (s32 liIndex = 0; liIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liIndex)
        {
            const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerInfo =
                lpGuiCache->GetOnlinePlayerInfo(liIndex);

            if (static_cast<EActiveRaceCarIndex>(lpPlayerInfo->meActiveRaceCarIndex) == leActiveRaceCarIndex)
                return lpPlayerInfo;
        }
        return NULL;
    }
}

// @ 0x8251D868
// The aggressor has armed a dirty trick. Only the local aggressor gets the "arming"
// heads-up ("DTArming"); everyone else is silent.
void HudMessageAnalyzer::HandleDirtyTrickNew(const GuiDirtyTrickNewEvent* lpEvent)
{
    CGS_ASSERT(lpEvent != NULL, "Invalid Dirty Trick Event");
    CGS_ASSERT(lpEvent->meAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               "lpDTEvent->meAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(lpEvent->meAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "lpDTEvent->meAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    const EActiveRaceCarIndex leLocalPlayer =
        static_cast<EActiveRaceCarIndex>(mpGuiCache->GetPlayerActiveRaceCarIndex());

    if (lpEvent->meAggressorActiveRaceCarIndex == leLocalPlayer)
    {
        GuiHudMessage lMessage;
        lMessage.Construct("DTArming");
        TriggerMessage(&lMessage);
    }
}

// @ 0x8251D988
// A dirty trick has fired. The local aggressor sees "PbkGdPayBack" tagged with the
// victim's name; the local victim sees "PbkBdPayBack" tagged with the aggressor's name
// (and parks the aggressor's name + trick type for the follow-up on-crash message).
void HudMessageAnalyzer::HandleDirtyTrickTriggered(const GuiDirtyTrickTriggerEvent* lpEvent)
{
    CGS_ASSERT(lpEvent != NULL, "Invalid Dirty Trick Event");
    CGS_ASSERT(lpEvent->meTrickType < 3, "Trick type invalid");
    CGS_ASSERT(lpEvent->meAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               "lpDTEvent->meAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(lpEvent->meAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "lpDTEvent->meAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    CGS_ASSERT(lpEvent->meVictimActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               "lpDTEvent->meVictimActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(lpEvent->meVictimActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "lpDTEvent->meVictimActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    const EActiveRaceCarIndex leLocalPlayer =
        static_cast<EActiveRaceCarIndex>(mpGuiCache->GetPlayerActiveRaceCarIndex());

    GuiHudMessage lMessage;

    if (lpEvent->meAggressorActiveRaceCarIndex == leLocalPlayer)
    {
        // The local player pulled off the dirty trick.
        lMessage.Construct("PbkGdPayBack");

        CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");
        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpVictimInfo =
            GetOnlinePlayerInfoByActiveRaceCarIndex(mpGuiCache, lpEvent->meVictimActiveRaceCarIndex);
        CGS_ASSERT(lpVictimInfo != NULL,
                   "mpGuiCache->GetOnlinePlayerInfo( lpDTEvent->meVictimActiveRaceCarIndex )");
        CGS_ASSERT(lpVictimInfo->mPlayerName.macName != NULL,
                   "mpGuiCache->GetOnlinePlayerInfo( lpDTEvent->meVictimActiveRaceCarIndex )->mPlayerName.GetPlayerName()");

        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1,
                          lpVictimInfo->mPlayerName.macName);
        TriggerMessage(&lMessage);
    }
    else if (lpEvent->meVictimActiveRaceCarIndex == leLocalPlayer)
    {
        // The local player was the target.
        lMessage.Construct("PbkBdPayBack");
        meDirtyTrickPending = lpEvent->meTrickType;

        // Park the aggressor's online name for the on-crash payback message.
        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpAggressorInfo =
            GetOnlinePlayerInfoByActiveRaceCarIndex(mpGuiCache, lpEvent->meAggressorActiveRaceCarIndex);
        CgsCore::StrCpy(macDTPendingVictim, sizeof(macDTPendingVictim),
                        lpAggressorInfo->mPlayerName.macName);

        CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");
        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpVictimInfo =
            GetOnlinePlayerInfoByActiveRaceCarIndex(mpGuiCache, lpEvent->meVictimActiveRaceCarIndex);
        CGS_ASSERT(lpVictimInfo != NULL,
                   "mpGuiCache->GetOnlinePlayerInfo( lpDTEvent->meVictimActiveRaceCarIndex )");
        CGS_ASSERT(lpVictimInfo->mPlayerName.macName != NULL,
                   "mpGuiCache->GetOnlinePlayerInfo( lpDTEvent->meVictimActiveRaceCarIndex )->mPlayerName.GetPlayerName()");

        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1,
                          lpAggressorInfo->mPlayerName.macName);
        TriggerMessage(&lMessage);
    }
}

// @ 0x8251DF08
// A dirty trick has resolved. The local aggressor sees whether the victim survived it
// ("DTSurvive") or which trick landed (KAPC_PAYBACK_TAKEDOWN_GD_MESSAGES); a local
// victim who survived sees "DTMlfStab". Either way the pending trick slot resets.
void HudMessageAnalyzer::HandleDirtyTrickEnded(const GuiDirtyTrickEndedEvent* lpEvent)
{
    CGS_ASSERT(lpEvent != NULL, "Invalid Dirty Trick Event");
    CGS_ASSERT(lpEvent->meTrickType < 3, "Trick type invalid");
    CGS_ASSERT(lpEvent->meAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               "lpDTEvent->meAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(lpEvent->meAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "lpDTEvent->meAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    CGS_ASSERT(lpEvent->meVictimActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               "lpDTEvent->meVictimActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(lpEvent->meVictimActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "lpDTEvent->meVictimActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    GuiHudMessage lMessage;
    bool lbNameValid = true;

    const EActiveRaceCarIndex leLocalPlayer =
        static_cast<EActiveRaceCarIndex>(mpGuiCache->GetPlayerActiveRaceCarIndex());

    if (lpEvent->meAggressorActiveRaceCarIndex == leLocalPlayer)
    {
        // The local player dealt the trick -- the pending slot is done.
        meDirtyTrickPending = static_cast<BrnNetwork::EPaybackType>(3); // X360 'none' sentinel

        if (lpEvent->mbSurvived)
        {
            lMessage.Construct("DTSurvive");
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1,
                              GetOnlineName(lpEvent->meVictimActiveRaceCarIndex, &lbNameValid));
        }
        else
        {
            CGS_ASSERT(lpEvent->meTrickType >= 0,
                       "lpDTEvent->meTrickType >= BrnNetwork::E_PAYBACK_TYPE_START");
            CGS_ASSERT(lpEvent->meTrickType < 3,
                       "lpDTEvent->meTrickType < BrnNetwork::E_PAYBACK_TYPE_COUNT");

            lMessage.Construct(KAPC_PAYBACK_TAKEDOWN_GD_MESSAGES[lpEvent->meTrickType]);
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1,
                              GetOnlineName(lpEvent->meVictimActiveRaceCarIndex, &lbNameValid));
        }
    }
    else if (lpEvent->meVictimActiveRaceCarIndex == leLocalPlayer && lpEvent->mbSurvived)
    {
        // The local player was the target and lived through it.
        meDirtyTrickPending = static_cast<BrnNetwork::EPaybackType>(3); // X360 'none' sentinel
        lMessage.Construct("DTMlfStab");
    }

    if (lbNameValid)
        TriggerMessage(&lMessage);
}

}

// ============================================================================
// FOLDED FROM BrnGuiHudMessageAnalyzer_wB_04.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

// BrnGui::HudMessageAnalyzer -- pursuit chatter, reconstructed from
// BURNOUT_X360_ARTIST.XEX.  Group hint 4.
//
// NOTE: HandleRivalryChangeEvent (@0x8251D270), HandleRivalFleeingEvent
// (@0x8251D430) and HandleSignatureStunt (@0x8251D7E8) are declared `const` in the
// frozen header but their X360 bodies tail-call the non-const
// TriggerMessage(const GuiHudMessage*) (BrnGuiHudMessageAnalyzer.h:139).  A const
// method cannot invoke that overload, so those three are reported blocked rather
// than hacked around; only the non-const HandleStartPursuitEvent is bodied here.

namespace BrnGui
{

// @ 0x8251D4A0 -- pursuit pre-announce ("EventPrePurs" + "CAR_CAPS_<id>").  The X360
// body takes no payload (the pursued car id comes from the cache).
void HudMessageAnalyzer::HandleStartPursuitEvent()
{
    CGS_ASSERT(mpGuiCache->GetGameMode() == 4,
               "GsmIO::E_MODE_PURSUIT == mpGuiCache->GetGameMode()");

    const CgsID lPursuitCarID = mpGuiCache->GetPursuitCarID();

    char lCarIdBuffer[16];
    CgsIDConvertToString(lPursuitCarID, lCarIdBuffer);
    lCarIdBuffer[12] = '\0';                              // asm @0x8251D51C

    char lCarNameBuffer[32];
    CgsCore::SPrintf(lCarNameBuffer, 32, "CAR_CAPS_%s", lCarIdBuffer);
    lCarNameBuffer[31] = '\0';                            // asm @0x8251D52C

    GuiHudMessage lMessage;
    lMessage.Construct("EventPrePurs");
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 0, lCarNameBuffer);
    TriggerMessage(&lMessage);
}

} // namespace BrnGui

// ============================================================================
// FOLDED FROM BrnGuiHudMessageAnalyzer_wB_05.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

// BrnGui::HudMessageAnalyzer -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Rival event-flow chatter (wave-B group 5): the three functions assigned to this
// partfile are all BLOCKED by the same frozen-header defect that already blocked
// HandleTraitorousTakedown (group 1, BrnGuiHudMessageAnalyzer_wB_01.cpp):
//
//   HudMessageAnalyzer::HandleJoinedEvent      @0x8251D078
//   HudMessageAnalyzer::HandleEliminatedEvent  @0x8251D0E8
//   HudMessageAnalyzer::HandleNetworkTailing   @0x8251CCD8
//
// The frozen header declares all three `const` (h:223/226/229). Each one's faithful
// X360 body ends in `bl sub_82517AF8` == TriggerMessage(const GuiHudMessage*) to fire
// the message it just built with AddParam. That overload is declared NON-const
// (h:139); the only const TriggerMessage overload (h:181) takes a CgsID hash, not a
// prebuilt message with params, so it cannot stand in. A const method cannot call the
// non-const overload -> MSVC C2663 (verified: qualifiers lost on `this`).
//
// This is a header-level const-correctness gap, not something a partfile can fix
// without editing the frozen header (forbidden) or substituting a different call than
// the asm makes (unfaithful). The one-line fix -- const-qualify
// TriggerMessage(const GuiHudMessage*) in the header -- unblocks this whole family of
// const message-firing handlers (HandleTraitorousTakedown, HandleImpact,
// HandleRivalryChangeEvent, HandleRivalFleeingEvent, HandleSignatureStunt,
// HandleTookLead, HandleTookLast, HandleRemotePlayerDisconnect, plus these three).
//
// No bodies are emitted here so the partfile stays gate-clean; the three functions are
// reported in funcs_blocked with this exact gap.

namespace BrnGui
{
}   // namespace BrnGui

// ============================================================================
// FOLDED FROM BrnGuiHudMessageAnalyzer_wB_06.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

// BrnGui::HudMessageAnalyzer -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (wave B group 06):
//   HudMessageAnalyzer::HandleImpact    @0x824F2E48  (h:218)
//
// (HandleTookLead @0x8251E820 and HandleTookLast @0x8251E938 are BLOCKED, the same
//  const-mismatch gap wB_01 hit for HandleTraitorousTakedown: the frozen header declares
//  both `const`, but their faithful bodies must call the non-const
//  TriggerMessage(const GuiHudMessage*) overload -- a const method cannot. Reported in
//  funcs_blocked.)

namespace BrnGui
{

// @ 0x824F2E48 -- impact chatter. In BURNOUT_X360_ARTIST.XEX this handler compiles
// down to the payload/impact-type validation tripwires only; the happy path falls
// straight through to the epilogue with no message construction (the pseudocode's three
// StrStream blocks are the streamed asserts, not message logic). Reproduced faithfully
// (three non-gating asserts, all streamed on the X360 -> folded static).
void HudMessageAnalyzer::HandleImpact(const GuiImpactEvent* lpImpactEvent) const
{
    CGS_ASSERT(lpImpactEvent != NULL, "Invalid impact message");        // cpp:3006
    CGS_ASSERT(lpImpactEvent->meImpactType > 0, "Impact type invalid"); // cpp:3007
    CGS_ASSERT(lpImpactEvent->meImpactType < 9, "Impact type invalid"); // cpp:3008
}

} // namespace BrnGui

// ============================================================================
// FOLDED FROM BrnGuiHudMessageAnalyzer_wB_07.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

// BrnGui::HudMessageAnalyzer -- reconstructed from BURNOUT_X360_ARTIST.XEX
// (wave-B partfile 07). Road-rules / showtime chatter handlers, driven from Update's
// event drain:
//   HandleNewRoadRulesHighScore @0x824F32A0  (cpp:4646-)
//   HandleRoadRuleFailed        @0x8251F020  (cpp:4855-)
//   HandleShowtimeModeSwitch    @0x8251F240  (cpp:4923-)

namespace BrnGui
{

// @ 0x824F32A0
// Park the whole 48-byte high-score record and (re)start the road-rules message
// delay timer; Update's deferred pass fires the announce once the timer elapses.
void HudMessageAnalyzer::HandleNewRoadRulesHighScore(const GuiEventRoadRuleNewHighScore* lpEvent)
{
    // Non-gating tripwires (cpp:4646/4647; both streamed on the X360, folded static).
    CGS_ASSERT(lpEvent != NULL, "lpEvent");
    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");

    mfRoadRulesMessageTimeout = 0.0f;   // flt_82001CC0 == 0.0f
    mbNewRoadRulesHighScores  = true;
    mRoadRuleHighScoreData    = *lpEvent;  // 48-byte record copy (6-qword loop @0x824F332C)
}

// @ 0x8251F020
// Road-rule failure chatter. The road id leads the record and is SPrintf'd into a
// numeric string; the message id and its parameters depend on the failed rule type
// (time vs crash) and, for time rules, who now rules the road.
void HudMessageAnalyzer::HandleRoadRuleFailed(const GuiEventRoadRuleFail* lpEvent)
{
    // Non-gating tripwire (cpp:4855; streamed on the X360, folded static).
    CGS_ASSERT(lpEvent != NULL, "lpEvent");

    char lacRoadNumber[16];
    CgsCore::SPrintf(lacRoadNumber, 12, "%llu", lpEvent->mRoadID);
    lacRoadNumber[12] = 0;

    GuiHudMessage lMessage;

    if (lpEvent->meRuleType == 0)
    {
        // Time rule.
        if (lpEvent->mbRoadRuledByLocalPlayer)
        {
            lMessage.Construct("RRFailTimeU");
            TriggerMessage(&lMessage);
            return;
        }

        if (lpEvent->mbRoadRuledByAI)
        {
            lMessage.Construct("RRFailTimeAI");
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 1,
                              lpEvent->macCurrentRulerName);
        }
        else
        {
            lMessage.Construct("RRFailTime");
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1,
                              lpEvent->macCurrentRulerName);
        }
        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 2, lacRoadNumber);
        TriggerMessage(&lMessage);
        return;
    }

    if (lpEvent->meRuleType == 1)
    {
        // Showtime / crash rule.
        lMessage.Construct("RRFailCrash");
        TriggerMessage(&lMessage);
        return;
    }

    // Unknown rule type -- assert then fall back to the time-rule presentation
    // (cpp:4887; streamed on the X360, folded static).
    CGS_ASSERT(false, "Unknown road rule type failed:  Showing time by default");

    if (lpEvent->mbRoadRuledByLocalPlayer)
    {
        lMessage.Construct("RRFailTimeU");
        TriggerMessage(&lMessage);
        return;
    }

    CgsGui::HudMessageParamTypes leNameParamType;
    if (lpEvent->mbRoadRuledByAI)
    {
        lMessage.Construct("RRFailTimeAI");
        leNameParamType = CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID;
    }
    else
    {
        lMessage.Construct("RRFailTime");
        leNameParamType = CgsGui::E_HUDMESSAGEPARAMTYPES_STRING;
    }
    lMessage.AddParam(leNameParamType, 1, lpEvent->macCurrentRulerName);
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 1, lacRoadNumber);
    TriggerMessage(&lMessage);
}

// @ 0x8251F240
// Showtime entry/exit chatter for a REMOTE player (a different ARCI than the local
// player, and only while an online start is in progress); "ShowStart" when the peer
// is entering showtime, "ShowStop" otherwise, tagged with the peer's online name.
void HudMessageAnalyzer::HandleShowtimeModeSwitch(const GuiShowtimeModeSwitch* lpEvent)
{
    // Non-gating tripwires (cpp:4923/4924; both streamed on the X360, folded static).
    CGS_ASSERT(lpEvent != NULL, "lpEvent");
    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");

    if (lpEvent->meActiveRaceCarIndex !=
            static_cast<EActiveRaceCarIndex>(mpGuiCache->GetPlayerActiveRaceCarIndex())
        && mpGuiCache->IsOnlineStartInProgress())
    {
        GuiHudMessage lMessage;
        lMessage.Construct(lpEvent->mbEnteringShowtime ? "ShowStart" : "ShowStop");

        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerInfo =
            mpGuiCache->GetOnlinePlayerInfoFromPlayerId(lpEvent->mNetworkPlayerID);
        if (lpPlayerInfo != NULL)
        {
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0,
                              lpPlayerInfo->mPlayerName.macName);
            TriggerMessage(&lMessage);
        }
    }
}

} // namespace BrnGui

// ============================================================================
// FOLDED FROM BrnGuiHudMessageAnalyzer_wB_08.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

// BrnGui::HudMessageAnalyzer -- challenge / trophy deferred-fire handlers,
// reconstructed from BURNOUT_X360_ARTIST.XEX (wave-B group 08).
//
// Bodied here:
//   HudMessageAnalyzer::HandleChallengeEnded              @0x824F33E0
//   HudMessageAnalyzer::HandleTrophyCarUnlocked           @0x8251F8B0
//   HudMessageAnalyzer::TriggerChallengeTriggeredMessage  @0x8251F970

namespace BrnGui
{

// @ 0x824F33E0 -- park the just-ended challenge's 24-byte record so Update's deferred
// pass can fire the "challenge ended" message. Only the success/aborted/failure
// statuses arm it; the abort-before-start status is ignored, and anything else is an
// unhandled-status tripwire.
void HudMessageAnalyzer::HandleChallengeEnded(const GuiChallengeEndEvent* lpEvent)
{
    switch (lpEvent->meChallengeStatus)
    {
    case BrnGameState::E_CHALLENGE_STATUS_SUCCESS:                     // 1
    case BrnGameState::E_CHALLENGE_STATUS_ABORTED:                     // 2
    case BrnGameState::E_CHALLENGE_STATUS_ABORTED_DUE_TO_PLAYER_LEAVE: // 3
    case BrnGameState::E_CHALLENGE_STATUS_FAILURE:                     // 6
        mbChallengeEnded    = true;
        mChallengedEndedData = *lpEvent;
        break;

    case BrnGameState::E_CHALLENGE_STATUS_ABORTED_BEFORE_STARTING:     // 4
        break;

    default:
        // X360 streamed assert ("Unknown Challenge Status " << status << "\n"),
        // folded to a static tripwire (cpp:5280).
        CGS_ASSERT(false, "Unknown Challenge Status ");
        break;
    }
}

// @ 0x8251F8B0 -- "trophy car unlocked" announce; prints the unlocked car's id via
// the "CAR_CAPS_<id>" string-id convention. Called from Update's deferred trophy pass
// with &mTrophyCarUnlockedEvent.
void HudMessageAnalyzer::HandleTrophyCarUnlocked(const GuiEventTrophyCarUnlock* lpEvent)
{
    CGS_ASSERT(lpEvent != NULL, "lpTrophyUnlockedEvent");   // cpp:5190

    GuiHudMessage lMessage;
    lMessage.Construct("CarAward");

    char lacCarIdBuffer[16];
    CgsIDConvertToString(lpEvent->mTrophyCarID, lacCarIdBuffer);
    lacCarIdBuffer[12] = '\0';                              // asm @0x8251F910

    char lacCarNameBuffer[32];
    CgsCore::SPrintf(lacCarNameBuffer, 31, "CAR_CAPS_%s", lacCarIdBuffer);
    lacCarNameBuffer[31] = '\0';                            // asm @0x8251F938

    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 0, lacCarNameBuffer);
    TriggerMessage(&lMessage);
}

// @ 0x8251F970 -- fire the parked "challenge triggered" message. Gated on the message
// director still permitting the id and the freeburn-challenge manager being active
// (initialised / running / results); the current challenge's title string-id tags the
// message. The pending flag is always cleared once the id is allowed.
void HudMessageAnalyzer::TriggerChallengeTriggeredMessage()
{
    const CgsID lMessageIdHash = CgsIDCompress("FBChalTrig");

    if (mpGuiCache->GetHudMessageDirector()->IsMessageAllowed(lMessageIdHash))
    {
        if (mpGuiCache->GetFreeburnChallengeManager()->IsActive())
        {
            GuiHudMessage lMessage;
            lMessage.Construct(lMessageIdHash);

            const BrnResource::ChallengeListEntry* lpChallenge =
                mpGuiCache->GetFreeburnChallengeManager()->GetCurrentChallenge();
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 1,
                              lpChallenge->GetTitleStringID());
            TriggerMessage(&lMessage);
        }

        mbChallengeTriggered = false;
    }
}

} // namespace BrnGui

// ============================================================================
// FOLDED FROM BrnGuiHudMessageAnalyzer_wB_09.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

// BrnGui::HudMessageAnalyzer -- reconstructed from BURNOUT_X360_ARTIST.XEX (wave-B).
//
// Bodied here (ledger group 9 -- lobby chatter):
//   HudMessageAnalyzer::HandlePlayerJoinedLobby    @0x8251F458
//   HudMessageAnalyzer::TriggerPlayerLeftLobby     @0x8251F5C0
//
// NOTE: HandleRemotePlayerDisconnect (@0x8251EBD0) is declared `const` in the frozen
// header, but its X360 body tail-calls the non-const TriggerMessage(const
// GuiHudMessage*) (BrnGuiHudMessageAnalyzer.h:139) -- a const method cannot invoke
// that overload.  Same header const/non-const gap that blocks the const handlers in
// _wB_04.cpp; reported blocked rather than hacked around (no const_cast).

namespace BrnGui
{

// CgsNetwork::K_INVALID_PLAYER_ID -- the X360 'no network player' sentinel; defined once in this
// TU's file-scope constants (the copy this section carried went with the partfile fold, issue #20).

// @ 0x8251F458
// Freeburn lobby-join chatter: a new remote player entered the lobby -- announce
// "OnlFBJoin" tagged with their online name.
void HudMessageAnalyzer::HandlePlayerJoinedLobby(const GuiEventNetworkPlayerJoinedLobby* lpEvent)
{
    // Non-gating pointer/range tripwires (cpp:5032-5034).
    CGS_ASSERT(lpEvent != NULL, "lpEvent");
    CGS_ASSERT(lpEvent->mNetworkPlayerID != KI_INVALID_PLAYER_ID,
               "lpEvent->mNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");
    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");

    // Debug trace of the joining player id (log-category gated).
    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "NetworkPlayerID: " << lpEvent->mNetworkPlayerID << "\n";
    }

    const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerData =
        mpGuiCache->GetOnlinePlayerInfoFromPlayerId(lpEvent->mNetworkPlayerID);
    CGS_ASSERT(lpPlayerData != NULL, "lpPlayerData");

    GuiHudMessage lMessage;
    lMessage.Construct("OnlFBJoin");
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0, lpPlayerData->mPlayerName.macName);
    TriggerMessage(&lMessage);
}

// @ 0x8251F5C0
// Fire the parked "player left the lobby" message: "OnlFBLeave" tagged with the
// stored name, then clear the pending flag + name.
void HudMessageAnalyzer::TriggerPlayerLeftLobby()
{
    CGS_ASSERT(mbOnlinePlayerLeft == true, "mbOnlinePlayerLeft == true");   // cpp:5079

    GuiHudMessage lMessage;
    lMessage.Construct("OnlFBLeave");
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0, mOnlinePlayerLeftName.macName);
    TriggerMessage(&lMessage);

    mbOnlinePlayerLeft = false;
    mOnlinePlayerLeftName.macName[0] = 0;
}

}

// ============================================================================
// FOLDED FROM BrnGuiHudMessageAnalyzer_wB_10.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

// BrnGui::HudMessageAnalyzer -- Burning Home Run team-change chatter and the
// online event-finisher ("winner") message, reconstructed from
// BURNOUT_X360_ARTIST.XEX.  Group hint 10.

namespace BrnGui
{

// @ 0x82525E10 -- Update-dispatched entry for the online team-change event (id 445).
// Only forwards to the Burning Home Run handler when the current mode is
// GsmIO::E_MODE_ONLINE_BURNING_HOME_RUN (== 13; cache meGameModeType @0x9E58).
void HudMessageAnalyzer::HandleOnlineTeamChangeMessage(const GuiOnlineTeamChangeEvent* lpEvent)
{
    CGS_ASSERT(lpEvent != NULL, "lpEvent");
    CGS_ASSERT(lpEvent->meActiveRaceCarIndex >= 0,
               "lpEvent->meActiveRaceCarIndex >= 0");
    CGS_ASSERT(lpEvent->meActiveRaceCarIndex < 8,
               "lpEvent->meActiveRaceCarIndex < BrnWorld::KI_MAX_ACTIVE_RACE_CARS");
    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");

    if (mpGuiCache->GetGameMode() == 13)   // GsmIO::E_MODE_ONLINE_BURNING_HOME_RUN
        HandleBurningHomeRunTeamChange(lpEvent->meActiveRaceCarIndex);
}

// @ 0x8251C110 -- fires the Burning Home Run "runner" / "traitor runner" messages
// when a player's team changes.
void HudMessageAnalyzer::HandleBurningHomeRunTeamChange(EActiveRaceCarIndex leActiveRaceCarIndex)
{
    CGS_ASSERT(leActiveRaceCarIndex >= 0, "leActiveRaceCarIndex >= 0");
    CGS_ASSERT(leActiveRaceCarIndex < 8,
               "leActiveRaceCarIndex < BrnWorld::KI_MAX_ACTIVE_RACE_CARS");
    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");

    const s32 leOnlinePlayerTeam = mpGuiCache->GetCurrentOnlinePlayerTeam(leActiveRaceCarIndex);
    CGS_ASSERT(leOnlinePlayerTeam != 0, "GsmIO::E_PLAYER_TEAM_NONE != leOnlinePlayerTeam");

    // NOTE: on the non-runner paths the X360 fires lMessage WITHOUT Construct-ing it
    // first -- LABEL_15 in the ARTIST asm (@0x8251C25C) is reachable with this stack
    // message left uninitialised.  Reproduced faithfully below (same TriggerMessage
    // call tail, no sanitising early-out).
    GuiHudMessage lMessage;

    if (leActiveRaceCarIndex == mpGuiCache->GetPlayerActiveRaceCarIndex())
    {
        // The local player's team changed.
        if (leOnlinePlayerTeam == 2)
            lMessage.Construct("OnlBHRRunner");
        TriggerMessage(&lMessage);
        return;
    }

    // A remote player's team changed.
    if (leOnlinePlayerTeam != 2)
    {
        TriggerMessage(&lMessage);   // LABEL_15 (unconstructed message on the X360)
        return;
    }

    lMessage.Construct("OnlBHRTrRnnr");

    bool lbNameValid = true;
    const char* lpcOnlineName = GetOnlineName(leActiveRaceCarIndex, &lbNameValid);
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0, lpcOnlineName);

    if (lbNameValid)
        TriggerMessage(&lMessage);
}

// @ 0x8251E790 -- deferred pass from Update: announces the online event winner.
void HudMessageAnalyzer::TriggerEventFinisher()
{
    CGS_ASSERT(mbOnlineWinnerWaiting, "mbOnlineWinnerWaiting");
    mbOnlineWinnerWaiting = false;

    GuiHudMessage lMessage;
    lMessage.Construct("EventRvlWins");
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1, mOnlineWinnerName.macName);
    TriggerMessage(&lMessage);
}

} // namespace BrnGui

// ============================================================================
// FOLDED FROM BrnGuiHudMessageAnalyzer_wB_11.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

// BrnGui::HudMessageAnalyzer -- reconstructed from BURNOUT_X360_ARTIST.XEX
// (wave-B partfile 11). Stunt-area completion + burning-route start-failure chatter,
// driven from Update's event drain:
//   HandleStuntsComplete(AreaComplete)      @0x8251F6E8  (cpp:5147-)
//   HandleFailedToStartBurningRoute         @0x8251D1A0  (cpp:3147-)
//
// The third fan-out function -- HandleStuntPerformed @0x8251B608 -- is reported blocked
// (see funcs_blocked): its skill-line body calls the undeclared
// CgsLanguage::LanguageManager::FormatTextFromInt.

namespace BrnGui
{

// @ 0x8251F6E8
// "All stunts of one collectible family are done in this county" -- fire "StntAllDone"
// tagged with the family-completion string id and the county string id (both looked up
// in the committed X360 rodata tables). Bounds-asserts both indices first.
void HudMessageAnalyzer::HandleStuntsComplete(const GuiEventStuntAreaComplete* lpEvent)
{
    // Non-gating tripwires (cpp:5147/5151/5152; all streamed on the X360, folded static).
    CGS_ASSERT(lpEvent != NULL, "lpStuntAreaCompleteEvent");
    CGS_ASSERT(lpEvent->meStuntElementType < E_STUNTTYPE_COUNT,
               "lpStuntAreaCompleteEvent->meStuntElementType < E_STUNTTYPE_COUNT");
    CGS_ASSERT(lpEvent->meCounty < BrnWorld::E_COUNTY_VALID_COUNT,
               "lpStuntAreaCompleteEvent->meCounty < BrnWorld::E_COUNTY_VALID_COUNT");

    GuiHudMessage lMessage;
    lMessage.Construct("StntAllDone");
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 0,
                      KAPC_COLLECTABLE_COMPLETION_STRINGID[lpEvent->meStuntElementType]);
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 1,
                      KAPC_COUNTY_STRINGID[lpEvent->meCounty]);
    TriggerMessage(&lMessage);
}

// @ 0x8251D1A0
// The burning route could not start because the player is not in the required special
// car -- fire "BRWrongCar" naming the wanted car ("CAR_CAPS_<id>", by string id).
void HudMessageAnalyzer::HandleFailedToStartBurningRoute(const GuiEventFailedToStartEvent* lpEvent)
{
    // Non-gating tripwires (cpp:3147/3148; both streamed on the X360, folded static).
    CGS_ASSERT(lpEvent != NULL, "lpEvent");
    CGS_ASSERT(lpEvent->mSpecialCarID != static_cast<CgsID>(0),
               "kCGSID_NULL != lpEvent->mSpecialCarID");

    GuiHudMessage lMessage;
    lMessage.Construct("BRWrongCar");

    char lacCarId[16];
    CgsIDConvertToString(lpEvent->mSpecialCarID, lacCarId);

    char lacCarName[32];
    CgsCore::SnPrintf(lacCarName, 32, "CAR_CAPS_%s", lacCarId);   // the asm names SnPrintf here
    lacCarName[31] = '\0';                                // asm @0x8251D250

    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 1, lacCarName);
    TriggerMessage(&lMessage);
}

} // namespace BrnGui

// ============================================================================
// FOLDED FROM BrnGuiHudMessageAnalyzer_wB_12.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

// BrnGui::HudMessageAnalyzer::Update -- reconstructed from BURNOUT_X360_ARTIST.XEX @0x82525FC0.
//
// The GUI-event-stream dispatcher: it drains the module input queue, routes every record by
// its wire id to the Handle* family (three dense X360 jump tables -- id 64+case @ 82526234,
// id 313+case @ 82526838, id 419+case @ 82526BF0; folded here into one switch on the absolute
// wire id), then runs the deferred-message timer passes (event-finisher, crash boundary,
// wreck reset, challenge triggered/ended, road-rules high score, developer challenge, road-rage
// target, trophy car, game-completed countdown, player-left).
//
// Header-exposure notes (wave-C keystone; the wave-B gaps are CLOSED):
//  * GuiCache friends HudMessageAnalyzer -- the consumer-carved snapshot members
//    (mbGameplayHudActive / miGameFlowState) are read by name below, exactly the inlined
//    X360 loads. [gateui r4] The former "+0x4930 pending cluster" carve is GONE: it was an
//    alias of the cache's InGameMessagesQueue @+0x4080, and the id-291/320 arm now goes
//    through GuiCache::GetInGameMessagesQueue(). See that case for the asm proof.
//  * Profile::Get/SetOneHundredHudMessageViewed are the DWARF-attested accessors
//    (BrnProfile.h:556/:560); the X360 inlines both here.
//  * HandleDeveloperChallengeMessageDEBUG(const GuiDeveloperChallengesCompleted*) is
//    declared in the frozen header (real named X360 symbol @ the case-596 `bl`
//    @0x825275D4; its body is its OWN ledger row).

namespace BrnGui
{

namespace
{
    // DWARF cpp:427-429 places these message-delay thresholds at .cpp file scope (internal
    // linkage). X360 rodata: flt_82001C98 == 1.0f, flt_82004270 == 3.0f, flt_82004A20 == 10.0f.
    const f32 KF_ROADRULEMESSAGEDELAY                 = 1.0f;
    const f32 KF_MIN_ROADRAGE_TARGET_ACHIEVED_DELAY   = 3.0f;
    const f32 KF_TIMEUNTIL_ONE_HUNDRED_MESSAGE        = 10.0f;

    // The X360 skillz dispatch (case 542) inlines the online-player lookup as a maPlayerInfo
    // scan keyed on each record's active-race-car index (the asserts name
    // "mpGuiCache->GetOnlinePlayerInfo( ... )"). The helper that reproduces it,
    // GetOnlinePlayerInfoByActiveRaceCarIndex, is defined ONCE in this TU, in the wave-B
    // dirty-trick section (formerly _wB_03) -- this section used to carry an identical copy,
    // removed when the partfiles were folded back (issue #20).
}

// @ 0x82525FC0
void HudMessageAnalyzer::Update(const CgsGui::GuiEventQueueBase<32768, 16>* lpGuiEventQueue,
                                CgsGui::GuiStackEventQueue::GuiEventQueueLarge* lpViewOutputQueue)
{
    // Entry tripwires (cpp:535 streamed / cpp:538) and adopt the view output queue.
    CGS_ASSERT(lpViewOutputQueue != NULL,
               "View output queue is not valid. Is it definately locked for writing?");
    mpViewOutputQueue = lpViewOutputQueue;
    CGS_ASSERT(lpGuiEventQueue != NULL, "lpEventQueue != NULL");

    bool lbHadWreckEvent = false;   // v84

    const CgsModule::Event* lpEvent = NULL;
    s32 liEventSize = 0;
    s32 liEventType = lpGuiEventQueue->GetFirstEvent(&lpEvent, &liEventSize);

    if (lpEvent != NULL)
    {
        do
        {
            switch (liEventType)
            {
                case 64:   // adopt the shared GuiCache pointer delivered by the first record
                    if (mpGuiCache == NULL)
                    {
                        mpGuiCache = *reinterpret_cast<GuiCache* const*>(lpEvent);
                        CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");
                    }
                    break;

                case 93:   // shutdown / sign-multiplier reset
                    mShutdownPending = 0;
                    miLastSignMultiplier = 0;
                    break;

                case 149:
                    HandleGenericHUDMessage(reinterpret_cast<const GuiGenericHUDMessage*>(lpEvent));
                    break;
                case 150:
                    HandleJoinedEvent(reinterpret_cast<const GuiEventCarJoinedEvent*>(lpEvent));
                    break;
                case 151:
                    HandleEliminatedEvent(reinterpret_cast<const GuiEventCarEliminatedFromEvent*>(lpEvent));
                    break;
                case 153:
                    TriggerMessage("WrongWay");
                    break;
                case 168:
                    mbRoadRageTargetReached = true;
                    break;
                case 177:
                    HandleDirtyTrickNew(reinterpret_cast<const GuiDirtyTrickNewEvent*>(lpEvent));
                    break;
                case 179:
                    HandleDirtyTrickTriggered(reinterpret_cast<const GuiDirtyTrickTriggerEvent*>(lpEvent));
                    break;
                case 181:
                    HandleDirtyTrickEnded(reinterpret_cast<const GuiDirtyTrickEndedEvent*>(lpEvent));
                    break;
                case 206:
                    HandleChainedBoost(reinterpret_cast<const GuiEventBoostInfo*>(lpEvent));
                    break;
                case 216:   // "SuperJump" -- only when no online start is in progress
                    if (!mpGuiCache->IsOnlineStartInProgress())
                    {
                        GuiHudMessage lMessage;
                        lMessage.Construct("SuperJump");
                        TriggerMessage(&lMessage);
                    }
                    break;
                case 217:
                case 218:
                    // [gateui r4 boot fix] 218 (GuiEventBoostBarStuntInfo) shares this arm: the
                    // console dispatches BOTH stunt-info flavours to HandleStuntInfo (scout.md
                    // section C and the _gUI_01 banner agree; the two payloads are the same
                    // 12-byte {type, current, total} record, ids 217/218 chosen by game mode in
                    // TranslateGameActionsToGuiEvents case 58). The reconstructed switch carried
                    // only 217, so the freeburn flavour (218, the one a free drive posts) fell
                    // through the default and the HUD popup never fired -- boot-drive
                    // 2026-08-20 17:21: gui-event id=218 posted, no hud rung.
                    HandleStuntInfo(reinterpret_cast<const GuiEventStuntInfo*>(lpEvent));
                    break;
                case 219:
                    HandleStuntsComplete(reinterpret_cast<const GuiEventStuntAreaComplete*>(lpEvent));
                    break;
                case 220:
                    HandleStuntsComplete(reinterpret_cast<const GuiEventStuntAllComplete*>(lpEvent));
                    break;
                case 267:
                    HandleRemotePlayerDisconnect(
                        reinterpret_cast<const GuiNetworkRemotePlayerDisconnectEvent*>(lpEvent));
                    break;
                case 276:
                    HandlePlayerJoinedLobby(reinterpret_cast<const GuiEventNetworkPlayerJoinedLobby*>(lpEvent));
                    break;
                case 277:
                    HandlePlayerLeftLobby(reinterpret_cast<const GuiEventNetworkPlayerLeftLobby*>(lpEvent));
                    break;

                case 291:   // shares the in-game-message-queue reset with id 320
                case 320:
                {
                    // ⭐ [gateui r4] CE-4: REWRITTEN AGAINST THE REAL MEMBERS. The round-2
                    // body wrote three consumer-named carve fields
                    // (mu64PendingEventPayload / miPendingEventStatusA / ...B) that were an
                    // ALIAS of the cache's InGameMessagesQueue: the console forms the queue
                    // base itself here --
                    //     0x82526B0C  addi r11, r11, 0x4080   ; &mInGameMessagesQueue
                    //     0x82526B10  lwz  r10, 0x8B0(r11)    ; maeMessageState[0]
                    //     0x82526B14  std  r25, 0x8B8(r11)    ; muCurrentEventEndTime = 0
                    //     0x82526B28  stw  r25, 0x8B0(r11)    ; maeMessageState[0] = 0
                    //     0x82526B2C  lwz  r10, 0x8B4(r11)    ; maeMessageState[1]
                    //     0x82526B40  stw  r25, 0x8B4(r11)    ; maeMessageState[1] = 0
                    // (r25 == 0, `li r25, 0` @0x82525FE0). 16512+0x8B0/0x8B4/0x8B8 are the
                    // 18736/18740/18744 the carve was named after.
                    //
                    // So the arm's real meaning: DROP THE LIVE EXPIRY AND CANCEL ANY SLOT
                    // THAT HAS NOT BECOME VISIBLE YET -- states 1 and 2 are
                    // E_MESSAGESTATE_WAITING and E_MESSAGESTATE_TRANSIN
                    // (BrnInGameMessagesComponent.h); VISIBLE(3)/TRANSOUT(4) slots are left
                    // alone to finish their transition, which is why the compare is against
                    // exactly {1,2} and not "non-zero".
                    InGameMessagesQueue* lpQueue = mpGuiCache->GetInGameMessagesQueue();

                    lpQueue->muCurrentEventEndTime = 0;
                    for (s32 liSlot = 0; liSlot < 2; ++liSlot)
                    {
                        if (lpQueue->maeMessageState[liSlot] == E_MESSAGESTATE_WAITING ||
                            lpQueue->maeMessageState[liSlot] == E_MESSAGESTATE_TRANSIN)
                        {
                            lpQueue->maeMessageState[liSlot] = E_MESSAGESTATE_NOMESSAGE;
                        }
                    }
                    break;
                }

                case 305:
                    CGS_ASSERT(lpEvent != NULL, "lpGuiEventAllOfTypeComplete != NULL");
                    HandleAllEventsOfTypeComplete(
                        static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(
                            *reinterpret_cast<const s32*>(lpEvent)));
                    break;
                case 306:
                {
                    GuiHudMessage lMessage;
                    lMessage.Construct("EvryRivShut");
                    TriggerMessage(&lMessage);
                    break;
                }
                case 310:   // arm the "100% complete online" game-completed countdown
                {
                    // The payload gate is a BYTE load on the X360 (lbz event+0 @0x825266B8).
                    if (mpGuiCache->GetGameMode() != -1
                        && *reinterpret_cast<const bool*>(lpEvent)
                        && !mpGuiCache->GetProfile()->GetOneHundredHudMessageViewed())
                    {
                        mbGameCompletedHudMessagePending = true;
                        mfTimeUntilGameCompletedHudMessage = KF_TIMEUNTIL_ONE_HUNDRED_MESSAGE;
                    }
                    break;
                }
                case 312:
                {
                    GuiHudMessage lMessage;
                    lMessage.Construct("EvryEventJnc");
                    TriggerMessage(&lMessage);
                    break;
                }
                case 313:
                    CGS_ASSERT(lpEvent != NULL, "lpGuiEventAllJunctionsDiscoveredOfType != NULL");
                    HandleAllJunctionsOfTypeFound(
                        static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(
                            *reinterpret_cast<const s32*>(lpEvent)));
                    break;
                case 315:
                {
                    GuiHudMessage lMessage;
                    lMessage.Construct("EvryDrveThru");
                    TriggerMessage(&lMessage);
                    break;
                }
                case 316:
                    HandleFailedToStartBurningRoute(reinterpret_cast<const GuiEventFailedToStartEvent*>(lpEvent));
                    break;
                case 322:
                    mbTimeExtMsgPending = false;
                    mbRoadOneMoreCrashToTotalledPending = false;
                    mbRoadRageTargetReached = false;
                    break;
                case 337:
                    HandleRoadRuleFailed(reinterpret_cast<const GuiEventRoadRuleFail*>(lpEvent));
                    break;
                case 342:
                    HandleNewRoadRulesHighScore(reinterpret_cast<const GuiEventRoadRuleNewHighScore*>(lpEvent));
                    break;
                case 348:   // road-rage "one more crash to totalled" flag (byte @ record+4)
                    if (reinterpret_cast<const GuiEventRoadRagePlayerDamage*>(lpEvent)->maData[4])
                        mbRoadOneMoreCrashToTotalledPending = true;
                    break;
                case 350:   // adopt the player profile delivered by the event
                {
                    BrnProgression::Profile* lpProfile =
                        *reinterpret_cast<BrnProgression::Profile* const*>(lpEvent);
                    mpProfile = lpProfile;
                    CGS_ASSERT(lpProfile != NULL, "mpProfile");
                    break;
                }
                case 363:
                    HandleTakedown(reinterpret_cast<const GuiTakedownEvent*>(lpEvent));
                    break;
                case 365:
                    HandleImpact(reinterpret_cast<const GuiImpactEvent*>(lpEvent));
                    break;
                case 366:
                    HandleDriveThrough(reinterpret_cast<const GuiDriveThroughEvent*>(lpEvent));
                    break;
                case 368:
                    HandleSignatureStunt(reinterpret_cast<const GuiSignatureStuntEvent*>(lpEvent));
                    break;
                case 369:
                    HandleLiveRevengeUpdate(reinterpret_cast<const GuiLiveRevengeUpdateEvent*>(lpEvent));
                    break;
                case 375:   // park the trophy-car-unlocked record (mode must be valid)
                    if (mpGuiCache->GetGameMode() != -1)
                    {
                        mbTrophyCarUnlockPending = true;
                        mTrophyCarUnlockedEvent = *reinterpret_cast<const GuiEventTrophyCarUnlock*>(lpEvent);
                    }
                    break;
                case 377:   // crash-bar boundary bookkeeping
                {
                    const GuiPlayerCrashingStateChangeEvent* lpCrashEvent =
                        reinterpret_cast<const GuiPlayerCrashingStateChangeEvent*>(lpEvent);
                    mbCrashBoundaryMessagePending = true;
                    meCrashEntryState = static_cast<s32>(lpCrashEvent->meCurrentState);
                    if (lpCrashEvent->meCurrentState != 0)
                        mbShowDrivableMessage = true;
                    break;
                }
                case 378:   // fire the deferred "Crashed" line
                    if (meCrashEntryState == 0 && mbShowDrivableMessage)
                    {
                        GuiHudMessage lMessage;
                        lMessage.Construct("Crashed");
                        TriggerMessage(&lMessage);
                        mbShowDrivableMessage = false;
                    }
                    break;
                case 397:
                    HandleShowtimeModeSwitch(reinterpret_cast<const GuiShowtimeModeSwitch*>(lpEvent));
                    break;
                case 399:
                    HandleShowtimeMultiplierMessage(reinterpret_cast<const GuiHUDMessageShowtimeMultiplier*>(lpEvent));
                    break;
                case 400:
                    HandleSignSmashMessage(reinterpret_cast<const GuiHUDMessageSignSmashed*>(lpEvent));
                    break;
                case 401:
                    HandleCrushComboMessage(reinterpret_cast<const GuiHUDMessageCrushCombo*>(lpEvent));
                    break;
                case 404:
                    HandlePowerParkingResult(reinterpret_cast<const GuiPowerParkResult*>(lpEvent));
                    break;
                case 419:
                    HandleEventDistanceToFinish(reinterpret_cast<const GuiInEventDistanceToFinish*>(lpEvent));
                    break;
                case 420:
                    HandleEventLeaderSplitTime(reinterpret_cast<const GuiInEventLeaderSplit*>(lpEvent));
                    break;
                case 421:
                {
                    GuiHudMessage lMessage;
                    lMessage.Construct("EventNeck");
                    TriggerMessage(&lMessage);
                    break;
                }
                case 423:
                    HandleEventFinisher(reinterpret_cast<const GuiInEventFinisher*>(lpEvent));
                    break;
                case 425:
                    HandleRaceCheckpointReached(reinterpret_cast<const GuiRaceCheckpointReached*>(lpEvent));
                    break;
                case 429:
                    HandleStuntPerformed(reinterpret_cast<const GuiHUDMessageStuntPerformed*>(lpEvent));
                    break;
                case 430:   // combo -- gated on a valid game-flow state
                    if (mpGuiCache != NULL && mpGuiCache->miGameFlowState != -1)
                        HandleComboPerformed(reinterpret_cast<const GuiHUDMessageComboPerformed*>(lpEvent));
                    break;
                case 431:
                    TriggerMessage("StuntTimeUp");
                    break;
                case 433:
                    HandleStartPursuitEvent();
                    break;
                case 439:
                    HandleRivalryChangeEvent(reinterpret_cast<const GuiRivalryStatusChange*>(lpEvent));
                    break;
                case 440:
                    HandleRivalFleeingEvent(reinterpret_cast<const GuiRivalIsFleeing*>(lpEvent));
                    break;
                case 445:
                    HandleOnlineTeamChangeMessage(reinterpret_cast<const GuiOnlineTeamChangeEvent*>(lpEvent));
                    break;
                case 446:
                {
                    GuiHudMessage lMessage;
                    lMessage.Construct("OnlRRBlEscpe");
                    TriggerMessage(&lMessage);
                    break;
                }
                case 447:
                {
                    GuiHudMessage lMessage;
                    lMessage.Construct("OnlRRRacerBh");
                    TriggerMessage(&lMessage);
                    break;
                }
                case 448:
                    HandleLeaderPassedMileBoundary(reinterpret_cast<const GuiLeaderPassedMileBoundaryEvent*>(lpEvent));
                    break;
                case 449:
                    HandleLeaderPassedKMBoundary(reinterpret_cast<const GuiLeaderPassedKMBoundaryEvent*>(lpEvent));
                    break;
                case 450:
                    HandlePlayerEliminated(reinterpret_cast<const s32*>(lpEvent));
                    break;
                case 451:
                {
                    GuiHudMessage lMessage;
                    lMessage.Construct("OnlRRYouElim");
                    TriggerMessage(&lMessage);
                    break;
                }
                case 452:
                {
                    GuiHudMessage lMessage;
                    lMessage.Construct("OnlRRLastBl");
                    TriggerMessage(&lMessage);
                    break;
                }
                case 453:
                    HandleTraitorousTakedown(reinterpret_cast<const GuiTraitorousTakedownEvent*>(lpEvent));
                    break;
                case 454:
                    HandleBurningHomeRunCheckpointReached(
                        reinterpret_cast<const GuiBHRCheckpointReachedEvent*>(lpEvent));
                    break;
                case 455:
                    HandleBurningHomeRunRunnerCrashes(
                        reinterpret_cast<const GuiHUDMessageBHRRunnerCrashed*>(lpEvent));
                    break;
                case 482:
                    HandleNetworkPlayerCrashedEvent(
                        reinterpret_cast<const GuiNetworkPlayerCrashingEvent*>(lpEvent));
                    break;
                case 483:
                    HandleNetworkBattling(reinterpret_cast<const GuiNetworkPlayerBattlingEvent*>(lpEvent));
                    break;
                case 484:
                    HandleTookLead(reinterpret_cast<const GuiTookLeadEvent*>(lpEvent));
                    break;
                case 485:
                    HandleTookLast(reinterpret_cast<const GuiTookLastEvent*>(lpEvent));
                    break;
                case 486:
                    HandleNetworkTailing(reinterpret_cast<const GuiNetworkPlayerOnTailEvent*>(lpEvent));
                    break;
                case 487:
                    HandleOnlineStuntRunElimination(
                        reinterpret_cast<const GuiOnlineStuntRunEliminationEvent*>(lpEvent));
                    break;
                case 488:
                    HandleOnlineStuntRunLeading(reinterpret_cast<const GuiOnlineStuntRunLeadingEvent*>(lpEvent));
                    break;
                case 489:
                    HandleOnlineStuntRunVictory(reinterpret_cast<const GuiOnlineStuntRunVictoryEvent*>(lpEvent));
                    break;
                case 490:
                    HandleOnlineStuntRunLastRun(reinterpret_cast<const GuiOnlineStuntRunLastRunEvent*>(lpEvent));
                    break;
                case 491:
                    HandleOnlineStuntRunMessage(reinterpret_cast<const GuiOnlineStuntRunMessageEvent*>(lpEvent));
                    break;

                case 538:   // ticker on/off
                    mbTickerIsActive = *reinterpret_cast<const bool*>(lpEvent);
                    break;
                case 540:   // toggle the "change car" prompt
                    mbShowChangeCarMessage = (mbShowChangeCarMessage == false);
                    break;

                case 542:   // burnout-skillz "X beat Y's record" HUD flash
                {
                    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");   // cpp:1220
                    const FreeburnChallengeManager* lpChallengeManager =
                        mpGuiCache->GetFreeburnChallengeManager();

                    // Suppress the flash while a freeburn challenge is live (running / showing results).
                    if (!(lpChallengeManager->IsRunning() || lpChallengeManager->IsShowingResults()))
                    {
                        const GuiNewBurnoutHudMessageEvent* lpHudMessageEvent =
                            reinterpret_cast<const GuiNewBurnoutHudMessageEvent*>(lpEvent);
                        const CgsID lMessageId =
                            KA_SKILLZ_MESSAGE_IDS[lpHudMessageEvent->meMessageType][lpHudMessageEvent->meSkill];

                        if (lMessageId != 0)
                        {
                            GuiHudMessage lMessage;
                            lMessage.Construct(lMessageId);

                            bool lbValid = true;
                            switch (lpHudMessageEvent->meMessageType)
                            {
                                case GuiNewBurnoutHudMessageEvent::E_BURNOUT_SKILLZ_MESSAGE_TYPE_X_BEAT_YS:
                                {
                                    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache != NULL");   // cpp:1237
                                    const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpNewOwner =
                                        GetOnlinePlayerInfoByActiveRaceCarIndex(mpGuiCache,
                                                                                lpHudMessageEvent->meNewOwner);
                                    CGS_ASSERT(lpNewOwner != NULL,
                                        "mpGuiCache->GetOnlinePlayerInfo( lpHudMessageEvent->meNewOwner ) != NULL"); // cpp:1238

                                    const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpFirst =
                                        GetOnlinePlayerInfoByActiveRaceCarIndex(mpGuiCache,
                                                                                lpHudMessageEvent->meNewOwner);
                                    if (lpFirst == NULL)
                                    {
                                        lbValid = false;
                                    }
                                    else
                                    {
                                        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpSecond =
                                            GetOnlinePlayerInfoByActiveRaceCarIndex(mpGuiCache,
                                                                                    lpHudMessageEvent->mePreviousOwner);
                                        if (lpSecond == NULL)
                                        {
                                            lbValid = false;
                                        }
                                        else
                                        {
                                            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0,
                                                              lpFirst->mPlayerName.macName);
                                            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0,
                                                              lpSecond->mPlayerName.macName);
                                        }
                                    }
                                    break;
                                }
                                case GuiNewBurnoutHudMessageEvent::E_BURNOUT_SKILLZ_MESSAGE_TYPE_X_BEAT_YOUR:
                                {
                                    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache != NULL");   // cpp:1264
                                    const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpOwner =
                                        GetOnlinePlayerInfoByActiveRaceCarIndex(mpGuiCache,
                                                                                lpHudMessageEvent->meNewOwner);
                                    if (lpOwner == NULL)
                                        lbValid = false;
                                    else
                                        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1,
                                                          lpOwner->mPlayerName.macName);
                                    break;
                                }
                                case GuiNewBurnoutHudMessageEvent::E_BURNOUT_SKILLZ_MESSAGE_TYPE_X_GOT:
                                {
                                    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache != NULL");   // cpp:1281
                                    const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpOwner =
                                        GetOnlinePlayerInfoByActiveRaceCarIndex(mpGuiCache,
                                                                                lpHudMessageEvent->meNewOwner);
                                    if (lpOwner == NULL)
                                        lbValid = false;
                                    else
                                        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1,
                                                          lpOwner->mPlayerName.macName);
                                    break;
                                }
                                case GuiNewBurnoutHudMessageEvent::E_BURNOUT_SKILLZ_MESSAGE_TYPE_YOU_GOT:
                                    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache != NULL");   // cpp:1298
                                    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 1,
                                                      "PLAYER_NAME_STRING_ID");
                                    break;
                                default:
                                    // X360 streams "Invalid state : " + meMessageType (folded); the
                                    // release flow continues (lbValid untouched).
                                    CGS_ASSERT(false, "Invalid state : ");   // cpp:1305
                                    break;
                            }

                            if (lbValid)
                            {
                                if (static_cast<s32>(lpHudMessageEvent->meSkill) == 12
                                    || static_cast<s32>(lpHudMessageEvent->meSkill) == 13)
                                {
                                    CGS_ASSERT(lpHudMessageEvent->mRoadID != 0,
                                               "lpHudMessageEvent->mRoadID != kCGSID_NULL");   // cpp:1318
                                    char lacRoadNumber[12];
                                    CgsCore::SPrintf(lacRoadNumber, 12, "%llu", lpHudMessageEvent->mRoadID);
                                    const s32 liRoadSlot =
                                        (lpHudMessageEvent->meMessageType != 0) ? 2 : 0;
                                    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, liRoadSlot,
                                                      lacRoadNumber);
                                }
                                TriggerMessage(&lMessage);
                            }
                        }
                    }
                    break;
                }

                case 548:   // wrecked-state machine (marks a wreck event this frame)
                    lbHadWreckEvent = true;
                    HandleWreckedEvent(reinterpret_cast<const GuiEventPlayerWrecked*>(lpEvent));
                    break;
                case 549:
                    TriggerMessage("JumpFailed");
                    break;
                case 550:   // BYTE payload on the X360 (lbz event+0 @0x82526F00)
                    if (*reinterpret_cast<const bool*>(lpEvent))
                        TriggerMessage("TimeUpPos");
                    else
                        TriggerMessage("TimeUpNeg");
                    break;
                case 551:
                    TriggerMessage("CantPaintCar");
                    break;
                case 552:
                    TriggerMessage("NeedToFixCar");
                    break;
                case 576:   // park the challenge-triggered flag (clears any pending challenge-end)
                    mbChallengeTriggered = true;
                    mbChallengeEnded = false;
                    break;
                case 578:
                    HandleChallengeEnded(reinterpret_cast<const GuiChallengeEndEvent*>(lpEvent));
                    break;
                case 596:   // developer-challenges-completed bit set (handler = its own ledger row)
                    HandleDeveloperChallengeMessageDEBUG(
                        reinterpret_cast<const GuiDeveloperChallengesCompleted*>(lpEvent));
                    break;

                default:
                    break;
            }

            liEventType = lpGuiEventQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize);
        }
        while (lpEvent != NULL);
    }

    // ---- deferred-message timer passes (run once per frame after the drain) ----

    // Parked online-winner "EventRvlWins".
    if (mbOnlineWinnerWaiting)
    {
        if (mpGuiCache->mbGameplayHudActive)
        {
            const s32 liGameFlowState = mpGuiCache->miGameFlowState;
            if (liGameFlowState == 1 || liGameFlowState == 3)
                TriggerEventFinisher();
        }
    }

    // Crash-boundary message (fires the parked takedown / dirty-trick / delayed revenge).
    if (mbCrashBoundaryMessagePending)
    {
        CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");   // cpp:1471
        if (mpGuiCache->mbGameplayHudActive)
        {
            HandleCrashedEvent(
                static_cast<GuiPlayerCrashingStateChangeEvent::CrashBarState>(meCrashEntryState));
            mbCrashBoundaryMessagePending = false;
        }
    }

    // Reset the wreck-duration state machine when no wreck event arrived this frame.
    if (!lbHadWreckEvent)
    {
        mfCurrentWreckDuration = 0.0f;
        mePlayersCurrentlyWreckState = E_WRECKED_STATE_NOT_WRECKED;
    }

    // Parked challenge-triggered message.
    if (mbChallengeTriggered)
    {
        if (mpGuiCache->mbGameplayHudActive)
        {
            const s32 liGameFlowState = mpGuiCache->miGameFlowState;
            if (liGameFlowState == 1 || liGameFlowState == 3)
                TriggerChallengeTriggeredMessage();
        }
    }

    // Parked challenge-ended message.
    if (mbChallengeEnded)
    {
        if (mpGuiCache->mbGameplayHudActive)
        {
            const s32 liGameFlowState = mpGuiCache->miGameFlowState;
            if (liGameFlowState == 1 || liGameFlowState == 3)
                TriggerChallengeEndedMessage();
        }
    }

    // Road-rules new-high-score message delay.
    if (mbNewRoadRulesHighScores)
    {
        const s32 liGameModeType = mpGuiCache->GetGameMode();
        if (!(liGameModeType == 2 || liGameModeType == 16))
        {
            if (mpGuiCache->mbGameplayHudActive)
            {
                if (!mpGuiCache->IsRaceCarCrashing(
                        static_cast<EActiveRaceCarIndex>(mpGuiCache->GetPlayerActiveRaceCarIndex())))
                {
                    const f32 lfTimeout = mpGuiCache->GetTimeStep() + mfRoadRulesMessageTimeout;
                    mfRoadRulesMessageTimeout = lfTimeout;
                    if (lfTimeout > KF_ROADRULEMESSAGEDELAY)
                    {
                        TriggerNewRoadRulesHighScoreMessage();
                        mfRoadRulesMessageTimeout = 0.0f;
                        mbNewRoadRulesHighScores = false;
                    }
                }
            }
        }
    }

    // Developer-challenge message (X360 debug build; reads 0x4F9, clears 0x4F9 + the
    // 0x500 bit set after firing -- @0x825277A8..0x82527814).
    if (mbDEBUGDeveloperChallengeComplete)
    {
        const s32 liGameModeType = mpGuiCache->GetGameMode();
        if (!(liGameModeType == 2 || liGameModeType == 16))
        {
            if (mpGuiCache->mbGameplayHudActive)
            {
                const s32 liGameFlowState = mpGuiCache->miGameFlowState;
                if (liGameFlowState == 1 || liGameFlowState == 3)
                {
                    TriggerDeveloperChallengeMessageDEBUG();
                    mbDEBUGDeveloperChallengeComplete = false;   // stb 0 -> 0x4F9
                    mCompletedDeveloperChallenges.UnSetAll();    // std 0 -> 0x500 (one u64 field)
                }
            }
        }
    }

    // Road-rage target-achieved message delay ("RRTargGot").
    if (mbRoadRageTargetReached)
    {
        const f32 lfDelay = mpGuiCache->GetTimeStep() + mfRoadRageAchievedMessageDelayTime;
        mfRoadRageAchievedMessageDelayTime = lfDelay;
        if (lfDelay > KF_MIN_ROADRAGE_TARGET_ACHIEVED_DELAY)
        {
            if (meCrashEntryState == 1 || meCrashEntryState == 3)
            {
                GuiHudMessage lMessage;
                lMessage.Construct("RRTargGot");
                TriggerMessage(&lMessage);
                mfRoadRageAchievedMessageDelayTime = 0.0f;
                mbRoadRageTargetReached = false;
            }
        }
    }

    // Trophy-car-unlocked message.
    if (mbTrophyCarUnlockPending)
    {
        if (mpGuiCache->mbGameplayHudActive)
        {
            const s32 liGameFlowState = mpGuiCache->miGameFlowState;
            if (liGameFlowState == 1 || liGameFlowState == 3)
            {
                HandleTrophyCarUnlocked(&mTrophyCarUnlockedEvent);
                mbTrophyCarUnlockPending = false;
                mTrophyCarUnlockedEvent.meUnlockType = 0;
                mTrophyCarUnlockedEvent.mTrophyCarID = 0;
            }
        }
    }

    // Game-completed ("100%") countdown -> "GCompOnline".
    if (mfTimeUntilGameCompletedHudMessage > 0.0f)
    {
        if (mbGameCompletedHudMessagePending)
        {
            const s32 liGameFlowState = mpGuiCache->miGameFlowState;
            if (liGameFlowState == 1 || liGameFlowState == 3)
            {
                const f32 lfRemaining = mfTimeUntilGameCompletedHudMessage - mpGuiCache->GetTimeStep();
                mfTimeUntilGameCompletedHudMessage = lfRemaining;
                if (lfRemaining <= 0.0f)
                {
                    GuiHudMessage lMessage;
                    lMessage.Construct("GCompOnline");
                    TriggerMessage(&lMessage);

                    BrnProgression::Profile* lpProfile = mpProfile;
                    mfTimeUntilGameCompletedHudMessage = 0.0f;
                    mbGameCompletedHudMessagePending = false;
                    if (lpProfile != NULL)
                        lpProfile->SetOneHundredHudMessageViewed(true);
                }
            }
        }
    }

    // Parked player-left-lobby message.
    if (mbOnlinePlayerLeft)
    {
        if (mpGuiCache->mbGameplayHudActive)
        {
            const s32 liGameFlowState = mpGuiCache->miGameFlowState;
            if (liGameFlowState == 1 || liGameFlowState == 3)
                TriggerPlayerLeftLobby();
        }
    }

    mpViewOutputQueue = NULL;
}

} // namespace BrnGui

// ============================================================================
// FOLDED FROM BrnGuiHudMessageAnalyzer_wRR.cpp (wave RR) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

// ============================================================================
// GameSource/Gui/BrnGuiHudMessageAnalyzer_wRR.cpp
//
// BrnGui::HudMessageAnalyzer -- the two ROAD RAGE event handlers. Reconstructed from
// BURNOUT_X360_ARTIST.XEX (road-rage wave, 2026-09-02).
//
//   HandleRoadRageTargetReached      DWARF BrnGuiHudMessageAnalyzer.h:478 / .cpp:1817
//   HandleRoadRagePlayerDamageEvent  DWARF BrnGuiHudMessageAnalyzer.h:296 / .cpp:2990
//
// ⚠ NEITHER HAS AN X360 SYMBOL. The ARTIST image carries no standalone
//   HandleRoadRage* function (zero rows in progress/identity.json / status.json); the
//   compiler folded both into HudMessageAnalyzer::Update @0x82525FC0's dispatch switch:
//
//     case 168 (GuiEventPlayerReachedRoadRageTarget), first jump table (id 64 + 104):
//         this->field_3DA = 1;                       -- mbRoadRageTargetReached = true
//     case 348 (GuiEventRoadRagePlayerDamage), second jump table (id 313 + 35):
//         if ( *(v10 + 4) ) this->field_3D9 = 1;     -- if (mbOneMoreCrashToTotalled)
//                                                       mbRoadOneMoreCrashToTotalledPending = true
//
//   BrnGuiHudMessageAnalyzer_wB_12.cpp reproduces both arms INLINE at :114 / :262, exactly
//   as the console has them, and is deliberately left untouched by this partfile. The
//   bodies below are the DWARF-attested member functions the console inlined -- the
//   inlining-reversal form of those two arms -- and are behaviourally identical to them.
//   They are NOT called from the switch today; wiring them in place of the inline
//   statements is a pure refactor the conductor may opt into (two one-line edits in
//   _wB_12.cpp), and would make this partfile a link requirement of that TU.
//
// The two pending flags are CONSUMED elsewhere, and both consumers already exist:
//   * mbRoadRageTargetReached -> Update's per-frame tail (_wB_12.cpp:714): after
//     KF_MIN_ROADRAGE_TARGET_ACHIEVED_DELAY, and only in crash-entry state 1/3,
//     TriggerMessage("RRTargGot").
//   * mbRoadOneMoreCrashToTotalledPending -> HandleCrashBarStateChange (_wB_02.cpp:52):
//     on E_CRASHBARSTATE_LEAVE_CRASHED / _LEAVE_TAKEDOWN, TriggerMessage("RRDamCrit").
//
// ---------------------------------------------------------------------------
// EVENT 427 (GuiEventRoadRageTimeExtended) -- re-verified against the image 2026-09-03
// (lane H). Every claim below is from the asm, not the pseudocode.
//
// (a) HudMessageAnalyzer::Update @0x82525FC0 has NO 427 arm. Its third dispatch table
//     is `addi r11, r3, -0x1A3 ; cmplwi r11, 0xB1` @0x82526BD0 (base id 419, 178
//     cases). 427 is case 8, and case 8 is in jpt_82526BF0's DEFAULT list
//     (cases 3,5,7-9,13,15-19,22-25,...). The 43 populated cases are 0,1,2,4,6,10,11,
//     12,14,20,21,26-36,63-72,119,121,123,129-133,157,159,177 -- i.e. ids 419..596.
// (b) mbTimeExtMsgPending (X360 +0x3D8) has a CONSUMER but no SETTER:
//       reader  HandleCrashedEvent @0x8251C7C0 (0x8251CA28 `lbz 0x3D8`): on crash-bar
//               states 1/3 (LEAVE_CRASHED / LEAVE_TAKEDOWN) it clears the flag and
//               triggers GuiHudMessage "EventRRTime" (_wB_02.cpp:46 has this arm).
//       writers Construct @0x82509110 (0), Update case 322 @0x82526AC8 (0, with 0x3D9),
//               HandleCrashedEvent @0x8251CA38 (0). All three store ZERO.
//     The analyzer is embedded at GuiModule+0xA1600 (GuiModule::Construct
//     @0x8251872C `addis r3, r31, 0xA ; addi r3, r3, 0x1600`); a scan of every
//     GuiModule-namespace export for a store to +0xA19D8 found none. So on the retail
//     X360 the "EventRRTime" message is dead code, and the DWARF-only
//     HandleRoadRageTimeExtension (h:388) has no body in the image.
// (c) THE REAL CONSOLE CONSUMER OF 427 IS THE ABOVE-CAR RENDERER, not this analyzer:
//       CustomRendererManager routes 427 (and 377) to AboveCarRenderer (the tree's
//       BrnCustomRenderer.cpp:422-432 already does).
//       AboveCarRenderer::RecvEvent @0x824544D8:
//         id 427 @0x8245499C..0x824549B0: `stb 1, 0x6B2(this)` mbTimeExtensionPending
//                                          `lwz r11, 0(ev) ; sth r11, 0x6B0(this)`
//                                          miTimeExtension (s16 <- the 4-byte seconds)
//         id 377 @0x824548FC..0x82454988: if (payload word == 3 /*LEAVE_TAKEDOWN*/ &&
//                 mbTimeExtensionPending) { if (!maBankingScores.IsFull()) Append(
//                 BankingScore{ mv2ScreenSpacePosition = (0.0f, flt_82F257D0 = 400.0f),
//                 world pos = 0, miBaseScore = miTimeExtension, miComboBonus = 0,
//                 mbIsRoadRageTimeExtension = true }); } mbTimeExtensionPending = false
//                 (the clear @0x82454988 runs even when the array is full).
//       RenderBankingScores @0x8245BF28 draws that entry through
//       LanguageManager::FindString("TIME_EXTENSION_SECONDS") @0x8245C5E4 +
//       FormatCurrencyString -- the floating "+N s" the player sees after the takedown
//       camera drops. Construct @0x82454478 `stb r30(=0), 0x6B2` clears the BOOL
//       (the tree's BrnAboveCarRenderer.cpp:107 attributes that store to the s16
//       miTimeExtension -- swapped; see the lane-H report). None of this is live in
//       this build: AboveCarRenderer.cpp is unmounted (RenderComponent /
//       RenderBankingScores declaration-only, RecvEvent undeclared) and
//       mapCustomRenderComponents[E_ABOVECAR] is 0 (BrnCustomRenderer.cpp:199).
// ============================================================================

namespace BrnGui
{

// DWARF .h:478 -- the road-rage target has just been met; arm the delayed "RRTargGot"
// message. The event carries no payload the console reads (`lpEvent` is unused on the
// X360 as well: case 168 is a bare `stb 1, 0x3DA`).
void HudMessageAnalyzer::HandleRoadRageTargetReached(const GuiEventPlayerReachedRoadRageTarget* /*lpEvent*/)
{
    mbRoadRageTargetReached = true;
}

// DWARF .h:296 -- a road-rage damage report. Only the "one more crash totals the car"
// flag is consulted: DWARF GuiEventRoadRagePlayerDamage (BrnGuiEventTypeDefs.h:2546-2548)
// is { f32 mfHowCloseToTotalled (+0); bool mbOneMoreCrashToTotalled (+4);
// bool mbPlayerTotalled (+5) }, and the console tests byte +4 (`lbz 4(r?)` in case 348).
// The tree's home for the record is still the raw-byte placeholder, so the byte is read
// through maData[4] exactly as _wB_12.cpp:263 does; upgrading the placeholder to the
// named DWARF shape belongs to whoever homes the producer.
void HudMessageAnalyzer::HandleRoadRagePlayerDamageEvent(const GuiEventRoadRagePlayerDamage* lpEvent)
{
    if (lpEvent->maData[4])                      // mbOneMoreCrashToTotalled
    {
        mbRoadOneMoreCrashToTotalledPending = true;
    }
}

}

// ============================================================================
// FOLDED FROM BrnGuiHudMessageAnalyzer_wB_res.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

// BrnGui::HudMessageAnalyzer -- the TU's data spine (wave-B keystone partfile).
// Reconstructed from BURNOUT_X360_ARTIST.XEX; every value below is dumped from the
// X360 image (scratchpad/waveB/hudmsg_rodata_dump.txt), NOT guessed.
//
//   * message-id string tables  -- rodata pointer tables @0x8206F59C..0x82F27824
//   * KF_REQUIRED_WRECK_DURATION -- flt_8206F8E0 == 0.6f
//   * KA_SKILLZ_MESSAGE_IDS      -- .data qword_82FB56D8 (4x14 CgsIDs), zero in the
//     image and filled by the TU's dynamic initialiser @0x82C56198 as
//     CgsIDCompress("Sklz_<row>_<skill>"); reproduced here as a dynamically
//     initialised static (the compiler emits the same compress-and-store init).
//
// The original TU kept the KAPC_* tables at file scope; the reconstructed TU is
// split across partfiles, so they are extern (declared in the class home header).

namespace BrnGui
{

// @0x8206F59C -- takedown-type message ids, indexed by BrnGameState::ETakedownType.
// Slots 6..8 are genuinely empty strings in the X360 image (the pointer parks on the
// shared "" rodata byte); slot 5 aliases slot 0's "TDGdGeneral".
const char* const KAPC_TAKEDOWN_TYPES[13] =
{
    "TDGdGeneral",   // [0]
    "TDGdGrind",     // [1]
    "TDGdTBone",     // [2]
    "TDGdVert",      // [3]
    "TDGdTraffCh",   // [4]
    "TDGdGeneral",   // [5]
    "",              // [6]
    "",              // [7]
    "",              // [8]
    "TDGdRevenge",   // [9]
    "TDGdCar",       // [10]
    "TDGdVan",       // [11]
    "TDGdBus",       // [12]
};

// @0x82F278BC -- chained-takedown message ids ("row" == chain length; index chain-2).
// ConstructTakedownMessage clamps chain overflow (chain-2 >= 9) to the last row.
const char* const KAPC_TAKEDOWN_CHAIN[9] =
{
    "TDGdRow2", "TDGdRow3", "TDGdRow4", "TDGdRow5", "TDGdRow6",
    "TDGdRow7", "TDGdRow8", "TDGdRow9", "TDGdRow10",
};

// @0x8206F618 / @0x8206F624 -- dirty-trick ("payback") takedown lines, indexed by
// BrnNetwork::EPaybackType (X360: 0 slash / 1 lock / 2 takeover; 3 == none). The X360
// tables hold THREE entries (the PS3-DWARF [4] included the six-axis slot).
const char* const KAPC_PAYBACK_TAKEDOWN_BD_MESSAGES[3] =
{
    "PbkBdSlashDn", "PbkBdLockDn", "PbkBdTOvrDn",
};
const char* const KAPC_PAYBACK_TAKEDOWN_GD_MESSAGES[3] =
{
    "PbkGdSlashDn", "PbkGdLockDn", "PbkGdTOvrDn",
};

// @0x82F277E0 -- finish-position string ids. HandleEliminatedEvent indexes from the
// SECOND slot (message = &KAPC_FINISH_POSITION_MESSAGES[1][position]); the four
// lowercase variants trail the eight uppercase slots in the X360 image.
const char* const KAPC_FINISH_POSITION_MESSAGES[12] =
{
    "POSITION_FIRST",   "POSITION_SECOND",  "POSITION_THIRD",   "POSITION_FOURTH",
    "POSITION_FIFTH",   "POSITION_SIXTH",   "POSITION_SEVENTH", "POSITION_EIGHTH",
    "POSITION_FIRST_LOWERCASE", "POSITION_SECOND_LOWERCASE",
    "POSITION_THIRD_LOWERCASE", "POSITION_FOURTH_LOWERCASE",
};

// @0x8206F684 -- "all collectibles of this family done" string ids, indexed by
// BrnGui::StuntType.
const char* const KAPC_COLLECTABLE_COMPLETION_STRINGID[3] =
{
    "HUDMESSAGE_JUMPS_COMPLETE",
    "HUDMESSAGE_SMASHES_COMPLETE",
    "HUDMESSAGE_STUNTS_COMPLETE",
};

// -----------------------------------------------------------------------------------------
// [gateui r3] The THREE drive-through message tables HandleDriveThrough @0x8251D570 selects
// between, all indexed by GuiDriveThroughEvent::DriveThroughType (0..5). Names verbatim from
// the DWARF (BrnGuiHudMessageAnalyzer.cpp:95 / :105 / :115) -- including the original's
// KPAC_ transposition in the third one, which is reproduced, not "corrected".
// Values dumped from the X360 image (scratchpad/waveB/hudmsg_rodata_dump.txt, the 18-entry
// run at 0x8206F5D0 that IDA labels as one block: 0x8206F5D0 / 0x8206F5E8 / 0x8206F600).
//
// The NULL slots are REAL and load-bearing, not gaps in the dump: the console reaches a table
// slot only on the paths its own branch structure allows, so the empty ones are unreachable
// -- MAGIC is only ever indexed for types 0/2 (the only two with a "2" variant), and
// INEFFECTIVE is only ever indexed when mbEffective is false. Reproduced as NULL, and
// HandleDriveThrough deliberately does NOT guard the Construct against a null id (that guard
// would be a behaviour the binary does not have).
// -----------------------------------------------------------------------------------------

// @0x8206F5D0 -- the normal ("basic") line for each drive-through type.
const char* const KAPC_DRIVE_THROUGH_MESSAGES[6] =
{
    "DriThrCarWsh",   // [0] E_DRIVE_THROUGH_TYPE_CAR_WASH
    "DriThrBdyShp",   // [1] E_DRIVE_THROUGH_TYPE_BODY_SHOP
    "DriThrPntShp",   // [2] E_DRIVE_THROUGH_TYPE_PAINT_SHOP
    "DriThrGasStn",   // [3] E_DRIVE_THROUGH_TYPE_GAS_STATION
    "DriThrAPts",     // [4] E_DRIVE_THROUGH_TYPE_AUTO_PARTS
    "DriThrClosed",   // [5] E_DRIVE_THROUGH_TYPE_FAILED
};

// @0x8206F5E8 -- the rare "magic" variant, drawn 1-in-
// KU_FREQUENCY_OF_DRIVETHROUGH_MAGIC_MESSAGES for the two effective types that have one.
const char* const KAPC_DRIVE_THROUGH_MAGIC_MESSAGES[6] =
{
    "DriThrCarWs2",   // [0] E_DRIVE_THROUGH_TYPE_CAR_WASH
    NULL,             // [1] (never indexed: only types 0 and 2 take the random branch)
    "DriThrPntSh2",   // [2] E_DRIVE_THROUGH_TYPE_PAINT_SHOP
    NULL,             // [3]
    NULL,             // [4]
    NULL,             // [5]
};

// @0x8206F600 -- the "you drove through but it did nothing" line (mbEffective == false).
// DWARF spelling KPAC_ (sic) preserved.
const char* const KPAC_DRIVE_THROUGH_INEFFECTIVE_MESSAGES[6] =
{
    NULL,             // [0] (car wash has no ineffective line)
    "DriThrBdyShX",   // [1] E_DRIVE_THROUGH_TYPE_BODY_SHOP
    "DriThrPntShX",   // [2] E_DRIVE_THROUGH_TYPE_PAINT_SHOP
    NULL,             // [3]
    NULL,             // [4]
    "DriThrClosed",   // [5] E_DRIVE_THROUGH_TYPE_FAILED (aliases the basic table's slot 5)
};

// @0x82F27704 -- county string ids (BrnWorld::ECounty order), the second "StntAllDone"
// parameter. FLAG: consumer-named (see the header declaration).
const char* const KAPC_COUNTY_STRINGID[5] =
{
    "BRH_PBH",   // [0]
    "BRH_SL",    // [1]
    "BRH_HT",    // [2]
    "BRH_WM",    // [3]
    "BRH_DTP",   // [4]
};

// flt_8206F8E0 -- how long the wreck state must persist before the wrecked message
// fires (HandleWreckedEvent accumulates the frame delta against it).
const f32 HudMessageAnalyzer::KF_REQUIRED_WRECK_DURATION = 0.6f;

// [gateui] .data qword_82FB5898 (the three qwords immediately after the 4x14
// KA_SKILLZ_MESSAGE_IDS block, which ends at 0x82FB5890) -- the collectible-tally message
// names HandleStuntInfo @0x8251F650 indexes by BrnGui::StuntType (`slwi r8,r8,3; ldx`).
// Zero in the image and filled by the TU's OWN second dynamic initialiser @0x82C56040
// (a distinct thunk from the skillz one @0x82C56198), which the pseudocode gives
// verbatim:
//     CgsIDCompress("StuntPartJmp") -> qword_82FB5898[0]
//     CgsIDCompress("StuntPartSma") -> qword_82FB58A0
//     CgsIDCompress("StuntFull")    -> qword_82FB58A8
// Reproduced here as a dynamically initialised static, exactly as KA_SKILLZ_MESSAGE_IDS
// is (the compiler emits the same compress-and-store init).
//
// (This closes the scout map's "PARK: qword_82FB5898 needs headless IDA" item -- the
// values were never in rodata to dump; they are constructed by a dyn-init thunk that IS
// in the JSON export set. Same class of recovery as the k*Def* CRT-init dumps: walk the
// initialiser, do not read the zeroed .data.)
const CgsID HudMessageAnalyzer::KA_STUNT_INFO_MESSAGES[3] =
{
    CgsIDCompress("StuntPartJmp"),   // [0] E_STUNTTYPE_JUMP
    CgsIDCompress("StuntPartSma"),   // [1] E_STUNTTYPE_SMASH
    CgsIDCompress("StuntFull"),      // [2] E_STUNTTYPE_STUNT
};

// .data qword_82FB56D8, filled by the TU's dynamic initialiser @0x82C56198: one
// compressed "Sklz_<messageType>_<skill>" id per (EBurnoutSkillzMessageTypes row,
// BurnoutSkillzData skill column). Columns 9, 12 and 13 are explicitly zeroed by the
// initialiser (no message for those skill slots); Update skips zero entries.
//   rows:    AbB == X_BEAT_YS, AbY == X_BEAT_YOUR, Ag == X_GOT, Yg == YOU_GOT
//   columns: InA Bar BCh Dri Spn InD NrM Onc PPk <0> RRT RRC <0> <0>
const CgsID HudMessageAnalyzer::KA_SKILLZ_MESSAGE_IDS[4][14] =
{
    {
        CgsIDCompress("Sklz_AbB_InA"), CgsIDCompress("Sklz_AbB_Bar"),
        CgsIDCompress("Sklz_AbB_BCh"), CgsIDCompress("Sklz_AbB_Dri"),
        CgsIDCompress("Sklz_AbB_Spn"), CgsIDCompress("Sklz_AbB_InD"),
        CgsIDCompress("Sklz_AbB_NrM"), CgsIDCompress("Sklz_AbB_Onc"),
        CgsIDCompress("Sklz_AbB_PPk"), 0,
        CgsIDCompress("Sklz_AbB_RRT"), CgsIDCompress("Sklz_AbB_RRC"),
        0, 0,
    },
    {
        CgsIDCompress("Sklz_AbY_InA"), CgsIDCompress("Sklz_AbY_Bar"),
        CgsIDCompress("Sklz_AbY_BCh"), CgsIDCompress("Sklz_AbY_Dri"),
        CgsIDCompress("Sklz_AbY_Spn"), CgsIDCompress("Sklz_AbY_InD"),
        CgsIDCompress("Sklz_AbY_NrM"), CgsIDCompress("Sklz_AbY_Onc"),
        CgsIDCompress("Sklz_AbY_PPk"), 0,
        CgsIDCompress("Sklz_AbY_RRT"), CgsIDCompress("Sklz_AbY_RRC"),
        0, 0,
    },
    {
        CgsIDCompress("Sklz_Ag_InA"),  CgsIDCompress("Sklz_Ag_Bar"),
        CgsIDCompress("Sklz_Ag_BCh"),  CgsIDCompress("Sklz_Ag_Dri"),
        CgsIDCompress("Sklz_Ag_Spn"),  CgsIDCompress("Sklz_Ag_InD"),
        CgsIDCompress("Sklz_Ag_NrM"),  CgsIDCompress("Sklz_Ag_Onc"),
        CgsIDCompress("Sklz_Ag_PPk"),  0,
        CgsIDCompress("Sklz_Ag_RRT"),  CgsIDCompress("Sklz_Ag_RRC"),
        0, 0,
    },
    {
        CgsIDCompress("Sklz_Yg_InA"),  CgsIDCompress("Sklz_Yg_Bar"),
        CgsIDCompress("Sklz_Yg_BCh"),  CgsIDCompress("Sklz_Yg_Dri"),
        CgsIDCompress("Sklz_Yg_Spn"),  CgsIDCompress("Sklz_Yg_InD"),
        CgsIDCompress("Sklz_Yg_NrM"),  CgsIDCompress("Sklz_Yg_Onc"),
        CgsIDCompress("Sklz_Yg_PPk"),  0,
        CgsIDCompress("Sklz_Yg_RRT"),  CgsIDCompress("Sklz_Yg_RRC"),
        0, 0,
    },
};

} // namespace BrnGui

// ============================================================================
// FOLDED FROM BrnGuiHudMessageAnalyzer_wC_00.cpp (wave C) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

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

// ============================================================================
// FOLDED FROM BrnGuiHudMessageAnalyzer_wC_01.cpp (wave C) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

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

// ============================================================================
// FOLDED FROM BrnGuiHudMessageAnalyzer_wC_02.cpp (wave C) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

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

// ============================================================================
// FOLDED FROM BrnGuiHudMessageAnalyzer_wC_03.cpp (wave C) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

// BrnGui::HudMessageAnalyzer -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (wave-C group wC_04, 3 ledger functions):
//   HudMessageAnalyzer::HandleTraitorousTakedown @0x8251B0E0
//   HudMessageAnalyzer::HandleWreckedEvent       @0x8251CA88
//   HudMessageAnalyzer::HandleStuntPerformed     @0x8251B608

namespace BrnGui
{

// @ 0x8251B0E0
// Online "traitorous takedown" (a team-mate slammed you): resolve the aggressor's
// online name and fire the "OnlRRTraitor" message with it -- but only when the name
// resolved (an invalid name adds an empty param and the message is suppressed).
void HudMessageAnalyzer::HandleTraitorousTakedown(const GuiTraitorousTakedownEvent* lpEvent) const
{
    CGS_ASSERT(lpEvent != NULL, "lpEvent");
    CGS_ASSERT(lpEvent->meAggActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               "lpEvent->meAggActiveRaceCarIndex >= 0");
    CGS_ASSERT(lpEvent->meAggActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "lpEvent->meAggActiveRaceCarIndex < BrnWorld::KI_MAX_ACTIVE_RACE_CARS");

    bool lbNameValid = true;

    GuiHudMessage lMessage;
    lMessage.Construct("OnlRRTraitor");

    const char* lpcOnlineName = GetOnlineName(lpEvent->meAggActiveRaceCarIndex, &lbNameValid);
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0, lpcOnlineName);

    if (lbNameValid)
    {
        TriggerMessage(&lMessage);
    }
}

// @ 0x8251CA88
// Wreck-duration state machine (E_WRECKED_STATE_*). The player-wrecked event carries
// no consumed field (the pointer is asserted then unused); the whole decision keys off
// the analyzer's own wreck-state member, the frame delta on the cache and -- on entry --
// the stunt-attack score:
//   NOT_WRECKED : stunt-attack mode with a positive last-stunt score arms the
//                 "StuntWrecked" lane, otherwise the ordinary "GameWrecked" lane;
//   WRECKING    : accumulate the wreck duration; hold until it passes the threshold;
//   WRECKED / WRECKED_STUNT : fire the message once the crash bar has dropped
//                 (meCrashEntryState == 0), then latch WRECK_HANDLED.
void HudMessageAnalyzer::HandleWreckedEvent(const GuiEventPlayerWrecked* lpEvent)
{
    CGS_ASSERT(lpEvent != NULL, "lpWreckedEvent");

    switch (mePlayersCurrentlyWreckState)
    {
        case E_WRECKED_STATE_NOT_WRECKED:
            if (mpGuiCache->GetGameMode() == BrnGameState::GameStateModuleIO::E_MODE_STUNT_ATTACK
                && mpGuiCache->miLastStuntScore > 0)
            {
                mePlayersCurrentlyWreckState = E_WRECKED_STATE_WRECKED_STUNT;
            }
            else
            {
                mePlayersCurrentlyWreckState = E_WRECKED_STATE_WRECKING;
            }
            return;

        case E_WRECKED_STATE_WRECKING:
            CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");
            // @0x8251CB6C lfs f13, 0(mpGuiCache) -- the cache's LEADING frame-delta float
            // (GuiCache::mfTimeStep @ +0x0000, the GetTimeStep() slot); same inlined load
            // as Update's three timer sites (@0x8252777C/0x8252782C/0x82527900), which the
            // committed wB_12 reads via GetTimeStep() too.
            mfCurrentWreckDuration += mpGuiCache->GetTimeStep();
            // @0x8251CB80 fcmpu + ble: the hold path is taken when the duration is
            // <= the threshold OR unordered, so the negated ordered predicate is the
            // faithful form (plain `<=` would diverge on NaN).
            if (!(mfCurrentWreckDuration > KF_REQUIRED_WRECK_DURATION))
            {
                return;
            }
            mePlayersCurrentlyWreckState = E_WRECKED_STATE_WRECKED;
            return;

        case E_WRECKED_STATE_WRECKED:
            if (meCrashEntryState != 0)
            {
                return;
            }
            {
                GuiHudMessage lMessage;
                lMessage.Construct("GameWrecked");
                TriggerMessage(&lMessage);
            }
            mePlayersCurrentlyWreckState = E_WRECKED_STATE_WRECK_HANDLED;
            return;

        case E_WRECKED_STATE_WRECKED_STUNT:
            if (meCrashEntryState != 0)
            {
                return;
            }
            {
                GuiHudMessage lMessage;
                lMessage.Construct("StuntWrecked");
                TriggerMessage(&lMessage);
            }
            mePlayersCurrentlyWreckState = E_WRECKED_STATE_WRECK_HANDLED;
            return;

        default:
            return;
    }
}

// @ 0x8251B608
// Skill/stunt chatter built from the stunt snapshot's flag masks. Two lanes:
//   * a failure lane (muStuntTypes bits 0x40000/0x80000 -> repetition / crashed;
//     either 0xC0000 bit set) fires a two-string "GenericBad" message;
//   * the scoring lane (positive multiplier) assembles a "StuntMulti" line by
//     concatenating one localised fragment per active awesome-stunt category
//     (air/spin/barrel-roll/leap/super-jump/smash/billboard/burnout/takedown/chain),
//     picking the singular key when the category count is <= 1 and the
//     FormatTextFromInt-rendered *_MULTIPLE key otherwise, then fires the composed
//     string plus the combined score as an int param.
// The 0x4000 "reverse" bit prefixes a "REVERSE" fragment onto the spin/roll/leap
// categories (and swaps AIR for REVERSE_AIR).
void HudMessageAnalyzer::HandleStuntPerformed(const GuiHUDMessageStuntPerformed* lpEvent)
{
    CgsLanguage::LanguageManager* lpLanguageManager = mpAccessPointers->mpLanguageManager;

    char lacBuffer[128];
    lacBuffer[0] = 0;
    std::memset(&lacBuffer[1], 0, 127);

    // Resolve a localisation key to its stored text, falling back to the raw key when
    // the database has no entry (FindString returns NULL).
    auto lResolveString = [lpLanguageManager](const char* lpcKey) -> const char*
    {
        const u8* lpcString = lpLanguageManager->FindString(lpcKey);
        return lpcString ? reinterpret_cast<const char*>(lpcString) : lpcKey;
    };

    GuiHudMessage lMessage;

    // ---- failure lane (repetition / crashed / plain fail) ----
    if ((lpEvent->mStuntInfo.muStuntTypes & 0xC0000) != 0)
    {
        lMessage.Construct("GenericBad");

        const char* lpcTitle = lResolveString("STUNT_RUN_MULTIPLIER_STUNT_FAILED");

        const char* lpcReason = lacBuffer;   // empty fragment unless a reason bit is set
        if ((lpEvent->mStuntInfo.muStuntTypes & 0x40000) != 0)
        {
            lpcReason = lResolveString("STUNT_RUN_MULTIPLIER_REPETITION");
        }
        else if ((lpEvent->mStuntInfo.muStuntTypes & 0x80000) != 0)
        {
            lpcReason = lResolveString("STUNT_RUN_MULTIPLIER_CRASHED");
        }

        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0, lpcTitle);
        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1, lpcReason);
        TriggerMessage(&lMessage);
        return;
    }

    // ---- scoring lane ----
    if (lpEvent->mStuntInfo.miStuntMultiplier > 0)
    {
        lMessage.Construct("StuntMulti");

        const bool lbReverse = (lpEvent->mStuntInfo.muStuntTypes & 0x4000) != 0;

        char lacFormatBuffer[64];

        // air (awesome bit 0x4) -- a single fragment, reversed variant swaps the key
        if ((lpEvent->mStuntInfo.muAwesomeStuntTypes & 0x4) != 0)
        {
            CgsCore::StrCat(lacBuffer, 0x80,
                            lResolveString(lbReverse ? "STUNT_RUN_MULTIPLIER_REVERSE_AIR"
                                                     : "STUNT_RUN_MULTIPLIER_AIR"));
        }

        // spin (awesome bit 0x1) -- optional REVERSE prefix + singular/plural count
        if ((lpEvent->mStuntInfo.muAwesomeStuntTypes & 0x1) != 0)
        {
            if (lbReverse)
            {
                CgsCore::StrCat(lacBuffer, 0x80, lResolveString("STUNT_RUN_MULTIPLIER_REVERSE"));
            }

            if (lpEvent->mStuntInfo.muFlatSpins <= 1)
            {
                CgsCore::StrCat(lacBuffer, 0x80, lResolveString("STUNT_RUN_MULTIPLIER_SPIN_SINGLE"));
            }
            else
            {
                lpLanguageManager->FormatTextFromInt(
                    lacFormatBuffer, 63, "STUNT_RUN_MULTIPLIER_SPIN_MULTIPLE",
                    CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
                    lpEvent->mStuntInfo.muFlatSpins,
                    CgsLanguage::LanguageManager::E_FORMAT_INTEGER);
                CgsCore::StrCat(lacBuffer, 0x80, lacFormatBuffer);
            }
        }

        // barrel roll (awesome bit 0x2)
        if ((lpEvent->mStuntInfo.muAwesomeStuntTypes & 0x2) != 0)
        {
            if (lbReverse)
            {
                CgsCore::StrCat(lacBuffer, 0x80, lResolveString("STUNT_RUN_MULTIPLIER_REVERSE"));
            }

            if (lpEvent->mStuntInfo.muBarrelRolls <= 1)
            {
                CgsCore::StrCat(lacBuffer, 0x80, lResolveString("STUNT_RUN_MULTIPLIER_BARREL_ROLL_SINGLE"));
            }
            else
            {
                lpLanguageManager->FormatTextFromInt(
                    lacFormatBuffer, 63, "STUNT_RUN_MULTIPLIER_BARREL_ROLL_MULTIPLE",
                    CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
                    lpEvent->mStuntInfo.muBarrelRolls,
                    CgsLanguage::LanguageManager::E_FORMAT_INTEGER);
                CgsCore::StrCat(lacBuffer, 0x80, lacFormatBuffer);
            }
        }

        // leap car (awesome bit 0x10000)
        if ((lpEvent->mStuntInfo.muAwesomeStuntTypes & 0x10000) != 0)
        {
            if (lbReverse)
            {
                CgsCore::StrCat(lacBuffer, 0x80, lResolveString("STUNT_RUN_MULTIPLIER_REVERSE"));
            }

            if (lpEvent->mu16TailCount16 <= 1)
            {
                CgsCore::StrCat(lacBuffer, 0x80, lResolveString("STUNT_RUN_MULTIPLIER_LEAP_CAR_SINGLE"));
            }
            else
            {
                lpLanguageManager->FormatTextFromInt(
                    lacFormatBuffer, 63, "STUNT_RUN_MULTIPLIER_LEAP_CAR_MULTIPLE",
                    CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
                    lpEvent->mu16TailCount16,
                    CgsLanguage::LanguageManager::E_FORMAT_INTEGER);
                CgsCore::StrCat(lacBuffer, 0x80, lacFormatBuffer);
            }
        }

        // super jump (awesome bit 0x10)
        if ((lpEvent->mStuntInfo.muAwesomeStuntTypes & 0x10) != 0)
        {
            CgsCore::StrCat(lacBuffer, 0x80, lResolveString("STUNT_RUN_MULTIPLIER_SUPER_JUMP"));
        }

        // smash (awesome bit 0x20)
        if ((lpEvent->mStuntInfo.muAwesomeStuntTypes & 0x20) != 0)
        {
            CgsCore::StrCat(lacBuffer, 0x80, lResolveString("STUNT_RUN_MULTIPLIER_SMASH"));
        }

        // billboard (awesome bit 0x40)
        if ((lpEvent->mStuntInfo.muAwesomeStuntTypes & 0x40) != 0)
        {
            CgsCore::StrCat(lacBuffer, 0x80, lResolveString("STUNT_RUN_MULTIPLIER_BILLBOARD"));
        }

        // burnout (awesome bit 0x80)
        if ((lpEvent->mStuntInfo.muAwesomeStuntTypes & 0x80) != 0)
        {
            CgsCore::StrCat(lacBuffer, 0x80, lResolveString("STUNT_RUN_MULTIPLIER_BURNOUT"));
        }

        // takedown (awesome bit 0x8000)
        if ((lpEvent->mStuntInfo.muAwesomeStuntTypes & 0x8000) != 0)
        {
            CgsCore::StrCat(lacBuffer, 0x80, lResolveString("STUNT_RUN_MULTIPLIER_TAKEDOWN"));
        }

        // chain (the tail count) -- always plural-formatted
        if (lpEvent->mu16TailCount14 != 0)
        {
            lpLanguageManager->FormatTextFromInt(
                lacFormatBuffer, 63, "STUNT_RUN_MULTIPLIER_CHAIN",
                CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
                lpEvent->mu16TailCount14,
                CgsLanguage::LanguageManager::E_FORMAT_INTEGER);
            CgsCore::StrCat(lacBuffer, 0x80, lacFormatBuffer);
        }

        CGS_ASSERT(std::strlen(lacBuffer) > 3, "strlen(lacBuffer) > 3");

        const s32 liChainScore = lpEvent->mu16TailCount14 + lpEvent->mStuntInfo.miStuntMultiplier;
        lacBuffer[std::strlen(lacBuffer) - 3] = 0;   // trim the trailing category separator

        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 0, liChainScore);
        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1, lacBuffer);
        TriggerMessage(&lMessage);
    }
}

}

// ============================================================================
// FOLDED FROM BrnGuiHudMessageAnalyzer_wO_00.cpp (wave O) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

namespace BrnGui
{

// @ 0x824F9D48 -- Update's case-596 drain target (X360-only sibling; no PS3-DWARF row).
// Update @0x82525FC0 calls it @0x825275D4 with the raw queue record in r4.
void HudMessageAnalyzer::HandleDeveloperChallengeMessageDEBUG(
        const GuiDeveloperChallengesCompleted* lpDeveloperChallengeEvent)
{
    // Non-gating tripwires. MEASURED: both strings and both line numbers are verbatim
    // X360 assert rodata (@0x824F9D78 cpp:0x1755 == 5973, @0x824F9DC4 cpp:0x1756 == 5974) --
    // that rodata is where the parameter name and the event's member name come from.
    CGS_ASSERT(lpDeveloperChallengeEvent != NULL, "lpDeveloperChallengeEvent");

    // @0x824F9D88..0x824F9DB4 is FastBitArray<15>::IsZero() folded inline: a ldx/cmpldi
    // scan over the bit fields (one, for <15>), with the assert firing when the scan
    // completes without finding a non-zero field (r11 = 1 at 0x824F9DA8 -> fire;
    // the early bne at 0x824F9D98 lands on r11 = 0 -> skip).
    CGS_ASSERT(!lpDeveloperChallengeEvent->mCompletedDeveloperChallenges.IsZero(),
               "!lpDeveloperChallengeEvent->mCompletedDeveloperChallenges.IsZero()");

    // Park the flag and OR-accumulate the event's completed-challenge bits.
    // MEASURED @0x824F9DD4..0x824F9DE8: li r11,1 / stb r11,0x4F9(r30), then
    // ld 0x500(r30), ld 0(r31), or, std 0x500(r30) -- the inlined SetOr over the one
    // u64 field. The console offsets 0x4F9 / 0x500 are COMMENTARY: both members are
    // reached by name, so the host layout stays host-correct.
    mbDEBUGDeveloperChallengeComplete = true;
    mCompletedDeveloperChallenges.SetOr(mCompletedDeveloperChallenges,
                                        lpDeveloperChallengeEvent->mCompletedDeveloperChallenges);
}

}   // namespace BrnGui

// ============================================================================
// FOLDED FROM BrnGuiHudMessageAnalyzer_wO_01.cpp (wave O) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

namespace BrnGui
{

// @ 0x82520488 -- fires the parked developer-challenge message (X360-only sibling).
// Update's deferred pass (@0x825277A8..0x82527814) gates on the flag, calls this, then
// clears both the flag (stb 0 -> 0x4F9) and the bit set (std 0 -> 0x500).
void HudMessageAnalyzer::TriggerDeveloperChallengeMessageDEBUG()
{
    // Non-gating tripwire; the string is verbatim X360 assert rodata (@0x825204B8,
    // cpp:0x176A == 5994) -- it is what names the bool member.
    CGS_ASSERT(mbDEBUGDeveloperChallengeComplete, "mbDEBUGDeveloperChallengeComplete");

    GuiHudMessage lMessage;
    lMessage.Construct("Generic");
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0, "DEVELOPER CHALLENGE COMPLETE");

    // Index of the FIRST completed challenge. MEASURED: the X360 folds
    // FastBitArray<15>::Begin() + Iterator::GetIndex() inline @0x825204EC..0x82520660
    // (bit-0 fast path; else a cntlzd test that parks an EMPTY set at 15 == End();
    // else a mask<<=1 / ++index walk to the lowest set bit). The two asserts inside
    // that inline are the CONTAINER's own (CgsFastBitArray.h:235 "Attempt to get index
    // when out of range" and h:282 "Internal mask has wrapped - expected to find valid
    // bit"), so per the committed container-header policy their StrStream scaffolding
    // is deliberately NOT reproduced here. Update only reaches this function after
    // Handle @0x824F9D48 parked a non-zero set.
    const s32 liChallengeIndex = mCompletedDeveloperChallenges.Begin().GetIndex();

    // MEASURED @0x82520664: SPrintf's length argument is 10, on a 16-byte stack slot.
    char lacIndexText[16];
    CgsCore::SPrintf(lacIndexText, 10, "%d", liChallengeIndex);

    // Both AddParam calls are the STRING (type 1) overload; the MIDDLE argument is the
    // display-string index (0 then 1), NOT a second param type.
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1, lacIndexText);

    TriggerMessage(&lMessage);   // bl sub_82517AF8 == TriggerMessage(const GuiHudMessage*)
}

}   // namespace BrnGui
