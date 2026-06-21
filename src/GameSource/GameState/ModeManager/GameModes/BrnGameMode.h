#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Containers/CgsArray.h"

namespace BrnGameState
{
// Forward declaration: GameMode methods take ModeManager* / return it, but only by
// pointer, so the full layout is not needed here. ModeManager has its own owning
// header (GameSource/GameState/ModeManager/BrnModeManager.h) reconstructed by its own TU.
class ModeManager;

// Forward declaration of the state-machine state base. GameMode owns eight by-value
// concrete states (CountdownState .. OnlineSplashState) and drives them through this base
// interface: maGameModeStates holds a GameModeState* to each, and Construct / SetCurrentState
// / PreWorldUpdate dispatch through its three virtuals (OnEnter, OnLeave, Update -- vtable
// slots 0/1/2, DWARF-attested in BrnGameModeState.h). Only the pointer-to-base is needed in
// this header (the array stores pointers); the concrete states and the real GameModeState
// body live in their own TUs (GameModeStates/BrnGameModeState.* and the eight Brn*State.* TUs),
// which the GameMode .cpp #includes when it dereferences a slot.
class GameModeState;

// Forward decls for the base GameMode::Start signature (used by-pointer only). Their full
// types live in BrnGameModeParams.h, which the concrete mode .cpp files #include.
class StartGameModeParams;
class GameModeParams;
class ScoringSystem;

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

// MERGED OWNING HEADER for the GameMode base of the game-mode hierarchy (consolidated from
// the GameMode / CountdownState / IntroState / OutroState worker contributions).
//
// Reconstructed from the DecFIGS DWARF (BrnGameMode.h / BrnGameMode.cpp) with each declaration
// GATED on the X360 ledger: only the methods the X360 build attests for BrnGameState::GameMode
// are declared (Construct, Initialise, PreWorldUpdate, GetIntroDurationSeconds, SendEvent,
// ShouldExit, GetOutroTimeout, SetCurrentState, CalculateMaxPlayerWrecks,
// HasCountdownDisplayChanged) plus GetName (attested via the concrete modes that override it,
// e.g. PursuitMode::GetName) and ShouldCountdownEnd (attested via the concrete-mode overrides,
// e.g. StuntAttackMode::ShouldCountdownEnd @0x827E2558). The ~30 further DWARF virtuals are
// PS3-only drift and are deliberately left out (see "DWARF supplies names; the X360 ledger
// decides what exists" in AGENTS.md).
//
// Virtual/const/return-type/vtable-order are taken from the DWARF declaration shape, not from
// the Hex-Rays pseudocode (which renders virtuals as direct calls and drops const). The
// leading virtuals appear in DWARF source order so derived classes (OfflineGameMode,
// PursuitMode, RaceMode) bind their overrides to the correct slots.
//
// LAYOUT (BOUNDED): the only fully-reconstructed members below are GameMode's own data
// (maGameModeStates, meCurrentState, mpModeManager, mfTimeStepSeconds and the mode flags).
// The eight by-value contained concrete state objects that the DWARF places between
// meCurrentState and mpModeManager (CountdownState mCountdownState, IntroState,
// InProgressState, OutroState, ResultsState, QuitState, OnlineLoadingState, OnlineSplashState)
// are NOT reconstructed here: doing so would cascade ~9 further headers well past the 2-3
// bound. They are stood in for by a named padding buffer (maContainedStatesBlob) so the named
// members after them keep their relative placement, and Construct wires the maGameModeStates
// pointers at the blob's slot addresses. Those contained types should be reconstructed when
// their own TUs are worked; this header is then extended to replace the blob with the real
// members (and seed each maGameModeStates slot with &mCountdownState .. &mOnlineSplashState).
//
// The named state members below (meCurrentState, mpModeManager, mbConstructed,
// mbFinalStandingsShown, mfTimeStepSeconds, mbIsOnline, ...) are GameMode's own protected
// data, X360-attested by the base GameMode::Construct body (0x8232F9D8) and read/written by
// derived modes and the nested states via direct base-member access (e.g. OnlineGameMode::
// SendEvent reads the state and sets mbFinalStandingsShown; CountdownState::Update reads
// mfTimeStepSeconds via GetUpdateTimeStep). Byte offsets are not X360-faithful on the x64 PC
// gate (pointers are 8 bytes, and the contained states are still opaque), so this preserves
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
    // X360-attested via the concrete-mode overrides (e.g. StuntAttackMode::ShouldCountdownEnd,
    // 0x827E2558). The base hook the CountdownState queries before advancing; shape (virtual /
    // trailing const / bool) is DWARF-attested (BrnGameMode.h:418). Declared between
    // GetIntroDurationSeconds and SendEvent in DWARF source order.
    virtual bool        ShouldCountdownEnd() const;
    virtual void        SendEvent(EGameModeEvent leEvent);
    virtual bool        ShouldExit() const;
    virtual f32         GetOutroTimeout() const;

    // X360-attested on the base (DWARF BrnGameMode.h:244); every concrete mode overrides it. Builds
    // the mutable GameModeParams from the immutable StartGameModeParams + rank/event data. Third
    // param (ScoringSystem*) unused by most bodies. Anchored here so the vtable slot exists once.
    virtual void        Start(const StartGameModeParams* lpStartGameModeParams,
                              GameModeParams* lpGameModeParams,
                              ScoringSystem* lpScoringSystem);

    // ---- DWARF-order base-vtable extensions (appended after the committed virtuals) -------
    // These grow the base vtable so the concrete-mode family has real base slots to override
    // (the mode overrides bind BY NAME). Declared in DWARF source order (BrnGameMode.h block,
    // slots numbered below); the committed virtuals above are left untouched in order/type.

    // DWARF slot 15 (BrnGameMode.cpp:352). The post-race "is the mode over?" hook the outer
    // loop polls each frame. Derived modes (RoadRageMode @0x82315D60, StuntAttackMode
    // @0x823162B8, OnlineStuntRunMode @0x8233A3F0) override it with mode-specific logic; the
    // base never auto-finishes (returns false). Takes the live ScoringSystem* (by-name fwd
    // decl); base body ignores it. Required base blocker for the online mode family.
    virtual bool        ShouldFinish(ScoringSystem* lpScoringSystem);

    // DWARF slot 25 (BrnGameMode.h:530). Whether entering this mode shows a loading screen.
    // No standalone X360 body (the trivial default was inlined / is PS3-DWARF-only); base
    // default returns false. Required base blocker for the online mode family.
    virtual bool        HasLoadingScreen() const;

    void    SetCurrentState(s32 liState);
    s32     CalculateMaxPlayerWrecks();
    bool    HasCountdownDisplayChanged(s32* lpiNewCountdownDisplay);

    // -- Inline accessors/setters used by the nested GameModeState states -----------------
    // None of these is separately X360-attested: the X360 build inlined them at every call
    // site (e.g. CountdownState::OnEnter reads mpModeManager off the mode; IntroState/OutroState
    // ::Update read mfTimeStepSeconds; CountdownState::OnLeave sets mbVisibleCars). De-inlined
    // back to their logical accessor form here, matching the DWARF declarations.
    ModeManager* GetModeManager() const          { return mpModeManager; }
    // The mode's current state-machine state (EGameModeState). Inlined in the X360 build at the
    // call sites that gate on it (e.g. OnlineRaceMode::PreWorldUpdate reads the current mode's
    // meCurrentState @+0x28 and only recomputes the time limit when it is E_GMS_IN_PROGRESS).
    // De-inlined to this named accessor.
    s32          GetCurrentState() const          { return meCurrentState; }
    f32          GetUpdateTimeStep() const        { return mfTimeStepSeconds; }
    bool         IsOnline() const                 { return mbIsOnline; }
    void         SetFinished(bool lbFinished)     { mbFinished = lbFinished; }
    void         SetIntroJustFinished(bool lbFin) { mbIntroJustFinished = lbFin; }
    void         SetVisibleCars(bool lbVisible)   { mbVisibleCars = lbVisible; }
    // Inlined in the X360 build at the state-machine call sites (InProgressState::OnEnter sets
    // the timer-start request; ResultsState::OnEnter requests the results screen). De-inlined to
    // their named-accessor form; both back the latch bytes GameMode::Initialise clears.
    void         SetTimerStartRequest(bool lbReq)  { mbTimerStartRequested = lbReq; }
    void         SetShowResultsRequest(bool lbReq) { mbShowResultsRequested = lbReq; }

    // Sets the displayed countdown value and flags it as changed iff it differs from the
    // previous value (a one-shot the GUI polls via HasCountdownDisplayChanged). This is the
    // logic the X360 inlined at the tail of CountdownState::Update.
    void SetCountdownDisplay(s32 liCountdownDisplay)
    {
        mbCountdownDisplayChanged = (liCountdownDisplay != miCountdownDisplay);
        miCountdownDisplay        = liCountdownDisplay;
    }

protected:
    // The eight state-machine states, addressed by EGameModeState index. The X360 build stores
    // this as a CgsArray<GameModeState*,8>; the reconstructed Array<T,N> wrapper models the
    // eight pointer slots. Construct points each slot at the matching contained state object;
    // SetCurrentState/PreWorldUpdate dispatch through it.
    Array<GameModeState*, 8> maGameModeStates;

    s32       meCurrentState;           // current state-machine state (-1 == none yet)

    // Stand-in for the eight by-value contained concrete state objects (CountdownState ..
    // OnlineSplashState) that the DWARF places here. Opaque padding: deferred contained types,
    // not raw-offset access of named data. The maGameModeStates pointers above are set to slot
    // addresses inside this blob by Construct. Replace with the real members when those state
    // TUs are reconstructed.
    u8        maContainedStatesBlob[112];

    ModeManager* mpModeManager;         // owning ModeManager, set by GameMode::Construct
    f32       mfTimeStepSeconds;        // per-frame update step (read via GetUpdateTimeStep);
                                        //   the sub-state Update()s tick their timers by this
    bool      mbConstructed;            // set true once Construct has run
    bool      mbFinalStandingsShown;    // online: results screen has been requested
    bool      mbIsOnline;               // true for the online modes (OnlineGameMode sets it)

    s32       miCountdownDisplay;       // current countdown value handed back to the GUI
    bool      mbVisibleCars;            // a rival is currently within visible range
    bool      mbFinished;
    bool      mbTimerStartRequested;
    bool      mbShowResultsRequested;
    bool      mbIntroJustFinished;
    bool      mbCountdownJustFinished;
    bool      mbCountdownDisplayChanged; // set when miCountdownDisplay changes; one-shot
};
}
