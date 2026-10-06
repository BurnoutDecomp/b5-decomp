// ============================================================================
// b5-decomp/src/GameSource/GameState/GameStateModule_wW_01.cpp
//
// Partfile of the BrnGameState::GameStateModule TU (owning header BrnGameStateModule.h).
//
// GameStateModule::PostWorldUpdate: the module's post-world pass, called once per sub-step by
// BrnGameModule::DoUpdate_GameStatePostWorld with the PostWorldInputBuffer that
// BridgeWorldToGameState has just filled. In order:
//   1. mbIsUpdating on; the sim-pause test; the buffer read-locked.
//   2. the module's snapshots of the buffer: the active and global race-car interfaces, the AI car
//      interface, the traffic-type responses; the game events folded into the carry queue that the
//      next pre-world pass drains.
//   3. unless the sim is paused: the takedown caches, then ModeManager::PostWorldUpdate with the
//      module's takedown-event queue and its cached sim timestep.
//   4. with update-set bit 0x8 (in game): the trigger queries.
//   5. ProcessContacts; the per-car crashing flags; the player index publish; the boost tips.
//   6. the buffer unlocked; mbIsUpdating off.
// ============================================================================

#include "GameSource/GameState/BrnGameStateModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameSource/GameState/BrnGameStateModuleIO.h"                   // PostWorldInputBuffer
#include "GameSource/GameState/BrnGameStateTakedownCache.h"              // mpTakedownCache's queues
#include "GameSource/GameState/ModeManager/BrnModeManager.h"             // ModeManager::PostWorldUpdate
#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h" // the landmark pass
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"     // the boost tips
#include "GameSource/GameState/Progression/BrnProfile.h"                 // Profile::HasPlayerSeenTrainingType
#include "SharedClasses/Progression/BrnTrainingTypes.h"                  // E_TRAINING_TYPE_DANGER_BOOST_FULL
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // BoostOutputInfo

namespace BrnGameState
{

namespace
{
    // Rodata 5.0f: the settle gap between two training tips (the same literal the other tip
    // requesters name KF_TRAINING_TIP_SETTLE_TIME).
    const f32 KF_TRAINING_TIP_SETTLE_TIME = 5.0f;

    // Update-set bit 0x8: raised by BrnGameModule::ConstructUpdateSetFromFsm while in game.
    const u32 KU_UPDATE_SET_IN_GAME = 0x8u;
}

void GameStateModule::PostWorldUpdate(CgsModule::IOBufferStack*                      /*lpUpdateInputBufferStack*/,
                                      CgsModule::IOBufferStack*                      /*lpUpdateOutputBufferStack*/,
                                      const GameStateModuleIO::PostWorldInputBuffer* lpPostWorldInputBuffer,
                                      BrnUpdateSet                                   lUpdateSet)
{
    // The console brackets the body and each leg below with PerfMonCpu monitors whose handles this
    // slice of the module does not model; they only time the frame.
    mbIsUpdating = true;
    const bool lbSimPaused = IsSimPaused(true, false);
    lpPostWorldInputBuffer->LockForRead();

    // ---- the snapshots. Copied by assignment: every one of these interfaces is a different size
    // on the host, so the console's byte counts do not apply.
    mLastActiveRaceCarInterface = *lpPostWorldInputBuffer->GetActiveRaceCarOutputInterface();
    mLastGlobalRaceCarInterface = *lpPostWorldInputBuffer->GetGlobalRaceCarOutputInterface();
    mLastAICarOutputInterface   = *lpPostWorldInputBuffer->GetAICarOutputInterface();

    // The traffic-type responses TakedownManager::Update reads next pre-world (gsm+278480).
    mpTakedownCache->mTrafficTypeResponseQueue.Clear();
    mpTakedownCache->mTrafficTypeResponseQueue.Append(*lpPostWorldInputBuffer->GetTrafficTypeResponseQueue());

    // The frame's game events join the carry queue; PreWorldUpdate's ProcessGameEvents drains it.
    mGameEventCarryQueue.Append(*lpPostWorldInputBuffer->GetGameEventQueue());

    if (!lbSimPaused)
    {
        CacheTakedownManagerPostWorldInputData(lpPostWorldInputBuffer);
        mModeManager.PostWorldUpdate(lpPostWorldInputBuffer,
                                     &mpTakedownCache->mTakedownEventQueue,
                                     mfSimTimeStep);
    }

    if ((static_cast<u32>(lUpdateSet) & KU_UPDATE_SET_IN_GAME) != 0)
    {
        mTriggerQueryManager.PostWorldUpdate(lpPostWorldInputBuffer, &mModeManager,
                                            GetPlayerActiveRaceCarIndex());
    }

    ProcessContacts(lpPostWorldInputBuffer->GetContactSpyInterface());

    // ---- the per-slot "this car is crashing" cache IsRaceCarCrashing answers from.
    for (s32 liSlot = 0; liSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liSlot)
    {
        const ::EActiveRaceCarIndex leSlot = static_cast< ::EActiveRaceCarIndex>(liSlot);
        maRaceCarCrashing[liSlot] =
            mLastActiveRaceCarInterface.IsRaceCarActive(leSlot) &&
            mLastActiveRaceCarInterface.GetRaceCarState(leSlot)->mbCrashing;
    }

    // ---- the player's indices. ProcessContacts above ran on the previous frame's values; that
    // one-frame lag is the console's.
    if (mLastActiveRaceCarInterface.IsPlayerCarActive())
    {
        mePlayerActiveRaceCarIndex = mLastActiveRaceCarInterface.GetPlayerActiveRaceCarIndex();
        miPlayerGlobalRaceCarIndex =
            lpPostWorldInputBuffer->GetGlobalRaceCarOutputInterface()->GetPlayerGlobalRaceCarIndex();

        const BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
        CGS_ASSERT(lpProfile != NULL, "lpProfile != NULL");
        (void)lpProfile;

        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutput =
            lpPostWorldInputBuffer->GetActiveRaceCarOutputInterface();
        const BrnWorld::RaceCarEntityModuleIO::BoostOutputInfo* lpBoostInfo =
            lpActiveRaceCarOutput->GetBoostOutputInfoN(lpActiveRaceCarOutput->GetPlayerActiveRaceCarIndex());

        // [X] NOT REPRODUCED: the best-burnout-chain block. When the player's chain count beats
        // the profile's muBestNewBurnoutChainScore the console stores the new best, asserts
        // "muNumBoostChains > 0", requests tip E_TRAINING_TYPE_BURNOUT and raises achievement 18
        // at a chain of 20 (AchievementManagerBase::OnBoostChain). Blocked: Profile has no
        // SetBestBurnoutChain on this tree and OnBoostChain has no body.

        // The danger-boost-full tip: a danger boost bar that is full. The console skips on `blt`,
        // so an unordered compare still asks.
        if (lpBoostInfo->meBoostType == BrnWorld::E_BOOST_TYPE_DANGER &&
            !(lpBoostInfo->mfBoostAmount < lpBoostInfo->mfMaxBoost) &&
            mpTrainingManager != 0 &&
            !mpTrainingManager->IsTipPending() &&
            !mpTrainingManager->IsInPictureParadise() &&
            mpTrainingManager->IsTipAllowedInGameMode(BrnProgression::E_TRAINING_TYPE_DANGER_BOOST_FULL))
        {
            BrnProgression::Profile* lpTipProfile = mpTrainingManager->GetProfile();
            CGS_ASSERT(lpTipProfile != 0, "lpProfile");
            if (!lpTipProfile->HasPlayerSeenTrainingType(BrnProgression::E_TRAINING_TYPE_DANGER_BOOST_FULL) &&
                !(mpTrainingManager->GetTimeSinceLastTip() < KF_TRAINING_TIP_SETTLE_TIME))
            {
                mpTrainingManager->RequestTip(BrnProgression::E_TRAINING_TYPE_DANGER_BOOST_FULL);
            }
        }

        // [X] NOT REPRODUCED: the lost-boost-chunk tip (E_TRAINING_TYPE_AGGRESSION_LOST_BOOST_CHUNK).
        // Its gate reads two flag bits (0x400000, 0x800000) of a 64-bit word at gsm+0x28960 that
        // this slice of the module does not model.
    }
    else
    {
        mePlayerActiveRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;
        miPlayerGlobalRaceCarIndex = ::E_GLOBAL_RACE_CAR_INDEX_INVALID;
    }

    lpPostWorldInputBuffer->UnlockForRead();
    mbIsUpdating = false;

    // [X] NOT REPRODUCED: the tail store of lpPostWorldInputBuffer->GetInPictureParadise() into
    // the rumble manager's mbInPictureParadise (its UpdatePictureParadiseState is not declared on
    // this tree).
}

}
