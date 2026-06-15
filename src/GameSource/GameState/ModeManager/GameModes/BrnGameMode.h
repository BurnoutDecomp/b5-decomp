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
// would cascade ~10 further headers well past the 2-3 bound. They are stood in for by
// named padding buffers (maStatesBlob / maManagerBlob) so the named members below keep
// their relative placement. Those contained types should be reconstructed when their own
// TUs are worked; this header is then extended to replace the blobs with the real members.
//
// The named state members below (meCurrentState, mpModeManager, mbConstructed,
// mbFinalStandingsShown) are GameMode's own protected data, X360-attested by the base
// GameMode::Construct body (0x8232F9D8: it writes *(this+40)=-1 [state], *(this+160)=arg
// [the ModeManager*], and zeroes *(this+172)/*(this+175)) and read/written by derived
// modes via direct base-member access (e.g. OnlineGameMode::SendEvent reads the state and
// sets mbFinalStandingsShown; OnlineGameMode::Construct sets mbConstructed). They are
// modelled as named protected members here -- the base's real home -- rather than being
// re-forked into each derived class. Byte offsets are not X360-faithful on the x64 PC gate
// (pointers are 8 bytes, and the leading state objects are still opaque), so this preserves
// relative ordering and named access for semantic parity, not exact placement.
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
    // Stand-in for the leading contained state objects that precede the current-state
    // field. Modelled as opaque padding (see header note above): deferred contained
    // types, not raw-offset access of named data.
    u8        maStatesBlob[32];

    s32       meCurrentState;           // current state-machine state (-1 == none yet)

    // Stand-in for the remaining contained state objects between the state field and the
    // ModeManager back-pointer. Opaque padding, same rationale as maStatesBlob.
    u8        maManagerBlob[112];

    ModeManager* mpModeManager;         // owning ModeManager, set by GameMode::Construct
    bool      mbConstructed;            // set true once Construct has run
    bool      mbFinalStandingsShown;    // online: results screen has been requested

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
