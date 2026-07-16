// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wB_06.cpp
// ============================================================================
// Wave-B partfile (group 6) for BrnGameState::ChallengeManager. Store-for-store
// reconstructions of the challenge-timer arming/countdown and the per-frame freeburn-skill
// driver, over the keystone-frozen NAMED members (see BrnChallengeManager.h).
//
// BODIED HERE:
//   UpdateTimer                    -- X360 0x823232B0
//   UpdateFreeburnSkillsThisFrame  -- X360 0x8233AFF8
//
// NOT bodied (reported as blocked): UpdateResults @0x82345FD0 -- posts the 32-byte
//   E_ACTION_FREEBURN_CHALLENGE (id 153) action, but no struct for that action is declared
//   in any frozen header (only the 155 / 158 / 159 actions exist in BrnGameActions.h). See
//   the wave report for the exact field layout the keystone needs to add.
// ============================================================================

#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "SharedClasses/DataLists/ChallengeListEntry.h"   // ChallengeListEntry::GetAction/GetNumActions, ChallengeListEntryAction::GetTimeLimit

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// UpdateTimer -- X360 0x823232B0. Arms the per-action challenge countdown on first call
// from the current action's time limit (only when that limit is > 0), then counts it down
// one frame with the fsel clamp-at-zero idiom. Returns true while the timer is still running
// (or was never armed), and false only once an armed timer has expired -- UpdateRunning uses
// the false result as the "challenge action timed out" signal it feeds to UpdateArbitration.
// ----------------------------------------------------------------------------
bool ChallengeManager::UpdateTimer(f32 lfTimeStep)
{
    if (!mbChallengeTimerRunning)
    {
        CGS_ASSERT(mpCurrentChallenge != 0, "mpCurrentChallenge");
        CGS_ASSERT(miCurrentChallengeAction < mpCurrentChallenge->GetNumActions(),
                   "miCurrentChallengeAction < mpCurrentChallenge->GetNumActions()");

        const BrnResource::ChallengeListEntryAction* lpAction =
            mpCurrentChallenge->GetAction(miCurrentChallengeAction);
        CGS_ASSERT(lpAction != 0, "lpAction");

        // X360 lfs f0,+0x40(action) compared to 0.0, then re-reads it via GetTimeLimit for the
        // store (both are the action's mfTimeLimit @+0x40).
        if (lpAction->GetTimeLimit() > 0.0f)
        {
            mfChallengeTimer        = lpAction->GetTimeLimit();
            mbChallengeTimerRunning = true;
        }
    }

    const bool lbTimerRunning = mbChallengeTimerRunning;

    if (lbTimerRunning && mfChallengeTimer > 0.0f)
    {
        // f0 = timer - dt; fsel(-(timer-dt), 0.0, f0) clamps the countdown at 0.0
        // (X360 fsubs / fneg / fsel f0,f13,f31,f0).
        const f32 lfNewTimer = mfChallengeTimer - lfTimeStep;
        mfChallengeTimer = (lfNewTimer >= 0.0f) ? lfNewTimer : 0.0f;
    }

    // X360 tail (cntlzw/extrwi == "== 0"): expired iff the timer is running AND has reached 0.0.
    if (lbTimerRunning && mfChallengeTimer <= 0.0f)
    {
        return false;
    }
    return true;
}

// ----------------------------------------------------------------------------
// UpdateFreeburnSkillsThisFrame -- X360 0x8233AFF8. Per-frame freeburn-skill driver: always
// clears this frame's skill scratch, then (only while the manager is RUNNING) folds in the
// burnout skill scores and, when the active challenge tracks leapt cars, the leap-car skills.
// (UpdateLeaptCars is the VMX128-blocked geometry callee -- declared in the header, called here.)
// ----------------------------------------------------------------------------
void ChallengeManager::UpdateFreeburnSkillsThisFrame(
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface,
        f32 lfTimeStep)
{
    ClearFreeburnSkillsThisFrame();

    if (meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RUNNING)   // X360 lwz +0xE08 == 2
    {
        UpdateBurnouts(lpActiveRaceCarOutputInterface);

        if (mbChallengeRequiresLeapCars)   // X360 lbz +0xE34
        {
            UpdateLeaptCars(lpActiveRaceCarOutputInterface, lfTimeStep);
        }
    }
}

}  // namespace BrnGameState
