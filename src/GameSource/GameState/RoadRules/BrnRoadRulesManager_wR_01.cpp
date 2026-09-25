// ---------------------------------------------------------------------------
// GameSource/GameState/RoadRules/BrnRoadRulesManager_wR_01.cpp
//
// BrnGameState::RoadRulesManager, the rule-attempt half: starting and ending a Time or Showtime
// road rule on a road, scoring a finished attempt, the per-frame score update the HUD shows, the
// time-rule clock with its countdown warnings, and the active-rule / online-mode messages.
//
// Game actions posted here (ids from BrnGameActions.h, record sizes from the console's AddEvent
// immediates, all equal to the host sizeof since none of the records holds a pointer):
//   277 OnStartRule (16)   278 OnEndRule (24)   279 OnUpdateActiveRoadScores (12)
//   282 SendActiveRuleState (4)   283 UpdateTimeRule (4)   286 SendRoadRuleModeSwitchMessage (1)
// ---------------------------------------------------------------------------
#include "GameSource/GameState/RoadRules/BrnRoadRulesManager.h"
#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"   // StreetManager (score tables, GetStreetData, ProcessNewRoadScore)
#include "GameSource/GameState/StreetData/BrnChallengeHighScoreEntry.h"  // BrnStreetData::ChallengeHighScoreEntry
#include "GameSource/GameState/ModeManager/BrnModeManager.h"            // ModeManager::ProcessNewRoadScore
#include "GameSource/GameState/BrnGameStateModuleIO.h"                  // OutputBuffer::GetGameActionQueue
#include "GameSource/GameState/BrnGameActions.h"                        // the road-rule action records and ids
#include "GameSource/GameState/BrnCgsPlayerName.h"                      // CgsNetwork::PlayerName
#include "SharedClasses/StreetData/BrnStreetData.h"                     // StreetData / Road / RoadIndex / ChallengeIndex
#include "SharedClasses/StreetData/BrnChallengeData.h"                  // ChallengePlayerScoreEntry / ChallengeParScoresEntry
#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT

namespace BrnGameState
{
    // File-scope tuning constants of BrnRoadRulesManager.cpp (names from the debug information,
    // values read from the constants the console code loads).
    const f32     KF_SECONDS_TO_MILLISECONDS      = 1000.0f;
    const f32     KF_TIME_RULE_TIMEOUT_PROPORTION = 2.0f;
    const f32     KF_WARNING_TIME_BEFORE_END      = 5.0f;
    const f32     KF_WARNING_SPACING              = 1.0f;
    // The console reads this one from writable data (value 6, nothing in the image writes it).
    const int32_t KI_NUM_TIME_WARNINGS            = 6;
    const int32_t KI_DEFAULT_ONLINE_TIME          = 600;

    // Not const in the original: a mutable global in writable data, initialised to 32.0f, read
    // only by OnStartRule.
    f32 KF_ROAD_RULE_RESET_TIME = 32.0f;

    // -----------------------------------------------------------------------------------------
    // SendRoadRuleModeSwitchMessage: action 286, one byte, "the active rule is an online rule".
    // The console has no body of its own for it; SendActiveRuleState carries it inlined.
    // -----------------------------------------------------------------------------------------
    void RoadRulesManager::SendRoadRuleModeSwitchMessage(GameStateModuleIO::GameActionQueue* lpGameActionQueue,
                                                         bool                                lbIsOnline)
    {
        GameStateModuleIO::RoadRulesModeSwitchAction lRoadRulesSwitchAction;
        lRoadRulesSwitchAction.mbIsOnline = lbIsOnline;

        lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRoadRulesSwitchAction),
                                    GameStateModuleIO::E_ACTION_ROAD_RULES_MODE_SWITCH,
                                    sizeof(lRoadRulesSwitchAction));   // console size 1
    }

    // -----------------------------------------------------------------------------------------
    // SendActiveRuleState: action 282 with the active rule, then the rule's score type into the
    // StreetManager (inlined ProcessActiveRoadRuleChange, store to +0x1CC0), then action 286 with
    // "is the rule an online one", and only after that post is mbIsOnlineMode latched. Every use
    // re-reads meActiveRoadRule from the object.
    // -----------------------------------------------------------------------------------------
    void RoadRulesManager::SendActiveRuleState(GameStateModuleIO::GameActionQueue* lpGameActionQueue)
    {
        GameStateModuleIO::RoadRulesActiveRuleChangeAction lRuleChangeAction;
        lRuleChangeAction.meActiveRoadRule = meActiveRoadRule;

        lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRuleChangeAction),
                                    GameStateModuleIO::E_ACTION_ROAD_RULES_ACTIVE_RULE_CHANGE,
                                    sizeof(lRuleChangeAction));   // console size 4

        mpStreetManager->ProcessActiveRoadRuleChange(meActiveRoadRule);

        const bool lbIsOnline = IsRoadRuleOnline(meActiveRoadRule);
        SendRoadRuleModeSwitchMessage(lpGameActionQueue, lbIsOnline);
        mbIsOnlineMode = lbIsOnline;
    }

    // -----------------------------------------------------------------------------------------
    // OnStartRule: a rule attempt begins on liRoadIndex. Nothing at all happens for an invalid
    // road. The road is recorded as the running challenge of that score type, the time rule's
    // clocks are armed from the par / personal / friend scores, and action 277 is posted.
    //
    // lbUsingDefaultOnlineScore starts as mbIsOnlineMode (read on entry) and is cleared whenever
    // a real score to beat exists, so it only stays set online with no recorded score.
    //
    // Scores are in milliseconds; the console scales them to seconds by multiplying by the
    // reciprocal 0.001f (not by dividing), which is what 1.0f / KF_SECONDS_TO_MILLISECONDS gives.
    // Every float compare below is ordered: `x > 0.0f` fails (and asserts) on NaN, as on the
    // console.
    // -----------------------------------------------------------------------------------------
    void RoadRulesManager::OnStartRule(GameStateModuleIO::GameActionQueue* lpGameActionQueue,
                                       BrnStreetData::RoadIndex            liRoadIndex,
                                       BrnStreetData::ScoreType            leScoreType,
                                       bool                                lbAllowTimeout)
    {
        GameStateModuleIO::RoadRulesStartRuleAction lStartRuleAction;
        bool lbUsingDefaultOnlineScore = mbIsOnlineMode;

        if (!IsValidRoad(liRoadIndex))
        {
            return;
        }

        maiChallengeRoadIndex[leScoreType] = liRoadIndex;

        switch (leScoreType)
        {
            case BrnStreetData::E_SCORE_TYPE_TIME:
            {
                BrnStreetData::ChallengePlayerScoreEntry lUserScoreData;
                BrnStreetData::ChallengeParScoresEntry   lParScoreData;
                BrnStreetData::ChallengeHighScoreEntry   lFriendScoreData;
                const BrnStreetData::ChallengeIndex      lChallengeIndex = maiChallengeRoadIndex[BrnStreetData::E_SCORE_TYPE_TIME];
                CgsID                                    lRivalID;
                int32_t                                  liParScore;
                int32_t                                  liBestScore;

                mfTime = 0.0f;

                mpStreetManager->GetChallengeUserScore(lChallengeIndex, &lUserScoreData, true);
                mpStreetManager->GetChallengeParScore(lChallengeIndex, &lParScoreData);
                mpStreetManager->GetChallengeFriendHighScore(lChallengeIndex, &lFriendScoreData, true);

                lParScoreData.GetScore(BrnStreetData::E_SCORE_TYPE_TIME, &liParScore, &lRivalID);

                mfTimeScoreTimeout = static_cast<f32>(liParScore) * (1.0f / KF_SECONDS_TO_MILLISECONDS)
                                   * KF_TIME_RULE_TIMEOUT_PROPORTION;
                CGS_ASSERT(mfTimeScoreTimeout > 0.0f, "mfTimeScoreTimeout > 0.0f");

                liBestScore = KI_DEFAULT_ONLINE_TIME;
                if (lUserScoreData.ContainsData(BrnStreetData::E_SCORE_TYPE_TIME))
                {
                    liBestScore               = lUserScoreData.GetScore(BrnStreetData::E_SCORE_TYPE_TIME);
                    lbUsingDefaultOnlineScore = false;
                }

                if (mbIsOnlineMode)
                {
                    if (lFriendScoreData.ContainsData(BrnStreetData::E_SCORE_TYPE_TIME))
                    {
                        CgsNetwork::PlayerName lPlayerName;
                        int32_t                liFriendScore;

                        lFriendScoreData.GetScore(BrnStreetData::E_SCORE_TYPE_TIME, &liFriendScore, &lPlayerName);
                        // Min(liBestScore, liFriendScore)
                        liBestScore               = (liBestScore < liFriendScore) ? liBestScore : liFriendScore;
                        lbUsingDefaultOnlineScore = false;
                    }
                }
                else
                {
                    // Min(liParScore, liBestScore)
                    liBestScore = (liParScore < liBestScore) ? liParScore : liBestScore;
                }

                miNumWarningsDone = 0;
                mfTimeTarget      = static_cast<f32>(liBestScore) * (1.0f / KF_SECONDS_TO_MILLISECONDS);
                mfNextWarningTime = mfTimeTarget - KF_WARNING_TIME_BEFORE_END;
                break;
            }

            case BrnStreetData::E_SCORE_TYPE_CRASH:
            {
                // Offline the showtime rule has no target to report: only the online flavour
                // looks for a friend's or the player's own crash score.
                if (mbIsOnlineMode)
                {
                    BrnStreetData::ChallengePlayerScoreEntry lUserScoreData;
                    BrnStreetData::ChallengeHighScoreEntry   lFriendScoreData;
                    const BrnStreetData::ChallengeIndex      lChallengeIndex = maiChallengeRoadIndex[BrnStreetData::E_SCORE_TYPE_CRASH];

                    mpStreetManager->GetChallengeUserScore(lChallengeIndex, &lUserScoreData, true);
                    mpStreetManager->GetChallengeFriendHighScore(lChallengeIndex, &lFriendScoreData, true);

                    if (lFriendScoreData.ContainsData(BrnStreetData::E_SCORE_TYPE_CRASH) ||
                        lUserScoreData.ContainsData(BrnStreetData::E_SCORE_TYPE_CRASH))
                    {
                        lbUsingDefaultOnlineScore = false;
                    }
                }
                break;
            }

            default:
                CGS_ASSERT(false, "Unknown road rule type");
                break;
        }

        mbAllowExitRoadRulesAfterTimeout = mbAllowExitRoadRulesAfterTimeout && lbAllowTimeout;
        if (mbAllowExitRoadRulesAfterTimeout)
        {
            mfExitRoadRulesTime = KF_ROAD_RULE_RESET_TIME;
        }

        lStartRuleAction.meScoreType                 = leScoreType;
        lStartRuleAction.mRoadId                     = mpStreetManager->GetStreetData()->GetRoad(maiChallengeRoadIndex[leScoreType])->GetId();
        lStartRuleAction.mbIdUsingDefaultOnlineScore = lbUsingDefaultOnlineScore;

        lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lStartRuleAction),
                                    GameStateModuleIO::E_ACTION_ROAD_RULES_START_RULE,
                                    sizeof(lStartRuleAction));   // console size 0x10
    }

    // -----------------------------------------------------------------------------------------
    // OnUpdateActiveRoadScores: action 279, the running score of each rule that is active (the
    // time-rule clock in seconds, the showtime score as a float).
    // -----------------------------------------------------------------------------------------
    void RoadRulesManager::OnUpdateActiveRoadScores(GameStateModuleIO::GameActionQueue* lpGameActionQueue)
    {
        GameStateModuleIO::RoadRulesUpdateAction lAction;
        lAction.Construct();

        if (IsValidRoad(maiChallengeRoadIndex[BrnStreetData::E_SCORE_TYPE_TIME]))
        {
            lAction.SetScore(BrnStreetData::E_SCORE_TYPE_TIME, mfTime);
        }
        if (IsValidRoad(maiChallengeRoadIndex[BrnStreetData::E_SCORE_TYPE_CRASH]))
        {
            lAction.SetScore(BrnStreetData::E_SCORE_TYPE_CRASH, static_cast<f32>(miCrashScore));
        }

        lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAction),
                                    GameStateModuleIO::E_ACTION_ROAD_RULES_UPDATE,
                                    sizeof(lAction));   // console size 0xC
    }

    // -----------------------------------------------------------------------------------------
    // UpdateTimeRule: advance the time-rule clock. Past the timeout the attempt ends unscored,
    // and the warning check below still runs on the same frame (the console does not re-test the
    // rule after OnEndRule). Up to KI_NUM_TIME_WARNINGS countdown warnings (action 283, the time
    // left to the target) are posted, KF_WARNING_SPACING seconds apart.
    // -----------------------------------------------------------------------------------------
    void RoadRulesManager::UpdateTimeRule(GameStateModuleIO::OutputBuffer* lpOutputBuffer, f32 lfTimeStep)
    {
        if (!IsTimeRuleActive())
        {
            return;
        }

        mfTime += lfTimeStep;
        if (mfTime > mfTimeScoreTimeout)
        {
            OnEndRule(lpOutputBuffer, BrnStreetData::E_SCORE_TYPE_TIME, false);
        }

        if (miNumWarningsDone < KI_NUM_TIME_WARNINGS && mfTime > mfNextWarningTime)
        {
            const f32 lfTimeRemaining = mfTimeTarget - mfTime;

            GameStateModuleIO::RoadRulesTimeWarningAction lTimeWarningAction;
            lTimeWarningAction.mfTimeRemaining = lfTimeRemaining;

            lpOutputBuffer->GetGameActionQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lTimeWarningAction),
                                                           GameStateModuleIO::E_ACTION_ROAD_RULES_TIME_WARNING,
                                                           sizeof(lTimeWarningAction));   // console size 4

            ++miNumWarningsDone;
            mfNextWarningTime += KF_WARNING_SPACING;
        }
    }

    // -----------------------------------------------------------------------------------------
    // OnEndRule: the attempt of this score type is over. With lbAllowScoring the attempt is scored
    // first (OnScoreCompleted). Then, only if a rule of that type was running, action 278 goes out
    // and the running-challenge index is cleared. An unknown score type asserts and posts a record
    // whose score was never written, as on the console.
    // -----------------------------------------------------------------------------------------
    void RoadRulesManager::OnEndRule(GameStateModuleIO::OutputBuffer* lpOutputBuffer,
                                     BrnStreetData::ScoreType         leScoreType,
                                     bool                             lbAllowScoring)
    {
        GameStateModuleIO::RoadRulesEndRuleAction lEndRuleAction;

        CGS_ASSERT(lpOutputBuffer, "lpOutputBuffer");
        GameStateModuleIO::GameActionQueue* lpGameActionQueue = lpOutputBuffer->GetGameActionQueue();

        switch (leScoreType)
        {
            case BrnStreetData::E_SCORE_TYPE_TIME:
                lEndRuleAction.mfScore = mfTime;
                if (lbAllowScoring)
                {
                    OnScoreCompleted(lpOutputBuffer, BrnStreetData::E_SCORE_TYPE_TIME);
                }
                break;

            case BrnStreetData::E_SCORE_TYPE_CRASH:
                lEndRuleAction.mfScore = static_cast<f32>(miCrashScore);
                if (lbAllowScoring)
                {
                    OnScoreCompleted(lpOutputBuffer, BrnStreetData::E_SCORE_TYPE_CRASH);
                }
                break;

            default:
                CGS_ASSERT(false, "Unknown road rule type");
                break;
        }

        const BrnStreetData::RoadIndex liRoadIndex = maiChallengeRoadIndex[leScoreType];
        if (IsValidRoad(liRoadIndex))
        {
            lEndRuleAction.meScoreType    = leScoreType;
            lEndRuleAction.mRoadId        = mpStreetManager->GetStreetData()->GetRoad(liRoadIndex)->GetId();
            lEndRuleAction.mbValidAttempt = lbAllowScoring;

            lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lEndRuleAction),
                                        GameStateModuleIO::E_ACTION_ROAD_RULES_END_RULE,
                                        sizeof(lEndRuleAction));   // console size 0x18

            maiChallengeRoadIndex[leScoreType] = BrnStreetData::KI_INVALID_ROAD_INDEX;
        }
    }

    // -----------------------------------------------------------------------------------------
    // OnScoreCompleted: build a player score record for the finished attempt (time in whole
    // milliseconds, truncated; showtime score as is) and hand it to the StreetManager (personal
    // and friend bests; the friend check runs only in online mode) and to the ModeManager.
    // ChallengeData::SetScore silently drops a score outside its type's [min, max] range; the
    // record is handed on either way.
    // -----------------------------------------------------------------------------------------
    void RoadRulesManager::OnScoreCompleted(GameStateModuleIO::OutputBuffer* lpOutputBuffer,
                                            BrnStreetData::ScoreType         leScoreType)
    {
        BrnStreetData::ChallengePlayerScoreEntry lChallengeData;
        const BrnStreetData::RoadIndex           liRoadIndex = maiChallengeRoadIndex[leScoreType];

        CGS_ASSERT(IsValidRoad(liRoadIndex), "IsValidRoad( liRoadIndex )");

        lChallengeData.Construct();

        switch (leScoreType)
        {
            case BrnStreetData::E_SCORE_TYPE_TIME:
                lChallengeData.SetScore(BrnStreetData::E_SCORE_TYPE_TIME,
                                        static_cast<int32_t>(mfTime * KF_SECONDS_TO_MILLISECONDS));
                break;

            case BrnStreetData::E_SCORE_TYPE_CRASH:
                lChallengeData.SetScore(BrnStreetData::E_SCORE_TYPE_CRASH, miCrashScore);
                break;

            default:
                CGS_ASSERT(false, "Unknown road rule type");
                break;
        }

        CGS_ASSERT(mpStreetManager, "mpStreetManager");
        mpStreetManager->ProcessNewRoadScore(lpOutputBuffer, lChallengeData, leScoreType, liRoadIndex, mbIsOnlineMode);

        mpModeManager->ProcessNewRoadScore(lpOutputBuffer, lChallengeData, leScoreType,
                                           mpStreetManager->GetStreetData()->GetRoad(liRoadIndex)->GetId(),
                                           liRoadIndex);
    }
}
