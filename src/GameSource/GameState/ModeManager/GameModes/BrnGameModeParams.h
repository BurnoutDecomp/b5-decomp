#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                   // Vector3, CgsID
#include "GameShared/GameClasses/Containers/CgsArray.h"       // Array<T, N>
#include "GameSource/GameState/BrnGameStateSharedIO.h"        // GameStateModuleIO::EGameModeType

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

namespace BrnProgression
{
// Per-rank tuning record. RaceMode::Start reads three values out of it. Real home:
// SharedClasses/Progression/BrnProgressionRankData.{h,cpp}. Only the accessors Start uses are
// declared; bodies + layout land with that TU.
class ProgressionRankData
{
public:
    f32 GetTrafficDensityRace() const;
    f32 GetLargeVehicleProbability() const;
    u32 GetRaceRivalsNumber() const;
};

// Per-event progression record. RaceMode::Start reads the start/add rival counts out of it.
// Real home: SharedClasses/Progression/BrnRaceEventData.{h,cpp}.
class RaceEventData
{
public:
    u8 GetStartRivalCount() const;
    u8 GetAddRivalCount() const;
};
}

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

struct NetworkPlayerID_Stub { u32 muValue; };               // stub: RoadRulesRecvData::NetworkPlayerID

// CheckpointData / OpponentData are reconstructed by their own TUs; only their sizes matter
// for the array members embedded by value here. Modelled as opaque fixed-size blobs so
// GameModeParams keeps a complete layout. Replaced at consolidation with the real types.
struct CheckpointData_Stub { u8 mBlob[44]; }; // stub: BrnGameState::CheckpointData (own TU)
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
    typedef Array<CheckpointData_Stub, 16u> CheckpointDataArray;
    typedef Array<OpponentData_Stub, 7u>    OpponentDataArray;

    // ---- X360-attested standalone methods (the only six in the ledger) -----------------
    // Construct: reset every field to its "no event" default. Takes the real
    // GameStateModuleIO::EGameModeType (wired in from BrnGameStateSharedIO.h; RaceMode::Start
    // threads StartGameModeParams::GetGameModeType() straight into it).
    void    Construct(GameStateModuleIO::EGameModeType leGameModeType);

    s32     GetCheckpointCount() const;                        // # checkpoints registered
    bool    GetFlag(u64 luFlag) const;                         // test flag bit(s) against muFlags
    Vector3 GetStartPosition(s32 liStartLocationIndex) const;  // spawn pos for grid slot
    Vector3 GetStartDirection(s32 liStartLocationIndex) const; // facing for grid slot
    s32     GetStartLocationCount() const;                     // # start-grid slots registered

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

public:
    // ---- Data members (DWARF source order, BrnGameModeParams.h:503-573) -----
    s8                      miNumRivals;
    s8                      miNumNetworkPlayers;
    f32                     mfProgressionRankAsRatio;
    NetworkPlayerID_Stub    maNetworkPlayerID[8];
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
    NetworkPlayerID_Stub    mLocalNetworkPlayerID;
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

// The immutable event/start description handed to a game mode. RaceMode::Start reads its
// event-data, rank-data, traffic density, start mechanism and traffic-light trigger out of it.
// Only the accessors Start uses are declared; the full class is reconstructed by its own TU.
class StartGameModeParams
{
public:
    GameStateModuleIO::EGameModeType           GetGameModeType() const;
    const BrnProgression::RaceEventData*       GetEventData() const;
    const BrnProgression::ProgressionRankData* GetProgressionRankData() const;
    f32                                        GetTrafficDensity() const;
    f32                                        GetBoostEarning() const;
    f32                                        GetProgressionRankAsRatio() const;
    EGameModeStartMechanism                    GetStartMechanism() const;
    LightTriggerId                             GetTrafficLightTriggerId() const;
};
}
