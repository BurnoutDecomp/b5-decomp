// Harness for run_showtime_velocity_cap.py: the shipped RaceCarPhysics::CapShowtimeVelocities text
// (and its five cap constants) is pasted in through extracted.inc over a test double of the car
// and the showtime singleton, then checked against the console body @0x825D7600 (crash parity
// G37-D1): a DESCENDING car keeps its original vertical speed; only its x/z are rebuilt.
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "rw/math/vpu/vector3_operation.h"
#include <cmath>
#include <cstdio>

namespace BrnPhysics { namespace Vehicle {
namespace vpu = rw::math::vpu;

struct ShowtimeParams { bool mbLaunchActive; bool mbBounceBoosting; };
static ShowtimeParams MS = { true, false };

struct RaceCarPhysics
{
    Vector3 mLinearVelocity;
    Vector3 mAngularVelocity;
    bool    mbUncapped;
    Vector3 GetLinearVelocity() const { return mLinearVelocity; }
    Vector3 GetAngularVelocity() const { return mAngularVelocity; }
    void SetLinearVelocity(Vector3 v) { mLinearVelocity = v; }
    void SetAngularVelocity(Vector3 v) { mAngularVelocity = v; }
    bool IsPlayerVehicleWithUncappedShowtimeSpeed() const { return mbUncapped; }
    void CapShowtimeVelocities();
};

#include "extracted.inc"
} }

int main()
{
    using BrnPhysics::Vehicle::RaceCarPhysics;
    int liChecks = 0, liFailures = 0;
    auto Check = [&](bool lbPass, const char* lpcName) {
        ++liChecks;
        if (!lbPass) { ++liFailures; std::printf("FAIL: %s\n", lpcName); }
    };
    auto Near = [](float a, float b) { return std::fabs(a - b) < 1e-4f; };

    RaceCarPhysics lCar{};
    // Falling straight down at 12 m/s, over the 8 m/s linear cap: the console restores the
    // ORIGINAL -12 into lane y (vcmpgtfp. 0 > vel.y ; vrlimi128 mask 4 @0x825D7908..0x825D791C).
    lCar.mLinearVelocity = { 0.0f, -12.0f, 0.0f, 0.0f };
    lCar.CapShowtimeVelocities();
    Check(Near(lCar.mLinearVelocity.y, -12.0f), "a descending car keeps its fall speed");

    // Falling and moving: x/z are the capped rebuild, y is the original fall speed.
    lCar.mLinearVelocity = { 6.0f, -8.0f, 0.0f, 0.0f };   // |v| = 10 > 8
    lCar.CapShowtimeVelocities();
    Check(Near(lCar.mLinearVelocity.x, 6.0f * 0.8f), "x is rebuilt from the capped magnitude");
    Check(Near(lCar.mLinearVelocity.y, -8.0f), "y keeps the original descent");

    // Rising: the rebuilt vector is stored whole (the .y direction clamp 9/8 does not bind at 0.6).
    lCar.mLinearVelocity = { 8.0f, 6.0f, 0.0f, 0.0f };    // |v| = 10
    lCar.CapShowtimeVelocities();
    Check(Near(lCar.mLinearVelocity.x, 6.4f) && Near(lCar.mLinearVelocity.y, 4.8f), "a rising car is capped whole");

    // Under the cap and not climbing too steeply: untouched.
    lCar.mLinearVelocity = { 3.0f, -4.0f, 0.0f, 0.0f };   // |v| = 5
    lCar.CapShowtimeVelocities();
    Check(Near(lCar.mLinearVelocity.x, 3.0f) && Near(lCar.mLinearVelocity.y, -4.0f), "under the cap nothing changes");

    std::printf("ShowtimeVelocityCap: %d checks, %d failures\n", liChecks, liFailures);
    return liFailures ? 1 : 0;
}
