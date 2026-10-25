// Observe real published records and real variable event queues. This fixture
// does not replace game records, observer bodies or event iteration.
#define NOMINMAX
#include "pc/input/VehicleFeedback.h"
#include <cstdio>

static unsigned guChecks = 0, guFailures = 0, guAsserts = 0;
namespace CgsDev
{
    namespace Assert
    {
        int BeginAssert() { return 0; }
        int FireAssert(const char*, const char*, int) { ++guAsserts; return 0; }
        void* EndAssert() { return nullptr; }
    }
    namespace Log { decltype(gpDebugPrint) gpDebugPrint = nullptr; }
    namespace Message { decltype(gxMessageFilterFlags) gxMessageFilterFlags = 0; }
}
static void Check(bool lbPass, const char* lpName)
{
    ++guChecks;
    if (!lbPass) { ++guFailures; std::printf("FAIL: %s\n", lpName); }
}

int main()
{
    namespace AT = CgsInput::AdaptiveTriggers;
    namespace EV = BrnGameState::GameStateModuleIO;
    BrnPhysics::Vehicle::RaceCarState lCar;
    BrnWorld::RaceCarEntityModuleIO::BoostOutputInfo lBoost{};
    lCar.mEntityId.muValue = 123;
    lCar.mCarAssetAttribKey = 456;
    lCar.mi8Gear = 2;
    lCar.mfGas = 0.8f; lCar.mfBrake = 0.25f;
    lCar.mfRPM = 4000; lCar.mfTimeInAir = 0.2f; lCar.mfTimeDrifting = 0.3f;
    lCar.mfAbsDriftScale = 0.5f; lCar.mLinearVelocity.y = -7;
    lBoost.mbIsBoosting = true; lBoost.meBoostType = BrnWorld::E_BOOST_TYPE_STUNT;
    for (unsigned i = 0; i < 4; ++i)
    {
        lCar.mabWheelExists[i] = true;
        lCar.maWheels[i].mRoadContact.mbLineTestIsValid = true;
        lCar.maWheels[i].mRoadContact.mbIsOnGround = true;
        lCar.maWheels[i].mbHasTraction = true;
        lCar.maWheels[i].mfRoadLongSpeed = 10;
        lCar.maWheels[i].mfWheelLongSpeed = 15;
        lCar.maWheels[i].mfSkidFactor = 0.6f;
    }
    auto lSample = AT::ProjectVehicle(lCar, lBoost, false);
    Check(lSample.mbActive && lSample.mfGas == 0.8f && lSample.mfBrake == 0.25f && lSample.miGear == 2,
        "snapshot projection preserves forward pedal roles and actual gear");
    Check(lSample.mfRPM == 4000 && lSample.mfVerticalSpeed == -7 && lSample.mfAirTime == 0.2f,
        "RPM and landing velocity come from named snapshot members");
    Check(lSample.mbBoosting && lSample.mbDrifting && lSample.meCharacter == AT::Character::Stunt,
        "boost, drift and vehicle character come from published semantic state");
    Check(lSample.muGroundedWheels == 4 && lSample.mfDrivenSlip == 0.5f && lSample.mfBrakeSkid == 0.6f,
        "valid wheel contact projects normalized relative slip and skid");
    lCar.maWheels[0].mRoadContact.mbLineTestIsValid = false;
    lCar.maWheels[1].mRoadContact.mbIsOnGround = false;
    lCar.mabWheelExists[2] = false;
    lSample = AT::ProjectVehicle(lCar, lBoost, false);
    Check(lSample.muGroundedWheels == 1, "missing, invalid and airborne wheels cannot contribute ground contact");
    lCar.mi8Gear = BrnPhysics::Vehicle::KU8_REVERSE_GEAR;
    lSample = AT::ProjectVehicle(lCar, lBoost, false);
    Check(lSample.mbReverse && lSample.mfGas == 0.25f && lSample.mfBrake == 0.8f,
        "actual reverse gear swaps logical gas/brake to match engine input swap");
    lCar.meDriverType = BrnPhysics::Vehicle::E_DRIVER_TYPE_AI;
    Check(!AT::ProjectVehicle(lCar, lBoost, false).mbDriving, "AI camera driving does not pretend player pedal control");
    Check(AT::ProjectVehicle(lCar, lBoost, true).mbActive, "Showtime retains feedback during crash physics");
    lCar.mbIsHidden = true;
    Check(!AT::ProjectVehicle(lCar, lBoost, true).mbActive, "hidden player car is inactive");
    lCar.mbIsHidden = false; lCar.meDriverType = BrnPhysics::Vehicle::E_DRIVER_TYPE_PLAYER;
    lCar.mfUpShiftRPM = 1000;
    Check(!AT::ProjectVehicle(lCar, lBoost, false).mbLimiter, "high RPM above shift threshold is not invented limiter activity");
    lBoost.mbWasChainJustCompleted = true; lBoost.muNumChained = 4;
    lSample = AT::ProjectVehicle(lCar, lBoost, false);
    Check((lSample.muEvents & AT::E_EVENT_CHAIN) && lSample.muChain == 4, "chain uses semantic award flag and serial count");

    EV::GameEventQueue lQueue;
    lQueue.MarkUnconstructed(); lQueue.Construct();
    EV::VehicleImpactEvent lImpact{};
    lImpact.meAggressorActiveRaceCarIndex = 1; lImpact.meVictimActiveRaceCarIndex = 2;
    lQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lImpact), EV::E_EVENT_VEHICLE_IMPACT, sizeof(lImpact));
    lSample = {};
    AT::ObserveEvents(lSample, E_ACTIVE_RACE_CAR_INDEX_0, lQueue);
    Check(!lSample.muEvents, "remote rival collision cannot generate a local impact");
    lImpact.meVictimActiveRaceCarIndex = 0;
    lQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lImpact), EV::E_EVENT_VEHICLE_IMPACT, sizeof(lImpact));
    AT::ObserveEvents(lSample, E_ACTIVE_RACE_CAR_INDEX_0, lQueue);
    Check((lSample.muEvents & AT::E_EVENT_IMPACT) && lQueue.GetLength() == 2,
        "local impact is observed without consuming gameplay queue");
    lQueue.Clear();
    EV::JustBouncedEvent lBounce{}; lBounce.mbBoostedBounce = true;
    lQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lBounce), EV::E_EVENT_JUST_BOUNCED, sizeof(lBounce));
    lSample = {}; AT::ObserveEvents(lSample, E_ACTIVE_RACE_CAR_INDEX_0, lQueue);
    Check(!lSample.muEvents, "bounce notification is gated by actual Showtime state");
    lSample.mbShowtime = true; AT::ObserveEvents(lSample, E_ACTIVE_RACE_CAR_INDEX_0, lQueue);
    Check((lSample.muEvents & (AT::E_EVENT_BOUNCE | AT::E_EVENT_BOOSTED_BOUNCE)) ==
        (AT::E_EVENT_BOUNCE | AT::E_EVENT_BOOSTED_BOUNCE), "Showtime reads boosted-bounce payload");
    lQueue.Clear();
    EV::NearMissScoredEvent lNear{}; lNear.miCount = 1;
    lQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lNear), EV::E_EVENT_NEAR_MISS_SCORED, sizeof(lNear));
    EV::CompletedStuntEvent lStunt{}; lStunt.muStuntActionComplete = 1;
    lQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lStunt), EV::E_EVENT_COMPLETED_STUNT, sizeof(lStunt));
    lSample = {}; AT::ObserveEvents(lSample, E_ACTIVE_RACE_CAR_INDEX_0, lQueue);
    Check((lSample.muEvents & (AT::E_EVENT_NEAR_MISS | AT::E_EVENT_STUNT)) ==
        (AT::E_EVENT_NEAR_MISS | AT::E_EVENT_STUNT), "reward cues read scored near miss and completed stunt");
    Check(lQueue.GetLength() == 2 && !guAsserts, "observer preserves queue and raises no asserts");
    lQueue.Destruct();
    std::printf("Vehicle feedback projection: %u/%u checks passed\n", guChecks - guFailures, guChecks);
    return guFailures ? 1 : 0;
}
