#include "GameSource/Director/Camera/Behaviours/BrnAttachmentTruck.h"
#include "rw/math/vpu/vector3_operation.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

static unsigned guChecks, guFailures;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++guFailures; return 0; }
void* EndAssert() { return 0; }
} }
using namespace BrnDirector;
using rw::math::vpu::Vector3;
namespace BrnDirector { namespace Camera {
#include "playtest_crash_truck_ratio_methods.inc"
} }

static void Check(bool lbPassed, const char* lpcLabel, f32 lfGot, f32 lfWant) {
    ++guChecks;
    if (!lbPassed) { ++guFailures; std::printf("FAIL %s: got %.6f want %.6f\n", lpcLabel, lfGot, lfWant); }
}

static bool Near(f32 lfA, f32 lfB) { return std::fabs(lfA - lfB) < 0.0001f; }

int main() {
    const Vector3 lvVelocity = { 20, 0, 0, 0 };

    // The drive-by takedown gyro block: the truck starts 4 m BEHIND the car and converges in
    // 0.125 s, so its first speed is 20 + 32 = 52 m/s and the unclamped ratio is 52 / 20 = 2.6.
    // The console clamps that ratio to [0, 1.25]: the truck settles at 1.25x the car's speed
    // and drives past it.
    Camera::AttachmentTruck lDriveBy;
    lDriveBy.Construct();
    const Camera::AttachmentTruck::Parameters lDriveByParams = { -4.0f, 0.125f };
    lDriveBy.Update(Vector3{}, lvVelocity, BrnDirector::VecFloat(0.0f), lDriveByParams);
    Check(Near(static_cast<f32>(lDriveBy.mDesiredSpeedRatio), 1.25f),
          "negative-offset truck ratio clamps at 1.25", static_cast<f32>(lDriveBy.mDesiredSpeedRatio), 1.25f);
    Check(Near(static_cast<f32>(lDriveBy.mSpeed), 52.0f + 0.1f * (20.0f * 1.25f - 52.0f)),
          "first step eases toward 1.25x the car", static_cast<f32>(lDriveBy.mSpeed), 52.0f + 0.1f * (20.0f * 1.25f - 52.0f));
    Check(Near(lDriveBy.GetPosition().x, -4.0f),
          "truck starts behind the car", lDriveBy.GetPosition().x, -4.0f);

    // Steady state: 1.25x the car's 20 m/s, i.e. 25 m/s.
    for (int liFrame = 0; liFrame < 400; ++liFrame)
        lDriveBy.Update(Vector3{}, lvVelocity, BrnDirector::VecFloat(0.0f), lDriveByParams);
    Check(Near(static_cast<f32>(lDriveBy.mSpeed), 25.0f),
          "drive-by truck settles at 1.25x the car's speed", static_cast<f32>(lDriveBy.mSpeed), 25.0f);

    // A positive offset (the default block, 4 m ahead over 0.5 s) never reaches either bound:
    // ratio = (20 - 8) / 20 = 0.6.
    Camera::AttachmentTruck lDefault;
    lDefault.Construct();
    const Camera::AttachmentTruck::Parameters lDefaultParams = { 4.0f, 0.5f };
    lDefault.Update(Vector3{}, lvVelocity, BrnDirector::VecFloat(0.0f), lDefaultParams);
    Check(Near(static_cast<f32>(lDefault.mDesiredSpeedRatio), 0.6f),
          "default truck ratio is unclamped", static_cast<f32>(lDefault.mDesiredSpeedRatio), 0.6f);

    // A car slower than offset / convergence time gives a negative ratio: the lower bound is 0.
    Camera::AttachmentTruck lSlow;
    lSlow.Construct();
    const Vector3 lvSlow = { 4, 0, 0, 0 };
    lSlow.Update(Vector3{}, lvSlow, BrnDirector::VecFloat(0.0f), lDefaultParams);
    Check(Near(static_cast<f32>(lSlow.mDesiredSpeedRatio), 0.0f),
          "slow car ratio clamps at 0", static_cast<f32>(lSlow.mDesiredSpeedRatio), 0.0f);

    std::printf("PlaytestCrashTruckRatioClamp: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures ? 1 : 0;
}
