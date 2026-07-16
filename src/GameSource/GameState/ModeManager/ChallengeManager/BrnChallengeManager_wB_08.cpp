// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wB_08.cpp
// ============================================================================
// Wave-B partfile for BrnGameState::ChallengeManager -- the challenge-END spine.
// Store-for-store reconstructions of the X360 ARTIST methods over the keystone's NAMED
// members (see BrnChallengeManager.h for the layout evidence):
//
//   RemoteEndChallenge @ 0x82347090  (DWARF :185)  -- remote flavour of the end tail
//   Disconnected       @ 0x8234E548  (DWARF :265)  -- local-player-disconnect teardown
//
// NOTE (group 8, func blocked):
//   EndChallenge @ 0x8234DE30 (DWARF :180) -- the local challenge-completion spine -- posts
//   the id-153 (E_ACTION_FREEBURN_CHALLENGE, 0x20-byte) challenge action (X360 li r5,0x99;
//   li r6,0x20; AddEvent). That action's dedicated payload struct was NOT frozen into
//   BrnGameActions.h (only the id-155 update / id-158 success-update / id-159 success actions
//   were). It cannot be bodied store-for-store without inventing that type (out of scope: no
//   header edits, no forked/local action structs -- the same call the sibling group-7 partfile
//   made for BeginChallenge/TriggerFreeburnChallenge). Reported in funcs_blocked with the exact
//   missing struct + attested layout.
//
//   Disconnected only CALLS EndChallenge (declared in the frozen header) and posts nothing
//   itself; RemoteEndChallenge only ever posts the empty id-154 end-not-active action. Both are
//   therefore bodiable here.
// ============================================================================

#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                // CGS_ASSERT
#include "GameSource/GameState/BrnGameActions.h"                  // GameStateModuleIO::{GameAction<>, E_ACTION_FREEBURN_CHALLENGE_END_NOT_ACTIVE}
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // TGameActionQueue::AddEvent, CgsModule::Event
#include "SharedClasses/DataLists/ChallengeListEntry.h"           // BrnResource::ChallengeListEntry::GetChallengeID

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// RemoteEndChallenge -- X360 0x82347090. Apply a remote player's challenge-end notification.
//
//   * The status must not be E_CHALLENGE_STATUS_ONGOING (non-fatal assert @2494).
//   * If a local challenge is still live (manager not NONE) the remote end cannot be applied
//     yet: latch the incoming status + set mbRemoteEndPending so UpdateRemoteRequests replays
//     it once the local challenge terminates, then return.
//   * Otherwise (manager NONE): on a SUCCESS end, mark the just-completed challenge complete
//     for every claimed remote slot (SetRemotePlayersChallengeCompleted by list index), then --
//     regardless of the end status -- post the empty id-154 end-not-active action and clear the
//     per-player success scratch.
// ----------------------------------------------------------------------------
void ChallengeManager::RemoteEndChallenge(TGameActionQueue* lpActionQueue, EChallengeStatus leChallengeStatus)
{
    CGS_ASSERT(leChallengeStatus != E_CHALLENGE_STATUS_ONGOING,
               "leChallengeStatus != E_CHALLENGE_STATUS_ONGOING");   // X360 cmpwi r30,0; bne skip; @2494

    if (meChallengeManagerStatus != E_CHALLENGE_MANAGER_STATUS_NONE)   // X360 lwz +0xE08; cmpwi 0; bne
    {
        meLocalChallengeStatus = leChallengeStatus;   // X360 stw r30,+0xE30
        mbRemoteEndPending      = true;               // X360 stb 1,+0xE37
        return;                                       // X360 b restore
    }

    if (leChallengeStatus == E_CHALLENGE_STATUS_SUCCESS)   // X360 loc_823470F4: cmpwi r30,1; bne
    {
        CGS_ASSERT(mpCurrentChallenge != 0, "mpCurrentChallenge");   // X360 lwz +0xE0C; @2509

        // X360 ld +0xC0 (mpCurrentChallenge->GetChallengeID()); bl GetChallengeIndex.
        const s32 liChallengeIndex = GetChallengeIndex(mpCurrentChallenge->GetChallengeID());
        if (liChallengeIndex >= 0)   // X360 cmpwi r4,0; bge
        {
            SetRemotePlayersChallengeCompleted(liChallengeIndex);   // X360 bl SetRemotePlayersChallengeCompleted
        }
        else
        {
            CGS_ASSERT(liChallengeIndex >= 0, "liChallengeIndex >= 0");   // X360 @2511
        }
    }

    // Empty end-not-active action (no payload): X360 posts &stack byte, size 1, id 154.
    GameStateModuleIO::GameAction<GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE_END_NOT_ACTIVE>
        lEndNotActiveAction;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lEndNotActiveAction),
                            GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE_END_NOT_ACTIVE, 1);  // X360 li r5,0x9A; li r6,1

    ClearPlayerSuccessData();   // X360 bl ClearPlayerSuccessData
}

// ----------------------------------------------------------------------------
// Disconnected -- X360 0x8234E548. The local player has dropped out of the online session:
// wipe every remote-player completion record, then tear down any in-flight challenge.
//
//   * Per remote slot (X360 loop base +0x5C8, stride 0x108: 32 std zeroing the completion bit
//     store, stw -1 @+0x100, stb 0 @+0x104): clear the completed-challenges bits, free the slot
//     (-1), clear the finalised flag.
//   * If the manager is not NONE, tear the challenge down via EndChallenge (X360 tail-call):
//       - RUNNING or RESULTS -> E_CHALLENGE_STATUS_ABORTED (X360 li r4,2)
//       - otherwise (PENDING) -> E_CHALLENGE_STATUS_ABORTED_BEFORE_STARTING (X360 li r4,4)
//     both with lbIsOnline == false (X360 li r6,0). A NONE manager returns after the wipe
//     (X360 beqlr).
// ----------------------------------------------------------------------------
void ChallengeManager::Disconnected(TGameActionQueue* lpActionQueue)
{
    for (s32 liSlot = 0; liSlot < KI_MAX_REMOTE_PLAYERS; ++liSlot)
    {
        maChallengeCompletionData[liSlot].mCompletedChallenges.UnSetAll();   // X360 32x stdx 0, base +0x5C8
        maChallengeCompletionData[liSlot].mNetworkPlayerID = -1;             // X360 stw -1, +0x100
        maChallengeCompletionData[liSlot].mbFinalised      = false;          // X360 stb 0, +0x104
    }

    if (meChallengeManagerStatus != E_CHALLENGE_MANAGER_STATUS_NONE)   // X360 lwz +0xE08; cmpwi 0; beqlr
    {
        // X360 folds (status == 2 || status == 3) into a bool, then selects 2 vs 4.
        if (meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RUNNING ||   // X360 cmpwi 2
            meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RESULTS)     // X360 cmpwi 3
        {
            EndChallenge(E_CHALLENGE_STATUS_ABORTED, lpActionQueue, false);              // X360 li r4,2
        }
        else
        {
            EndChallenge(E_CHALLENGE_STATUS_ABORTED_BEFORE_STARTING, lpActionQueue, false); // X360 li r4,4
        }
    }
}

} // namespace BrnGameState
