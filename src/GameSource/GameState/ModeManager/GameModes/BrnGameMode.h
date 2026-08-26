#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/BurnoutConstants.h"                       // ::EGlobalRaceCarIndex / ::EActiveRaceCarIndex (slots 17/18/19/20)
#include "GameShared/GameClasses/System/Timer/CgsFrameRate.h"  // CgsSystem::EFrameRateManagerType (slot 7)
#include "SharedClasses/StreetData/BrnChallengeData.h"         // BrnStreetData::{ChallengePlayerScoreEntry, ScoreType} (slot 20)
#include "SharedClasses/StreetData/BrnStreetData.h"            // BrnStreetData::{ChallengeIndex, RoadIndex} (slots 20/21)

// THE EIGHT BY-VALUE CONTAINED STATES (console +0x2C .. +0x94 -- see THE CONTAINED STATES table
// below). These are members, not pointers, so their complete types are required right here. No
// include cycle: every one of these headers stops at BrnGameModeState.h, which reaches GameMode
// only through a forward declaration.
#include "GameSource/GameState/ModeManager/GameModeStates/BrnCountdownState.h"
#include "GameSource/GameState/ModeManager/GameModeStates/BrnIntroState.h"
#include "GameSource/GameState/ModeManager/GameModeStates/BrnInProgressState.h"
#include "GameSource/GameState/ModeManager/GameModeStates/BrnOutroState.h"
#include "GameSource/GameState/ModeManager/GameModeStates/BrnResultsState.h"
#include "GameSource/GameState/ModeManager/GameModeStates/BrnQuitState.h"
#include "GameSource/GameState/ModeManager/GameModeStates/BrnOnlineLoadingState.h"
#include "GameSource/GameState/ModeManager/GameModeStates/BrnOnlineSplashState.h"

// Pointer-only parameter types for the console vtable slots below. The real definitions live in
// GameSource/World/EntityModules/RaceCarEntityModule/... and are declaration-only in this tree
// today -- this is the same forward declaration BrnGameStateModuleIO.h:34 and BrnScoringSystem.h:82
// already carry.
namespace BrnWorld
{
namespace RaceCarEntityModuleIO
{
struct RCEntityGlobalRaceCarOutputInterface;
struct RCEntityActiveRaceCarOutputInterface;
}
}

// Pointer-only: slot 22 HandleGameEvents' record type. Real definition CgsVariableEventQueue.h:38.
namespace CgsModule { struct Event; }

namespace BrnGameState
{
// Forward declaration: GameMode methods take ModeManager* / return it, but only by
// pointer, so the full layout is not needed here. ModeManager has its own owning
// header (GameSource/GameState/ModeManager/BrnModeManager.h) reconstructed by its own TU.
class ModeManager;

// The state-machine state base is a real, COMPLETE type here now (BrnGameModeState.h arrives via
// the eight concrete-state includes above). GameMode owns eight by-value concrete states
// (mCountdownState .. mOnlineSplashState) and drives them through this base interface:
// maGameModeStates holds a GameModeState* to each, and Construct / SetCurrentState /
// PreWorldUpdate dispatch through its three virtuals (OnEnter, OnLeave, Update -- vtable slots
// 0/1/2, dumped from the eight console state vtables at 0x820CF3DC..0x820CF43C; see the block in
// BrnGameModeState.h). The forward declaration that used to stand here went with the blob.

// Forward decls for the base GameMode::Start signature (used by-pointer only). Their full
// types live in BrnGameModeParams.h, which the concrete mode .cpp files #include.
class StartGameModeParams;
class GameModeParams;
class ScoringSystem;

namespace GameStateModuleIO
{
    // Spine-parameter buffers -- BY POINTER ONLY in this header. BrnGameStateModuleIO.h must NOT
    // be included from here: it reaches a SECOND `enum EActiveRaceCarIndex : s32` inside
    // `namespace BrnGameState`, which would silently re-bind the unqualified enum names used in
    // the declarations below (that is why slots 18/19/20 spell them `::EActiveRaceCarIndex`).
    // The same ban, with its full post-mortem, is at BrnModeManager.h:52-64.
    struct OutputBuffer;
    struct PreWorldInputBuffer;
    struct PostWorldInputBuffer;

    // Slot 15's out-parameter record. Real definition BrnGameActions.h:738.
    struct FinishedModeAction;
}

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
// ===================================================================================================
// CONSOLE VTABLE (X360, 26 slots) -- re-derived from image.bin on 2026-08-26 by the wave-B fix round,
// independently of the micro-check that first reported it.
// ===================================================================================================
// The virtual block below is IN CONSOLE SLOT ORDER and must stay that way. A derived override whose
// signature does not match its base declaration silently MINTS A NEW SLOT instead of binding, and
// nothing a compile-only gate can see will complain. Every mode header therefore carries a
// member-pointer static_assert block that re-binds the base signature onto the derived class (see
// e.g. BrnStuntAttackMode.h) -- that block is the tripwire; do not delete it.
//
// HOW THIS WAS RECOVERED (three anchors, all re-dumped this session, none taken on trust):
//   * Neither GameMode nor OfflineGameMode emits a vtable of its own: GameMode::Construct
//     (0x8232F9D8) occurs image-wide EXACTLY ONCE, in .pdata at 0x821C13A0, never in .rdata. The
//     base slot map below is therefore the CONSENSUS of all 15 concrete mode vtables.
//   * The eight offline vtables are exactly 26 slots / 0x68 long: 0x820D0498 (RaceMode) + 104 ==
//     0x820D0500 (FaceOffMode), and the word at RaceMode slot 26 IS FaceOffMode's slot 0
//     (0x8232FE58 OfflineGameMode::Construct). The seven ONLINE vtables are 27 slots / 0x6C --
//     slot 26 is an X360-only online-family addition and is deliberately NOT part of this base.
//   * All 15 vtables were identified by decoding their slot-6 GetName leaf (`lis r11,hi /
//     addi r3,r11,lo / blr`) and reading the string it returns: 0x820D0498 "Race", 0x820D0500
//     "FaceOff", 0x820D0570 "CrashMode", 0x820D05E8 "RoadRage", 0x820D0650 "Pursuit", 0x820D06B8
//     "BurningRoute", 0x820D0720 "Stunt Race" (@0x82029ADC), 0x820D0788 "Survivor", 0x820D07F0
//     "OnlineRace", 0x820D0860 "OnlineRoadRage", 0x820D08E0 "OnlineStuntRun", 0x820D0960
//     "OnlineBurningHomeRun", 0x820D09E8 "OnlineFreeBurn", 0x820D0A68 "OnlineFreeBurnLobby",
//     0x820D0AE8 "OnlineShowtime".
//   * The DecFIGS DWARF (references/DecFIGS/.../GameModes/BrnGameMode.h:229-361) declares exactly
//     these 26 virtuals IN THIS ORDER, and every arity below is independently pinned by a
//     ModeManager dispatch site (the "call site" column).
//
// FOLDED-LEAF LEGEND -- COMDAT identical-code folding makes func_index.tsv resolve these leaves to
// unrelated names. Disassembled from image.bin; do NOT "fix" the index against them:
//     0x8284CB38 = `blr`                (empty void)
//     0x82C296C8 = `li r3,1;  blr`      (return true / 1)
//     0x827E2F38 = `li r3,0;  blr`      (return false / 0)
//     0x82661058 = `li r3,-1; blr`      (return -1)
//     0x827DF718 = `li r3,2;  blr`      (OnlineGameMode::GetFrameRateType == 2)
//
// COLUMN 3 IS THE WORD IN RaceMode's OWN VTABLE (0x820D0498), read slot-by-slot out of image.bin.
// For 25 of the 26 rows that word IS the GameMode base implementation. [!] ROW 16 IS THE ONE
// EXCEPTION and is called out in place -- RaceMode OVERRIDES slot 16, so its word is NOT the base.
// Do not read this column as "the base" without checking the row's own text (corrected 2026-08-26,
// stuntrace waveB CLOSURE round: the head previously read "base impl (RaceMode ...)", which row 16
// contradicted by printing the base leaf instead of RaceMode's word).
//
//  #  off  RaceMode's word (0x820D0498)        name / console call site
// --  ---  ---------------------------------   --------------------------------------------------
//  0   +0  8232FE58 OfflineGameMode::Construct  Construct(ModeManager*)
//  1   +4  8284CB38 (blr)                       Destruct()
//  2   +8  8232FB20 GameMode::PreWorldUpdate    PreWorldUpdate(6 args) -- UpdateCurrentMode
//                                                 `(*(**(a1+3480)+8))(mode,a2,a3,a8,a28,a30,a1+3504)`
//  3  +12  8284CB38 (blr)                       PostWorldUpdate(const PostWorldInputBuffer*) --
//                                                 ModeManager::PostWorldUpdate `(...+12)(mode,lpInput)`
//  4  +16  82315A28 GameMode::Initialise        Initialise() -- SetupGameMode `(...+16)(mode)`
//  5  +20  82330018 RaceMode::Start             Start(startParams, params, scoring) -- StartGameMode
//                                                 `(*(**(a1+3480)+20))(mode,a3,&v30,a1+3504)`
//  6  +24  827E2488 RaceMode::GetName           const char* GetName() const
//  7  +28  82C296C8 (ret 1)                     GetFrameRateType() -- StartGameMode `(...+28)(mode)`.
//                                                 Offline family == 1; online family == 0x827DF718 == 2.
//  8  +32  82315A88 GameMode::GetIntroDuration  f32 GetIntroDurationSeconds() -- StartModeIntro
//                                                 `(*(*v20+32))(v20)`
//  9  +36  82C296C8 (ret 1)                     bool HasTimedIntro() const -- base TRUE; the four DWARF
//                                                 overrides all fold onto the same leaf.
// 10  +40  8284CB38 (blr)                       OnPlayerInShortCut() -- PostWorldUpdate `(...+40)(mode)`
// 11  +44  82C296C8 (ret 1)                     bool ShouldCountdownEnd() const -- base TRUE
// 12  +48  8232FDA0 GameMode::SendEvent         SendEvent(EGameModeEvent) -- ExitCurrentMode sends 0
//                                                 (E_GME_RESTART); StartModeIntro / FinishCurrentMode /
//                                                 FinishOfflineModeIntro send 1 (E_GME_NEXT). The sends
//                                                 are ENUM values, never a bool.
// 13  +52  82315B80 GameMode::ShouldExit        bool ShouldExit(const ScoringSystem*) const --
//                                                 ModeManager::PreWorldUpdate `(...+52)(mode,a1+3504)`
// 14  +56  827E2F38 (ret 0)                     bool ShouldFinish(ScoringSystem*) -- UpdateCurrentMode
//                                                 and ModeManager::PreWorldUpdate
//                                                 `(*(*mode+56))(mode,a1+3504)`
// 15  +60  8284CB38 (blr)                       FillInGameModeSpecificResults(const ScoringSystem*,
//                                                 FinishedModeAction*)
// 16  +64  827E2498 -> [0x820211EC] = 0.0f      f32 GetOutroTimeout() const.
//                                               [!] THE ONE ROW WHERE RaceMode's WORD IS NOT THE
//                                                 BASE: RaceMode OVERRIDES slot 16 to 0.0f
//                                                 (827E2498 = `lis r11,0x8202; lfs f1,0x11EC(r11);
//                                                 blr`, and [0x820211EC] dumps as 0.0f).
//                                                 THE GameMode BASE IS 0.1f -- leaf 827DFBC8
//                                                 (`lfs f1,0x11D8(r11)`, [0x820211D8] == 0.1f),
//                                                 which is the word in EIGHT of the fifteen
//                                                 vtables (Crash, Pursuit, RoadRage,
//                                                 OnlineRoadRage, OnlineBurningHomeRun,
//                                                 OnlineFreeBurn, OnlineFreeBurnLobby,
//                                                 OnlineShowtime) -- i.e. the consensus, which is
//                                                 how every other base value here was recovered.
//                                                 StuntRace also overrides, to 0.0f
//                                                 (827E2548 -> [0x82021244]).
// 17  +68  82661058 (ret -1)                    EGlobalRaceCarIndex GetGlobalRivalToShow() const
// 18  +72  82661058 (ret -1)                    EActiveRaceCarIndex GetActiveRivalToShow() const
// 19  +76  8284CB38 (blr)                       PlayerHasSpawned(EActiveRaceCarIndex) -- PostWorldUpdate
//                                                 `(...+76)(mode,v13)`
// 20  +80  8284CB38 (blr)                       ProcessNewRoadScore(...) -- OnlineFreeBurnLobbyMode
//                                                 overrides it @0x8234CF98
// 21  +84  8284CB38 (blr)                       OnEnterRoad(RoadIndex) -- OnlineFreeBurnLobbyMode
//                                                 @0x82331700
// 22  +88  8284CB38 (blr)                       HandleGameEvents(const CgsModule::Event*, s32) --
//                                                 RoadRageMode @0x82315FF8
// 23  +92  82C296C8 (ret 1)                     bool RequiresStreaming() const -- SetupGameMode
//                                                 `if ((*(**(a1+3480)+92))(mode))`. THE BASE IS TRUE: the
//                                                 seven modes whose slot 23 is 0x827E2F38 are exactly the
//                                                 seven the DWARF declares an override in (Crash, RoadRage,
//                                                 BurningRoute, StuntAttack, OnlineFreeBurn,
//                                                 OnlineFreeBurnLobby, OnlineShowtime).
// 24  +96  827E2F38 (ret 0)                     bool HasLoadingScreen() const -- SetupGameMode (twice) and
//                                                 HandleLoadingScreenLoaded `(*(*v4+96))()`. THE BASE IS
//                                                 FALSE and ALL EIGHT OFFLINE MODES ARE FALSE, so a stunt
//                                                 race takes neither the loading-screen path nor (slot 23)
//                                                 the WaitForStreaming path. Only OnlineRace /
//                                                 OnlineRoadRage / OnlineStuntRun / OnlineBurningHomeRun
//                                                 return true.
// 25 +100  8284CB38 (blr)                       OnPlayerUsesPaintShop() -- SurvivorMode @0x827E2580
//
// The non-virtual SetCurrentState / CalculateMaxPlayerWrecks / HasCountdownDisplayChanged and all the
// inline accessors are NOT in the console vtable (the dump confirms it) and stay non-virtual.
// ===================================================================================================
//
// ===================================================================================================
// THE CONTAINED STATES -- UNPARKED 2026-08-26. The opaque `maContainedStatesBlob[112]` that used to
// stand between meCurrentState and mpModeManager is GONE; the eight by-value state objects are named
// members again. That blob was the last stopper on the whole mode state machine: with no typed
// object to take the address of, GameMode::Construct could publish nothing into maGameModeStates,
// so GameMode::Initialise -> SendEvent(E_GME_RESTART) -> SetCurrentState(E_GMS_INTRO) indexed a
// ZERO-LENGTH array and tripped CgsArray.h:41 (then AV'd).
//
// HOW THE EIGHT WERE IDENTIFIED (re-decoded from GameMode::Construct 0x8232F9D8 this round, not
// inherited from the previous note). The console body addresses each embed once and publishes it
// once:
//
//   0x8232F9F0..0x8232FA20  addi r29,r31,0x2C / r28,+0x40 / r27,+0x54 / r26,+0x60 /
//                           r25,+0x70 / r24,+0x7C / r23,+0x88 / r22,+0x94   -- the eight embeds
//   0x8232FA1C..0x8232FA6C  per embed: `stw r31,8(rN)` (mpGameMode = this) and `stw r11,4(rN)`
//                           (mpModeManager) -- the inlined GameModeState::Construct
//   0x8232FA70              `stw r10(8),0x24(r31)` -- the array count word (base this+4, 8 * 4-byte
//                           pointers -> the count lands at 4 + 32 == 0x24)
//   0x8232FA74..0x8232FB08  eight Array<GameModeState*,8>::GetI(this+4, i) calls, i = 0..7, each
//                           followed by `stw <embed address>,0(r3)` -- IN THIS ORDER:
//                             i=0 -> +0x2C   i=1 -> +0x40   i=2 -> +0x54   i=3 -> +0x60
//                             i=4 -> +0x70   i=5 -> +0x7C   i=6 -> +0x88   i=7 -> +0x94
//
// The publish index IS the E_GMS_* id (SetCurrentState indexes this same array with it), so the
// address order above IS the enum order -- and since C++ lays members out in declaration order and
// the addresses ascend monotonically with i, DECLARATION ORDER == E_GMS ORDER. Three independent
// witnesses agree, and none of them was assumed:
//
//   #  console off  size  member                E_GMS id                     size cross-check
//  --  -----------  ----  --------------------  ---------------------------  ---------------------------
//   0        +0x2C    20  mCountdownState       E_GMS_COUNTDOWN      == 0    12 base + f32 + s32  = 20
//   1        +0x40    20  mIntroState           E_GMS_INTRO          == 1    12 base + f32 + bool = 20
//   2        +0x54    12  mInProgressState      E_GMS_IN_PROGRESS    == 2    12 base, no members
//   3        +0x60    16  mOutroState           E_GMS_OUTRO          == 3    12 base + f32        = 16
//   4        +0x70    12  mResultsState         E_GMS_RESULTS        == 4    12 base, no members
//   5        +0x7C    12  mQuitState            E_GMS_QUIT           == 5    12 base, no members
//   6        +0x88    12  mOnlineLoadingState   E_GMS_ONLINE_LOADING == 6    12 base, no members
//   7        +0x94    12  mOnlineSplashState    E_GMS_ONLINE_SPLASH  == 7    12 base, no members
//
//   * SIZES are the gaps between consecutive embeds (the last runs to mpModeManager at +0xA0), and
//     sizeof(GameModeState) on the 32-bit console ABI is 12 (vptr + mpModeManager + mpGameMode).
//     The "size cross-check" column is the ALREADY-COMMITTED member list of each state TU -- every
//     one of those was reconstructed independently of this function, and every one matches its gap.
//     The three DIFFERENT sizes pin slots 2 and 3 on their own: of the eight states only
//     InProgressState has no members at all, and only OutroState has exactly one f32.
//   * VTABLES: the eight state vtables sit consecutively in .rdata at 0x820CF3DC, 0x820CF3E8,
//     0x820CF3F4, 0x820CF400, 0x820CF40C, 0x820CF418, 0x820CF424, 0x820CF430 -- three slots each, in
//     THIS ORDER, each slot 0 holding the matching state's own attested OnEnter (0x82332D70,
//     0x823163C8, 0x82316600, 0x82316610, 0x82316690, 0x823166A0, 0x823166B0, 0x823166B0). Dumped
//     from image.bin; the full table is in BrnGameModeState.h.
//   * ENUM: BrnGameStateSharedIO.h:153-162 already spells E_GMS_COUNTDOWN=0 .. E_GMS_ONLINE_SPLASH=7,
//     E_GMS_COUNT=8 -- the domain SetCurrentState (0x82327238) asserts `a2 < 8` against.
//
// Slots 0 and 1 are the only pair the SIZES alone cannot separate (both 20). The vtables separate
// them: 0x820CF3DC slot 0 is 0x82332D70 == CountdownState::OnEnter and 0x820CF3E8 slot 0 is
// 0x823163C8 == IntroState::OnEnter, both IDA-named, in that order -- Countdown first.
//
// Byte offsets are NOT X360-faithful on the x64 PC gate (pointers are 8 bytes wide, so each state is
// 24 bytes here rather than 12/16/20), and per the house rule a by-value embed is reconstructed for
// NAMED, semantically-parallel members, not for byte-exact sizeof. The member run below preserves
// the console's relative ORDERING exactly; nothing in this tree reads GameMode by raw offset.
// ModeManager -- the one class that embeds all fifteen modes by value -- pins its own layout with
// ORDER-ONLY static_asserts (BrnModeManager_AssertLayout.cpp:88-116, `offsetof(a) < offsetof(b)`),
// never absolute offsets, so growing GameMode here is invisible to them by construction.
// ===================================================================================================
class GameMode
{
public:
    GameMode();

    // ---- THE CONSOLE VTABLE, SLOT 0 .. SLOT 25 (see the table above; DO NOT REORDER) ----------
    virtual void        Construct(ModeManager* lpModeManager);                        // slot 0  vtbl+0

    // slot 1  vtbl+4. Folded leaf 0x8284CB38 (`blr`) in ALL FIFTEEN concrete vtables -- the console
    // body is empty. Spelled `Destruct()` after the DWARF (BrnGameMode.cpp:105) rather than as a
    // real destructor, so the slot exists between Construct and PreWorldUpdate without dragging the
    // host compiler's implicit-destructor machinery into the layout.
    virtual void        Destruct();                                                   // slot 1  vtbl+4

    // slot 2  vtbl+8. SIX arguments: UpdateCurrentMode @0x82350EC8 dispatches
    // `(*(**(a1+3480)+8))(*(a1+3480), a2, a3, a8, a28, a30, a1+3504)`. The 4th argument is the
    // ACTIVE race-car output interface -- GameMode::PreWorldUpdate @0x8232FB20 walks a5 through
    // RCEntityActiveRaceCarOutputInterface::IsRaceCarActive and fires that header's own asserts
    // ("mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT", :967).
    virtual void        PreWorldUpdate(GameStateModuleIO::OutputBuffer* lpOutput,
                                       const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                       const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpGlobalRaceCars,
                                       const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
                                       // NAME NOTE: lbPaused is borrowed from the ModeManager
                                       // caller's own argument map (BrnModeManager_UpdateMode.cpp
                                       // :26/:259 -- the stacked arg_5F byte it loads into r8).
                                       // GameMode's body never reads it, so on THIS side only the
                                       // position is proven, not the semantics.
                                       bool lbPaused,
                                       const ScoringSystem* lpScoringSystem);         // slot 2  vtbl+8

    virtual void        PostWorldUpdate(const GameStateModuleIO::PostWorldInputBuffer* lpInput); // slot 3  vtbl+12
    virtual void        Initialise();                                                 // slot 4  vtbl+16

    // slot 5  vtbl+20. Builds the mutable GameModeParams from the immutable StartGameModeParams +
    // rank/event data. ALL FIFTEEN concrete modes override it and there is no GameMode::Start
    // export, so the base body is a link-hole closer (see the .cpp), not a recovered console body.
    virtual void        Start(const StartGameModeParams* lpStartGameModeParams,
                              GameModeParams* lpGameModeParams,
                              ScoringSystem* lpScoringSystem);                        // slot 5  vtbl+20

    virtual const char* GetName() const;                                              // slot 6  vtbl+24
    virtual CgsSystem::EFrameRateManagerType GetFrameRateType() const;                // slot 7  vtbl+28
    virtual f32         GetIntroDurationSeconds() const;                              // slot 8  vtbl+32
    virtual bool        HasTimedIntro() const;                                        // slot 9  vtbl+36  base TRUE
    virtual void        OnPlayerInShortCut();                                         // slot 10 vtbl+40  base empty
    virtual bool        ShouldCountdownEnd() const;                                   // slot 11 vtbl+44  base TRUE
    virtual void        SendEvent(EGameModeEvent leEvent);                            // slot 12 vtbl+48
    virtual bool        ShouldExit(const ScoringSystem* lpScoringSystem) const;       // slot 13 vtbl+52
    virtual bool        ShouldFinish(ScoringSystem* lpScoringSystem);                 // slot 14 vtbl+56  base FALSE
    virtual void        FillInGameModeSpecificResults(const ScoringSystem* lpScoringSystem,
                                                      GameStateModuleIO::FinishedModeAction* lpAction); // slot 15 vtbl+60
    virtual f32         GetOutroTimeout() const;                                      // slot 16 vtbl+64  base 0.1f
    virtual ::EGlobalRaceCarIndex GetGlobalRivalToShow() const;                       // slot 17 vtbl+68  base -1
    virtual ::EActiveRaceCarIndex GetActiveRivalToShow() const;                       // slot 18 vtbl+72  base -1
    virtual void        PlayerHasSpawned(::EActiveRaceCarIndex leActiveRaceCarIndex); // slot 19 vtbl+76  base empty
    virtual void        ProcessNewRoadScore(GameStateModuleIO::OutputBuffer* lpOutput,
                                            BrnStreetData::ChallengePlayerScoreEntry lScoreEntry,
                                            BrnStreetData::ScoreType leScoreType,
                                            BrnStreetData::ChallengeIndex lChallengeIndex,
                                            ::EActiveRaceCarIndex leActiveRaceCarIndex); // slot 20 vtbl+80  base empty
    virtual void        OnEnterRoad(BrnStreetData::RoadIndex lRoadIndex);             // slot 21 vtbl+84  base empty
    virtual void        HandleGameEvents(const CgsModule::Event* lpEvent, s32 liCount); // slot 22 vtbl+88  base empty
    virtual bool        RequiresStreaming() const;                                    // slot 23 vtbl+92  base TRUE
    virtual bool        HasLoadingScreen() const;                                     // slot 24 vtbl+96  base FALSE
    virtual void        OnPlayerUsesPaintShop();                                      // slot 25 vtbl+100 base empty
    // ---- END OF THE CONSOLE VTABLE ------------------------------------------------------------

    void    SetCurrentState(s32 liState);
    s32     CalculateMaxPlayerWrecks();
    bool    HasCountdownDisplayChanged(s32* lpiNewCountdownDisplay);

    // -- Inline accessors/setters used by the nested GameModeState states -----------------
    // None of these is separately X360-attested: the X360 build inlined them at every call
    // site (e.g. CountdownState::OnEnter reads mpModeManager off the mode; IntroState/OutroState
    // ::Update read mfTimeStepSeconds; CountdownState::OnLeave sets mbVisibleCars). De-inlined
    // back to their logical accessor form here, matching the DWARF declarations.
    ModeManager* GetModeManager() const           { return mpModeManager; }
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

    s32       meCurrentState;           // console +40; GameMode::Construct `*(a1+40) = -1`

    // The eight by-value contained states, IN E_GMS_* ORDER (see THE CONTAINED STATES table above
    // for each member's console offset, size and evidence). GameMode::Construct Construct()s each
    // one and Appends its address into maGameModeStates, so array slot i IS &(the i'th member here).
    // DO NOT REORDER: the order IS the enum, and the enum is what SetCurrentState indexes with.
    CountdownState      mCountdownState;      // console +0x2C, 20B -- E_GMS_COUNTDOWN      (0)
    IntroState          mIntroState;          // console +0x40, 20B -- E_GMS_INTRO          (1)
    InProgressState     mInProgressState;     // console +0x54, 12B -- E_GMS_IN_PROGRESS    (2)
    OutroState          mOutroState;          // console +0x60, 16B -- E_GMS_OUTRO          (3)
    ResultsState        mResultsState;        // console +0x70, 12B -- E_GMS_RESULTS        (4)
    QuitState           mQuitState;           // console +0x7C, 12B -- E_GMS_QUIT           (5)
    OnlineLoadingState  mOnlineLoadingState;  // console +0x88, 12B -- E_GMS_ONLINE_LOADING (6)
    OnlineSplashState   mOnlineSplashState;   // console +0x94, 12B -- E_GMS_ONLINE_SPLASH  (7)

    // ===============================================================================================
    // THE CONSOLE +160..+179 RUN -- X360 ASM BEATS THE PS3 DWARF HERE.
    // The DecFIGS DWARF lists this run as {mpModeManager, mfTimeStepSeconds, mbIsOnline, mbFinished,
    // mbTimerStartRequested, mbShowResultsRequested, mbIntroJustFinished, mbCountdownJustFinished,
    // mbCountdownDisplayChanged, miCountdownDisplay, mbVisibleCars} -- i.e. it puts the s32
    // miCountdownDisplay AFTER the bool block. The X360 asm puts it BEFORE, and pins every byte:
    //   +160 mpModeManager             GameMode::Construct @0x8232FA00 `stw r11, 0xA0(r31)`
    //   +164 mfTimeStepSeconds         UpdateCurrentMode `*(*(a1+3480)+164) = v40`
    //   +168 miCountdownDisplay (s32)  HasCountdownDisplayChanged @0x82311398 `lwz r11,0xA8(r31);
    //                                  stw r11,0(r30)`; Initialise @0x82315A70 `stw r10(-1),0xA8(r31)`
    //   +172 mbIsOnline                OfflineGameMode::Construct @0x8232FE78 `li r11,0; stb r11,0xAC`
    //                                  vs OnlineGameMode::Construct @0x8232FEB4 `li r11,1; stb r11,0xAC`.
    //                                  The three ONLINE Start bodies re-assert it (OnlineShowtime
    //                                  @0x823222A0, OnlineStuntRun @0x82339EA4, OnlineFreeBurnLobby
    //                                  @0x82322360); NO offline mode's Start touches +0xAC. It is also
    //                                  what ModeManager::IsOnlineGameMode and ProcessEvent's
    //                                  online-vs-offline stunt-scorer pick read.
    //   +173 mbFinished                UpdateCurrentMode `if (*(*(a1+3480)+173))` -> ExitCurrentMode
    //   +174 mbTimerStartRequested     UpdateCurrentMode read-then-clear
    //   +175 mbShowResultsRequested    OnlineGameMode::SendEvent @0x8232FFA4 `li r11,1; stb r11,0xAF`
    //                                  on entering state 3; GameMode::Construct zeroes 0xAF
    //   +176 mbIntroJustFinished       UpdateCurrentMode read-then-clear -> StopModeIntro;
    //                                  GameMode::Construct zeroes 0xB0
    //   +177 mbCountdownJustFinished   UpdateCurrentMode read-then-clear -> action 33
    //   +178 mbCountdownDisplayChanged HasCountdownDisplayChanged @0x82311364 tests 0xB2, then clears it
    //   +179 mbVisibleCars             GameMode::PreWorldUpdate `*(result+179)=0` then recompute;
    //                                  GameMode::ShouldExit @0x82315BA8 `lbz r11,0xB3(r3)` picks the
    //                                  3.0f-vs-10.0f stationary threshold from it
    // Initialise (0x82315A28) clears exactly 173,174,175,176,177,178 and sets 168 = -1.
    //
    // => ALL EIGHT BYTES 172..179 ARE CLAIMED by a proven reader AND writer. There is no console home
    //    in this stretch for `mbConstructed` or `mbFinalStandingsShown`: +180/+184 belong to
    //    OfflineGameMode (mbDebugAlwaysRaceToSingleLocation / miDebugDesignIndexOfLandmarkToAlwaysRaceTo,
    //    written by OfflineGameMode::Construct @0x8232FE7C/0x8232FE80). Both of those names were
    //    mis-transcriptions -- of the +172 store (really mbIsOnline) and the +175 store (really
    //    mbShowResultsRequested) -- and are RETIRED by this fix round. If a real writer for either
    //    ever turns up, it gets its own byte with its own citation; do not re-interleave them here.
    // ===============================================================================================
    ModeManager* mpModeManager;         // +160  owning ModeManager, set by GameMode::Construct
    f32       mfTimeStepSeconds;        // +164  per-frame update step (read via GetUpdateTimeStep)
    s32       miCountdownDisplay;       // +168  current countdown value handed back to the GUI
    bool      mbIsOnline;               // +172  true for the online mode family
    bool      mbFinished;               // +173
    bool      mbTimerStartRequested;    // +174
    bool      mbShowResultsRequested;   // +175
    bool      mbIntroJustFinished;      // +176
    bool      mbCountdownJustFinished;  // +177
    bool      mbCountdownDisplayChanged;// +178  set when miCountdownDisplay changes; one-shot
    bool      mbVisibleCars;            // +179  a rival is currently within visible range
};
}
