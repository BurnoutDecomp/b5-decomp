// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wC_06.cpp
// ============================================================================
// BrnGameState::ChallengeManager -- wave-C partfile (group G7): the two orchestration
// giants of the freeburn-challenge runtime.
//
//   ProcessEvent     (X360 0x8233D6A8)
//   UpdateChallenge  (X360 0x82347190)
//
// SOURCE-OF-TRUTH: the X360 ARTIST asm is authoritative for every store, branch, early-out,
// assert and call. Raw offsets are mapped onto the keystone-frozen NAMED members/accessors of
// BrnChallengeManager.h, the frozen event payloads in BrnGameEvents.h and the frozen action
// payloads in BrnGameActions.h. No header was edited by this partfile.
//
// ProcessEvent DISCRIMINANTS: the X360 jump table is RELATIVE (r11 = leEventType - 54, 120
// slots), so the raw table cases 0/1/11/12/13/16/17/65/66/111/112/113/119 are the ABSOLUTE
// event types 54/55/65/66/67/70/71/119/120/165/166/167/173. The frozen E_EVENT_* tags carry
// exactly those values (each case is annotated `== raw N`). The two placeholder tags
// E_EVENT_FREEBURN_CHALLENGE_SUCCESS_UPDATE/SUCCESS duplicate 165/166 and are deliberately
// never switched on by name (keystone pitfall F5 note).
//
// SetCurrentSkillScore's third argument (r6 at every callsite) is the "bank immediately"
// flag: false for the in-progress event flavours (POWER_PARK/NEAR_MISS/DRIFT/ONCOMING/
// InProgressStunt) and true for the completed flavours (BOOST_TIME_COMPLETE/
// NEAR_MISS_CHAIN_COMPLETED/ONCOMING_COMPLETED/CompletedStunt) -- taken verbatim from the asm.
//
// The X360 skill ids 19/20/37 are the drifted (X360-only, unnamed) stunt-run skills; they are
// written as attested numeric casts exactly like the committed UpdateStuntScores body does.
//
// Formatted diagnostics follow the keystone convention: pure-literal messages fold to
// CGS_ASSERT; the ones that stream runtime values use the committed local-StrStream idiom
// (X360 file/line noted in a comment, __FILE__/__LINE__ used by the committed macro machinery).
// ============================================================================

#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h"
#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManagerDebugComponent.h"

#include "GameSource/GameState/BrnGameActions.h"                  // FreeburnChallengeAction (153) / FburnChallengeSuccessAction (159)
#include "GameSource/GameState/BrnGameEvents.h"                   // the ProcessEvent payload family
#include "GameSource/Network/BrnNetworkModuleIO.h"                // BrnNetworkModuleIO::E_CHALLENGE_EVENT_* (values 0..3)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // TGameActionQueue::AddEvent / CgsModule::Event
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"
#include "SharedClasses/DataLists/ChallengeList.h"                // ChallengeList::GetChallengeData(CgsID)
#include "SharedClasses/DataLists/ChallengeListEntry.h"           // GetChallengeID / GetNumActions / GetAction / GetActionType / GetCoopType / GetCombineAction / GetTargetValue
#include "GameSource/GameState/BrnGameStateModule.h"              // GameStateModule::GetPlayerActiveRaceCarIndex
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // GetPlayerActiveRaceCarIndex
#include "GameShared/GameClasses/Core/CgsAssert.h"                // CGS_ASSERT + Begin/Fire/EndAssert
#include "GameShared/GameClasses/Development/CgsStrStream.h"      // CgsDev::StrStream (runtime-value assert messages)

#include <cmath>                                                  // std::floor (the de-inlined fsel/magic-constant floor)
#include <cstring>                                                // std::memcpy (XMemCpy)

namespace BrnGameState
{

// ============================================================================
// ProcessEvent  (X360 0x8233D6A8)
// ============================================================================
// Jump-table dispatch of the world/stunt/network game events the challenge system scores off.
// Every arm either feeds SetCurrentSkillScore or mutates the challenge bookkeeping; the
// default arm does nothing.
//
// Register map: r3 this, r4 leEventType (the switch selector, `addi r11, r4, -0x36`),
// r5 lpEvent (kept in r24 throughout).
void ChallengeManager::ProcessEvent(GameStateModuleIO::EGameEventType leEventType,
                                    const CgsModule::Event* lpEvent)
{
    switch (leEventType)
    {
        // --------------------------------------------------------------------
        case GameStateModuleIO::E_EVENT_POWER_PARK_RESULT:            // == raw 54
        {
            const GameStateModuleIO::PowerParkResultEvent* lpPowerParkResultEvent =
                reinterpret_cast<const GameStateModuleIO::PowerParkResultEvent*>(lpEvent);

            if (lpPowerParkResultEvent->meOutcome == 1)               // E_PPO_SUCCESS
            {
                if (lpPowerParkResultEvent->miOtherPlayersInvolved >= 2)
                {
                    SetCurrentSkillScore(E_FREEBURN_SKILL_PLAYER_POWER_PARKING,
                                         static_cast<f32>(lpPowerParkResultEvent->miOverallRating), false);
                }
                else
                {
                    SetCurrentSkillScore(E_FREEBURN_SKILL_TRAFFIC_POWER_PARKING,
                                         static_cast<f32>(lpPowerParkResultEvent->miOverallRating), false);
                }
            }
            break;
        }

        // --------------------------------------------------------------------
        case GameStateModuleIO::E_EVENT_BOOST_TIME_COMPLETE:          // == raw 55
        {
            const GameStateModuleIO::BoostTimeCompleteEvent* lpBoostTimeComplete =
                reinterpret_cast<const GameStateModuleIO::BoostTimeCompleteEvent*>(lpEvent);

            CGS_ASSERT(lpBoostTimeComplete, "lpBoostTimeComplete");   // BrnChallengeManager.cpp:5816

            SetCurrentSkillScore(E_FREEBURN_SKILL_BOOST_TIME, lpBoostTimeComplete->mfTimeSpentBoosting, true);
            break;
        }

        // --------------------------------------------------------------------
        case GameStateModuleIO::E_EVENT_NEAR_MISS:                    // == raw 65
        {
            const GameStateModuleIO::NearMissEvent* lpNearMissEvent =
                reinterpret_cast<const GameStateModuleIO::NearMissEvent*>(lpEvent);

            CGS_ASSERT(lpNearMissEvent, "lpNearMissEvent");           // :5443

            SetCurrentSkillScore(E_FREEBURN_SKILL_NEAR_MISS,
                                 static_cast<f32>(lpNearMissEvent->miCount), false);
            break;
        }

        // --------------------------------------------------------------------
        case GameStateModuleIO::E_EVENT_NEAR_MISS_CHAIN_COMPLETED:    // == raw 66
        {
            const GameStateModuleIO::NearMissChainCompleteEvent* lpNearMissCompleteEvent =
                reinterpret_cast<const GameStateModuleIO::NearMissChainCompleteEvent*>(lpEvent);

            CGS_ASSERT(lpNearMissCompleteEvent, "lpNearMissCompleteEvent");   // :5454

            SetCurrentSkillScore(E_FREEBURN_SKILL_NEAR_MISS,
                                 static_cast<f32>(lpNearMissCompleteEvent->miCount), true);
            break;
        }

        // --------------------------------------------------------------------
        case GameStateModuleIO::E_EVENT_DRIFTING:                     // == raw 67
        {
            const GameStateModuleIO::DriftingEvent* lpDriftEvent =
                reinterpret_cast<const GameStateModuleIO::DriftingEvent*>(lpEvent);

            CGS_ASSERT(lpDriftEvent, "lpDriftEvent");                 // :5432

            SetCurrentSkillScore(E_FREEBURN_SKILL_DRIFT, lpDriftEvent->mfDistance, false);
            break;
        }

        // --------------------------------------------------------------------
        case GameStateModuleIO::E_EVENT_ONCOMING:                     // == raw 70
        {
            const GameStateModuleIO::OncomingEvent* lpOncomingEvent =
                reinterpret_cast<const GameStateModuleIO::OncomingEvent*>(lpEvent);

            SetCurrentSkillScore(E_FREEBURN_SKILL_ONCOMING, lpOncomingEvent->mfDistance, false);
            break;
        }

        // --------------------------------------------------------------------
        case GameStateModuleIO::E_EVENT_ONCOMING_COMPLETED:           // == raw 71
        {
            const GameStateModuleIO::OncomingCompletedEvent* lpOncomingCompletedEvent =
                reinterpret_cast<const GameStateModuleIO::OncomingCompletedEvent*>(lpEvent);

            SetCurrentSkillScore(E_FREEBURN_SKILL_ONCOMING, lpOncomingCompletedEvent->mfDistance, true);
            break;
        }

        // --------------------------------------------------------------------
        case GameStateModuleIO::E_EVENT_COMPLETED_STUNT:              // == raw 119
        {
            const GameStateModuleIO::CompletedStuntEvent* lpCompletedStuntEvent =
                reinterpret_cast<const GameStateModuleIO::CompletedStuntEvent*>(lpEvent);

            CGS_ASSERT(lpCompletedStuntEvent, "lpCompletedStuntEvent");   // :5635

            // Drift: either of the two drift bits scores the completed drift distance.
            if ((lpCompletedStuntEvent->muStuntActionComplete & 0x40) == 0x40 ||
                (lpCompletedStuntEvent->muStuntActionComplete & 0x80) == 0x80)
            {
                SetCurrentSkillScore(E_FREEBURN_SKILL_DRIFT,
                                     lpCompletedStuntEvent->mfCompletedDriftDistance, true);
            }

            // Landing: a 1.0f/0.0f score, mirrored onto the reverse twin when in reverse.
            if ((lpCompletedStuntEvent->muStuntActionComplete & 0x10) == 0x10)
            {
                const f32 lfLandingScore = lpCompletedStuntEvent->mbSuccessfulLanding ? 1.0f : 0.0f;
                SetCurrentSkillScore(E_FREEBURN_SKILL_SUCCESSFUL_LANDING, lfLandingScore, true);
                if (lpCompletedStuntEvent->mbInReverse)
                {
                    SetCurrentSkillScore(E_FREEBURN_SKILL_SUCCESSFUL_LANDING_REVERSE, lfLandingScore, true);
                }
            }

            if ((lpCompletedStuntEvent->muStuntActionComplete & 1) == 1)
            {
                SetCurrentSkillScore(E_FREEBURN_SKILL_BARREL_ROLL,
                                     static_cast<f32>(lpCompletedStuntEvent->miCompletedBarrelRolls), true);
                if (lpCompletedStuntEvent->mbInReverse)
                {
                    SetCurrentSkillScore(E_FREEBURN_SKILL_BARREL_ROLL_REVERSE,
                                         static_cast<f32>(lpCompletedStuntEvent->miCompletedBarrelRolls), true);
                }
            }

            if ((lpCompletedStuntEvent->muStuntActionComplete & 2) == 2)
            {
                SetCurrentSkillScore(E_FREEBURN_SKILL_FLATSPIN,
                                     lpCompletedStuntEvent->mfCompletedAirSpinAngle * 57.29578f, true);
                if (lpCompletedStuntEvent->mbInReverse)
                {
                    SetCurrentSkillScore(E_FREEBURN_SKILL_FLATSPIN_REVERSE,
                                         lpCompletedStuntEvent->mfCompletedAirSpinAngle * 57.29578f, true);
                }
            }

            if ((lpCompletedStuntEvent->muStuntActionComplete & 0x800) == 0x800)
            {
                // X360-drift skill 37 (unnamed enumerator; attested numeric id).
                SetCurrentSkillScore(static_cast<EFreeburnSkill>(37),
                                     static_cast<f32>(lpCompletedStuntEvent->miCompletedSkill37Count), true);
            }

            if ((lpCompletedStuntEvent->muStuntActionComplete & 0x100) == 0x100)
            {
                SetCurrentSkillScore(E_FREEBURN_SKILL_AIR, lpCompletedStuntEvent->mfCompletedAirTime, true);
            }

            if ((lpCompletedStuntEvent->muStuntActionComplete & 0x200) == 0x200)
            {
                SetCurrentSkillScore(E_FREEBURN_SKILL_AIR_DISTANCE,
                                     lpCompletedStuntEvent->mfCompletedAirDistance, true);
            }

            if ((lpCompletedStuntEvent->muStuntActionComplete & 0x400) == 0x400)
            {
                // Stunt-run slot: find the challenge's (X360-drift) type-23 action; its
                // GetTargetValue(1) is the 1-BASED stunt-run slot K, so the event arrays read [K-1].
                if (mpCurrentChallenge != NULL)
                {
                    s32 liStuntRunAction = -1;
                    for (s32 liActionIndex = 0;
                         liActionIndex < mpCurrentChallenge->GetNumActions();
                         ++liActionIndex)
                    {
                        if (static_cast<s32>(mpCurrentChallenge->GetAction(liActionIndex)->GetActionType()) == 23)
                        {
                            liStuntRunAction = liActionIndex;
                        }
                    }

                    if (liStuntRunAction >= 0)
                    {
                        const s32 liStuntRunSlot =
                            mpCurrentChallenge->GetAction(liStuntRunAction)->GetTargetValue(1);
                        if (lpCompletedStuntEvent->mabStuntRunScored[liStuntRunSlot - 1])
                        {
                            // X360-drift skill 20 (unnamed enumerator; attested numeric id).
                            SetCurrentSkillScore(static_cast<EFreeburnSkill>(20),
                                                 lpCompletedStuntEvent->mafStuntRunScores[liStuntRunSlot - 1], true);
                        }
                    }
                }

                if (lpCompletedStuntEvent->mbStuntRunEnded)
                {
                    // X360-drift skill 19 (unnamed enumerator; attested numeric id).
                    SetCurrentSkillScore(static_cast<EFreeburnSkill>(19), 0.0f, true);
                }
            }
            break;
        }

        // --------------------------------------------------------------------
        case GameStateModuleIO::E_EVENT_INPROGRESS_STUNT:             // == raw 120
        {
            const GameStateModuleIO::InProgressStuntEvent* lpInProgressStuntEvent =
                reinterpret_cast<const GameStateModuleIO::InProgressStuntEvent*>(lpEvent);

            CGS_ASSERT(lpInProgressStuntEvent, "lpInProgressStuntEvent");   // :5466

            if ((lpInProgressStuntEvent->muStuntActionInProgress & 1) == 1)
            {
                // Radians -> degrees -> whole rolls (the X360 inlines floor() as the
                // magic-constant fsel/round-and-adjust sequence).
                const f32 lfWholeBarrelRolls = static_cast<f32>(std::floor(
                    lpInProgressStuntEvent->mfInProgressBarrelRollAngle * 57.29578f * 0.0027777778f + 0.5f));

                SetCurrentSkillScore(E_FREEBURN_SKILL_BARREL_ROLL, lfWholeBarrelRolls, false);
                if (lpInProgressStuntEvent->mbInReverse)
                {
                    SetCurrentSkillScore(E_FREEBURN_SKILL_BARREL_ROLL_REVERSE, lfWholeBarrelRolls, false);
                }
            }

            if ((lpInProgressStuntEvent->muStuntActionInProgress & 2) == 2)
            {
                SetCurrentSkillScore(E_FREEBURN_SKILL_FLATSPIN,
                                     lpInProgressStuntEvent->mfInProgressAirSpinAngle * 57.29578f, false);
                if (lpInProgressStuntEvent->mbInReverse)
                {
                    SetCurrentSkillScore(E_FREEBURN_SKILL_FLATSPIN_REVERSE,
                                         lpInProgressStuntEvent->mfInProgressAirSpinAngle * 57.29578f, false);
                }
            }

            if ((lpInProgressStuntEvent->muStuntActionInProgress & 0x20) == 0x20)
            {
                SetCurrentSkillScore(E_FREEBURN_SKILL_AIR, lpInProgressStuntEvent->mfTimeInAir, false);
            }

            if ((lpInProgressStuntEvent->muStuntActionInProgress & 0x40) == 0x40)
            {
                SetCurrentSkillScore(E_FREEBURN_SKILL_AIR_DISTANCE,
                                     lpInProgressStuntEvent->mfDistanceInAir, false);
            }

            if ((lpInProgressStuntEvent->muStuntActionInProgress & 0x80) != 0x80)
            {
                break;
            }
            if (mpCurrentChallenge == NULL)
            {
                break;
            }
            if (meChallengeManagerStatus != E_CHALLENGE_MANAGER_STATUS_RUNNING)
            {
                break;
            }

            // Locate the challenge's convoy (X360-drift type 22) and stunt-run (type 23) actions.
            s32 liConvoyAction   = -1;
            s32 liStuntRunAction = -1;
            for (s32 liActionIndex = 0;
                 liActionIndex < mpCurrentChallenge->GetNumActions();
                 ++liActionIndex)
            {
                if (static_cast<s32>(mpCurrentChallenge->GetAction(liActionIndex)->GetActionType()) == 22)
                {
                    liConvoyAction = liActionIndex;
                }
                if (static_cast<s32>(mpCurrentChallenge->GetAction(liActionIndex)->GetActionType()) == 23)
                {
                    liStuntRunAction = liActionIndex;
                }
            }

            if (liConvoyAction >= 0)
            {
                // Which challenge player is currently banking the longest convoy?
                s32                 liLongestConvoy       = 0;
                ::EActiveRaceCarIndex leLongestConvoyPlayer = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;
                for (::EActiveRaceCarIndex lePlayer = ::E_ACTIVE_RACE_CAR_INDEX_0;
                     lePlayer < ::E_ACTIVE_RACE_CAR_INDEX_COUNT; lePlayer++)
                {
                    if (mabPlayerStartedChallenge[lePlayer])
                    {
                        const f32 lfContribution = maafCumulativeContributions[lePlayer][liConvoyAction];
                        if (lfContribution > static_cast<f32>(liLongestConvoy))
                        {
                            leLongestConvoyPlayer = lePlayer;
                            liLongestConvoy       = static_cast<s32>(lfContribution);
                        }
                    }
                }

                bool lbLocalConvoyIsLongest =
                    lpInProgressStuntEvent->miConvoyMemberCount > liLongestConvoy;
                if (lpInProgressStuntEvent->miConvoyMemberCount == liLongestConvoy)
                {
                    CGS_ASSERT(lpInProgressStuntEvent->miConvoyMemberCount > 0,
                               "Trying to score a zero length convoy\n");                        // :5571
                    CGS_ASSERT(leLongestConvoyPlayer != ::E_ACTIVE_RACE_CAR_INDEX_INVALID,
                               "The longest convoy doesn't have any cars in it!\n");             // :5572

                    // Tie-break: the convoy's tail car decides who owns the run.
                    lbLocalConvoyIsLongest =
                        lpInProgressStuntEvent->maConvoyMemberARCIs[lpInProgressStuntEvent->miConvoyMemberCount - 1]
                            == static_cast<s32>(leLongestConvoyPlayer);
                }

                if (lbLocalConvoyIsLongest)
                {
                    // Walk the convoy front-to-back to the local player, skipping (by counting
                    // down) any member that is not in the challenge.
                    s32 liPlayerPosition = 0;
                    for (s32 liMember = 0; liMember < lpInProgressStuntEvent->miConvoyMemberCount; ++liMember)
                    {
                        const s32 liMemberARCI = lpInProgressStuntEvent->maConvoyMemberARCIs[liMember];
                        if (!mabPlayerStartedChallenge[liMemberARCI])
                        {
                            --liPlayerPosition;
                        }
                        if (static_cast<s32>(mpGameStateModule->GetPlayerActiveRaceCarIndex()) == liMemberARCI)
                        {
                            break;
                        }
                        ++liPlayerPosition;
                    }

                    if (liPlayerPosition < 0)
                    {
                        // Runtime-valued diagnostic; X360 file/line BrnChallengeManager.cpp:5607.
                        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                        CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                        lStrStream << "Player in longest convoy but has an invalid position"
                                   << liPlayerPosition << "\n";
                        CgsDev::Assert::BeginAssert();
                        CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
                        CgsDev::Assert::EndAssert();
                    }

                    // X360-drift skill 19 (unnamed enumerator; attested numeric id).
                    SetCurrentSkillScore(static_cast<EFreeburnSkill>(19),
                                         static_cast<f32>(liPlayerPosition + 1), false);
                }
            }

            if (liStuntRunAction < 0)
            {
                break;
            }

            {
                const s32 liStuntRunSlot = mpCurrentChallenge->GetAction(liStuntRunAction)->GetTargetValue(1);
                const f32 lfStuntRunScore = lpInProgressStuntEvent->mafStuntRunScores[liStuntRunSlot - 1];
                if (lfStuntRunScore > 0.0f)
                {
                    // X360-drift skill 20 (unnamed enumerator; attested numeric id).
                    SetCurrentSkillScore(static_cast<EFreeburnSkill>(20), lfStuntRunScore, false);
                }
            }
            break;
        }

        // --------------------------------------------------------------------
        case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_ACTION_SUCCESS:   // == raw 165
        {
            const GameStateModuleIO::FreeburnChallengeActionSuccessEvent* lpActionSuccessEvent =
                reinterpret_cast<const GameStateModuleIO::FreeburnChallengeActionSuccessEvent*>(lpEvent);

            CGS_ASSERT(lpActionSuccessEvent, "lpActionSuccessEvent");   // :5299

            if (meChallengeManagerStatus != E_CHALLENGE_MANAGER_STATUS_RUNNING)
            {
                break;
            }
            // 64-bit challenge-id gate (`ld 0(event); ld 0xC0(challenge); cmpld`).
            if (lpActionSuccessEvent->mChallengeID != mpCurrentChallenge->GetChallengeID())
            {
                break;
            }

            CGS_ASSERT(lpActionSuccessEvent->miActionIndex < mpCurrentChallenge->GetNumActions(),
                       "lpActionSuccessEvent->miActionIndex < mpCurrentChallenge->GetNumActions()");   // :5309

            const BrnResource::ChallengeListEntryAction* lpAction =
                mpCurrentChallenge->GetAction(lpActionSuccessEvent->miActionIndex);
            CGS_ASSERT(lpAction, "lpAction");   // :5311

            if (lpAction->GetCombineAction() != BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_COUNT)
            {
                // Advance the arbitration cursor past the action that just succeeded, clamping
                // both cursors to the last action slot.
                miCurrentArbitrationIndex = lpActionSuccessEvent->miActionIndex + 1;
                if (miCurrentArbitrationIndex >= mpCurrentChallenge->GetNumActions() - 1)
                {
                    miCurrentArbitrationIndex = mpCurrentChallenge->GetNumActions() - 1;
                }
                if (miCurrentArbitrationIndex >= miCurrentChallengeAction)
                {
                    miCurrentChallengeAction = miCurrentArbitrationIndex;
                    if (miCurrentChallengeAction >= mpCurrentChallenge->GetNumActions() - 1)
                    {
                        miCurrentChallengeAction = mpCurrentChallenge->GetNumActions() - 1;
                    }
                }
            }

            if (lpAction->GetCoopType() == BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_CUMULATIVE)
            {
                for (::EActiveRaceCarIndex lePlayer = ::E_ACTIVE_RACE_CAR_INDEX_0;
                     lePlayer < ::E_ACTIVE_RACE_CAR_INDEX_COUNT; lePlayer++)
                {
                    if (mabPlayerStartedChallenge[lePlayer])
                    {
                        maaePlayersSuccessStatus[lePlayer][lpActionSuccessEvent->miActionIndex] =
                            GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE;
                    }
                }
            }
            break;
        }

        // --------------------------------------------------------------------
        case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_RESET:            // == raw 166
        {
            const GameStateModuleIO::FreeburnChallengeResetEvent* lpResetEvent =
                reinterpret_cast<const GameStateModuleIO::FreeburnChallengeResetEvent*>(lpEvent);

            CGS_ASSERT(lpResetEvent, "lpResetEvent");   // :5351

            if (meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RUNNING &&
                lpResetEvent->mChallengeID == mpCurrentChallenge->GetChallengeID())
            {
                miCurrentChallengeAction  = lpResetEvent->miActionIndex;
                miCurrentArbitrationIndex = lpResetEvent->miActionIndex;
                ResetCurrentChallengeData(miCurrentChallengeAction);

                if (miLastChallengeResetFrame < miFramesSinceNetworkStart)
                {
                    miLastChallengeResetFrame = miFramesSinceNetworkStart;
                }
            }
            break;
        }

        // --------------------------------------------------------------------
        case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_RESET_ALL_ACTIONS:   // == raw 167
        {
            const GameStateModuleIO::FreeburnChallengeResetEvent* lpResetEvent =
                reinterpret_cast<const GameStateModuleIO::FreeburnChallengeResetEvent*>(lpEvent);

            CGS_ASSERT(lpResetEvent, "lpResetEvent");   // :5381

            if (meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RUNNING &&
                lpResetEvent->mChallengeID == mpCurrentChallenge->GetChallengeID())
            {
                miCurrentChallengeAction  = 0;
                miCurrentArbitrationIndex = 0;

                for (s32 liActionIndex = 0; liActionIndex < lpResetEvent->miActionIndex + 1; ++liActionIndex)
                {
                    ResetActionData(liActionIndex);
                }

                if (miLastChallengeResetFrame < miFramesSinceNetworkStart)
                {
                    miLastChallengeResetFrame = miFramesSinceNetworkStart;
                }
            }
            break;
        }

        // --------------------------------------------------------------------
        case GameStateModuleIO::E_EVENT_ACTIVE_FREEBURN_CHALLENGE:            // == raw 173
        {
            const GameStateModuleIO::ActiveFburnChallengeEvent* lpActiveChallengeEvent =
                reinterpret_cast<const GameStateModuleIO::ActiveFburnChallengeEvent*>(lpEvent);

            CGS_ASSERT(lpActiveChallengeEvent, "lpActiveChallengeEvent");   // :5788

            if (meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_NONE)
            {
                for (s32 liIndex = 0; liIndex < lpActiveChallengeEvent->miNumPlayersInChallenge; ++liIndex)
                {
                    CGS_ASSERT(lpActiveChallengeEvent->maePlayersInChallengeARCI[liIndex] >= ::E_ACTIVE_RACE_CAR_INDEX_0,
                               "lpActiveChallengeEvent->maePlayersInChallengeARCI[liIndex] >= E_ACTIVE_RACE_CAR_INDEX_0");   // :5798
                    CGS_ASSERT(lpActiveChallengeEvent->maePlayersInChallengeARCI[liIndex] < ::E_ACTIVE_RACE_CAR_INDEX_COUNT,
                               "lpActiveChallengeEvent->maePlayersInChallengeARCI[liIndex] < E_ACTIVE_RACE_CAR_INDEX_COUNT");   // :5799

                    mabPlayerStartedChallenge[lpActiveChallengeEvent->maePlayersInChallengeARCI[liIndex]] = true;
                }

                mpCurrentChallenge =
                    mpFreeburnChallengeList->GetChallengeData(lpActiveChallengeEvent->mChallengeID);
            }
            break;
        }

        default:
            break;
    }
}

// ============================================================================
// UpdateChallenge  (X360 0x82347190)
// ============================================================================
// The RUNNING master loop. Drives every action slot from the current action forward:
// UpdateAction scores it, UpdateActionSuccess broadcasts the transition, the failure/reset
// flavours rewind the challenge (posting the id-153 FreeburnChallengeAction reset messages),
// and the per-player banked/current score pair is finally published as the id-159
// FburnChallengeSuccessAction.
//
// Register map: r3 this, f1 lfTimeStep (kept in f30), r5 liFramesSinceNetworkStart,
// r6 lpActiveRaceCarOutputInterface, r7 lpActionQueue, r8 lbIsOnline.
void ChallengeManager::UpdateChallenge(f32 lfTimeStep, s32 liFramesSinceNetworkStart,
                                       const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface,
                                       TGameActionQueue* lpActionQueue, bool lbIsOnline)
{
    CGS_ASSERT(mpCurrentChallenge, "mpCurrentChallenge");                       // :4255
    CGS_ASSERT(miCurrentArbitrationIndex <= miCurrentChallengeAction,
               "miCurrentArbitrationIndex <= miCurrentChallengeAction");        // :4256

    // The per-frame success broadcast is built up across the action loop and posted at the
    // tail; only the flags are cleared here (the scores are memcpy'd in just before the post).
    GameStateModuleIO::FburnChallengeSuccessAction lSuccessAction;
    lSuccessAction.mabSuccessfulActions[0] = false;
    lSuccessAction.mabSuccessfulActions[1] = false;

    // The accessor carries its own "Player car index hasn't been set" assert
    // (BrnRaceCarEntityModuleOutputInterface.h:980) -- do not duplicate it here.
    // `::` qualified: BrnGameState also declares a same-valued EActiveRaceCarIndex
    // (BrnTakedownManagerTypes.h); the interface accessor returns the global-scope one.
    const ::EActiveRaceCarIndex leActiveRaceCarIndex =
        lpActiveRaceCarOutputInterface->GetPlayerActiveRaceCarIndex();

    bool lbPostSuccessAction = false;

    for (s32 liActionIndex = miCurrentChallengeAction;
         liActionIndex < mpCurrentChallenge->GetNumActions();
         ++liActionIndex)
    {
        // GetAction inlines its own "liActionIndex >= 0" / "liActionIndex <
        // KI_MAX_ACTIONS_PER_CHALLENGE" range asserts (ChallengeListEntry.h:941/942).
        const BrnResource::ChallengeListEntryAction* lpAction = mpCurrentChallenge->GetAction(liActionIndex);
        CGS_ASSERT(lpAction, "lpAction");   // :4271

        // NOTE: the X360 passes a literal false for UpdateAction's lbIsOnline (`li r8, 0`),
        // NOT this function's lbIsOnline parameter.
        const EChallengeStatus leActionStatus =
            UpdateAction(liActionIndex, lpAction, lfTimeStep, lpActiveRaceCarOutputInterface, false);

        lSuccessAction.mabSuccessfulActions[liActionIndex] =
            (leActionStatus == E_CHALLENGE_STATUS_SUCCESS &&
             lpAction->GetCoopType() != BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_SIMULTANEOUS);
        lSuccessAction.mabAccumulationThisFrame[liActionIndex] = false;

        if (maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] ==
            GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE)
        {
            if (lSuccessAction.mabSuccessfulActions[liActionIndex])
            {
                // GetActionType inlines the X360 (drifted, 41-entry) range assert.
                const s32 liFreeburnSkill =
                    KAI_CHALLENGE_ACTION_TYPE_TO_FREEBURN_SKILL[lpAction->GetActionType()];
                if (liFreeburnSkill != KI_FREEBURN_SKILL_COUNT_X360 &&
                    mabBankedSkillThisFrame[liFreeburnSkill])
                {
                    const f32 lfCurrentActionScore = maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex];
                    if (lpAction->GetCoopType() ==
                        BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL)
                    {
                        if (lfCurrentActionScore >= maafCumulativeContributions[leActiveRaceCarIndex][liActionIndex])
                        {
                            if (lfCurrentActionScore < 0.0f)
                            {
                                // X360 file/line BrnChallengeManager.cpp:4337.
                                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                                CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                                lStrStream << "We have banked an action score but have not set a valid current score: "
                                           << lfCurrentActionScore
                                           << " Challenge ID: " << mpCurrentChallenge->GetChallengeID()
                                           << " Action Index: " << liActionIndex
                                           << "/" << mpCurrentChallenge->GetNumActions() << "\n";
                                CgsDev::Assert::BeginAssert();
                                CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
                                CgsDev::Assert::EndAssert();
                            }
                            lbPostSuccessAction = true;
                        }
                    }
                    else
                    {
                        if (lfCurrentActionScore < 0.0f)
                        {
                            // X360 file/line BrnChallengeManager.cpp:4348.
                            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                            lStrStream << "We have banked an action score but have not set a valid current score: "
                                       << lfCurrentActionScore
                                       << " Challenge ID: " << mpCurrentChallenge->GetChallengeID()
                                       << " Action Index: " << liActionIndex
                                       << "/" << mpCurrentChallenge->GetNumActions() << "\n";
                            CgsDev::Assert::BeginAssert();
                            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
                            CgsDev::Assert::EndAssert();
                        }
                        lbPostSuccessAction = true;
                    }
                }
            }
        }
        else if (lSuccessAction.mabSuccessfulActions[liActionIndex])
        {
            const f32 lfCurrentActionScore = maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex];
            if (lfCurrentActionScore < 0.0f)
            {
                // X360 file/line BrnChallengeManager.cpp:4290.
                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "We have banked an action score but have not set a valid current score: "
                           << lfCurrentActionScore
                           << " Challenge ID: " << mpCurrentChallenge->GetChallengeID()
                           << " Action Index: " << liActionIndex
                           << "/" << mpCurrentChallenge->GetNumActions() << "\n";
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
                CgsDev::Assert::EndAssert();
            }
            lbPostSuccessAction = true;
        }
        else if (lpAction->GetCoopType() ==
                 BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION)
        {
            const s32 liFreeburnSkill =
                KAI_CHALLENGE_ACTION_TYPE_TO_FREEBURN_SKILL[lpAction->GetActionType()];
            if (liFreeburnSkill != KI_FREEBURN_SKILL_COUNT_X360 &&
                mabBankedSkillThisFrame[liFreeburnSkill])
            {
                const f32 lfCurrentActionScore = maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex];
                if (lfCurrentActionScore < 0.0f)
                {
                    // X360 file/line BrnChallengeManager.cpp:4308.
                    char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                    CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                    lStrStream << "We have banked an action score but have not set a valid current score: "
                               << lfCurrentActionScore
                               << " Challenge ID: " << mpCurrentChallenge->GetChallengeID()
                               << " Action Index: " << liActionIndex
                               << "/" << mpCurrentChallenge->GetNumActions() << "\n";
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
                    CgsDev::Assert::EndAssert();
                }
                lbPostSuccessAction = true;
                lSuccessAction.mabAccumulationThisFrame[liActionIndex] = true;
            }
        }

        // NOTE: the 5th argument (the header spells it liNumPlayers) is this frame counter --
        // the X360 threads its own r5 parameter straight through.
        UpdateActionSuccess(lpAction, liActionIndex, leActionStatus, leActiveRaceCarIndex,
                            liFramesSinceNetworkStart, lpActionQueue, lbIsOnline);

        if (lpAction->GetCombineAction() == BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_CHAIN)
        {
            if (leActionStatus == E_CHALLENGE_STATUS_SUCCESS)
            {
                ++miCurrentChallengeAction;
                continue;
            }
        }
        else if (leActionStatus == E_CHALLENGE_STATUS_SUCCESS)
        {
            continue;
        }

        if (leActionStatus == E_CHALLENGE_STATUS_RESET_IF_NEEDED)
        {
            if (maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] !=
                GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE)
            {
                if (lpAction->GetCombineAction() ==
                    BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_FAILURE_RESETS_CHAIN)
                {
                    // Rewind the whole chain locally: restock the remaining targets and clear
                    // this player's per-action state.
                    miCurrentChallengeAction  = 0;
                    miCurrentArbitrationIndex = 0;

                    for (s32 liResetIndex = 0;
                         liResetIndex < mpCurrentChallenge->GetNumActions();
                         ++liResetIndex)
                    {
                        maiRemainingTarget[liResetIndex] =
                            mpCurrentChallenge->GetAction(liResetIndex)->GetTargetValue(0);
                    }

                    for (s32 liResetIndex = 0;
                         liResetIndex < mpCurrentChallenge->GetNumActions();
                         ++liResetIndex)
                    {
                        maafCumulativeContributions[leActiveRaceCarIndex][liResetIndex] = 0.0f;
                        maaePlayersSuccessStatus[leActiveRaceCarIndex][liResetIndex] =
                            GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_NONE;
                        mabIndividualActionsSuccessUpdateSent[liResetIndex] = false;

                        for (EFreeburnSkill leSkill = E_FREEBURN_SKILL_START;
                             static_cast<s32>(leSkill) < KI_FREEBURN_SKILL_COUNT_X360; leSkill++)
                        {
                            mafBankedActionScores[leSkill] = 0.0f;
                        }
                    }
                }
                else if (lpAction->GetCombineAction() ==
                         BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_FAILURE_RESETS_EVERYONE)
                {
                    // Rewind everyone: reset the local data and broadcast the reset.
                    miCurrentChallengeAction  = 0;
                    miCurrentArbitrationIndex = 0;

                    GameStateModuleIO::FreeburnChallengeAction lResetAction;
                    lResetAction.mChallengeID   = mpCurrentChallenge->GetChallengeID();
                    lResetAction.meEventType    = BrnNetwork::BrnNetworkModuleIO::E_CHALLENGE_EVENT_RESET;   // 3
                    lResetAction.meChallengeStatus       = E_CHALLENGE_STATUS_ONGOING;
                    lResetAction.miActionIndex           = 0;
                    lResetAction.miNumChallengesComplete = -1;
                    lResetAction.miTotalNumChallenges    = -1;
                    lResetAction.mbIsHost                = false;
                    lResetAction.mbAbortingToStartNewChallenge = false;

                    ResetCurrentChallengeData(0);

                    CGS_ASSERT(lpActionQueue, "lpActionQueue");   // :4431
                    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResetAction),
                                            GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE, 0x20);

                    if (miLastChallengeResetFrame < miFramesSinceNetworkStart)
                    {
                        miLastChallengeResetFrame = miFramesSinceNetworkStart;
                    }
                }
            }

            if (lpAction->GetCoopType() == BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_COUNT ||
                lpAction->GetCombineAction() == BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_SIMULTANEOUS)
            {
                // Reset every action slot up to (and including) the one before the cursor and
                // tell the other machines about it.
                s32 liLastResetAction = miCurrentChallengeAction - 1;
                if (liLastResetAction <= 0)
                {
                    liLastResetAction = 0;
                }
                miCurrentChallengeAction  = 0;
                miCurrentArbitrationIndex = 0;

                GameStateModuleIO::FreeburnChallengeAction lResetAction;
                lResetAction.mChallengeID = mpCurrentChallenge->GetChallengeID();
                // X360 DRIFT: raw enumerator 4 -- the X360 EChallengeEventType carries one extra
                // (unattested) enumerator ahead of ENDED, so this is NOT E_CHALLENGE_EVENT_ENDED.
                lResetAction.meEventType             = 4;
                lResetAction.meChallengeStatus       = E_CHALLENGE_STATUS_ONGOING;
                lResetAction.miActionIndex           = liLastResetAction;
                lResetAction.miNumChallengesComplete = -1;
                lResetAction.miTotalNumChallenges    = -1;
                lResetAction.mbIsHost                = false;
                lResetAction.mbAbortingToStartNewChallenge = false;

                for (s32 liResetIndex = 0; liResetIndex < liLastResetAction + 1; ++liResetIndex)
                {
                    ResetActionData(liResetIndex);
                }

                CGS_ASSERT(lpActionQueue, "lpActionQueue");   // :4469
                lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResetAction),
                                        GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE, 0x20);

                if (miLastChallengeResetFrame < miFramesSinceNetworkStart)
                {
                    miLastChallengeResetFrame = miFramesSinceNetworkStart;
                }
            }
        }

        if (liActionIndex > 0)
        {
            // An INDEPENDENT predecessor means the earlier slots don't feed this broadcast.
            if (mpCurrentChallenge->GetAction(liActionIndex - 1)->GetCombineAction() ==
                BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_INDEPENDENT)
            {
                lbPostSuccessAction = false;
                for (s32 liPreviousIndex = 0; liPreviousIndex < liActionIndex; ++liPreviousIndex)
                {
                    maaePlayersSuccessStatus[leActiveRaceCarIndex][liPreviousIndex] =
                        GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_NONE;
                }
            }
        }

        if (lpAction->GetCombineAction() != BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_COUNT)
        {
            break;
        }
    }

    if (lbPostSuccessAction)
    {
        std::memcpy(lSuccessAction.mafActionScores, maafCurrentActionsScores[leActiveRaceCarIndex],
                    sizeof(lSuccessAction.mafActionScores));   // X360 XMemCpy(dest, src, 8)

        for (s32 liScoreIndex = 0; liScoreIndex < KI_MAX_CHALLENGE_ACTIONS; ++liScoreIndex)
        {
            if (lSuccessAction.mafActionScores[liScoreIndex] < 0.0f)
            {
                // X360 file/line BrnChallengeManager.cpp:4519.
                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "Negative successful action score reported: "
                           << lSuccessAction.mafActionScores[liScoreIndex]
                           << " Challenge ID: " << mpCurrentChallenge->GetChallengeID()
                           << " Action Index: " << liScoreIndex
                           << "/" << mpCurrentChallenge->GetNumActions() << "\n";
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
                CgsDev::Assert::EndAssert();
            }
        }

        CGS_ASSERT(lpActionQueue, "lpActionQueue");   // :4523
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSuccessAction),
                                GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE_SUCCESS, 0xC);
    }
}

}   // namespace BrnGameState
