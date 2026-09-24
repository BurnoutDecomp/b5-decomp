// FX-RCEM4 (crash parity 2026-09-24): the Power Parking scorer's per-frame bodies.
// run_fxrcem4_power_park_update.py extracts VERBATIM from PowerParking/BrnPowerParkingManager.cpp:
//   PowerParkingManager::ClearData / AddNearTraffic / AddContactTraffic / SetNearbyParkedTrafficData /
//   Update / UpdateScoring / DetermineOutcome and the file-scope KF_* tuning globals, plus
//   PowerParkingDetail (the RwMathFPU constants) from BrnPowerParkingManager.h.
// A body the pre-fix source lacks is replayed as an empty stub.
//   Update        0x822F8400  debug-force / mode gate (-1, 7, 15) / outcome countdown + event 54 /
//                             IsActive, IsCrashing / handbrake 0.2 + speed 15 start / squared deltas /
//                             fsel Min lowest speed / UpdateScoring / contact -> FAILURE, rest (1.0, 0.2),
//                             drive-off (accel > 0, !IsDrifting, v > lowest * 1.3) / DetermineOutcome +
//                             1.5 s countdown / tallies cleared on the full pass only
//   UpdateScoring 0x822A7140  0.5 / Max(d1 - 6, 1) + 0.5 / Max(d2 / 11, 1) ; rotation/distance sums ;
//                             speed Max ; alignment Max(p^3 - 0.4, 0), Clamp(1 - a*a), Clamp(1 - p'/3) ;
//                             Clamp(score * scale, 0, 100) * (1 / 0.98) * weight, fctiwz
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "rw/math/vpu/vector3_operation.h"
#include "rw/math/fpu/scalar_operation.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"
#include "GameSource/GameState/BrnGameEvents.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnPlayerVehicleControls.h"
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

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
#include "fxrcem4_pu_detail.inc"
#include "fxrcem4_pu_kf.inc"

struct PhysicsStateFixture { Vector3 mAngularVelocity; };
class ActiveRaceCar {
public:
    bool mbActive = true, mbCrashing = false;
    f32 mfTimeDrifting = 0.0f;
    Vector3 mPosition = { 0.0f, 0.0f, 0.0f, 0.0f }, mFacing = { 0.0f, 0.0f, 1.0f, 0.0f }, mVelocity = { 0.0f, 0.0f, 0.0f, 0.0f };
    PhysicsStateFixture mPhysicsState = { { 0.0f, 0.0f, 0.0f, 0.0f } };
    bool IsActive() const { return mbActive; }
    bool IsCrashing() const { return mbCrashing; }
    bool IsDrifting() const { return mfTimeDrifting > 0.0f; }
    Vector3 GetPosition() const { return mPosition; }
    Matrix44Affine GetTransform() const { Matrix44Affine m; m.SetIdentity(); m.zAxis = mFacing; m.wAxis = mPosition; return m; }
    Vector3 GetVelocity() const { return mVelocity; }
    const PhysicsStateFixture* GetPhysicsState() const { return &mPhysicsState; }
};

namespace RaceCarEntityModuleIO {
struct GameEventQueue {
    struct Posted { s32 miType; s32 miSize; s32 maiWords[3]; };
    std::vector<Posted> maPosted;
    bool AddEvent(const CgsModule::Event* lpEvent, s32 liType, s32 liSize) {
        Posted p = { liType, liSize, { 0, 0, 0 } };
        std::memcpy(p.maiWords, lpEvent, (liSize < 12 ? liSize : 12));
        maPosted.push_back(p);
        return true;
    }
};
}

struct DebugComponentFixture { u32 muUpdates = 0; void Update() { ++muUpdates; } };

struct PowerParkingManager {
    bool              mbPowerParkInProgress = false;
    EPowerParkOutcome mePowerParkOutcome = E_PPO_TO_BE_DETERMINED;
    f32 mfTimeUntilDisplayOutcome = 0.0f;
    s32 miOverallRating = 0;
    f32 mfProximityScore = 0.0f, mfRotationScore = 0.0f, mfDistanceScore = 0.0f, mfSpeedScore = 0.0f,
        mfPositionAlignmentScore = 0.0f, mfAngleAlignmentScore = 0.0f;
    s32 miWeightedDistanceScore = 0, miWeightedProximityScore = 0, miWeightedSpeedScore = 0,
        miWeightedRotationScore = 0, miWeightedPositionAlignmentScore = 0, miWeightedAngleAlignmentScore = 0;
    Vector3 mvPositionLastFrame = { 0.0f, 0.0f, 0.0f, 0.0f }, mvFacingLastFrame = { 0.0f, 0.0f, 0.0f, 0.0f };
    f32 mfLowestSpeedThisPark = 0.0f;
    s32 miContactTrafficCount = 0, miNearTrafficCount = 0;
    u32 muNearbyParkedCarCount = 0, muNearbyParkedPlayerCount = 0;
    f32 mfClosestDistanceSq = 0.0f, mfSecondClosestDistanceSq = 0.0f, mfClosestAngleDiff = 0.0f, mfClosestPerpendicularDist = 0.0f;
    DebugComponentFixture mPowerParkingDebugComponent;
    bool mbDebugForcePowerPark = false;

    void ClearData();
    void DetermineOutcome();
    void AddNearTraffic(u32 luEntityId);
    void AddContactTraffic(u32 luEntityId);
    void SetNearbyParkedTrafficData(u32, u32, f32, f32, f32, f32);
    void UpdateScoring(f32 lfAngleChange, f32 lfPositionChange, f32 lfCurrentLinearVelocity);
    void Update(BrnGameState::GameStateModuleIO::EGameModeType leGameModeType, f32 lfSimTimerStep,
                ActiveRaceCar* lpPlayerActiveRaceCar, PlayerVehicleControls* lpPlayerControls,
                RaceCarEntityModuleIO::GameEventQueue* lpEventQueue);
};
#include "fxrcem4_pu_bodies.inc"
}

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName) {
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}
static bool Near(f32 lfA, f64 lfB, f64 lfTolerance = 1.0e-5) { return std::fabs(static_cast<f64>(lfA) - lfB) <= lfTolerance; }

using namespace BrnWorld;
namespace GsmIO = BrnGameState::GameStateModuleIO;

static PlayerVehicleControls Controls(f32 lfAccel, f32 lfHandBrake) {
    PlayerVehicleControls c; std::memset(&c, 0, sizeof(c)); c.mfAcceleration = lfAccel; c.mfHandBrake = lfHandBrake; return c;
}

int main() {
    const f32 lfNan = std::numeric_limits<f32>::quiet_NaN();

    // ---- the tuning globals (.data 0x82CDB4C4..0x82CDB504, .bss 0x82FAD2E4..0x82FAD2F0 / 0x82FAD3FC) ---------
    Check(KF_MIN_LINEAR_VELOCITY_TO_START_POWER_PARK == 15.0f && KF_MIN_HANDBRAKE_TO_START_POWER_PARK == 0.2f,
          "start gates: 15.0 (flt_82CDB4C4), 0.2 (flt_82CDB4C8)");
    Check(KF_MAX_LINEAR_VELOCITY_TO_END_POWER_PARK == 1.0f && KF_MAX_ANGULAR_VELOCITY_TO_END_POWER_PARK == 0.2f,
          "rest gates: 1.0 (flt_82CDB4CC), 0.2 (flt_82CDB4D0)");
    Check(KF_MAX_PERPENDICULAR_DISTANCE_FOR_PERFECT == 0.4f && KF_PROMIXITY_IDEAL_1ST_CAR_DISTANCE == 6.0f
          && KF_PROMIXITY_IDEAL_2ND_CAR_DISTANCE == 11.0f, "0.4 / 6.0 / 11.0 (flt_82CDB4D8..0x82CDB4E0)");
    Check(KF_DISTANCE_SCORE_SCALE == 30.0f && KF_PROXIMITY_SCORE_SCALE == 100.0f && KF_SPEED_SCORE_SCALE == 5.0f
          && KF_ROTATION_SCORE_SCALE == 800.0f && KF_POSITION_ALIGNMENT_SCORE_SCALE == 100.0f
          && KF_ANGLE_ALIGNMENT_SCORE_SCALE == 100.0f, "scales 30/100/5/800/100/100 (flt_82CDB4E4..0x82CDB4F8)");
    Check(KF_DISTANCE_SCORE_WEIGHT == 0.0f && KF_PROXIMITY_SCORE_WEIGHT == 0.0f && KF_SPEED_SCORE_WEIGHT == 0.0f
          && KF_ROTATION_SCORE_WEIGHT == 0.0f, "four weights are zero-initialised .bss (0x82FAD2E4..0x82FAD2F0)");
    Check(KF_ANGLE_ALIGNMENT_SCORE_WEIGHT == 0.5f && KF_POSITION_ALIGNMENT_SCORE_WEIGHT == 0.5f,
          "the two alignment weights 0.5 (flt_82CDB4FC / flt_82CDB500)");
    Check(KF_TOTAL_SCORE_WEIGHTS == 0.98f, "KF_TOTAL_SCORE_WEIGHTS = sum * 0.98 (dyn-init 0x82C4C128, flt_82020A90)");
    Check(KF_WAIT_FOR_OUTCOME_TIME == 1.5f, "KF_WAIT_FOR_OUTCOME_TIME 1.5 (flt_82CDB504)");

    // ---- UpdateScoring (0x822A7140) -------------------------------------------------------------------
    guAssertions = 0;
    {
        PowerParkingManager m;
        m.muNearbyParkedCarCount = 2u; m.mfClosestDistanceSq = 64.0f; m.mfSecondClosestDistanceSq = 144.0f;
        m.mfClosestPerpendicularDist = 1.0f; m.mfClosestAngleDiff = 0.5f;
        m.mfRotationScore = 0.25f; m.mfDistanceScore = 2.0f; m.mfSpeedScore = 3.0f;
        m.UpdateScoring(0.125f, 0.5f, 7.0f);
        Check(Near(m.mfProximityScore, 0.25 + 0.5 / (12.0 / 11.0)),
              "proximity: 0.5 / Max(8 - 6, 1) + 0.5 / Max(12 / 11, 1) (fsubs then fdivs)");
        Check(m.mfRotationScore == 0.375f && m.mfDistanceScore == 2.5f && m.mfSpeedScore == 7.0f,
              "rotation += angle change, distance += position change, speed = Max(v, speed)");
        Check(Near(m.mfAngleAlignmentScore, 0.75), "angle alignment = Clamp(1 - 0.5 * 0.5, 0, 1)");
        Check(Near(m.mfPositionAlignmentScore, 1.0 - 0.6 / 3.0), "position alignment = Clamp(1 - Max(1^3 - 0.4, 0) / 3, 0, 1)");
        Check(m.miWeightedAngleAlignmentScore == 38 && m.miWeightedPositionAlignmentScore == 40,
              "weighted: trunc(75 / 0.98 * 0.5) = 38, trunc(80 / 0.98 * 0.5) = 40");
        Check(m.miWeightedDistanceScore == 0 && m.miWeightedProximityScore == 0 && m.miWeightedSpeedScore == 0
              && m.miWeightedRotationScore == 0, "the four zero-weight scores never count");
    }
    {
        PowerParkingManager m;
        m.muNearbyParkedCarCount = 1u; m.mfClosestDistanceSq = 25.0f; m.mfSecondClosestDistanceSq = 1.0f;
        m.mfClosestPerpendicularDist = 0.5f; m.mfClosestAngleDiff = 0.0f;
        m.UpdateScoring(0.0f, 0.0f, 0.0f);
        Check(Near(m.mfProximityScore, 0.5), "one parked car: only the first term, 0.5 / Max(5 - 6, 1)");
        Check(Near(m.mfPositionAlignmentScore, 1.0), "0.5^3 - 0.4 < 0 -> Max(.., 0) -> a perfect 1.0 (the CUBE, not the distance)");
        Check(m.miWeightedAngleAlignmentScore == 51 && m.miWeightedPositionAlignmentScore == 51,
              "a perfect park: trunc(100 / 0.98 * 0.5) = 51 each");
    }
    {
        PowerParkingManager m;
        m.muNearbyParkedCarCount = 2u; m.mfClosestDistanceSq = 49.0f; m.mfSecondClosestDistanceSq = 169.0f;
        m.mfClosestPerpendicularDist = 3.0f; m.mfAngleAlignmentScore = 0.9f; m.mfPositionAlignmentScore = 0.9f;
        m.UpdateScoring(0.0f, 0.0f, 0.0f);
        Check(m.mfAngleAlignmentScore == 0.0f && m.mfPositionAlignmentScore == 0.0f,
              "3.0 is not inside the alignment band (bge): both alignment scores 0");
        m.mfClosestPerpendicularDist = lfNan; m.mfAngleAlignmentScore = 0.9f;
        m.UpdateScoring(0.0f, 0.0f, 0.0f);
        Check(m.mfAngleAlignmentScore == 0.0f, "a NaN perpendicular distance scores nothing (bge taken on NaN)");
        m.muNearbyParkedCarCount = 0u; m.mfProximityScore = 5.0f;
        m.UpdateScoring(0.0f, 0.0f, 0.0f);
        Check(m.mfProximityScore == 0.0f, "no parked car: proximity 0 (stfs f31, 0x10 before the count test)");
    }
    Check(guAssertions == 0, "no assertion in range");
    {
        PowerParkingManager m;
        m.muNearbyParkedCarCount = 1u; m.mfClosestDistanceSq = 49.0f; m.mfClosestPerpendicularDist = 1.0f;
        m.mfClosestAngleDiff = lfNan;
        m.UpdateScoring(0.0f, 0.0f, 0.0f);
        Check(guAssertions == 0, ":300 does not fire on a NaN angle (blt not taken, ble taken)");
        m.mfClosestAngleDiff = 2.0f;
        m.UpdateScoring(0.0f, 0.0f, 0.0f);
        Check(guAssertions == 1 && std::strcmp(gpcLastAssertion, "mfClosestAngleDiff >= 0 && mfClosestAngleDiff <= RwMathFPU::HALF_PI") == 0,
              ":300 fires above HALF_PI");
        guAssertions = 0;
    }

    // ---- Update (0x822F8400) --------------------------------------------------------------------------
    {
        PowerParkingManager m; ActiveRaceCar car; RaceCarEntityModuleIO::GameEventQueue q;
        PlayerVehicleControls c = Controls(0.0f, 1.0f);
        car.mVelocity = { 20.0f, 0.0f, 0.0f, 0.0f };

        m.mbPowerParkInProgress = true; m.mfSpeedScore = 9.0f;
        m.Update(GsmIO::E_MODE_ROAD_RAGE, 0.1f, &car, &c, &q);
        Check(!m.mbPowerParkInProgress && m.mfSpeedScore == 0.0f && m.mPowerParkingDebugComponent.muUpdates == 1u
              && guAssertions == 0, "Road Rage drops a park in progress (ClearData), after ticking the debug component");

        m.mbDebugForcePowerPark = true; m.mfSpeedScore = 9.0f;
        m.Update(GsmIO::E_MODE_ROAD_RAGE, 0.1f, &car, &c, &q);
        Check(m.mbPowerParkInProgress && m.mfSpeedScore == 0.0f && m.mfLowestSpeedThisPark == FLT_MAX,
              "FORCE PARKING starts a park in any mode (ClearData, in progress)");
        m.mbDebugForcePowerPark = false;
    }
    {
        PowerParkingManager m; ActiveRaceCar car; RaceCarEntityModuleIO::GameEventQueue q;
        car.mVelocity = { 20.0f, 0.0f, 0.0f, 0.0f };
        m.miContactTrafficCount = 3; m.miNearTrafficCount = 4;

        PlayerVehicleControls c = Controls(0.0f, 0.1f);
        m.Update(GsmIO::E_MODE_NONE, 0.1f, &car, &c, &q);
        Check(!m.mbPowerParkInProgress && m.miContactTrafficCount == 3 && m.miNearTrafficCount == 4,
              "handbrake 0.1 is not > 0.2: no park, and the early return keeps both tallies");

        c = Controls(0.0f, 1.0f); car.mVelocity = { 10.0f, 0.0f, 0.0f, 0.0f };
        m.Update(GsmIO::E_MODE_NONE, 0.1f, &car, &c, &q);
        Check(!m.mbPowerParkInProgress, "10 m/s is not above the 15 start speed: no park");

        car.mbActive = false; car.mVelocity = { 20.0f, 0.0f, 0.0f, 0.0f };
        m.Update(GsmIO::E_MODE_NONE, 0.1f, &car, &c, &q);
        Check(!m.mbPowerParkInProgress, "an inactive car does not park");
        car.mbActive = true;

        m.miContactTrafficCount = 0; m.miNearTrafficCount = 4;
        car.mPosition = { 1.0f, 0.0f, 2.0f, 0.0f };
        m.Update(GsmIO::E_MODE_STUNT_ATTACK, 0.1f, &car, &c, &q);
        Check(m.mbPowerParkInProgress && m.mfLowestSpeedThisPark == 20.0f && m.mvPositionLastFrame.x == 1.0f
              && m.mfDistanceScore == 0.0f && m.mfRotationScore == 0.0f,
              "Stunt Run, handbrake, 20 m/s: the park starts with zero deltas; lowest speed = Min(20, FLT_MAX)");
        Check(m.miNearTrafficCount == 0 && m.miContactTrafficCount == 0, "the full pass clears both tallies (0x822F8894/8)");

        car.mPosition = { 2.0f, 0.0f, 2.0f, 0.0f }; car.mFacing = { 0.6f, 0.0f, 0.8f, 0.0f };
        car.mVelocity = { 12.0f, 0.0f, 0.0f, 0.0f }; c = Controls(0.0f, 0.0f);
        m.Update(GsmIO::E_MODE_ONLINE_FREE_BURN_LOBBY, 0.1f, &car, &c, &q);
        Check(m.mbPowerParkInProgress && Near(m.mfDistanceScore, 1.0) && Near(m.mfRotationScore, 0.4),
              "in progress (no handbrake needed): SQUARED position and facing deltas, 1.0 and 0.4");
        Check(m.mfLowestSpeedThisPark == 12.0f && m.mfSpeedScore == 20.0f, "lowest speed 12, speed score keeps the max 20");

        // rest: 0.5 m/s, 0.1 rad/s, two cars, aligned -> SUCCESS and the countdown
        m.muNearbyParkedCarCount = 2u; m.mfClosestDistanceSq = 49.0f; m.mfSecondClosestDistanceSq = 144.0f;
        m.mfClosestPerpendicularDist = 0.5f; m.mfClosestAngleDiff = 0.0f; m.muNearbyParkedPlayerCount = 1u;
        car.mVelocity = { 0.5f, 0.0f, 0.0f, 0.0f }; car.mPhysicsState.mAngularVelocity = { 0.0f, 0.1f, 0.0f, 0.0f };
        m.Update(GsmIO::E_MODE_NONE, 0.1f, &car, &c, &q);
        Check(!m.mbPowerParkInProgress && m.mePowerParkOutcome == E_PPO_SUCCESS && m.miOverallRating == 100
              && m.mfTimeUntilDisplayOutcome == 1.5f, "came to rest: SUCCESS, rating 51 + 51 capped at 100, countdown 1.5");
        Check(q.maPosted.empty(), "no result event until the countdown runs out");

        car.mVelocity = { 0.0f, 0.0f, 0.0f, 0.0f };
        m.Update(GsmIO::E_MODE_NONE, 1.0f, &car, &c, &q);
        Check(q.maPosted.empty() && Near(m.mfTimeUntilDisplayOutcome, 0.5), "1.5 - 1.0 = 0.5: still counting");
        m.Update(GsmIO::E_MODE_NONE, 1.0f, &car, &c, &q);
        Check(q.maPosted.size() == 1u && q.maPosted[0].miType == 54 && q.maPosted[0].miSize == 12
              && q.maPosted[0].maiWords[0] == 1 && q.maPosted[0].maiWords[1] == 100 && q.maPosted[0].maiWords[2] == 1
              && m.mfTimeUntilDisplayOutcome == 0.0f,
              "the countdown runs out: game event 54 {outcome 1, rating 100, 1 other player}, countdown 0");
    }
    {
        PowerParkingManager m; ActiveRaceCar car; RaceCarEntityModuleIO::GameEventQueue q;
        PlayerVehicleControls c = Controls(0.5f, 0.0f);
        m.mfTimeUntilDisplayOutcome = 1.0f; m.mePowerParkOutcome = E_PPO_SUCCESS;
        car.mbActive = false;
        m.Update(GsmIO::E_MODE_NONE, 0.1f, &car, &c, &q);
        Check(q.maPosted.empty() && m.mfTimeUntilDisplayOutcome == 0.0f,
              "accelerating (0.5 > 0.2, flt_82014A98) during the countdown drops the result unsent");
    }
    {
        PowerParkingManager m; ActiveRaceCar car; RaceCarEntityModuleIO::GameEventQueue q;
        PlayerVehicleControls c = Controls(0.0f, 1.0f);
        car.mVelocity = { 20.0f, 0.0f, 0.0f, 0.0f };
        m.Update(GsmIO::E_MODE_NONE, 0.1f, &car, &c, &q);          // start, lowest 20
        car.mVelocity = { 16.0f, 0.0f, 0.0f, 0.0f };
        m.Update(GsmIO::E_MODE_NONE, 0.1f, &car, &c, &q);          // lowest 16
        c = Controls(1.0f, 0.0f); car.mVelocity = { 21.0f, 0.0f, 0.0f, 0.0f }; car.mfTimeDrifting = 1.0f;
        m.Update(GsmIO::E_MODE_NONE, 0.1f, &car, &c, &q);
        Check(m.mbPowerParkInProgress, "accelerating while drifting does not end the park (IsDrifting, lfs 0x4E0)");
        car.mfTimeDrifting = 0.0f;
        m.Update(GsmIO::E_MODE_NONE, 0.1f, &car, &c, &q);
        Check(!m.mbPowerParkInProgress && m.mePowerParkOutcome == E_PPO_TO_BE_DETERMINED && m.mfTimeUntilDisplayOutcome == 0.0f,
              "accelerating to 21 > 16 * 1.3 drives off: park over, no parked cars -> nothing to show");
    }
    {
        PowerParkingManager m; ActiveRaceCar car; RaceCarEntityModuleIO::GameEventQueue q;
        PlayerVehicleControls c = Controls(0.0f, 1.0f);
        car.mVelocity = { 20.0f, 0.0f, 0.0f, 0.0f };
        m.muNearbyParkedCarCount = 2u; m.mfClosestDistanceSq = 49.0f; m.mfSecondClosestDistanceSq = 144.0f;
        m.mfClosestPerpendicularDist = 0.5f; m.mfClosestAngleDiff = 0.0f;
        m.Update(GsmIO::E_MODE_NONE, 0.1f, &car, &c, &q);          // start
        m.AddContactTraffic(77u);
        m.AddNearTraffic(78u);
        Check(m.miContactTrafficCount == 1 && m.miNearTrafficCount == 1, "AddContactTraffic / AddNearTraffic bump the tallies");
        m.Update(GsmIO::E_MODE_NONE, 0.1f, &car, &c, &q);
        Check(!m.mbPowerParkInProgress && m.mePowerParkOutcome == E_PPO_FAILURE && m.mfTimeUntilDisplayOutcome == 1.5f,
              "a contact during the park: FAILURE (li 2), DetermineOutcome keeps it, the countdown arms");
        m.SetNearbyParkedTrafficData(3u, 2u, 1.0f, 2.0f, 0.25f, 0.75f);
        Check(m.muNearbyParkedCarCount == 3u && m.muNearbyParkedPlayerCount == 2u && m.mfClosestDistanceSq == 1.0f
              && m.mfSecondClosestDistanceSq == 2.0f && m.mfClosestAngleDiff == 0.25f && m.mfClosestPerpendicularDist == 0.75f,
              "SetNearbyParkedTrafficData stores all six");
    }
    Check(guAssertions == 0, "no assertion on the Update paths");

    std::printf("FxRcem4PowerParkUpdate: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
