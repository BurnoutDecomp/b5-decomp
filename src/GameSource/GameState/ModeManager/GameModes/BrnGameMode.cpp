#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"
#include "SharedClasses/DataLists/VehicleListEntry.h"   // VehicleListEntry::GetStrengthStat (CalculateMaxPlayerWrecks)

#include "GameSource/GameState/ModeManager/GameModeStates/BrnGameModeState.h"
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"   // ScoringSystem::GetPlayerNoInputTime / GetPlayerStationaryTime (slot 13)
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
// GameMode::GameMode() -- the console body is INLINED in ModeManager::ModeManager @0x827E38B0
// (2026-08-26 state-machine verify): for every one of the fifteen embedded modes it writes the
// eight state vptrs into +0x2C..+0x94 (0x827E38F0..0x827E393C -- the host gets that from the
// states' own default construction) and, load-bearingly, `li r31,-1` -> `stw r31,0x24` -- the
// KI_UNCONSTRUCTED(-1) sentinel into maGameModeStates' count word (host CgsArray has no default
// ctor, so without this the "Array used before Construct/Clear" tripwire reads garbage instead
// of firing). Everything else is seeded by GameMode::Construct (0x8232F9D8) and the derived
// Constructs, exactly as before.
// ===========================================================================
GameMode::GameMode()
{
    maGameModeStates.MarkUnconstructed();   // console `stw -1, 0x24(mode)` @0x827E38E4
}

// ===========================================================================
// X360: BrnGameState::GameMode::Construct (0x8232F9D8).
//
// Binds every contained state object to this mode + the owning ModeManager, publishes a pointer to
// each into maGameModeStates, and resets the mode to the "no current state" baseline.
//
// [!!] UNPARKED 2026-08-26 (states-blob round). The publish leg below used to be missing entirely:
// the eight states were an opaque `maContainedStatesBlob[112]` in BrnGameMode.h, so there was no
// typed object whose address could be published, the array was left Construct()'d-but-empty, and the
// wiring loop was gated on GetCount() so it ran ZERO times. That is what the boot died on --
// GameMode::Initialise -> SendEvent(E_GME_RESTART) -> SetCurrentState(E_GMS_INTRO) indexing a
// zero-length array (CgsArray.h:41 bounds assert, then the AV). The blob is now the eight named
// members (see THE CONTAINED STATES table in BrnGameMode.h) and this function does what the console
// does.
//
// THE CONSOLE BODY, INSTRUCTION FOR INSTRUCTION (re-decoded this round, whole function visible):
//   0x8232FA00  stw  r11,0xA0(r31)                     mpModeManager = lpModeManager
//   0x8232FA08  stb  r21(0),0xAC(r31)                  mbIsOnline             = false
//   0x8232FA0C  stb  r21(0),0xAF(r31)                  mbShowResultsRequested = false
//   0x8232FA14  stb  r21(0),0xB0(r31)                  mbIntroJustFinished    = false
//   0x8232F9F0..0x8232FA20  addi r29..r22 <- r31 + 0x2C,0x40,0x54,0x60,0x70,0x7C,0x88,0x94
//                                                      the eight BY-VALUE embeds
//   0x8232FA1C..0x8232FA6C  per embed `stw r31,8(rN)` + `stw r11,4(rN)`
//                                                      inlined GameModeState::Construct(mgr, this)
//   0x8232FA70  stw  r10(8),0x24(r31)                  the array's count word <- 8
//   0x8232FA74..0x8232FB08  eight GetI(r31+4, i) calls, i = 0..7, each followed by
//                           `stw <embed>,0(r3)`        slot i <- &embed(i)
//   0x8232FB0C  stb  r21(0),0xB3(r31)                  mbVisibleCars  = false
//   0x8232FB10  stw  r11(-1),0x28(r31)                 meCurrentState = -1
//
// The three flag bytes really are zeroed BEFORE the state wiring and the fourth after it; that order
// is reproduced below (it is observationally irrelevant -- nothing aliases -- but free to keep).
//
// PUBLISH SHAPE: the console sets the count word to 8 and THEN writes the eight slots through the
// checked accessor. Append() off the Construct()'d (count 0) array reaches the identical end state
// -- count 8, slot i holding the i'th embed's address -- without the console's intermediate window
// where the count claims eight live slots before any of them has been written. Both the count and
// the slot contents are asserted at the tail.
// ===========================================================================
void GameMode::Construct(ModeManager* lpModeManager)
{
    mpModeManager = lpModeManager;

    // console 0x8232FA08 / 0x8232FA0C / 0x8232FA14 (r21 == 0 from `li r21,0` @0x8232F9EC).
    //
    // [!] NAMES CORRECTED 2026-08-26 (wave-B fix round), kept here because the mis-transcriptions are
    // still in circulation: +172 is mbIsOnline (OfflineGameMode::Construct @0x8232FE78 stores 0,
    // OnlineGameMode::Construct @0x8232FEB4 stores 1, and ModeManager::ProcessEvent @0x82340AF4 reads
    // it to pick the ONLINE vs OFFLINE stunt scorer -- a "constructed" flag can be none of those);
    // +175 is mbShowResultsRequested (OnlineGameMode::SendEvent @0x8232FFA4 sets it on entering state
    // 3); +176 is mbIntroJustFinished (the byte Initialise also clears at 0xB0). See the +160..+179
    // table in BrnGameMode.h for the full per-byte pinning.
    mbIsOnline             = false;   // +172 / 0xAC
    mbShowResultsRequested = false;   // +175 / 0xAF
    mbIntroJustFinished    = false;   // +176 / 0xB0

    // console 0x8232FA1C..0x8232FA6C -- the inlined GameModeState::Construct(lpModeManager, this)
    // expanded over each embed in address order (`stw r31,8(rN)` == mpGameMode = this,
    // `stw r11,4(rN)` == mpModeManager). Unrolled on the console and unrolled here: the states are
    // named members now, so there is nothing to loop over an array for.
    mCountdownState.Construct(lpModeManager, this);      // +0x2C
    mIntroState.Construct(lpModeManager, this);          // +0x40
    mInProgressState.Construct(lpModeManager, this);     // +0x54
    mOutroState.Construct(lpModeManager, this);          // +0x60
    mResultsState.Construct(lpModeManager, this);        // +0x70
    mQuitState.Construct(lpModeManager, this);           // +0x7C
    mOnlineLoadingState.Construct(lpModeManager, this);  // +0x88
    mOnlineSplashState.Construct(lpModeManager, this);   // +0x94

    // console 0x8232FA70 (count word <- 8) + the eight GetI/`stw` pairs at 0x8232FA74..0x8232FB08.
    // The Append index IS the E_GMS_* id: SetCurrentState / PreWorldUpdate index this same array
    // with meCurrentState, whose domain the console asserts as `< 8` (0x82327238).
    //
    // [!] THE Construct() ON THE NEXT LINE IS LOAD-BEARING, NOT TIDINESS. GameMode::Construct is
    // NOT called once per mode: ModeManager::Construct walks eighteen mapGameModes slots and three
    // of them (12, 14, 17) alias the SAME OnlineStuntRunMode, so that mode is Construct()ed THREE
    // times (BrnModeManager_Lifecycle.cpp:186-207, and the console does the same -- its publish leg
    // is a count store plus eight indexed writes, which is idempotent by construction). Resetting
    // the count to zero here makes the eight Appends idempotent the same way. Appending onto a
    // surviving count would overflow the eight-slot array on the second pass.
    maGameModeStates.Construct();
    maGameModeStates.Append(&mCountdownState);       // i=0 -> +0x2C  E_GMS_COUNTDOWN
    maGameModeStates.Append(&mIntroState);           // i=1 -> +0x40  E_GMS_INTRO
    maGameModeStates.Append(&mInProgressState);      // i=2 -> +0x54  E_GMS_IN_PROGRESS
    maGameModeStates.Append(&mOutroState);           // i=3 -> +0x60  E_GMS_OUTRO
    maGameModeStates.Append(&mResultsState);         // i=4 -> +0x70  E_GMS_RESULTS
    maGameModeStates.Append(&mQuitState);            // i=5 -> +0x7C  E_GMS_QUIT
    maGameModeStates.Append(&mOnlineLoadingState);   // i=6 -> +0x88  E_GMS_ONLINE_LOADING
    maGameModeStates.Append(&mOnlineSplashState);    // i=7 -> +0x94  E_GMS_ONLINE_SPLASH

    // The console's `stw 8, 0x24(r31)` restated as a post-condition. This is the tripwire for the
    // exact defect this round closed: if the publish leg is ever short again, the array's own bounds
    // assert would only fire later, from inside whichever state transition happened to be first.
    CGS_ASSERT(maGameModeStates.GetCount() == KI_GMS_COUNT,
               "maGameModeStates.GetCount() == GameStateModuleIO::E_GMS_COUNT");

    mbVisibleCars  = false;   // console 0x8232FB0C `stb r21,0xB3(r31)`
    meCurrentState = -1;      // console 0x8232FB10 `stw r11(-1),0x28(r31)`
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
// and calls *through the vtable* -- `lwz r11,0(r31); lwz r11,0x30(r11); bctrl`.
// X360 vtable slots are FOUR bytes (32-bit function pointers), so byte offset
// 0x30 is slot 12 -- and slot 12 of the console 26-slot map dumped in
// BrnGameMode.h is GameMode::SendEvent (RaceMode's word at +48 is 0x8232FDA0,
// the same leaf in all fifteen concrete vtables). The literal `0` is therefore
// an EGameModeEvent, not a state id: 0 == E_GME_RESTART. So this is a *virtual*
// SendEvent(E_GME_RESTART), NOT the non-virtual SetCurrentState the earlier
// recon rendered (SetCurrentState has no vtable slot -- see BrnGameMode.h). The
// base SendEvent maps E_GME_RESTART -> SetCurrentState(KI_GMS_INTRO), so the net
// effect is still "go to Intro", but derived modes can override SendEvent, which
// the virtual dispatch here honours.
//
// [!] MISCOUNT CORRECTED 2026-08-26 (stuntrace waveB CLOSURE round). This banner
// used to gloss the pseudocode's `(*(*a1+48))(a1,0)` as "Hex-Rays counts the
// 8-byte-slot offset 48 == 0x30". That is wrong twice over and it invites the
// exact arithmetic that mints a phantom slot: 48 IS 0x30, the same BYTE offset
// the `lwz` uses -- no unit conversion happens -- and the slots are 4 bytes, not
// 8, so 48 / 4 == 12. (Had the slots been 8 bytes, 48 would have been slot 6,
// GetName, which is not what the console dispatches.) The banner also cited a
// 13-entry "DWARF GameMode vtable"; the authority is now the 26-slot console map
// in BrnGameMode.h, whose slots 0..12 happen to agree with it.
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
// SIGNATURE (corrected 2026-08-26, wave-B fix round): this is vtable slot 2 and the console
// passes SIX arguments after `this`. UpdateCurrentMode @0x82350EC8 dispatches
// `(*(**(a1+3480)+8))(*(a1+3480), a2, a3, a8, a28, a30, a1+3504)`, and the DecFIGS DWARF
// (BrnGameMode.cpp:122) spells them out: (OutputBuffer*, const PreWorldInputBuffer*,
// const RCEntityGlobalRaceCarOutputInterface*, const RCEntityActiveRaceCarOutputInterface*,
// bool, const ScoringSystem*). The previous no-arg shape was NOT a harmless simplification:
// StuntAttackMode (0x82344EE0), FaceOffMode, RoadRageMode, BurningRouteMode, SurvivorMode,
// OnlineRaceMode, OnlineStuntRunMode, OnlineBurningHomeRunMode and OnlineFreeBurnLobbyMode all
// override this slot, and a 0-arg override would have MINTED A NEW SLOT rather than binding.
//
// [!] STILL PARKED -- the rival-visibility scan. The console walks lpActiveRaceCars with
// RCEntityActiveRaceCarOutputInterface::IsRaceCarActive (0x8232FB20's inner loop) and fires that
// header's own asserts ("mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT" and
// "Player car index hasn't been set", BrnRaceCarEntityModuleOutputInterface.h:967/:980). The
// argument is now present, but the TYPE is still declaration-only tree-wide (no member layout, no
// IsRaceCarActive body -- grep BrnWorld::RaceCarEntityModuleIO), so the scan cannot be written
// yet. That is the exact blocker; it is no longer "the signature drops the input".
// DELETE-WHEN: RCEntityActiveRaceCarOutputInterface lands. Then, for each active rival !=
// the player car, compute its offset from the player along travel and set mbVisibleCars if
// |ahead| < KF_VISIBLE_DISTANCE_AHEAD (150) or |behind| < KF_VISIBLE_DISTANCE_BEHIND (10).
// Consequence while parked: mbVisibleCars stays false, so GameMode::ShouldExit always takes its
// 3.0f stationary threshold rather than the 10.0f "a rival is on screen" one.
// ===========================================================================
void GameMode::PreWorldUpdate(GameStateModuleIO::OutputBuffer* lpOutput,
                              const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                              const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpGlobalRaceCars,
                              const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
                              bool lbPaused,
                              const ScoringSystem* lpScoringSystem)
{
    (void)lpOutput;
    (void)lpInput;
    (void)lpGlobalRaceCars;
    (void)lpActiveRaceCars;   // [!] the parked scan's input -- see the banner above
    (void)lbPaused;
    (void)lpScoringSystem;

    mbVisibleCars = false;

    if (meCurrentState != -1)
    {
        CGS_ASSERT(meCurrentState >= 0 && meCurrentState < KI_GMS_COUNT,
                   "meCurrentState >=0 && meCurrentState < GameStateModuleIO::E_GMS_COUNT");

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
// SIGNATURE (corrected 2026-08-26, wave-B fix round): this is vtable slot 13 and it takes the
// live const ScoringSystem*. ModeManager::PreWorldUpdate @0x823537B8 dispatches
// `(*(**(a1+3480)+52))(*(a1+3480), a1+3504)` (a1+3504 == &mScoringSystem), the DWARF spells it
// `virtual bool ShouldExit(const ScoringSystem *) const`, and six concrete modes override it
// (Crash / RoadRage / Pursuit / StuntAttack / OnlineShowtime return false; Survivor has its own
// body @0x82316318). The previous no-arg shape would have made every one of those a NEW slot.
//
// With the argument present the two timer reads are no longer placeholders. Console asm,
// instruction for instruction (0x82315B80):
//     lfs f13, 0x5CF4(r4)                  ; scoring +23796 == mfPlayerTimeWithoutInput
//     lfs f0,  flt_820211D4                ; image.bin @0x820211D4 == 4.0f
//     fcmpu / ble -> return 0
//     lfs f0,  0x5CF0(r4)                  ; scoring +23792 == mfPlayerTimeStationary
//     lfs f13, flt_82020F90                ; image.bin @0x82020F90 == 3.0f
//     fcmpu / ble -> return 0
//     lbz r11, 0xB3(r3)                    ; mbVisibleCars (+179)
//     beq -> return 1                      ; no rival visible: 3.0 s is enough
//     lfs f13, flt_82004A20                ; image.bin @0x82004A20 == 10.0f
//     fcmpu / ble -> return 0 ; else return 1
// (Both floats re-dumped big-endian from image.bin this session; +23796 is the SECOND of the two
// f32s declared in BrnScoringSystem.h:757-758, i.e. mfPlayerTimeWithoutInput, and +23792 is the
// first, mfPlayerTimeStationary -- reached through the named accessors per hazard H9, never by
// raw offset.)
// ===========================================================================
bool GameMode::ShouldExit(const ScoringSystem* lpScoringSystem) const
{
    const f32 KF_STATIONARY_TIME_FOR_MODE_EXIT     = 3.0f;
    const f32 KF_NO_INPUT_TIME_FOR_MODE_EXIT       = 10.0f;
    const f32 KF_MAX_STATIONARY_TIME_FOR_MODE_EXIT = 4.0f;

    const f32 lfNoInputTime    = lpScoringSystem->GetPlayerNoInputTime();
    const f32 lfStationaryTime = lpScoringSystem->GetPlayerStationaryTime();

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
//
// INDEX DOMAIN RE-VERIFIED 2026-08-26 (states-blob unpark round) -- this is the assert the boot was
// dying under, so it was checked rather than assumed. The console body (0x82327238) takes the state
// as an UNSIGNED and guards it once at the top and once on the outgoing state:
//     if ( a2 >= 8 )      -> "leState >=0 && leState < GameStateModuleIO::E_GMS_COUNT"  BrnGameMode.h:483
//     if ( v4 >= 8 )      -> "meCurrentState >=0 && meCurrentState < ...E_GMS_COUNT"    BrnGameMode.h:489
// then `v5 = GetI(v3 + 1, v3[10])` and `v6 = GetI(v3 + 1, a2)` -- v3+1 is the array base (this+4) and
// v3[10] is meCurrentState (this+40), both matching the offsets GameMode::Construct writes. So the
// domain is exactly [0, 8) == [E_GMS_COUNTDOWN, E_GMS_COUNT) == KI_GMS_COUNT below, which is also
// exactly the eight slots Construct now publishes. The -1 (E_GMS_INVALID) outgoing case is excluded
// by the `!= -1` test before the index is ever formed, on both sides.
//
// The practical consequence of the unpark: for the whole of that domain, Array::operator[]'s live
// bounds check (luIndex < miCount, miCount == 8) now PASSES. Until this round miCount was 0, so the
// very first transition -- Initialise -> SendEvent(E_GME_RESTART) -> SetCurrentState(KI_GMS_INTRO) --
// tripped it. No change was needed to this function; it was correct and starved.
// ===========================================================================
void GameMode::SetCurrentState(s32 liState)
{
    CGS_ASSERT(liState >= 0 && liState < KI_GMS_COUNT, "leState >=0 && leState < GameStateModuleIO::E_GMS_COUNT");

    if (meCurrentState != liState)
    {
        if (meCurrentState != -1)
        {
            CGS_ASSERT(meCurrentState >= 0 && meCurrentState < KI_GMS_COUNT, "meCurrentState >=0 && meCurrentState < GameStateModuleIO::E_GMS_COUNT");

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
// SHAPE NOTE (RETIRED 2026-09-02, road-rage wave -- the DWARF shape is restored below): the committed (gated) PC class declared
// CalculateMaxPlayerWrecks() with no parameters; the X360 build takes the
// const StartGameModeParams* it reads the player car entry from. StartGameModeParams
// and the vehicle-list-entry / gameplay-data types are out of scope for this TU and
// are not threaded through the kept no-arg shape, so the strength read is left as a
// documented placeholder. The bucketing (the entire decision content of this method)
// is reproduced exactly; widening the signature later only requires substituting the
// strength read below.
// ===========================================================================
s32 GameMode::CalculateMaxPlayerWrecks(const StartGameModeParams* lpStartGameModeParams)
{
    // [road-rage wave 2026-09-02] THE PLACEHOLDER IS GONE: the DWARF shape is restored and the
    // strength read is the console's -- StartGameModeParams +828 (mpPlayerCarVehicleListEntry,
    // assert BrnGameModeParams.h:992 inside the inlined getter, then BrnGameMode.cpp:414) ->
    // mGamePlayData +0xB (`lbz 0xB(r30+0x90)` == VehicleListEntry::GetStrengthStat(), +0x9B).
    // The console also asserts the +144 sub-object pointer (cpp:417) -- always true for an
    // embedded block; reproduced as the tripwire it is.
    const BrnResource::VehicleListEntry* lpPlayerCarVehicleListEntry =
        lpStartGameModeParams->GetPlayerVehicleGamePlayData();
    CGS_ASSERT(lpPlayerCarVehicleListEntry != NULL, "lpPlayerCarVehicleListEntry != NULL");   // cpp:414
    CGS_ASSERT(lpPlayerCarVehicleListEntry != NULL, "lpPlayerCarVehicleListEntryGamePlayData != NULL"); // cpp:417
    if (lpPlayerCarVehicleListEntry == NULL)
    {
        return KI_NUM_WEAKEST_CAR_CRASHES;   // [PC GUARD] assert-is-not-a-guard; console would deref NULL
    }
    const s32 liStrength = static_cast<s32>(lpPlayerCarVehicleListEntry->GetStrengthStat());

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

    CGS_ASSERT(lpiNewCountdownDisplay != nullptr, "lpiNewCountdownDisplay != NULL");

    *lpiNewCountdownDisplay = miCountdownDisplay;
    mbCountdownDisplayChanged = false;
    return true;
}

// ===========================================================================
// X360: BrnGameState::GameMode::ShouldFinish (vtable slot 14, DWARF BrnGameMode.cpp:352).
//
// The base post-race "should the mode end now?" hook, polled each frame. It is the FOLDED LEAF
// 0x827E2F38 (`li r3,0; blr`) at slot 14 of RaceMode's vtable 0x820D0498 -- i.e. the base never
// auto-finishes. Three modes override it: RoadRageMode @0x82315D60, StuntAttackMode @0x823162B8
// and OnlineStuntRunMode @0x8233A3F0. Returns false; the ScoringSystem* is taken (so the override
// slot binds) but unused by the base body.
// ===========================================================================
bool GameMode::ShouldFinish(ScoringSystem* lpScoringSystem)
{
    (void)lpScoringSystem;
    return false;
}

// ===========================================================================
// X360: BrnGameState::GameMode::HasLoadingScreen (vtable slot 24, DWARF BrnGameMode.h:530).
//
// Whether entering this mode puts up a loading screen. SetupGameMode @0x8234B158 reads it twice
// and HandleLoadingScreenLoaded @0x8234B8A8 once, both through vtbl+96. Slot 24 is the folded
// leaf 0x827E2F38 (`li r3,0; blr`) in ALL EIGHT offline vtables and in OnlineFreeBurn /
// OnlineFreeBurnLobby / OnlineShowtime, so the base is FALSE; only OnlineRace, OnlineRoadRage,
// OnlineStuntRun and OnlineBurningHomeRun return true (0x82C296C8), which is exactly the set the
// DWARF declares an override in.
// ===========================================================================
bool GameMode::HasLoadingScreen() const
{
    return false;
}

// ===========================================================================
// THE REMAINING CONSOLE VTABLE SLOTS.
//
// These are the slots that were MISSING from the committed header entirely. Every one of them is
// a real console slot -- see the 26-slot table in BrnGameMode.h, which was re-derived from
// image.bin (all 15 concrete mode vtables) and cross-checked against the DecFIGS DWARF's
// declaration order. They were not "PS3-only drift": leaving them out did not merely lose
// behaviour, it SHIFTED every slot after the first gap, so the committed order bound
// OfflineGameMode::Construct at 0, StuntAttackMode::GetName at 3, StuntAttackMode::Start at 9 and
// so on -- none of which is where the console dispatches.
//
// Where the base implementation is a COMDAT-folded leaf, the leaf address is quoted. Those leaves
// resolve to unrelated names in func_index.tsv; the disassembly is the authority:
//   0x8284CB38 = `blr` / 0x82C296C8 = `li r3,1; blr` / 0x827E2F38 = `li r3,0; blr`
//   0x82661058 = `li r3,-1; blr` / 0x827DF718 = `li r3,2; blr`
// ===========================================================================

// Slot 1 (vtbl+4). Folded leaf 0x8284CB38 (`blr`) in all 15 concrete vtables -- the console body
// is empty. DWARF BrnGameMode.cpp:105.
void GameMode::Destruct()
{
}

// Slot 3 (vtbl+12). Folded leaf 0x8284CB38 (`blr`) in 14 of the 15 vtables; only
// OnlineFreeBurnLobbyMode overrides it (0x82340F40, IDA-named PostWorldUpdate -- one of the four
// independent witnesses that pin this index). ModeManager::PostWorldUpdate @0x8234A9E0 dispatches
// `(*(**(a1+3480)+12))(*(a1+3480), lpInput)`, which is where the single argument comes from.
void GameMode::PostWorldUpdate(const GameStateModuleIO::PostWorldInputBuffer* lpInput)
{
    (void)lpInput;
}

// Slot 5 (vtbl+20). LINK-HOLE CLOSER, not a recovered console body: there is no GameMode::Start
// export, because all fifteen concrete modes override the slot. Eight of the fifteen mode headers
// in this tree do not yet declare their own Start, so without this definition their vtables would
// be unresolved externals the moment they mount (the M1 hole). Empty and inert -- a mode whose
// Start has not been reconstructed simply builds no GameModeParams, which is what happens today.
void GameMode::Start(const StartGameModeParams* lpStartGameModeParams,
                     GameModeParams* lpGameModeParams,
                     ScoringSystem* lpScoringSystem)
{
    (void)lpStartGameModeParams;
    (void)lpGameModeParams;
    (void)lpScoringSystem;
}

// Slot 6 (vtbl+24). LINK-HOLE CLOSER: all fifteen console vtables carry their own GetName leaf, so
// no GameMode::GetName body is attested -- but CrashMode, RoadRageMode and several others do not
// declare one in this tree and would otherwise leave the slot unresolved. Returns the class name
// so a debug print of an un-reconstructed mode is still readable rather than a null deref.
const char* GameMode::GetName() const
{
    return "GameMode";
}

// Slot 7 (vtbl+28). ModeManager::StartGameMode @0x8234FCE8 calls it directly:
// `v17 = (*(**(a1+3480)+28))(*(a1+3480))`. RESTORED 2026-08-26: the committed headers gated this
// out of BOTH intermediate bases as "PS3-only drift". The image refutes that -- slot 7 is
// 0x82C296C8 (`li r3,1`) in ALL EIGHT offline vtables and 0x827DF718 (`li r3,2`) in ALL SEVEN
// online ones, which is only possible if both OfflineGameMode and OnlineGameMode override it
// (and the DWARF declares exactly those two overrides).
//
// [!] UNATTESTED BASE. Because both intermediates override it, no vtable in the image ever
// exposes GameMode's own slot 7, so the base return value is unrecoverable from the ROM. The
// value below is the safest inert default (E_FRAMERATEMANAGER_SINGLE == 0) and is UNREACHABLE in
// practice: every instantiable mode derives from one of the two intermediates.
CgsSystem::EFrameRateManagerType GameMode::GetFrameRateType() const
{
    return CgsSystem::E_FRAMERATEMANAGER_SINGLE;
}

// Slot 9 (vtbl+36). Folded leaf 0x82C296C8 (`li r3,1; blr`) in ALL FIFTEEN vtables -- base TRUE.
// The DWARF declares overrides in RaceMode, BurningRouteMode, StuntAttackMode and SurvivorMode,
// but every one of them folded onto the same `li r3,1` leaf, so they are invisible in the image
// and are (correctly) not re-declared in those headers.
bool GameMode::HasTimedIntro() const
{
    return true;
}

// Slot 10 (vtbl+40). Folded leaf 0x8284CB38 (`blr`) -- base empty. ModeManager::PostWorldUpdate
// @0x8234A9E0 drives it (`(*(**(a1+3480)+40))(*(a1+3480))`). Overridden by RoadRageMode
// (0x823160A0) and SurvivorMode (0x82316398); neither body is reconstructed yet, so those two
// modes currently inherit this no-op -- a documented behaviour gap, not a slot error.
void GameMode::OnPlayerInShortCut()
{
}

// Slot 11 (vtbl+44). Folded leaf 0x82C296C8 (`li r3,1; blr`) in 14 of the 15 vtables -- base TRUE
// ("nothing is holding the countdown"). The sole override is StuntAttackMode (0x827E2558,
// `lbz r3,0xD4(r3); blr` == mbPlayerPointingInStartDirection). LINK-HOLE CLOSER as well: 13 of the
// 14 embedded modes do not declare this override.
bool GameMode::ShouldCountdownEnd() const
{
    return true;
}

// Slot 15 (vtbl+60). Folded leaf 0x8284CB38 (`blr`) -- base empty. Three IDA-named overrides land
// here and independently corroborate the index: FaceOffMode 0x82315CC0, RoadRageMode 0x82315D40,
// SurvivorMode 0x823163A8. Called by the results packer to add the mode-specific fields to the
// FinishedModeAction record.
void GameMode::FillInGameModeSpecificResults(const ScoringSystem* lpScoringSystem,
                                             GameStateModuleIO::FinishedModeAction* lpAction)
{
    (void)lpScoringSystem;
    (void)lpAction;
}

// Slot 17 (vtbl+68). Folded leaf 0x82661058 (`li r3,-1; blr`) in all 15 vtables -- no mode
// overrides it on this build, so -1 IS the shipped behaviour, not a placeholder.
::EGlobalRaceCarIndex GameMode::GetGlobalRivalToShow() const
{
    return E_GLOBAL_RACE_CAR_INDEX_INVALID;
}

// Slot 18 (vtbl+72). Folded leaf 0x82661058 (`li r3,-1; blr`) in all 15 vtables -- same as slot 17.
::EActiveRaceCarIndex GameMode::GetActiveRivalToShow() const
{
    return E_ACTIVE_RACE_CAR_INDEX_INVALID;
}

// Slot 19 (vtbl+76). Folded leaf 0x8284CB38 (`blr`) -- base empty. ModeManager::PostWorldUpdate
// dispatches `(*(**(a1+3480)+76))(*(a1+3480), v13)`, which is where the single
// EActiveRaceCarIndex argument comes from; IDA independently names the OnlineFreeBurnLobbyMode
// override at 0x823315A8 PlayerHasSpawned, on this exact index.
void GameMode::PlayerHasSpawned(::EActiveRaceCarIndex leActiveRaceCarIndex)
{
    (void)leActiveRaceCarIndex;
}

// Slot 20 (vtbl+80). Folded leaf 0x8284CB38 (`blr`) -- base empty. IDA names the
// OnlineFreeBurnLobbyMode override at 0x8234CF98 ProcessNewRoadScore, on this index. Parameter
// list from the DWARF (BrnGameMode.cpp:381); the road-rules score record is passed BY VALUE.
void GameMode::ProcessNewRoadScore(GameStateModuleIO::OutputBuffer* lpOutput,
                                   BrnStreetData::ChallengePlayerScoreEntry lScoreEntry,
                                   BrnStreetData::ScoreType leScoreType,
                                   BrnStreetData::ChallengeIndex lChallengeIndex,
                                   ::EActiveRaceCarIndex leActiveRaceCarIndex)
{
    (void)lpOutput;
    (void)lScoreEntry;
    (void)leScoreType;
    (void)lChallengeIndex;
    (void)leActiveRaceCarIndex;
}

// Slot 21 (vtbl+84). Folded leaf 0x8284CB38 (`blr`) -- base empty. IDA names the
// OnlineFreeBurnLobbyMode override at 0x82331700 OnEnterRoad, on this index.
void GameMode::OnEnterRoad(BrnStreetData::RoadIndex lRoadIndex)
{
    (void)lRoadIndex;
}

// Slot 22 (vtbl+88). Folded leaf 0x8284CB38 (`blr`) -- base empty. RoadRageMode overrides it at
// 0x82315FF8.
void GameMode::HandleGameEvents(const CgsModule::Event* lpEvent, s32 liCount)
{
    (void)lpEvent;
    (void)liCount;
}

// Slot 23 (vtbl+92). BASE IS TRUE -- folded leaf 0x82C296C8 (`li r3,1; blr`). SetupGameMode
// @0x8234B158 gates the streaming wait on it (`if ((*(**(a1+3480)+92))(*(a1+3480)))`). Proof the
// base is true rather than false: the seven vtables whose slot 23 is 0x827E2F38 (Crash, RoadRage,
// BurningRoute, StuntAttack, OnlineFreeBurn, OnlineFreeBurnLobby, OnlineShowtime) are EXACTLY the
// seven the DWARF declares a RequiresStreaming override in -- so the other eight are inheriting
// 0x82C296C8, i.e. the base.
bool GameMode::RequiresStreaming() const
{
    return true;
}

// Slot 25 (vtbl+100). Folded leaf 0x8284CB38 (`blr`) -- base empty. SurvivorMode overrides it at
// 0x827E2580 (`lfs f0,[0x82001CC0]=0.0f; stfs f0,0xC0(r3); blr`).
void GameMode::OnPlayerUsesPaintShop()
{
}

// ===========================================================================
// BrnGameState::OnlineSplashState::OnEnter -- THE EIGHTH STATE'S ONLY BODY, HOMED IN THIS TU.
//
// [!] IT DOES NOT BELONG HERE. The console has its own BrnOnlineSplashState.cpp (DWARF
// BrnOnlineSplashState.cpp:38) and so should this tree. It is defined in BrnGameMode.cpp only
// because the shipping source list lives in tools/build/build_game_exe.bat, which this round is not
// allowed to edit: a new GameModeStates/BrnOnlineSplashState.cpp would pass the compile gate and
// then silently never be built into the exe, leaving OnlineSplashState's vtable with an unresolved
// slot 0 the moment GameMode::Construct embeds one. BrnGameMode.cpp IS on that list
// (build_game_exe.bat:4203). [x] MOVE THIS to GameModeStates/BrnOnlineSplashState.cpp the next time
// that file list is touched.
//
// THE BODY IS ATTESTED, not invented. OnlineSplashState's vtable is the eighth of the eight
// consecutive state vtables in .rdata, at 0x820CF430, and it reads
// {0x823166B0, 0x8284CB38, 0x8284CB38}. Slot 0 holds the SAME word as OnlineLoadingState's slot 0
// (0x820CF424) -- the two OnEnter bodies COMDAT-folded onto one leaf, which IDA names
// OnlineLoadingState::OnEnter:
//     0x823166B0  lwz r11, 8(r3)      ; mpGameMode  (GameModeState +8)
//     0x823166B4  li  r10, 0
//     0x823166B8  stb r10, 0xAD(r11)  ; GameMode +173 == mbFinished
//     0x823166BC  blr
// i.e. mpGameMode->SetFinished(false) -- byte for byte the committed BrnOnlineLoadingState.cpp body.
// Slots 1/2 are the folded `blr` leaf 0x8284CB38, i.e. the empty GameModeState base, so OnLeave and
// Update are not overridden here (the same call the committed ResultsState / QuitState /
// OnlineLoadingState headers make).
// ===========================================================================
void OnlineSplashState::OnEnter()
{
    mpGameMode->SetFinished(false);
}
}
