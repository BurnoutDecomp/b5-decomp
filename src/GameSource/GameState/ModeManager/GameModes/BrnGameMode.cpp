#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"

#include "GameSource/GameState/ModeManager/GameModeStates/BrnGameModeState.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnGameState
{
// ---------------------------------------------------------------------------
// EGameModeState (BrnGameStateModuleIO) state-machine ids, X360-attested by the
// base SetCurrentState/SendEvent bodies (the raw 0..7 the pseudocode passes). The
// GameStateModuleIO::EGameModeState enum is not yet reconstructed (its own TU), so
// the states are passed as raw s32 to SetCurrentState, matching the X360 body and
// the already-committed OnlineGameMode::SendEvent. Named here for readability only.
//   0 Countdown  1 Intro  2 InProgress  3 Outro
//   4 Results    5 Quit   6 OnlineLoading  7 OnlineSplash
// E_GMS_COUNT == 8.
// ---------------------------------------------------------------------------
enum
{
    KI_GMS_COUNTDOWN       = 0,
    KI_GMS_INTRO           = 1,
    KI_GMS_IN_PROGRESS     = 2,
    KI_GMS_OUTRO           = 3,
    KI_GMS_RESULTS         = 4,
    KI_GMS_QUIT            = 5,
    KI_GMS_ONLINE_LOADING  = 6,
    KI_GMS_ONLINE_SPLASH   = 7,
    KI_GMS_COUNT           = 8
};

// Car-strength -> max-player-wrecks thresholds. X360-attested file-scope consts in
// BrnGameMode.cpp (DecFIGS BrnGameMode.cpp:38-44). Only the *_STRENGTH cut-offs and
// the corresponding crash counts that CalculateMaxPlayerWrecks actually reads are
// reproduced here.
static const s32 KI_HIGHEST_WEAKEST_CAR_STRENGTH = 2;
static const s32 KI_HIGHEST_WEAK_CAR_STRENGTH    = 3;
static const s32 KI_HIGHEST_MEDIUM_CAR_STRENGTH  = 7;
static const s32 KI_NUM_WEAKEST_CAR_CRASHES      = 2;
static const s32 KI_NUM_WEAK_CAR_CRASHES         = 3;
static const s32 KI_NUM_MEDIUM_CAR_CRASHES       = 4;
static const s32 KI_NUM_STRONG_CAR_CRASHES       = 5;

// Source path baked into the X360 asserts of this TU; reused verbatim for parity.
static const char* const KPC_SOURCE_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\gamestate\\modemanager\\gamemodes\\BrnGameMode.cpp";

// ===========================================================================
// X360: BrnGameState::GameMode::Construct (0x8232F9D8).
//
// Binds every contained state object to this mode + the owning ModeManager and
// publishes a pointer to each into maGameModeStates, then resets the mode to the
// "no current state" baseline. In the X360 build the eight states are by-value
// members (mCountdownState .. mOnlineSplashState); the Hex-Rays output shows the
// pattern plainly: for each state it writes *(state+8)=this (GameModeState::
// mpGameMode) and *(state+4)=lpModeManager (GameModeState::mpModeManager) -- i.e.
// the inlined GameModeState::Construct(lpModeManager, this) -- and stores &state
// into the array slot (the inlined CgsArray::GetI/operator[] plus a count of 8).
//
// Under this bounded reconstruction the eight concrete states still live in the
// opaque maContainedStatesBlob, so Construct can't name them yet. The logical
// shape is preserved: each array slot is pointed at its state and Construct'd, and
// the loop is the de-inlined form of the eight unrolled writes. When the state TUs
// land, the blob becomes named members and &mCountdownState etc. replace the slot
// addresses (the loop body is unchanged). meCurrentState is reset to -1 and the
// mode flags are zeroed, exactly as the X360 body's tail does.
//
// INTEGRATOR NOTE: maGameModeStates is published from the (currently opaque) state
// objects in maContainedStatesBlob. Until those concrete state members exist, the
// pointer slots are not yet populated with real addresses; the loop expresses the
// X360 wiring so that swapping the blob for named members (and seeding each slot
// with &mCountdownState .. &mOnlineSplashState) completes it without touching the
// loop body.
// ===========================================================================
void GameMode::Construct(ModeManager* lpModeManager)
{
    mpModeManager = lpModeManager;

    // De-inlined GameModeState::Construct(lpModeManager, this) over the eight
    // contained states, each addressed through its published array slot.
    for (u32 luState = 0; luState < KI_GMS_COUNT; ++luState)
    {
        GameModeState* lpState = maGameModeStates[luState];
        lpState->Construct(lpModeManager, this);
    }

    // X360 0x8232F9D8 zeroes four flag bytes in its tail: offset 172 (stb @0x8232FA08,
    // mbConstructed), 175 (stb @0x8232FA0C, mbFinalStandingsShown), 176 (stb @0x8232FA14,
    // mbShowResultsRequested) and 179 (stb @0x8232FB0C, mbVisibleCars) -- all r21==0. The
    // pseudocode shows the same set (*(a1+172)=0; *(a1+175)=0; *(a1+176)=0; *(a1+179)=0).
    mbConstructed          = false;
    mbFinalStandingsShown  = false;
    mbShowResultsRequested = false;
    mbVisibleCars          = false;

    meCurrentState         = -1;
}

// ===========================================================================
// X360: BrnGameState::GameMode::Initialise (0x82315A28).
//
// Re-arms the mode for a fresh run by sending the RESTART event through the
// state-machine, then clears the per-run latch flags plus the cached countdown
// display. In the X360 build the flags sit in a contiguous byte run
// (*(a1+173)..*(a1+178) = 0, *(a1+168) = -1); here they are the named one-shot
// flags + miCountdownDisplay.
//
// The asm makes the dispatch explicit: it loads the literal argument `li r4,0`
// and calls *through the vtable* -- `lwz r11,0(r31); lwz r11,0x30(r11); bctrl`
// -- i.e. virtual slot 12 (0x30/4). Slot 12 of the DWARF GameMode vtable is
// SendEvent(EGameModeEvent) (Construct=0, Destruct=1, PreWorldUpdate=2,
// PostWorldUpdate=3, Initialise=4, Start=5, GetName=6, GetFrameRateType=7,
// GetIntroDurationSeconds=8, HasTimedIntro=9, OnPlayerInShortCut=10,
// ShouldCountdownEnd=11, SendEvent=12). The literal `0` is therefore an
// EGameModeEvent, not a state id: 0 == E_GME_RESTART. So this is a *virtual*
// SendEvent(E_GME_RESTART), NOT the non-virtual SetCurrentState the earlier
// recon rendered (SetCurrentState has no vtable slot -- see BrnGameMode.h). The
// base SendEvent maps E_GME_RESTART -> SetCurrentState(KI_GMS_INTRO), so the net
// effect is still "go to Intro", but derived modes can override SendEvent, which
// the virtual dispatch here honours. The pseudocode's (*(*a1+48))(a1,0) is this
// dispatch (Hex-Rays counts the 8-byte-slot offset 48 == 0x30; arg literal 0).
// ===========================================================================
void GameMode::Initialise()
{
    SendEvent(E_GME_RESTART);

    mbFinished              = false;
    mbTimerStartRequested   = false;
    mbShowResultsRequested  = false;
    mbIntroJustFinished     = false;
    mbCountdownJustFinished = false;
    mbCountdownDisplayChanged = false;

    miCountdownDisplay      = -1;
}

// ===========================================================================
// X360: BrnGameState::GameMode::PreWorldUpdate (0x8232FB20).
//
// Per-frame tick of the active state, gated by a rival-visibility test. The X360
// body first clears mbVisibleCars, then scans every active rival race-car against
// the player car: if any rival is within the visible distance ahead/behind
// (KF_VISIBLE_DISTANCE_AHEAD == 150 / KF_VISIBLE_DISTANCE_BEHIND == 10, selected by
// whether the rival is in front of or behind the player), it sets mbVisibleCars and
// stops scanning. It then dispatches Update() on the current state object.
//
// SHAPE NOTE (kept class shape): the committed (gated) PC class declares
// PreWorldUpdate() with no parameters. The X360 build receives the per-frame world
// IO buffers as arguments (OutputBuffer*, const PreWorldInputBuffer*, the global +
// active race-car output interfaces, bool, const ScoringSystem*) and runs the
// rival-visibility scan over the active race-car output interface. Those world-IO
// buffer types are out of scope for this TU; to honour the kept no-arg shape, the
// data-driven scan is left as a documented placeholder (the `work stubs` philosophy
// applied to a dropped *input*, not an invented callee). The fully-recovered control
// flow -- clear the flag, scan, then drive the current state through Update() -- is
// preserved. When this signature is widened to the DWARF shape (or the active
// race-car output interface type is reconstructed), the scan body below is filled in:
// for each active rival != player, compute its offset from the player along travel
// and set mbVisibleCars if |ahead| < 150 or |behind| < 10.
// ===========================================================================
void GameMode::PreWorldUpdate()
{
    mbVisibleCars = false;

    // STUB (dropped input): the rival-visibility scan reads the per-frame active
    // race-car output interface, which the kept no-arg shape does not carry. Left as
    // a no-op placeholder so the mbVisibleCars default (false == "no cars visible")
    // holds until the world-IO buffers are threaded in. See SHAPE NOTE.

    if (meCurrentState != -1)
    {
        if (meCurrentState < 0 || meCurrentState >= KI_GMS_COUNT)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "meCurrentState >=0 && meCurrentState < GameStateModuleIO::E_GMS_COUNT",
                KPC_SOURCE_FILE,
                159);
            CgsDev::Assert::EndAssert();
        }

        maGameModeStates[meCurrentState]->Update();
    }
}

// ===========================================================================
// X360: BrnGameState::GameMode::GetIntroDurationSeconds (0x82315A88).
//
// How long the pre-race intro flyby runs, in seconds. The X360 body asks the owning
// ModeManager which game-mode is active and (for non-stunt modes) how many cars take
// part in the flyby:
//   - When the active mode is the stunt-run / showtime family (mode ids 15 and 16),
//     or the mode has no flyby intro, it returns a fixed duration:
//       * flyby present but stunt/showtime  -> 2.0  (KF_STUNT_INTRO_TIME_SECONDS)
//       * no flyby                           -> 6.0  (KF_INTRO_TIME_SECONDS)
//   - Otherwise the duration scales with the flyby car count:
//       (ModeManager::GetNumberOfCarsInFlyby() + 1) * 4.0
//
// In the pseudocode the active mode id is *(mpModeManager+3476) and the flyby flag is
// the byte at *(*(mpModeManager+3480)+172); both live on the ModeManager.
//
// SHAPE NOTE (kept class shape): GetNumberOfCarsInFlyby() and the active-mode/flyby
// fields live on ModeManager, whose committed header is the minimal one-method slice.
// Threading those reads here would fork several accessors onto that bounded header,
// which is out of scope for this TU. The branch structure (stunt/showtime + no-flyby
// fixed paths vs. car-count-scaled path) is reproduced faithfully; the ModeManager
// reads are left as a documented placeholder returning the standard fixed intro until
// ModeManager is reconstructed far enough to expose them.
// ===========================================================================
f32 GameMode::GetIntroDurationSeconds() const
{
    const f32 KF_INTRO_TIME_SECONDS       = 6.0f;
    const f32 KF_STUNT_INTRO_TIME_SECONDS = 2.0f;

    // STUB (ModeManager reads not yet exposed): the X360 body branches on the active
    // mode id (stunt-run/showtime == 15/16) and the flyby flag, both read from
    // mpModeManager, and otherwise returns (GetNumberOfCarsInFlyby()+1)*4.0. Those
    // ModeManager accessors are out of scope for this TU (see SHAPE NOTE); the
    // standard fixed intro is returned until they are wired in. The stunt-mode short
    // path (2.0) and the car-count-scaled path are documented above for the
    // integrator to restore once ModeManager exposes the fields.
    (void)KF_STUNT_INTRO_TIME_SECONDS;
    return KF_INTRO_TIME_SECONDS;
}

// ===========================================================================
// X360: BrnGameState::GameMode::SendEvent (0x8232FDA0).
//
// The base game-mode state machine: maps an inbound EGameModeEvent to a state
// transition. ABORT always jumps to Quit; RESTART always jumps to Intro; otherwise
// the transition depends on the current state. SetCurrentState returns void in the
// DWARF-attested API -- the pseudocode's `result = ... / return result` are
// register-reuse artifacts of a void function and are dropped (matching the already-
// committed OnlineGameMode::SendEvent). State ids are passed as raw s32 (see the
// EGameModeState note above).
// ===========================================================================
void GameMode::SendEvent(EGameModeEvent leEvent)
{
    if (leEvent == E_GME_ABORT)
    {
        SetCurrentState(KI_GMS_QUIT);
        return;
    }
    if (leEvent == E_GME_RESTART)
    {
        SetCurrentState(KI_GMS_INTRO);
        return;
    }

    switch (meCurrentState)
    {
        case KI_GMS_COUNTDOWN:
            if (leEvent == E_GME_NEXT)
            {
                SetCurrentState(KI_GMS_IN_PROGRESS);
            }
            break;
        case KI_GMS_INTRO:
            if (leEvent == E_GME_NEXT || leEvent == E_GME_USER_ACCEPT)
            {
                SetCurrentState(KI_GMS_COUNTDOWN);
            }
            break;
        case KI_GMS_IN_PROGRESS:
            if (leEvent == E_GME_NEXT)
            {
                SetCurrentState(KI_GMS_OUTRO);
            }
            break;
        case KI_GMS_OUTRO:
            if (leEvent == E_GME_NEXT)
            {
                SetCurrentState(KI_GMS_RESULTS);
            }
            break;
        case KI_GMS_RESULTS:
            if (leEvent == E_GME_USER_ACCEPT)
            {
                SetCurrentState(KI_GMS_QUIT);
            }
            break;
        default:
            break;
    }
}

// ===========================================================================
// X360: BrnGameState::GameMode::ShouldExit (0x82315B80).
//
// Whether the mode should exit itself because the player has gone idle. The X360
// body reads two timers off the ScoringSystem: a no-input timer (*(scoring+23796))
// and a stationary timer (*(scoring+23792)). It returns true when the no-input timer
// exceeds KF_MAX_STATIONARY_TIME_FOR_MODE_EXIT (4 s) AND the stationary timer exceeds
// KF_STATIONARY_TIME_FOR_MODE_EXIT (3 s) -- but if rival cars are currently visible
// (mbVisibleCars), the stationary timer must instead exceed
// KF_NO_INPUT_TIME_FOR_MODE_EXIT (10 s) before exit is allowed. Defaults to false.
//
// SHAPE NOTE (kept class shape): the committed (gated) PC class declares ShouldExit()
// const with no parameters; the X360 build takes the const ScoringSystem* it reads
// these timers from. ScoringSystem is out of scope for this TU and is not threaded
// through the kept no-arg shape, so the two timer reads are left as documented
// placeholders. The threshold logic and the mbVisibleCars gate (which this mode owns)
// are reproduced exactly, so widening the signature / exposing the timers later only
// requires substituting the two reads below.
// ===========================================================================
bool GameMode::ShouldExit() const
{
    const f32 KF_STATIONARY_TIME_FOR_MODE_EXIT     = 3.0f;
    const f32 KF_NO_INPUT_TIME_FOR_MODE_EXIT       = 10.0f;
    const f32 KF_MAX_STATIONARY_TIME_FOR_MODE_EXIT = 4.0f;

    // STUB (dropped input): the X360 reads these two timers from the ScoringSystem*
    // the no-arg shape does not carry. Zeroed so the idle-exit predicate is inert
    // (never forces an exit) until ScoringSystem is threaded in; the predicate shape
    // below is the faithful X360 logic.
    const f32 lfNoInputTime    = 0.0f;
    const f32 lfStationaryTime = 0.0f;

    if (lfNoInputTime > KF_MAX_STATIONARY_TIME_FOR_MODE_EXIT)
    {
        if (lfStationaryTime > KF_STATIONARY_TIME_FOR_MODE_EXIT &&
            (!mbVisibleCars || lfStationaryTime > KF_NO_INPUT_TIME_FOR_MODE_EXIT))
        {
            return true;
        }
    }

    return false;
}

// ===========================================================================
// X360: BrnGameState::GameMode::GetOutroTimeout (0x827DFBC8).
//
// The base post-race outro hold time. The X360 body is a single constant return
// (KF_OUTRO_TIME_SECONDS == 0.1 s). Derived modes override this for longer outros.
// ===========================================================================
f32 GameMode::GetOutroTimeout() const
{
    const f32 KF_OUTRO_TIME_SECONDS = 0.1f;
    return KF_OUTRO_TIME_SECONDS;
}

// ===========================================================================
// X360: BrnGameState::GameMode::SetCurrentState (0x82327238).
//
// The single entry point for state transitions. Validates the requested state, and
// if it differs from the current one: leaves the outgoing state (OnLeave, vtable
// slot 1), records the new state, then enters the incoming state (OnEnter, vtable
// slot 0). On the very first transition meCurrentState is -1 (no outgoing state to
// leave). Dispatch is through maGameModeStates, exactly as the X360 body indexes the
// inlined CgsArray (GetI). leState arrives as a raw s32 EGameModeState id.
// ===========================================================================
void GameMode::SetCurrentState(s32 liState)
{
    if (liState < 0 || liState >= KI_GMS_COUNT)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "leState >=0 && leState < GameStateModuleIO::E_GMS_COUNT",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\gamestate\\modemanager\\gamemodes\\BrnGameMode.h",
            483);
        CgsDev::Assert::EndAssert();
    }

    if (meCurrentState != liState)
    {
        if (meCurrentState != -1)
        {
            if (meCurrentState < 0 || meCurrentState >= KI_GMS_COUNT)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(
                    "meCurrentState >=0 && meCurrentState < GameStateModuleIO::E_GMS_COUNT",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\gamestate\\modemanager\\gamemodes\\BrnGameMode.h",
                    489);
                CgsDev::Assert::EndAssert();
            }

            maGameModeStates[meCurrentState]->OnLeave();
        }

        meCurrentState = liState;
        maGameModeStates[liState]->OnEnter();
    }
}

// ===========================================================================
// X360: BrnGameState::GameMode::CalculateMaxPlayerWrecks (0x82315BD8).
//
// How many times the player may be wrecked before the mode ends, derived from the
// strength of the player's car. The X360 body reads the player car's vehicle-list
// entry (StartGameModeParams::mpPlayerCarVehicleListEntry, *(params+828)) -> its
// gameplay data (+144) -> the car strength field, and buckets it:
//   strength <= 2  -> 2 wrecks   (weakest)
//   strength <= 3  -> 3 wrecks   (weak)
//   strength <= 7  -> 4 wrecks   (medium)
//   strength  > 7  -> 5 wrecks   (strong)
//
// SHAPE NOTE (kept class shape): the committed (gated) PC class declares
// CalculateMaxPlayerWrecks() with no parameters; the X360 build takes the
// const StartGameModeParams* it reads the player car entry from. StartGameModeParams
// and the vehicle-list-entry / gameplay-data types are out of scope for this TU and
// are not threaded through the kept no-arg shape, so the strength read is left as a
// documented placeholder. The bucketing (the entire decision content of this method)
// is reproduced exactly; widening the signature later only requires substituting the
// strength read below.
// ===========================================================================
s32 GameMode::CalculateMaxPlayerWrecks()
{
    // STUB (dropped input): car strength is read from the player car's gameplay data
    // off the StartGameModeParams* the no-arg shape does not carry. Defaulted to the
    // weakest bucket until StartGameModeParams is threaded in; the bucketing below is
    // the faithful X360 logic.
    const s32 liStrength = 0;

    if (liStrength <= KI_HIGHEST_WEAKEST_CAR_STRENGTH)
    {
        return KI_NUM_WEAKEST_CAR_CRASHES;
    }
    if (liStrength <= KI_HIGHEST_WEAK_CAR_STRENGTH)
    {
        return KI_NUM_WEAK_CAR_CRASHES;
    }
    if (liStrength > KI_HIGHEST_MEDIUM_CAR_STRENGTH)
    {
        return KI_NUM_STRONG_CAR_CRASHES;
    }
    return KI_NUM_MEDIUM_CAR_CRASHES;
}

// Hands the latest countdown value back to the caller (the ModeManager GUI feed) and
// reports whether it changed since the last query. The change flag is one-shot: it is
// cleared here so a subsequent call returns false until SetCountdownDisplay sets it
// again. Returns false (and writes nothing) when nothing changed.
bool GameMode::HasCountdownDisplayChanged(s32* lpiNewCountdownDisplay)
{
    if (!mbCountdownDisplayChanged)
    {
        return false;
    }

    if (lpiNewCountdownDisplay == nullptr)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpiNewCountdownDisplay != NULL",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\gamestate\\modemanager\\gamemodes\\BrnGameMode.h",
            449);
        CgsDev::Assert::EndAssert();
    }

    *lpiNewCountdownDisplay = miCountdownDisplay;
    mbCountdownDisplayChanged = false;
    return true;
}
}
