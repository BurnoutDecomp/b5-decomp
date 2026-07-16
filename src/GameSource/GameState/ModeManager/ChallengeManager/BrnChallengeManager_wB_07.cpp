// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wB_07.cpp
// ============================================================================
// Wave-B partfile for BrnGameState::ChallengeManager -- the challenge CANCEL path.
// Store-for-store reconstruction of one X360 ARTIST method over the keystone's NAMED
// members (see BrnChallengeManager.h for the layout evidence):
//
//   CancelFreeburnChallenge @ 0x823506E8  (DWARF :162)
//
// NOTE (group 7, funcs blocked): the other two functions of this group --
//   BeginChallenge          @ 0x823505B8
//   TriggerFreeburnChallenge @ 0x82346DA0
// both post the id-153 (E_ACTION_FREEBURN_CHALLENGE, 0x20-byte) begin/trigger action,
// whose dedicated payload struct was NOT frozen into BrnGameActions.h (only the id-155/158/
// 159 challenge actions were). They cannot be bodied store-for-store without inventing that
// type (out of scope: no header edits, no forked/local action structs). Reported in
// funcs_blocked with the exact missing struct + attested layout. CancelFreeburnChallenge
// only ever posts the empty id-154 end-not-active action, so it IS bodiable here.
// ============================================================================

#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameSource/GameState/BrnGameActions.h"             // GameStateModuleIO::{GameAction<>, E_ACTION_FREEBURN_CHALLENGE_END_NOT_ACTIVE}
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h" // TGameActionQueue::AddEvent, CgsModule::Event

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// CancelFreeburnChallenge -- X360 0x823506E8. Abort the in-flight freeburn challenge (local
// or, when nothing is running yet, notify the network that the currently-selected challenge
// is no longer active).
//
//   * If the manager is in any non-NONE state, tear the challenge down via EndChallenge:
//       - RUNNING or RESULTS -> E_CHALLENGE_STATUS_ABORTED
//       - otherwise (PENDING) -> E_CHALLENGE_STATUS_ABORTED_BEFORE_STARTING
//     (both with lbIsOnline == false, matching the X360 `li r6,0`).
//   * If the manager is NONE but a challenge is still selected (mpCurrentChallenge != NULL),
//     post the empty id-154 end-not-active action and clear the per-player success scratch.
//     The X360 posts an uninitialised 1-byte stack buffer (the action carries no payload) --
//     reproduced with the empty GameAction<E_ACTION_FREEBURN_CHALLENGE_END_NOT_ACTIVE> tag.
// ----------------------------------------------------------------------------
void ChallengeManager::CancelFreeburnChallenge(TGameActionQueue* lpActionQueue)
{
    if (meChallengeManagerStatus != E_CHALLENGE_MANAGER_STATUS_NONE)   // X360 lwz +0xE08; cmpwi 0; bne
    {
        // X360 computes (status == 2 || status == 3) into a bool, then branches on == 1.
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
    else if (mpCurrentChallenge != 0)   // X360 loc_82350758: lwz +0xE0C; cmplwi 0; beq -> return
    {
        CGS_ASSERT(lpActionQueue != 0, "lpOutput");   // X360 cmplwi r30,0; assert @ ...2213

        // Empty end-not-active action (no payload): X360 posts &stack, size 1, id 154.
        GameStateModuleIO::GameAction<GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE_END_NOT_ACTIVE>
            lEndNotActiveAction;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lEndNotActiveAction),
                                GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE_END_NOT_ACTIVE, 1);  // X360 li r5,0x9A; li r6,1

        ClearPlayerSuccessData();   // X360 bl ClearPlayerSuccessData
    }
}

} // namespace BrnGameState
