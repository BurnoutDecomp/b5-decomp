// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wB_11.cpp
// ============================================================================
// Wave-B partfile for BrnGameState::ChallengeManager -- the three network/road success
// event handlers. Store-for-store reconstructions of the X360 ARTIST methods over the
// keystone's NAMED members (see BrnChallengeManager.h for the layout evidence):
//
//   HandleSuccessUpdateEvent   @ 0x8233CDE0  (DWARF :212) -- remote last-second success mask
//   HandleChallengeSuccessEvent@ 0x82316AE0  (DWARF :218) -- per-action success/accumulation grid
//   HandleRoadRuleScore        @ 0x82334D48  (DWARF :224) -- road-rule time/crash score feed
//
// Every handler gates on the manager being RUNNING (meChallengeManagerStatus == 2). The
// two remote handlers additionally verify the event's active-race-car index; the two frame
// stamped events (Success/SuccessUpdate) drop nothing on frame here (HandleSuccessUpdateEvent
// has no frame gate), while HandleChallengeSuccessEvent drops events whose update frame is
// not newer than miLastChallengeResetFrame (X360-only latch, spec section A).
//
// The FastBitArray<60>/<120> bit scan + set in HandleSuccessUpdateEvent is the X360 inlined
// container iteration (GetFirstBitSet/GetNextBitSet/SetBit, complete with its CgsDev::StrStream
// range-assert scaffolding); per spec pitfall 5 the committed container methods are called
// directly and the assert/StrStream machinery is intentionally not reproduced (benign).
// ============================================================================

#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h" // IsSimTimerFrequency50Hz
#include "SharedClasses/DataLists/ChallengeListEntry.h"           // BrnResource::ChallengeListEntry(Action)
#include "SharedClasses/StreetData/BrnChallengeData.h"            // BrnStreetData::{ChallengePlayerScoreEntry, ScoreType}

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// HandleSuccessUpdateEvent -- X360 0x8233CDE0. Fold a remote player's per-frame "last second
// success" bit mask into that player's success ring, then latch the player's arbitration slot.
//
//   * Only while RUNNING (X360 lwz +0xE08; cmpwi 2). No frame gate on this event.
//   * mabReceivedSuccessUpdates[arci] = true (X360 stb 1, +0x590 + arci).
//   * For every set bit of the event mask (FastBitArray<60> mChallengeSuccessUpdate): the
//     network frame (event->miChallengeUpdateFrame + bit) is folded modulo the sim ring size
//     (100 @ 50Hz, 120 @ 60Hz) and its slot is set in this player's FastBitArray<120> ring
//     (X360 inlines GetFirstBitSet/GetNextBitSet/SetBit + the range asserts).
//   * Finally the mask's per-frequency last-second bit (bit 49 @ 50Hz, bit 59 @ 60Hz) selects
//     the arbitration success: DONE if set, NONE otherwise.
// ----------------------------------------------------------------------------
void ChallengeManager::HandleSuccessUpdateEvent(
    const CgsSystem::TimerStatusInterface* lpTimerStatusInterface,
    const GameStateModuleIO::FburnChallengeSuccessUpdateEvent* lpEvent)
{
    CGS_ASSERT(lpTimerStatusInterface, "lpTimerStatusInterface");   // X360 line 4698
    CGS_ASSERT(lpEvent, "lpEvent");                                 // X360 line 4699

    if (meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RUNNING)   // X360 lwz +0xE08; cmpwi 2
    {
        const EActiveRaceCarIndex leActiveRaceCarIndex = lpEvent->meActiveRaceCarIndex;   // X360 lwz +8
        CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                   "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");     // X360 line 4712
        CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");  // X360 line 4713

        mabReceivedSuccessUpdates[leActiveRaceCarIndex] = true;             // X360 stb 1, +0x590 + arci

        // Walk every set bit of the incoming last-second success mask; each maps a network
        // frame into this player's per-frame success ring.
        for (s32 liBit = lpEvent->mChallengeSuccessUpdate.GetFirstBitSet();
             liBit != GameStateModuleIO::LastSecondChallengeSuccess::KI_INVALID_BIT_INDEX;
             liBit = lpEvent->mChallengeSuccessUpdate.GetNextBitSet(liBit))
        {
            // X360 divides by 100 (50Hz) or 120 (60Hz) and keeps the remainder.
            const s32 liRingSize = lpTimerStatusInterface->IsSimTimerFrequency50Hz() ? 100 : 120;
            const s32 liFrameSlot = (lpEvent->miChallengeUpdateFrame + liBit) % liRingSize;
            maPlayerSuccessUpdateArray[leActiveRaceCarIndex].SetBit(static_cast<u32>(liFrameSlot));
        }

        // X360 tail: reads the sim time step directly (== IsSimTimerFrequency50Hz) and tests the
        // matching last-second bit of the mask (1<<49 @ 50Hz, 1<<59 @ 60Hz).
        bool lbLastSecondSuccess;
        if (lpTimerStatusInterface->IsSimTimerFrequency50Hz())
        {
            lbLastSecondSuccess = lpEvent->mChallengeSuccessUpdate.IsBitSet(49);
        }
        else
        {
            lbLastSecondSuccess = lpEvent->mChallengeSuccessUpdate.IsBitSet(59);
        }

        maaePlayersSuccessStatus[leActiveRaceCarIndex][miCurrentArbitrationIndex] =
            lbLastSecondSuccess ? GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE
                                : GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_NONE;
    }
}

// ----------------------------------------------------------------------------
// HandleChallengeSuccessEvent -- X360 0x82316AE0. Apply a remote player's per-action challenge
// score/success event to that player's row of the success grids.
//
//   * Only while RUNNING and only when the event's update frame is newer than the last
//     challenge-reset frame (X360 lwz +0xC > lwz +0x1000).
//   * arci + mpCurrentChallenge validated (non-fatal asserts).
//   * For each action slot: if the event flags it successful, copy the score, (re)arm the
//     convoy timer on the arbitration action, then fold the score into the grids per coop
//     type. If not successful but the event carries an individual-accumulation contribution,
//     record it as a CONTRIBUTING partial score.
// ----------------------------------------------------------------------------
void ChallengeManager::HandleChallengeSuccessEvent(
    const GameStateModuleIO::FburnChallengeSuccessEvent* lpEvent)
{
    CGS_ASSERT(lpEvent, "lpEvent");   // X360 line 4778

    if (meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RUNNING &&      // X360 lwz +0xE08; cmpwi 2
        lpEvent->miChallengeUpdateFrame > miLastChallengeResetFrame)           // X360 lwz +0xC > lwz +0x1000
    {
        const EActiveRaceCarIndex leActiveRaceCarIndex = lpEvent->meActiveRaceCarIndex;   // X360 lwz +0x10
        CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                   "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");        // X360 line 4792
        CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");     // X360 line 4793
        CGS_ASSERT(mpCurrentChallenge, "mpCurrentChallenge");                  // X360 line 4794

        for (s32 liActionIndex = 0; liActionIndex < KI_MAX_CHALLENGE_ACTIONS; ++liActionIndex)
        {
            const BrnResource::ChallengeListEntryAction* lpAction =
                mpCurrentChallenge->GetAction(liActionIndex);
            CGS_ASSERT(lpAction, "lpAction");   // X360 line 4800

            if (lpEvent->mabSuccessfulActions[liActionIndex])   // X360 lbzx +8 + action
            {
                maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex] =
                    lpEvent->mafActionScores[liActionIndex];    // X360 stfsx +0x468 grid

                if (liActionIndex == miCurrentArbitrationIndex &&   // X360 cmpw action, +0xE14
                    lpAction->HasConvoyTime() &&                    // X360 lfs +0x44 > 0.0
                    !mbConvoyTimerRunning)                          // X360 lbz +0xE24 == 0
                {
                    mfConvoyTimer        = lpAction->GetConvoyTime();   // X360 stfs +0xE20
                    mbConvoyTimerRunning = true;                        // X360 stb 1, +0xE24
                }

                switch (static_cast<s32>(lpAction->GetCoopType()))   // X360 lbz +1 (muCoopType)
                {
                    case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_CUMULATIVE:   // 4
                    {
                        s32 liRemainingTarget = maiRemainingTarget[liActionIndex] -
                            static_cast<s32>(maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex]);
                        if (liRemainingTarget < 0)
                        {
                            liRemainingTarget = 0;
                        }
                        maiRemainingTarget[liActionIndex] = liRemainingTarget;

                        const GameStateModuleIO::EFreeburnChallengeSuccess lePreviousStatus =
                            maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex];
                        maafCumulativeContributions[leActiveRaceCarIndex][liActionIndex] +=
                            maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex];
                        if (lePreviousStatus != GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE)
                        {
                            maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] =
                                GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_CONTRIBUTING;
                        }
                        break;
                    }
                    case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL:              // 1
                    case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION: // 2
                        maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] =
                            GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE;
                        maafCumulativeContributions[leActiveRaceCarIndex][liActionIndex] =
                            maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex];
                        break;
                    case 6:   // X360 coop-type value 6 (no committed enumerator; scored as CONTRIBUTING)
                        maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] =
                            GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_CONTRIBUTING;
                        maafCumulativeContributions[leActiveRaceCarIndex][liActionIndex] =
                            maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex];
                        break;
                    default:
                        maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] =
                            GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE;
                        break;
                }
            }
            else if (lpEvent->mabAccumulationThisFrame[liActionIndex] &&   // X360 lbz +0xA + action
                     lpAction->GetCoopType() ==
                         BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION)
            {
                maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex] =
                    lpEvent->mafActionScores[liActionIndex];
                maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] =
                    GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_CONTRIBUTING;
                maafCumulativeContributions[leActiveRaceCarIndex][liActionIndex] =
                    lpEvent->mafActionScores[liActionIndex];
            }
        }
    }
}

// ----------------------------------------------------------------------------
// HandleRoadRuleScore -- X360 0x82334D48. Feed a freshly recorded road-rule score into the
// current challenge's road-rule action.
//
//   * Only while RUNNING; the current challenge + action index are validated.
//   * The current action must be the matching road-rule action for the score type
//     (ROAD_RULE_TIME + E_SCORE_TYPE_TIME, or ROAD_RULE_CRASH + E_SCORE_TYPE_CRASH) and its
//     target road id (GetCgsIDTarget(1)) must match the scored road.
//   * When the score record actually carries a score for that type, it is pushed under the
//     matching freeburn skill: the time score is scaled by the 0.001 rodata epsilon
//     (flt_82013F90), the crash score is pushed raw. Neither banks immediately.
// ----------------------------------------------------------------------------
void ChallengeManager::HandleRoadRuleScore(BrnStreetData::ChallengePlayerScoreEntry lChallengeScore,
                                           BrnStreetData::ScoreType leScoreType, CgsID lRoadID)
{
    if (meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RUNNING)   // X360 lwz +0xE08; cmpwi 2
    {
        CGS_ASSERT(mpCurrentChallenge, "mpCurrentChallenge");             // X360 line 5849
        CGS_ASSERT(miCurrentChallengeAction < mpCurrentChallenge->GetNumActions(),
                   "miCurrentChallengeAction < mpCurrentChallenge->GetNumActions()");   // X360 line 5851

        const BrnResource::ChallengeListEntryAction* lpAction =
            mpCurrentChallenge->GetAction(miCurrentChallengeAction);

        if (lpAction->GetActionType() ==
                BrnResource::ChallengeListEntryAction::E_CHALLENGE_ACTION_ROAD_RULE_TIME &&
            leScoreType == BrnStreetData::E_SCORE_TYPE_TIME)
        {
            if (lpAction->GetCgsIDTarget(1) == lRoadID)   // X360 (u64)action+0x38 == lRoadID
            {
                if (lChallengeScore.ContainsData(BrnStreetData::E_SCORE_TYPE_TIME))
                {
                    SetCurrentSkillScore(E_FREEBURN_SKILL_ROAD_RULE_TIME,
                        static_cast<f32>(lChallengeScore.GetScore(BrnStreetData::E_SCORE_TYPE_TIME)) * 0.001f,
                        false);
                }
            }
        }
        else if (lpAction->GetActionType() ==
                     BrnResource::ChallengeListEntryAction::E_CHALLENGE_ACTION_ROAD_RULE_CRASH &&
                 leScoreType == BrnStreetData::E_SCORE_TYPE_CRASH)
        {
            if (lpAction->GetCgsIDTarget(1) == lRoadID)
            {
                if (lChallengeScore.ContainsData(BrnStreetData::E_SCORE_TYPE_CRASH))
                {
                    SetCurrentSkillScore(E_FREEBURN_SKILL_ROAD_RULE_CRASH,
                        static_cast<f32>(lChallengeScore.GetScore(BrnStreetData::E_SCORE_TYPE_CRASH)),
                        false);
                }
            }
        }
    }
}

} // namespace BrnGameState
