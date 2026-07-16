// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wB_03.cpp
// ============================================================================
// Wave-B partfile 03 -- lifecycle + list. Store-for-store reconstructions of three X360
// ChallengeManager methods, bodied onto the keystone-frozen named layout:
//   * Destruct              (X360 0x8233A9C0) -- mirror of Construct minus the pool/asserts.
//   * Prepare               (X360 0x8233AAF8) -- cache the list, histogram per player count,
//                                                refill the leaping-car pool.
//   * UnHackAllChallenges   (X360 0x82333040) -- restore each entry's backed-up player count.
//
// The manager's own state is reached exclusively through named members. All offsets quoted in
// comments are X360-asm-attested (see the frozen header BrnChallengeManager.h).
// ============================================================================

#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h"
#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManagerDebugComponent.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/GameState/BrnGameEvents.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"

#include "SharedClasses/DataLists/ChallengeList.h"          // BrnResource::ChallengeList::GetChallengeCount/GetChallengeData
#include "SharedClasses/DataLists/ChallengeListEntry.h"     // BrnResource::ChallengeListEntry::GetNumPlayers/GetOriginalNumPlayers/SetNumPlayers
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h" // CgsDev::DebugComponent::Destruct

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// Destruct -- X360 0x8233A9C0. The tear-down mirror of Construct with the pool refill and the
// back-pointer asserts dropped: scalar/timer/flag resets, the pointers nulled,
// miLastChallengeResetFrame back to -1, the local completion bit store + last-second success
// mask cleared, the billboard list + leaping-car pool re-constructed, ClearPlayerSuccessData,
// the per-slot completion records freed (-1 id / not finalised -- the bit stores are NOT
// touched here, unlike Construct), the cached-skill sweep, the scores-set mask cleared, and
// finally the embedded debug component's back-pointer nulled + the component torn down.
//
// PITFALL 1: the pseudocode's `= 0xFFFFFFFF00000000uLL` stores are ZERO stores (`li r30,0`
// then `std r30`) -- container UnSetAll()/Construct() clears, not sentinel writes.
// PITFALL 2: the pseudocode tail `BaseCollisionGenerator::Destruct(this+0x1004)` is ICF for
// CgsDev::DebugComponent::Destruct on the embedded component.
// ----------------------------------------------------------------------------
void ChallengeManager::Destruct()
{
    mfLeapCarsValidTimer = -1.0f;                 // X360 stfs -1.0,+0x110
    mbUpdateLeaptCars = false;                    // X360 stb 0,+0x118
    mbChallengeTimerRunning = false;              // X360 stb 0,+0xE1C
    mbConvoyTimerRunning    = false;              // X360 stb 0,+0xE24
    mbResultsTimerRunning   = false;              // X360 stb 0,+0xE2C
    mfChallengeTimer = 0.0f;                      // X360 stfs 0.0,+0xE18
    mbRemoteStartPending = false;                 // X360 stb 0,+0xE35
    mfConvoyTimer = 0.0f;                         // X360 stfs 0.0,+0xE20
    mbRemoteTriggerPending = false;               // X360 stb 0,+0xE36
    mfResultsTimer = 0.0f;                        // X360 stfs 0.0,+0xE28
    mbRemoteEndPending = false;                   // X360 stb 0,+0xE37
    mbChallengeRequiresLeapCars = false;          // X360 stb 0,+0xE34
    mpCurrentChallenge = 0;                       // X360 stw 0,+0xE0C
    miCurrentChallengeAction = 0;                 // X360 stw 0,+0xE10
    miCurrentArbitrationIndex = 0;                // X360 stw 0,+0xE14
    miLastChallengeResetFrame = -1;               // X360 stw -1,+0x1000
    mpProgression = 0;                            // X360 stw 0,+0xE44
    mpGameStateModule = 0;                        // X360 stw 0,+0xE48
    mpModeManager = 0;                            // X360 stw 0,+0xE4C
    miNumCarsLeapt = 0;                           // X360 stw 0,+0x114

    mLocalChallengeCompletionData.UnSetAll();     // X360 loop base +0xD00: 32 std of 0 (PITFALL 1)
    maBillboardsCollected.Construct();            // X360 stw 0,+0x3A0 (miCount = 0)
    mLastSecondSuccessStatus.UnSetAll();          // X360 std 0,+0xE00 (PITFALL 1)
    mPotentiallyLeaptCars.Construct();            // X360 std 0,+0x100 (occupancy only; PITFALL 1)

    ClearPlayerSuccessData();

    // Per remote-player completion record: free the slot (-1) + clear the finalised flag. The
    // bit store is deliberately left as-is here (X360 loop base +0x6C8, stride 0x108:
    // stw -1 @+0x100, stb 0 @+0x104 -- no queue clear).
    for (s32 liSlot = 0; liSlot < KI_MAX_REMOTE_PLAYERS; ++liSlot)
    {
        maChallengeCompletionData[liSlot].mNetworkPlayerID = -1;
        maChallengeCompletionData[liSlot].mbFinalised      = false;
    }

    // Cached per-skill location-enter values (X360 loop base +0xF34, 38 stfs; the post-increment
    // operator carries the "leEnumIndex <= E_FREEBURN_SKILL_COUNT" assert, BrnChallengeManager.h:113).
    for (EFreeburnSkill leSkill = E_FREEBURN_SKILL_START;
         static_cast<s32>(leSkill) < KI_FREEBURN_SKILL_COUNT_X360;
         leSkill++)
    {
        mafCachedActiveSkillValueOnLocationEnter[leSkill] = 0.0f;
    }

    mScoresSetThisFrameBitArray.UnSetAll();       // X360 std 0,+0xFD0 (PITFALL 1)

    // Embedded debug component tear-down (X360 stw 0,+0x1010 then the ICF'd DebugComponent::Destruct).
    mChallengeManagerDebugComponent.mpChallengeManager = 0;   // X360 stw 0,+0x1010
    mChallengeManagerDebugComponent.Destruct();               // X360 ICF tail (PITFALL 2)
}

// ----------------------------------------------------------------------------
// Prepare -- X360 0x8233AAF8. Caches the freeburn challenge list, rebuilds the per-player-count
// histogram (one bucket per challenge, keyed by the entry's num-players), and refills the
// potentially-leapt-cars pool. Returns true.
// ----------------------------------------------------------------------------
bool ChallengeManager::Prepare(const BrnResource::ChallengeList* lpFreeburnChallengeList)
{
    CGS_ASSERT(lpFreeburnChallengeList != 0, "lpFreeburnChallengeList");
    mpFreeburnChallengeList = lpFreeburnChallengeList;   // X360 stw,+0xE38

    for (s32 liPlayerCount = 0; liPlayerCount < KI_MAX_CHALLENGE_PLAYERS; ++liPlayerCount)
    {
        maChallengePlayerCounts[liPlayerCount] = 0;      // X360 loop base +0xFDC, 8 stw
    }

    for (s32 liChallenge = 0; liChallenge < mpFreeburnChallengeList->GetChallengeCount(); ++liChallenge)
    {
        const BrnResource::ChallengeListEntry* lpChallenge = mpFreeburnChallengeList->GetChallengeData(liChallenge);
        CGS_ASSERT(lpChallenge != NULL, "lpChallengeListEntry != NULL");
        // X360: index == ((byte0xD3 & 0xF) + 0x3F6) word -> maChallengePlayerCounts[GetNumPlayers()-1].
        ++maChallengePlayerCounts[lpChallenge->GetNumPlayers() - 1];
    }

    mPotentiallyLeaptCars.Clear();   // X360 free-queue refill @+0xE0..+0x100
    return true;
}

// ----------------------------------------------------------------------------
// UnHackAllChallenges -- X360 0x82333040. Undoes HackAllChallenges: for every challenge in the
// list, restore the active num-players (byte +0xD3 low nibble) from the backup HackAllChallenges
// stashed in the high nibble. The range asserts ("liNumPlayers >= 1" / "<= KI_MAX_PLAYERS",
// ChallengeListEntry.h:888/890) live inside the entry's GetOriginalNumPlayers accessor.
// ----------------------------------------------------------------------------
void ChallengeManager::UnHackAllChallenges()
{
    for (s32 liChallenge = 0; liChallenge < mpFreeburnChallengeList->GetChallengeCount(); ++liChallenge)
    {
        BrnResource::ChallengeListEntry* lpChallenge =
            const_cast<BrnResource::ChallengeListEntry*>(mpFreeburnChallengeList->GetChallengeData(liChallenge));
        // X360: newByte = (byte0xD3 & 0xF0) | (byte0xD3 >> 4) -> SetNumPlayers(GetOriginalNumPlayers()).
        lpChallenge->SetNumPlayers(lpChallenge->GetOriginalNumPlayers());
    }
}

}   // namespace BrnGameState
