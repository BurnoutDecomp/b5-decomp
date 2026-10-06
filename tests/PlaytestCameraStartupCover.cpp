#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorStateContainer.h"
#include <cstdio>

using namespace BrnDirector;
struct FakeContainer {
    int miCarSelect, miRoaming;
    int* mpCurrent;
    int* GetCurrentState() { return mpCurrent; }
    int* GetState(ArbitratorStateContainer::EState leState) {
        return leState == ArbitratorStateContainer::E_STATE_CAR_SELECT ? &miCarSelect : &miRoaming;
    }
};
struct StartupFrame {
    FakeContainer mStateContainer;
    Camera::Camera mCamera;
    bool mbStartOfGame;
    const Camera::Camera& GetNormalCamera() const { return mCamera; }
    void Publish() {
        Camera::Camera& lrCameraInOut = mCamera;
#include "playtest_camera_startup_latch.inc"
#include "playtest_camera_startup_cover.inc"
    }
};
static unsigned guChecks, guFailures;
static void Check(bool lbPassed, const char* lpcLabel) {
    ++guChecks; if (!lbPassed) { ++guFailures; std::printf("FAIL %s\n", lpcLabel); }
}
int main() {
    StartupFrame lFrame = {};
    lFrame.mbStartOfGame = true;
    lFrame.mStateContainer.mpCurrent = &lFrame.mStateContainer.miCarSelect;
    lFrame.Publish();
    Check(lFrame.mbStartOfGame && !lFrame.mCamera.GetState().IsFlagSet(Camera::CameraState::E_FLAG_VALID),
          "unprepared startup remains covered");
    lFrame.mCamera.GetState().mCurrentFlags.SetBit(Camera::CameraState::E_FLAG_VALID);
    lFrame.Publish();
    Check(!lFrame.mbStartOfGame && lFrame.mCamera.GetState().IsFlagSet(Camera::CameraState::E_FLAG_VALID),
          "first valid car-select camera opens the view");
    lFrame.mStateContainer.mpCurrent = &lFrame.mStateContainer.miRoaming;
    lFrame.Publish();
    Check(!lFrame.mbStartOfGame && lFrame.mCamera.GetState().IsFlagSet(Camera::CameraState::E_FLAG_VALID),
          "startup latch stays clear in gameplay");
    lFrame.mbStartOfGame = true;
    lFrame.Publish();
    Check(lFrame.mbStartOfGame && !lFrame.mCamera.GetState().IsFlagSet(Camera::CameraState::E_FLAG_VALID),
          "other camera states cannot prematurely open startup");
    std::printf("PlaytestCameraStartupCover: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures ? 1 : 0;
}
