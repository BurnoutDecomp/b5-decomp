// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Scoring/BrnCarData.cpp
// ============================================================================
// Out-of-line method BODIES for BrnGameState::CarData (the per-car gameplay /
// score record embedded by value in ScoringSystem::maCarData[8]). The layout
// + every signature are owned by the keystone BrnScoringSystem.h; this TU only
// bodies the five addressed methods.
//
// SHAPE = DecFIGS DWARF (BrnScoringSystem.h keystone); BODY = X360 pseudocode/asm
// (overrides DWARF on conflict). Members accessed BY NAME against the keystone
// layout -- no offset casts.
//
//   Construct             0x823270F0  init the record (clear score data + seed fields)
//   Prepare               0x82327158  per-event prep: alloc the 64-entry road-rule table
//   GetRoadRulesScores    0x8231E0D0  copy one challenge's high-score entry OUT
//   SetRoadRulesScores    0x8231E188  copy one challenge's high-score entry IN
//   ResetRoadRulesScores  0x8231E218  re-Construct every entry in the road-rule table
// ----------------------------------------------------------------------------

#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h" // BrnGameState::CarData keystone

// The road-rule table element type + its Copy/Construct. mpaOnlineGameRoadRuleHighScores
// points at a contiguous run of ChallengeHighScoreEntry (X360 stride 56 bytes); the trio
// touches them by name through this header.
#include "GameSource/GameState/StreetData/BrnChallengeHighScoreEntry.h"

// CgsMemory::HeapMalloc::Malloc + GetAllocator()->ValidateHeap, the network-heap allocator
// CarData::Prepare services its road-rule table from.
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"

// CGS_ASSERT (the house assert machinery the X360 Begin/Fire/End triples map to).
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnGameState
{
    // ------------------------------------------------------------------------
    // CarData::Construct  (X360 0x823270F0)
    // ------------------------------------------------------------------------
    // Reset the record to its empty/unassigned state. Chains the embedded score
    // record's clear first (CarScoreData::ClearData via the value member), then seeds
    // every scalar to the X360 store map:
    //   mCarId                 = 0          (std 0 @0x128, the full 64-bit CgsID)
    //   miCumulativePoints     = 0          (stw 0 @0x130)
    //   miRoundDisconnectedIn  = -1         (stw -1 @0x134, "not disconnected")
    //   mfCurrentDriftDistance = 0.0f       (stfs 0.0 @0x138)
    //   mePlayerStatus         = PLAYING    (stw 0 @0x13C)
    //   mePlayerTeam           = 0          (stw 0 @0x140)
    //   meRaceCarIndex         = COUNT      (stw 8 @0x144, the empty-slot sentinel)
    //   mNetworkPlayerID       = -1         (stw -1 @0x148, invalid network id)
    //   mbIsEliminated         = false      (stb 0 @0x154)
    // NOTE on the Hex-Rays delta: the pseudocode renders the 64-bit `std 0 @0x128` as
    // `*(a1+296) = 0xFFFFFFFF00000000uLL`, but the asm loads r11=0 and stores a 64-bit
    // ZERO, so mCarId is cleared to 0 (the -1 words it conflates are the separate
    // miRoundDisconnectedIn/mNetworkPlayerID stw -1's). The asm is authoritative.
    void CarData::Construct()
    {
        GetScoreData()->ClearData();

        mCarId                 = 0;
        miCumulativePoints     = 0;
        miRoundDisconnectedIn  = -1;
        mfCurrentDriftDistance = 0.0f;
        mePlayerStatus         = E_PLAYER_STATUS_PLAYING;
        mePlayerTeam           = static_cast<GameStateModuleIO::EPlayerTeam>(0);
        meRaceCarIndex         = E_ACTIVE_RACE_CAR_INDEX_COUNT;
        mNetworkPlayerID       = -1;
        mbIsEliminated         = false;
    }

    // ------------------------------------------------------------------------
    // CarData::Prepare  (X360 0x82327158)
    // ------------------------------------------------------------------------
    // Per-event prep: adopt the network heap, allocate the per-car road-rule
    // high-score table from it, then seed every entry.
    //   * mpNetworkHeapMalloc = lpNetworkHeapMalloc                       (stw @0x150)
    //   * validate the heap is intact BEFORE the alloc                    (ValidateHeap full)
    //   * mpaOnlineGameRoadRuleHighScores =
    //         mpNetworkHeapMalloc->Malloc(3584, 4)                        (stw @0x14C)
    //         3584 == KI_MAX_CHALLENGES(64) * sizeof(ChallengeHighScoreEntry)(56);
    //         alignment 4 == HeapMalloc::KI_DEFAULT_ALIGNMENT.
    //   * validate the heap again AFTER the alloc, and assert the table is non-NULL
    //   * ResetRoadRulesScores() Construct-initialises all 64 entries
    // Returns true (the X360 `li r3, 1`).
    bool CarData::Prepare(CgsMemory::HeapMalloc* lpNetworkHeapMalloc)
    {
        mpNetworkHeapMalloc = lpNetworkHeapMalloc;

        CGS_ASSERT(mpNetworkHeapMalloc->GetAllocator()->ValidateHeap(
                       EA::Allocator::GeneralAllocator::kHeapValidationLevelFull),
                   "mpNetworkHeapMalloc->GetAllocator()->ValidateHeap(rw::core::GeneralAllocator::kHeapValidationLevelFull)");

        mpaOnlineGameRoadRuleHighScores = static_cast<BrnStreetData::ChallengeHighScoreEntry*>(
            mpNetworkHeapMalloc->Malloc(3584, CgsMemory::HeapMalloc::KI_DEFAULT_ALIGNMENT));

        CGS_ASSERT(mpNetworkHeapMalloc->GetAllocator()->ValidateHeap(
                       EA::Allocator::GeneralAllocator::kHeapValidationLevelFull),
                   "mpNetworkHeapMalloc->GetAllocator()->ValidateHeap(rw::core::GeneralAllocator::kHeapValidationLevelFull)");
        CGS_ASSERT(mpaOnlineGameRoadRuleHighScores, "mpaOnlineGameRoadRuleHighScores");

        ResetRoadRulesScores();
        return true;
    }

    // ------------------------------------------------------------------------
    // CarData::GetRoadRulesScores  (X360 0x8231E0D0)
    // ------------------------------------------------------------------------
    // Copy this car's recorded high-score entry for one challenge OUT into lpEntry.
    // Three leading guards (lpEntry non-NULL; lChallenge in [0, KI_MAX_CHALLENGES);
    // the indexed table slot non-NULL), then ChallengeHighScoreEntry::Copy from the
    // table slot into the caller's entry. The 0x40 bound is KI_MAX_CHALLENGES (== 64);
    // the slot address mpaOnlineGameRoadRuleHighScores[lChallenge] is the X360
    // `*(a1+332) + 56*a2`.
    void CarData::GetRoadRulesScores(BrnNetwork::Road::ChallengeIndex lChallenge,
                                     BrnStreetData::ChallengeHighScoreEntry* lpEntry) const
    {
        CGS_ASSERT(lpEntry, "lpEntry");
        CGS_ASSERT(lChallenge < 0x40 && lChallenge >= 0,
                   "lChallengeIndex < BrnGameState::KI_MAX_CHALLENGES && lChallengeIndex >= 0");

        BrnStreetData::ChallengeHighScoreEntry* lpChallenge =
            &mpaOnlineGameRoadRuleHighScores[lChallenge];
        CGS_ASSERT(lpChallenge, "lpChallenge");

        lpEntry->Copy(lpChallenge);
    }

    // ------------------------------------------------------------------------
    // CarData::SetRoadRulesScores  (X360 0x8231E188)
    // ------------------------------------------------------------------------
    // Copy lpEntry INTO this car's table slot for one challenge. Two leading guards
    // (lpEntry non-NULL; lChallenge in [0, KI_MAX_CHALLENGES)), then
    // ChallengeHighScoreEntry::Copy from lpEntry into the indexed table slot
    // (X360 `Copy(*(a1+332) + 56*a2, a3)` == slot.Copy(lpEntry)).
    void CarData::SetRoadRulesScores(BrnNetwork::Road::ChallengeIndex lChallenge,
                                     BrnStreetData::ChallengeHighScoreEntry* lpEntry)
    {
        CGS_ASSERT(lpEntry, "lpEntry");
        CGS_ASSERT(lChallenge < 0x40 && lChallenge >= 0,
                   "lChallengeIndex < BrnGameState::KI_MAX_CHALLENGES && lChallengeIndex >= 0");

        mpaOnlineGameRoadRuleHighScores[lChallenge].Copy(lpEntry);
    }

    // ------------------------------------------------------------------------
    // CarData::ResetRoadRulesScores  (X360 0x8231E218)
    // ------------------------------------------------------------------------
    // Re-initialise every entry in the road-rule high-score table. The X360 walks the
    // table by byte stride (i += 56 over [0, 3584)) and INLINES each entry's Construct:
    // ChallengeData::Construct(entry), then constructs the two embedded
    // CgsNetwork::PlayerName fields (entry+24, stride 16) to the empty string -- which
    // is exactly ChallengeHighScoreEntry::Construct(). Expressed by name as the
    // per-entry Construct() over the KI_MAX_CHALLENGES (== 64) table.
    void CarData::ResetRoadRulesScores()
    {
        for (s32 liChallenge = 0; liChallenge < 0x40; ++liChallenge)
        {
            mpaOnlineGameRoadRuleHighScores[liChallenge].Construct();
        }
    }
}
