#pragma once

#include "types.hpp"

namespace BrnGameState
{
// Forward declaration: GameMode methods take ModeManager* / return it, but only by
// pointer, so the full layout is not needed here. ModeManager has its own owning
// header (GameSource/GameState/ModeManager/...) reconstructed by its own TU.
class ModeManager;

// Events the mode state-machine reacts to. X360-attested via GameMode::SendEvent,
// matching the DecFIGS DWARF (BrnGameMode.h:46). Kept here because SendEvent takes it.
enum EGameModeEvent
{
    E_GME_RESTART     = 0,
    E_GME_NEXT        = 1,
    E_GME_ABORT       = 2,
    E_GME_USER_ACCEPT = 3,
    E_GME_COUNT       = 4
};

// Owning header for the GameMode base of the game-mode hierarchy. Reconstructed from
// the DecFIGS DWARF (BrnGameMode.h / BrnGameMode.cpp) with each declaration GATED on
// the X360 ledger: only the methods the X360 build attests for BrnGameState::GameMode
// are declared here (Construct, Initialise, PreWorldUpdate, GetIntroDurationSeconds,
// SendEvent, ShouldExit, GetOutroTimeout, SetCurrentState, CalculateMaxPlayerWrecks,
// HasCountdownDisplayChanged) plus GetName (attested via the concrete modes that
// override it, e.g. PursuitMode::GetName). The ~30 further DWARF virtuals are PS3-only
// drift and are deliberately left out (see "DWARF supplies names; the X360 ledger
// decides what exists" in AGENTS.md).
//
// Virtual/const/return-type/vtable-order are taken from the DWARF declaration shape,
// not from the Hex-Rays pseudocode (which renders virtuals as direct calls and drops
// const). The leading virtuals appear in DWARF source order so derived classes
// (OfflineGameMode, PursuitMode) bind their GetName override to the correct slot.
//
// LAYOUT (BOUNDED): this TU's only function, HasCountdownDisplayChanged, touches just
// two members -- miCountdownDisplay and mbCountdownDisplayChanged. The ten by-value
// contained state objects that precede them in the real layout (maGameModeStates,
// CountdownState mCountdownState, IntroState, InProgressState, OutroState, ResultsState,
// QuitState, OnlineLoadingState, OnlineSplashState) are NOT reconstructed here: doing so
// would cascade ~10 further headers well past the 2-3 bound. They are stood in for by a
// single named padding buffer (maStatesAndManagerBlob) so the named members below keep
// their relative placement. Those contained types should be reconstructed when their own
// TUs are worked; this header is then extended to replace the blob with the real members.
class GameMode
{
public:
    GameMode();

    virtual void        Construct(ModeManager* lpModeManager);
    virtual void        Initialise();
    virtual void        PreWorldUpdate();
    virtual const char* GetName() const;
    virtual f32         GetIntroDurationSeconds() const;
    virtual void        SendEvent(EGameModeEvent leEvent);
    virtual bool        ShouldExit() const;
    virtual f32         GetOutroTimeout() const;

    void    SetCurrentState(s32 liState);
    s32     CalculateMaxPlayerWrecks();
    bool    HasCountdownDisplayChanged(s32* lpiNewCountdownDisplay);

protected:
    // Stand-in for the leading contained state objects + ModeManager pointer + the
    // earlier per-frame flags. Modelled as opaque padding (see header note above):
    // these are deferred contained types, not raw-offset access of named data.
    u8        maStatesAndManagerBlob[168];

    s32       miCountdownDisplay;       // current countdown value handed back to the GUI
    bool      mbVisibleCars;            // (DWARF order: precedes the countdown flag)
    bool      mbFinished;
    bool      mbTimerStartRequested;
    bool      mbShowResultsRequested;
    bool      mbIntroJustFinished;
    bool      mbCountdownJustFinished;
    bool      mbCountdownDisplayChanged; // set when miCountdownDisplay changes; one-shot
};
}
