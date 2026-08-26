#pragma once

#include "types.hpp"
#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineGameMode.h"

namespace BrnGameState
{
// OnlineShowtimeMode is a concrete online game mode. Bases (OnlineGameMode -> GameMode) are
// #included from their owning headers, never forked. GetName / GetIntroDurationSeconds / SendEvent
// are owned by this TU.
class OnlineShowtimeMode : public OnlineGameMode
{
public:
    virtual const char* GetName() const;                               // slot 6,  X360 0x827E25F0
    virtual f32         GetIntroDurationSeconds() const;                // slot 8,  X360 0x827E2600
    virtual void        SendEvent(EGameModeEvent leEvent);              // slot 12, X360 0x82331330

    // X360 0x82322278. Builds the online showtime GameModeParams + copies the per-player network ids.
    virtual void Start(const StartGameModeParams* lpStartGameModeParams,
                       GameModeParams* lpGameModeParams,
                       ScoringSystem* lpScoringSystem);                // slot 5

    // Slot 13 (vtbl+52). Folded leaf 0x827E2F38 (`li r3,0; blr`) at slot 13 of vtable 0x820D0AE8;
    // the GameMode base carries GameMode::ShouldExit 0x82315B80 there. Showtime is a stationary
    // mode, so the shared idle-exit test must never fire. DWARF BrnOnlineShowtimeMode.h:17 declares
    // this override; ADDED 2026-08-26 with the 26-slot base, because GameMode::ShouldExit is now
    // wired to the real ScoringSystem idle timers.
    virtual bool ShouldExit(const ScoringSystem* lpScoringSystem) const;

    // Slot 23 (vtbl+92). Folded leaf 0x827E2F38 (`li r3,0; blr`); the base is 0x82C296C8
    // (`li r3,1`). DWARF BrnOnlineShowtimeMode.h:20 declares this override.
    virtual bool RequiresStreaming() const;

private:
    // X360 GetIntroDurationSeconds returns 0.0002 (a near-zero timed intro); 0.0002f is the
    // float32 the X360 build returns. Named per the project rule to reverse inlined constants.
    static const f32 KF_INTRO_DURATION_SECONDS;
};

// ---- VTABLE-BINDING TRIPWIRE (see the explanation in BrnOfflineGameMode.h) ----------------------
static_assert(sizeof(static_cast<const char* (OnlineShowtimeMode::*)() const>(&OnlineShowtimeMode::GetName)) != 0,
              "OnlineShowtimeMode::GetName must bind GameMode vtable slot 6");
static_assert(sizeof(static_cast<f32 (OnlineShowtimeMode::*)() const>(&OnlineShowtimeMode::GetIntroDurationSeconds)) != 0,
              "OnlineShowtimeMode::GetIntroDurationSeconds must bind GameMode vtable slot 8");
static_assert(sizeof(static_cast<void (OnlineShowtimeMode::*)(EGameModeEvent)>(&OnlineShowtimeMode::SendEvent)) != 0,
              "OnlineShowtimeMode::SendEvent must bind GameMode vtable slot 12");
static_assert(sizeof(static_cast<void (OnlineShowtimeMode::*)(const StartGameModeParams*, GameModeParams*, ScoringSystem*)>(&OnlineShowtimeMode::Start)) != 0,
              "OnlineShowtimeMode::Start must bind GameMode vtable slot 5");
static_assert(sizeof(static_cast<bool (OnlineShowtimeMode::*)(const ScoringSystem*) const>(&OnlineShowtimeMode::ShouldExit)) != 0,
              "OnlineShowtimeMode::ShouldExit must bind GameMode vtable slot 13");
static_assert(sizeof(static_cast<bool (OnlineShowtimeMode::*)() const>(&OnlineShowtimeMode::RequiresStreaming)) != 0,
              "OnlineShowtimeMode::RequiresStreaming must bind GameMode vtable slot 23");
}
