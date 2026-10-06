#include "GameSource/Director/Camera/Camera.h"
#include <cstdio>

using BrnDirector::Camera::Camera;
using BrnDirector::Camera::CameraEffects;
static unsigned guChecks, guFailures;

struct FakeTake {
    f32 mfLetterbox;
    f32 GetValueFloat(int) const { return mfLetterbox; }
};
static const int E_ICE_LETTERBOX = 0;
#include "playtest_camera_bars_constants.inc"

static void KeyedBars(FakeTake& lrTake, CameraEffects& lrEffects) {
#include "playtest_camera_bars_keyed.inc"
}
struct Stunt {
    Camera mCamera;
    Camera& GetNonConstCamera() { return mCamera; }
    void Bars() {
#include "playtest_camera_bars_stunt.inc"
    }
};
static void Check(bool lbPassed, const char* lpcLabel) {
    ++guChecks; if (!lbPassed) { ++guFailures; std::printf("FAIL %s\n", lpcLabel); }
}
int main() {
    CameraEffects lEffects = {};
    lEffects.mfRaceEndEffectAmount = 0.73f;
    FakeTake lTake = { 1.0f };
    KeyedBars(lTake, lEffects);
    Check(lEffects.mfBlackBarAmount == 0.15f, "authored ICE letterbox writes original +A8 lane");
    Check(lEffects.mfRaceEndEffectAmount == 0.73f, "ICE leaves independent +84 race-end lane alone");
    lTake.mfLetterbox = 0;
    KeyedBars(lTake, lEffects);
    Check(lEffects.mfBlackBarAmount == 0.0f, "unauthored/off ICE letterbox removes bars");
    Stunt lStunt = {};
    lStunt.mCamera.GetEffects().mfRaceEndEffectAmount = 0.62f;
    lStunt.Bars();
    Check(lStunt.mCamera.GetEffects().mfBlackBarAmount == 0.15f, "stunt camera writes original moment+120 lane");
    Check(lStunt.mCamera.GetEffects().mfRaceEndEffectAmount == 0.62f, "stunt preserves race-end effect");
    std::printf("PlaytestCameraBarsProducers: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures ? 1 : 0;
}
