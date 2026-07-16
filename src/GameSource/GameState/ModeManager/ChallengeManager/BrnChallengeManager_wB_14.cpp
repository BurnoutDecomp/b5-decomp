// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wB_14.cpp
// ============================================================================
// Wave-B partfile (group 14) for BrnGameState::ChallengeManager -- the per-frame drivers.
// Store-for-store reconstructions over the keystone-frozen named layout (BrnChallengeManager.h):
//   * PreWorldUpdate  (X360 0x82353640) -- caches miFramesSinceNetworkStart, drains remote
//                                          success/requests, then runs the running/results/skill
//                                          sub-updates and flushes the update action to output.
//   * UpdateRunning   (X360 0x82353008) -- the RUNNING-state driver: honours the "make all
//                                          challenges N-player" debug toggles (inline hack /
//                                          UnHackAllChallenges), then UpdateTimer -> UpdateChallenge
//                                          -> UpdateArbitration.
//
// All manager state is reached through named members / committed accessors; every offset quoted
// in a comment is X360-asm-attested.
//
// BLOCKED in this group (reported in funcs_blocked, NOT bodied here):
//   PostWorldUpdate @ 0x8233AC28 -- the X360 body takes THREE parameters (r4 = crash-event queue,
//   r5 = the external stunt-score snapshot passed straight through to UpdateStuntScores, r6 = the
//   TimerStatusInterface). The keystone-frozen declaration in BrnChallengeManager.h has only TWO
//   (lpRaceCarCrashEventQueue, lpTimerStatusInterface): the middle stunt-score-snapshot parameter
//   is absent, so the function cannot be bodied against the frozen signature without either
//   dropping the UpdateStuntScores(snapshot) call or fabricating its argument. The header may not
//   be edited here (see funcs_blocked).
// ============================================================================

#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h"
#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManagerDebugComponent.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/GameState/BrnGameEvents.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "SharedClasses/DataLists/ChallengeList.h"                       // BrnResource::ChallengeList::GetChallengeCount/GetChallengeData
#include "SharedClasses/DataLists/ChallengeListEntry.h"                  // BrnResource::ChallengeListEntry::SetNumPlayers
#include "GameSource/GameState/BrnGameStateModuleIO.h"                   // GameStateModuleIO::OutputBuffer::GetGuiOutputQueue
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h" // CgsSystem::TimerStatusInterface (sim time step / 50Hz)
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // GetPlayerActiveRaceCarIndex

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// PreWorldUpdate -- X360 0x82353640.
// ----------------------------------------------------------------------------
// The mode manager's pre-world tick. Validates the output buffer + its game-action queue + the
// timer interface, snapshots the network-frame counter, then fans out to the remote-sync,
// running-state, results-state, output-flush and freeburn-skill sub-updates. The frame's fixed
// time step is the SIM timer's current step (mfBaseTimeStep * mfTimeStepMultiplier); the
// 50Hz-fixed-step predicate is IsSimTimerFrequency50Hz() (X360 inlines it as a fresh
// `simStep == 0.02f` compare against flt_82005574).
//
// X360 trailing-bool NOTE: the X360 build passes UpdateRunning's last two GPR args as
// (r8 = is-50Hz, r9 = is-online) -- i.e. the SIM-50Hz predicate lands in UpdateRunning's 5th
// parameter slot and the online flag in its 6th. That order is preserved verbatim below (the two
// values are handed to the slots exactly as the binary does); UpdateRunning consumes them in the
// same slot order (5th -> UpdateChallenge, 6th -> the UpdateArbitration gate).
// ----------------------------------------------------------------------------
void ChallengeManager::PreWorldUpdate(
    const CgsSystem::TimerStatusInterface*                                       lpTimerStatusInterface,
    s32                                                                          liFramesSinceNetworkStart,
    const CgsModule::BaseEventQueue<GameStateModuleIO::CompletedFburnChallengesData>* lpCompletedChallengeStatusQueue,
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface,
    bool                                                                         lbIsOnline,
    GameStateModuleIO::OutputBuffer*                                             lpOutputBuffer)
{
    CGS_ASSERT(lpOutputBuffer, "lpOutput");                                       // X360 line 0x2B0
    CGS_ASSERT(lpOutputBuffer->GetGuiOutputQueue(), "lpOutput->GetGameActionQueue()"); // X360 line 0x2B1
    CGS_ASSERT(lpTimerStatusInterface, "lpTimerStatusInterface");                 // X360 line 0x2B4

    // Fixed frame step + the network-frame snapshot (X360 stw,+0xFFC).
    const f32 lfTimeStep = lpTimerStatusInterface->GetSimTimerStatus()->GetCurrentTimeStep(); // lfs +0x20 * lfs +0x1C
    miFramesSinceNetworkStart = liFramesSinceNetworkStart;

    UpdateRemotePlayerSuccessStatus(lpCompletedChallengeStatusQueue, lpOutputBuffer->GetGuiOutputQueue());
    UpdateRemoteRequests(lpOutputBuffer->GetGuiOutputQueue(), lpActiveRaceCarOutputInterface, lbIsOnline);

    const bool lbIsSimTimerFrequency50Hz = lpTimerStatusInterface->IsSimTimerFrequency50Hz(); // fresh simStep == 0.02f

    // X360-attested arg slots: is-50Hz into the 5th slot, is-online into the 6th (see NOTE above).
    UpdateRunning(lfTimeStep, liFramesSinceNetworkStart, lpOutputBuffer->GetGuiOutputQueue(),
                  lpActiveRaceCarOutputInterface, lbIsSimTimerFrequency50Hz, lbIsOnline);

    UpdateResults(lfTimeStep, lpOutputBuffer->GetGuiOutputQueue());
    WriteDataToOutput(lpOutputBuffer);
    UpdateFreeburnSkillsThisFrame(lpActiveRaceCarOutputInterface, lfTimeStep);
}

// ----------------------------------------------------------------------------
// UpdateRunning -- X360 0x82353008.
// ----------------------------------------------------------------------------
// First reconciles the two "make every challenge N-player" debug toggles against the last-applied
// state: when a toggle newly turns ON, every challenge's active player count (ChallengeListEntry
// byte +0xD3 low nibble) is forced to 1 (all-one-player) or 2 (all-two-player) via SetNumPlayers;
// when it turns OFF, UnHackAllChallenges restores the backed-up counts. The effective all-two flag
// is gated by the all-one flag (all-one wins).
//
// Then, only while the manager is RUNNING, runs the countdown timer and (when it is still going)
// the per-challenge update, latching the local player's received-success flag; the meet-up /
// convoy arbitration pass runs on the trailing gate.
//
// X360 slot NOTE (mirrors PreWorldUpdate): the 5th parameter (declared lbIsOnline) carries the
// SIM-50Hz predicate the binary routes into UpdateChallenge, and the 6th (declared
// lbIsTimerFrequency50Hz) carries the online flag that gates UpdateArbitration. The parameters are
// used strictly in that slot order below, reproducing the X360 data flow exactly.
// ----------------------------------------------------------------------------
void ChallengeManager::UpdateRunning(
    f32                                                                          lfTimeStep,
    s32                                                                          liFramesSinceNetworkStart,
    TGameActionQueue*                                                            lpActionQueue,
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface,
    bool                                                                         lbIsOnline,
    bool                                                                         lbIsTimerFrequency50Hz)
{
    // ---- all-one-player toggle ---- (byte101C vs byte_82FAD9C1 == mbChallengesAreAllOnePlayer)
    if (mbWereAllChallengesOnePlayer != mbChallengesAreAllOnePlayer)
    {
        if (mbChallengesAreAllOnePlayer)
        {
            // Hack every entry's active player count to 1 (X360 rlwimi ,1,28,23 -> low nibble = 1).
            for (s32 liChallenge = 0; liChallenge < mpFreeburnChallengeList->GetChallengeCount(); ++liChallenge)
            {
                BrnResource::ChallengeListEntry* lpChallenge =
                    const_cast<BrnResource::ChallengeListEntry*>(mpFreeburnChallengeList->GetChallengeData(liChallenge));
                lpChallenge->SetNumPlayers(1);
            }
        }
        else
        {
            UnHackAllChallenges();
        }
        mbWereAllChallengesOnePlayer = mbChallengesAreAllOnePlayer;
    }

    // Effective all-two-player state: mbChallengesAreAllTwoPlayer AND NOT mbChallengesAreAllOnePlayer
    // (all-one wins). X360: `(!C1 && C0) ? 1 : 0`, C1 == AllOnePlayer, C0 == AllTwoPlayer.
    const bool lbEffectiveAllTwoPlayer =
        (!mbChallengesAreAllOnePlayer) && mbChallengesAreAllTwoPlayer;
    if (mbWereAllChallengesTwoPlayer != lbEffectiveAllTwoPlayer)
    {
        if (lbEffectiveAllTwoPlayer)
        {
            // Hack every entry's active player count to 2 (X360 rlwimi ,1,28,23 with rol 1 -> low nibble = 2).
            for (s32 liChallenge = 0; liChallenge < mpFreeburnChallengeList->GetChallengeCount(); ++liChallenge)
            {
                BrnResource::ChallengeListEntry* lpChallenge =
                    const_cast<BrnResource::ChallengeListEntry*>(mpFreeburnChallengeList->GetChallengeData(liChallenge));
                lpChallenge->SetNumPlayers(2);
            }
        }
        else
        {
            UnHackAllChallenges();
        }
        mbWereAllChallengesTwoPlayer =
            (!mbChallengesAreAllOnePlayer) && mbChallengesAreAllTwoPlayer;
    }

    if (meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RUNNING)   // dwordE08 == 2
    {
        bool lbTimerNotFinished = false;   // r28; set when the countdown is still running

        CGS_ASSERT(mpCurrentChallenge, "mpCurrentChallenge");             // X360 line 0x3E3

        const ::EActiveRaceCarIndex lePlayerActiveRaceCarIndex =
            lpActiveRaceCarOutputInterface->GetPlayerActiveRaceCarIndex(); // inlined +10328 read + "index set" assert (h:980)
        CGS_ASSERT(lePlayerActiveRaceCarIndex >= ::E_ACTIVE_RACE_CAR_INDEX_0,
                   "lePlayerActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");      // X360 line 0x3E7
        CGS_ASSERT(lePlayerActiveRaceCarIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "lePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");   // X360 line 0x3E8

        if (UpdateTimer(lfTimeStep))
        {
            // 5th param (X360-routed SIM-50Hz predicate) flows into UpdateChallenge.
            UpdateChallenge(lfTimeStep, liFramesSinceNetworkStart, lpActiveRaceCarOutputInterface,
                            lpActionQueue, lbIsOnline);
            mabReceivedSuccessUpdates[lePlayerActiveRaceCarIndex] = true;  // stb 1,+0x590+idx
        }
        else
        {
            lbTimerNotFinished = true;
        }

        // 6th param (X360-routed online flag) gates the arbitration pass.
        if (lbIsTimerFrequency50Hz)
        {
            UpdateArbitration(lfTimeStep, lpActionQueue, lpActiveRaceCarOutputInterface, lbTimerNotFinished);
        }
    }
}

} // namespace BrnGameState
