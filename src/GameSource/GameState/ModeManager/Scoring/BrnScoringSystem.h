#pragma once

// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h
// ============================================================================
// Minimal sized-stub home for BrnGameState::ScoringSystem + BrnGameState::CarData.
//
// NEITHER type had a committed home before this slice (only `class ScoringSystem;`
// forward-declares scattered across the GameState headers). The online-mode scorers
// (OnlineRaceModeScoring / OnlineBurningHomeRunModeScoring) reach the per-car scoring
// records exclusively through ScoringSystem::GetCarData(EActiveRaceCarIndex) -> CarData*,
// then CarData::GetScoreData() -> CarScoreData*, plus CarData's race-car-index / network-id
// getters. Those are the only ScoringSystem/CarData methods this scoring cluster needs.
//
// SHAPE is DWARF-authoritative (references/DecFIGS/dwarfdump/.../BrnScoringSystem.h):
//   - CarData embeds `CarScoreData mCarScoreData` at offset 0 (BrnScoringSystem.h:268), so a
//     CarData* and the CarScoreData* it yields via GetScoreData() share the same address.
//     CarData stride is 344 (X360 GetCarData @ 0x8231DC18 multiplies the slot by 0x158).
//   - ScoringSystem holds `CarData maCarData[8]` (BrnScoringSystem.h:1243); GetCarData returns
//     &maCarData[match] (X360 base + 0x4F00 + 344*slot).
//
// Both types are declared with a NOMINAL sized-reserved storage member so they are usable
// BY POINTER (which is all the scorers need); their full member layouts + the method BODIES
// land with BrnScoringSystem's own TU. Single owner: grow this slice, do not fork.

#include "types.hpp"
#include "GameSource/BurnoutConstants.h"                 // EActiveRaceCarIndex
#include "GameSource/GameState/BrnGameStateSharedIO.h"   // GameStateModuleIO::CarScoreData, EPlayerScoringIndex
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h" // BrnNetwork::NetworkPlayerID

namespace BrnGameState
{
    // Per-car scoring record wrapper. DWARF home BrnScoringSystem.h:123 (sizeof 344). The embedded
    // CarScoreData at offset 0 carries the actual per-car scoring fields; CarData adds the car id,
    // cumulative points, team/status and the race-car-index / network-player-id the scorers read.
    //
    // NOMINAL: only the accessors the online scorers call are declared (declare-only -- bodies live
    // in BrnScoringSystem's TU); the remaining ~40 CarData methods + the full field layout land there.
    class CarData
    {
    public:
        // DWARF BrnScoringSystem.h:151 / :154. The embedded per-car score record (offset 0).
        GameStateModuleIO::CarScoreData* GetScoreData();              // 0x... (declare-only)
        const GameStateModuleIO::CarScoreData* GetScoreData() const;  // 0x... (declare-only)

        // DWARF BrnScoringSystem.h:178. The slot this car occupies (CarData field @ +0x144 in the
        // X360 record -- past sizeof(CarScoreData), which is why the scorers read it off CarData).
        EActiveRaceCarIndex GetActiveRaceCarIndex() const;            // declare-only

        // DWARF BrnScoringSystem.h:181. The network player id (CarData field @ +0x148). DWARF spells
        // the param/return RoadRulesRecvData::NetworkPlayerID; that resolves to top-level
        // BrnNetwork::NetworkPlayerID (== s32) per the committed network home.
        BrnNetwork::NetworkPlayerID GetNetworkPlayerID() const;       // declare-only

    private:
        // NOMINAL reserved storage so sizeof(CarData) is non-trivial and the type is usable by
        // pointer. The real 344-byte layout (CarScoreData mCarScoreData; CgsID mCarId; ... ;
        // EActiveRaceCarIndex meRaceCarIndex; NetworkPlayerID mNetworkPlayerID; ...) lands with the
        // BrnScoringSystem TU. NOT byte-verified here.
        u8 maReserved[344];
    };

    // The scoring system. DWARF home BrnScoringSystem.h:306. Owns maCarData[8] plus a large amount
    // of per-mode scoring state. NOMINAL sized-stub: only the per-car lookups the online scorers use
    // are declared (declare-only). The full layout + bodies land with BrnScoringSystem's own TU.
    class ScoringSystem
    {
    public:
        // X360 0x8231DC18 (.h:1061) / const twin 0x8231DCD0 (.h:1057). Returns &maCarData[slot] whose
        // meRaceCarIndex matches, or NULL when no car occupies the slot.
        CarData* GetCarData(EActiveRaceCarIndex leActiveRaceCarIndex);             // 0x8231DC18
        const CarData* GetCarData(EActiveRaceCarIndex leActiveRaceCarIndex) const; // 0x8231DCD0

        // X360 0x82310E30 (.h:1073) / const overload (.h:1077). Returns &maCarData[index] by player
        // scoring index (no match search -- direct index).
        CarData* GetCarDataFromPlayerScoringIndex(GameStateModuleIO::EPlayerScoringIndex lePlayerScoringIndex);             // 0x82310E30
        const CarData* GetCarDataFromPlayerScoringIndex(GameStateModuleIO::EPlayerScoringIndex lePlayerScoringIndex) const; // declare-only

    private:
        // NOMINAL reserved storage so the type is usable by pointer. The real layout (timers, the
        // sub-scorers, maCarData[8] @ +0x4F00, positioning data, ...) lands with the BrnScoringSystem
        // TU. NOT byte-verified here; sized generously to cover the maCarData array region the X360
        // GetCarData indexes into (base + 0x4F00 + 344*8).
        u8 maReserved[0x4F00 + 344 * 8];
    };
}
