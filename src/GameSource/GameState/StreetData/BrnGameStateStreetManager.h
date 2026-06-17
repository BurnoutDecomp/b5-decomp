#pragma once

// ---------------------------------------------------------------------------
// GameSource/GameState/StreetData/BrnGameStateStreetManager.h
//   (canonical home for BrnGameState::StreetManager)
//
// MINIMAL-COHERENT SLICE. The full BrnGameState::StreetManager (DecFIGS DWARF
// GameSource/GameState/StreetData/BrnGameStateStreetManager.h, ~70 methods +
// the +0xE08 player / +0 net challenge tables) is uncommitted: its complete
// member layout drags in BrnNetwork / BrnAI / ResourcePtr / GameStateModuleIO
// and dozens of other not-yet-committed types. This header is the single
// PROGRAM-WIDE home for the type so far as the currently-recovered callers
// touch it -- it replaces the former file-local `struct StreetManager` shim
// that lived inside BrnRoadRulesManager.cpp (which caused an ODR redefinition
// once a second TU needed the type). All members are deferred (opaque type);
// only the out-of-line accessors the recovered callers invoke are declared.
//
// Declared `struct` to agree with the DWARF tag (BrnGameStateStreetManager.h:199)
// and the forward declaration in BrnRoadRulesManager.h, avoiding a future MSVC
// C4099 struct/class tag mismatch.
//
// Callers covered by this slice:
//   - BrnRoadRulesManager::GetCurrentRoadID            -> GetStreetData()
//   - BrnGameState::RoadRulesDebugComponent::RenderHUD -> GetStreetData(),
//       GetPlayerChallengeData(), GetNetChallengeData()
// ---------------------------------------------------------------------------

#include "types.hpp"   // int32_t (RoadIndex)

namespace BrnStreetData
{
    struct StreetData;                 // const StreetData* GetStreetData();   (DWARF :326)
    struct ChallengeData;              // Player-column high-score record
    class  ChallengeHighScoreEntry;    // Net-column online high-score record
    // (BrnStreetData::RoadIndex == int32_t in the committed BrnStreetData.h; the
    //  debug accessors below take a plain int32_t row index to avoid re-declaring
    //  that typedef in this forward-only header.)
}

namespace BrnGameState
{
    // X360-out-of-line StreetManager surface used by the currently-recovered
    // callers. The type is otherwise opaque here (no member layout): every use
    // is by pointer, and the dereferencing TUs include the committed
    // BrnStreetData headers for the returned record types.
    struct StreetManager
    {
        // DWARF BrnGameStateStreetManager.h:326. X360: returns
        // mpStreetData.operator->() (ResourcePtr<StreetData>::operator->,
        // out-of-line @ 0x82324E60). Used by RoadRulesManager::GetCurrentRoadID
        // and the road-rules debug HUD.
        const BrnStreetData::StreetData* GetStreetData();

        // ---- PROVISIONAL debug accessors (FLAGGED) -------------------------
        // The road-rules debug HUD (RoadRulesDebugComponent::RenderHUD @ 0x82335350)
        // reads the local-player and online high-score tables that live INSIDE this
        // class. The X360 reaches them at fixed StreetManager offsets:
        //   player table:  base StreetManager + 0xE08, stride 0x28 (40)  -- valid
        //                  bit at +0xE08 (read as u64 & 1), score at +0xE10.
        //   net    table:  base StreetManager + 0,     stride 0x38 (56)  -- valid
        //                  bit at +8 (read as u64 & 1); == ChallengeHighScoreEntry.
        // These two accessors model that access for the recovered caller. CAVEAT:
        // the +0xE08/+0xE10 player table is NOT byte-for-byte a BrnStreetData::ChallengeData
        // (ChallengeData has mValidScores@+8 / mScoreList.maScores[0]@+16, an 8-byte
        // stride between valid bit and score; the X360 player table packs them 8 bytes
        // apart at +0xE08/+0xE10 over a 40-byte stride that is the on-disk
        // ChallengePlayerScoreEntry, not ChallengeData). They are presented through the
        // committed record types for the HUD's read-only valid-bit + first-score use;
        // do NOT treat them as the committed StreetManager member layout. They will be
        // replaced by the real member tables when the full StreetManager is committed.
        const BrnStreetData::ChallengeData*           GetPlayerChallengeData( int32_t liRoad );
        const BrnStreetData::ChallengeHighScoreEntry* GetNetChallengeData( int32_t liRoad );
    };
}
