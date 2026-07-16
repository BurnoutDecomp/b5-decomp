// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wB_15.cpp
// ----------------------------------------------------------------------------
// Wave-B partfile (group 15) for BrnGameState::ChallengeManager.
//
// Bodied here: UpdateAction (X360 0x8233B410).
//
// The other two group-15 giants -- UpdateChallenge (0x82347190) and ProcessEvent
// (0x8233D6A8) -- are reported blocked (see funcs_blocked / the final report):
//   * ProcessEvent switches on the raw X360 game-event discriminant and reads the
//     event payload off the untyped `const CgsModule::Event*` for a family of event
//     kinds (completed-stunt +0x68/+0x5C/+0x6C/+0x50/+0x54/+0x80..., in-progress-stunt,
//     near-miss, drift, boost-time-complete, action-success {CgsID@0, actionIndex@8},
//     challenge-reset {CgsID@0, action@8}, active-challenge {arci[]@0, id@0x20, count@0x28}).
//     None of these event-payload structs are defined in the frozen headers
//     (BrnGameEvents.h homes only the Fburn success / success-update, road-rules and
//     network events), so faithful named-member field access is impossible -- the reads
//     would have to be raw-offset casts (offset_hack lint failures).
//   * UpdateChallenge's body is dominated by runtime-value-formatted assert blocks
//     (BeginAssert -> StrStream -> operator<< of the banked score / challenge-id /
//     action-index -> AppendFormat -> FireAssert) that the CGS_ASSERT(cond,"literal")
//     convention cannot express, and it posts a 32-byte id-153 challenge-reset action
//     built on the stack whose 0x20 payload has no committed action struct.
//
// UpdateAction is a per-action state machine: it refreshes location/car/modifier gates,
// then dispatches on the action's EChallengeActionType, feeding the per-skill score into
// UpdateCurrentActionScore / GetCurrentSkillScore / IsSkillScoreCurrentlySuccessful, and
// finally recomputes the per-action remaining-target for the INDIVIDUAL / INDIVIDUAL_
// ACCUMULATION coop types. Returns the resulting EChallengeStatus (ONGOING / SUCCESS /
// RESET_IF_NEEDED).
//
// NOTE on IsSkillScoreCurrentlySuccessful (fixed in wave C): the header now declares the
// TRUE 7-param X360 shape (f1=lfScore, r5=lpAction, r6=lbIsCumulativeArbitration,
// r7=liTargetValueIndex, r8=liActionIndex, r9=leActiveRaceCarIndex,
// r10=lbLargerScoresAreBetter). Every callsite below passes the seven attested register
// values 1:1 (r6 always carries this caller's lbIsOnline; r7 is the GetTargetValue index).
// ============================================================================

#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h"
#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManagerDebugComponent.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/GameState/BrnGameEvents.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"
#include "SharedClasses/DataLists/ChallengeListEntry.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"

namespace BrnGameState
{
    // X360 0x8233B410.
    EChallengeStatus ChallengeManager::UpdateAction(
        s32                                                                            liActionIndex,
        const BrnResource::ChallengeListEntryAction*                                   lpAction,
        f32                                                                            lfTimeStep,
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*   lpActiveRaceCarOutputInterface,
        bool                                                                           lbIsOnline)
    {
        typedef BrnResource::ChallengeListEntryAction Action;

        (void)lfTimeStep;   // consumed by the (blocked) CheckCurrentLocation path only

        CGS_ASSERT(lpActiveRaceCarOutputInterface, "lpActiveRaceCarOutputInterface");
        CGS_ASSERT(lpAction, "lpAction");
        CGS_ASSERT(liActionIndex < BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE,
                   "liActionIndex < BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE");

        EChallengeStatus leChallengeStatus = E_CHALLENGE_STATUS_ONGOING;

        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface::RaceCarState* lpRaceCarState =
            lpActiveRaceCarOutputInterface->GetPlayerRaceCarState();
        CGS_ASSERT(lpRaceCarState, "lpRaceCarState");

        CGS_ASSERT(lpActiveRaceCarOutputInterface, "lpActiveRaceCarOutputInterface");

        // Player active-race-car index (baked "Player car index hasn't been set" assert).
        const EActiveRaceCarIndex leActiveRaceCarIndex =
            lpActiveRaceCarOutputInterface->GetPlayerActiveRaceCarIndex();

        if (!lbIsOnline)
        {
            maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex] = 0.0f;
        }

        const bool lbWasLocationOk = mabIsLocationOk[liActionIndex];
        mabIsLocationOk[liActionIndex] = CheckCurrentLocation(lpAction, lpActiveRaceCarOutputInterface);
        const bool lbIsCarOk        = CheckCurrentCar(mpCurrentChallenge, lpActiveRaceCarOutputInterface);
        const bool lbAreModifiersOk = CheckForModifiers(lpAction, lpActiveRaceCarOutputInterface);
        const bool lbLocationChanged =
            UpdateLocationOKStatusChange(lbWasLocationOk, mabIsLocationOk[liActionIndex]);

        if ((((mabIsLocationOk[liActionIndex] || lbLocationChanged) && lbIsCarOk && lbAreModifiersOk)) || lbIsOnline)
        {
            // The X360 range assert here (`cmplwi 0x29` == 41, the drifted action-type count)
            // is the one INLINED from the committed ChallengeListEntryAction::GetActionType()
            // (ChallengeListEntry.h:318, whose X360-drift note records the 22-vs-41 threshold
            // call) -- do not emit a second explicit copy.
            const Action::EChallengeActionType leActionType = lpAction->GetActionType();

            const Action::EChallengeCoopType leCoopType = lpAction->GetCoopType();
            f32 lfCurrentSkillScore = 0.0f;

            switch (leActionType)
            {
            case Action::E_CHALLENGE_ACTION_MINIMUM_SPEED:               // 0
            {
                CGS_ASSERT(leCoopType != Action::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION,
                           "Can't currently accumulate minimum speed\n");

                f32 lfSpeed;
                if (lbIsOnline)
                {
                    CGS_ASSERT(liActionIndex < mpCurrentChallenge->GetNumActions(),
                               "liActionIndex < mpCurrentChallenge->GetNumActions()");
                    lfSpeed = mafCumulativeActionScores[liActionIndex];
                }
                else
                {
                    lfSpeed = lpRaceCarState->mfMaxSpeedMPH;
                    CGS_ASSERT(liActionIndex < mpCurrentChallenge->GetNumActions(),
                               "liActionIndex < mpCurrentChallenge->GetNumActions()");
                    maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex] = lpRaceCarState->mfMaxSpeedMPH;
                }

                if (lfSpeed >= static_cast<f32>(lpAction->GetTargetValue(0)))
                {
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                }
                else if ((leCoopType == Action::E_CHALLENGE_COOP_TYPE_CUMULATIVE ||
                          leCoopType == Action::E_CHALLENGE_COOP_TYPE_AVERAGE) &&
                         lfSpeed > 0.0f && !lbIsOnline)
                {
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                }
                break;
            }

            case Action::E_CHALLENGE_ACTION_IN_AIR:                      // 1 -> AIR
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_AIR, leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_AIR, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case Action::E_CHALLENGE_ACTION_AIR_DISTANCE:                // 2 -> AIR_DISTANCE
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_AIR_DISTANCE, leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_AIR_DISTANCE, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case Action::E_CHALLENGE_ACTION_LEAP_CARS:                   // 3 -> LEAP_CARS
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_LEAP_CARS, leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_LEAP_CARS, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case Action::E_CHALLENGE_ACTION_DRIFT:                       // 4 -> DRIFT
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_DRIFT, leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_DRIFT, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case Action::E_CHALLENGE_ACTION_NEAR_MISS:                   // 5 -> NEAR_MISS
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_NEAR_MISS, leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_NEAR_MISS, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case Action::E_CHALLENGE_ACTION_BARREL_ROLLS:                // 6 -> BARREL_ROLL
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_BARREL_ROLL, leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_BARREL_ROLL, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case Action::E_CHALLENGE_ACTION_ONCOMING:                    // 7 -> ONCOMING(0)
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_ONCOMING, leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_ONCOMING, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case Action::E_CHALLENGE_ACTION_FLATSPIN:                    // 8 -> FLATSPIN
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_FLATSPIN, leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_FLATSPIN, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case Action::E_CHALLENGE_ACTION_LAND_SUCCESSFUL:             // 9 -> SUCCESSFUL_LANDING
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_SUCCESSFUL_LANDING, leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_SUCCESSFUL_LANDING, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                else if (mabActiveSkillThisFrame[E_FREEBURN_SKILL_SUCCESSFUL_LANDING] && lfCurrentSkillScore == 0.0f)
                    leChallengeStatus = E_CHALLENGE_STATUS_RESET_IF_NEEDED;
                break;

            case Action::E_CHALLENGE_ACTION_ROAD_RULE_TIME:             // 10 -> ROAD_RULE_TIME
                CGS_ASSERT(leCoopType != Action::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION,
                           "Can't currently accumulate road rule times\n");
                if (leCoopType == Action::E_CHALLENGE_COOP_TYPE_CUMULATIVE ||
                    leCoopType == Action::E_CHALLENGE_COOP_TYPE_AVERAGE)
                    UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_ROAD_RULE_TIME, leActiveRaceCarIndex, 1000.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_ROAD_RULE_TIME, leCoopType, false, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, false))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case Action::E_CHALLENGE_ACTION_ROAD_RULE_CRASH:            // 11 -> ROAD_RULE_CRASH
                CGS_ASSERT(leCoopType != Action::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION,
                           "Can't currently accumulate road rule times\n");
                if (leCoopType == Action::E_CHALLENGE_COOP_TYPE_CUMULATIVE ||
                    leCoopType == Action::E_CHALLENGE_COOP_TYPE_AVERAGE)
                    UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_ROAD_RULE_CRASH, leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_ROAD_RULE_CRASH, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case Action::E_CHALLENGE_ACTION_PLAYER_POWER_PARKING:       // 12 -> PLAYER_POWER_PARKING
                CGS_ASSERT(leCoopType != Action::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION,
                           "Can't currently accumulate power parking\n");
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_PLAYER_POWER_PARKING, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, (lbIsOnline ? 0 : 1), liActionIndex, leActiveRaceCarIndex, true))
                {
                    if (mabActiveSkillThisFrame[E_FREEBURN_SKILL_PLAYER_POWER_PARKING])
                    {
                        CGS_ASSERT(liActionIndex < mpCurrentChallenge->GetNumActions(),
                                   "liActionIndex < mpCurrentChallenge->GetNumActions()");
                        maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex] = 1.0f;
                    }
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                }
                break;

            case Action::E_CHALLENGE_ACTION_TRAFFIC_POWER_PARKING:      // 13 -> TRAFFIC_POWER_PARKING
                CGS_ASSERT(leCoopType != Action::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION,
                           "Can't currently accumulate power parking\n");
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_TRAFFIC_POWER_PARKING, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, (lbIsOnline ? 0 : 1), liActionIndex, leActiveRaceCarIndex, true))
                {
                    if (mabActiveSkillThisFrame[E_FREEBURN_SKILL_TRAFFIC_POWER_PARKING])
                    {
                        CGS_ASSERT(liActionIndex < mpCurrentChallenge->GetNumActions(),
                                   "liActionIndex < mpCurrentChallenge->GetNumActions()");
                        maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex] = 1.0f;
                    }
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                }
                break;

            case Action::E_CHALLENGE_ACTION_CRASH_INTO_PLAYER:          // 14
            {
                CGS_ASSERT(leCoopType != Action::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION,
                           "Can't currently accumulate crashing into player cars\n");

                s32 liCrashValue;
                if (lbIsOnline)
                {
                    CGS_ASSERT(liActionIndex < mpCurrentChallenge->GetNumActions(),
                               "liActionIndex < mpCurrentChallenge->GetNumActions()");
                    liCrashValue = static_cast<s32>(mafCumulativeActionScores[liActionIndex]);
                }
                else
                {
                    liCrashValue = maiCrashedWithChallengePlayer[leActiveRaceCarIndex];
                    CGS_ASSERT(liActionIndex < mpCurrentChallenge->GetNumActions(),
                               "liActionIndex < mpCurrentChallenge->GetNumActions()");
                    maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex] =
                        static_cast<f32>(maiCrashedWithChallengePlayer[leActiveRaceCarIndex]);
                }

                if (liCrashValue >= lpAction->GetTargetValue(0))
                {
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                }
                else if ((leCoopType == Action::E_CHALLENGE_COOP_TYPE_CUMULATIVE ||
                          leCoopType == Action::E_CHALLENGE_COOP_TYPE_AVERAGE) &&
                         liCrashValue > 0 && !lbIsOnline)
                {
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                }
                break;
            }

            case Action::E_CHALLENGE_ACTION_BURNOUTS:                   // 15 -> BURNOUTS
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_BURNOUTS, leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_BURNOUTS, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case Action::E_CHALLENGE_ACTION_MEET_UP:                    // 16 -- always succeeds when gated in
                leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case Action::E_CHALLENGE_ACTION_BILLBOARD:                  // 17 -> BILLBOARDS
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_BILLBOARDS, leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_BILLBOARDS, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case Action::E_CHALLENGE_ACTION_BOOST_TIME:                 // 18 -> BOOST_TIME
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_BOOST_TIME, leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_BOOST_TIME, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case Action::E_CHALLENGE_ACTION_BARREL_ROLLS_REVERSE:       // 19 -> BARREL_ROLL_REVERSE
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_BARREL_ROLL_REVERSE, leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_BARREL_ROLL_REVERSE, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case Action::E_CHALLENGE_ACTION_FLATSPIN_REVERSE:           // 20 -> FLATSPIN_REVERSE(2)
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_FLATSPIN_REVERSE, leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_FLATSPIN_REVERSE, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case Action::E_CHALLENGE_ACTION_LAND_SUCCESSFUL_REVERSE:    // 21 -> SUCCESSFUL_LANDING_REVERSE(6)
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_SUCCESSFUL_LANDING_REVERSE, leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_SUCCESSFUL_LANDING_REVERSE, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                else if (mabActiveSkillThisFrame[E_FREEBURN_SKILL_SUCCESSFUL_LANDING_REVERSE] && lfCurrentSkillScore == 0.0f)
                    leChallengeStatus = E_CHALLENGE_STATUS_RESET_IF_NEEDED;
                break;

            // ---- X360-drift stunt-run action types (22..40) -> drifted skills (19..37). ----
            // These skill ids have no recoverable PS3 enumerator names; the X360 asm uses them as
            // plain numeric casts (see BrnChallengeManager.h EFreeburnSkill note), which is
            // reproduced verbatim here.
            case 22:   // -> drift skill 19; RESET_IF_NEEDED when the score was banked this frame at 0.
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(19), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(19), leCoopType, true, 1.0f);
                if (mabBankedSkillThisFrame[19] && lfCurrentSkillScore == 0.0f)
                    leChallengeStatus = E_CHALLENGE_STATUS_RESET_IF_NEEDED;
                else if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 23:   // -> drift skill 20; RESET_IF_NEEDED when the score was banked this frame.
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(20), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(20), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                else if (mabBankedSkillThisFrame[20])
                    leChallengeStatus = E_CHALLENGE_STATUS_RESET_IF_NEEDED;
                break;

            case 24:   // -> drift skill 21
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(21), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(21), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 25:   // -> drift skill 22
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(22), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(22), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 26:   // -> drift skill 23
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(23), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(23), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 27:   // -> drift skill 24
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(24), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(24), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 28:   // -> drift skill 25
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(25), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(25), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 29:   // -> drift skill 26
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(26), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(26), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 30:   // -> drift skill 27
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(27), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(27), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 31:   // -> drift skill 28
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(28), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(28), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 32:   // -> drift skill 29
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(29), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(29), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 33:   // -> drift skill 30
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(30), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(30), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 34:   // -> drift skill 31
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(31), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(31), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 35:   // -> drift skill 32
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(32), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(32), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 36:   // -> drift skill 33
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(33), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(33), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 37:   // -> drift skill 34
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(34), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(34), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 38:   // -> drift skill 35
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(35), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(35), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 39:   // -> drift skill 36
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(36), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(36), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 40:   // -> drift skill 37
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(37), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(37), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            default:
                leChallengeStatus = E_CHALLENGE_STATUS_ONGOING;
                CGS_ASSERT(false, "Unknown challenge type.");
                break;
            }
        }

        // Recompute the per-action remaining target for the two individual coop types.
        const Action::EChallengeCoopType leTailCoopType = lpAction->GetCoopType();
        if (leTailCoopType == Action::E_CHALLENGE_COOP_TYPE_INDIVIDUAL)
        {
            s32 liRemaining = lpAction->GetTargetValue(0) -
                              static_cast<s32>(maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex]);
            if (liRemaining < 0)
                liRemaining = 0;
            maiRemainingTarget[liActionIndex] = liRemaining;
        }
        else if (leTailCoopType == Action::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION &&
                 leChallengeStatus == E_CHALLENGE_STATUS_SUCCESS)
        {
            s32 liRemaining = maiRemainingTarget[liActionIndex] -
                              static_cast<s32>(maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex]);
            if (liRemaining < 0)
                liRemaining = 0;
            maiRemainingTarget[liActionIndex] = liRemaining;
        }

        return leChallengeStatus;
    }
}
