#pragma once

#include "GameSource/GameState/ModeManager/GameModes/BrnOfflineGameMode.h"

namespace BrnGameState
{
// DWARF BrnPursuitMode.h:31 (namespace BrnGameState): the pursuit's intro length. X360: the only load
// in slot 8's body 0x827E24E8 is flt_82001C98, which x360rd reads as 0x3F800000 == 1.0f.
const f32 KF_PURSUIT_INTRO_TIME = 1.0f;

// PursuitMode is a concrete game mode. The base types (OfflineGameMode, GameMode)
// are #included from their own owning headers rather than forked locally. Only
// GetName is owned by this TU; PursuitMode's remaining members/methods belong to
// the BrnPursuitMode.cpp TU.
//
// SLOT 8 LANDED 2026-09-25 (crash parity FX-SCENARIOS): vtable 0x820D0650 slot 8 (0x820D0670) is
// 0x827E24E8, the 1.0f body; until then the mode inherited the GameMode base (6.0 offline).
class PursuitMode : public OfflineGameMode
{
public:
    virtual const char* GetName() const;                               // slot 6, X360 0x827E24F8

    // Slot 8 (vtbl+32). X360 0x827E24E8 -- a body shared by ICF (IDA files it as
    // VehiclePhysics::GetShowtimeDeformationScale): `lis/lfs f1, flt_82001C98 ; blr` == 1.0f,
    // KF_PURSUIT_INTRO_TIME. DWARF BrnPursuitMode.h:80 declares the override.
    virtual f32 GetIntroDurationSeconds() const;

    // X360 0x823220A0. Builds the pursuit GameModeParams from the StartGameModeParams + rank data.
    virtual void Start(const StartGameModeParams* lpStartGameModeParams,
                       GameModeParams* lpGameModeParams,
                       ScoringSystem* lpScoringSystem);                // slot 5

    // Slot 13 (vtbl+52). Folded leaf 0x827E2F38 (`li r3,0; blr`) -- a pursuit never idle-exits.
    // DWARF BrnPursuitMode.h:21 declares this override. ADDED 2026-08-26 with the 26-slot base,
    // because GameMode::ShouldExit is now wired to the real ScoringSystem idle timers.
    virtual bool ShouldExit(const ScoringSystem* lpScoringSystem) const;
};

// ---- VTABLE-BINDING TRIPWIRE (see the explanation in BrnOfflineGameMode.h) ----------------------
static_assert(sizeof(static_cast<const char* (PursuitMode::*)() const>(&PursuitMode::GetName)) != 0,
              "PursuitMode::GetName must bind GameMode vtable slot 6");
static_assert(sizeof(static_cast<f32 (PursuitMode::*)() const>(&PursuitMode::GetIntroDurationSeconds)) != 0,
              "PursuitMode::GetIntroDurationSeconds must bind GameMode vtable slot 8");
static_assert(sizeof(static_cast<void (PursuitMode::*)(const StartGameModeParams*, GameModeParams*, ScoringSystem*)>(&PursuitMode::Start)) != 0,
              "PursuitMode::Start must bind GameMode vtable slot 5");
static_assert(sizeof(static_cast<bool (PursuitMode::*)(const ScoringSystem*) const>(&PursuitMode::ShouldExit)) != 0,
              "PursuitMode::ShouldExit must bind GameMode vtable slot 13");
}
