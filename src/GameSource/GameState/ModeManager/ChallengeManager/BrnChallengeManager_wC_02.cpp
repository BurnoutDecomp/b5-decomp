// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wC_02.cpp
// ============================================================================
// Wave-C partfile (group G3) for BrnGameState::ChallengeManager -- the network-player
// book-keeping trio that the wave-B pass had to leave blocked (the id-156/160/161 action
// payload structs and GameStateModule::GetActiveRaceCarIndex are now declared in the frozen
// headers). Store-for-store reconstruction over the NAMED members declared in
// BrnChallengeManager.h / BrnGameActions.h / BrnGameStateModule.h:
//
//   NetworkPlayerFinalised            @ 0x82347E88  (DWARF :248)
//   NetworkPlayerRemoved              @ 0x8234E420  (DWARF :255)
//   SetRemotePlayersChallengeCompleted@ 0x82323DF8  (DWARF :7xx private helper)
//
// The maChallengeCompletionData slot layout (stride 0x108: FastBitArray<2000> bit store
// @+0x00, NetworkPlayerID @+0x100 where -1 == free, mbFinalised @+0x104) is the committed
// Construct/NetworkPlayerAdded shape (BrnChallengeManager.cpp / _wB_09.cpp).
// ============================================================================

#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h"
#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManagerDebugComponent.h"
#include "GameSource/GameState/BrnGameActions.h"                 // GameStateModuleIO::{FburnChallengeStatusAction, FburnChallengeShowSelectorAction, ActiveFburnChallengeAction}
#include "GameSource/GameState/BrnGameStateModule.h"             // GameStateModule::GetActiveRaceCarIndex
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h" // TGameActionQueue::AddEvent, CgsModule::Event
#include "GameShared/GameClasses/Core/CgsAssert.h"               // CGS_ASSERT

#include <cstring>                                               // memcpy

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// NetworkPlayerFinalised -- X360 0x82347E88. A remote player's join has completed: mark that
// player's completion slot finalised, send that player OUR completion bit store (id-156), and
// -- when online and a challenge is actually up -- tell them which challenge is active and who
// is in it (id-161).
//
// The slot scan is the same stride-0x108 walk over maChallengeCompletionData the sibling
// NetworkPlayerAdded uses; a miss leaves lpEntry NULL and fires the NON-GATING "lpEntry"
// assert (X360 :4958) -- the body then proceeds to the store exactly as the asm does (the
// element-address test `r30 != 0` after a hit is never false for a member array element).
// ----------------------------------------------------------------------------
void ChallengeManager::NetworkPlayerFinalised(BrnNetwork::NetworkPlayerID lPlayerID,
                                              TGameActionQueue* lpActionQueue,
                                              bool lbIsOnline)
{
    ChallengeCompletionData* lpEntry = 0;   // X360 li r30, 0
    for (s32 liSlot = 0; liSlot < KI_MAX_REMOTE_PLAYERS; ++liSlot)
    {
        if (maChallengeCompletionData[liSlot].mNetworkPlayerID == lPlayerID)   // X360 lwz slot+0x100; cmpw
        {
            lpEntry = &maChallengeCompletionData[liSlot];
            break;
        }
    }
    CGS_ASSERT(lpEntry != 0, "lpEntry");   // X360 :4958 -- non-gating

    lpEntry->mbFinalised = true;           // X360 stb 1, 0x104(r30)

    // id-156: hand this player our own completion bit store. The X360 emits a plain
    // memcpy(&action, &mLocalChallengeCompletionData, 0x100) -- the action's
    // GameStateModuleIO::CompletedFburnChallenges field and the manager's
    // CgsContainers::FastBitArray<2000> member are the identical 256-byte bit store
    // (same bridge the committed UpdateRemotePlayerSuccessStatus uses).
    GameStateModuleIO::FburnChallengeStatusAction lStatusAction;
    std::memcpy(&lStatusAction.mCompletedChallenges, &mLocalChallengeCompletionData, 256);
    lStatusAction.mPlayerID = lPlayerID;    // X360 stw r29, sp+0x180 (action+0x100)

    CGS_ASSERT(lpActionQueue != 0, "lpActionQueue");   // X360 :4964
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lStatusAction),
                            GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE_COMPLETION_STATUS,
                            sizeof(GameStateModuleIO::FburnChallengeStatusAction));   // X360 li r5,0x9C; li r6,0x108

    if (lbIsOnline)
    {
        // X360 materialises (status == 2 || status == 3) into a byte, then branches on it.
        if (meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RUNNING ||   // X360 cmpwi 2
            meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RESULTS)     // X360 cmpwi 3
        {
            CGS_ASSERT(mpCurrentChallenge != 0, "mpCurrentChallenge");   // X360 :4974

            GameStateModuleIO::ActiveFburnChallengeAction lActiveChallengeAction;
            lActiveChallengeAction.mChallengeID            = mpCurrentChallenge->GetChallengeID(); // X360 ld +0xC0
            lActiveChallengeAction.mPlayerToSendToID       = lPlayerID;                            // X360 stw sp+0x74
            lActiveChallengeAction.miNumPlayersInChallenge = 0;                                    // X360 stw sp+0x78

            for (EActiveRaceCarIndex leActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
                 (s32)leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT;
                 leActiveRaceCarIndex++)   // X360 op++ carries the BurnoutConstants.h:39 range assert
            {
                if (mabPlayerStartedChallenge[leActiveRaceCarIndex])   // X360 lbzx r11, r26(this+0x598), r30
                {
                    CGS_ASSERT(lActiveChallengeAction.miNumPlayersInChallenge <
                                   GameStateModuleIO::ActiveFburnChallengeAction::KI_MAX_NETWORK_PLAYERS,
                               "lActiveChallengeAction.miNumPlayersInChallenge < KI_MAX_NETWORK_PLAYERS");   // X360 :4983 -- non-gating
                    lActiveChallengeAction.maePlayersInChallengeARCI[
                        lActiveChallengeAction.miNumPlayersInChallenge] = leActiveRaceCarIndex;   // X360 stwx r30, count*4, sp+0x58
                    ++lActiveChallengeAction.miNumPlayersInChallenge;   // X360 lwz sp+0x78; addi 1; stw back
                }
            }

            lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lActiveChallengeAction),
                                    GameStateModuleIO::E_ACTION_ACTIVE_FREEBURN_CHALLENGE,
                                    sizeof(GameStateModuleIO::ActiveFburnChallengeAction));   // X360 li r5,0xA1; li r6,0x30
        }
    }
}

// ----------------------------------------------------------------------------
// NetworkPlayerRemoved -- X360 0x8234E420. A network player left: release their completion
// slot, and if they were taking part in the live challenge, abort it (bouncing everyone back
// to the challenge selector when online).
//
// The slot release mirrors the Construct/NetworkPlayerAdded init pattern in reverse
// (stb 0 +0x104 / stw -1 +0x100 / 32 std zeroes over the 256-byte bit store == UnSetAll()).
// ----------------------------------------------------------------------------
void ChallengeManager::NetworkPlayerRemoved(BrnNetwork::NetworkPlayerID lPlayerID,
                                            TGameActionQueue* lpActionQueue,
                                            bool lbIsOnline)
{
    ChallengeCompletionData* lpEntry = 0;   // X360 r31 seed 0
    for (s32 liSlot = 0; liSlot < KI_MAX_REMOTE_PLAYERS; ++liSlot)
    {
        if (maChallengeCompletionData[liSlot].mNetworkPlayerID == lPlayerID)   // X360 lwz slot+0x100; cmpw
        {
            lpEntry = &maChallengeCompletionData[liSlot];
            break;
        }
    }

    if (lpEntry != 0)   // X360 cmplwi r31,0; beq -> skip the release
    {
        lpEntry->mbFinalised      = false;          // X360 stb r28(0), 0x104(r31)
        lpEntry->mNetworkPlayerID = -1;             // X360 stw -1, 0x100(r31)
        lpEntry->mCompletedChallenges.UnSetAll();   // X360 32 std 0 over the 256-byte bit store
    }

    if (meChallengeManagerStatus != E_CHALLENGE_MANAGER_STATUS_NONE)   // X360 lwz +0xE08; cmpwi 0; beq -> return
    {
        CGS_ASSERT(mpGameStateModule != 0, "mpGameStateModule");   // X360 :5026

        const EActiveRaceCarIndex leActiveRaceCarIndex = mpGameStateModule->GetActiveRaceCarIndex(lPlayerID);
        if (leActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID &&   // X360 cmpwi r3,-1; beq -> return
            mabPlayerStartedChallenge[leActiveRaceCarIndex])              // X360 lbz 0x598(r3+this)
        {
            if (lbIsOnline && lpEntry != 0)   // X360 clrlwi r26; beq / cmplwi r31,0; beq
            {
                // The X360 posts the action ZEROED (std r28 == 0 into the single CgsID field):
                // "no challenge" -> the GUI falls back to the selector.
                GameStateModuleIO::FburnChallengeShowSelectorAction lShowSelectorAction;
                lShowSelectorAction.mChallengeID = 0;
                lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lShowSelectorAction),
                                        GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE_SHOW_SELECTOR,
                                        sizeof(GameStateModuleIO::FburnChallengeShowSelectorAction));   // X360 li r5,0xA0; li r6,8
            }

            EndChallenge(E_CHALLENGE_STATUS_ABORTED_DUE_TO_PLAYER_LEAVE, lpActionQueue, false);   // X360 li r4,3; li r6,0
        }
    }
}

// ----------------------------------------------------------------------------
// SetRemotePlayersChallengeCompleted -- X360 0x82323DF8. Called from EndChallenge /
// RemoteEndChallenge when the local player completes challenge index liChallengeIndex: every
// remote player who STARTED this challenge gets the same challenge marked complete in their
// own completion bit store (they finished it alongside us).
//
// The X360 inlines FastBitArray<2000>::SetBit (the "Index N is out of range (max bits: 2000)"
// StrStream blob at CgsFastBitArray.h:431 is that container's own range assert, and the
// shift/or triple is its bit math) -- call the committed container method.
// ----------------------------------------------------------------------------
void ChallengeManager::SetRemotePlayersChallengeCompleted(s32 liChallengeIndex)
{
    for (s32 liSlot = 0; liSlot < KI_MAX_REMOTE_PLAYERS; ++liSlot)   // X360 counts 7 down, slot stride 0x108
    {
        if (maChallengeCompletionData[liSlot].mNetworkPlayerID != -1)   // X360 lwz +0x100; cmpwi -1; beq next
        {
            CGS_ASSERT(mpGameStateModule != 0, "mpGameStateModule");   // X360 :2436

            const EActiveRaceCarIndex leActiveRaceCarIndex =
                mpGameStateModule->GetActiveRaceCarIndex(maChallengeCompletionData[liSlot].mNetworkPlayerID);
            CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                       "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");        // X360 :2438 -- non-gating
            CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                       "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");     // X360 :2439 -- non-gating

            if (mabPlayerStartedChallenge[leActiveRaceCarIndex])   // X360 lbz 0x598(r31+this)
            {
                maChallengeCompletionData[liSlot].mCompletedChallenges.SetBit(liChallengeIndex);
            }
        }
    }
}

} // namespace BrnGameState
