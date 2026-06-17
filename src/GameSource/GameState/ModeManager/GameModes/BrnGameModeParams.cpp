#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CgsDev::Assert::Begin/Fire/EndAssert

// Debug-print plumbing for the X360 message-filter logging block (used by
// StartGameModeParams::SetProgressionRankAsRatio). Modelled exactly as the established
// reconstruction in BrnRaceMode.cpp: gxMessageFilterFlags is the global filter word and
// gpDebugPrint is the global line-writer whose vtable slot 1 is a printf-style writer. These are
// extern declarations only (definitions live in their own TU), so re-declaring here is safe.
namespace CgsDev
{
    namespace Message
    {
        extern u32 gxMessageFilterFlags;
    }

    namespace Log
    {
        struct DebugPrint;
        typedef int (*DebugPrintFn)(DebugPrint*, const char*);
        struct DebugPrint
        {
            DebugPrintFn* mpVTable;
        };
        extern DebugPrint* gpDebugPrint;
    }
}

// =============================================================================
// BrnGameState::GameModeParams - the six X360-attested members.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct              @ 0x8231C370
//   GetCheckpointCount     @ 0x822B1EF0
//   GetFlag                @ 0x821F2C88
//   GetStartDirection      @ 0x822CBA20
//   GetStartLocationCount  @ 0x822B1F48
//   GetStartPosition       @ 0x822CB9B0
//
// GameModeParams is the per-event parameter block: the ModeManager constructs one,
// fills it from the event/progression data, and hands it to the world / AI / traffic
// modules when a mode starts. It owns no resources (plain value type).
// =============================================================================

namespace BrnGameState
{
namespace
{
// The start-location array is indexed by active-race-car slot; the X360 build asserts
// the index against BrnWorld::KI_MAX_ACTIVE_RACE_CARS (== 8). Modelled as a local
// constant rather than pulling in the BrnWorld header for a single bound.
const u32 KU_MAX_ACTIVE_RACE_CARS = 8u;

// Assert source path baked into the X360 build for the start-location bounds checks.
const char* const KPC_PARAMS_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\gamestate\\ModeManager/GameModes/BrnGameModeParams.h";
}

// -----------------------------------------------------------------------------
// Construct - reset every field to its "no event configured" default.
//
// The X360 body is a single flat initialiser (the compiler interleaved the writes and
// hoisted the array-count loop). Reconstructed as the logical member-by-member reset.
// Takes the real GameStateModuleIO::EGameModeType (wired in at consolidation, replacing the
// EGameModeType_Stub the worker used in isolation).
// -----------------------------------------------------------------------------
void GameModeParams::Construct(GameStateModuleIO::EGameModeType leGameModeType)
{
    meGameModeType = leGameModeType;

    // Scalars / flags.
    meStartMechanism                  = E_GAMEMODESTARTMECHANISM_DEFAULT;
    muFlags                           = 0;
    mePursuedCarGlobalIndex           = E_GLOBALRACECARINDEX_STUB;
    mfProgressionRankAsRatio          = 0.0f;
    mbIsOnline                        = false;
    mbInfiniteBoost                   = false;
    mfOnlineFreeburnDeformationAmount = 0.0f;
    mfModeTimeLimit                   = 0.0f;
    mfTrafficDensityScale             = 1.0f;
    mfLargeVehicleProbability         = 1.0f;
    mfTrafficSpeedScale               = 1.0f;
    miNumRivals                       = 0;
    miNumNetworkPlayers               = 0;
    // The X360 tail clears the player wreck count to 0 (it lands in the trailing all-zero
    // store cluster @ 0x838-0x860, just before the u64 muFlags `std` @ 0x860 -- there is no
    // -1 store in the tail). The only -1 stores are the four head-region words @ 0x40/0x4C/
    // 0x50/0x54 and the per-slot maModelIds in the loop. (Was -1; see LOW-CONFIDENCE note.)
    miPlayerWreckCount                = 0;

    // Per-event identity / counts cleared.
    muEventJunctionID            = 0;
    muJunctionID                 = 0;
    muNumberOfCheckpointsInEvent = 0;

    // Per-slot grade thresholds and difficulty cleared.
    mfNeedForBronze = 0.0f;
    mfNeedForSilver = 0.0f;
    mfNeedForGold   = 0.0f;

    // Per-slot reset. The X360 hoisted these into one 8-iteration loop (@ 0x8231C418..0x8231C444):
    // each pass writes six per-slot members --
    //   std  r11=0  -> maNetworkPlayerID[i]        (network player id cleared to 0)
    //   sth  r11=0  -> mau16CarColourIndex[i]      (colour index cleared to 0)
    //   sth  r11=0  -> mau16CarPaintFinishIndex[i] (paint-finish index cleared to 0)
    //   stfs f0=-1.0 -> mfOvertakingDifficulty[i]  ("no handicap" sentinel, -1)
    //   stw  r11=0  -> maePlayerTeam[i]            (team cleared to 0)
    //   stw  r6=-1  -> maModelIds[i]               ("no model" sentinel, -1)
    for (u32 luCar = 0; luCar < KU_MAX_ACTIVE_RACE_CARS; ++luCar)
    {
        maNetworkPlayerID[luCar]           = 0;
        mau16CarColourIndex[luCar]         = 0;
        mau16CarPaintFinishIndex[luCar]    = 0;
        mfOvertakingDifficulty[luCar]      = -1.0f;
        maePlayerTeam[luCar]               = E_PLAYERTEAM_STUB;     // 0
        maModelIds[luCar]                  = static_cast<CgsID>(-1);
    }

    // Reset the two embedded fixed-size arrays to empty-but-usable: the X360 stores 0 to each
    // count word (maStartLocations.miCount @ +0x250, maCheckpointDataArray.miCount @ +0x520),
    // flipping them off the KI_UNCONSTRUCTED(-1) sentinel so GetStartLocationCount() /
    // GetCheckpointCount() return 0 (rather than wrongly firing the use-before-Construct
    // assert). The Feb-2007 owner spells this maLandmarkDataArray.Construct().
    maStartLocations.Construct();
    maCheckpointDataArray.Construct();
}

// -----------------------------------------------------------------------------
// GetCheckpointCount - number of checkpoints registered for this event.
// -----------------------------------------------------------------------------
s32 GameModeParams::GetCheckpointCount() const
{
    CGS_ASSERT(maCheckpointDataArray.GetCount() != CheckpointDataArray::KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
    return maCheckpointDataArray.GetCount();
}

// -----------------------------------------------------------------------------
// GetStartLocationCount - number of start-grid slots registered.
// -----------------------------------------------------------------------------
s32 GameModeParams::GetStartLocationCount() const
{
    CGS_ASSERT(maStartLocations.GetCount() != StartLocationArray::KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
    return maStartLocations.GetCount();
}

// -----------------------------------------------------------------------------
// GetFlag - test a flag bit (or bit set) against muFlags.
// -----------------------------------------------------------------------------
bool GameModeParams::GetFlag(u64 luFlag) const
{
    return (muFlags & luFlag) != 0;
}

// -----------------------------------------------------------------------------
// GetStartPosition - spawn position for the start-grid slot liStartLocationIndex.
// -----------------------------------------------------------------------------
Vector3 GameModeParams::GetStartPosition(s32 liStartLocationIndex) const
{
    CGS_ASSERT(static_cast<u32>(liStartLocationIndex) < KU_MAX_ACTIVE_RACE_CARS,
               "liOpponentIndex >= 0 && liOpponentIndex < BrnWorld::KI_MAX_ACTIVE_RACE_CARS");
    return maStartLocations.Ge(static_cast<u32>(liStartLocationIndex)).mPosition;
}

// -----------------------------------------------------------------------------
// GetStartDirection - facing for the start-grid slot liStartLocationIndex.
// -----------------------------------------------------------------------------
Vector3 GameModeParams::GetStartDirection(s32 liStartLocationIndex) const
{
    CGS_ASSERT(static_cast<u32>(liStartLocationIndex) < KU_MAX_ACTIVE_RACE_CARS,
               "liOpponentIndex >= 0 && liOpponentIndex < BrnWorld::KI_MAX_ACTIVE_RACE_CARS");
    return maStartLocations.Ge(static_cast<u32>(liStartLocationIndex)).mDirection;
}

// =============================================================================
// BrnGameState::StartGameModeParams - the seven X360-attested standalone methods.
// =============================================================================

// -----------------------------------------------------------------------------
// StartGameModeParams::Construct (X360 @ 0x8231C1F8) - reset to "no event configured".
//
// Validates the start mechanism, stashes game-mode type / mechanism / player position, zeroes the
// start direction, writes the traffic-light trigger / takedown target / pursued-car index / shot
// group to their all-FF/-1 invalid sentinels, the boost earning to 1.0 and the traffic density to
// 0.0 (the binary has exactly ONE 1.0 store, [199]=mfBoostEarning), the rank ratio / base
// deformation / event+rank pointers / junction id to 0, and Construct()s the checkpoint array.
// The player position arrives in a vector register (Hex-Rays vmr128 v127,v1). Offsets not x64-
// faithful (AGENTS.md); X360 word index per member is in the class declaration. miRaceId /
// mPursuedCarID / muEventJunctionId / mpPlayerCarVehicleListEntry are NOT written here (absent
// from the X360 store cluster -- set by their own setters before use).
// -----------------------------------------------------------------------------
void StartGameModeParams::Construct(GameStateModuleIO::EGameModeType leGameModeType,
                                    Vector3                          lPlayerPosition,
                                    EGameModeStartMechanism          leStartMechanism)
{
    CGS_ASSERT(static_cast<u32>(leStartMechanism) < E_GAMEMODESTARTMECHANISM_COUNT, "(uint32_t)leStartMechanism < E_GAMEMODESTARTMECHANISM_COUNT");

    meGameModeType   = leGameModeType;        // [180] +720
    meStartMechanism = leStartMechanism;      // [196] +784
    mPlayerPosition  = lPlayerPosition;       // [184] +736 (vmr128 v127,v1)
    mStartDirection.SetZero();                // [188] +752 (vspltisw v0,0)

    // Four all-FF / NaN invalid sentinels ([192],[193],[197],[200]).
    miTakedownTarget        = -1;                                        // [192] +768
    mePursuedCarGlobalIndex = static_cast<EGlobalRaceCarIndex_Stub>(-1); // [193] +772 (invalid index)
    mTrafficLightTriggerId  = static_cast<LightTriggerId>(0xFFFFFFFFu);  // [197] +788 (IsValid()==false)
    miShotGroup             = -1;                                        // [200] +800

    // The single 1.0 store is the boost-earning default; traffic density defaults to 0.0.
    mfBoostEarning           = 1.0f;          // [199] +796
    mfTrafficDensity         = 0.0f;          // [198] +792
    mfPlayerBaseDeformation  = 0.0f;          // [201] +804
    mfProgressionRankAsRatio = 0.0f;          // [206] +824

    // Per-event handles / ids cleared.
    mpEventData           = NULL;             // [203] +812
    muJunctionID          = 0;                // [204] +816
    mpProgressionRankData = NULL;             // [205] +820

    // Empty-but-usable checkpoint list (X360: trailing count word -> 0, off the -1 sentinel).
    maCheckpointDataArray.Construct();        // count @ +704
}

// -----------------------------------------------------------------------------
// StartGameModeParams::GetTrafficLightTriggerId (X360 @ 0x8231C2D8). Return the stored trigger
// handle, asserting it is valid first (IsValid() inlined).
// -----------------------------------------------------------------------------
LightTriggerId StartGameModeParams::GetTrafficLightTriggerId() const
{
    const bool lbIsValid = !(((mTrafficLightTriggerId & 0xFFFF00u) == 0xFFFF00u) ||
                             (mTrafficLightTriggerId == 255u));
    CGS_ASSERT(lbIsValid, "mTrafficLightTriggerId.IsValid()");
    return mTrafficLightTriggerId;
}

// -----------------------------------------------------------------------------
// StartGameModeParams::SetTrafficLightTriggerId (X360 @ 0x823616E8). Store the trigger handle
// after asserting it is valid (same IsValid() test as the getter).
// -----------------------------------------------------------------------------
void StartGameModeParams::SetTrafficLightTriggerId(LightTriggerId lTriggerId)
{
    const bool lbIsValid = !(((lTriggerId & 0xFFFF00u) == 0xFFFF00u) ||
                             (lTriggerId == 255u));
    CGS_ASSERT(lbIsValid, "lTriggerId.IsValid()");
    mTrafficLightTriggerId = lTriggerId;
}

// -----------------------------------------------------------------------------
// StartGameModeParams::SetProgressionRankData (X360 @ 0x82354490). Store the rank-data pointer
// (asserts non-null). X360 word [205] (+820) -> mpProgressionRankData.
// -----------------------------------------------------------------------------
void StartGameModeParams::SetProgressionRankData(const BrnProgression::ProgressionRankData* lpProgressionRankData)
{
    CGS_ASSERT(lpProgressionRankData != NULL, "lpProgressionRankData != NULL");
    mpProgressionRankData = lpProgressionRankData;
}

// -----------------------------------------------------------------------------
// StartGameModeParams::SetProgressionRankAsRatio (X360 @ 0x823544F0). Store the 0..1 rank ratio
// (X360 byte +824 -> mfProgressionRankAsRatio). When the AI message-filter bit is set the X360
// logs "<AI> Setting progression rank to <value>\n" through the debug-print stream.
// -----------------------------------------------------------------------------
void StartGameModeParams::SetProgressionRankAsRatio(f32 lfProgressionRankAsRatio)
{
    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        CgsDev::Log::DebugPrint* lpPrint = CgsDev::Log::gpDebugPrint;
        lpPrint->mpVTable[1](lpPrint, "<AI> Setting progression rank to ");
        lpPrint->mpVTable[1](lpPrint, "\n");
    }
    mfProgressionRankAsRatio = lfProgressionRankAsRatio;
}

// -----------------------------------------------------------------------------
// StartGameModeParams::SetPlayerVehicleGamePlayData (X360 @ 0x82354590). Store the player car's
// vehicle-list entry pointer (asserts non-null). X360 word [207] (+828) -> mpPlayerCarVehicleListEntry.
// -----------------------------------------------------------------------------
void StartGameModeParams::SetPlayerVehicleGamePlayData(const BrnResource::VehicleListEntry* lpPlayerCarVehicleListEntry)
{
    CGS_ASSERT(lpPlayerCarVehicleListEntry != NULL, "lpPlayerCarVehicleListEntry != NULL");
    mpPlayerCarVehicleListEntry = lpPlayerCarVehicleListEntry;
}

// -----------------------------------------------------------------------------
// StartGameModeParams::AddCheckpoint (X360 @ 0x8236AAC0). Append one checkpoint (landmark +
// AI-section) to the event's checkpoint list. Element setup is CheckpointData::Construct inlined
// (district = E_DISTRICT_INVALID (18), empty block-section list).
// -----------------------------------------------------------------------------
void StartGameModeParams::AddCheckpoint(LandmarkIndex luLandmarkIndex, u16 luAISectionIndex)
{
    CheckpointData lCheckpointData;
    lCheckpointData.Construct(luLandmarkIndex, luAISectionIndex);
    maCheckpointDataArray.Append(lCheckpointData);
}
}
