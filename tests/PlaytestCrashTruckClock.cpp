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
using rw::math::vpu::Matrix44Affine;
namespace BrnDirector { namespace Camera {
#include "playtest_crash_truck_methods.inc"
} }
struct Clock {
    Timestep mStep;
    f32 GetTimestep(Timestep::EType leType) const { return mStep.Get(leType); }
    void Set(Timestep::EType leType, f32 lfStep) {
        mStep.mafTimestep[leType] = lfStep;
        mStep.maVecFloatTimestep[leType] = BrnDirector::VecFloat(lfStep);
    }
};
struct Settings {
    bool mbUseTruck;
    Camera::AttachmentTruck::Parameters mAttachmentTruckParams;
};
struct ProductionTruckCall {
    Settings* mpParameters;
    bool mbIsPlanted;
    Camera::AttachmentTruck mAttachmentTruck;
    Vector3 Step(const Clock& lrInfo, Vector3 velocity) {
        Matrix44Affine target = {};
#include "playtest_crash_truck_call.inc"
        return target.Pos();
    }
};
static void Check(bool lbPassed, const char* lpcLabel) {
    ++guChecks; if (!lbPassed) { ++guFailures; std::printf("FAIL %s\n", lpcLabel); }
}
int main() {
    Settings lSettings = { true, { 0.0f, 2.0f } };
    ProductionTruckCall lCall = {};
    lCall.mpParameters = &lSettings;
    Clock lClock;
    lClock.Set(Timestep::E_WORLD, 0.0001f);
    lClock.Set(Timestep::E_WORLD_NO_SLOMO, 1.0f / 60.0f);
    lClock.Set(Timestep::E_GAME, 1.0f / 60.0f);
    const Vector3 lvVelocity = { 20, 0, 0, 0 };
    lCall.mAttachmentTruck.Set(BrnDirector::VecFloat(0.5f), lvVelocity, Vector3{});
    Vector3 lvPosition = {};
    for (int liFrame = 0; liFrame < 100; ++liFrame) lvPosition = lCall.Step(lClock, lvVelocity);
    const f32 lfExpectedDistance = 0.1f + 0.009f * (1.0f - std::pow(0.9f, 100.0f));
    Check(std::fabs(lvPosition.x - lfExpectedDistance) < 0.0001f, "trucking remains with slow-motion world, rather than escaping 18 metres");
    lClock.Set(Timestep::E_WORLD, 0.0f);
    const Vector3 lvPaused = lCall.Step(lClock, lvVelocity);
    Check(lvPaused.x == lvPosition.x, "paused world cannot move crash camera");
    lSettings.mbUseTruck = false; lCall.mbIsPlanted = true;
    lClock.Set(Timestep::E_WORLD, 0.001f);
    const Vector3 lvPlanted = lCall.Step(lClock, lvVelocity);
    Check(std::fabs(lvPlanted.x - lvPaused.x - 0.01f) < 0.0001f, "planted camera uses the same world clock");
    lSettings.mbUseTruck = true; lCall.mbIsPlanted = false;
    lClock.Set(Timestep::E_WORLD, 1.0f / 60.0f);
    const Vector3 lvNormal = lCall.Step(lClock, lvVelocity);
    Check(std::fabs(lvNormal.x - lvPlanted.x - 10.0f / 60.0f) < 0.0001f, "normal speed preserves original steady tracking");
    Camera::AttachmentTruck lInitial;
    lInitial.Construct();
    Camera::AttachmentTruck::Parameters lNegativeOffset = { -10, 1 };
    lInitial.Update(Vector3{}, lvVelocity, BrnDirector::VecFloat(0), lNegativeOffset);
    Check(static_cast<f32>(lInitial.mDesiredSpeedRatio) == 1.0f, "initial convergence ratio clamps at original unit maximum");
    std::printf("PlaytestCrashTruckClock: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures ? 1 : 0;
}
