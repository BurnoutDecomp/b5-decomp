// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wB_04.cpp
// ============================================================================
// Wave-B partfile (group 4) for BrnGameState::ChallengeManager. Challenge-list + profile
// walkers, store-for-store from the X360 ARTIST asm over the keystone's named members:
//   GetChallengeIndex               @ 0x82333238
//   CheckForOnlineChallengeUnlocks  @ 0x82333100
//   OnProfileLoaded                 @ 0x82334B00
// ============================================================================

#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h"
#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManagerDebugComponent.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/GameState/BrnGameEvents.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "SharedClasses/DataLists/ChallengeList.h"                       // BrnResource::ChallengeList
#include "SharedClasses/DataLists/ChallengeListEntry.h"                  // BrnResource::ChallengeListEntry
#include "GameSource/GameState/Progression/BrnProgressionManager.h"      // BrnProgression::ProgressionManager
#include "GameSource/GameState/Progression/BrnProfile.h"                 // BrnProgression::Profile
#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h" // AchievementManagerBase

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// GetChallengeIndex -- X360 0x82333238. Forwards to the cached freeburn challenge list's
// index lookup after asserting the list is present; on a missing id the X360 builds a
// "Failed to find index for challenge <id> from list of <count> challenges" assert message
// via StrStream and fires it (the message machinery is the assert front-end -- represented
// here by the failure-condition CGS_ASSERT, exactly as the committed foundation bodies do).
// ----------------------------------------------------------------------------
s32 ChallengeManager::GetChallengeIndex(CgsID lChallengeID) const
{
    CGS_ASSERT(mpFreeburnChallengeList != 0, "mpFreeburnChallengeList");   // X360 asserts +0xE38 non-null (line 2566)

    const s32 liChallengeIndex = mpFreeburnChallengeList->GetChallengeIndex(lChallengeID);
    CGS_ASSERT(liChallengeIndex >= 0, "liChallengeIndex >= 0");            // X360 asserts index found (line 2570)

    return liChallengeIndex;
}

// ----------------------------------------------------------------------------
// CheckForOnlineChallengeUnlocks -- X360 0x82333100. Tally, by challenge player-count, how many
// challenges the local player has completed (mpProgression->GetProfile()->HasPlayerCompleted...),
// then for the multi-player player-count categories 1..7 count how many are fully cleared
// (completed >= the challenge-list histogram maChallengePlayerCounts). Once two categories are
// fully cleared, unlock trophy 10 and notify the achievement manager (fires for every qualifying
// category from the second onwards -- the X360 shape kept verbatim).
// ----------------------------------------------------------------------------
void ChallengeManager::CheckForOnlineChallengeUnlocks()
{
    // X360 zeroes an 8-entry stack tally (init loop of 8 word stores).
    s32 laiCompletedPerPlayerCount[KI_MAX_CHALLENGE_PLAYERS];
    for (s32 liInit = 0; liInit < KI_MAX_CHALLENGE_PLAYERS; ++liInit)
    {
        laiCompletedPerPlayerCount[liInit] = 0;
    }

    for (s32 liChallenge = 0; liChallenge < mpFreeburnChallengeList->GetChallengeCount(); ++liChallenge)
    {
        const BrnResource::ChallengeListEntry* lpChallengeListEntry =
            mpFreeburnChallengeList->GetChallengeData(liChallenge);
        CGS_ASSERT(lpChallengeListEntry != 0, "lpChallengeListEntry != NULL");   // X360 line 2392

        if (mpProgression->GetProfile()->HasPlayerCompletedFreeburnChallenge(lpChallengeListEntry->GetChallengeID()))
        {
            ++laiCompletedPerPlayerCount[lpChallengeListEntry->GetNumPlayers() - 1];   // X360 tally[nibble-1]++
        }
    }

    s32 liNumFullyCompletedCategories = 0;
    for (s32 liPlayerCount = 1; liPlayerCount < KI_MAX_CHALLENGE_PLAYERS; ++liPlayerCount)
    {
        if (laiCompletedPerPlayerCount[liPlayerCount] >= maChallengePlayerCounts[liPlayerCount] &&
            ++liNumFullyCompletedCategories >= 2)
        {
            mpProgression->OnTrophyUnlock(10);                                             // X360 li r4,0xA
            mpProgression->GetAchievementManager()->OnFreeburnChallengeBlockComplete();    // X360 lwzx progression+0x20938
        }
    }
}

// ----------------------------------------------------------------------------
// OnProfileLoaded -- X360 0x82334B00. Rebuilds the local completion bit array from the freshly
// loaded profile: for every challenge the profile records as completed, set that challenge's bit
// in mLocalChallengeCompletionData (the inlined FastBitArray<2000> range assert is folded into
// the committed SetBit).
// ----------------------------------------------------------------------------
void ChallengeManager::OnProfileLoaded()
{
    CGS_ASSERT(mpProgression != 0, "mpProgression");                    // X360 line 4895
    CGS_ASSERT(mpFreeburnChallengeList != 0, "mpFreeburnChallengeList"); // X360 line 4896

    for (s32 liChallenge = 0; liChallenge < mpFreeburnChallengeList->GetChallengeCount(); ++liChallenge)
    {
        const BrnResource::ChallengeListEntry* lpChallengeListEntry =
            mpFreeburnChallengeList->GetChallengeData(liChallenge);
        if (mpProgression->GetProfile()->HasPlayerCompletedFreeburnChallenge(lpChallengeListEntry->GetChallengeID()))
        {
            mLocalChallengeCompletionData.SetBit(liChallenge);
        }
    }
}

} // namespace BrnGameState
