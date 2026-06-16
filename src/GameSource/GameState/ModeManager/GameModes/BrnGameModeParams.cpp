#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CgsDev::Assert::Begin/Fire/EndAssert

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
        maNetworkPlayerID[luCar].muValue   = 0;
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
    if (maCheckpointDataArray.GetCount() == CheckpointDataArray::KI_UNCONSTRUCTED)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "Array used before Construct/Clear was called",
            "..\\..\\..\\GameShared\\GameClasses\\Containers/CgsArray.h",
            336);
        CgsDev::Assert::EndAssert();
    }
    return maCheckpointDataArray.GetCount();
}

// -----------------------------------------------------------------------------
// GetStartLocationCount - number of start-grid slots registered.
// -----------------------------------------------------------------------------
s32 GameModeParams::GetStartLocationCount() const
{
    if (maStartLocations.GetCount() == StartLocationArray::KI_UNCONSTRUCTED)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "Array used before Construct/Clear was called",
            "..\\..\\..\\GameShared\\GameClasses\\Containers/CgsArray.h",
            336);
        CgsDev::Assert::EndAssert();
    }
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
    if (static_cast<u32>(liStartLocationIndex) >= KU_MAX_ACTIVE_RACE_CARS)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "liOpponentIndex >= 0 && liOpponentIndex < BrnWorld::KI_MAX_ACTIVE_RACE_CARS",
            KPC_PARAMS_FILE,
            1177);
        CgsDev::Assert::EndAssert();
    }
    return maStartLocations.Ge(static_cast<u32>(liStartLocationIndex)).mPosition;
}

// -----------------------------------------------------------------------------
// GetStartDirection - facing for the start-grid slot liStartLocationIndex.
// -----------------------------------------------------------------------------
Vector3 GameModeParams::GetStartDirection(s32 liStartLocationIndex) const
{
    if (static_cast<u32>(liStartLocationIndex) >= KU_MAX_ACTIVE_RACE_CARS)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "liOpponentIndex >= 0 && liOpponentIndex < BrnWorld::KI_MAX_ACTIVE_RACE_CARS",
            KPC_PARAMS_FILE,
            1185);
        CgsDev::Assert::EndAssert();
    }
    return maStartLocations.Ge(static_cast<u32>(liStartLocationIndex)).mDirection;
}
}
