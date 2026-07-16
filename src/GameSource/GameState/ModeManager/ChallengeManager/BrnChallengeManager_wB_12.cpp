// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wB_12.cpp
// ============================================================================
// BrnGameState::ChallengeManager -- wave-B partfile (group 12): the two output-writer
// helpers that pack the per-frame FreeburnChallengeUpdateAction plus the driver that dispatches
// the deferred remote start/trigger/end requests.
//
//   WriteDataToOutput            (X360 0x82346918)
//   WriteDataToOutputForTarget   (X360 0x823236D0)
//   UpdateRemoteRequests         (X360 0x82351990)
//
// SOURCE-OF-TRUTH: the X360 ARTIST asm is authoritative for every store/branch/early-out;
// raw offsets are mapped onto the keystone-frozen named members/accessors. The output action's
// [action][player] grids are the TRANSPOSED counterpart of the manager's [player][action] grids
// (spec pitfall 4). sub_8231D800 is OutputBuffer::GetGameStateToNetworkInterface (asm assert
// string), NOT the game-action queue; the game-action queue is OutputBuffer::GetGameActionQueue
// (null-checked) / GetGuiOutputQueue (concrete-typed twin actually posted through, matching the
// committed DeveloperChallengeManager pattern). The active-race-car loops are reconstructed as
// plain counted loops over the raw X360 register counter (the inlined EActiveRaceCarIndex
// operator++ range guard is a non-behavioural debug assert).

#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h"
#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManagerDebugComponent.h"
#include "GameSource/GameState/BrnGameActions.h"                             // FreeburnChallengeUpdateAction / E_ACTION_FREEBURN_CHALLENGE_UPDATE
#include "GameSource/GameState/BrnGameEvents.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"                       // GameStateModuleIO::EFreeburnChallengeSuccess
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"

#include "GameSource/GameState/BrnGameStateModuleIO.h"                       // OutputBuffer accessors
#include "GameSource/GameState/BrnGameStateModule.h"                         // GameStateModule::GetPlayerActiveRaceCarIndex / GetModeManager
#include "GameSource/GameState/ModeManager/BrnModeManager.h"                 // ModeManager::GetScoringSystem
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"       // ScoringSystem::GetCarData
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h" // GameStateToNetworkInterface::SetPlayerInFreeburnChallenge
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"            // CgsModule::VariableEventQueue<13312,16>::AddEvent / Event

#include <cstring>                                                          // std::memset (XMemSet)

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// WriteDataToOutput  (X360 0x82346918)
// ----------------------------------------------------------------------------
// Builds the per-frame FreeburnChallengeUpdateAction on the stack (time-left, current action,
// per-target rows) and posts it to the output buffer's game-action queue, then mirrors the
// per-player "started challenge" flags into the GameState->Network interface.
void ChallengeManager::WriteDataToOutput(GameStateModuleIO::OutputBuffer* lpOutputBuffer)
{
    CGS_ASSERT(lpOutputBuffer, "lpOutput");   // BrnChallengeManager.cpp:1705

    if (meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RUNNING ||
        meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RESULTS)
    {
        CGS_ASSERT(mpCurrentChallenge, "mpCurrentChallenge");   // :1718
        CGS_ASSERT(miCurrentArbitrationIndex < mpCurrentChallenge->GetNumActions(),
                   "miCurrentArbitrationIndex < mpCurrentChallenge->GetNumActions()");   // :1719

        const BrnResource::ChallengeListEntryAction* lpArbitrationAction =
            mpCurrentChallenge->GetAction(miCurrentArbitrationIndex);

        // Skip forward over any run of E_COMBINE_ACTION_INDEPENDENT actions starting at the
        // arbitration index; the resulting index is what the event reports as the current action.
        s32 liCurrentActionIndex = miCurrentArbitrationIndex;
        if (lpArbitrationAction->GetCombineAction() == BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_INDEPENDENT)
        {
            if (liCurrentActionIndex < mpCurrentChallenge->GetNumActions())
            {
                do
                {
                    if (mpCurrentChallenge->GetAction(liCurrentActionIndex)->GetCombineAction() !=
                        BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_INDEPENDENT)
                    {
                        break;
                    }
                    ++liCurrentActionIndex;
                }
                while (liCurrentActionIndex < mpCurrentChallenge->GetNumActions());
            }
        }

        GameStateModuleIO::FreeburnChallengeUpdateAction loUpdateAction;
        loUpdateAction.mfTimeLeftInChallenge = mbChallengeTimerRunning ? mfChallengeTimer : -1.0f;
        loUpdateAction.miCurrentActionIndex  = liCurrentActionIndex;
        loUpdateAction.miNumTargetsUsed      = 0;

        // Does the challenge contain an action of the (X360-drift) type 22? -- if so every target
        // is written out below regardless of the arbitration action's combine mode.
        bool lbWriteAllTargets = false;
        if (mpCurrentChallenge->GetNumActions() != 0)
        {
            s32 liScanAction = 0;
            while (true)
            {
                CGS_ASSERT(mpCurrentChallenge, "mpCurrentChallenge");   // :1749
                const BrnResource::ChallengeListEntryAction* lpScanAction =
                    mpCurrentChallenge->GetAction(liScanAction);
                CGS_ASSERT(lpScanAction, "mpCurrentChallenge->GetAction(liIndex)");   // :1750
                if (static_cast<s32>(lpScanAction->GetActionType()) == 22)   // X360-drift MEET-UP-style action id
                {
                    lbWriteAllTargets = true;
                    break;
                }
                ++liScanAction;
                if (liScanAction >= mpCurrentChallenge->GetNumActions())
                {
                    break;
                }
            }
        }

        const BrnResource::ChallengeListEntryAction::ECombineActionType leCombineAction =
            lpArbitrationAction->GetCombineAction();
        if (leCombineAction == BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_COUNT ||
            leCombineAction == BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_INDEPENDENT ||
            lbWriteAllTargets)
        {
            if (mpCurrentChallenge->GetNumActions() != 0)
            {
                s32 liTargetIndex = 0;
                do
                {
                    WriteDataToOutputForTarget(&loUpdateAction, liTargetIndex);
                    ++liTargetIndex;
                }
                while (liTargetIndex < mpCurrentChallenge->GetNumActions());
            }
        }
        else if (lpArbitrationAction->GetNumTargets() != 0)
        {
            WriteDataToOutputForTarget(&loUpdateAction, miCurrentArbitrationIndex);
        }

        CGS_ASSERT(lpOutputBuffer->GetGameActionQueue() != nullptr, "lpOutput->GetGameActionQueue()");   // :1778
        TGameActionQueue* lpActionQueue = lpOutputBuffer->GetGuiOutputQueue();
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&loUpdateAction),
                                GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE_UPDATE,
                                sizeof(GameStateModuleIO::FreeburnChallengeUpdateAction));
    }

    // Publish the per-player "player started the challenge" flags to the network interface.
    for (s32 liActiveRaceCarIndex = 0; liActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liActiveRaceCarIndex)
    {
        CGS_ASSERT(lpOutputBuffer->GetGameStateToNetworkInterface(),
                   "lpOutput->GetGameStateToNetworkInterface()");   // :1787
        lpOutputBuffer->GetGameStateToNetworkInterface()->SetPlayerInFreeburnChallenge(
            static_cast<BrnNetwork::EActiveRaceCarIndex>(liActiveRaceCarIndex),
            mabPlayerStartedChallenge[liActiveRaceCarIndex]);
    }
}

// ----------------------------------------------------------------------------
// WriteDataToOutputForTarget  (X360 0x823236D0)
// ----------------------------------------------------------------------------
// Fills one target column (row [miNumTargetsUsed]) of the update action: per-player contribution
// + completion status + overall remaining, driven by the action's coop type, then bumps
// miNumTargetsUsed. The output grids are [target][player], transposed vs the manager's grids.
void ChallengeManager::WriteDataToOutputForTarget(GameStateModuleIO::FreeburnChallengeUpdateAction* lpUpdateAction,
                                                  s32 liActionIndex)
{
    const BrnResource::ChallengeListEntryAction* lpAction = mpCurrentChallenge->GetAction(liActionIndex);

    s32 liOverallTargetRemaining;
    if (lpAction->GetNumTargets() != 0 && lpAction->GetTargetValue(0) > 1)
    {
        switch (lpAction->GetCoopType())
        {
        case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_CUMULATIVE:   // 4
            for (s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar)
            {
                CGS_ASSERT(mpGameStateModule, "mpGameStateModule");   // :1821
                f32 lfContribution = maafCumulativeContributions[liCar][liActionIndex];
                if (static_cast<s32>(mpGameStateModule->GetPlayerActiveRaceCarIndex()) == liCar)
                {
                    const s32 liSkill =
                        KAI_CHALLENGE_ACTION_TYPE_TO_FREEBURN_SKILL[lpAction->GetActionType()];
                    if (liSkill != KI_FREEBURN_SKILL_COUNT_X360 && !mabBankedSkillThisFrame[liSkill])
                    {
                        lfContribution = mafActiveSkillValue[liSkill] +
                                         maafCumulativeContributions[liCar][liActionIndex];
                    }
                }
                lpUpdateAction->maafIndividualTargetContributions[lpUpdateAction->miNumTargetsUsed][liCar] =
                    lfContribution;
            }
            break;

        case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL:   // 1
        case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_COUNT:        // 6
            for (s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar)
            {
                CGS_ASSERT(mpGameStateModule, "mpGameStateModule");   // :1859
                if (static_cast<s32>(mpGameStateModule->GetPlayerActiveRaceCarIndex()) == liCar &&
                    maaePlayersSuccessStatus[liCar][liActionIndex] !=
                        GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE)
                {
                    const s32 liSkill =
                        KAI_CHALLENGE_ACTION_TYPE_TO_FREEBURN_SKILL[lpAction->GetActionType()];
                    if (liSkill == KI_FREEBURN_SKILL_COUNT_X360)
                    {
                        lpUpdateAction->maafIndividualTargetContributions[lpUpdateAction->miNumTargetsUsed][liCar] = 0.0f;
                    }
                    else
                    {
                        lpUpdateAction->maafIndividualTargetContributions[lpUpdateAction->miNumTargetsUsed][liCar] =
                            mafActiveSkillValue[liSkill];
                    }
                }
                else
                {
                    lpUpdateAction->maafIndividualTargetContributions[lpUpdateAction->miNumTargetsUsed][liCar] =
                        maafCumulativeContributions[liCar][liActionIndex];
                }
            }
            break;

        case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION:   // 2
            for (s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar)
            {
                CGS_ASSERT(mpGameStateModule, "mpGameStateModule");   // :1894
                if (static_cast<s32>(mpGameStateModule->GetPlayerActiveRaceCarIndex()) == liCar &&
                    maaePlayersSuccessStatus[liCar][liActionIndex] !=
                        GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE)
                {
                    const s32 liSkill =
                        KAI_CHALLENGE_ACTION_TYPE_TO_FREEBURN_SKILL[lpAction->GetActionType()];
                    if (liSkill == KI_FREEBURN_SKILL_COUNT_X360)
                    {
                        lpUpdateAction->maafIndividualTargetContributions[lpUpdateAction->miNumTargetsUsed][liCar] = 0.0f;
                    }
                    else
                    {
                        lpUpdateAction->maafIndividualTargetContributions[lpUpdateAction->miNumTargetsUsed][liCar] =
                            mafActiveSkillValue[liSkill] + mafBankedActionScores[liSkill];
                    }
                }
                else
                {
                    lpUpdateAction->maafIndividualTargetContributions[lpUpdateAction->miNumTargetsUsed][liCar] =
                        maafCumulativeContributions[liCar][liActionIndex];
                }
            }
            break;

        case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_SIMULTANEOUS:   // 3
            for (s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar)
            {
                CGS_ASSERT(mpGameStateModule, "mpGameStateModule");   // :1929
                if (static_cast<s32>(mpGameStateModule->GetPlayerActiveRaceCarIndex()) == liCar)
                {
                    const s32 liSkill =
                        KAI_CHALLENGE_ACTION_TYPE_TO_FREEBURN_SKILL[lpAction->GetActionType()];
                    if (liSkill == KI_FREEBURN_SKILL_COUNT_X360)
                    {
                        lpUpdateAction->maafIndividualTargetContributions[lpUpdateAction->miNumTargetsUsed][liCar] = 0.0f;
                    }
                    else
                    {
                        lpUpdateAction->maafIndividualTargetContributions[lpUpdateAction->miNumTargetsUsed][liCar] =
                            mafActiveSkillValue[liSkill];
                    }
                }
                else
                {
                    lpUpdateAction->maafIndividualTargetContributions[lpUpdateAction->miNumTargetsUsed][liCar] = 0.0f;
                }
            }
            break;

        default:   // E_CHALLENGE_COOP_TYPE_ONCE (0) / E_CHALLENGE_COOP_TYPE_AVERAGE (5)
            std::memset(&lpUpdateAction->maafIndividualTargetContributions[lpUpdateAction->miNumTargetsUsed][0], 0,
                        sizeof(lpUpdateAction->maafIndividualTargetContributions[lpUpdateAction->miNumTargetsUsed]));
            break;
        }

        CGS_ASSERT(liActionIndex < BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE,
                   "liTargetIndex < BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE");   // :1956
        CGS_ASSERT(lpUpdateAction->miNumTargetsUsed < BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE,
                   "lpOutEvent->miNumTargetsUsed < BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE");   // :1957
        liOverallTargetRemaining = maiRemainingTarget[liActionIndex];
    }
    else
    {
        std::memset(&lpUpdateAction->maafIndividualTargetContributions[lpUpdateAction->miNumTargetsUsed][0], 0,
                    sizeof(lpUpdateAction->maafIndividualTargetContributions[lpUpdateAction->miNumTargetsUsed]));
        CGS_ASSERT(lpUpdateAction->miNumTargetsUsed < BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE,
                   "lpOutEvent->miNumTargetsUsed < BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE");   // :1963
        liOverallTargetRemaining = 0;
    }

    lpUpdateAction->maiOverallTargetRemaining[lpUpdateAction->miNumTargetsUsed] = liOverallTargetRemaining;

    for (s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar)
    {
        if (mabPlayerStartedChallenge[liCar])
        {
            if (lpAction->GetCombineAction() == BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_CHAIN)
            {
                // Walk the chain of E_COMBINE_ACTION_CHAIN actions to its end, then report that
                // action's per-player success status.
                s32 liChainActionIndex = liActionIndex;
                if (liActionIndex < mpCurrentChallenge->GetNumActions())
                {
                    while (mpCurrentChallenge->GetAction(liChainActionIndex)->GetCombineAction() ==
                           BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_CHAIN)
                    {
                        ++liChainActionIndex;
                        if (liChainActionIndex >= mpCurrentChallenge->GetNumActions())
                        {
                            break;
                        }
                    }
                }
                lpUpdateAction->maaeCompleted[lpUpdateAction->miNumTargetsUsed][liCar] =
                    maaePlayersSuccessStatus[liCar][liChainActionIndex];
            }
            else
            {
                lpUpdateAction->maaeCompleted[lpUpdateAction->miNumTargetsUsed][liCar] =
                    maaePlayersSuccessStatus[liCar][liActionIndex];
            }
        }
        else
        {
            CGS_ASSERT(mpGameStateModule, "mpGameStateModule");   // :2001
            CGS_ASSERT(mpGameStateModule->GetModeManager(), "mpGameStateModule->GetModeManager()");   // :2002
            ScoringSystem* lpScoringSystem = mpGameStateModule->GetModeManager()->GetScoringSystem();
            CGS_ASSERT(lpScoringSystem, "lpScoringSystem");   // :2004
            if (lpScoringSystem->GetCarData(static_cast<EActiveRaceCarIndex>(liCar)))
            {
                lpUpdateAction->maaeCompleted[lpUpdateAction->miNumTargetsUsed][liCar] =
                    GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_NOT_IN_CHALLENGE;
            }
            else
            {
                lpUpdateAction->maaeCompleted[lpUpdateAction->miNumTargetsUsed][liCar] =
                    GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_NONE;
            }
        }
    }

    ++lpUpdateAction->miNumTargetsUsed;
}

// ----------------------------------------------------------------------------
// UpdateRemoteRequests  (X360 0x82351990)
// ----------------------------------------------------------------------------
// Drains the three deferred remote-request flags set while the arbitrator's decisions were
// pending. On the arbitrator (lbIsOnline) each request is an error (the arbitrator drives the
// challenge itself); otherwise the request is replayed locally as a remote begin/trigger/end.
// The asm asserts the request-holder is not the arbitrator via a verbatim "!lbIsArbitrator"
// (the r6 bool the header spells lbIsOnline).
void ChallengeManager::UpdateRemoteRequests(TGameActionQueue* lpActionQueue,
                                            const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface,
                                            bool lbIsOnline)
{
    if (mbRemoteStartPending)
    {
        if (lbIsOnline)
        {
            CGS_ASSERT(!lbIsOnline, "!lbIsArbitrator");   // BrnChallengeManager.cpp:818
        }
        else
        {
            BeginChallenge(mpCurrentChallenge->GetChallengeID(), lpActionQueue,
                           lpActiveRaceCarOutputInterface, lbIsOnline, true);
        }
        mbRemoteStartPending = false;
    }

    if (mbRemoteTriggerPending)
    {
        if (lbIsOnline)
        {
            CGS_ASSERT(!lbIsOnline, "!lbIsArbitrator");   // :830
        }
        else
        {
            BeginChallenge(mpCurrentChallenge->GetChallengeID(), lpActionQueue,
                           lpActiveRaceCarOutputInterface, lbIsOnline, false);
            TriggerFreeburnChallenge(mpCurrentChallenge->GetChallengeID(), lpActionQueue, lbIsOnline);
        }
        mbRemoteTriggerPending = false;
    }

    if (mbRemoteEndPending)
    {
        if (lbIsOnline)
        {
            CGS_ASSERT(!lbIsOnline, "!lbIsArbitrator");   // :844
        }
        else if (meChallengeManagerStatus != E_CHALLENGE_MANAGER_STATUS_NONE)
        {
            EndChallenge(meLocalChallengeStatus, lpActionQueue, false);
        }
        mbRemoteEndPending = false;
    }
}

} // namespace BrnGameState
