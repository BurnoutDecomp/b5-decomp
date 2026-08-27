#ifndef BRN_PROGRESSION_RANK_DATA_H
#define BRN_PROGRESSION_RANK_DATA_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // CgsID (mFreeCarForRankUpID)

// =============================================================================
// BrnProgressionRankData.h  (OWNING HEADER for BrnProgression::ProgressionRankData)
//
// The per-rank tuning record of PROGRESSION.DAT -- one 112-byte block per progression rank,
// six of them in the retail bundle. Everything that scales with the player's career rank is
// here: per-mode traffic densities, shunt strengths, the road-rage ladder, the per-position
// overtaking-difficulty table, and the four "wins needed to rank up" thresholds.
//
// THIS FILE SUPERSEDES THE STAND-IN. Until 2026-08-26 the tree's only ProgressionRankData was
// a MEMBER-LESS, BODY-LESS class in GameSource/GameState/ModeManager/GameModes/
// BrnGameModeParams.h -- eleven declared accessors over no layout at all, which is exactly why
// eleven of the 63 unresolved externals of the wave-B event-core mount were its accessors. That
// stand-in is retired; BrnGameModeParams.h now #includes this header (ODR -- one owner), and
// BrnProgressionData.h's `class ProgressionRankData;` forward declaration resolves here.
//
// -----------------------------------------------------------------------------------------
// WHY THE LAYOUT BELOW IS THE LAYOUT (four independent witnesses, all checked this pass)
// -----------------------------------------------------------------------------------------
// (1) DWARF MEMBER ORDER. references/DecFIGS/dwarfdump/SharedClasses/Progression/
//     BrnProgressionRankData.h lists the members in source order, lines :277..:315. The record
//     is a flat scalar run (no pointers), so the declaration order lands every member at its
//     console byte offset on the host as well as on the console. Each member below carries its
//     DWARF source line.
//
// (2) X360 CONSUMER LOADS. Every offset the mounted tree actually reads is pinned by an
//     instruction, not by counting:
//       +0x00  RaceMode::Start        @0x82330130 `lfs f31, 0(r24)`     mfTrafficDensityRace
//       +0x14  PursuitMode::Start                 (wave-A note, "rank byte offset 20")
//       +0x24  RaceMode::Start        @0x82330184 `lfs f31, 0x24(r24)`  mfLargeVehicleProbability
//       +0x2C  PursuitMode::Start                 (wave-A note, "byte +44", 8 floats copied out)
//       +0x50  ModeManager::GetRoadRageTakedownTarget @0x8232765C `lhz r3, 0x50(r30)` (x3)
//       +0x56  ModeManager::SetupGameMode @0x8234B58C `lhz r11, 0x56(rank)`
//       +0x5C  RaceMode::Start        @0x8233022C `lbz r25, 0x5C(r24)`  muRaceRivalsNumber
//       +0x60..+0x63  ProgressionManager::GetRankThresholdForEvent @0x82370260's jump table:
//              `lbz r3,0x60` (E_MODE_OFFLINE_RACE) / `0x61` (E_MODE_STUNT_ATTACK) /
//              `0x62` (E_MODE_ROAD_RAGE) / `0x63` (E_MODE_MARKED_MAN), i.e. the DWARF's own
//              Race/Stunt/RoadRage/MarkedMan declaration order, each byte on the mode its name
//              says. Two independent orderings agreeing is what makes that mapping safe.
//     The 112-byte STRIDE is likewise an instruction, not an inference: ProgressionData::
//     GetProgressionRankData @0x82311790 and GetRankThresholdForEvent @0x823275F4 both do
//     `mulli r11, r31, 0x70`.
//
// (3) THE TRANSCODER THAT WROTE THE SHIPPED FILE. tools/assets/bundles/progression_transcode.py
//     ports the retail X360 bundle with SIZEOF['RANK'] == 112 and
//         SPEC_RANK = [(0x00, 4, 19),   # 10 f32 + muId + f32[8] maOvertakingDifficulty
//                      (0x4C, 2, 8),    # mu16MedalThreshold .. muRoadRageTriggerExtension
//                      # 0x5C..0x63 = 8 x u8, 0x64..0x67 = pad
//                      (0x68, 8, 1)]    # CgsID mFreeCarForRankUpID
//     -- byte-for-byte identical to the member list below, and its --verify contest reproduces
//     EA's own little-endian port over the whole ~19.4 KB payload.
//
// (4) THE SHIPPED DATA READS AS SENSE. Dumping build/game/PROGRESSION.DAT's six records
//     THROUGH this exact layout (2026-08-26) gives a monotonic career ladder in every field:
//       muId                         0x80C31, 0x80C32 ... 0x80C36 (consecutive)
//       mfTrafficDensityRace         0.50 0.70 0.80 0.90 0.95 1.00
//       mu16MedalThresholdToNextRank 2 7 15 26 40 120
//       muRoadRageTakedownTarget     3 6 10 15 20 30    muRoadRageTime 180..330 (30 s steps)
//       muRaceRivalsNumber           7 at every rank (the 7 rivals of an 8-car Paradise race)
//       muNumWinsToRankUpRace        0 4 10 18 35 70    ...Stunt 0 4 8 12 17 22
//       the +0x64 pad                0x00000000 in all six records
//       mFreeCarForRankUpID          0 at rank 0, a real CgsID at ranks 1..5
//     A one-slot offset error anywhere in the record would scramble every one of those runs.
//     (Repro: scratch/stuntrace_waveb/tmp/dump_ranks.py.)
//
// -----------------------------------------------------------------------------------------
// EVERY ACCESSOR IS INLINE ON PURPOSE -- these are not stubs.
// -----------------------------------------------------------------------------------------
// The X360 ledger (scratch/func_index.tsv) attests NO standalone symbol for any
// ProgressionRankData method: the record's whole API was header-inline on the console, which is
// exactly what its consumers' asm shows -- RaceMode::Start reads +0x00 / +0x24 / +0x5C with
// bare `lfs` / `lbz` and no `bl` anywhere near them. So the faithful reconstruction is an
// inline body over the named member, on the same "AGENTS.md: DWARF supplies names, the X360
// ledger decides what exists" precedent already used by RaceEventData::GetCheckpointCount and
// CheckpointData::GetLandmarkId in the sibling BrnRaceEventData.h. There is no
// BrnProgressionRankData.cpp and no mount request for one.
//
// SCOPE. The DWARF declares ~60 methods on this record (the full Set* mutator surface plus a
// dozen more getters). Only the ELEVEN the mounted tree calls are given bodies here; the rest
// are deliberately absent rather than fabricated. Growing one is a two-line edit over a member
// that is already named and already at its proven offset -- grow this owner, do not fork it.
//
// TAG. The DWARF spells the type `struct BrnProgression::ProgressionRankData`. It is written
// `class` + an explicit `public:` here to match the forward declaration BrnProgressionData.h:41
// already committed (`class ProgressionRankData;`) and so avoid MSVC C4099 -- the same
// reconciliation BrnProgressionManager.h records for its own type.
// =============================================================================

namespace BrnProgression
{

class ProgressionRankData
{
public:
    // Sizes maOvertakingDifficulty. Eight entries == the eight race positions the per-position
    // AI overtaking-difficulty table is authored for; PursuitMode::Start copies all eight into
    // GameModeParams::mfOvertakingDifficulty (f32[8]).
    static const s32 KI_OVERTAKING_DIFFICULTY_COUNT = 8;

    // ---- The eleven accessors the mounted tree calls (DWARF line in the trailing comment) ----

    // DWARF :112. Race traffic-density scale. RaceMode::Start multiplies the start params'
    // base density by it (`lfs f13, 0(r24)` @0x82330310, then `fmuls`).
    f32 GetTrafficDensityRace() const { return mfTrafficDensityRace; }

    // DWARF :124. Pursuit traffic-density scale; PursuitMode::Start's equivalent multiply.
    f32 GetTrafficDensityPursuit() const { return mfTrafficDensityPursuit; }

    // DWARF :76. Probability that a spawned traffic vehicle is a LARGE one at this rank
    // (`lfs f31, 0x24(r24)` @0x82330184). 0.0 at rank 0 -- no trucks in the first hours.
    f32 GetLargeVehicleProbability() const { return mfLargeVehicleProbability; }

    // DWARF :67. Rivals on an offline race grid at this rank. Stored as a BYTE (`lbz`), returned
    // as uint32_t exactly as the DWARF declares it; RaceMode::Start clamps the event's own
    // mu8StartRivalCount against this value (`cmplw` of +0xEE against rank+0x5C @0x82330334).
    u32 GetRaceRivalsNumber() const { return muRaceRivalsNumber; }

    // DWARF :192. Copies the whole per-position overtaking-difficulty table out to the caller's
    // f32[8]. The console open-codes the eight loads/stores at PursuitMode::Start's call site
    // (the optimizer folds index 1), which is the inlining this body reconstructs.
    // The console does NOT bounds-check or null-test lpafOut; reproduced without a guard.
    void GetOvertakingDifficulty(f32* lpafOut) const
    {
        for (s32 liPosition = 0; liPosition < KI_OVERTAKING_DIFFICULTY_COUNT; ++liPosition)
        {
            lpafOut[liPosition] = maOvertakingDifficulty[liPosition];
        }
    }

    // DWARF :91. Takedowns needed to win a road rage at this rank (3/6/10/15/20/30).
    u16 GetRoadRageTakedownTarget() const { return muRoadRageTakedownTarget; }

    // DWARF :294. Medals needed to reach the NEXT rank (the authored series 2/7/15/26/40/120).
    // ProgressionManager::GetPercentageOfEventsCompleted @0x8237B390 divides the player's total
    // win count by it (`lhz r11, 0x4C(r11)` + `extsh` @0x8237B480, so it is read SIGNED there);
    // the X360 has no standalone symbol for the read, hence the inline, same as the siblings.
    u16 GetMedalThresholdToNextRank() const { return mu16MedalThresholdToNextRank; }

    // DWARF :97. Seconds a road-rage time extension is worth at this rank.
    u16 GetRoadRageExtensionTime() const { return muRoadRageExtensionTime; }

    // DWARF :255/:258/:261/:264. The four per-mode rank-up thresholds -- "how many wins at this
    // mode does reaching the NEXT rank take". ProgressionManager::GetRankThresholdForEvent
    // @0x82370260 is the single consumer and its jump table is what pins each byte to its mode
    // (witness (2) above). Read as `lbz` with no `extsb`, so unsigned; the console widens the
    // answer to s32 at the return, which is that method's business and not this one's.
    u8 GetNumWinsToRankUpRace() const      { return muNumWinsToRankUpRace; }
    u8 GetNumWinsToRankUpStunt() const     { return muNumWinsToRankUpStunt; }
    u8 GetNumWinsToRankUpRoadRage() const  { return muNumWinsToRankUpRoadRage; }
    u8 GetNumWinsToRankUpMarkedMan() const { return muNumWinsToRankUpMarkedMan; }

private:
    // ===========================================================================================
    // THE 112-BYTE SERIALISED RECORD. Pointer-free scalar run -> the console byte offsets hold on
    // the x64 host unchanged (unlike ProgressionData / RaceEventData, which carry FixUp-rebased
    // 32-bit table slots). Offsets in the trailing comments are console == host.
    // ===========================================================================================
    f32   mfTrafficDensityRace;          // 0x00 (DWARF :277)
    f32   mfBurningRouteTimeScale;       // 0x04 (DWARF :278)
    f32   mfTrafficDensityBurningRoute;  // 0x08 (DWARF :279)
    f32   mfTrafficDensityRoadRage;      // 0x0C (DWARF :280)
    f32   mfTrafficDensitySurvival;      // 0x10 (DWARF :281)
    f32   mfTrafficDensityPursuit;       // 0x14 (DWARF :282)

    f32   mfShuntStrengthRace;           // 0x18 (DWARF :284)
    f32   mfShuntStrengthRoadRage;       // 0x1C (DWARF :285)
    f32   mfShuntStrengthMarkedMan;      // 0x20 (DWARF :286)

    f32   mfLargeVehicleProbability;     // 0x24 (DWARF :288)

    u32   muId;                          // 0x28 (DWARF :290)  0x80C31 + rank in the retail file

    // 0x2C..0x4B (DWARF :292). Per-race-position AI overtaking difficulty.
    f32   maOvertakingDifficulty[KI_OVERTAKING_DIFFICULTY_COUNT];

    u16   mu16MedalThresholdToNextRank;  // 0x4C (DWARF :294)
    u16   mu16EventThresholdToNextRank;  // 0x4E (DWARF :295)
    u16   muRoadRageTakedownTarget;      // 0x50 (DWARF :296)
    u16   muRoadRageTime;                // 0x52 (DWARF :297)
    u16   muRoadRageTimeExtensions;      // 0x54 (DWARF :298)
    u16   muRoadRageExtensionTime;       // 0x56 (DWARF :299)
    u16   muRoadRageDamageLimit;         // 0x58 (DWARF :300)
    u16   muRoadRageTriggerExtension;    // 0x5A (DWARF :301)

    u8    muRaceRivalsNumber;            // 0x5C (DWARF :305)
    u8    muGauntletRivalsNumber;        // 0x5D (DWARF :306)
    u8    muRoadRageRivalsNumber;        // 0x5E (DWARF :307)
    u8    muNumGiftCars;                 // 0x5F (DWARF :308)

    u8    muNumWinsToRankUpRace;         // 0x60 (DWARF :310)
    u8    muNumWinsToRankUpStunt;        // 0x61 (DWARF :311)
    u8    muNumWinsToRankUpRoadRage;     // 0x62 (DWARF :312)
    u8    muNumWinsToRankUpMarkedMan;    // 0x63 (DWARF :313)

    // 0x64..0x67 -- the alignment gap the 8-byte CgsID below forces. NAMED rather than left to
    // the compiler because progression_transcode.py deliberately keeps it OUT of its byte-swap
    // runs ("the X360 leaves uninitialised junk in those gaps"), and because an unnamed gap is
    // how a future grow-in-place edit silently moves mFreeCarForRankUpID. The shipped file has
    // 0x00000000 here in all six records.
    u32   muPad_64;

    CgsID mFreeCarForRankUpID;           // 0x68 (DWARF :315)  the car this rank-up gifts
};

// The 112-byte (0x70) stride ProgressionData::GetProgressionRankData bakes as `mulli r11,r31,0x70`
// and progression_transcode.py ports the retail rank table at (SIZEOF['RANK'] == 112). This is a
// pointer-free scalar record, so the check is a real check of the console layout rather than a
// host-only tautology: if it fires, the record has been grown or reordered and every shipped rank
// now reads garbage.
static_assert(sizeof(ProgressionRankData) == 112,
              "BrnProgression::ProgressionRankData is a 112-byte (0x70) serialised record");

}

#endif // BRN_PROGRESSION_RANK_DATA_H
