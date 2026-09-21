// Construct must overwrite stale mode state before PrepareForModeAction copies it.
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"
#undef private
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Check(bool condition, const char* message)
{
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

int main()
{
    using namespace BrnGameState;
    BrnGameState::GameModeParams params;
    // A valid-looking stale trigger reproduced the live Showtime director assertions.
    // Poison all storage so zero-filled allocation cannot hide missing initializers.
    std::memset(&params, 0x35, sizeof(params));
    params.muEventJunctionID = 12345;
    params.muJunctionID = 67890;
    params.mfOnlineFreeburnDeformationAmount = 0.75f;
    params.mfOnlineModeTimeLimit = 23.0f;
    params.Construct(GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME);
    Check(params.mTrafficLightTriggerId == 0xFFFFFFFFu, "start traffic light must be invalid");
    Check(params.miRoadRageThreshold == -1 && params.miPursuitRivalTotalDamage == -1 &&
          static_cast<int>(params.mePursuedCarGlobalIndex) == -1, "mode thresholds and pursued-car sentinel");
    Check(params.mSpecialEventCarId == 0, "special event car ID cleared");
    Check(params.muEventJunctionID == 12345 && params.muJunctionID == 67890 &&
          params.mfOnlineFreeburnDeformationAmount == 0.75f && params.mfOnlineModeTimeLimit == 23.0f,
          "fields not written by ARTIST retain their supplied values");
    Check(params.GetGameModeType() == GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME, "requested mode survives");
    Check(params.miNumRivals == 0 && params.miNumNetworkPlayers == 0, "player counts reset");
    Check(params.mfTrafficDensityScale == 1.0f && params.mfTrafficSpeedScale == 1.0f &&
          params.mfLargeVehicleProbability == 1.0f, "traffic defaults reset");
    Check(params.maStartLocations.GetCount() == 0 && params.maCheckpointDataArray.GetCount() == 0 &&
          params.maOpponentData.GetCount() == 0, "embedded arrays are constructed");
    for (unsigned i = 0; i < 8; ++i)
    {
        Check(params.maModelIds[i] == 0, "model sentinel");
        Check(params.maNetworkPlayerID[i] == -1 && params.mau16CarColourIndex[i] == 0 &&
              params.mau16CarPaintFinishIndex[i] == 0, "per-player defaults");
        Check(params.mfOvertakingDifficulty[i] == -1.0f, "overtaking sentinel");
    }
    std::puts("PASS: GameModeParams initialization from stale storage");
}
