// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Scoring/BrnCarData.cpp
// ============================================================================
// Out-of-line method BODIES for BrnGameState::CarData (the per-car gameplay /
// score record embedded by value in ScoringSystem::maCarData[8]) and for its
// sibling per-slot record RaceCarPositioningData. Both types are homed in the
// keystone BrnScoringSystem.h, both are per-car arrays on ScoringSystem, and both
// are driven by the SAME ScoringSystem per-slot loops (Prepare / Release /
// ClearCumulativeData / ClearData), so their small bodies share this TU rather than
// fork a third one-function file. The layout + every signature stay owned by the
// keystone header.
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
//
// ACCESSOR CLOSURE (2026-08-26) -- three more bodies, all declared-only until now and all
// real link residue of the scoring mount (they are named in
// scratch/stuntrace_scout/datafeed/objs/undef_demangled.txt). None has its own out-of-line
// X360 symbol; each is recovered from the ScoringSystem loop that INLINES it:
//   CarData::Release                    <- ScoringSystem::Release           0x823124A0
//   CarData::ClearCumulativeData        <- ScoringSystem::ClearCumulativeData 0x8231F140
//   RaceCarPositioningData::Construct   <- ScoringSystem::ClearData         0x8232A4A8
// In all three the record base is pinned by the loop stride: ScoringSystem::Prepare
// (0x8232A430) walks maCarData with `addi r30, this, 0x4F00 ; addi r30, r30, 0x158`, so
// maCarData[0] == ScoringSystem+0x4F00 and sizeof(CarData) == 0x158; ScoringSystem::ClearData
// walks maRaceCarPositioningData with base ScoringSystem+0x59C0 and stride 0x18. Every
// `*(this + N)` in the three loops therefore resolves to a named member at N - base, and each
// one lands exactly on the keystone's declared member run.
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

// EActiveRaceCarIndex (the -1 / COUNT sentinels RaceCarPositioningData::Construct seeds).
#include "GameSource/BurnoutConstants.h"

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

        // Console literal 3584 == 64 * console-sizeof(56); the HOST entry is wider (widened
        // pointers/PlayerName layout), so the size is computed -- the console value stays in
        // the banner. (The 3584 literal here was the console-size-feeds-host-copy killer,
        // latent until the first Prepare call: 64 host entries would overrun a 3584 block.)
        mpaOnlineGameRoadRuleHighScores = static_cast<BrnStreetData::ChallengeHighScoreEntry*>(
            mpNetworkHeapMalloc->Malloc(
                64u * sizeof(BrnStreetData::ChallengeHighScoreEntry),
                CgsMemory::HeapMalloc::KI_DEFAULT_ALIGNMENT));

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
        // [FLAG PC bring-up guard, 2026-08-26] On the console this runs only after
        // CarData::Prepare allocated the table from the network heap; on this build the
        // ScoringSystem Prepare chain is not yet staged, so the table can legitimately be
        // NULL here (Release's own asm carries the same null-tolerance: "skip the free when
        // the table was never allocated"). A null table == road-rule high scores inert;
        // boot-proven crash in the first live ScoringSystem::AddPlayer otherwise.
        // DELETE-WHEN the ScoringSystem/CarData Prepare staging (network heap) lands.
        if (mpaOnlineGameRoadRuleHighScores == 0)
        {
            return;
        }
        for (s32 liChallenge = 0; liChallenge < 0x40; ++liChallenge)
        {
            mpaOnlineGameRoadRuleHighScores[liChallenge].Construct();
        }
    }

    // ------------------------------------------------------------------------
    // CarData::Release  (inlined by ScoringSystem::Release, X360 0x823124A0)
    // ------------------------------------------------------------------------
    // Hand the road-rule high-score table back to the network heap and drop both pointers.
    // The X360 loop body, with r31 walking maCarData[i] + 0x14C (== 0x504C - 0x4F00):
    //     lwz  r4, 0(r31)            ; mpaOnlineGameRoadRuleHighScores  (+0x14C)
    //     cmplwi r4, 0 ; beq  ->     ; skip the free when the table was never allocated
    //     lwz  r3, 4(r31)            ; mpNetworkHeapMalloc              (+0x150)
    //     bl   CgsMemory::HeapMalloc::Free   ; heap->Free(table)   (heap is the `this` in r3)
    //     stw  r29(=0), 0(r31)       ; mpaOnlineGameRoadRuleHighScores = NULL  (INSIDE the if)
    //     stw  r29(=0), 4(r31)       ; mpNetworkHeapMalloc            = NULL  (UNCONDITIONAL)
    // Note the asymmetry, which is faithful and deliberate below: the table pointer is nulled
    // only on the path that freed it, the heap pointer is nulled every time. Returns true
    // (ScoringSystem::Release's own `li r3, 1` is its own return, but each per-car Release is
    // `bool` in the DWARF and the X360 discards the value -- reported as true, matching the
    // sibling Prepare).
    bool CarData::Release()
    {
        if (mpaOnlineGameRoadRuleHighScores != NULL)
        {
            mpNetworkHeapMalloc->Free(mpaOnlineGameRoadRuleHighScores);   // heap->Free(table)
            mpaOnlineGameRoadRuleHighScores = NULL;                       // +0x14C (gated)
        }

        mpNetworkHeapMalloc = NULL;                                       // +0x150 (unconditional)
        return true;
    }

    // ------------------------------------------------------------------------
    // CarData::ClearCumulativeData  (inlined by ScoringSystem::ClearCumulativeData, 0x8231F140)
    // ------------------------------------------------------------------------
    // Reset only the cross-round running totals; everything else on the record survives.
    // The X360 fully unrolls the eight-slot loop into a flat store pair per slot:
    //     stw r9(=0),  0x5030(this) ; stw r10(=-1), 0x5034(this)   ; slot 0
    //     stw r9,      0x5188(this) ; stw r10,      0x518C(this)   ; slot 1  (+0x158)
    //     ... eight pairs, stride 0x158 ...
    // 0x5030 - 0x4F00 == +0x130 == miCumulativePoints and 0x5034 == +0x134 ==
    // miRoundDisconnectedIn -- the same two offsets CarData::Construct above stores 0 / -1 to.
    // -1 is the "never disconnected" sentinel (Construct uses the identical pair), so this is
    // NOT a memset: the other cumulative-looking members (mfCurrentDriftDistance @+0x138,
    // the embedded CarScoreData @+0) are deliberately left alone by the console.
    void CarData::ClearCumulativeData()
    {
        miCumulativePoints    = 0;    // +0x130
        miRoundDisconnectedIn = -1;   // +0x134 ("not disconnected")
    }

    // ------------------------------------------------------------------------
    // RaceCarPositioningData::Construct  (inlined by ScoringSystem::ClearData, 0x8232A4A8)
    // ------------------------------------------------------------------------
    // Seed one slot of the transient race-position scratch to "no car, no measurement yet".
    // The X360 per-slot block, with r28 walking maRaceCarPositioningData[i] + 4 (base
    // ScoringSystem+0x59C0, r28 initialised to +0x59C4) and r25 = 8, r27 = -1, r30 = 0,
    // f0 = flt_82CDB7D0:
    //     stw  r27, -4(r28)    ; +0x00 meActiveRaceCarIndex      := -1
    //     stfs f0,   0(r28)    ; +0x04 mfDistanceToNextCheckpoint := flt_82CDB7D0
    //     stfs f0,   4(r28)    ; +0x08 mfDistanceToFinish         := flt_82CDB7D0
    //     stw  r27,  8(r28)    ; +0x0C miCurrentCheckpoint        := -1
    //     stw  r25, 0xC(r28)   ; +0x10 miFinishPosition           := 8
    //     stb  r30, 0x10(r28)  ; +0x14 mbDisconnected             := false
    //     addi r28, r28, 0x18  ; stride 24 == the six declared members
    // The store order + offsets match the keystone's declared member run exactly.
    //
    // flt_82CDB7D0 is the tree's already-named KF_INVALID_RACE_DISTANCE sentinel: FLT_MAX.
    // Its magnitude is settled twice over -- BrnScoringSystem_Lookup.cpp recovered it from the
    // PS3 DecFIGS ScoringSystem::ClearData (0x1E364C), and a direct big-endian read of the
    // decrypted ARTIST basefile at VA 0x82CDB7D0 gives 0x7F7FFFFF == 3.4028235e38. Seeding the
    // distances with the largest finite float is what makes the first real measurement always
    // win the nearest-car comparisons.
    void RaceCarPositioningData::Construct()
    {
        const f32 KF_INVALID_RACE_DISTANCE = 3.4028235e38f;   // flt_82CDB7D0 == FLT_MAX

        meActiveRaceCarIndex       = E_ACTIVE_RACE_CAR_INDEX_INVALID;   // +0x00  (-1)
        mfDistanceToNextCheckpoint = KF_INVALID_RACE_DISTANCE;          // +0x04
        mfDistanceToFinish         = KF_INVALID_RACE_DISTANCE;          // +0x08
        miCurrentCheckpoint        = -1;                                // +0x0C
        miFinishPosition           = E_ACTIVE_RACE_CAR_INDEX_COUNT;     // +0x10  (8)
        mbDisconnected             = false;                             // +0x14
    }
}
