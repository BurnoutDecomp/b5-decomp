// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wB_09.cpp
// ============================================================================
// Wave-B partfile (group 9) for BrnGameState::ChallengeManager -- the network-player
// completion-slot book-keeping trio. Store-for-store reconstruction over the keystone's
// NAMED members (see BrnChallengeManager.h for the layout evidence):
//
//   NetworkPlayerAdded     @ 0x823240B8  (DWARF :241)  -- claim a maChallengeCompletionData slot
//
// The maChallengeCompletionData slot layout (stride 0x108: FastBitArray<2000> bit store @+0x00,
// NetworkPlayerID @+0x100 where -1 == free, mbFinalised @+0x104) is the same free-slot/claim
// pattern the committed Construct/Destruct foundation uses.
//
// GROUP FUNCS BLOCKED (reported in funcs_blocked; NOT bodied here):
//   * NetworkPlayerFinalised @ 0x82347E88 -- unconditionally builds + posts the id-161
//     E_ACTION_ACTIVE_FREEBURN_CHALLENGE action (size 0x30). Its payload struct
//     {CgsID mChallengeID @+0x00, EActiveRaceCarIndex maPlayers[7] @+0x08 (packed indices of
//     the players who started the challenge), BrnNetwork::NetworkPlayerID mPlayerID @+0x24,
//     s32 miNumPlayersInChallenge @+0x28} was NOT frozen into BrnGameActions.h (only the
//     id-155/158/159 challenge actions were). It cannot be bodied store-for-store without
//     inventing that action type (out of scope -- same precedent as the id-153 gap that
//     blocked BeginChallenge/TriggerFreeburnChallenge in BrnChallengeManager_wB_07.cpp). The
//     id-156 half maps cleanly onto the header's ChallengeCompletionData, but the id-161 half
//     is a hard gap, so the whole function is blocked.
//   * NetworkPlayerRemoved @ 0x8234E420 -- unconditionally calls
//     mpGameStateModule->GetActiveRaceCarIndex(lPlayerID), but
//     BrnGameState::GameStateModule::GetActiveRaceCarIndex(BrnNetwork::NetworkPlayerID) is NOT
//     declared on GameStateModule in BrnGameStateModule.h (the method lives only on the
//     Network/Skillz classes). Cannot call it without editing that header (out of scope).
// ============================================================================

#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h"
#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManagerDebugComponent.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/GameState/BrnGameEvents.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// NetworkPlayerAdded -- X360 0x823240B8. Register a newly-joined network player in the
// completion-tracking pool. Posts no actions (lpActionQueue / lbIsOnline are unused by the
// X360 body). Two passes over maChallengeCompletionData (stride 0x108, scanning the
// mNetworkPlayerID field at slot+0x100):
//   1. Duplicate guard: if any slot already holds this player id, fire the "same player"
//      assert (the X360 builds the id into the message via StrStream -- the assert front-end,
//      represented by the failure condition per the committed foundation convention).
//   2. Claim the first free slot (mNetworkPlayerID == -1). If none is free, fire the
//      "added more than KI_MAX_REMOTE_PLAYERS players" assert. On success: store the player id,
//      clear the finalised flag, and wipe the completion bit store (X360 stw +0x100, stb 0
//      +0x104, then 32 std zeroes over the 256-byte FastBitArray == UnSetAll()).
// ----------------------------------------------------------------------------
void ChallengeManager::NetworkPlayerAdded(BrnNetwork::NetworkPlayerID lPlayerID,
                                          TGameActionQueue*, bool)
{
    // Pass 1 -- duplicate detection. The X360 fires only when a matching slot is found AND its
    // element pointer is non-null; a member-array element address is never null, so a match
    // alone triggers the assert.
    ChallengeCompletionData* lpExistingEntry = 0;   // X360 r24 seed 0
    for (s32 liSlot = 0; liSlot < KI_MAX_REMOTE_PLAYERS; ++liSlot)
    {
        if (maChallengeCompletionData[liSlot].mNetworkPlayerID == lPlayerID)   // X360 lwz slot+0x100; cmpw
        {
            lpExistingEntry = &maChallengeCompletionData[liSlot];
            break;
        }
    }
    CGS_ASSERT(lpExistingEntry == 0, "Trying to add the same player to the challenge manager");

    // Pass 2 -- claim the first free slot (mNetworkPlayerID == -1).
    ChallengeCompletionData* lpFreeEntry = 0;   // X360 r24 re-seed 0
    for (s32 liSlot = 0; liSlot < KI_MAX_REMOTE_PLAYERS; ++liSlot)
    {
        if (maChallengeCompletionData[liSlot].mNetworkPlayerID == -1)   // X360 lwz slot+0x100; cmpwi -1
        {
            lpFreeEntry = &maChallengeCompletionData[liSlot];
            break;
        }
    }
    CGS_ASSERT(lpFreeEntry != 0, "Added more than KI_MAX_REMOTE_PLAYERS players to the challenge manager");

    lpFreeEntry->mNetworkPlayerID = lPlayerID;         // X360 stw r23,+0x100
    lpFreeEntry->mbFinalised      = false;             // X360 stb 0,+0x104
    lpFreeEntry->mCompletedChallenges.UnSetAll();      // X360 32 std 0 over the 256-byte bit store
}

} // namespace BrnGameState
