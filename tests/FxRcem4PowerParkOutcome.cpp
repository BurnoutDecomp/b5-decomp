// FX-RCEM4 (crash parity 2026-09-24): PowerParkingManager::DetermineOutcome (ARTIST 0x822A74A0).
// run_fxrcem4_power_park_outcome.py extracts it VERBATIM, with the KF_MAX_PERPENDICULAR_DISTANCE_FOR_
// ALIGNMENT definition, from PowerParking/BrnPowerParkingManager.cpp.
//   0x822A74B4  lwz 0x6C ; cmplwi 2 ; bge -> go on  else  outcome 0
//   0x822A74C8  lfs 0x80 ; lfs flt_82CDB4D4 (3.0) ; fcmpu ; bge -> outcome 0   (bc 4,lt: TAKEN on NaN)
//   0x822A74DC  sum the six weighted scores (add.) ; bge skips the :399 "miOverallRating >= 0" assert
//   0x822A7534  cmpwi 100 ; blt skips ; li 100 ; 0x822A7544 outcome 0 -> 1 (a FAILURE stays)
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstring>
#include <limits>

static unsigned guAssertions = 0;
static const char* gpcLastAssertion = "";
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) { ++guAssertions; gpcLastAssertion = lpcMessage; return 0; }
void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; void WriteToLog(const char*) {} }
namespace Message { u64 gxMessageFilterFlags = 0; }
}

namespace BrnWorld {
enum EPowerParkOutcome { E_PPO_TO_BE_DETERMINED = 0, E_PPO_SUCCESS = 1, E_PPO_FAILURE = 2, E_PPO_COUNT = 3 };   // DWARF :48
#include "fxrcem4_po_kf.inc"
struct PowerParkingManager {
    EPowerParkOutcome mePowerParkOutcome = E_PPO_TO_BE_DETERMINED;
    s32 miOverallRating = -1;
    s32 miWeightedDistanceScore = 0, miWeightedProximityScore = 0, miWeightedSpeedScore = 0,
        miWeightedRotationScore = 0, miWeightedPositionAlignmentScore = 0, miWeightedAngleAlignmentScore = 0;
    u32 muNearbyParkedCarCount = 0;
    f32 mfClosestPerpendicularDist = 0.0f;
    void DetermineOutcome();
};
#include "fxrcem4_po_body.inc"
}

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName) {
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}

int main() {
    using namespace BrnWorld;
    const f32 lfNan = std::numeric_limits<f32>::quiet_NaN();
    Check(KF_MAX_PERPENDICULAR_DISTANCE_FOR_ALIGNMENT == 3.0f, "KF_MAX_PERPENDICULAR_DISTANCE_FOR_ALIGNMENT == flt_82CDB4D4 (3.0)");

    PowerParkingManager m;
    m.miWeightedAngleAlignmentScore = 30; m.miWeightedPositionAlignmentScore = 30;

    m.muNearbyParkedCarCount = 1u; m.mfClosestPerpendicularDist = 1.0f; m.mePowerParkOutcome = E_PPO_SUCCESS;
    m.DetermineOutcome();
    Check(m.mePowerParkOutcome == E_PPO_TO_BE_DETERMINED && m.miOverallRating == -1,
          "one parked car (cmplwi 2 ; bge not taken): outcome reset to 0, no rating");

    m.muNearbyParkedCarCount = 2u; m.mfClosestPerpendicularDist = 3.0f; m.mePowerParkOutcome = E_PPO_SUCCESS;
    m.DetermineOutcome();
    Check(m.mePowerParkOutcome == E_PPO_TO_BE_DETERMINED && m.miOverallRating == -1,
          "3.0 is not inside the alignment band: outcome 0");

    m.mfClosestPerpendicularDist = lfNan; m.mePowerParkOutcome = E_PPO_SUCCESS;
    m.DetermineOutcome();
    Check(m.mePowerParkOutcome == E_PPO_TO_BE_DETERMINED && m.miOverallRating == -1,
          "a NaN perpendicular distance: bge (bc 4,lt) is TAKEN -> outcome 0, no rating");

    m.mfClosestPerpendicularDist = 1.0f; m.mePowerParkOutcome = E_PPO_TO_BE_DETERMINED;
    m.DetermineOutcome();
    Check(m.mePowerParkOutcome == E_PPO_SUCCESS && m.miOverallRating == 60, "aligned between two cars: rating 30 + 30, SUCCESS");

    m.miWeightedDistanceScore = 90; m.mePowerParkOutcome = E_PPO_FAILURE;
    m.DetermineOutcome();
    Check(m.miOverallRating == 100 && m.mePowerParkOutcome == E_PPO_FAILURE, "150 is capped at 100; a FAILURE stays a FAILURE");

    guAssertions = 0;
    m.miWeightedDistanceScore = -65; m.mePowerParkOutcome = E_PPO_TO_BE_DETERMINED;
    m.DetermineOutcome();
    Check(guAssertions == 1 && std::strcmp(gpcLastAssertion, "miOverallRating >= 0") == 0 && m.miOverallRating == -5,
          "a negative sum fires :399 and is stored as it is");

    std::printf("FxRcem4PowerParkOutcome: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
