// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wB_10.cpp
// ============================================================================
// Wave-B partfile for BrnGameState::ChallengeManager -- the remote completion-status
// intake plumbing. Store-for-store reconstruction over the keystone's NAMED members
// (see BrnChallengeManager.h for the layout evidence):
//
//   UpdateRemotePlayerSuccessStatus @ 0x8234E210 (DWARF :492) -- drain the incoming
//     CompletedFburnChallengesData queue into the per-player completion records, then
//     re-broadcast the every-player status event.
//
// GROUP NOTE: two sibling methods of this group hit genuine FROZEN-header gaps and are
// left for a follow-up (reported in funcs_blocked, exact gaps below); they are NOT bodied
// here per the "skip + report, do not hack around a missing accessor" rule:
//
//   * OutputFreeburnChallengeEveryPlayerStatusEvent @ 0x82348080 -- its pre-loop
//     `memcpy(event+0x738, &mLocalChallengeCompletionData, 0x100)` writes the trailing
//     PRIVATE member GameStateModuleIO::FburnChallengeEveryPlayerStatusData::
//     mLocalChallengeCompletionData (BrnGameStateSharedIO.h), which exposes only
//     Construct()/AddCompletionStatus() and no accessor/setter for that field. Reaching it
//     by offset is an offset_hack lint failure and the header must not be edited.
//   * SetRemotePlayersChallengeCompleted @ 0x82323DF8 -- calls
//     BrnGameState::GameStateModule::GetActiveRaceCarIndex(BrnNetwork::NetworkPlayerID),
//     which is NOT declared on GameStateModule anywhere in the committed tree
//     (BrnGameStateModule.h); bodying it needs an additive declare-only grow to that
//     (non-frozen) header, which this partfile is forbidden to make.
//
// (Both are called by name from UpdateRemotePlayerSuccessStatus below / declared in the
// frozen header, so this file compiles against the declarations alone.)
// ============================================================================

#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h"
#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManagerDebugComponent.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"      // GameStateModuleIO::CompletedFburnChallengesData
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h" // CgsModule::BaseEventQueue<T>::GetLength/GetEvent
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT

#include <cstring>                                           // memcpy

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// UpdateRemotePlayerSuccessStatus -- X360 0x8234E210. Called by PreWorldUpdate when online.
// For every event queued on the incoming completed-challenge-status queue, copy that remote
// player's completion bit store into the matching maChallengeCompletionData slot (matched by
// mNetworkPlayerID), then -- if anything was queued -- re-emit the aggregated every-player
// status event onto the action queue.
//
// The queue element is GameStateModuleIO::CompletedFburnChallengesData (264-byte stride:
// s32 mNetworkPlayerID @0 + 4 pad + CompletedFburnChallenges bit store @8). The X360 copies
// the whole element to a stack temp (memcpy 0x108), matches by id, then blits the 256-byte
// bit store into the slot's FastBitArray<2000> mCompletedChallenges. When no slot owns the
// id it fires the "Failed to find ... who sent us data" assert (reduced from the X360's
// StrStream-formatted message per the plain-string CGS_ASSERT convention) and -- faithfully
// to the asm -- still falls through to the (now null-destination) copy.
// ----------------------------------------------------------------------------
void ChallengeManager::UpdateRemotePlayerSuccessStatus(
    const CgsModule::BaseEventQueue<GameStateModuleIO::CompletedFburnChallengesData>* lpCompletedChallengeStatusQueue,
    TGameActionQueue* lpActionQueue)
{
    // X360 cmplwi r20,0; bne -> skip. Non-gating tripwire, then the queue is used regardless.
    CGS_ASSERT(lpCompletedChallengeStatusQueue != 0, "lpCompletedChallengeStatusQueue");

    if (lpCompletedChallengeStatusQueue->GetLength() > 0)   // X360 lwz r11,8(r20); cmpwi 0; ble skip
    {
        s32 liEvent = 0;
        do
        {
            // X360 bl BaseEventQueue<>::GetEvent(v6); memcpy(v30, event, 0x108) -- take a
            // local copy of the whole 264-byte element.
            GameStateModuleIO::CompletedFburnChallengesData lEvent =
                lpCompletedChallengeStatusQueue->GetEvent(liEvent);

            // Find the completion record owned by this player id (X360 walks
            // maChallengeCompletionData[i].mNetworkPlayerID from slot 0, stride 0x108).
            CgsContainers::FastBitArray<2000>* lpDestCompletionData = 0;   // X360 r29 = 0
            s32 liSlot = 0;
            while (maChallengeCompletionData[liSlot].mNetworkPlayerID != lEvent.mNetworkPlayerID)
            {
                ++liSlot;
                if (liSlot >= KI_MAX_REMOTE_PLAYERS)   // X360 cmpwi r11,7; blt loop / else fall to assert
                {
                    break;
                }
            }

            if (liSlot < KI_MAX_REMOTE_PLAYERS)
            {
                // Found: X360 computes r29 = &slot.mCompletedChallenges (the r29 != 0 test is
                // always true for this member address, so the assert below is never taken here).
                lpDestCompletionData = &maChallengeCompletionData[liSlot].mCompletedChallenges;
            }
            else
            {
                CGS_ASSERT(false,
                           "Failed to find challenge status data entry for player () who sent us data\n");
            }

            // X360 LABEL_15 (reached from both the found path and the assert fall-through):
            // memcpy(r29, &event.mCompletedFreeburnChallenges, 0x100). Bridges the slot's
            // FastBitArray<2000> store and the event's CompletedFburnChallenges field (identical
            // 256-byte layouts).
            std::memcpy(lpDestCompletionData, &lEvent.mCompletedFreeburnChallenges, 256);

            ++liEvent;
        }
        while (liEvent < lpCompletedChallengeStatusQueue->GetLength());   // X360 re-reads lwz 8(r20)
    }

    if (lpCompletedChallengeStatusQueue->GetLength() > 0)   // X360 second lwz r11,8(r20); cmpwi 0; ble skip
    {
        CGS_ASSERT(lpActionQueue != 0, "lpActionQueue");   // X360 cmplwi r31,0; bne -> skip
        OutputFreeburnChallengeEveryPlayerStatusEvent(lpActionQueue);
    }
}

} // namespace BrnGameState
