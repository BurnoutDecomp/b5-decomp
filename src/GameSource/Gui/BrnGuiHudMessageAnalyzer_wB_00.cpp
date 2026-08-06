#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"

#include <cstring>   // std::strlen (the generic-message title/subtitle guards)

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"         // CgsGui::GuiAccessPointers::mpLanguageManager
#include "GameShared/GameClasses/Language/CgsLanguageManager.h" // FormatAndAddText / E_FORMAT_ID_LOOKUP
#include "GameSource/Gui/BrnGuiCache.h"                      // GuiCache (frozen-header include set)
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"        // remaining opaque event placeholders

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
