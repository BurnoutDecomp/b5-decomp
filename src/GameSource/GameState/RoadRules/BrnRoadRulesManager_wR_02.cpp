// ---------------------------------------------------------------------------
// GameSource/GameState/RoadRules/BrnRoadRulesManager_wR_02.cpp
//
// RoadRulesManager, the per-frame half: Construct, and Update with the two helpers it drives
// (UpdateSwitchingRoadRulesOnOrOff, UpdateActiveRoadRule).
//
// Every body is read off the console code of the function it defines. The rule events it calls
// (OnStartRule, OnEndRule, OnShowtimeStart, SendActiveRuleState, OnUpdateActiveRoadScores,
// UpdateTimeRule) are defined in the other partfiles; OnEnterRoad and OnLeaveRoad in
// BrnRoadRulesManager.cpp.
// ---------------------------------------------------------------------------
#include "GameSource/GameState/RoadRules/BrnRoadRulesManager.h"
#include "GameSource/GameState/BrnGameStateModuleIO.h"                   // ControllerInput, OutputBuffer::GetGameActionQueue
#include "GameSource/GameState/BrnGameStateSharedIO.h"                   // EGameModeType, IsOnlineFreeBurnLobby, IsShowtimeGameMode
#include "GameSource/GameState/ModeManager/BrnModeManager.h"             // ModeManager accessors
#include "GameSource/GameState/Progression/BrnProgressionManager.h"      // ProgressionManager::AreRoadRulesAvailable / GetProfile
#include "GameSource/GameState/Progression/BrnProfile.h"                 // Profile::HasPlayerSeenTrainingType
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"     // TrainingManager::RequestTraining
#include "SharedClasses/Progression/BrnTrainingTypes.h"                  // BrnProgression::ETrainingType
#include "SharedClasses/StreetData/BrnStreetData.h"                      // KI_INVALID_ROAD_INDEX, ScoreType
#include "SharedClasses/DataLists/ChallengeListEntry.h"                  // EFreeburnChallengeStyle
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"               // the opt-in witness below
#include <cstdlib>                                                       // getenv

namespace BrnGameState
{

// Seconds the player may be off every road before the current road is dropped (read-only data,
// 15.0f in the image).
const f32 KF_IN_ROAD_TIMEOUT = 15.0f;

// Seconds without player input after which a running time rule is abandoned (read-only data,
// 5.0f in the image).
const f32 KF_ROAD_RULE_NO_INPUT_QUIT_TIME = 5.0f;

// Every member is set here except mfStuntRuleComboTimeout and miCrashScore. The debug component's
// own Construct is inlined at the head of the body.
void RoadRulesManager::Construct(StreetManager* lpStreetManager, ModeManager* lpModeManager,
                                 TrainingManager* lpTrainingManager)
{
    mRoadRulesDebugComponent.Construct(this);

    mpStreetManager   = lpStreetManager;
    mpModeManager     = lpModeManager;
    mpTrainingManager = lpTrainingManager;

    miNumWarningsDone = 0;
    mLastLimitId      = 0;

    mfTime             = 0.0f;
    mfTimeScoreTimeout = 0.0f;
    mfTimeTarget       = 0.0f;
    mfNextWarningTime  = 0.0f;
    mfStuntTime        = 0.0f;
    mfInRoadTimeout    = 0.0f;

    mbAllowExitRoadRulesAfterTimeout = true;
    mfExitRoadRulesTime              = 0.0f;

    miLastRoadIndex = BrnStreetData::KI_INVALID_ROAD_INDEX;
    for (s32 liChallengeIndex = 0; liChallengeIndex < BrnStreetData::E_SCORE_TYPE_COUNT; ++liChallengeIndex)
    {
        maiChallengeRoadIndex[liChallengeIndex] = BrnStreetData::KI_INVALID_ROAD_INDEX;
    }

    mePreviousActiveRoadRule = E_ACTIVE_ROAD_RULE_NONE;
    meActiveRoadRule         = E_ACTIVE_ROAD_RULE_NONE;
    mbRoadRulesNotAllowed    = false;
    mbSwitchingActive        = true;
    mbIsOnlineMode           = false;
}

// Road rules are suspended outside free roam (any mode but the online free-burn lobby and
// showtime), during car select, and while a normal free-burn challenge runs outside showtime.
// On the frame they become suspended, or when a crash rule's exit timer runs out, both rules end
// unscored and the rule selection drops to NONE. Returns (and latches) the suspended state.
bool RoadRulesManager::UpdateSwitchingRoadRulesOnOrOff(GameStateModuleIO::GameActionQueue*                 lpGameActionQueue,
                                                       GameStateModuleIO::OutputBuffer*                    lpOutputBuffer,
                                                       GameStateModuleIO::EGameModeType                    leGameModeType,
                                                       bool                                                lbCarSelectActive,
                                                       BrnResource::ChallengeListEntry::EFreeburnChallengeStyle leFreeburnChallengeStyle,
                                                       f32                                                 lfTimeStep,
                                                       bool                                                lbFreeburnActive)
{
    const bool lbRoadRulesNotAllowed =
        (leGameModeType != GameStateModuleIO::E_MODE_NONE &&
         !GameStateModuleIO::IsOnlineFreeBurnLobby(leGameModeType) &&
         !GameStateModuleIO::IsShowtimeGameMode(leGameModeType)) ||
        lbCarSelectActive ||
        (leFreeburnChallengeStyle == BrnResource::ChallengeListEntry::E_FREEBURN_STYLE_NORMAL &&
         !GameStateModuleIO::IsShowtimeGameMode(leGameModeType));

    bool lbTimeout = false;
    if (IsRoadRuleCrash(meActiveRoadRule) && !lbFreeburnActive)
    {
        mfExitRoadRulesTime -= lfTimeStep;

        // `bge` skips on an unordered compare: a NaN clock never times out, as `<` gives.
        if (mbAllowExitRoadRulesAfterTimeout && mfExitRoadRulesTime < 0.0f)
        {
            lbTimeout = true;
        }
    }

    if (lbTimeout || (lbRoadRulesNotAllowed && !mbRoadRulesNotAllowed))
    {
        if (IsRoadRuleTime(meActiveRoadRule))
        {
            OnEndRule(lpOutputBuffer, BrnStreetData::E_SCORE_TYPE_TIME, false);
        }
        if (IsRoadRuleCrash(meActiveRoadRule))
        {
            OnEndRule(lpOutputBuffer, BrnStreetData::E_SCORE_TYPE_CRASH, false);
        }

        meActiveRoadRule = E_ACTIVE_ROAD_RULE_NONE;
        SendActiveRuleState(lpGameActionQueue);
    }

    mbRoadRulesNotAllowed = lbRoadRulesNotAllowed;
    return lbRoadRulesNotAllowed;
}

// The start-event button steps the rule selection NONE -> offline time -> online time ->
// offline crash -> online crash -> NONE. A crash rule that is running holds the selection.
// COUNT is a pending "reset to NONE" that is applied without a button press.
void RoadRulesManager::UpdateActiveRoadRule(GameStateModuleIO::OutputBuffer*                    lpOutputBuffer,
                                            GameStateModuleIO::GameActionQueue*                 lpGameActionQueue,
                                            bool                                                lbStartEventPressed,
                                            bool                                                lbDisableSwitchingOnline,
                                            BrnResource::ChallengeListEntry::EFreeburnChallengeStyle leFreeburnChallengeStyle)
{
    if (!mbSwitchingActive || lbDisableSwitchingOnline ||
        leFreeburnChallengeStyle != BrnResource::ChallengeListEntry::E_FREEBURN_STYLE_NONE)
    {
        return;
    }

    if (!lbStartEventPressed && meActiveRoadRule != E_ACTIVE_ROAD_RULE_COUNT)
    {
        return;
    }

    switch (meActiveRoadRule)
    {
        case E_ACTIVE_ROAD_RULE_NONE:
            if (mpModeManager->GetProgressionManager()->AreRoadRulesAvailable() ||
                mpModeManager->IsOnlineGameMode())
            {
                meActiveRoadRule                 = E_ACTIVE_ROAD_RULE_OFFLINE_TIME;
                mbAllowExitRoadRulesAfterTimeout = false;
            }
            break;

        case E_ACTIVE_ROAD_RULE_OFFLINE_TIME:
            OnEndRule(lpOutputBuffer, BrnStreetData::E_SCORE_TYPE_TIME, false);
            mbAllowExitRoadRulesAfterTimeout = false;
            meActiveRoadRule                 = E_ACTIVE_ROAD_RULE_ONLINE_TIME;
            break;

        case E_ACTIVE_ROAD_RULE_ONLINE_TIME:
            OnEndRule(lpOutputBuffer, BrnStreetData::E_SCORE_TYPE_TIME, false);
            mbAllowExitRoadRulesAfterTimeout = false;
            meActiveRoadRule                 = E_ACTIVE_ROAD_RULE_OFFLINE_CRASH;
            break;

        case E_ACTIVE_ROAD_RULE_OFFLINE_CRASH:
            if (IsCrashRuleActive())
            {
                return;
            }
            meActiveRoadRule = E_ACTIVE_ROAD_RULE_ONLINE_CRASH;
            break;

        case E_ACTIVE_ROAD_RULE_ONLINE_CRASH:
            if (IsCrashRuleActive())
            {
                return;
            }
            meActiveRoadRule = E_ACTIVE_ROAD_RULE_NONE;
            break;

        case E_ACTIVE_ROAD_RULE_COUNT:
            meActiveRoadRule = E_ACTIVE_ROAD_RULE_NONE;
            break;

        default:
            CGS_ASSERT(false, "How did it end up here?");
            break;
    }

    SendActiveRuleState(lpGameActionQueue);

    if (lbStartEventPressed)
    {
        if (GameStateModuleIO::IsOnlineFreeBurnLobby(mpModeManager->GetCurrentGameModeType()))
        {
            mpTrainingManager->RequestTraining(BrnProgression::E_TRAINING_TYPE_ACTIVATED_ROADRULES_ONLINE);
        }
        else if (IsRoadRuleTime(meActiveRoadRule))
        {
            mpTrainingManager->RequestTraining(BrnProgression::E_TRAINING_TYPE_ROAD_RULES_ON);
        }
        else if (IsRoadRuleCrash(meActiveRoadRule))
        {
            mpTrainingManager->RequestTraining(BrnProgression::E_TRAINING_TYPE_CRASH_ROAD_RULES_ON);
        }
    }

    if (meActiveRoadRule == E_ACTIVE_ROAD_RULE_NONE)
    {
        mbAllowExitRoadRulesAfterTimeout = true;
    }
}

// Once per frame. lbInAir and lbPlayerIsCrashing are passed by the caller and never read.
void RoadRulesManager::Update(const GameStateModuleIO::ControllerInput*               lpControllerInput,
                              BrnStreetData::RoadIndex                                 liCurrentRoadIndex,
                              f32                                                      lfTimeStep,
                              bool                                                     lbInAir,
                              bool                                                     lbPlayerIsCrashing,
                              GameStateModuleIO::OutputBuffer*                         lpOutputBuffer,
                              bool                                                     lbShowtimeActive,
                              GameStateModuleIO::EGameModeType                         leGameModeType,
                              f32                                                      lfPlayerNoInputTime,
                              bool                                                     lbCarSelectActive,
                              bool                                                     lbDisableSwitchingOnline,
                              BrnResource::ChallengeListEntry::EFreeburnChallengeStyle leFreeburnChallengeStyle)
{
    (void)lbInAir;
    (void)lbPlayerIsCrashing;

    CGS_ASSERT(lpOutputBuffer, "lpOutputBuffer");
    GameStateModuleIO::GameActionQueue* lpGameActionQueue = lpOutputBuffer->GetGameActionQueue();

    // [FLAG PC witness] NOT IN THE CONSOLE. Opt-in (BRN_ROADRULES_DIAG): the rule state whenever
    // the road, the rule or the rule's road changes, and every 300th update; 80 lines in all.
    {
        static const bool sbDiag     = (std::getenv("BRN_ROADRULES_DIAG") != 0);
        static s32        siFrame    = 0;
        static s32        siLeft     = 80;
        static s32        siLastKey  = -2;
        const s32         liKey      = (liCurrentRoadIndex & 0xFFF) | (static_cast<s32>(meActiveRoadRule) << 12) |
                                       ((maiChallengeRoadIndex[BrnStreetData::E_SCORE_TYPE_TIME] & 0xFFF) << 16);
        const bool        lbChanged  = (liKey != siLastKey);
        siLastKey = liKey;
        if (sbDiag && siLeft > 0 && (lbChanged || (++siFrame % 300) == 0) && CgsDev::Log::gpDebugPrint != 0)
        {
            --siLeft;
            *CgsDev::Log::gpDebugPrint
                << "[roadrules] update road " << liCurrentRoadIndex << " last " << miLastRoadIndex
                << " rule " << static_cast<s32>(meActiveRoadRule)
                << " timeRoad " << maiChallengeRoadIndex[BrnStreetData::E_SCORE_TYPE_TIME]
                << " t " << mfTime << " timeout " << mfTimeScoreTimeout
                << " inRoad " << mfInRoadTimeout << " noInput " << lfPlayerNoInputTime << "\n";
        }
    }

    // Showtime is over: score the crash rule.
    if (IsCrashRuleActive() && !lbShowtimeActive)
    {
        OnEndRule(lpOutputBuffer, BrnStreetData::E_SCORE_TYPE_CRASH, true);
    }

    // Track the road the player is on. A running time rule ends unscored when the player leaves
    // its road or stops giving input.
    if (IsValidRoad(liCurrentRoadIndex) && !IsCrashRuleActive())
    {
        if (liCurrentRoadIndex != miLastRoadIndex)
        {
            OnLeaveRoad(lpGameActionQueue, miLastRoadIndex);
            OnEnterRoad(lpGameActionQueue, liCurrentRoadIndex);
        }

        // `ble` skips on an unordered compare: a NaN input time never quits, as `>` gives.
        if (IsTimeRuleActive() &&
            (maiChallengeRoadIndex[BrnStreetData::E_SCORE_TYPE_TIME] != liCurrentRoadIndex ||
             lfPlayerNoInputTime > KF_ROAD_RULE_NO_INPUT_QUIT_TIME))
        {
            OnEndRule(lpOutputBuffer, BrnStreetData::E_SCORE_TYPE_TIME, false);
        }

        miLastRoadIndex = liCurrentRoadIndex;
    }

    if (UpdateSwitchingRoadRulesOnOrOff(lpGameActionQueue, lpOutputBuffer, leGameModeType, lbCarSelectActive,
                                        leFreeburnChallengeStyle, lfTimeStep, lbShowtimeActive))
    {
        return;
    }

    bool lbStartEventPressed = lpControllerInput->mbStartEventPressed;

    // The first time road rules are available and the player has been in free burn for three
    // seconds, show the road-rules tip and switch the time rule on as if start had been pressed.
    {
        BrnProgression::Profile* lpProfile = mpModeManager->GetProgressionManager()->GetProfile();

        // `ble` skips on an unordered compare, as `>` gives.
        if (!lpProfile->HasPlayerSeenTrainingType(BrnProgression::E_TRAINING_TYPE_ROAD_RULES_ON) &&
            mpModeManager->GetProgressionManager()->AreRoadRulesAvailable() &&
            mpModeManager->GetTimeInFreeBurn() > 3.0f)
        {
            mpTrainingManager->RequestTraining(BrnProgression::E_TRAINING_TYPE_ROAD_RULES_ON);
            lbStartEventPressed = true;
            meActiveRoadRule    = E_ACTIVE_ROAD_RULE_NONE;
        }
    }

    UpdateActiveRoadRule(lpOutputBuffer, lpGameActionQueue, lbStartEventPressed, lbDisableSwitchingOnline,
                         leFreeburnChallengeStyle);

    if (IsRoadRuleTime(meActiveRoadRule))
    {
        UpdateTimeRule(lpOutputBuffer, lfTimeStep);
    }

    // Showtime started on a road: switch the selection to the crash rule of the matching flavour
    // (ending a time rule first) and start the crash attempt.
    if (lbShowtimeActive && !IsCrashRuleActive())
    {
        if (!IsRoadRuleCrash(meActiveRoadRule))
        {
            if (IsRoadRuleTime(meActiveRoadRule))
            {
                OnEndRule(lpOutputBuffer, BrnStreetData::E_SCORE_TYPE_TIME, false);
            }

            meActiveRoadRule = GameStateModuleIO::IsOnlineFreeBurnLobby(leGameModeType)
                             ? E_ACTIVE_ROAD_RULE_ONLINE_CRASH
                             : E_ACTIVE_ROAD_RULE_OFFLINE_CRASH;
            SendActiveRuleState(lpGameActionQueue);
        }

        OnShowtimeStart(lpOutputBuffer);
    }

    // Off every road: after KF_IN_ROAD_TIMEOUT seconds leave the last road and end the time rule
    // unscored.
    if (!IsCrashRuleActive())
    {
        if (IsValidRoad(miLastRoadIndex) && !IsValidRoad(liCurrentRoadIndex))
        {
            mfInRoadTimeout -= lfTimeStep;

            // `bgt` skips only on an ordered greater-than: a NaN timeout leaves the road.
            if (!(mfInRoadTimeout > 0.0f))
            {
                OnLeaveRoad(lpGameActionQueue, miLastRoadIndex);
                miLastRoadIndex = BrnStreetData::KI_INVALID_ROAD_INDEX;
                OnEndRule(lpOutputBuffer, BrnStreetData::E_SCORE_TYPE_TIME, false);
            }
        }
        else
        {
            mfInRoadTimeout = KF_IN_ROAD_TIMEOUT;
        }
    }

    OnUpdateActiveRoadScores(lpGameActionQueue);
}

}  // namespace BrnGameState
