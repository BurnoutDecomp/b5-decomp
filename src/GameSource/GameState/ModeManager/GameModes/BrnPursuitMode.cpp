#include "GameSource/GameState/ModeManager/GameModes/BrnPursuitMode.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnGameState
{
const char* PursuitMode::GetName() const
{
    return "Pursuit";
}

// X360: BrnGameState::PursuitMode::Start (0x823220A0).
//
// Sets up the mutable GameModeParams for the offline Pursuit (road-rage-style) mode from the
// immutable StartGameModeParams and its attached per-rank tuning record. The Hex-Rays output
// operates on the OLD GameModeParams/StartGameModeParams layouts via raw byte pokes; this
// reconstruction restores the committed named accessors (AGENTS.md no-raw-offset rule). The
// third DWARF parameter (ScoringSystem*) is unused by the body and dropped by Hex-Rays.
//
// The X360 build guards the rank pointer with CGS_ASSERT(... != NULL) several times (the baked
// source paths are BrnGameModeParams.h:945/960 for the StartGameModeParams accessor asserts and
// BrnPursuitMode.cpp:53/54 for the local pointer/param asserts). The project discards the baked
// d:\p4 paths/line numbers (captured in this comment only); CGS_ASSERT fills __FILE__/__LINE__.
void PursuitMode::Start(const StartGameModeParams* lpStartGameModeParams,
                        GameModeParams*            lpGameModeParams,
                        ScoringSystem*             /*lpScoringSystem*/)
{
    CGS_ASSERT(lpStartGameModeParams->GetProgressionRankData() != NULL, "mpProgressionRankData != NULL");

    const BrnProgression::ProgressionRankData* lpProgressionRankData =
        lpStartGameModeParams->GetProgressionRankData();

    CGS_ASSERT(lpProgressionRankData != NULL, "lpProgressionRankData != NULL");
    CGS_ASSERT(lpGameModeParams != NULL, "lpGameModeParams != NULL");

    lpGameModeParams->Construct(GameStateModuleIO::E_MODE_PURSUIT);

    // Exactly one rival (the pursued car) in pursuit mode (gap0[0]=1 == miNumRivals).
    lpGameModeParams->SetNumRivals(1);

    // Traffic density = the start params' base density scaled by the rank's pursuit factor
    // (ProgressionRankData::mfTrafficDensityPursuit, rank byte offset 20; DWARF :282/:124).
    lpGameModeParams->SetTrafficDensityScale(
        lpStartGameModeParams->GetTrafficDensity() * lpProgressionRankData->GetTrafficDensityPursuit());

    CGS_ASSERT(lpStartGameModeParams->GetProgressionRankData() != NULL, "mpProgressionRankData != NULL");
    lpGameModeParams->SetProgressionRankAsRatio(lpStartGameModeParams->GetProgressionRankAsRatio());

    // Copy the rank's per-position overtaking-difficulty table into the mode params. The X360
    // pseudocode unrolls this as field_74 + gap7C[0..20] <- rank gap0[44],[52..72] (the index-1
    // entry folded by the optimizer); the logical operation is the rank accessor that writes the
    // whole 8-float array (ProgressionRankData::GetOvertakingDifficulty(out), DWARF :292/:192).
    // mfOvertakingDifficulty is f32[8] and decays to f32*.
    lpProgressionRankData->GetOvertakingDifficulty(lpGameModeParams->mfOvertakingDifficulty);

    // gap144[1788/1792/1796/1800] = 3,3,3,1 -- the default-player/default-AI route-finding styles,
    // the AI speed-selection method, and the A*-distance-function fields set to their fixed pursuit
    // ordinals. These deep GameModeParams enum fields are stored as the *_Stub enums in the committed
    // header (BrnAI enums have no committed home yet); the raw pursuit ordinals 3,3,3,1 map straight
    // onto those storage slots without forking the real BrnAI enums -- matching the committed
    // RaceMode::Start precedent (which leaves its equivalent 8,8,8,2 race defaults to the full
    // GameModeParams TU). INTEGRATOR: set meDefaultPlayerRouteFindingStyle=3, meDefaultAIRouteFindingStyle=3,
    // meAISpeedSelectionMethod=3, meAStarDistanceFunction=1 once those BrnAI enums have a committed home.

    // Tail (X360 gap45[11/15/19] <- gap2D4[44/48/52], interleaved with a muFlags re-store that nets
    // to no flag change): copy the pursued-car global index / pursued-car id / takedown target out of
    // the start params into the mode params, via the committed named accessors. The exact source
    // members are inferred from the StartGameModeParams DWARF order (LOW confidence on the precise
    // field-to-field pairing; source->dest value pairing is preserved). All three are type-matched
    // (EGlobalRaceCarIndex_Stub, CgsID, s32).
    lpGameModeParams->mePursuedCarGlobalIndex = lpStartGameModeParams->GetPursuedCarGlobalIndex();
    lpGameModeParams->mPursuedCarID           = lpStartGameModeParams->GetPursuedCarID();
    lpGameModeParams->miRoadRageThreshold     = lpStartGameModeParams->GetTakedownTarget();
}

// X360 vtable slot 13 (vtbl+52), folded leaf 0x827E2F38 == `li r3,0; blr` at slot 13 of
// PursuitMode's vtable 0x820D0650 (the offline base carries GameMode::ShouldExit 0x82315B80 there
// instead). The pursuit mode never ends itself on the shared idle-exit test.
bool PursuitMode::ShouldExit(const ScoringSystem* lpScoringSystem) const
{
    (void)lpScoringSystem;
    return false;
}
}
