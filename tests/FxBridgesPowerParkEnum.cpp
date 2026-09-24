// FX-BRIDGES (crash parity 2026-09-24) header request H-PP1: BrnWorld::EPowerParkOutcome has ONE home.
//
// BrnGameActions.h used to carry a provisional mirror of the enum "until the PowerParking TU lands"; that TU
// (BrnPowerParkingManager.h, DWARF :48) landed in b5 fd8d4ce1 / 259fa839, so any TU that included both
// headers failed C2011 -- which blocked the Power Parking producer (TrafficEntityModule, which includes
// BrnGameActions.h) and consumer (RaceCarEntityModule). This TU includes BOTH, in the order those TUs do,
// and pins the enum the power-park result action carries to the DWARF home's values:
//   E_PPO_TO_BE_DETERMINED 0, E_PPO_SUCCESS 1 (StuntModeScoring::DealWithPowerPark @0x82321530 tests
//   meOutcome == 1), E_PPO_FAILURE 2, E_PPO_COUNT 3.
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/PowerParking/BrnPowerParkingManager.h"
#include <cstdio>
#include <type_traits>

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { return 0; }
    void* EndAssert() { return nullptr; }
}
}

static unsigned gChecks = 0, gFailures = 0;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("  FAIL %s\n", lpcName);
    }
    else
    {
        std::printf("  ok   %s\n", lpcName);
    }
}

int main()
{
    using BrnGameState::GameStateModuleIO::PowerParkResultAction;
    Check(std::is_same<decltype(PowerParkResultAction::meOutcome), BrnWorld::EPowerParkOutcome>::value,
          "PowerParkResultAction::meOutcome is the DWARF home's BrnWorld::EPowerParkOutcome");
    Check(BrnWorld::E_PPO_TO_BE_DETERMINED == 0 && BrnWorld::E_PPO_SUCCESS == 1
          && BrnWorld::E_PPO_FAILURE == 2 && BrnWorld::E_PPO_COUNT == 3,
          "EPowerParkOutcome values (DWARF BrnPowerParkingManager.h:48; SUCCESS == 1 per 0x82321530)");
    Check(std::is_same<decltype(BrnWorld::PowerParkingManager::mePowerParkOutcome), BrnWorld::EPowerParkOutcome>::value,
          "the manager and the action share one enum type");
    std::printf("FxBridgesPowerParkEnum: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
