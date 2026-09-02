#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                   // Vector3, CgsID
#include "GameShared/GameClasses/Containers/CgsArray.h"       // Array<T, N>
#include "GameSource/GameState/BrnGameStateSharedIO.h"        // GameStateModuleIO::EGameModeType
#include "GameSource/GameState/BrnCheckpointData.h"           // BrnGameState::CheckpointData (real, single owner)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"   // BrnNetwork::NetworkPlayerID
#include "SharedClasses/Progression/BrnRaceEventData.h"       // BrnProgression::RaceEventData (real, single owner)
#include "SharedClasses/Progression/BrnProgressionRankData.h" // BrnProgression::ProgressionRankData (real, single owner)

// =============================================================================
// BrnGameModeParams.h  (MERGED OWNING HEADER)
//
// Consolidated from the GameModeParams worker (full DWARF-faithful layout of
// BrnGameState::GameModeParams + StartLocation + EGameModeStartMechanism) and the
// RaceMode worker (StartGameModeParams facade + the GameModeParams setters/accessors
// RaceMode::Start uses + BrnProgression::ProgressionRankData / RaceEventData + the
// ScoringSystem forward decl). Where the two overlapped (EGameModeStartMechanism,
// LightTriggerId, the EGameModeType enum, the KU_FLAG_* table) the single authoritative
// copy from the full-layout version is kept and the facade's duplicate dropped.
//
// The layouts and member names/types are taken from the DecFIGS DWARF for this exact
// source path (references/DecFIGS/dwarfdump/.../BrnGameModeParams.h).
//
// METHOD DECLARATIONS ARE GATED ON THE X360 LEDGER. The X360 2007-02 ARTIST build attests
// only six GameModeParams methods as standalone functions (Construct, GetCheckpointCount,
// GetFlag, GetStartDirection, GetStartLocationCount, GetStartPosition). The Set* mutators /
// GetNumRivals that RaceMode::Start drives were INLINED in the X360 build (they have no own
// address in the ledger), so they are modelled here as inline accessors over the named
// members -- not as further attested functions. The remaining ~30 DWARF methods are PS3-only
// drift and are deliberately left out (see "DWARF supplies names; the X360 ledger decides what
// exists" in AGENTS.md).
//
// LAYOUT NOTE: the data members follow the DWARF member list in source order (DWARF
// authoritative for names/types). Exact byte offsets are NOT X360-faithful on the x64 PC gate
// (Vector3 is a 16-byte SIMD type and the array element strides differ from the console) --
// semantic parity is by named members, not by byte offset (AGENTS.md).
// =============================================================================

// ScoringSystem is the third (unused) RaceMode::Start parameter (DWARF: ScoringSystem*).
// RaceMode::Start never dereferences it, so an incomplete type is sufficient. Real home: the
// scoring-system TU. Declared at BrnGameState scope below.

// ---------------------------------------------------------------------------------------------
// BrnProgression::ProgressionRankData -- THE STAND-IN IS RETIRED (stuntrace waveB MOUNT-CLOSURE
// round, 2026-08-26). What stood here was a member-less, body-less class with eleven declared
// accessors and no layout; eleven of the 63 unresolved externals the wave-B event-core mount
// measured were exactly those accessors, and nothing in the tree could ever have defined them
// because no TU knew where a single one of its fields lived.
//
// The record now has ONE real owner: SharedClasses/Progression/BrnProgressionRankData.h
// (#included above). It carries the full DWARF-faithful 112-byte layout with a sizeof pin, and
// every one of the eleven accessors is an INLINE body over its named member -- which is the
// faithful shape, because the X360 ledger attests no standalone symbol for any of them (they
// were header-inline on the console; RaceMode::Start reads rank+0x00 / +0x24 / +0x5C with bare
// `lfs` / `lbz` and no call). The owning header states all four layout witnesses.
//
// Per-event progression record. RaceMode::Start reads the start/add rival counts out of it.
// The former 2-method RaceEventData stub here is RETIRED for the same reason: BrnProgression::
// RaceEventData has a single complete owner (SharedClasses/Progression/BrnRaceEventData.h,
// included above), which declares GetStartRivalCount/GetAddRivalCount among its attested API.
// (ODR -- one owner; same pattern as the retired CheckpointData stub above.)
// ---------------------------------------------------------------------------------------------

// Forward decl for StartGameModeParams::mpPlayerCarVehicleListEntry / SetPlayerVehicleGamePlayData.
// Real home: BrnResource vehicle-list TU. Used only by-pointer here.
namespace BrnResource { struct VehicleListEntry; }

namespace BrnGameState
{
// ---------------------------------------------------------------------------
// Minimal stubs for dependency types that are not yet reconstructed in b5-decomp/src.
// These stand in for the real owning headers so the value types have a complete layout
// under the compile gate; the consolidation step swaps in the reconstructed types. Each is
// listed in deps_stubbed. (EGameModeType is NOT stubbed -- the real GameStateModuleIO enum
// from BrnGameStateSharedIO.h is wired in below.)
// ---------------------------------------------------------------------------
// NOTE: LandmarkIndex is NOT defined here. It used to be a `typedef u32 LandmarkIndex`
// stub at this spot, but it is a real wrapper class now owned by
// GameSource/GameState/BrnGameStateTypes.h (the GameStateTypes reconstruction). This
// header never uses LandmarkIndex in any member, so the stub is simply removed rather
// than re-pointed; consumers that need the handle (e.g. BrnOfflineGameMode) include the
// owning header directly. Do not re-add a stub here — there is one owner.
typedef u32 LightTriggerId;                 // stub: traffic-light trigger handle

enum EDistrict_Stub          { E_DISTRICT_STUB = 0 };           // stub: BrnWorld::EDistrict
enum ERouteFindingStyle_Stub { E_ROUTEFINDINGSTYLE_STUB = 0 };  // stub: BrnAI::ERouteFindingStyle
enum EAISpeedSelMethod_Stub  { E_AISPEEDSEL_STUB = 0 };         // stub: BrnAI::EAISpeedSelectionMethod
enum EAStarDistFunc_Stub     { E_ASTARDIST_STUB = 0 };          // stub: BrnAI::AStarDistanceFunction
enum EGlobalRaceCarIndex_Stub { E_GLOBALRACECARINDEX_STUB = 0 };// stub: EGlobalRaceCarIndex
enum EBoostType_Stub         { E_BOOSTTYPE_STUB = 0 };          // stub: BrnNetwork::EBoostType
enum EPlayerTeam_Stub        { E_PLAYERTEAM_STUB = 0 };         // stub: GameStateModuleIO::EPlayerTeam


// CheckpointData / OpponentData are reconstructed by their own TUs; only their sizes matter
// for the array members embedded by value here. Modelled as opaque fixed-size blobs so
// GameModeParams keeps a complete layout. Replaced at consolidation with the real types.
// CheckpointData stub retired: the real BrnGameState::CheckpointData is now its single owner
// (BrnCheckpointData.h, included above; 44-byte stride preserved, so GameModeParams' layout is unchanged).
struct OpponentData_Stub   { u8 mBlob[48]; }; // stub: BrnGameState::OpponentData (own TU)

// Forward-only handle for the third RaceMode::Start parameter (DWARF: ScoringSystem*).
class ScoringSystem;

// ---------------------------------------------------------------------------
// EGameModeStartMechanism - owned by this header (DWARF BrnGameModeParams.h:41). How a mode
// hands control to the player at the start line.
// ---------------------------------------------------------------------------
enum EGameModeStartMechanism
{
    E_GAMEMODESTARTMECHANISM_DEFAULT               = 0,
    E_GAMEMODESTARTMECHANISM_SPIN_WHEELS_ANYWHERE  = 1,
    E_GAMEMODESTARTMECHANISM_SPIN_WHEELS_AT_LIGHTS = 2,
    E_GAMEMODESTARTMECHANISM_OVERTAKE_RIVAL        = 3,
    E_GAMEMODESTARTMECHANISM_COUNT                 = 4
};

// ---------------------------------------------------------------------------
// StartLocation - owned by this header (DWARF BrnGameModeParams.h:55). A single grid slot:
// where a car spawns and which way it faces. 32 bytes on the console (two 16-byte SIMD
// vectors), confirmed by Array<StartLocation,8>::Ge returning (32 * index + base).
// ---------------------------------------------------------------------------
struct StartLocation
{
    Vector3 mPosition;
    Vector3 mDirection;
};

// ===========================================================================
// GameModeParams - the per-event parameter block the mode manager fills in and hands to the
// world/AI/traffic modules. A plain value type (no vtable).
// ===========================================================================
class GameModeParams
{
public:
    // -- Flag bits (DWARF BrnGameModeParams.h:344-379). 64-bit flag word. --
    static const u64 KU_FLAG_SET_CARS_TO_START_GRID            = 0x1ull;
    static const u64 KU_FLAG_REMOVE_RIVALS_FROM_WORLD          = 0x2ull;
    static const u64 KU_FLAG_DISABLE_CRASH_CLEAN_UP            = 0x4ull;
    static const u64 KU_FLAG_ENABLE_EASY_CRASHING              = 0x8ull;
    static const u64 KU_FLAG_PLAYER_MUST_BE_CRASHING           = 0x10ull;
    static const u64 KU_FLAG_WRAP_AI_CARS_WHEN_OUT_OF_RANGE    = 0x20ull;
    static const u64 KU_FLAG_DISABLE_TRAFFIC_SWERVING          = 0x40ull;
    static const u64 KU_FLAG_SET_DIRECTOR_TO_CRASH_MODE_AFTER_INTRO = 0x80ull;
    static const u64 KU_FLAG_ALLOW_CRASH_PLAY_CONTROLS         = 0x100ull;
    static const u64 KU_FLAG_USE_SHOWTIME_VEHICLE_BEHAVIOUR    = 0x200ull;
    static const u64 KU_FLAG_CAR_SELECT_ALLOWED                = 0x400ull;
    static const u64 KU_FLAG_CLEAR_NEARBY_TRAFFIC              = 0x800ull;
    static const u64 KU_FLAG_HARDCORE_TRAFFIC_SWERVING         = 0x1000ull;
    static const u64 KU_FLAG_HAS_ROUTE                         = 0x2000ull;
    static const u64 KU_FLAG_AI_DRIVE_BY_START                 = 0x4000ull;
    static const u64 KU_FLAG_USE_RACE_BALANCING                = 0x8000ull;
    static const u64 KU_FLAG_SET_ALL_CARS_TO_STARTING_AI_CONTROL = 0x10000ull;
    static const u64 KU_FLAG_DISABLE_TRAFFIC_RESET             = 0x20000ull;
    static const u64 KU_FLAG_RAPID_CRASHES                     = 0x40000ull;
    static const u64 KU_FLAG_TRAFFIC_CHECKING_NOT_ALLOWED      = 0x80000ull;
    static const u64 KU_FLAG_DISABLE_UPCOMING_ROAD_SIGNS       = 0x100000ull;
    static const u64 KU_FLAG_DISABLE_AFTERTOUCH_TDS            = 0x200000ull;
    static const u64 KU_FLAG_DISABLE_ALL_TDS                   = 0x400000ull;
    static const u64 KU_FLAG_DISABLE_CRASH_EXTENSIONS          = 0x800000ull;
    static const u64 KU_FLAG_LIMITED_CRASH_EXTENSIONS          = 0x1000000ull;
    static const u64 KU_FLAG_SHORT_CRASH_TIME                  = 0x2000000ull;
    static const u64 KU_FLAG_ROLLING_START                     = 0x4000000ull;
    static const u64 KU_FLAG_DONUT_START                       = 0x8000000ull;
    static const u64 KU_FLAG_EASY_SMASH_PROPS                  = 0x10000000ull;
    static const u64 KU_FLAG_USES_NAVIGATION                   = 0x20000000ull;
    static const u64 KU_FLAG_AI_PERSISTENT_DAMAGE              = 0x40000000ull;
    static const u64 KU_FLAG_AI_RESET_ON_TRACK_BEHIND          = 0x80000000ull;
    static const u64 KU_FLAG_DISABLE_PROP_PROGRESSION          = 0x100000000ull;
    static const u64 KU_FLAG_ENFORCE_SOFT_TAKEDOWNS            = 0x200000000ull;
    static const u64 KU_FLAG_SET_OPPONENTS_TO_COPS             = 0x400000000ull;
    static const u64 KU_FLAG_ALLOW_REVENGE_TAKEDOWNS           = 0x800000000ull;

    // The DWARF spells the embedded fixed arrays as Array<T, N>.
    typedef Array<StartLocation, 8u>        StartLocationArray;
    typedef Array<CheckpointData, 16u>      CheckpointDataArray;
    typedef Array<OpponentData_Stub, 7u>    OpponentDataArray;

    // ---- X360-attested standalone methods (the only six in the ledger) -----------------
    // Construct: reset every field to its "no event" default. Takes the real
    // GameStateModuleIO::EGameModeType (wired in from BrnGameStateSharedIO.h; RaceMode::Start
    // threads StartGameModeParams::GetGameModeType() straight into it).
    void    Construct(GameStateModuleIO::EGameModeType leGameModeType);

    s32     GetCheckpointCount() const;                        // # checkpoints registered

    // ===========================================================================================
    // [stuntrace waveB fix round, 2026-08-26] THE CHECKPOINT PAIR. This class published only
    // GetCheckpointCount() while its SIBLING StartGameModeParams published AddCheckpoint (:369) and
    // GetCheckpointData (:392) over the identical Array<CheckpointData,16>. That asymmetry is what
    // parked SetUpCheckPointsForGameMode's publish leg AND SetupPathfinding's block-section copy AND
    // all of SetupCheckpointDistricts -- so a RACE / MARKED_MAN / BURNING_ROUTE event starts with an
    // EMPTY checkpoint array today. Both are DWARF-attested for GameModeParams itself (dwarfdump
    // .../BrnGameModeParams.h:388 and :395/:399) and X360-INLINED at every call site, hence
    // declare-only here like the rest of this class:
    //   * AddCheckpoint -- ModeManager::SetUpCheckPointsForGameMode @0x82328BC8 emits it as
    //     CheckpointData::Construct + Array<CheckpointData,16>::Append @0x82317B30 on params+0x260.
    //   * GetCheckpointData -- Array<CheckpointData,16>::GetIt @0x8231A7D8 on params+0x260; that
    //     Array helper OWNS the CgsArray.h:336/:338 constructed/bounds asserts, so call sites must
    //     NOT restate them. Also asm-pinned from ModeManager::Construct's
    //     `Array<CheckpointData,16>::GetItem(this+34272, 0)` @0x82340858 (mCurrentGameModeParams+608
    //     == +0x260, bounds-checked against the count word at base+704 == 16*44).
    // [!] GetCheckpoints() (the whole-array form, StartGameModeParams' :393 twin) is deliberately
    // NOT added: CheckpointDataArray is a PRIVATE typedef on this class, so a public accessor
    // returning it would be unusable by name at any call site. Promote the typedef first if a
    // consumer really needs the array rather than an element.
    void                  AddCheckpoint(LandmarkIndex luLandmarkIndex, u16 luAISectionIndex);
    const CheckpointData* GetCheckpointData(s32 liIndex) const;
    CheckpointData*       GetCheckpointData(s32 liIndex);
    // ===========================================================================================
    // [gateui r8 link fix] GetFlag @0x821F2C88 is `ld r11,0x860(r3); and; cntlzw-normalise` --
    // a pure muFlags bit test. Its out-of-line body lives only in an UNMOUNTED (and currently
    // non-compiling) TU, and the round-8 PREPARE_FOR_MODE re-route in the MOUNTED
    // WorldBridgeInputToEntityModules.cpp now calls it -> LNK2019 without this inline
    // (measured by the r8 verifier). Inline body per the console semantics; fold back into the
    // full TU when it mounts.
    bool    GetFlag(u64 luFlag) const                   { return (muFlags & luFlag) != 0; }
    Vector3 GetStartPosition(s32 liStartLocationIndex) const;  // spawn pos for grid slot
    Vector3 GetStartDirection(s32 liStartLocationIndex) const; // facing for grid slot
    s32     GetStartLocationCount() const;                     // # start-grid slots registered

    // [stuntrace waveB fix round, 2026-08-26] DWARF BrnGameModeParams.h:577. X360-INLINED at
    // every call site (no standalone export): ModeManager::SetStartingGrid @0x82328608 emits it
    // as "build a 32-byte StartLocation{lPosition, lDirection} and Array<StartLocation,8>::Append
    // @0x823287D0 it onto maStartLocations (console this+0x150)", guarded by
    //     CGS_ASSERT( BrnMath::IsNormal( lDirection ), "BrnMath::IsNormal( lDirection )" )
    // whose baked file/line is BrnGameModeParams.h:1168 (asm `li r5,0x490` @0x823287B4) -- i.e.
    // the guard belongs to THIS body, and SetStartingGrid carries it only because the console
    // inlined it there. maStartLocations is private and there is no other public mutator, so
    // without this method SetStartingGrid cannot seat a single car.
    //
    // [!] BANNER CORRECTED 2026-08-26 (wave-B CLOSURE round) -- IT WAS FALSE, AND THE FALSEHOOD
    // WAS LOAD-BEARING. The committed text justified leaving this declare-only as "matching every
    // other method on this class (Construct / GetCheckpointCount / GetStartPosition /
    // GetStartDirection / GetStartLocationCount are all declare-only here)". All five of those
    // are BODIED, in BrnGameModeParams.cpp:44 / :108 / :129 / :139 / :117 -- so the cited
    // precedent did not exist and the only method on this class without a body was this one, i.e.
    // the sole LNK2019 on the seat-the-cars path. Bodied now at BrnGameModeParams.cpp (see the
    // instruction-for-instruction derivation there).
    void    AddStartLocation(Vector3 lPosition, Vector3 lDirection);

    // ---- Inlined-in-X360 mutators/accessors used by RaceMode::Start --------------------
    // These had no standalone address in the X360 ledger (the compiler inlined them at the
    // call site); modelled here as trivial inline methods over the named members. (de_inlined)
    void SetTrafficDensityScale(f32 lfScale)            { mfTrafficDensityScale = lfScale; }
    void SetLargeVehicleProbability(f32 lfProbability)  { mfLargeVehicleProbability = lfProbability; }
    void SetNumRivals(s32 liNumRivals)                  { miNumRivals = static_cast<s8>(liNumRivals); }
    void SetProgressionRankAsRatio(f32 lfRatio)         { mfProgressionRankAsRatio = lfRatio; }
    void SetStartMechanism(EGameModeStartMechanism leM) { meStartMechanism = leM; }
    void SetTrafficLightTriggerId(LightTriggerId luId)  { mTrafficLightTriggerId = luId; }
    void SetFlag(u64 luFlags)                           { muFlags |= luFlags; }
    s32  GetNumRivals() const                           { return miNumRivals; }

    // [stuntrace start-grid wave, 2026-08-27] The WHOLE 64-bit word, not a bit test. The
    // console copies it wholesale in RaceCarEntityModule::HandlePrepareForModeAction
    // @0x82309480 -- `ld r11, 0x860(r25)` / `stdx r11, r31, 0x18358` -- into the world module's
    // own mxGameModeFlags mirror, which SetUpPlayerCarForMode @0x823058F8 and SetupOpponents
    // @0x82307DF0 then read back through RaceCarEntityModule::GetGameModeFlag @0x822A3A20.
    // Same (de_inlined) treatment as SetFlag/GetFlag above; muFlags is private and there is no
    // other way to express that one instruction by name.
    u64  GetFlags() const                               { return muFlags; }

    // [road-rage wave 2026-09-02] setters RoadRageMode::Start @0x82330678 inlines (DWARF :438-450/:483;
    // `li r8,5 / li r11,2` -> stw 0x840/0x844/0x848/0x84C @0x823309AC-BC, `stw r3,0x858` @0x823309D0):
    void SetDefaultPlayerRouteFindingStyle(ERouteFindingStyle_Stub leStyle) { meDefaultPlayerRouteFindingStyle = leStyle; }
    void SetDefaultAIRouteFindingStyle(ERouteFindingStyle_Stub leStyle)     { meDefaultAIRouteFindingStyle = leStyle; }
    void SetAISpeedSelectionMethod(EAISpeedSelMethod_Stub leMethod)         { meAISpeedSelectionMethod = leMethod; }
    void SetAIAggresiveCarCount(s32 liCount)                                { miAIAggressiveCarCount = liCount; }   // sic, DWARF spelling
    void SetPlayerWreckCount(s32 liWreckCount)                              { miPlayerWreckCount = liWreckCount; }

    // ===========================================================================================
    // [!!] OFFSET RUN CORRECTED 2026-08-26 (wave-B fix round) -- THIS WAS A LIVE DEFECT, not a
    // comment tidy. The committed block here read the run as
    //     mfOnlineModeTimeLimit(+0x854), meAStarDistanceFunction(+0x858), miPlayerWreckCount(+0x85C)
    // and bodied `GetAStarDistanceFunctionRaw()` over the +0x858 member so that
    // ScoringSystem::OnModeStart's road-rage crash target was fed the A* DISTANCE FUNCTION. The run
    // is ONE SLOT LOW. Four asm facts, all re-dumped this pass, and they cannot all be satisfied by
    // the old reading:
    //   (a) ModeManager::SetupPathfinding @0x823291B0 STORES the A*-type identity {0,1,2} as an
    //       INTEGER to +0x854 (`li r11,1 / li r11,0 ... stw r11, 0x854(r29)` @0x82329250). A float
    //       time limit cannot be the destination of that store.
    //   (b) ModeManager::UpdateCurrentMode @0x82350EC8 LOADS +0x850 as a FLOAT (`lfs f0, 0x850(r30)`
    //       @0x823512FC) immediately before firing the assert whose literal is
    //       "lpGameModeParams->GetOnlineTimeLimit() > 0.0f", and re-loads it at 0x82351324 as the
    //       value it passes on. So mfOnlineModeTimeLimit IS +0x850.
    //   (c) GameModeParams::Construct @0x8231C370 writes 0 to 0x840/0x844/0x848/0x84C/0x854/0x858
    //       and NEVER to 0x850 -- exactly right if 0x850 is the online-only time limit (set by the
    //       online mode's Start, not by the generic reset) and 0x854/0x858 are the two resettable
    //       integers.
    //   (d) ScoringSystem::OnModeStart @0x82338220 does `lwz r11, 0x858(r29)` @0x823382E4 ->
    //       `stw r11, 0x4B58(r31)` == miMaximumPlayerCrashedNumber. Under the corrected run that
    //       word is miPlayerWreckCount -- "how many wrecks the player is allowed" feeding "the max
    //       number of player crashes", which is the reading that actually makes sense.
    // CORRECTED RUN: mfOnlineModeTimeLimit(+0x850), meAStarDistanceFunction(+0x854),
    //                miPlayerWreckCount(+0x858), muFlags(+0x860, fixed by GetFlag's `ld 0x860`).
    // `GetAStarDistanceFunctionRaw()` is therefore RETIRED (it named the wrong member for its one
    // consumer) and replaced by the two honest accessors below. All three are X360-INLINED (no
    // standalone export), so inline bodies are the faithful form.
    // ===========================================================================================

    // DWARF BrnGameModeParams.h:479. The console's road-rage / marked-man crash allowance; the ONE
    // consumer is ScoringSystem::OnModeStart's `lwz r11, 0x858` -> miMaximumPlayerCrashedNumber.
    s32  GetPlayerWreckCount() const                    { return miPlayerWreckCount; }

    // The console assert string names this accessor verbatim: "lpGameModeParams->GetOnlineTimeLimit()
    // > 0.0f" (BrnModeManager.cpp:2010), fired by UpdateCurrentMode's mode-13 timer-start leg
    // immediately after `lfs f0, 0x850(r30)`.
    f32  GetOnlineTimeLimit() const                     { return mfOnlineModeTimeLimit; }

    // DWARF BrnGameModeParams.h:458 `void SetAStarDistanceFunction(BrnAI::AStarDistanceFunction)`.
    // Typed s32 here ONLY because the member is still the EAStarDistFunc_Stub placeholder -- re-type
    // both together when BrnAI::AStarDistanceFunction (BrnAStar.h:47) is wired in. X360-INLINED:
    // ModeManager::SetupPathfinding @0x823291B0 emits it as `stw r11, 0x854(lpGameModeParams)`.
    void SetAStarDistanceFunction(s32 leAStarDistanceFunction)
    { meAStarDistanceFunction = static_cast<EAStarDistFunc_Stub>(leAStarDistanceFunction); }

    // ⭐ [gateui] ADDED 2026-08-20 (round 8). DWARF-attested method (BrnGameModeParams.h:417,
    // `EGameModeType GetGameModeType() const`), inlined by the X360 compiler at every call site,
    // so it has no standalone export -- same shape as the accessors above.
    //
    // THE MEMBER IS PINNED BY THE ASM, not by DWARF source order:
    //   GameModeParams::Construct @0x8231C370, first instruction that touches `this`:
    //       0x8231C374  stw  r4, 0x148(r3)      <- the EGameModeType argument -> +0x148
    //   (the neighbouring word is a different field: 0x8231C414 `stw r11, 0x144(r3)` zeroes it.)
    // and the ONE consumer this wave cares about reads it through the embedded copy inside
    // PrepareForModeAction (mGameModeParams @ action+0x30, so member+0x30 == 0x178):
    //   WorldModule::BridgeInputToEntityModules @0x827ADF88, prepare-for-mode case:
    //       0x827AE8E8  lwz   r11, 0x178(r31)   ; r31 = the action record
    //       0x827AE8EC  cmpwi cr6, r11, 0xA     ; SIGNED -- E_MODE_NONE is -1
    //       0x827AE8F4  bge   -> "this is an online mode"
    // 0x178 - 0x30 == 0x148 == the offset Construct stores to, which is what identifies the two
    // as the same member. NOTHING below indexes by offset; the host layout is by named member
    // (the x64 divergence is deliberate -- see the header banner and BrnGameActions.h).
    GameStateModuleIO::EGameModeType GetGameModeType() const { return meGameModeType; }

public:
    // ---- Data members (DWARF source order, BrnGameModeParams.h:503-573) -----
    s8                      miNumRivals;
    s8                      miNumNetworkPlayers;
    f32                     mfProgressionRankAsRatio;
    BrnNetwork::NetworkPlayerID maNetworkPlayerID[8];
    CgsID                   mSpecialEventCarId;
    f32                     mfTrafficDensityScale;
    f32                     mfLargeVehicleProbability;
    f32                     mfTrafficSpeedScale;
    EGameModeStartMechanism meStartMechanism;
    LightTriggerId          mTrafficLightTriggerId;
    u32                     muEventJunctionID;
    u32                     muJunctionID;
    s32                     miRoadRageThreshold;
    s32                     miPursuitRivalTotalDamage;
    EGlobalRaceCarIndex_Stub mePursuedCarGlobalIndex;
    CgsID                   mPursuedCarID;
    f32                     mfOnlineFreeburnDeformationAmount;
    f32                     mfNeedForBronze;
    f32                     mfNeedForSilver;
    f32                     mfNeedForGold;
    f32                     mfModeTimeLimit;
    u8                      muDifficultyLevel;
    f32                     mfOvertakingDifficulty[8];
    bool                    mbIsOnline;
    CgsID                   maModelIds[8];
    u16                     mau16CarColourIndex[8];
    u16                     mau16CarPaintFinishIndex[8];
    EPlayerTeam_Stub        maePlayerTeam[8];
    BrnNetwork::NetworkPlayerID mLocalNetworkPlayerID;
    bool                    mbInfiniteBoost;
    EBoostType_Stub         meOnlineBoostStrategy;
    u32                     muNumberOfCheckpointsInEvent;

private:
    GameStateModuleIO::EGameModeType meGameModeType;
    StartLocationArray      maStartLocations;
    CheckpointDataArray     maCheckpointDataArray;
    OpponentDataArray       maOpponentData;
    ERouteFindingStyle_Stub meDefaultPlayerRouteFindingStyle;
    ERouteFindingStyle_Stub meDefaultAIRouteFindingStyle;
    EAISpeedSelMethod_Stub  meAISpeedSelectionMethod;
    s32                     miAIAggressiveCarCount;
    f32                     mfOnlineModeTimeLimit;
    EAStarDistFunc_Stub     meAStarDistanceFunction;
    s32                     miPlayerWreckCount;
    u64                     muFlags;
};

// The immutable event/start description handed to a game mode. DWARF :153 (:303-327). X360 word in
// [], byte offset in (); offsets NOT x64-faithful -- parity by member. The seven X360-attested
// standalone methods (Construct/AddCheckpoint/Set+GetTrafficLightTriggerId/SetProgressionRankData/
// SetProgressionRankAsRatio/SetPlayerVehicleGamePlayData) have bodies in BrnGameModeParams.cpp.
// [x] 2026-08-26 MOUNT-CLOSURE round: the fifteen READ accessors the mounted event core actually
// calls are now bodied in that same TU as well. They have no out-of-line X360 symbol (all inlined),
// so each is derived from consumer asm -- see the evidence banner above their block. The remaining
// declarations here (the Set* twins, GetPlayerPosition, GetCheckpoint*, GetPlayerVehicleGamePlayData,
// Get/SetPlayerBaseDeformation) are still declared-only: nothing mounted references them, and a
// body with no live consumer is a body with no way to check its member mapping.
class StartGameModeParams
{
public:
    typedef Array<CheckpointData, 16u> CheckpointDataArray;   // DWARF :99

    // ---- X360-attested standalone methods (this TU) ----
    void Construct(GameStateModuleIO::EGameModeType leGameModeType,
                   Vector3 lPlayerPosition, EGameModeStartMechanism leStartMechanism);   // 0x8231C1F8
    void AddCheckpoint(LandmarkIndex luLandmarkIndex, u16 luAISectionIndex);             // 0x8236AAC0
    void           SetTrafficLightTriggerId(LightTriggerId lTriggerId);                  // 0x823616E8
    LightTriggerId GetTrafficLightTriggerId() const;                                     // 0x8231C2D8
    void SetProgressionRankData(const BrnProgression::ProgressionRankData* lpProgressionRankData); // 0x82354490
    void SetProgressionRankAsRatio(f32 lfProgressionRankAsRatio);                        // 0x823544F0
    void SetPlayerVehicleGamePlayData(const BrnResource::VehicleListEntry* lpPlayerCarVehicleListEntry); // 0x82354590

    // ---- Declared-only accessors (consumed by committed RaceMode::Start; compile-only gate) ----
    GameStateModuleIO::EGameModeType                  GetGameModeType() const;
    void                                              SetRaceId(CgsID lId);
    CgsID                                             GetRaceId() const;
    void                                              SetPlayerPosition(Vector3 lPlayerPosition);
    void                                              SetStartDirection(Vector3 lStartDirection);
    Vector3                                           GetPlayerPosition() const;
    Vector3                                           GetStartDirection() const;
    EGameModeStartMechanism                           GetStartMechanism() const;
    void                                              SetTrafficDensity(f32 lfTrafficDensity);
    f32                                               GetTrafficDensity() const;
    void                                              SetBoostEarning(f32 lfBoostEarning);
    f32                                               GetBoostEarning() const;
    void                                              SetShotGroup(s32 liShotGroup);
    s32                                               GetShotGroup() const;
    s32                                               GetCheckpointCount() const;
    const CheckpointData*                             GetCheckpointData(s32 liIndex) const;
    const CheckpointDataArray*                        GetCheckpoints() const;
    s32                                               GetTakedownTarget() const;
    void                                              SetTakedownTarget(s32 liTakedownTarget);
    void                                              SetPursuedCarGlobalIndex(EGlobalRaceCarIndex_Stub lePursuedCarIndex);
    EGlobalRaceCarIndex_Stub                          GetPursuedCarGlobalIndex() const;
    void                                              SetPursuedCarID(CgsID lId);
    CgsID                                             GetPursuedCarID() const;
    void                                              SetEventJunctionId(u32 luEventJunctionId);
    u32                                               GetEventJunctionId() const;
    void                                              SetEventData(const BrnProgression::RaceEventData* lpEventData);
    const BrnProgression::RaceEventData*              GetEventData() const;
    const BrnProgression::ProgressionRankData*        GetProgressionRankData() const;
    void                                              SetJunctionID(u32 luJunctionID);
    u32                                               GetJunctionID() const;
    const BrnResource::VehicleListEntry*              GetPlayerVehicleGamePlayData() const;
    f32                                               GetProgressionRankAsRatio() const;
    f32                                               GetPlayerBaseDeformation() const;
    void                                              SetPlayerBaseDeformation(f32 lfPlayerBaseDeformation);

private:
    // ---- Data members (DWARF source order :303-327) ----
    CheckpointDataArray                         maCheckpointDataArray;        // :303  [..176]  count @+704
    // [x] CORRECTED 2026-08-26: +712, not +708, and Construct DOES clear it. 708 is the word
    // index times four; a CgsID is 8-aligned so the member sits at 712, which is what
    // ModeManager::Start's `ld r11, 0x2C8(r30)` @0x8234FEB0 reads. Construct's clear is the
    // easily-missed `std r11(0), 0x2C8(r31)` @0x8231C274 (an 8-byte store, apart from the
    // scalar cluster). The old SUSPECT flag on this member is discharged.
    CgsID                                       miRaceId;                     // :304  [177] +712  (cleared by Construct)
    GameStateModuleIO::EGameModeType            meGameModeType;               // :305  [180] +720
    Vector3                                     mPlayerPosition;              // :306  [184] +736
    Vector3                                     mStartDirection;              // :307  [188] +752
    s32                                         miTakedownTarget;             // :308  [192] +768
    EGlobalRaceCarIndex_Stub                    mePursuedCarGlobalIndex;      // :309  [193] +772  (real EGlobalRaceCarIndex)
    // [x] CORRECTED 2026-08-26: Construct DOES clear it -- `std r11(0), 0x308(r31)` @0x8231C280,
    // the second of the two 8-byte stores the earlier pass read past. See BrnGameModeParams.cpp.
    CgsID                                       mPursuedCarID;                // :310  [194] +776  (cleared by Construct)
    EGameModeStartMechanism                     meStartMechanism;             // :311  [196] +784
    LightTriggerId                              mTrafficLightTriggerId;       // :312  [197] +788
    f32                                         mfTrafficDensity;             // :314  [198] +792
    f32                                         mfBoostEarning;               // :315  [199] +796
    s32                                         miShotGroup;                  // :317  [200] +800
    f32                                         mfPlayerBaseDeformation;      // :319  [201] +804
    u32                                         muEventJunctionId;            // :321  [202] +808  (NOT reset by Construct)
    const BrnProgression::RaceEventData*        mpEventData;                  // :322  [203] +812
    u32                                         muJunctionID;                 // :323  [204] +816
    const BrnProgression::ProgressionRankData*  mpProgressionRankData;        // :324  [205] +820
    f32                                         mfProgressionRankAsRatio;     // :325  [206] +824
    const BrnResource::VehicleListEntry*        mpPlayerCarVehicleListEntry;  // :327  [207] +828  (NOT reset by Construct)
};
}
