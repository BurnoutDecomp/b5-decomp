// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wB_13.cpp
// ============================================================================
// Wave-B partfile (group 13) for BrnGameState::ChallengeManager. Store-for-store
// reconstruction over the keystone-frozen NAMED members (see BrnChallengeManager.h).
//
// BODIED HERE:
//   UpdateActionSuccess  -- X360 0x823460A0
//
// NOT bodied (reported as blocked): the group's other two members --
//   UpdateArbitration         @ 0x82351AF8
//   UpdateArbitrationSuccess  @ 0x82350340
// Both post the 0x20-byte id-153 E_ACTION_FREEBURN_CHALLENGE action (X360
// AddEvent(queue, &ev, 153, 0x20)), whose dedicated payload struct is NOT declared in
// any frozen header -- only the id-155/158/159 challenge actions exist in BrnGameActions.h
// (this is the SAME blocker the sibling partfiles wB_06 UpdateResults and wB_07 Begin/
// TriggerFreeburnChallenge report). The id-153 payload is integral to both bodies (the
// arbitration-accept spine), so neither can be reconstructed store-for-store without
// inventing that type or editing a header (out of scope). Exact attested layout is in the
// funcs_blocked report so a keystone follow-up can add the struct and unblock them.
// ============================================================================

#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"               // CGS_ASSERT
#include "SharedClasses/DataLists/ChallengeListEntry.h"          // ChallengeListEntry::GetAction, ChallengeListEntryAction accessors
#include "GameSource/GameState/BrnGameActions.h"                 // GameStateModuleIO::{FburnChallengeSuccessUpdateAction, E_ACTION_FREEBURN_CHALLENGE_SUCCESS_UPDATE}
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h" // TGameActionQueue::AddEvent, CgsModule::Event

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// UpdateActionSuccess -- X360 0x823460A0. Called by UpdateChallenge once a player's
// action score has been (re)evaluated: it folds that player's per-action success state
// into the manager's [player][action] grids according to the action's coop type.
//
// Three coop-type regimes (outer switch on lpAction->GetCoopType()):
//   * SIMULTANEOUS (3) -- latch this player into the last-second success bit sets and post
//     the id-158 FburnChallengeSuccessUpdateAction carrying the current last-second mask.
//   * ONCE/INDIVIDUAL/INDIVIDUAL_ACCUMULATION/CUMULATIVE/AVERAGE/COUNT (0,1,2,4,5,6) --
//     on SUCCESS, bank the contribution against the CURRENT challenge action; otherwise
//     re-derive this player's CONTRIBUTING/NONE status from the running skill value.
//   * anything else -- "Unknown action coop type" assert.
// ----------------------------------------------------------------------------
void ChallengeManager::UpdateActionSuccess(const BrnResource::ChallengeListEntryAction* lpAction,
                                           s32 liActionIndex,
                                           EChallengeStatus leChallengeStatus,
                                           EActiveRaceCarIndex leActiveRaceCarIndex,
                                           s32 liNumPlayers,
                                           TGameActionQueue* lpActionQueue,
                                           bool lbIsOnline)
{
    CGS_ASSERT(lpAction != 0, "lpAction");   // X360 @ BrnChallengeManager.cpp:1130
    CGS_ASSERT(leActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID &&
                   leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "( lePlayerActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID ) && ( lePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT )"); // :1131

    switch (lpAction->GetCoopType())   // X360 lbz +0x01(lpAction); jump table (cases 0-6)
    {
        case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_SIMULTANEOUS:  // 3
        {
            // Two independent "last-second window" bit indices: mLastSecondSuccessStatus is a
            // FastBitArray<60> (frame modulo 50 online / 60 offline), maPlayerSuccessUpdateArray
            // a FastBitArray<120> (frame modulo 100 online / 120 offline). X360 computes each via
            // a signed reciprocal-multiply divide then `frame - D*(frame/D)`.
            s32 liLastSecondBit;
            s32 liPlayerUpdateBit;
            if (lbIsOnline)
            {
                liLastSecondBit   = liNumPlayers - 50 * (liNumPlayers / 50);
                liPlayerUpdateBit = liNumPlayers - 100 * (liNumPlayers / 100);
            }
            else
            {
                liLastSecondBit   = liNumPlayers - 60 * (liNumPlayers / 60);
                liPlayerUpdateBit = liNumPlayers - 120 * (liNumPlayers / 120);
            }

            if (leChallengeStatus == E_CHALLENGE_STATUS_SUCCESS)   // X360 cmpwi r26,1
            {
                mLastSecondSuccessStatus.SetBit(static_cast<u32>(liLastSecondBit));
                maPlayerSuccessUpdateArray[leActiveRaceCarIndex].SetBit(static_cast<u32>(liPlayerUpdateBit));
                maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] =
                    GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE;   // 3
            }
            else
            {
                mLastSecondSuccessStatus.UnSetBit(static_cast<u32>(liLastSecondBit));
                maPlayerSuccessUpdateArray[leActiveRaceCarIndex].UnSetBit(static_cast<u32>(liPlayerUpdateBit));
                maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] =
                    GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_NONE;   // 0
            }

            // Post the id-158 success-update action carrying the whole last-second mask (X360
            // `ld r11,+0xE00; std r11` == copy the FastBitArray<60>'s single field) + this action.
            GameStateModuleIO::FburnChallengeSuccessUpdateAction lSuccessUpdate;
            lSuccessUpdate.mChallengeSuccessUpdate = mLastSecondSuccessStatus;
            lSuccessUpdate.miActionIndex           = liActionIndex;
            lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSuccessUpdate),
                                    GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE_SUCCESS_UPDATE,
                                    sizeof(lSuccessUpdate));   // X360 li r5,0x9E; li r6,0x10
            break;
        }

        case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_ONCE:                    // 0
        case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL:              // 1
        case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION: // 2
        case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_CUMULATIVE:              // 4
        case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_AVERAGE:                 // 5
        case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_COUNT:                   // 6
        {
            if (leChallengeStatus == E_CHALLENGE_STATUS_SUCCESS)   // X360 loc_823465B4: cmpwi r26,1 (fall-through)
            {
                CGS_ASSERT(mpCurrentChallenge != 0, "mpCurrentChallenge");   // :1197
                const BrnResource::ChallengeListEntryAction* lpCurrentAction =
                    mpCurrentChallenge->GetAction(liActionIndex);

                // Arm the convoy timer from the current action's convoy time on first success.
                if (lpCurrentAction->GetConvoyTime() > 0.0f && !mbConvoyTimerRunning)
                {
                    mfConvoyTimer        = lpCurrentAction->GetConvoyTime();
                    mbConvoyTimerRunning = true;
                }

                switch (lpCurrentAction->GetCoopType())   // X360 lbz +0x01(current action)
                {
                    case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_CUMULATIVE: // 4
                        // Draw down the shared remaining target by this player's action score
                        // (clamped at 0), accumulate the scaled contribution, mark CONTRIBUTING.
                        maiRemainingTarget[liActionIndex] -=
                            static_cast<s32>(maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex]);
                        if (maiRemainingTarget[liActionIndex] < 0)
                        {
                            maiRemainingTarget[liActionIndex] = 0;
                        }
                        {
                            const f32 lfScale = GetScaleFactor();
                            maafCumulativeContributions[leActiveRaceCarIndex][liActionIndex] =
                                lfScale * maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex] +
                                maafCumulativeContributions[leActiveRaceCarIndex][liActionIndex];
                        }
                        if (maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] !=
                            GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE)
                        {
                            maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] =
                                GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_CONTRIBUTING;   // 2
                        }
                        break;

                    case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL:              // 1
                    case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION: // 2
                    {
                        maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] =
                            GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE;   // 3
                        const f32 lfCumulative = maafCumulativeContributions[leActiveRaceCarIndex][liActionIndex];
                        const f32 lfCurrent    = maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex];
                        // X360 fsel: keep the larger of the accumulated / this-frame contribution.
                        maafCumulativeContributions[leActiveRaceCarIndex][liActionIndex] =
                            ((lfCumulative - lfCurrent) >= 0.0f) ? lfCumulative : lfCurrent;
                        break;
                    }

                    case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_COUNT: // 6
                        maafCumulativeContributions[leActiveRaceCarIndex][liActionIndex] =
                            maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex];
                        maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] =
                            GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_CONTRIBUTING;   // 2
                        break;

                    default:
                        maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] =
                            GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE;   // 3
                        break;
                }
            }
            else   // X360 loc_823467BC: leChallengeStatus != SUCCESS
            {
                if (maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] ==
                    GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE)   // already DONE -> leave it
                {
                    break;
                }

                const BrnResource::ChallengeListEntryAction::EChallengeCoopType leCoop = lpAction->GetCoopType();
                if (leCoop == BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL ||   // 1
                    leCoop == BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_COUNT)          // 6
                {
                    const s32 liSkill = KAI_CHALLENGE_ACTION_TYPE_TO_FREEBURN_SKILL[lpAction->GetActionType()];
                    if (liSkill == KI_FREEBURN_SKILL_COUNT_X360)   // 38 == "no skill" sentinel
                    {
                        break;
                    }
                    maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] =
                        (mafActiveSkillValue[liSkill] > 0.0f)
                            ? GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_CONTRIBUTING   // 2
                            : GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_NONE;          // 0
                }
                else if (leCoop == BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION) // 2
                {
                    const s32 liSkill = KAI_CHALLENGE_ACTION_TYPE_TO_FREEBURN_SKILL[lpAction->GetActionType()];
                    if (liSkill == KI_FREEBURN_SKILL_COUNT_X360)   // 38
                    {
                        break;
                    }
                    if (mafActiveSkillValue[liSkill] > 0.0f)
                    {
                        maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] =
                            GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_CONTRIBUTING;   // 2
                    }
                    else if (mafBankedActionScores[liSkill] != 0.0f)
                    {
                        break;   // banked value present -> leave status untouched
                    }
                    else
                    {
                        maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] =
                            GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_NONE;   // 0
                    }
                }
                // any other coop type -> no change
            }
            break;
        }

        default:
            CGS_ASSERT(false, "Unknown action coop type to deal with\n");   // X360 @ :1300
            break;
    }
}

}  // namespace BrnGameState
