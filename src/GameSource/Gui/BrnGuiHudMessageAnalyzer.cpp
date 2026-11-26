#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Gui/BrnGuiHudMessageAnalyzer.h"
#include <cstring>   // std::memcpy (the delayed-message adopt)
#define CGS_ARTIST_BRNGUIHUDMESSAGEANALYZER_FILE "..\\..\\..\\GameSource\\Gui/BrnGuiHudMessageAnalyzer.cpp"
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h" // CGS_ASSERT + MessageAt/StreamMessageAt
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

namespace
{
    const char* ResolveStuntString(CgsLanguage::LanguageManager* lpManager, const char* lpcKey)
    {
        const u8* lpString = lpManager->FindString(lpcKey);
        return lpString ? reinterpret_cast<const char*>(lpString) : lpcKey;
    }
}
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

// Asserts pass __FILE__; ARTIST's reads "..\\..\\..\\GameSource\\Gui/BrnGuiHudMessageAnalyzer.cpp".

namespace BrnGui
{

// HudMessageAnalyzer::TriggerMessage (both overloads, ARTIST 0x825179E8 / 0x82517AF8) are header-inline:
// BrnGuiHudMessageAnalyzer.h, on the lines their asserts carry.

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
    CGS_ASSERT_AT(lpEvent->meAggressorActiveRaceCarIndex >= 0,
               "lpEvent->meAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0", CGS_ARTIST_FILE, 3594);
    CGS_ASSERT_AT(lpEvent->meAggressorActiveRaceCarIndex < 8,
               "lpEvent->meAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT", CGS_ARTIST_FILE, 3595);
    CGS_ASSERT_AT(lpEvent->meVictimActiveRaceCarIndex >= 0,
               "lpEvent->meVictimActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0", CGS_ARTIST_FILE, 3596);
    CGS_ASSERT_AT(lpEvent->meVictimActiveRaceCarIndex < 8,
               "lpEvent->meVictimActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT", CGS_ARTIST_FILE, 3597);

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
    // Non-gating range tripwires: BrnGuiHudMessageAnalyzer.h:792-794 (a header body).
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               CGS_ASSERT_MESSAGE_AT("leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0", CGS_ARTIST_FILE, 792));
    CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               CGS_ASSERT_MESSAGE_AT("leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT", CGS_ARTIST_FILE, 793));
    CGS_ASSERT(mpGuiCache != NULL, CGS_ASSERT_MESSAGE_AT("mpGuiCache", CGS_ARTIST_FILE, 794));

    // DWARF h:751 lpPlayerInfo; GuiCache::GetOnlinePlayerInfo(EActiveRaceCarIndex) is inlined
    // (the eight-slot scan @0x824ECEA0).
    const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerInfo =
        mpGuiCache->GetOnlinePlayerInfo(leActiveRaceCarIndex);
    if (lpPlayerInfo != NULL)
    {
        *lpbValid = true;
        return lpPlayerInfo->mPlayerName.macName;
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
    CGS_ASSERT(lpEliminationEvent != NULL, CGS_ASSERT_MESSAGE_AT("lpEliminationEvent != NULL", __FILE__, 5299 /* ARTIST 0x8251FAC8 */));

    GuiHudMessage lMessage;

    if (lpEliminationEvent->mbIsLocalPlayer)
    {
        CGS_ASSERT(lpEliminationEvent->mPlayerId == KI_INVALID_PLAYER_ID,
                   CGS_ASSERT_MESSAGE_AT("lpEliminationEvent->mPlayerId == CgsNetwork::K_INVALID_PLAYER_ID", __FILE__, 5305 /* ARTIST 0x8251FAFC */));

        if (lpEliminationEvent->mbIsTeammate)
            lMessage.Construct("OnlSrPtElim");
        else
            lMessage.Construct("OnlSrRtElim");

        TriggerMessage(&lMessage);
        return;
    }

    CGS_ASSERT(lpEliminationEvent->mPlayerId != KI_INVALID_PLAYER_ID,
               CGS_ASSERT_MESSAGE_AT("lpEliminationEvent->mPlayerId != CgsNetwork::K_INVALID_PLAYER_ID", __FILE__, 5320 /* ARTIST 0x8251FB70 */));

    if (lpEliminationEvent->mbIsLastPlayer)
    {
        lMessage.Construct("OnlSrLpElim");
    }
    else
    {
        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpEliminatedPlayer =
            mpGuiCache->GetOnlinePlayerInfo(lpEliminationEvent->mPlayerId);

        // One Construct per arm (ARTIST 0x8251FBBC: r3 = &lMessage above the branch, the
        // call cross-jumped -- an if/else, not a ternary argument).
        if (lpEliminationEvent->mbIsTeammate)
        {
            lMessage.Construct("OnlSRTpElim");
        }
        else
        {
            lMessage.Construct("OnlSrRiElim");
        }
        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0,
                          lpEliminatedPlayer->mPlayerName.macName);
    }

    TriggerMessage(&lMessage);
}

// @ 0x8251FC08
void HudMessageAnalyzer::HandleOnlineStuntRunLeading(
        const GuiOnlineStuntRunLeadingEvent* lpLeadingEvent)
{
    CGS_ASSERT(lpLeadingEvent != NULL,
        CGS_ASSERT_MESSAGE_AT("lpLeadingEvent != NULL", __FILE__, 5362 /* ARTIST 0x8251FC34 */));

    GuiHudMessage lMessage;

    if (lpLeadingEvent->mbIsLocalPlayer)
    {
        CGS_ASSERT(lpLeadingEvent->mPlayerId == KI_INVALID_PLAYER_ID,
            CGS_ASSERT_MESSAGE_AT(
                "lpLeadingEvent->mPlayerId == CgsNetwork::K_INVALID_PLAYER_ID",
                __FILE__, 5368 /* ARTIST 0x8251FC68 */));

        if (lpLeadingEvent->mbIsTeammate)
            lMessage.Construct("OnlSrPtLead");
        else
            lMessage.Construct("OnlSrRtLead");
    }
    else
    {
        CGS_ASSERT(lpLeadingEvent->mPlayerId != KI_INVALID_PLAYER_ID,
            CGS_ASSERT_MESSAGE_AT(
                "lpLeadingEvent->mPlayerId != CgsNetwork::K_INVALID_PLAYER_ID",
                __FILE__, 5383 /* ARTIST 0x8251FCDC */));

        if (lpLeadingEvent->mbIsLastPlayer)
        {
            lMessage.Construct("OnlSrLpLead");
        }
        else
        {
            const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpLeadingPlayer =
                mpGuiCache->GetOnlinePlayerInfo(lpLeadingEvent->mPlayerId);

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
    CGS_ASSERT(lpVictoryEvent != NULL,
        CGS_ASSERT_MESSAGE_AT("lpVictoryEvent != NULL", __FILE__, 5416 /* ARTIST 0x8251FD94 */));

    GuiHudMessage lMessage;

    if (lpVictoryEvent->mbIsLocalPlayer)
    {
        CGS_ASSERT(lpVictoryEvent->mPlayerId == KI_INVALID_PLAYER_ID,
            CGS_ASSERT_MESSAGE_AT(
                "lpVictoryEvent->mPlayerId == CgsNetwork::K_INVALID_PLAYER_ID",
                __FILE__, 5422 /* ARTIST 0x8251FDC8 */));

        if (lpVictoryEvent->mbIsTeammate)
            lMessage.Construct("OnlSrPtWin");
        else
            lMessage.Construct("OnlSrRtWin");
    }
    else
    {
        CGS_ASSERT(lpVictoryEvent->mPlayerId != KI_INVALID_PLAYER_ID,
            CGS_ASSERT_MESSAGE_AT(
                "lpVictoryEvent->mPlayerId != CgsNetwork::K_INVALID_PLAYER_ID",
                __FILE__, 5437 /* ARTIST 0x8251FE3C */));

        if (lpVictoryEvent->mbIsLastPlayer)
        {
            lMessage.Construct("OnlSrLpWin");
        }
        else
        {
            const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpVictoryPlayer =
                mpGuiCache->GetOnlinePlayerInfo(lpVictoryEvent->mPlayerId);

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
    CGS_ASSERT(lpLastRunEvent != NULL,
        CGS_ASSERT_MESSAGE_AT("lpLastRunEvent", __FILE__, 5470 /* ARTIST 0x8251FEEC */));
    (void)lpLastRunEvent;   // pointer-only tripwire; no field is read.

    GuiHudMessage lMessage;
    lMessage.Construct("OnlSRLastRun");
    TriggerMessage(&lMessage);
}

// @ 0x8251FF38
void HudMessageAnalyzer::HandleOnlineStuntRunMessage(
        const GuiOnlineStuntRunMessageEvent* lpMessageEvent)
{
    CGS_ASSERT_AT(lpMessageEvent != NULL, "lpMessageEvent", CGS_ARTIST_FILE, 5492);

    GuiHudMessage lMessage;

    if (lpMessageEvent->meMessageType == GuiOnlineStuntRunMessageEvent::E_ONLINE_STUNT_RUN_MESSAGE_SCORE)
    {
        // A team-scored / rival-scored notification: pick the message by comparing the local
        // player's online team to the scoring player's online team, and tag it with that
        // player's name.
        const s32 liLocalPlayerTeam = mpGuiCache->GetCurrentOnlinePlayerTeam(
            static_cast<EActiveRaceCarIndex>(mpGuiCache->GetPlayerActiveRaceCarIndex()));

        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpEliminatedPlayer =
            mpGuiCache->GetOnlinePlayerInfo(lpMessageEvent->mPlayerId);
        CGS_ASSERT_AT(lpEliminatedPlayer != NULL, "lpEliminatedPlayer", CGS_ARTIST_FILE, 5503);

        const s32 liScoringPlayerTeam = mpGuiCache->GetCurrentOnlinePlayerTeam(
            static_cast<EActiveRaceCarIndex>(lpEliminatedPlayer->meActiveRaceCarIndex));

        if (liLocalPlayerTeam == liScoringPlayerTeam)
        {
            lMessage.Construct("OnlSrTpScore");
        }
        else
        {
            lMessage.Construct("OnlSrRiScore");
        }
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
    CGS_ASSERT(lpMessage != NULL, CGS_ASSERT_MESSAGE_AT("lpMessage", __FILE__, 5916 /* ARTIST 0x824F3540 */));
    CGS_ASSERT(lpcStringID != NULL, CGS_ASSERT_MESSAGE_AT("lpcStringID", __FILE__, 5917 /* ARTIST 0x824F3564 */));
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               CGS_ASSERT_MESSAGE_AT("leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0", __FILE__, 5919 /* ARTIST 0x824F3588 */));
    CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               CGS_ASSERT_MESSAGE_AT("leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT", __FILE__, 5920 /* ARTIST 0x824F35AC */));

    // DWARF locals (BrnGuiHudMessageAnalyzer.cpp:5533/5535): lbSuccess, lpcGamertag.
    bool lbSuccess = true;
    const char* lpcGamertag = GetOnlineName(leActiveRaceCarIndex, &lbSuccess);

    if (lbSuccess == true)   // ARTIST 0x824F35D8: cmplwi r11,1 / bne
    {
        // Register the resolved name as a runtime "GAMERTAG"-formatted string under
        // lpcStringID, then reference it by id from the message.
        mpAccessPointers->mpLanguageManager->FormatAndAddText(
            lpcStringID, "GAMERTAG",
            CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP, 1,
            lpcGamertag, CgsLanguage::LanguageManager::E_FORMAT_TEXT);
        lpMessage->AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 0, lpcStringID);
    }

    return lbSuccess;
}

// @ 0x8251C010
// Pass-through title/subtitle message: build a "Generic" HUD message, guard both
// strings against the 64-byte field size, add them as display-string params, and fire.
void HudMessageAnalyzer::HandleGenericHUDMessage(const GuiGenericHUDMessage* lpEvent)
{
    GuiHudMessage lMessage;
    lMessage.Construct("Generic");

    CGS_ASSERT(std::strlen(lpEvent->mTitle) < sizeof(lpEvent->mTitle),
        CGS_ASSERT_MESSAGE_AT(
            "strlen(lpEvent->mTitle) < sizeof(lpEvent->mTitle)",
            __FILE__, 2252 /* ARTIST 0x8251C070 */));
    CGS_ASSERT(std::strlen(lpEvent->mSubtitle) < sizeof(lpEvent->mSubtitle),
        CGS_ASSERT_MESSAGE_AT(
            "strlen(lpEvent->mSubtitle) < sizeof(lpEvent->mSubtitle)",
            __FILE__, 2253 /* ARTIST 0x8251C0BC */));

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
    else if (!mpGuiCache->IsOnlineStartInProgress() || mpGuiCache->GetOnlinePlayerCount() != 0)
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

        CGS_ASSERT_AT(lpTakedown->meAggressorIndex != lpTakedown->meVictimIndex,
                   "lpTakedown->meAggressorIndex != lpTakedown->meVictimIndex", CGS_ARTIST_FILE, 2514);

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
    CGS_ASSERT(lpHudMessage != NULL, CGS_ASSERT_MESSAGE_AT("lpHudMessage", __FILE__, 2576 /* ARTIST 0x824F9C60 */));
    CGS_ASSERT(lpTakedown != NULL, CGS_ASSERT_MESSAGE_AT("lpTakedown", __FILE__, 2577 /* ARTIST 0x824F9C84 */));

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
            CGS_ASSERT(liChainCount - 2 >= 0, CGS_ASSERT_MESSAGE_AT("liChainIndex >= 0 && liChainIndex < liChainHudMessageCount", __FILE__, 2631 /* ARTIST 0x824F9CFC */));
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
    CGS_ASSERT_AT(leCrashBarState >= 0, "lCrashingState >= 0", CGS_ARTIST_FILE, 2719);
    CGS_ASSERT_AT(leCrashBarState < GuiPlayerCrashingStateChangeEvent::E_CRASHBARSTATE_COUNT,
               "lCrashingState < BrnGui::GuiPlayerCrashingStateChangeEvent::E_CRASHBARSTATE_COUNT", CGS_ARTIST_FILE, 2720);
    CGS_ASSERT_AT(mpGuiCache != NULL, "mpGuiCache", CGS_ARTIST_FILE, 2721);

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
            CGS_ASSERT_AT(meDirtyTrickPending >= BrnNetwork::E_PAYBACK_TYPE_START,
                       "meDirtyTrickPending >= BrnNetwork::E_PAYBACK_TYPE_START", CGS_ARTIST_FILE, 2786);
            CGS_ASSERT_AT(meDirtyTrickPending < 3,
                       "meDirtyTrickPending < BrnNetwork::E_PAYBACK_TYPE_COUNT", CGS_ARTIST_FILE, 2787);

            lMessage.Construct(KAPC_PAYBACK_TAKEDOWN_BD_MESSAGES[meDirtyTrickPending]);

            CGS_ASSERT_AT(macDTPendingVictim != NULL, "macDTPendingVictim", CGS_ARTIST_FILE, 2795);
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
    CGS_ASSERT(lpEvent != NULL,
        CGS_ASSERT_MESSAGE_AT(
            "lpNetworkPlayerCrashedEvent",
            __FILE__, 2920 /* ARTIST 0x8251CC1C */));
    CGS_ASSERT(lpEvent->meNetworkPlayerActiveRaceCarIndex >= 0,
        CGS_ASSERT_MESSAGE_AT(
            "lpNetworkPlayerCrashedEvent->meNetworkPlayerActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0",
            __FILE__, 2921 /* ARTIST 0x8251CC44 */));
    CGS_ASSERT(lpEvent->meNetworkPlayerActiveRaceCarIndex < 8,
        CGS_ASSERT_MESSAGE_AT(
            "lpNetworkPlayerCrashedEvent->meNetworkPlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT",
            __FILE__, 2922 /* ARTIST 0x8251CC6C */));

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

// (The payback handlers call GuiCache::GetOnlinePlayerInfo(EActiveRaceCarIndex), the header-inline
// DWARF h:775 scan; the local helper that re-built it over the by-index accessor is gone.)





















// @ 0x8251D868
// The aggressor has armed a dirty trick. Only the local aggressor gets the "arming"
// heads-up ("DTArming"); everyone else is silent.
void HudMessageAnalyzer::HandleDirtyTrickNew(const GuiDirtyTrickNewEvent* lpEvent)
{
    CGS_ASSERT(lpEvent != NULL, "Invalid Dirty Trick Event");
    CGS_ASSERT_AT(lpEvent->meAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               "lpDTEvent->meAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0", CGS_ARTIST_FILE, 3415);
    CGS_ASSERT_AT(lpEvent->meAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "lpDTEvent->meAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT", CGS_ARTIST_FILE, 3416);

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
    CGS_ASSERT_AT(lpEvent->meAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               "lpDTEvent->meAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0", CGS_ARTIST_FILE, 3441);
    CGS_ASSERT_AT(lpEvent->meAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "lpDTEvent->meAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT", CGS_ARTIST_FILE, 3442);
    CGS_ASSERT_AT(lpEvent->meVictimActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               "lpDTEvent->meVictimActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0", CGS_ARTIST_FILE, 3443);
    CGS_ASSERT_AT(lpEvent->meVictimActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "lpDTEvent->meVictimActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT", CGS_ARTIST_FILE, 3444);

    const EActiveRaceCarIndex leLocalPlayer =
        static_cast<EActiveRaceCarIndex>(mpGuiCache->GetPlayerActiveRaceCarIndex());

    GuiHudMessage lMessage;

    if (lpEvent->meAggressorActiveRaceCarIndex == leLocalPlayer)
    {
        // The local player pulled off the dirty trick.
        lMessage.Construct("PbkGdPayBack");

        CGS_ASSERT_AT(mpGuiCache != NULL, "mpGuiCache", CGS_ARTIST_FILE, 3457);
        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpVictimInfo =
            mpGuiCache->GetOnlinePlayerInfo( lpEvent->meVictimActiveRaceCarIndex);
        CGS_ASSERT(lpVictimInfo != NULL,
                   "mpGuiCache->GetOnlinePlayerInfo( lpDTEvent->meVictimActiveRaceCarIndex )");
        CGS_ASSERT_AT(lpVictimInfo->mPlayerName.macName != NULL,
                   "mpGuiCache->GetOnlinePlayerInfo( lpDTEvent->meVictimActiveRaceCarIndex )->mPlayerName.GetPlayerName()", CGS_ARTIST_FILE, 3459);

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
            mpGuiCache->GetOnlinePlayerInfo( lpEvent->meAggressorActiveRaceCarIndex);
        CgsCore::StrCpy(macDTPendingVictim, sizeof(macDTPendingVictim),
                        lpAggressorInfo->mPlayerName.macName);

        CGS_ASSERT_AT(mpGuiCache != NULL, "mpGuiCache", CGS_ARTIST_FILE, 3479);
        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpVictimInfo =
            mpGuiCache->GetOnlinePlayerInfo( lpEvent->meVictimActiveRaceCarIndex);
        CGS_ASSERT_AT(lpVictimInfo != NULL,
                   "mpGuiCache->GetOnlinePlayerInfo( lpDTEvent->meVictimActiveRaceCarIndex )", CGS_ARTIST_FILE, 3480);
        CGS_ASSERT_AT(lpVictimInfo->mPlayerName.macName != NULL,
                   "mpGuiCache->GetOnlinePlayerInfo( lpDTEvent->meVictimActiveRaceCarIndex )->mPlayerName.GetPlayerName()", CGS_ARTIST_FILE, 3481);

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
    CGS_ASSERT_AT(lpEvent != NULL, "Invalid Dirty Trick Event", CGS_ARTIST_FILE, 3508);
    CGS_ASSERT_AT(lpEvent->meTrickType < 3, "Trick type invalid", CGS_ARTIST_FILE, 3509);
    CGS_ASSERT_AT(lpEvent->meAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               "lpDTEvent->meAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0", CGS_ARTIST_FILE, 3510);
    CGS_ASSERT_AT(lpEvent->meAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "lpDTEvent->meAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT", CGS_ARTIST_FILE, 3511);
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
            CGS_ASSERT_AT(lpEvent->meTrickType >= 0,
                       "lpDTEvent->meTrickType >= BrnNetwork::E_PAYBACK_TYPE_START", CGS_ARTIST_FILE, 3535);
            CGS_ASSERT_AT(lpEvent->meTrickType < 3,
                       "lpDTEvent->meTrickType < BrnNetwork::E_PAYBACK_TYPE_COUNT", CGS_ARTIST_FILE, 3536);

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
               CGS_ASSERT_MESSAGE_AT("GsmIO::E_MODE_PURSUIT == mpGuiCache->GetGameMode()",
                                         __FILE__, 3272 /* ARTIST 0x8251D4E8 */));

    // The id is passed straight through: ARTIST forms the buffer address before the
    // GetPursuitCarID call (addi r31, r1, 0x50 @0x8251D4F4) and hands it over after.
    char lCarIdBuffer[16];
    CgsIDConvertToString(mpGuiCache->GetPursuitCarID(), lCarIdBuffer);
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

   // namespace BrnGui

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
    CGS_ASSERT(lpEvent != NULL,
        CGS_ASSERT_MESSAGE_AT("lpEvent", __FILE__, 4646 /* ARTIST 0x824F32CC */));
    CGS_ASSERT(mpGuiCache != NULL,
        CGS_ASSERT_MESSAGE_AT("mpGuiCache", __FILE__, 4647 /* ARTIST 0x824F32F4 */));

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
    CGS_ASSERT_AT(lpEvent != NULL, "lpEvent", CGS_ARTIST_FILE, 4855);

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
    CGS_ASSERT(lpEvent != NULL, CGS_ASSERT_MESSAGE_AT("lpEvent", __FILE__, 4923 /* ARTIST 0x8251F278 */));
    CGS_ASSERT(mpGuiCache != NULL, CGS_ASSERT_MESSAGE_AT("mpGuiCache", __FILE__, 4924 /* ARTIST 0x8251F2A0 */));

    if (lpEvent->meActiveRaceCarIndex !=
            static_cast<EActiveRaceCarIndex>(mpGuiCache->GetPlayerActiveRaceCarIndex())
        && mpGuiCache->IsOnlineStartInProgress())
    {
        GuiHudMessage lMessage;
        // Two Construct calls, one per arm (DWARF lists GuiHudMessage::Construct twice; ARTIST
        // 0x8251F2CC sets r3 = &lMessage above the branch and cross-jumps the call).
        if (lpEvent->mbEnteringShowtime)
        {
            lMessage.Construct("ShowStart");
        }
        else
        {
            lMessage.Construct("ShowStop");
        }

        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerInfo =
            mpGuiCache->GetOnlinePlayerInfo(lpEvent->mNetworkPlayerID);
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
    CGS_ASSERT(lpEvent != NULL,
        CGS_ASSERT_MESSAGE_AT("lpTrophyUnlockedEvent", __FILE__, 5190 /* ARTIST 0x8251F8DC */));   // cpp:5190

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
    GuiHudMessage lMessage;                                                       // DWARF local
    CgsNetwork::PlayerName lPlayerName;                                           // DWARF local
    const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerData;   // DWARF local

    // Non-gating pointer/range tripwires (cpp:5032-5034).
    CGS_ASSERT_AT(lpEvent != NULL, "lpEvent", CGS_ARTIST_FILE, 5032);
    CGS_ASSERT_AT(lpEvent->mNetworkPlayerID != KI_INVALID_PLAYER_ID,
               "lpEvent->mNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID", CGS_ARTIST_FILE, 5033);
    CGS_ASSERT_AT(mpGuiCache != NULL, "mpGuiCache", CGS_ARTIST_FILE, 5034);

    // Debug trace of the joining player id (log-category gated).
    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "NetworkPlayerID: " << lpEvent->mNetworkPlayerID << "\n";
    }

    lpPlayerData = mpGuiCache->GetOnlinePlayerInfo(lpEvent->mNetworkPlayerID);
    CGS_ASSERT_AT(lpPlayerData != NULL, "lpPlayerData", CGS_ARTIST_FILE, 5039);

    lPlayerName = lpPlayerData->mPlayerName;   // the 16-byte copy (memcpy @0x8251F584)
    lMessage.Construct("OnlFBJoin");
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0, lPlayerName.GetPlayerName());
    TriggerMessage(&lMessage);
}

// @ 0x8251F5C0
// Fire the parked "player left the lobby" message: "OnlFBLeave" tagged with the
// stored name, then clear the pending flag + name.
void HudMessageAnalyzer::TriggerPlayerLeftLobby()
{
    CGS_ASSERT(mbOnlinePlayerLeft == true,
        CGS_ASSERT_MESSAGE_AT(
            "mbOnlinePlayerLeft == true", __FILE__, 5079 /* ARTIST 0x8251F5E8 */));   // cpp:5079

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
    CGS_ASSERT(lpEvent != NULL,
        CGS_ASSERT_MESSAGE_AT("lpEvent", __FILE__, 2271 /* ARTIST 0x82525E3C */));
    CGS_ASSERT(lpEvent->meActiveRaceCarIndex >= 0,
        CGS_ASSERT_MESSAGE_AT(
            "lpEvent->meActiveRaceCarIndex >= 0",
            __FILE__, 2272 /* ARTIST 0x82525E64 */));
    CGS_ASSERT(lpEvent->meActiveRaceCarIndex < 8,
        CGS_ASSERT_MESSAGE_AT(
            "lpEvent->meActiveRaceCarIndex < BrnWorld::KI_MAX_ACTIVE_RACE_CARS",
            __FILE__, 2273 /* ARTIST 0x82525E8C */));
    CGS_ASSERT(mpGuiCache != NULL,
        CGS_ASSERT_MESSAGE_AT("mpGuiCache", __FILE__, 2274 /* ARTIST 0x82525EB4 */));

    if (mpGuiCache->GetGameMode() == 13)   // GsmIO::E_MODE_ONLINE_BURNING_HOME_RUN
        HandleBurningHomeRunTeamChange(lpEvent->meActiveRaceCarIndex);
}

// @ 0x8251C110 -- fires the Burning Home Run "runner" / "traitor runner" messages
// when a player's team changes.
void HudMessageAnalyzer::HandleBurningHomeRunTeamChange(EActiveRaceCarIndex leActiveRaceCarIndex)
{
    CGS_ASSERT(leActiveRaceCarIndex >= 0,
        CGS_ASSERT_MESSAGE_AT(
            "leActiveRaceCarIndex >= 0",
            __FILE__, 2306 /* ARTIST 0x8251C13C */));
    CGS_ASSERT(leActiveRaceCarIndex < 8,
        CGS_ASSERT_MESSAGE_AT(
            "leActiveRaceCarIndex < BrnWorld::KI_MAX_ACTIVE_RACE_CARS",
            __FILE__, 2307 /* ARTIST 0x8251C160 */));
    CGS_ASSERT(mpGuiCache != NULL,
        CGS_ASSERT_MESSAGE_AT("mpGuiCache", __FILE__, 2308 /* ARTIST 0x8251C188 */));

    const s32 leOnlinePlayerTeam = mpGuiCache->GetCurrentOnlinePlayerTeam(leActiveRaceCarIndex);
    CGS_ASSERT(leOnlinePlayerTeam != 0,
        CGS_ASSERT_MESSAGE_AT(
            "GsmIO::E_PLAYER_TEAM_NONE != leOnlinePlayerTeam",
            __FILE__, 2312 /* ARTIST 0x8251C1BC */));

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
    CGS_ASSERT(mbOnlineWinnerWaiting,
        CGS_ASSERT_MESSAGE_AT("mbOnlineWinnerWaiting", __FILE__, 3934 /* ARTIST 0x8251E7B8 */));
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
    CGS_ASSERT(lpEvent != NULL,
        CGS_ASSERT_MESSAGE_AT("lpStuntAreaCompleteEvent", __FILE__, 5147 /* ARTIST 0x8251F714 */));
    CGS_ASSERT(lpEvent->meStuntElementType < E_STUNTTYPE_COUNT,
        CGS_ASSERT_MESSAGE_AT(
            "lpStuntAreaCompleteEvent->meStuntElementType < E_STUNTTYPE_COUNT",
            __FILE__, 5151 /* ARTIST 0x8251F73C */));
    CGS_ASSERT(lpEvent->meCounty < BrnWorld::E_COUNTY_VALID_COUNT,
        CGS_ASSERT_MESSAGE_AT(
            "lpStuntAreaCompleteEvent->meCounty < BrnWorld::E_COUNTY_VALID_COUNT",
            __FILE__, 5152 /* ARTIST 0x8251F764 */));

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
    CGS_ASSERT(lpEvent != NULL,
        CGS_ASSERT_MESSAGE_AT("lpEvent", __FILE__, 3147 /* ARTIST 0x8251D1CC */));
    CGS_ASSERT(lpEvent->mSpecialCarID != static_cast<CgsID>(0),
        CGS_ASSERT_MESSAGE_AT(
            "kCGSID_NULL != lpEvent->mSpecialCarID",
            __FILE__, 3148 /* ARTIST 0x8251D1F4 */));

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
    // "mpGuiCache->GetOnlinePlayerInfo( ... )"): GuiCache::GetOnlinePlayerInfo(EActiveRaceCarIndex)
    // is that header-inline scan (DWARF BrnGuiCache.h:775), called directly.
    //
    //
}

// Keep the event switch separate from the queue iterator.
__declspec(noinline) void HudMessageAnalyzer::ProcessInputEvent(const CgsModule::Event* lpEvent,
                                                          s32 liEventType, bool& lbHadWreckEvent)
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
                                        mpGuiCache->GetOnlinePlayerInfo(
                                                                                lpHudMessageEvent->meNewOwner);
                                    CGS_ASSERT(lpNewOwner != NULL,
                                        "mpGuiCache->GetOnlinePlayerInfo( lpHudMessageEvent->meNewOwner ) != NULL"); // cpp:1238

                                    const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpFirst =
                                        mpGuiCache->GetOnlinePlayerInfo(
                                                                                lpHudMessageEvent->meNewOwner);
                                    if (lpFirst == NULL)
                                    {
                                        lbValid = false;
                                    }
                                    else
                                    {
                                        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpSecond =
                                            mpGuiCache->GetOnlinePlayerInfo(
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
                                        mpGuiCache->GetOnlinePlayerInfo(
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
                                        mpGuiCache->GetOnlinePlayerInfo(
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
}

// @ 0x82525FC0
void HudMessageAnalyzer::Update(const CgsGui::GuiEventQueueBase<32768, 16>* lpGuiEventQueue,
                                CgsGui::GuiStackEventQueue::GuiEventQueueLarge* lpViewOutputQueue)
{
    // Entry tripwires (cpp:535 streamed / cpp:538) and adopt the view output queue.
    CGS_ASSERT_AT(lpViewOutputQueue != NULL,
               "View output queue is not valid. Is it definately locked for writing?", CGS_ARTIST_FILE, 538);
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
            ProcessInputEvent(lpEvent, liEventType, lbHadWreckEvent);

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
const char* KAPC_TAKEDOWN_CHAIN[9] =
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
    CGS_ASSERT(lpEvent != NULL,
        CGS_ASSERT_MESSAGE_AT("lpOnTailEvent", __FILE__, 2966 /* ARTIST 0x8251CD04 */));

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
    CGS_ASSERT_AT(lpEvent->meLeadActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "lpEvent->meLeadActiveRaceCarIndex < KI_MAX_PLAYERS", CGS_ARTIST_FILE, 3980);

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
    CGS_ASSERT_AT(lpEvent->meLastActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "lpEvent->meLastActiveRaceCarIndex < KI_MAX_PLAYERS", CGS_ARTIST_FILE, 4011);

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
    CGS_ASSERT_AT(mpGuiCache != NULL, "mpGuiCache", CGS_ARTIST_FILE, 4587);

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
    CGS_ASSERT(lpEvent != NULL,
        CGS_ASSERT_MESSAGE_AT("lpEvent", __FILE__, 1800 /* ARTIST 0x8251B10C */));
    CGS_ASSERT(lpEvent->meAggActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
        CGS_ASSERT_MESSAGE_AT(
            "lpEvent->meAggActiveRaceCarIndex >= 0",
            __FILE__, 1801 /* ARTIST 0x8251B134 */));
    CGS_ASSERT(lpEvent->meAggActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
        CGS_ASSERT_MESSAGE_AT(
            "lpEvent->meAggActiveRaceCarIndex < BrnWorld::KI_MAX_ACTIVE_RACE_CARS",
            __FILE__, 1802 /* ARTIST 0x8251B15C */));

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
    CGS_ASSERT(lpEvent != NULL, CGS_ASSERT_MESSAGE_AT("lpWreckedEvent", __FILE__, 2836 /* ARTIST 0x8251CAC4 */));

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
            CGS_ASSERT(mpGuiCache != NULL, CGS_ASSERT_MESSAGE_AT("mpGuiCache", __FILE__, 2857 /* ARTIST 0x8251CB5C */));
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

    GuiHudMessage lMessage;

    // ---- failure lane (repetition / crashed / plain fail) ----
    if ((lpEvent->mStuntInfo.muStuntTypes & 0xC0000) != 0)
    {
        lMessage.Construct("GenericBad");

        const char* lpcTitle = ResolveStuntString(lpLanguageManager, "STUNT_RUN_MULTIPLIER_STUNT_FAILED");

        const char* lpcReason = lacBuffer;   // empty fragment unless a reason bit is set
        if ((lpEvent->mStuntInfo.muStuntTypes & 0x40000) != 0)
        {
            lpcReason = ResolveStuntString(lpLanguageManager, "STUNT_RUN_MULTIPLIER_REPETITION");
        }
        else if ((lpEvent->mStuntInfo.muStuntTypes & 0x80000) != 0)
        {
            lpcReason = ResolveStuntString(lpLanguageManager, "STUNT_RUN_MULTIPLIER_CRASHED");
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
            std::strncat(lacBuffer,
                            ResolveStuntString(lpLanguageManager, lbReverse ? "STUNT_RUN_MULTIPLIER_REVERSE_AIR"
                                                     : "STUNT_RUN_MULTIPLIER_AIR"), 0x80);
        }

        // spin (awesome bit 0x1) -- optional REVERSE prefix + singular/plural count
        if ((lpEvent->mStuntInfo.muAwesomeStuntTypes & 0x1) != 0)
        {
            if (lbReverse)
            {
                std::strncat(lacBuffer, ResolveStuntString(lpLanguageManager, "STUNT_RUN_MULTIPLIER_REVERSE"), 0x80);
            }

            if (lpEvent->mStuntInfo.muFlatSpins <= 1)
            {
                std::strncat(lacBuffer, ResolveStuntString(lpLanguageManager, "STUNT_RUN_MULTIPLIER_SPIN_SINGLE"), 0x80);
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
                std::strncat(lacBuffer, ResolveStuntString(lpLanguageManager, "STUNT_RUN_MULTIPLIER_REVERSE"), 0x80);
            }

            if (lpEvent->mStuntInfo.muBarrelRolls <= 1)
            {
                std::strncat(lacBuffer, ResolveStuntString(lpLanguageManager, "STUNT_RUN_MULTIPLIER_BARREL_ROLL_SINGLE"), 0x80);
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
                std::strncat(lacBuffer, ResolveStuntString(lpLanguageManager, "STUNT_RUN_MULTIPLIER_REVERSE"), 0x80);
            }

            if (lpEvent->mStuntInfo.mu16X360Tail16 <= 1)
            {
                std::strncat(lacBuffer, ResolveStuntString(lpLanguageManager, "STUNT_RUN_MULTIPLIER_LEAP_CAR_SINGLE"), 0x80);
            }
            else
            {
                lpLanguageManager->FormatTextFromInt(
                    lacFormatBuffer, 63, "STUNT_RUN_MULTIPLIER_LEAP_CAR_MULTIPLE",
                    CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
                    lpEvent->mStuntInfo.mu16X360Tail16,
                    CgsLanguage::LanguageManager::E_FORMAT_INTEGER);
                CgsCore::StrCat(lacBuffer, 0x80, lacFormatBuffer);
            }
        }

        // super jump (awesome bit 0x10)
        if ((lpEvent->mStuntInfo.muAwesomeStuntTypes & 0x10) != 0)
        {
            std::strncat(lacBuffer, ResolveStuntString(lpLanguageManager, "STUNT_RUN_MULTIPLIER_SUPER_JUMP"), 0x80);
        }

        // smash (awesome bit 0x20)
        if ((lpEvent->mStuntInfo.muAwesomeStuntTypes & 0x20) != 0)
        {
            std::strncat(lacBuffer, ResolveStuntString(lpLanguageManager, "STUNT_RUN_MULTIPLIER_SMASH"), 0x80);
        }

        // billboard (awesome bit 0x40)
        if ((lpEvent->mStuntInfo.muAwesomeStuntTypes & 0x40) != 0)
        {
            std::strncat(lacBuffer, ResolveStuntString(lpLanguageManager, "STUNT_RUN_MULTIPLIER_BILLBOARD"), 0x80);
        }

        // burnout (awesome bit 0x80)
        if ((lpEvent->mStuntInfo.muAwesomeStuntTypes & 0x80) != 0)
        {
            std::strncat(lacBuffer, ResolveStuntString(lpLanguageManager, "STUNT_RUN_MULTIPLIER_BURNOUT"), 0x80);
        }

        // takedown (awesome bit 0x8000)
        if ((lpEvent->mStuntInfo.muAwesomeStuntTypes & 0x8000) != 0)
        {
            std::strncat(lacBuffer, ResolveStuntString(lpLanguageManager, "STUNT_RUN_MULTIPLIER_TAKEDOWN"), 0x80);
        }

        // chain (the tail count) -- always plural-formatted
        if (lpEvent->mStuntInfo.mu16X360Tail14 != 0)
        {
            lpLanguageManager->FormatTextFromInt(
                lacFormatBuffer, 63, "STUNT_RUN_MULTIPLIER_CHAIN",
                CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
                lpEvent->mStuntInfo.mu16X360Tail14,
                CgsLanguage::LanguageManager::E_FORMAT_INTEGER);
            CgsCore::StrCat(lacBuffer, 0x80, lacFormatBuffer);
        }

        CGS_ASSERT_AT(std::strlen(lacBuffer) > 3, "strlen(lacBuffer) > 3", CGS_ARTIST_FILE, 2139);

        const s32 liChainScore = lpEvent->mStuntInfo.mu16X360Tail14 + lpEvent->mStuntInfo.miStuntMultiplier;
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
    CGS_ASSERT_AT(lpDeveloperChallengeEvent != NULL, "lpDeveloperChallengeEvent", CGS_ARTIST_FILE, 5973);

    // @0x824F9D88..0x824F9DB4 is FastBitArray<15>::IsZero() folded inline: a ldx/cmpldi
    // scan over the bit fields (one, for <15>), with the assert firing when the scan
    // completes without finding a non-zero field (r11 = 1 at 0x824F9DA8 -> fire;
    // the early bne at 0x824F9D98 lands on r11 = 0 -> skip).
    CGS_ASSERT_AT(!lpDeveloperChallengeEvent->mCompletedDeveloperChallenges.IsZero(),
               "!lpDeveloperChallengeEvent->mCompletedDeveloperChallenges.IsZero()", CGS_ARTIST_FILE, 5974);

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
    CGS_ASSERT_AT(mbDEBUGDeveloperChallengeComplete, "mbDEBUGDeveloperChallengeComplete", CGS_ARTIST_FILE, 5994);

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

// FOLDED FROM BrnGuiHudMessageAnalyzer_gUI_01.cpp
#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"   // brings BrnGuiEventTypeDefs + PlayerName
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"         // GuiEventStuntInfo / GuiEventStuntAllComplete / GuiHUDMessageSignSmashed

#include "GameShared/GameClasses/Core/CgsAssert.h"      // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"          // CgsID

#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint ([DIAG] BRN_PROP_DIAG)
#include <stdlib.h>                                          // getenv    ([DIAG] BRN_PROP_DIAG)

// BrnGui::HudMessageAnalyzer -- reconstructed from BURNOUT_X360_ARTIST.XEX
// (gateui wave partfile 01). The three collectible/sign HUD-popup handlers the wave-B
// fan-out dispatches but never bodied:
//
//   HandleStuntInfo                          @0x8251F650  (37 insns)
//   HandleStuntsComplete(GuiEventStuntAllComplete*) @0x8251F7E8  (50 insns, cpp:5164/5168)
//   HandleSignSmashMessage                   @0x8251BF40  (25 insns)
//
// WHICH ONE IS THE BILLBOARD/SMASH-GATE POPUP -- the wave brief had this backwards.
// HandleSignSmashMessage is the CRASH-MODE OVERHEAD SIGN: its sole producer is
// BrnGame::BrnGameModule::TranslateShowtimeActionToGuiEvent @0x823E1988 case 128
// (E_ACTION_OVERHEAD_SIGN_HIT), whose whole translate call is gated on game mode 2/16, and
// its feeder is PropEntityIO::OutputBuffer_PostPhysics::mHitOverheadSignQueue (+0x7B0) ->
// CrashModeScoring::DealWithHitOverheadSign @0x82312928. A different subsystem.
// The REAL billboard / smash-gate popup is HandleStuntInfo: game action 58
// (E_ACTION_ON_STUNT_ELEMENT_COMPLETE, posted by StuntManager::ProcessStuntElement
// @0x8239CDB0) -> TranslateGameActionsToGuiEvents @0x823E9CE0 case 58 -> GuiEventStuntInfo
// (217) or GuiEventBoostBarStuntInfo (218) -> Update case 217 -> here. That is literally
// the "Billboards Smashed 12/45" line.
//
// The message-name table is KA_STUNT_INFO_MESSAGES (X360 .data qword_82FB5898), defined in
// BrnGuiHudMessageAnalyzer_wB_res.cpp alongside the other recovered rodata; the
// collectible-completion string ids are the already-committed KAPC_COLLECTABLE_COMPLETION_
// STRINGID (@0x8206F684) that the sibling area-complete handler in _wB_11 indexes.

namespace BrnGui
{

// @ 0x8251F650
// The running collectible tally popup -- "<family> <current>/<total>". The X360 body
// inlines the CgsID Construct overload (the message-name hash is stored straight into the
// stack record at +0x00 and the three parameter counts are zeroed at +0x08/+0x0C/+0x10,
// asm 0x8251F694..0x8251F6A4), then appends BOTH counts as INT parameters of display
// string 1 -- both calls pass `li r5,1` (0x8251F680 / 0x8251F6AC), i.e. two parameters on
// the same display string, not one each on strings 0 and 1.
//
// NOTE: no assert guards the type here (unlike the two sibling *Complete handlers) -- the
// X360 body indexes the 3-entry table unchecked (`lwz r8,8(r31); slwi r8,r8,3; ldx`).
// Reproduced as-is; the producer's own MapStuntEnumsFromGameplayToGui @0x823AA4A8 is what
// asserts >= 3.
void HudMessageAnalyzer::HandleStuntInfo(const GuiEventStuntInfo* lpEvent)
{
    GuiHudMessage lMessage;
    lMessage.Construct(KA_STUNT_INFO_MESSAGES[lpEvent->meStuntType]);
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 1, lpEvent->miCurrentCount);
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 1, lpEvent->miTotalCount);

    // [DIAG] NOT IN THE X360 BINARY. The last rung of the gateui ladder (set BRN_PROP_DIAG
    // -- the same knob the wave-Q prop probe uses, so the whole chain prints under one
    // switch). Emitted BEFORE TriggerMessage so a line appears even if the director drops
    // the message (IsMessageAllowed can veto it). The latch is evaluated ONCE.
    {
    }

    TriggerMessage(&lMessage);
}

// @ 0x8251F7E8
// "Every collectible of this family is done, city-wide" -- the sibling of the
// per-county HandleStuntsComplete(GuiEventStuntAreaComplete*) in _wB_11. Same
// "StntAllDone" message, same family-completion string id in slot 0, but the county slot
// carries the literal "PARADISE_CITY" instead of a county string id.
void HudMessageAnalyzer::HandleStuntsComplete(const GuiEventStuntAllComplete* lpEvent)
{
    // Non-gating tripwires (cpp:5164/5168; both streamed on the X360, folded static).
    CGS_ASSERT(lpEvent != NULL, "lpStuntAllCompleteEvent");
    CGS_ASSERT(lpEvent->meStuntElementType < E_STUNTTYPE_COUNT,
               "lpStuntAllCompleteEvent->meStuntElementType < E_STUNTTYPE_COUNT");

    GuiHudMessage lMessage;
    lMessage.Construct("StntAllDone");
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 0,
                      KAPC_COLLECTABLE_COMPLETION_STRINGID[lpEvent->meStuntElementType]);
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 1, "PARADISE_CITY");

    // [DIAG] NOT IN THE X360 BINARY -- see HandleStuntInfo.
    {
    }

    TriggerMessage(&lMessage);
}

// @ 0x8251BF40
// The crash-mode overhead-sign score popup ("ShowSignHit" + the points awarded as an INT
// parameter of display string 2 -- `li r4,2; li r5,2` @0x8251BF6C/0x8251BF70). Landed as a
// freebie alongside the two stunt handlers because it is 25 instructions and the fan-out
// already dispatches it (Update case 400); it is NOT the wave's proof rung.
void HudMessageAnalyzer::HandleSignSmashMessage(const GuiHUDMessageSignSmashed* lpEvent)
{
    GuiHudMessage lMessage;
    lMessage.Construct("ShowSignHit");
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 2, lpEvent->miPointsAwarded);

    // [DIAG] NOT IN THE X360 BINARY -- see HandleStuntInfo.
    {
    }

    TriggerMessage(&lMessage);
}

} // namespace BrnGui

// FOLDED FROM BrnGuiHudMessageAnalyzer_gUI_02.cpp
#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"   // brings BrnGuiEventTypeDefs + PlayerName
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"         // the four HUD-message payloads below
#include "GameSource/Gui/BrnGuiCache.h"                 // GuiCache (friend access + GetOnlinePlayerInfo)
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h" // InGamePlayerStatusData
#include "GameSource/GameState/BrnGameStateSharedIO.h"  // GameStateModuleIO::EGameModeType

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"      // CGS_ASSERT

// BrnGui::HudMessageAnalyzer -- reconstructed from BURNOUT_X360_ARTIST.XEX
// (gateui wave partfile 02). Six more of the fan-out's declaration-only arms, landed to
// shrink the analyzer's link surface: every one of these is dispatched by the committed
// Update switch in _wB_12, so each missing body was an LNK2019 standing between this tree
// and a mountable HudMessageAnalyzer. They do NOT close that surface on their own -- the
// measured residual after this partfile was twelve more bodyless Handle*/Trigger* arms
// (rounds 2 and 3 landed them in _gUI_03 / _gUI_04).
// [gateui r3] The `report_r2_guipump.md` this line used to point at was never written; the
// dumpbin UNDEF sweep that measures the surface is in
// scratch/gateui_wave/verify_r2_guipump/VERDICT.md (section 2, WRONG-2).
//
//   HandleComboPerformed            @0x8251BE40  (31 insns)
//   HandleShowtimeMultiplierMessage @0x8251BEC0  (31 insns)
//   HandleCrushComboMessage         @0x8251BFA8  (25 insns)
//   HandleAllEventsOfTypeComplete   @0x82520318  (38 insns)
//   HandleAllJunctionsOfTypeFound   @0x825203D0  (38 insns)
//   HandlePlayerLeftLobby           @0x824F3348  (37 insns, cpp:5059)
//   GetOnlineName(s32, bool*)       @0x824ECF10  (the player-id keyed overload)

namespace BrnGui
{

namespace
{
    // The two metres-to-display-unit factors the boundary handlers below bake in
    // (X360 rodata flt_8203AF08 / flt_82013F90).
    const f32 KF_METRES_TO_MILES      = 0.0006213712f;
    const f32 KF_METRES_TO_KILOMETRES = 0.001f;
}

// @ 0x824ECF10
// The network-player-id keyed sibling of the EActiveRaceCarIndex overload in the class
// TU. Where that one linear-scans the cache's eight player records by car index, this one
// hands the id to the cache's own lookup and reads the name off the record it returns
// (X360 `sub_82482738(mpGuiCache, id)` == GuiCache::GetOnlinePlayerInfo, then
// `record + 256` == mPlayerName). PITFALL, per the header: pass a genuine s32 player id --
// an EActiveRaceCarIndex argument silently binds the OTHER overload.
const char* HudMessageAnalyzer::GetOnlineName(s32 liNetworkPlayerID, bool* lpbValid) const
{
    // Non-gating tripwires (h:827/828).
    CGS_ASSERT(liNetworkPlayerID != -1, "lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");
    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");

    const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerInfo =
        mpGuiCache->GetOnlinePlayerInfo(liNetworkPlayerID);

    if (lpPlayerInfo != NULL)
    {
        *lpbValid = true;
        return lpPlayerInfo->mPlayerName.macName;
    }

    // No matching player: empty name, flagged invalid (callers skip the message).
    *lpbValid = false;
    return "";
}

// @ 0x8251BE40
// Crash-mode combo popup. The BEST-SCORE byte picks the boosted message id; the score
// itself rides as an INT param of display string 1.
void HudMessageAnalyzer::HandleComboPerformed(const GuiHUDMessageComboPerformed* lpEvent)
{
    GuiHudMessage lMessage;
    // One Construct per arm (ARTIST 0x8251BE40 sets r3 = &lMessage above the branch and
    // cross-jumps the call, the shape of an if/else, not a ternary argument).
    if (lpEvent->mbBestScore)
    {
        lMessage.Construct("StuntCmbBst");
    }
    else
    {
        lMessage.Construct("StuntCmb");
    }
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 1, lpEvent->miScore);
    TriggerMessage(&lMessage);
}

// @ 0x8251BEC0
// Crash-mode multiplier popup, EDGE-TRIGGERED: it only fires when the incoming multiplier
// differs from the one the analyzer last showed, and the latch is updated AFTER the
// message goes out (X360 `lwz r11,0(r30); stw r11,0x4B0(r31)` @0x8251BF1C, i.e. re-read
// from the event, not from a saved register). Note the value SHOWN is the EARNED field
// (+0x04) while the value LATCHED is the NEW multiplier (+0x00) -- they are different
// words; do not collapse them.
void HudMessageAnalyzer::HandleShowtimeMultiplierMessage(const GuiHUDMessageShowtimeMultiplier* lpEvent)
{
    if (lpEvent->miNewMultiplier == miLastSignMultiplier)
        return;

    GuiHudMessage lMessage;
    lMessage.Construct("ShowSmash");
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 1, lpEvent->miMultiplierEarned);
    TriggerMessage(&lMessage);

    miLastSignMultiplier = lpEvent->miNewMultiplier;
}

// @ 0x8251BFA8
// Crash-mode crush-combo popup ("ShowCombo" + the count).
void HudMessageAnalyzer::HandleCrushComboMessage(const GuiHUDMessageCrushCombo* lpEvent)
{
    GuiHudMessage lMessage;
    lMessage.Construct("ShowCombo");
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 1, lpEvent->miCrushComboCount);
    TriggerMessage(&lMessage);
}

// @ 0x82520318
// "Every event of this mode is complete" announce. The X360 jump table covers exactly five
// of the eighteen EGameModeType values (0/3/5/7/8 -- the five offline event families that
// have city-wide completion); cases 1/2/4/6 and everything above 8 fall through the
// default arm and fire NOTHING. Faithful: no default message, no assert.
void HudMessageAnalyzer::HandleAllEventsOfTypeComplete(
        BrnGameState::GameStateModuleIO::EGameModeType leGameModeType)
{
    const char* lpcMessageId = NULL;
    switch (leGameModeType)
    {
        case BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_RACE:  lpcMessageId = "EvryRaceCmp";  break;
        case BrnGameState::GameStateModuleIO::E_MODE_ROAD_RAGE:     lpcMessageId = "EvryRRCmp";    break;
        case BrnGameState::GameStateModuleIO::E_MODE_BURNING_ROUTE: lpcMessageId = "EvryBrnRCmp";  break;
        case BrnGameState::GameStateModuleIO::E_MODE_STUNT_ATTACK:  lpcMessageId = "EvryStuntCmp"; break;
        case BrnGameState::GameStateModuleIO::E_MODE_MARKED_MAN:    lpcMessageId = "EvryMMCmp";    break;
        default:                                                    return;
    }

    GuiHudMessage lMessage;
    lMessage.Construct(lpcMessageId);
    TriggerMessage(&lMessage);
}

// @ 0x825203D0
// The junction ("event start location") twin of the above -- same five modes, same
// fall-through-does-nothing default, different message-id family.
void HudMessageAnalyzer::HandleAllJunctionsOfTypeFound(
        BrnGameState::GameStateModuleIO::EGameModeType leGameModeType)
{
    const char* lpcMessageId = NULL;
    switch (leGameModeType)
    {
        case BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_RACE:  lpcMessageId = "EvryRaceJnc";  break;
        case BrnGameState::GameStateModuleIO::E_MODE_ROAD_RAGE:     lpcMessageId = "EvryRRJnc";    break;
        case BrnGameState::GameStateModuleIO::E_MODE_BURNING_ROUTE: lpcMessageId = "EvryBrnRJnc";  break;
        case BrnGameState::GameStateModuleIO::E_MODE_STUNT_ATTACK:  lpcMessageId = "EvryStuntJnc"; break;
        case BrnGameState::GameStateModuleIO::E_MODE_MARKED_MAN:    lpcMessageId = "EvryMMJnc";    break;
        default:                                                    return;
    }

    GuiHudMessage lMessage;
    lMessage.Construct(lpcMessageId);
    TriggerMessage(&lMessage);
}

// @ 0x824F3348
// A remote player left the freeburn lobby. This handler does NOT fire the message: it only
// PARKS the departing player's name and raises the pending flag, and the committed
// TriggerPlayerLeftLobby @0x8251F5C0 (in the class TU) fires "OnlFBLeave" from Update's
// deferred pass. Both gates are load-bearing -- the local player must be in-game
// (`lbz 0x14(event)`) and the cache's game mode must be exactly
// E_MODE_ONLINE_FREE_BURN_LOBBY (`cmpwi r11, 0xF` against mpGuiCache+0x9E58).
void HudMessageAnalyzer::HandlePlayerLeftLobby(const GuiEventNetworkPlayerLeftLobby* lpEvent)
{
    CGS_ASSERT(lpEvent != NULL,
        CGS_ASSERT_MESSAGE_AT(
            "lpEvent",
            CGS_ARTIST_BRNGUIHUDMESSAGEANALYZER_FILE, 5059 /* ARTIST 0x824F3374 */));   // cpp:5059

    if (!lpEvent->mbIsLocalPlayerInGame)
        return;

    if (mpGuiCache->meGameModeType != BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY)
        return;

    mbOnlinePlayerLeft = true;
    mOnlinePlayerLeftName.Construct(lpEvent->mPlayerName.macName);
}

// @ 0x8251C270 / @ 0x8251C318
// Online road-rage progress chatter: "<leader> is N miles/km from the finish". Both are
// the same 42-instruction shape -- resolve the leader's online name, append it as STRING
// param 0, and only if the name RESOLVED append the distance as FLOAT param 0 and fire.
// (An unresolved name aborts the whole message: the X360 falls straight out past both the
// float param and TriggerMessage, `beq cr6, loc_8251C300`.)
//
// FLAG (console defect, reproduced verbatim): BOTH handlers construct the SAME message id
// "OnlRRMiToFin" -- the KM variant never gets a kilometre-specific line. The two differ
// only in the metres conversion factor (flt_8203AF08 == 0.0006213712 miles/m at
// 0x8251C2D4, flt_82013F90 == 0.001 km/m at 0x8251C37C). This is the shipped behaviour;
// do not "fix" it here.
void HudMessageAnalyzer::HandleLeaderPassedMileBoundary(const GuiLeaderPassedMileBoundaryEvent* lpEvent) const
{
    GuiHudMessage lMessage;      // DWARF cpp:2226
    bool lbSuccess = true;       // DWARF cpp:2227 (seeded before Construct: stb @0x8251C29C)
    lMessage.Construct("OnlRRMiToFin");

    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0,
                      GetOnlineName(lpEvent->meActiveRaceCarIndex, &lbSuccess));

    if (lbSuccess)
    {
        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_FLOAT, 0,
                          lpEvent->mfDistanceToFinishInMetres * KF_METRES_TO_MILES);
        TriggerMessage(&lMessage);
    }
}

void HudMessageAnalyzer::HandleLeaderPassedKMBoundary(const GuiLeaderPassedKMBoundaryEvent* lpEvent) const
{
    GuiHudMessage lMessage;
    bool lbSuccess = true;       // seeded before Construct, as in the mile twin
    lMessage.Construct("OnlRRMiToFin");   // FLAG: console reuses the MILE id -- see above.

    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0,
                      GetOnlineName(lpEvent->meActiveRaceCarIndex, &lbSuccess));

    if (lbSuccess)
    {
        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_FLOAT, 0,
                          lpEvent->mfDistanceToFinishInMetres * KF_METRES_TO_KILOMETRES);
        TriggerMessage(&lMessage);
    }
}

} // namespace BrnGui

// FOLDED FROM BrnGuiHudMessageAnalyzer_gUI_03.cpp
#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"   // the class home (brings BrnGuiEventTypeDefs)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"         // GuiHudMessage / GuiChallengeEndEvent / GuiEventRoadRuleNewHighScore
#include "GameSource/Gui/BrnGuiCache.h"                 // GuiCache (friend access + GetWorldDataController/GetHudMessageDirector/IsActiveRaceCarDisconnected)
#include "GameSource/Gui/BrnGuiWorldDataController.h"   // [gateui r3] WorldDataController::GetFreeburnChallengeList (the console two-hop)
#include "GameSource/Gui/BrnGuiHudMessageDirector.h"    // HudMessageDirector::IsMessageAllowed
#include "GameSource/GameState/BrnGameStateSharedIO.h"  // GameStateModuleIO::EGameModeType (meGameModeType compare)
#include "GameSource/BurnoutConstants.h"                // BrnGameState::EChallengeStatus
#include "SharedClasses/StreetData/BrnChallengeData.h"  // BrnStreetData::ScoreType
#include "SharedClasses/DataLists/ChallengeList.h"      // BrnResource::ChallengeList::GetChallengeData
#include "SharedClasses/DataLists/ChallengeListEntry.h" // BrnResource::ChallengeListEntry::GetTitleStringID

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Core/CgsID.h"          // CgsID / CgsIDCompress
#include "GameShared/GameClasses/Core/CgsStringUtils.h" // CgsCore::SPrintf
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // [gateui r3] gpDebugPrint / gxMessageFilterFlags (the drive-through + finisher dumps)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiHudMessage.h"  // CgsGui::E_HUDMESSAGEPARAMTYPES_*
#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostType.h" // [gateui r3] BrnWorld::E_BOOST_TYPE_DANGER

// ===========================================================================================
// BrnGui::HudMessageAnalyzer -- reconstructed from BURNOUT_X360_ARTIST.XEX
// (gateui wave, partfile 03. Round 2 landed the two Trigger* bodies; round 3 landed the four
// handlers round 2 had to park, and corrected one round-2 defect.)
//
//   TriggerNewRoadRulesHighScoreMessage  @0x8251ED08  (198 insns)
//   TriggerChallengeEndedMessage         @0x82520078  (162 insns)
//   HandleDriveThrough                   @0x8251D570  (rd 3)
//   HandleEventFinisher                  @0x824F2FB0  (rd 3)
//   HandleNetworkBattling                @0x8251CDC0  (rd 3)
//   HandleChainedBoost                   @0x8251EA50  (rd 3)
//   + the class static KA_CHALLENGE_END_MESSAGE_IDS (X360 .data qword_82FB58B0, recovered
//     from its dynamic initialiser @0x82C56578 -- it is ZERO in the image, never dump it)
//
// [gateui r3] ROUND-2 DEFECT FIXED: TriggerNewRoadRulesHighScoreMessage's "you now rule N
// roads" tail, copy 2, carried an extra `&& miNumRoadsNowRuled > 0` conjunct that the console
// does not have -- a dropped console branch (verify_r2_guihand1/VERDICT.md WRONG-1). Removed,
// and both copies' FLAGs rewritten to state the asymmetry the way the asm has it.
//
// [gateui r3] The four round-3 handlers were parked in round 2 on payload/accessor
// DECLARATIONS that round's ownership rules forbade. All four dependencies landed this round
// and every one of them is now a real named type/member -- no maData[] indexing, no
// raw-offset hack:
//   GuiDriveThroughEvent (366) / GuiInEventFinisher (423) / GuiNetworkPlayerBattlingEvent
//   (483) / GuiEventBoostInfo (206)   -> real field sets in BrnGuiEventTypeDefs.h
//   GuiCache::IsActiveRaceCarDisconnected @0x82443C50 + maRaceCarDisconnected[8] @+0xA0F4
//                                        -> BrnGuiCache.h
//   CgsNumeric::Random::RandomUInt()     -> bodied in CgsRandom.h next to RandomBool
//   KAPC_DRIVE_THROUGH_* (3 tables)      -> BrnGuiHudMessageAnalyzer_wB_res.cpp
// Verdicts + the round-2 evidence this file was corrected against:
// scratch/gateui_wave/verify_r2_guihand1/VERDICT.md.
// ===========================================================================================

namespace BrnGui
{

// -------------------------------------------------------------------------------------------
// X360 .data qword_82FB58B0 -- the challenge-end HUD message ids, indexed by
// BrnGameState::EChallengeStatus (TriggerChallengeEndedMessage does `slwi r9,r9,3; ldx`,
// unchecked). ZERO in the image; filled by the TU's dynamic initialiser sub_82C56578,
// which the asm gives verbatim (0x82C5658C..0x82C565E0):
//     CgsIDCompress("FBChalComp")  -> qword_82FB58B8   (== [1])
//     CgsIDCompress("FBChalAbort") -> qword_82FB58C0   (== [2])
//     CgsIDCompress("FBChalAbort") -> qword_82FB58C8   (== [3], a SECOND compress of the
//                                                       same string, not an alias store)
//     0                            -> qword_82FB58D0   (== [4])
//     0                            -> qword_82FB58D8   (== [5])
//     CgsIDCompress("FBChalFail")  -> qword_82FB58E0   (== [6])
// [0] is never written by the initialiser -- it stays the zero the image has.
//
// The zero slots are load-bearing behaviour, not gaps: a zero id makes the trigger clear
// mbChallengeEnded and fire nothing (ONGOING / ABORTED_BEFORE_STARTING / RESET_IF_NEEDED).
// Size == E_CHALLENGE_STATUS_COUNT (7): the header declares the array with an incomplete
// bound, so this definition completes it.
//
// (Same recovery class as KA_STUNT_INFO_MESSAGES in _wB_res.cpp: walk the dyn-init thunk,
// never read the zeroed .data.)
const CgsID HudMessageAnalyzer::KA_CHALLENGE_END_MESSAGE_IDS[BrnGameState::E_CHALLENGE_STATUS_COUNT] =
{
    0,                              // [0] E_CHALLENGE_STATUS_ONGOING                     (no message)
    CgsIDCompress("FBChalComp"),    // [1] E_CHALLENGE_STATUS_SUCCESS
    CgsIDCompress("FBChalAbort"),   // [2] E_CHALLENGE_STATUS_ABORTED
    CgsIDCompress("FBChalAbort"),   // [3] E_CHALLENGE_STATUS_ABORTED_DUE_TO_PLAYER_LEAVE
    0,                              // [4] E_CHALLENGE_STATUS_ABORTED_BEFORE_STARTING     (no message)
    0,                              // [5] E_CHALLENGE_STATUS_RESET_IF_NEEDED             (no message)
    CgsIDCompress("FBChalFail"),    // [6] E_CHALLENGE_STATUS_FAILURE
};

// @ 0x8251ED08
// The deferred road-rules high-score announce: Update parks the 48-byte event in
// mRoadRuleHighScoreData + starts mfRoadRulesMessageTimeout (HandleNewRoadRulesHighScore
// @0x824F32A0), and this fires when the timer expires.
//
// Two completely separate shapes share the body, picked by mbMultipleScores:
//   * multi-score summary  -- "you lost N roads" (online/lobby only) + "you now rule N roads"
//   * single-score line    -- "<time|crash> record on <road>", parameterised by the road id
//     PRINTED AS DECIMAL (`CgsCore::SPrintf(buf, 12, "%llu", mRoadId)`) and handed to the
//     message as a STRINGID -- the localisation key for a road IS its compressed id.
//
// [gateui r3] RESOLVED -- the shared-header layout request this body was written against has
// landed. GuiEventRoadRuleNewHighScore in BrnGuiEventTypeDefs.h now carries the X360 member
// order (mRoadId@0x00, meScoreType@0x08, miNumScoresLost@0x0C, miNumRoadsNowRuled@0x10,
// mPlayerName@0x14, the five bools @0x24..0x28), pinned by offsetof, on the two witnesses this
// FLAG cited: this function's own reads (`ld r6, 0x480(r31)` feeding SPrintf("%llu") from
// record+0x00; `addi r30, r31, 0x494` passing record+0x14 as a STRING param, with
// mRoadRuleHighScoreData at analyzer+0x480) and the producer TranslateGameActionsToGuiEvents
// case 281 @0x823EC7C4. sizeof was 48 in BOTH orders, which is why the existing static_assert
// never caught it. The body below reads BY NAME and needed no change.
// (This was faithfulness debt, not a live corruption: BrnGuiHudMessageAnalyzer_wB_07.cpp fills
// the latch with a typed struct copy and nothing in the tree reads the record at raw X360
// offsets -- re-verified this round.)
void HudMessageAnalyzer::TriggerNewRoadRulesHighScoreMessage()
{
    if (mRoadRuleHighScoreData.mbMultipleScores)
    {
        // ---- multi-score summary -------------------------------------------------------
        if (mRoadRuleHighScoreData.miNumScoresLost > 0)
        {
            // The "roads you lost" half shows ONLY when no game mode is running or the
            // player is sitting in the online freeburn lobby: the console tests
            // meGameModeType (cache +0x9E58) against exactly those two values and skips
            // the whole message otherwise (0x8251ED48 cmpwi -1 / 0x8251ED50 cmpwi 0xF).
            const bool lbAnnounceLostRoads =
                (mpGuiCache->meGameModeType == BrnGameState::GameStateModuleIO::E_MODE_NONE) ||
                (mpGuiCache->meGameModeType == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY);

            if (lbAnnounceLostRoads)
            {
                GuiHudMessage lMessage;
                if (mRoadRuleHighScoreData.miNumScoresLost == 1)
                {
                    lMessage.Construct("RRLost1Road");
                }
                else
                {
                    lMessage.Construct("RRLostXRoads");
                    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 0,
                                      mRoadRuleHighScoreData.miNumScoresLost);
                }
                TriggerMessage(&lMessage);
            }
        }
        else
        {
            GuiHudMessage lMessage;
            lMessage.Construct("RRNewScores");
            TriggerMessage(&lMessage);
        }

        // ---- the shared "you now rule N roads" tail (copy 1 of 2) --------------------
        // The X360 emitted the MESSAGE-BUILDING block twice -- an inlined private helper --
        // but the two call sites GATE it differently (see the corrected FLAG on copy 2).
        // Here the gate is the count (`0x8251EDC4 lwz 0x490 ; cmpwi 0 ; ble -> exit`), with
        // no mbWasRulePlayersBefore test; this leg falls straight into it.
        // AGENTS.md would have the shared block de-inlined into a private helper
        // (`TriggerRoadsRuledMessage()`, the declaration requested of this header's owner);
        // until that lands it is reproduced at both sites. When it does land, the GATES stay
        // at the call sites -- they are not part of the helper.
        if (mRoadRuleHighScoreData.miNumRoadsNowRuled > 0)
        {
            GuiHudMessage lRuledMessage;
            if (mRoadRuleHighScoreData.miNumRoadsNowRuled == 1)
            {
                lRuledMessage.Construct("RRYouRule1");
            }
            else
            {
                lRuledMessage.Construct("RRYouRuleX");
                lRuledMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 0,
                                       mRoadRuleHighScoreData.miNumRoadsNowRuled);
            }
            TriggerMessage(&lRuledMessage);
        }
        return;
    }

    // ---- single-score line -------------------------------------------------------------
    // The road's localisation key is its compressed id in decimal. The buffer is 16 bytes on
    // the stack but SPrintf is given 12 and the terminator is written at [12] by hand
    // (X360 `li r4, 0xC` @0x8251EDF4 then `stb r10, var_A04` @0x8251EE10 -- var_A10 + 12).
    char lacRoadStringId[16];
    CgsCore::SPrintf(lacRoadStringId, 12, "%llu", mRoadRuleHighScoreData.mRoadId);
    lacRoadStringId[12] = 0;

    if (mRoadRuleHighScoreData.mbIsLocalPlayer)
    {
        // "You set a new <time|crash> record" -- the ...Off variants are the online-loss /
        // offline-win case (the score only stands offline).
        const char* lpcMessageId = NULL;
        if (mRoadRuleHighScoreData.mbOnlineLossButOfflineWin)
        {
            if (mRoadRuleHighScoreData.meScoreType == BrnStreetData::E_SCORE_TYPE_TIME)
                lpcMessageId = "RRTimeYouOff";
            else if (mRoadRuleHighScoreData.meScoreType == BrnStreetData::E_SCORE_TYPE_CRASH)
                lpcMessageId = "RRCrshYouOff";
            else
                return;   // faithful: any other score type fires nothing at all
        }
        else
        {
            if (mRoadRuleHighScoreData.meScoreType == BrnStreetData::E_SCORE_TYPE_TIME)
                lpcMessageId = "RRTimeYou";
            else if (mRoadRuleHighScoreData.meScoreType == BrnStreetData::E_SCORE_TYPE_CRASH)
                lpcMessageId = "RRCrashYou";
            else
                return;
        }

        GuiHudMessage lMessage;
        lMessage.Construct(lpcMessageId);
        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 1, lacRoadStringId);
        TriggerMessage(&lMessage);

        if (mRoadRuleHighScoreData.mbIsWholeRoadOwned)
        {
            GuiHudMessage lRoadMessage;
            lRoadMessage.Construct("RRRoadYou");
            lRoadMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 1, lacRoadStringId);
            TriggerMessage(&lRoadMessage);
        }
        return;
    }

    // Somebody else took the record. The "Beat" variants are used when the road was ruled by
    // the local player before (i.e. the player just LOST it).
    const char* lpcMessageId = NULL;
    if (mRoadRuleHighScoreData.mbWasRulePlayersBefore)
    {
        if (mRoadRuleHighScoreData.meScoreType == BrnStreetData::E_SCORE_TYPE_TIME)
            lpcMessageId = "RRTimeBeat";
        else if (mRoadRuleHighScoreData.meScoreType == BrnStreetData::E_SCORE_TYPE_CRASH)
            lpcMessageId = "RRCrashBeat";
        else
            return;
    }
    else
    {
        if (mRoadRuleHighScoreData.meScoreType == BrnStreetData::E_SCORE_TYPE_TIME)
            lpcMessageId = "RRTimeX";
        else if (mRoadRuleHighScoreData.meScoreType == BrnStreetData::E_SCORE_TYPE_CRASH)
            lpcMessageId = "RRCrashX";
        else
            return;
    }

    {
        GuiHudMessage lMessage;
        lMessage.Construct(lpcMessageId);
        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1,
                          mRoadRuleHighScoreData.mPlayerName.macName);
        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 2, lacRoadStringId);
        TriggerMessage(&lMessage);
    }

    if (mRoadRuleHighScoreData.mbIsWholeRoadOwned)
    {
        GuiHudMessage lRoadMessage;
        lRoadMessage.Construct("RRRoadX");
        lRoadMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1,
                              mRoadRuleHighScoreData.mPlayerName.macName);
        lRoadMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 2, lacRoadStringId);
        TriggerMessage(&lRoadMessage);
    }

    // ---- the shared "you now rule N roads" tail (copy 2 of 2) --------------------------
    // FLAG (console asymmetry, reproduced -- CORRECTED [gateui r3]). The two copies of this
    // tail gate DIFFERENTLY, and the difference is the opposite way round from what the
    // round-2 banner claimed. From the asm, verbatim:
    //   copy 1 (multi-score leg): reached by FALLTHROUGH from 0x8251EDC0 -- no
    //     mbWasRulePlayersBefore test at all -- and gated on the count:
    //     `0x8251EDC4 lwz r11,0x490 ; 0x8251EDC8 cmpwi r11,0 ; 0x8251EDCC ble -> exit`.
    //   copy 2 (this one, single-score / not-local leg): gated on the FLAG and NOT on the
    //     count: `0x8251EFB0 lbz r11,0x4A6 ; 0x8251EFB8 beq -> exit`, then
    //     `0x8251EFBC lwz r11,0x490` is loaded only to pick the singular/plural line
    //     (`0x8251EFC4 cmpwi r11,1`) -- there is no `> 0` branch anywhere on this path.
    // So a count of 0 (or negative) reaching here fires "RRYouRuleX" with the parameter 0:
    // another player took the local player's LAST ruled road. Round 2 carried an extra
    // `&& miNumRoadsNowRuled > 0` conjunct here, which removed that console
    // branch; it is removed. Do not re-add it, and if this tail is ever de-inlined into the
    // requested TriggerRoadsRuledMessage() helper, the helper must NOT carry the `> 0` test
    // -- it belongs to copy 1's call site only.
    if (mRoadRuleHighScoreData.mbWasRulePlayersBefore)
    {
        GuiHudMessage lRuledMessage;
        if (mRoadRuleHighScoreData.miNumRoadsNowRuled == 1)
        {
            lRuledMessage.Construct("RRYouRule1");
        }
        else
        {
            lRuledMessage.Construct("RRYouRuleX");
            lRuledMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 0,
                                   mRoadRuleHighScoreData.miNumRoadsNowRuled);
        }
        TriggerMessage(&lRuledMessage);
    }
}

// @ 0x82520078
// The deferred freeburn-challenge-end announce (Update parks the 24-byte event in
// mChallengedEndedData via HandleChallengeEnded @0x824F33E0 and raises mbChallengeEnded;
// this fires it and lowers the flag).
//
// The message id comes from KA_CHALLENGE_END_MESSAGE_IDS keyed on the status, and a ZERO
// entry is a real outcome, not an error: the flag is cleared and nothing is shown. The
// availability pre-check is genuine console behaviour -- the analyzer asks the director
// IsMessageAllowed BEFORE building the message and, if the answer is no, returns WITHOUT
// clearing mbChallengeEnded, so the announce retries on the next Update (0x82520178 branches
// past the `stb 0, 0x458`). Do not hoist the clear.
void HudMessageAnalyzer::TriggerChallengeEndedMessage()
{
    CGS_ASSERT_AT(mbChallengeEnded == true, "mbChallengeEnded == true", CGS_ARTIST_FILE, 5552);   // cpp:5552

    // [gateui r3] THE CONSOLE TWO-HOP, RESTORED. The X360 inlines
    //     mpGuiCache->GetWorldDataController()->GetFreeburnChallengeList()
    // -- the BrnGuiCache.h:2324 "mpWorldDataController" assert (`lwz r11, 0x4064`) followed
    // by the WorldDataController+0x4A8 read. Round 2 could not write it because
    // BrnGuiWorldDataController.h typed that accessor's return as `const BrnGui::ChallengeList*`,
    // a phantom defined nowhere in the tree, and routed through GuiCache::
    // GetFreeburnChallengeList @0x8240F018 instead (the same two hops plus one extra
    // non-gating null assert). That header now names the real BrnResource::ChallengeList, so
    // the console's own shape is back -- and it links out of two MOUNTED TUs (BrnGuiCache.cpp,
    // BrnGuiWorldDataController.cpp) rather than the unmounted BrnGuiCache_wB_01.cpp.
    const BrnResource::ChallengeList* lpChallengeList =
        mpGuiCache->GetWorldDataController()->GetFreeburnChallengeList();
    CGS_ASSERT_AT(lpChallengeList != NULL, "lpChallengeList", CGS_ARTIST_FILE, 5558);            // cpp:5558

    const CgsID lMessageId =
        KA_CHALLENGE_END_MESSAGE_IDS[mChallengedEndedData.meChallengeStatus];

    if (lMessageId == 0)
    {
        mbChallengeEnded = false;
        return;
    }

    if (!mpGuiCache->GetHudMessageDirector()->IsMessageAllowed(lMessageId))
        return;   // still pending -- mbChallengeEnded deliberately NOT cleared

    GuiHudMessage lMessage;
    lMessage.Construct(lMessageId);

    switch (mChallengedEndedData.meChallengeStatus)
    {
        case BrnGameState::E_CHALLENGE_STATUS_SUCCESS:
            // "Challenge complete <done>/<total>" -- BOTH counts on display string 1
            // (`li r5,1` at 0x825201D0 and 0x825201E4), the same two-params-one-string idiom
            // HandleStuntInfo uses.
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 1,
                              mChallengedEndedData.miNumChallengesComplete);
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 1,
                              mChallengedEndedData.miTotalNumChallenges);
            break;

        case BrnGameState::E_CHALLENGE_STATUS_ABORTED:
            // FLAG (console oddity, reproduced verbatim): the abort line takes an EMPTY
            // string id -- the X360 loads the shared empty rodata byte unk_820046A7 and
            // still calls AddParam (0x82520220). The reason slot is simply left blank here.
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 1, "");
            break;

        case BrnGameState::E_CHALLENGE_STATUS_ABORTED_DUE_TO_PLAYER_LEAVE:
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 1,
                              "CHALLENGE_ABORT_PLAYER_LEFT");
            break;

        case BrnGameState::E_CHALLENGE_STATUS_FAILURE:
            // The failure line names the challenge itself.
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 1,
                              lpChallengeList->GetChallengeData(mChallengedEndedData.mChallengeID)
                                  ->GetTitleStringID());
            break;

        default:
            // Reachable only for a status whose table slot is non-zero, i.e. a value past the
            // 7-entry table (the three in-range zero slots are filtered above). The console
            // streams the offending status into the assert buffer and, crucially, returns
            // WITHOUT firing and WITHOUT clearing the flag.
            CGS_ASSERT(false, "Flag should not be set to trigger this in this state");  // cpp:5599
            return;
    }

    // Non-gating tripwires re-evaluated by the console on EVERY arm, not just the failure one
    // (0x82520238 / 0x82520268 call GetChallengeData again after the switch); the second is
    // the compiler's `GetTitleStringID() != NULL` rendered as `entry + 0xB0 != 0`.
    CGS_ASSERT_AT(lpChallengeList->GetChallengeData(mChallengedEndedData.mChallengeID) != NULL,
               "lpChallengeList->GetChallengeData( mChallengedEndedData.mChallengeID )", CGS_ARTIST_FILE, 5604);   // cpp:5604
    CGS_ASSERT_AT(lpChallengeList->GetChallengeData(mChallengedEndedData.mChallengeID)
                   ->GetTitleStringID() != NULL,
               "lpChallengeList->GetChallengeData( mChallengedEndedData.mChallengeID )->GetTitleStringID()", CGS_ARTIST_FILE, 5605);  // cpp:5605

    TriggerMessage(&lMessage);
    mbChallengeEnded = false;
}

// ===========================================================================================
// [gateui r3] The four handlers round 2 parked. Each is dispatched by the committed fan-out
// in BrnGuiHudMessageAnalyzer_wB_12.cpp :: Update, so each was an LNK2019 standing between
// this tree and a mountable HudMessageAnalyzer.
// ===========================================================================================

// @ 0x8251D570
// "You drove through a <drive-thru>" -- one line per GuiDriveThroughEvent::DriveThroughType,
// picked out of three tables:
//   * mbEffective == false                -> KPAC_DRIVE_THROUGH_INEFFECTIVE_MESSAGES
//   * effective, type not CAR_WASH/PAINT  -> KAPC_DRIVE_THROUGH_MESSAGES
//   * effective CAR_WASH or PAINT_SHOP    -> a 1-in-KU_FREQUENCY_OF_DRIVETHROUGH_MAGIC_MESSAGES
//                                            draw between the MAGIC table and the basic one.
// The console's branch shape is `if (type != 0 && type != 2) { … } else { … }` with the
// mbEffective test duplicated inside both arms (0x8251D66C..0x8251D6AC); it is re-expressed
// here as the three cases above, which is the same predicate lattice with the duplication
// removed. CAR_WASH(0) and PAINT_SHOP(2) are exactly the two types with a MAGIC entry.
//
// INLINING REVERSAL (the draw): the console inlines the whole thing --
//     0x8251D6D8  ld    r11, 0x30(r22)     ; mRandom.muSeed, the OLD value
//     0x8251D6DC  srdi  r8, r11, 32        ; the draw is the PRE-step high word
//     0x8251D6E0  mulld r11, r11, r10      ; * 0x5851F42D4C957F2D
//     0x8251D6E4  addi  r11, r11, 1
//     0x8251D6F0  std   r11, 0x30(r22)
//     0x8251D6EC  mulhwu r9, r10, 0xCCCCCCCD ; srwi 2 ; slwi/add/subf  -> an UNSIGNED % 5
// which is `CgsNumeric::Random::RandomInt(0, 4)` with both bounds constant-folded: RandomInt
// (bodied in the mounted GameShared/GameClasses/Numeric/CgsRandom.cpp) is
// `liMin + (u32)(RandomUInt() % (u32)(liMax - liMin + 1))` -- pre-step draw, unsigned
// reduction, no ring-buffer touch -- and at (0, 4) its two baked asserts are constant-true
// and its `divwu` strength-reduces to exactly the magic multiply above. The PS3 DWARF
// (BrnGuiHudMessageAnalyzer.cpp:3178) confirms the source shape: a local `int32_t
// liRandomNumber` filled by a RandomInt call. The local keeps the DWARF's name; the bound is
// written as the existing class constant so the 1-in-N reads where the N lives.
//
// FLAG (console defect, reproduced): three MAGIC slots and three INEFFECTIVE slots are NULL in
// the X360 image and the Construct is NOT guarded against a null id. Every NULL slot is
// unreachable through this function's own branch structure (see the table definitions in
// BrnGuiHudMessageAnalyzer_wB_res.cpp), so adding a guard would add behaviour the binary does
// not have. Left ungated deliberately.
void HudMessageAnalyzer::HandleDriveThrough(const GuiDriveThroughEvent* lpEvent)
{
    // Non-gating tripwires (cpp:3301 / cpp:3302; the first message text really does say
    // "impact" -- copied from the sibling impact handler in the original).
    CGS_ASSERT(lpEvent != NULL, "Invalid impact message");
    CGS_ASSERT(lpEvent->meDriveThroughType < GuiDriveThroughEvent::E_DRIVE_THROUGH_TYPE_COUNT,
               "Drive Through type invalid");

    // The two types that have a "magic" variant, i.e. the two the console excludes from the
    // straight table lookup (`cmpwi r11,0` / `cmpwi r11,2` @0x8251D670/0x8251D678).
    const bool lbTypeHasMagicVariant =
        (lpEvent->meDriveThroughType == GuiDriveThroughEvent::E_DRIVE_THROUGH_TYPE_CAR_WASH) ||
        (lpEvent->meDriveThroughType == GuiDriveThroughEvent::E_DRIVE_THROUGH_TYPE_PAINT_SHOP);

    const char* lpcMessageId = NULL;

    if (!lpEvent->mbEffective)
    {
        lpcMessageId = KPAC_DRIVE_THROUGH_INEFFECTIVE_MESSAGES[lpEvent->meDriveThroughType];
    }
    else if (!lbTypeHasMagicVariant)
    {
        lpcMessageId = KAPC_DRIVE_THROUGH_MESSAGES[lpEvent->meDriveThroughType];
    }
    else
    {
        const s32 liRandomNumber =
            mRandom.RandomInt(0, static_cast<s32>(KU_FREQUENCY_OF_DRIVETHROUGH_MAGIC_MESSAGES) - 1);

        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "Drive through number : " << liRandomNumber << "\n";

        if (liRandomNumber != 0)
        {
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "Basic Drive Through\n";
            lpcMessageId = KAPC_DRIVE_THROUGH_MESSAGES[lpEvent->meDriveThroughType];
        }
        else
        {
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "Special Drive Through\n";
            lpcMessageId = KAPC_DRIVE_THROUGH_MAGIC_MESSAGES[lpEvent->meDriveThroughType];
        }
    }

    GuiHudMessage lMessage;
    lMessage.Construct(lpcMessageId);
    TriggerMessage(&lMessage);
}

// @ 0x824F2FB0
// Online-event finisher: latch the WINNER's gamer tag so the HUD can announce it. Nothing is
// triggered from here -- the message itself is fired later off mbOnlineWinnerWaiting.
// Three gates, in the console's order:
//   * the whole body is inside `if (GuiCache +0x4B4C)` -- the "this is an online event"
//     discriminator the tree spells IsOnlineStartInProgress()
//   * only the winner counts (miFinishPosition == 1)
//   * a car that has dropped out is not announced (GuiCache::IsActiveRaceCarDisconnected)
// The name resolve seeds its out-flag TRUE and the flag gates ONLY the waiting latch -- the
// PlayerName is constructed from whatever GetOnlineName returned either way
// (`bl PlayerName::Construct` @0x824F3280 precedes the `lbz`/`beq` on the flag). Reproduced.
void HudMessageAnalyzer::HandleEventFinisher(const GuiInEventFinisher* lpEvent)
{
    CGS_ASSERT_AT(lpEvent != NULL, "lpEvent", CGS_ARTIST_FILE, 3892);   // cpp:3892

    if (!mpGuiCache->IsOnlineStartInProgress())
        return;

    // The console's debug dump of the whole disconnected table, unrolled eight times in the
    // image (`ori r10, r10, 0xA0F4` .. `0xA0FB`, "," between and "\n\n" after the last),
    // re-rolled here. Read through the accessor rather than the raw byte the console loads.
    if (CgsDev::Message::gxMessageFilterFlags & 1)
    {
        *CgsDev::Log::gpDebugPrint << "Handling Event Finisher in hud message analyzer ::\n";
        *CgsDev::Log::gpDebugPrint << "Disconnected :";

        for (s32 liIndex = 0; liIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liIndex)
        {
            const EActiveRaceCarIndex leIndex = static_cast<EActiveRaceCarIndex>(liIndex);
            *CgsDev::Log::gpDebugPrint
                << " "
                << static_cast<s32>(mpGuiCache->IsActiveRaceCarDisconnected(leIndex) ? 1 : 0)
                << ((liIndex == (E_ACTIVE_RACE_CAR_INDEX_COUNT - 1)) ? "\n\n" : ",");
        }
    }

    if (lpEvent->miFinishPosition != 1)
        return;

    if (mpGuiCache->IsActiveRaceCarDisconnected(lpEvent->meActiveRaceCarIndex))
        return;

    bool lbNameValid = true;
    const char* lpcOnlineName = GetOnlineName(lpEvent->meActiveRaceCarIndex, &lbNameValid);
    mOnlineWinnerName.Construct(lpcOnlineName);

    if (lbNameValid)
        mbOnlineWinnerWaiting = true;
}

// @ 0x8251CDC0
// Online "X is battling Y" line: one message, two gamer tags, both on display string 0. The
// message is ABANDONED the moment a name fails to resolve -- the console re-reads the shared
// out-flag after each GetOnlineName and branches straight to the epilogue (0x8251D024 and
// 0x8251D054), so a half-filled message is never triggered. The first AddParam still runs
// with whatever the failed resolve returned; only the second call and the trigger are skipped.
void HudMessageAnalyzer::HandleNetworkBattling(const GuiNetworkPlayerBattlingEvent* lpEvent) const
{
    // Five non-gating tripwires, cpp:3045..3049. The two range pairs share a message text
    // each, exactly as the console does.
    CGS_ASSERT(lpEvent != NULL, "Invalid network battling message");
    CGS_ASSERT(0 <= lpEvent->meAggressorActiveRaceCarIndex, "Battling aggressor index invalid");
    CGS_ASSERT(lpEvent->meAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "Battling aggressor index invalid");
    CGS_ASSERT(0 <= lpEvent->meVictimActiveRaceCarIndex, "Battling victim invalid");
    CGS_ASSERT(lpEvent->meVictimActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "Battling victim invalid");

    bool lbNameValid = true;

    GuiHudMessage lMessage;
    lMessage.Construct("AggDrFghting");

    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0,
                      GetOnlineName(lpEvent->meAggressorActiveRaceCarIndex, &lbNameValid));
    if (!lbNameValid)
        return;

    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0,
                      GetOnlineName(lpEvent->meVictimActiveRaceCarIndex, &lbNameValid));
    if (!lbNameValid)
        return;

    TriggerMessage(&lMessage);
}

// @ 0x8251EA50
// Two unrelated jobs share this handler, split on mbWasChainJustCompleted (+0x17):
//
//   * the chain just ENDED -> announce the burnout chain, keyed on muNumChained (+0x00):
//       1        -> "BBBurnout1"  + an EMPTY string parameter (the console loads the shared
//                   empty rodata byte unk_820046A7 and still calls AddParam @0x8251EAD0)
//       2..99    -> "BBBurnouts"  + the count as an INT parameter
//       >= 100   -> "BBBurnoutWow", no parameters
//     (`cmplwi` at both compares -- muNumChained is unsigned.)
//
//   * otherwise -> the "your boost bar is full, you can boost now" one-shot tutorial line,
//     latched on mbBoostOkMessageJustTriggered (analyzer +0x456) so it fires once per fill:
//       meBoostType must be the danger/blue bar (`cmpwi r11,0` @0x8251EB3C -- any other bar
//       returns immediately); then, while the bar reads full and the player is NOT boosting
//       and has no chain running, fire "BBBoostOK" and raise the latch. The latch is lowered
//       again only on the not-full + boosting path (0x8251EBA0), i.e. when the player spends
//       the bar. Note the console RETURNS out of the whole mbBoostIsFull arm whether or not
//       the inner test passed (`cmplwi r11,0; bne` @0x8251EB98 re-tests the same byte it
//       already branched on), so a full bar can never fall through to the lowering path.
void HudMessageAnalyzer::HandleChainedBoost(const GuiEventBoostInfo* lpEvent)
{
    CGS_ASSERT(lpEvent != NULL, CGS_ASSERT_MESSAGE_AT("lpEvent", CGS_ARTIST_BRNGUIHUDMESSAGEANALYZER_FILE, 4519 /* ARTIST 0x8251EA8C */));   // cpp:4519

    if (lpEvent->mbWasChainJustCompleted)
    {
        GuiHudMessage lMessage;
        if (lpEvent->muNumChained == 1u)
        {
            lMessage.Construct("BBBurnout1");
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0, "");
        }
        else if (lpEvent->muNumChained >= 100u)
        {
            lMessage.Construct("BBBurnoutWow");
        }
        else
        {
            lMessage.Construct("BBBurnouts");
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 0,
                              static_cast<s32>(lpEvent->muNumChained));
        }
        TriggerMessage(&lMessage);
        return;
    }

    if (lpEvent->meBoostType != BrnWorld::E_BOOST_TYPE_DANGER)
        return;

    if (lpEvent->mbBoostIsFull)
    {
        if (!lpEvent->mbIsBoosting &&
            (lpEvent->muNumChained == 0u) &&
            !mbBoostOkMessageJustTriggered)
        {
            GuiHudMessage lMessage;
            lMessage.Construct("BBBoostOK");
            TriggerMessage(&lMessage);
            mbBoostOkMessageJustTriggered = true;
        }
        return;
    }

    if (!lpEvent->mbIsBoosting)
        return;

    mbBoostOkMessageJustTriggered = false;
}

} // namespace BrnGui

// FOLDED FROM BrnGuiHudMessageAnalyzer_gUI_04.cpp
#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"   // HudMessageAnalyzer + the KAPC_* rodata tables
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"         // GuiHudMessage (Construct / AddParam)
#include "GameSource/Gui/BrnGuiCache.h"                 // GuiCache accessors the six handlers read
#include "GameSource/GameState/BrnGameActions.h"        // BrnWorld::EPowerParkOutcome (E_PPO_SUCCESS / _FAILURE)

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"      // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h" // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiHudMessage.h"  // CgsGui::E_HUDMESSAGEPARAMTYPES_*

// =====================================================================================
// BrnGui::HudMessageAnalyzer -- reconstructed from BURNOUT_X360_ARTIST.XEX
// (gateui wave round-2 partfile 04, owner guihand2). Six more of the wave-B fan-out's
// declaration-only arms; each was an LNK2019 standing between this tree and a mountable
// HudMessageAnalyzer:
//
//   HandleBurningHomeRunRunnerCrashes     @0x8251B440  (114 insns, cpp:1925-1928)
//   HandleBurningHomeRunCheckpointReached @0x8251B1D0  ( 95 insns, cpp:1829-1832)
//   HandleEventLeaderSplitTime            @0x8251E640  ( 84 insns, cpp:3785)
//   HandlePowerParkingResult              @0x8251F330  ( 74 insns, cpp:4989/4998/5007)
//   HandleEventDistanceToFinish           @0x8251E550  ( 59 insns, cpp:3758/3759)
//   HandleRaceCheckpointReached           @0x8251B350  ( 59 insns, cpp:1882/1883)
//
// -------------------------------------------------------------------------------------
// [gateui r3] THE TWO PREPROCESSOR GATES ARE GONE -- these six bodies are LIVE.
// -------------------------------------------------------------------------------------
// Round 2 shipped this partfile behind `BRN_GUI_GUI04_PAYLOADS` /
// `BRN_GUI_GUI04_CACHE_MEMBERS` because all six handlers read members of GUI-event payload
// structs that were still opaque placeholders in a header round 2's ownership rules put in
// another owner's lane. Neither macro was ever defined anywhere in the tree, so the TU
// compiled to an EMPTY object: gate-green, six LNK2019s still open, nothing diagnosing it
// (verify_r2_guihand2/VERDICT.md finding 2).
//
// Round 3 landed both dependencies and deleted both `#if`s in the same change:
//   * the six payload structs now carry their real DWARF field sets in
//     BrnGuiEventTypeDefs.h (each with its own GetEventType() -- the member
//     CgsGui::GuiModule::AddGuiEvent<T> bakes -- plus the house sizeof/offsetof pins), and
//     their opaque twins were DELETED from BrnGuiDemangledEventTypes.h, so there is exactly
//     one definition of each name and no ODR split with CgsGuiModule_AddGuiEvent_Inst.cpp;
//   * GuiCache carries the two members HandleRaceCheckpointReached reads
//     (+0x4B04 mePlayerGlobalRaceCarIndex / +0x5286 muNumActiveLandmarks), carved out of
//     mPad_4B04 / mPad_5284 without shifting anything, with the two accessors used below.
// Prove it the way the verdict asked: `dumpbin /SYMBOLS` this object and look for six
// SECT-External `Handle*` rows. No row means the file went dark again.
// =====================================================================================

// Asserts pass __FILE__; ARTIST's reads "..\\..\\..\\GameSource\\Gui/BrnGuiHudMessageAnalyzer.cpp".

namespace BrnGui
{


namespace
{
    // X360 rodata flt_82013F90 (0x8251E610) -- the metres-to-kilometres factor the
    // distance-to-finish line bakes in. Same constant the round-1 KM boundary handler
    // uses; kept file-local per partfile, not hoisted into a shared header.


    // Power-parking rating bands: the console divides the 0..99 overall rating by a
    // literal 20 (`li r10,0x14; divw` @0x8251F380) and asserts the resulting index is
    // below 6 ("luRatingIndex<6", cpp:4998).
    const s32 KI_POWER_PARK_RATING_BAND_SIZE = 20;
    const u32 KU_POWER_PARK_RATING_COUNT     = 6;
}

// -------------------------------------------------------------------------------------
// @ 0x8251B1D0
// Burning-Home-Run "landmark reached" chatter. Two message families: "your" lines when
// the event's car is the local player's, "their" lines otherwise; within each family the
// "...Lmk1" variant is the singular ("1 landmark to go") and "...LdMk" the plural.
//
// The remote branch resolves the crasher's online name FIRST and appends it as STRING
// parameter of display string 0; if the name did NOT resolve the console abandons the
// whole message (`lbz var_380; beq` @0x8251B318 skips both the count parameter and
// TriggerMessage). The local branch has no name to resolve and drops straight into the
// shared tail -- which is why the compiler shares one epilogue between them.
//
// PITFALL (round-1's, honoured here): GetOnlineName is overloaded. The console calls the
// EActiveRaceCarIndex overload @0x824ECE10 (`lwz r4,0(r30)` = meActiveRaceCarIndex), so
// the argument must stay enum-typed -- an s32 would silently bind the player-id overload.
// -------------------------------------------------------------------------------------
void HudMessageAnalyzer::HandleBurningHomeRunCheckpointReached(
        const GuiBHRCheckpointReachedEvent* lpEvent)
{
    // Non-gating tripwires (cpp:1829-1832, in the console's order).
    CGS_ASSERT(lpEvent != NULL, CGS_ASSERT_MESSAGE_AT("lpEvent", __FILE__, 1829 /* ARTIST 0x8251B208 */));
    CGS_ASSERT(lpEvent->meActiveRaceCarIndex >= 0, CGS_ASSERT_MESSAGE_AT("lpEvent->meActiveRaceCarIndex >= 0", __FILE__, 1830 /* ARTIST 0x8251B230 */));
    CGS_ASSERT(lpEvent->meActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               CGS_ASSERT_MESSAGE_AT("lpEvent->meActiveRaceCarIndex < BrnWorld::KI_MAX_ACTIVE_RACE_CARS", __FILE__, 1831 /* ARTIST 0x8251B258 */));
    CGS_ASSERT(mpGuiCache != NULL, CGS_ASSERT_MESSAGE_AT("mpGuiCache", __FILE__, 1832 /* ARTIST 0x8251B280 */));

    GuiHudMessage lMessage;

    if (static_cast<s32>(lpEvent->meActiveRaceCarIndex) == mpGuiCache->GetPlayerActiveRaceCarIndex())
    {
        // One Construct per arm in both branches (ARTIST 0x8251B28C hoists r3 = &lMessage
        // above the whole test and emits a call per arm: if/else, not a ternary argument).
        if (lpEvent->miNumCheckpointsToGo == 1)
        {
            lMessage.Construct("OnlBHRYrLmk1");
        }
        else
        {
            lMessage.Construct("OnlBHRYrLdMk");
        }
    }
    else
    {
        if (lpEvent->miNumCheckpointsToGo == 1)
        {
            lMessage.Construct("OnlBHRTrLmk1");
        }
        else
        {
            lMessage.Construct("OnlBHRTrLdMk");
        }

        bool lbNameValid = true;   // the X360 seeds the out-flag true before the call
        const char* lpcOnlineName = GetOnlineName(lpEvent->meActiveRaceCarIndex, &lbNameValid);
        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0, lpcOnlineName);

        if (!lbNameValid)
            return;
    }

    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 0, lpEvent->miNumCheckpointsToGo);
    TriggerMessage(&lMessage);
}

// -------------------------------------------------------------------------------------
// @ 0x8251B440
// The Burning-Home-Run runner's crash budget ("N crashes left"). Same local/remote and
// singular/plural split as the landmark handler above, but the plural line carries the
// count as an INT parameter of display string 1 while the singular line carries none.
//
// FLAG (console defect, reproduced verbatim): NEITHER branch has an arm for
// miNumCrashesToGo <= 0. The compiler proves it -- @0x8251B550 `bne cr6, loc_8251B5F4`
// jumps a zero/negative count straight to TriggerMessage, and @0x8251B5A8 `bne cr6,
// loc_8251B5BC` does the same on the remote side -- so the message is triggered with an
// UNCONSTRUCTED GuiHudMessage (a stack-garbage id hash and garbage parameter counts).
// This is the shipped shape of the source (an if/else-if with no else, followed by an
// unconditional TriggerMessage); do not "fix" it by adding a guard. On the host that
// means the same uninitialised read, so if the BHR mode ever posts a zero count the
// director will look up a garbage hash -- worth a probe when BHR first runs, not a
// silent divergence here.
// -------------------------------------------------------------------------------------
void HudMessageAnalyzer::HandleBurningHomeRunRunnerCrashes(
        const GuiHUDMessageBHRRunnerCrashed* lpEvent)
{
    // Non-gating tripwires (cpp:1925-1928).
    CGS_ASSERT(lpEvent != NULL,
        CGS_ASSERT_MESSAGE_AT("lpEvent", __FILE__, 1925 /* ARTIST 0x8251B46C */));
    CGS_ASSERT(lpEvent->meActiveRaceCarIndex >= 0,
        CGS_ASSERT_MESSAGE_AT(
            "lpEvent->meActiveRaceCarIndex >= 0",
            __FILE__, 1926 /* ARTIST 0x8251B494 */));
    CGS_ASSERT(lpEvent->meActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
        CGS_ASSERT_MESSAGE_AT(
            "lpEvent->meActiveRaceCarIndex < BrnWorld::KI_MAX_ACTIVE_RACE_CARS",
            __FILE__, 1927 /* ARTIST 0x8251B4BC */));
    CGS_ASSERT(mpGuiCache != NULL,
        CGS_ASSERT_MESSAGE_AT("mpGuiCache", __FILE__, 1928 /* ARTIST 0x8251B4E4 */));

    GuiHudMessage lMessage;   // FLAG: deliberately left unconstructed on the <= 0 arms

    if (static_cast<s32>(lpEvent->meActiveRaceCarIndex) == mpGuiCache->GetPlayerActiveRaceCarIndex())
    {
        if (lpEvent->miNumCrashesToGo > 1)
        {
            lMessage.Construct("OnlBHRCrLp2");
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 1, lpEvent->miNumCrashesToGo);
        }
        else if (lpEvent->miNumCrashesToGo == 1)
        {
            lMessage.Construct("OnlBHRCrLp1");
        }

        TriggerMessage(&lMessage);
    }
    else
    {
        if (lpEvent->miNumCrashesToGo > 1)
        {
            lMessage.Construct("OnlBHRCrRp2");
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 1, lpEvent->miNumCrashesToGo);
        }
        else if (lpEvent->miNumCrashesToGo == 1)
        {
            lMessage.Construct("OnlBHRCrRp1");
        }

        bool lbNameValid = true;
        const char* lpcOnlineName = GetOnlineName(lpEvent->meActiveRaceCarIndex, &lbNameValid);
        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1, lpcOnlineName);

        if (lbNameValid)
            TriggerMessage(&lMessage);
    }
}

// -------------------------------------------------------------------------------------
// @ 0x8251E640
// The in-event split-time / checkpoint line. Three shapes off one gate:
//   offline  -> "EventCheckPt" + (checkpoints reached, checkpoints in event)
//   online, local player leading -> "EventSpltYou" + the lead time
//   online, a rival leading      -> "EventSpltRvl" + the leader's gamer tag
//
// FLAG (console oddity, reproduced verbatim): the RIVAL arm never fires its message. The
// X360 constructs a SECOND, separate GuiHudMessage (stack slot var_360, distinct from the
// var_6B0 the other two arms share), hands it to AddGamerTagToMessage, and then branches
// straight to the epilogue -- `bl AddGamerTagToMessage` @0x8251E6C4 is followed by
// `b loc_8251E778` @0x8251E6C8, with no TriggerMessage anywhere on that path. Two distinct
// stack objects is the tell that the source really did declare a second local here rather
// than the decompiler aliasing one; the missing trigger is a shipped bug, not a transcription
// slip. Kept as-is, with the second local kept distinct so the shape stays legible.
//
// The lead time rides as an INT: `lfs f0,8(lpEvent); fctiwz; stfiwx` @0x8251E6E0 truncates
// toward zero before the INT AddParam -- it is not a FLOAT parameter.
//
// FLAG (naming, out of this lane -- see the report): the online gate is the byte at
// GuiCache +0x4B4C, which this tree currently names mbOnlineStartInProgress /
// IsOnlineStartInProgress(). Both this handler and HandleRaceCheckpointReached read it as
// a plain "this is an online event" discriminator, which that name does not describe.
// Coded against the existing accessor; the rename is a request, not an edit.
// -------------------------------------------------------------------------------------
void HudMessageAnalyzer::HandleEventLeaderSplitTime(const GuiInEventLeaderSplit* lpEvent) const
{
    CGS_ASSERT_AT(lpEvent != NULL, "lpEvent", CGS_ARTIST_FILE, 3785);   // cpp:3785

    // Three messages (DWARF lMessage at cpp:3652 / :3667 / :3675): the online branch's own
    // message encloses the leading-player block's, so the rival message keeps a separate
    // stack slot (0x3B0) while the two triggered ones share 0x60; ARTIST cross-jumps their
    // shared AddParam + TriggerMessage tail.
    if (mpGuiCache->IsOnlineStartInProgress())
    {
        GuiHudMessage lMessage;
        if (!lpEvent->mbLocalPlayerIsLeading)
        {
            lMessage.Construct("EventSpltRvl");
            AddGamerTagToMessage(&lMessage, "TEMP_ONLINE_NAME",
                                 lpEvent->meLeaderActiveRaceCarIndex);
            // FLAG: the console never triggers this one -- see above.
        }
        else
        {
            GuiHudMessage lMessage;
            lMessage.Construct("EventSpltYou");
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 0,
                              static_cast<s32>(lpEvent->mfLeadTime));
            TriggerMessage(&lMessage);
        }
    }
    else
    {
        GuiHudMessage lMessage;
        lMessage.Construct("EventCheckPt");
        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 0,
                          mpGuiCache->GetCheckpointReached());
        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 0,
                          mpGuiCache->GetCheckpointsInEvent());   // u8 return, zero-extended
        TriggerMessage(&lMessage);
    }
}

// -------------------------------------------------------------------------------------
// @ 0x8251F330
// The power-parking (stunt-park) result popup. On success the overall rating picks one of
// six rating-name string ids by NUMERIC SUFFIX -- the console formats the id at run time
// with SPrintf rather than indexing a table, so there is no rodata table to recover here.
// Both the numeric rating and the formatted rating-name string id ride display string 1.
// On failure a single line, no parameters.
//
// The rating index is a SIGNED divide (`divw`) compared UNSIGNED (`cmplwi`) -- a negative
// rating would wrap past the assert rather than trip it. Reproduced by dividing as s32 and
// holding the result in a u32, which is what the original expression did.
// -------------------------------------------------------------------------------------
void HudMessageAnalyzer::HandlePowerParkingResult(const GuiPowerParkResult* lpEvent)
{
    CGS_ASSERT_AT(lpEvent != NULL, "lpEvent", CGS_ARTIST_FILE, 4989);   // cpp:4989

    GuiHudMessage lMessage;

    if (lpEvent->meOutcome == BrnWorld::E_PPO_SUCCESS)
    {
        const u32 luRatingIndex = lpEvent->miOverallRating / KI_POWER_PARK_RATING_BAND_SIZE;
        CGS_ASSERT_AT(luRatingIndex < KU_POWER_PARK_RATING_COUNT, "luRatingIndex<6", CGS_ARTIST_FILE, 4998);  // cpp:4998

        char lacRatingStringID[32];
        CgsCore::SPrintf(lacRatingStringID, sizeof(lacRatingStringID),
                         "HUDMESSAGE_POWERPARK_RATING%d", luRatingIndex);

        lMessage.Construct("PowPkWin");
        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 1, lpEvent->miOverallRating);
        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 1, lacRatingStringID);
        TriggerMessage(&lMessage);
    }
    else
    {
        CGS_ASSERT_AT(lpEvent->meOutcome == BrnWorld::E_PPO_FAILURE,
                   "lpEvent->meOutcome == BrnWorld::E_PPO_FAILURE", CGS_ARTIST_FILE, 5007);   // cpp:5007

        lMessage.Construct("PowPkLose");
        TriggerMessage(&lMessage);
    }
}

// -------------------------------------------------------------------------------------
// @ 0x8251E550
// "<Nth> -- N km from the finish". The ordinal comes from the already-recovered
// KAPC_FINISH_POSITION_MESSAGES table (X360 off_82F277E0, defined in
// BrnGuiHudMessageAnalyzer_wB_res.cpp): the console loads `lwzx r6, r10, r11` with r11
// biased by -4 (`lwz r6,-4(r11)` @0x8251E5F0), i.e. it indexes the table with
// miPlayerPosition - 1 -- a 1-based position against a 0-based table. Note the sibling
// HandleEliminatedEvent indexes the SAME table from slot 1 instead; the two conventions
// are genuinely different, do not unify them.
//
// The distance is metres, converted and TRUNCATED to an INT parameter (`fmuls` then
// `fctiwz` @0x8251E614), not passed as a FLOAT parameter.
// -------------------------------------------------------------------------------------
void HudMessageAnalyzer::HandleEventDistanceToFinish(const GuiInEventDistanceToFinish* lpEvent) const
{
    // Non-gating tripwires (cpp:3758/3759; the second is a single combined assert on the
    // console, message text verbatim including its spacing).
    CGS_ASSERT(lpEvent != NULL,
        CGS_ASSERT_MESSAGE_AT("lpEvent", __FILE__, 3758 /* ARTIST 0x8251E57C */));
    CGS_ASSERT((0 < lpEvent->miPlayerPosition) &&
               (E_ACTIVE_RACE_CAR_INDEX_COUNT >= lpEvent->miPlayerPosition),
        CGS_ASSERT_MESSAGE_AT(
            "(0 < lpEvent->miPlayerPosition) && ( E_ACTIVE_RACE_CAR_INDEX_COUNT >= lpEvent->miPlayerPosition)",
            __FILE__, 3759 /* ARTIST 0x8251E5AC */));

    GuiHudMessage lMessage;
    lMessage.Construct("EventDistFin");
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 0,
                      KAPC_FINISH_POSITION_MESSAGES[lpEvent->miPlayerPosition - 1]);
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 0,
                      static_cast<s32>(lpEvent->mfDistanceToFinish * KF_METRES_TO_KILOMETRES));
    TriggerMessage(&lMessage);
}


// -------------------------------------------------------------------------------------
// @ 0x8251B350
// The online-race "you passed landmark N of M" line. Three gates, all of which must hold
// before anything is shown (the console has no else arm at all):
//   * the event is online                            (GuiCache +0x4B4C)
//   * the event's GLOBAL car index is the local player's (GuiCache +0x4B04 -- NOT the
//     active index the BHR handlers compare; GuiCache::Construct seeds both to -1 and
//     GuiCache::RecEvent writes them as a pair from one event record)
//   * the checkpoint index is below the active-landmark count (GuiCache +0x5286, the byte
//     GuiCache::HandleSetActiveLandmarksEvent @0x824EE7D0 stores alongside the u16
//     landmark array at +0x5288)
// The displayed index is 1-based (`addi r6, r11, 1` @0x8251B400) over a 0-based member.
//
// [gateui r3] UNBLOCKED: both members are carved into BrnGuiCache.h this round
// (mePlayerGlobalRaceCarIndex / muNumActiveLandmarks) with the two accessors this body
// already called, so it now compiles against the real header set like its five siblings.
// -------------------------------------------------------------------------------------
void HudMessageAnalyzer::HandleRaceCheckpointReached(const GuiRaceCheckpointReached* lpEvent)
{
    // Non-gating tripwires (cpp:1882/1883 -- note the cache assert comes FIRST here,
    // the reverse of every other handler in this partfile).
    CGS_ASSERT(mpGuiCache != NULL,
        CGS_ASSERT_MESSAGE_AT("mpGuiCache", __FILE__, 1882 /* ARTIST 0x8251B380 */));
    CGS_ASSERT(lpEvent != NULL,
        CGS_ASSERT_MESSAGE_AT("lpEvent", __FILE__, 1883 /* ARTIST 0x8251B3A4 */));

    if (!mpGuiCache->IsOnlineStartInProgress())
        return;

    if (lpEvent->meGlobalRaceCarIndex != mpGuiCache->GetPlayerGlobalRaceCarIndex())
        return;

    if (lpEvent->miCheckpointIndex >= mpGuiCache->GetNumActiveLandmarks())
        return;

    GuiHudMessage lMessage;
    lMessage.Construct("OnlRacTrLdMk");
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 0, lpEvent->miCheckpointIndex + 1);
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 0,
                      mpGuiCache->GetNumActiveLandmarks());
    TriggerMessage(&lMessage);
}



} // namespace BrnGui

#undef CGS_ARTIST_FILE
#undef CGS_ARTIST_BRNGUIHUDMESSAGEANALYZER_FILE
