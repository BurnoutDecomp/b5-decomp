// ---------------------------------------------------------------------------
// GameSource/GameState/RoadRules/BrnRoadRulesManager.cpp
// ---------------------------------------------------------------------------
#include "GameSource/GameState/RoadRules/BrnRoadRulesManager.h"
#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"  // BrnGameState::StreetManager (single home for GetStreetData)
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "SharedClasses/StreetData/BrnStreetData.h"       // BrnStreetData::StreetData / Road
#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/RoadRules/BrnRoadRulesManager.cpp"
// BrnGameState::StreetManager (incl. its GetStreetData() accessor) now lives in its single
// canonical home GameSource/GameState/StreetData/BrnGameStateStreetManager.h (included above).
// The former file-local `struct StreetManager` shim was removed from this TU: it ODR-collided
// with the new header once a second TU (the road-rules debug component) needed the type.

// extern const CgsID K_INVALID_ID == 0 (X360 'return 0' on no-current-road).
const CgsID BrnGameState::RoadRulesManager::K_INVALID_ID = 0;

// 0x82327438
// DWARF: CgsID GetCurrentRoadID() const;  (the Hex-Rays 'int' return + 4-byte
// read of *(Road+16) is a decompiler truncation of the 8-byte CgsID load).
CgsID BrnGameState::RoadRulesManager::GetCurrentRoadID() const
{
    // *(this+32) == -1  ->  no current road.
    if ( miLastRoadIndex == -1 )
    {
        return K_INVALID_ID;
    }

    // *(this+20): asserted present (baked "mpStreetManager").
    CGS_ASSERT_AT( mpStreetManager, "mpStreetManager", CGS_ARTIST_FILE, 1324 );

    // ResourcePtr<StreetData>::operator-> wrapped by StreetManager::GetStreetData().
    const BrnStreetData::StreetData* lpStreetData = mpStreetManager->GetStreetData();
    const BrnStreetData::Road*       lpRoad       = lpStreetData->GetRoad( miLastRoadIndex );

    // baked "lpRoad".
    CGS_ASSERT_AT( lpRoad, "lpRoad", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\gamestate\\RoadRules/BrnRoadRulesManager.h", 396 );

    // X360 reads *(Road+16) == Road::mId; the inlined Road::GetId().
    return lpRoad->GetId();
}

// ----------------------------------------------------------------------------
// IsRoadLimitRegionValid @ 0x82335268
//
// ⭐ [gateui] REAL (2026-08-20, round 3). Was declaration-only, and it is one of the two
// RoadRulesManager symbols that gate the BrnTriggerQueryManager.cpp mount
// (build_game_exe.bat:2369-2375's list) -- its only caller in the image is
// TriggerQueryManager::UpdateTriggers @0x82391FD8, which asserts on the answer once per track:
// "Road limit region <id>(<limit>) is broken\nDid you build triggers and forget to build RoadRules?"
//
// Answers "does any Road in the loaded StreetData claim this road-limit id?" -- a linear scan of
// StreetData::mpaRoads comparing lLimitId against each Road's TWO road-limit ids, returning true
// on the first hit.
//
// ⚠️ lRegionId IS GENUINELY UNUSED, and that is the console's shape, not a dropped read. The asm
// saves only r3 (this) and r5 (the limit id) -- `0x82335274 mr r28, r3` / `0x82335278 mr r27, r5`
// -- and r4 is never moved, spilled or read anywhere in the body. It exists so the CALLER's assert
// message can name both ids. Kept in the signature (the DWARF/mangled shape is
// `bool (CgsID, CgsID) const`) and explicitly voided.
//
// Reads, all off the asm rather than the pseudocode (Hex-Rays renders the 8-byte big-endian CgsID
// loads as 4-byte reads at +28/+36 -- they are `ld` at +0x18/+0x20):
//   0x82335280  lwz r11, 0x14(this)        ; mpStreetManager
//   0x82335284  addi r3, r11, 0x1CC8       ; +7368 == the ResourcePtr<StreetData>, i.e.
//   0x82335288  bl  StreetData_::oper      ;   StreetManager::GetStreetData()
//   0x8233528C  lwz r11, 0x20(r3)          ; StreetData::miRoadCount   -> GetRoadCount()
//   0x823352E8  lwz r11, 0x10(r30)         ; StreetData::mpaRoads      \ the inlined GetRoad(i),
//   0x823352EC  add r11, r11, r29          ;   + i*0x40 (road stride)  / whose bounds assert
//   0x823352D4  li  r5, 0x26D              ;   BrnStreetData.h:621 fires here -- reproduced by
//                                          ;   calling the tree's own inline GetRoad()
//   0x823352F0  ld  r10, 0x18(r11)         ; Road::miRoadLimitId0 -> GetRoadLimitId0()
//   0x823352FC  ld  r11, 0x20(r11)         ; Road::miRoadLimitId1 -> GetRoadLimitId1()
//   0x823352F4/0x82335300  cmpld           ; 64-bit compares against the limit id
// ⓘ This is the FIRST attested call site for Road::GetRoadLimitId1() -- BrnStreetData.h:324 notes
// it had none and was bodied by symmetry. The `ld r11, 0x20` above attests it now (that header is
// outside this owner's lane, so the note is left for its owner to fold in).
//
// The return is the console's own expression: `miRoadCount != liRoadIndex`, i.e. TRUE when the
// loop broke early on a match (0x82335338-0x82335344 `subf`/`cntlzw`/`extrwi 1,26`/`xori 1` is the
// compiler's !=0 test on that difference). A zero road count therefore answers false, which is
// what makes the caller's assert fire when RoadRules was never built.
// NO assert on mpStreetManager here -- unlike GetCurrentRoadID above, the console does not check it.
// ----------------------------------------------------------------------------
bool BrnGameState::RoadRulesManager::IsRoadLimitRegionValid(CgsID lRegionId, CgsID lLimitId) const
{
    (void)lRegionId;   // see the banner: r4 is never read by the console body

    // The console re-fetches the StreetData through the ResourcePtr on EVERY loop test (five
    // separate `StreetData_::oper` calls in 0xE0 bytes) -- that is the compiler evaluating the
    // source's `mpStreetManager->GetStreetData()->GetRoadCount()` in the loop condition, so the
    // condition is written the same way here.
    BrnStreetData::RoadIndex liRoadIndex = 0;

    while (liRoadIndex < mpStreetManager->GetStreetData()->GetRoadCount())
    {
        const BrnStreetData::Road* lpRoad = mpStreetManager->GetStreetData()->GetRoad(liRoadIndex);

        if (lLimitId == lpRoad->GetRoadLimitId0())
            break;
        if (lLimitId == lpRoad->GetRoadLimitId1())
            break;

        ++liRoadIndex;
    }

    return liRoadIndex != mpStreetManager->GetStreetData()->GetRoadCount();
}

#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/GameState/ModeManager/BrnModeManager.h"
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdlib>

namespace BrnGameState {
// ARTIST 0x823481E8.
void RoadRulesManager::OnEnterRoad(GameStateModuleIO::GameActionQueue* queue, BrnStreetData::RoadIndex road)
{
    if (road == BrnStreetData::KI_INVALID_ROAD_INDEX) return;
    GameStateModuleIO::RoadRulesEnterRoadAction action;
    action.mParScores.Copy(mpStreetManager->GetStreetData()->GetChallengeParScore(road));
    mpStreetManager->GetChallengeUserScore(road, &action.mUserScores, true);
    mpStreetManager->GetChallengeFriendHighScore(road, &action.mFriendScores, true);
    mpStreetManager->UnlockUpcomingRoadSigns();
    action.mRoadId = mpStreetManager->GetStreetData()->GetRoad(road)->GetId();
    action.miRoadIndex = road;
    for (s32 i = 0; i < 2; ++i)
        action.maParRivalIds[i] = mpStreetManager->GetParRivalId(road, static_cast<BrnStreetData::ScoreType>(i));
    queue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&action),
                    GameStateModuleIO::E_ACTION_ROAD_RULES_ENTER_ROAD, sizeof(action));   // console size 0xA8
    CGS_ASSERT_AT(mpModeManager != 0, "mpModeManager", CGS_ARTIST_FILE, 983);
    if (GameMode* mode = mpModeManager->GetCurrentGameMode())
        mode->OnEnterRoad(road);
}

// ARTIST 0x82348320.
void RoadRulesManager::OnLeaveRoad(GameStateModuleIO::GameActionQueue* queue, BrnStreetData::RoadIndex road)
{
    if (road == BrnStreetData::KI_INVALID_ROAD_INDEX) return;
    GameStateModuleIO::RoadRulesLeaveRoadAction lAction;
    lAction.mRoadId = mpStreetManager->GetStreetData()->GetRoad(road)->GetId();
    queue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAction),
                    GameStateModuleIO::E_ACTION_ROAD_RULES_LEAVE_ROAD, sizeof(lAction));   // console size 8
}

}

// FOLDED FROM BrnRoadRulesManager_wR_01.cpp
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
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

namespace BrnGameState
{
    // File-scope tuning constants of BrnRoadRulesManager.cpp (names from the debug information,
    // values read from the constants the console code loads).
    const f32     KF_SECONDS_TO_MILLISECONDS      = 1000.0f;
    const f32     KF_TIME_RULE_TIMEOUT_PROPORTION = 2.0f;
    const f32     KF_WARNING_TIME_BEFORE_END      = 5.0f;
    const f32     KF_WARNING_SPACING              = 1.0f;
    const int32_t KI_DEFAULT_ONLINE_TIME          = 600;

    // Not const in this build: ARTIST keeps it as a writable .data word (0x82CDB888 = 6, among the
    // unity's other mutable file globals, cf. KF_ROAD_RULE_RESET_TIME 0x82CDB7CC below) and
    // UpdateTimeRule loads it at run time (0x8235299C lwz). The PS3 DWARF (cpp:55) has the const
    // form, which MSVC folds to an immediate; the console's load is the merge-window delta.
    int32_t KI_NUM_TIME_WARNINGS = 6;

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
                CGS_ASSERT_AT(mfTimeScoreTimeout > 0.0f, "mfTimeScoreTimeout > 0.0f", CGS_ARTIST_FILE, 1072);

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
                CGS_FAIL_AT("Unknown road rule type", CGS_ARTIST_FILE, 1140);
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

            // AddGameAction (DWARF BrnGameActions.h:501) inlined: getter, then &action (0x823529DC).
            lpOutputBuffer->GetGameActionQueue()->AddGameAction(lTimeWarningAction);   // console size 4

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

        CGS_ASSERT_AT(lpOutputBuffer, "lpOutputBuffer", CGS_ARTIST_FILE, 1182);
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
                CGS_FAIL_AT("Unknown road rule type", CGS_ARTIST_FILE, 1217);
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

        CGS_ASSERT_AT(IsValidRoad(liRoadIndex), "IsValidRoad( liRoadIndex )", CGS_ARTIST_FILE, 1296);

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
                CGS_FAIL_AT("Unknown road rule type", CGS_ARTIST_FILE, 1318);
                break;
        }

        CGS_ASSERT_AT(mpStreetManager, "mpStreetManager", CGS_ARTIST_FILE, 1324);
        mpStreetManager->ProcessNewRoadScore(lpOutputBuffer, lChallengeData, leScoreType, liRoadIndex, mbIsOnlineMode);

        mpModeManager->ProcessNewRoadScore(lpOutputBuffer, lChallengeData, leScoreType,
                                           mpStreetManager->GetStreetData()->GetRoad(liRoadIndex)->GetId(),
                                           liRoadIndex);
    }
}

// FOLDED FROM BrnRoadRulesManager_wR_02.cpp
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
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
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
    bool lbRoadRulesNotAllowed =
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
            CGS_FAIL_AT("How did it end up here?", CGS_ARTIST_FILE, 474);
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

    CGS_ASSERT_AT(lpOutputBuffer, "lpOutputBuffer", CGS_ARTIST_FILE, 204);
    GameStateModuleIO::GameActionQueue* lpGameActionQueue = lpOutputBuffer->GetGameActionQueue();

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

// FOLDED FROM BrnRoadRulesManager_wR_03.cpp
// ---------------------------------------------------------------------------
// GameSource/GameState/RoadRules/BrnRoadRulesManager_wR_03.cpp
//
// RoadRulesManager, the event half: the road-limit crossing that starts and scores the time
// rule (OnRoadLimit), showtime starting on a road (OnShowtimeStart), the offline/online mode
// switch (SetRoadRulesMode), the profile-load quit (QuitAnyActiveRules), the GUI's rule pick
// (SetActiveRoadRule) and the GUI's request for a road's rule data (OnRoadRulesDataRequest).
//
// Every body is read off the console code of the function it defines. OnStartRule, OnEndRule
// and SendActiveRuleState are defined in their own partfile; OnEnterRoad and OnLeaveRoad in
// BrnRoadRulesManager.cpp.
// ---------------------------------------------------------------------------
#include "GameSource/GameState/RoadRules/BrnRoadRulesManager.h"
#include "GameSource/GameState/BrnGameStateModuleIO.h"                   // OutputBuffer::GetGameActionQueue
#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"   // StreetManager::GetStreetData
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"     // TrainingManager::RequestTraining
#include "SharedClasses/StreetData/BrnStreetData.h"                      // StreetData / Road
#include "SharedClasses/Progression/BrnTrainingTypes.h"                  // BrnProgression::ETrainingType
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT, CgsDev::Assert
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"             // CgsDev::StrStream (OnRoadLimit's message)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"               // the opt-in witness below
#include <cstdlib>                                                       // getenv

namespace BrnGameState
{

// The player crossed a road-limit region. A road carries two limit ids, one at each end: find
// the road that owns lRoadLimitId and remember the id at its other end. Crossing back out over
// the limit the time rule started from ends the attempt with scoring allowed; any other crossing
// ends it without a score. Crossing INTO a road starts a new time attempt (when a time rule is
// selected and the car is not crashing) and makes that road the current one.
void RoadRulesManager::OnRoadLimit(CgsID lRoadLimitId, bool lbEntryDirection,
                                   GameStateModuleIO::OutputBuffer* lpOutputBuffer, bool lbIsPlayerCarCrashing)
{
    CgsID lOtherRoadLimitId = 0;

    CGS_ASSERT_AT(lpOutputBuffer, "lpOutputBuffer", CGS_ARTIST_FILE, 799);
    GameStateModuleIO::GameActionQueue* lpGameActionQueue = lpOutputBuffer->GetGameActionQueue();

    // The road count is re-read through the street-data resource on every test, as the console does.
    BrnStreetData::RoadIndex liRoadIndex;
    for (liRoadIndex = 0; liRoadIndex < mpStreetManager->GetStreetData()->GetRoadCount(); ++liRoadIndex)
    {
        const BrnStreetData::Road* lpRoad = mpStreetManager->GetStreetData()->GetRoad(liRoadIndex);

        if (lRoadLimitId == lpRoad->GetRoadLimitId0())
        {
            lOtherRoadLimitId = lpRoad->GetRoadLimitId1();
            break;
        }
        if (lRoadLimitId == lpRoad->GetRoadLimitId1())
        {
            lOtherRoadLimitId = lpRoad->GetRoadLimitId0();
            break;
        }
    }

    if (liRoadIndex == mpStreetManager->GetStreetData()->GetRoadCount())
    {
        return;   // no road owns this limit id
    }

    bool lbAllowScoring = false;
    if (lOtherRoadLimitId == mLastLimitId && IsTimeRuleActive())
    {
        // An attempt that finishes within 2.5 s of starting should be impossible. Written as
        // !(a > b) so a NaN clock fires the assert too, matching the console's unordered compare.
        if (!(mfTime > 2.5f))
        {
            CgsDev::Assert::BeginAssert();

            // The console streams into the shared assert message buffer; the tree's idiom is a
            // stack buffer of the same size. The longest message this can build is 232 bytes.
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "How the hell did this happen? Find Iain or Marti\xF1" "o straightaway and tell them "
                          "exactly what you did. road index "
                       << miLastRoadIndex
                       << " Challenge index = " << maiChallengeRoadIndex[BrnStreetData::E_SCORE_TYPE_TIME]
                       << " Road limit ID = " << lRoadLimitId
                       << " Other road limit ID = " << lOtherRoadLimitId
                       << "\n";

            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }
        lbAllowScoring = true;
    }

    OnEndRule(lpOutputBuffer, BrnStreetData::E_SCORE_TYPE_TIME, lbAllowScoring);

    if (lbEntryDirection)
    {
        if (IsRoadRuleTime(meActiveRoadRule) && !lbIsPlayerCarCrashing)
        {
            OnStartRule(lpGameActionQueue, liRoadIndex, BrnStreetData::E_SCORE_TYPE_TIME, false);
            mLastLimitId = lRoadLimitId;
        }

        if (liRoadIndex != miLastRoadIndex && !IsCrashRuleActive())
        {
            OnLeaveRoad(lpGameActionQueue, miLastRoadIndex);
            OnEnterRoad(lpGameActionQueue, liRoadIndex);
            miLastRoadIndex = liRoadIndex;
        }
    }
    else
    {
        mLastLimitId = 0;
    }
}

// Showtime began on the current road: any time attempt ends unscored and a crash attempt starts
// on the road the player is on.
void RoadRulesManager::OnShowtimeStart(GameStateModuleIO::OutputBuffer* lpOutputBuffer)
{
    CGS_ASSERT_AT(lpOutputBuffer, "lpOutputBuffer", CGS_ARTIST_FILE, 886);
    GameStateModuleIO::GameActionQueue* lpGameActionQueue = lpOutputBuffer->GetGameActionQueue();

    CGS_ASSERT_AT(!IsValidRoad(maiChallengeRoadIndex[BrnStreetData::E_SCORE_TYPE_CRASH]),
               "!IsValidRoad( maiChallengeRoadIndex[BrnStreetData::E_SCORE_TYPE_CRASH] )", CGS_ARTIST_FILE, 890);

    if (IsTimeRuleActive())
    {
        OnEndRule(lpOutputBuffer, BrnStreetData::E_SCORE_TYPE_TIME, false);
    }

    OnStartRule(lpGameActionQueue, miLastRoadIndex, BrnStreetData::E_SCORE_TYPE_CRASH, true);
    mpTrainingManager->RequestTraining(BrnProgression::E_TRAINING_TYPE_CRASH_ROAD_RULES_ON);
}

// Switch between the offline and online flavour of the selected rule. A running time attempt
// ends unscored when the time rule flips; the crash rule flips without ending anything. The
// active-rule state is re-sent whether or not the mode changed.
void RoadRulesManager::SetRoadRulesMode(GameStateModuleIO::OutputBuffer* lpOutputBuffer, bool lbMode)
{
    const bool lbHasModeChanged = (mbIsOnlineMode != lbMode);
    mbIsOnlineMode = lbMode;

    if (lbHasModeChanged)
    {
        switch (meActiveRoadRule)
        {
        case E_ACTIVE_ROAD_RULE_NONE:
        case E_ACTIVE_ROAD_RULE_COUNT:
            break;

        case E_ACTIVE_ROAD_RULE_OFFLINE_TIME:
            OnEndRule(lpOutputBuffer, BrnStreetData::E_SCORE_TYPE_TIME, false);
            mbAllowExitRoadRulesAfterTimeout = false;
            meActiveRoadRule = E_ACTIVE_ROAD_RULE_ONLINE_TIME;
            break;

        case E_ACTIVE_ROAD_RULE_ONLINE_TIME:
            OnEndRule(lpOutputBuffer, BrnStreetData::E_SCORE_TYPE_TIME, false);
            meActiveRoadRule = E_ACTIVE_ROAD_RULE_OFFLINE_TIME;
            mbAllowExitRoadRulesAfterTimeout = false;
            break;

        case E_ACTIVE_ROAD_RULE_OFFLINE_CRASH:
            meActiveRoadRule = E_ACTIVE_ROAD_RULE_ONLINE_CRASH;
            break;

        case E_ACTIVE_ROAD_RULE_ONLINE_CRASH:
            meActiveRoadRule = E_ACTIVE_ROAD_RULE_OFFLINE_CRASH;
            break;

        default:
            CGS_FAIL_AT("How did it end up here?", CGS_ARTIST_FILE, 606);
            break;
        }
    }

    SendActiveRuleState(lpOutputBuffer->GetGameActionQueue());
}

// A profile was loaded: end both attempts, neither scored.
void RoadRulesManager::QuitAnyActiveRules(GameStateModuleIO::OutputBuffer* lpOutputBuffer)
{
    if (IsTimeRuleActive())
    {
        OnEndRule(lpOutputBuffer, BrnStreetData::E_SCORE_TYPE_TIME, false);
    }
    if (IsCrashRuleActive())
    {
        OnEndRule(lpOutputBuffer, BrnStreetData::E_SCORE_TYPE_CRASH, false);
    }
}

void RoadRulesManager::SetActiveRoadRule(GameStateModuleIO::GameActionQueue* lpGameActionQueue,
                                         EActiveRoadRule leActiveRoadRule)
{
    meActiveRoadRule = leActiveRoadRule;
    mbAllowExitRoadRulesAfterTimeout = false;
    SendActiveRuleState(lpGameActionQueue);
}

// The GUI asked for a road's rule data: re-post the enter-road action for it. An id of 0 means
// the current road; an id no road carries posts nothing.
void RoadRulesManager::OnRoadRulesDataRequest(CgsID lRoadId, GameStateModuleIO::GameActionQueue* lpGameActionQueue)
{
    if (lRoadId == 0)
    {
        OnEnterRoad(lpGameActionQueue, miLastRoadIndex);
        return;
    }

    for (BrnStreetData::RoadIndex liRoadIndex = 0;
         liRoadIndex < mpStreetManager->GetStreetData()->GetRoadCount();
         ++liRoadIndex)
    {
        const BrnStreetData::Road* lpRoad = mpStreetManager->GetStreetData()->GetRoad(liRoadIndex);
        if (lRoadId == lpRoad->GetId())
        {
            OnEnterRoad(lpGameActionQueue, liRoadIndex);
            return;
        }
    }
}

} // namespace BrnGameState

#undef CGS_ARTIST_FILE
