// FX-DIRECTOR2 (crash parity 2026-09-25, CC-14): MainDirector::Update applies the ICE take's game-camera blend.
//
// The runner (run_fxdirector2_game_camera_blend.py) lifts the revision's PRODUCTION blend statement out of
// MainDirector::Update -- the `if (lCamera.GetEffects().mfGameCameraBlend > ...) { ... } else { ... }` at the console's
// 0x822749D4..0x82274A24 -- into fxd2_blend_block.inc, and the gate constant's definition into fxd2_blend_const.inc.
// It compiles them inside a fixture whose members carry the director's names:
//   - mCameraInterpolationController, the REAL class; its Update is a recording stand-in, and its Construct is the
//     real inline one over Interpolater::Construct (BrnInterpolater.cpp's two stores, restated here);
//   - mArbitrator.GetSharedCameras(), a REAL SharedCameraContainer whose two handles' GetProducedCamera are
//     recording stand-ins;
//   - lpIO->mpInputBuffer->GetRaceCarInfo(), a race-car array.
//
// Against the ARTIST body (MainDirector::Update @0x82274070):
//   0x822749D4  lfs f0, lCamera +0x108 (CameraEffects::mfGameCameraBlend, effects +0xA0)
//   0x822749D8  fcmpu f0, f29 (0.0, flt_82001CC0) ; ble -> 0x82274A08   -- only a blend ABOVE zero runs; a NaN resets
//   0x822749E4  sub_82212288(this + 0x166A4) -- the shared container's gameplay-EXTERNAL handle's produced camera
//   0x822749FC  CameraInterpolationController::Update(this + 0x121B0, &lCamera, that camera, raceCars[car] + 0x1F0)
//   0x82274A08  the reset: stvx128 0 + stb 0 at +0x121B0 and at +0x121D0 (both interpolaters)
#include "GameSource/Director/Shots/ShotControllers/BrnCameraInterpolationController.h"
#include "GameSource/Director/Camera/BrnSharedCameraContainer.h"
#include "GameSource/Director/Camera/Camera.h"
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++gAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
}

using namespace BrnDirector;
using rw::math::vpu::Matrix44Affine;
using rw::math::vpu::Vector3;

// ---- recording stand-ins ------------------------------------------------------------------------------------
struct UpdateCall
{
    const void* mpController;
    const void* mpCamera;
    const void* mpTo;
    const void* mpEyeTarget;
    f32         mfBlend;
};
static UpdateCall gLastUpdate;
static int giUpdates = 0, giExternalReads = 0, giBumperReads = 0;
static const void* gpExternalHandle = nullptr;
static Camera::Camera gExternalCamera, gBumperCamera;

namespace BrnDirector
{
    void CameraInterpolationController::Update(Camera::Camera& lrCamera, const Camera::Camera& lrTo,
                                               const Matrix44Affine& lrEyeTarget)
    {
        ++giUpdates;
        gLastUpdate = UpdateCall{ this, &lrCamera, &lrTo, &lrEyeTarget, lrCamera.GetEffects().mfGameCameraBlend };
    }
namespace Camera
{
    void Camera::Construct() {}
namespace Utils
{
    void Interpolater::Construct()   // BrnInterpolater.cpp's body
    {
        mLastAxis             = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
        mbWasInvertedLastTime = false;
    }
}
    template <>
    const Camera& BehaviourHandle<BehaviourGameplayExternal>::GetProducedCamera() const
    {
        ++giExternalReads;
        gpExternalHandle = this;
        return gExternalCamera;
    }
    template <>
    const Camera& BehaviourHandle<BehaviourGameplayBumper>::GetProducedCamera() const
    {
        ++giBumperReads;
        return gBumperCamera;
    }
}
}

struct FakeInput
{
    const Camera::VehicleInfo* mpCars;
    const Camera::VehicleInfo* GetRaceCarInfo() const { return mpCars; }
};
struct FakeIO
{
    const FakeInput* mpInputBuffer;
};
struct FakeArbitrator
{
    SharedCameraContainer mSharedCameraContainer;
    SharedCameraContainer& GetSharedCameras() { return mSharedCameraContainer; }
};

namespace BrnDirector
{
namespace
{
#include "fxd2_blend_const.inc"
}

// The director's members the statement names, and the statement itself.
struct BlendFixture
{
    CameraInterpolationController mCameraInterpolationController;
    FakeArbitrator                mArbitrator;

    void Run(Camera::Camera& lCamera, const FakeIO* lpIO, s32 liPlayerCarIndex)
    {
#include "fxd2_blend_block.inc"
    }
};
}

// ---- helpers -----------------------------------------------------------------------------------------------
static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("FAIL  %s\n", lpcName);
    }
}

static BlendFixture gDirector;
static Camera::VehicleInfo gaCars[8];
static FakeInput gInput;
static FakeIO gIO;

static void Dirty()
{
    gDirector.mCameraInterpolationController.mRotationInterpolater.mLastAxis = Vector3{ 1.0f, 2.0f, 3.0f, 4.0f };
    gDirector.mCameraInterpolationController.mRotationInterpolater.mbWasInvertedLastTime = true;
    gDirector.mCameraInterpolationController.mPivotInterpolater.mLastAxis = Vector3{ 5.0f, 6.0f, 7.0f, 8.0f };
    gDirector.mCameraInterpolationController.mPivotInterpolater.mbWasInvertedLastTime = true;
}
static bool Reset()
{
    const Camera::Utils::Interpolater& lrA = gDirector.mCameraInterpolationController.mRotationInterpolater;
    const Camera::Utils::Interpolater& lrB = gDirector.mCameraInterpolationController.mPivotInterpolater;
    return lrA.mLastAxis.x == 0.0f && lrA.mLastAxis.y == 0.0f && lrA.mLastAxis.z == 0.0f && !lrA.mbWasInvertedLastTime
        && lrB.mLastAxis.x == 0.0f && lrB.mLastAxis.y == 0.0f && lrB.mLastAxis.z == 0.0f && !lrB.mbWasInvertedLastTime;
}
static bool Untouched()
{
    const Camera::Utils::Interpolater& lrA = gDirector.mCameraInterpolationController.mRotationInterpolater;
    const Camera::Utils::Interpolater& lrB = gDirector.mCameraInterpolationController.mPivotInterpolater;
    return lrA.mLastAxis.x == 1.0f && lrA.mbWasInvertedLastTime && lrB.mLastAxis.z == 7.0f && lrB.mbWasInvertedLastTime;
}

// One frame: the blend keyed into the frame camera, the statement run.
static void Frame(Camera::Camera& lrCamera, f32 lfBlend, s32 liCar)
{
    lrCamera.GetEffects().mfGameCameraBlend = lfBlend;
    Dirty();
    gDirector.Run(lrCamera, &gIO, liCar);
}

int main()
{
    gInput.mpCars = gaCars;
    gIO.mpInputBuffer = &gInput;
    gDirector.mArbitrator.mSharedCameraContainer.mbUseGameplayExternal = true;
    gDirector.mArbitrator.mSharedCameraContainer.mbLookbackOverride = false;
    Camera::Camera lCamera;

    // ---- no blend keyed ----
    Frame(lCamera, 0.0f, 3);
    Check(giUpdates == 0 && giExternalReads == 0 && Reset(),
          "K1 blend 0.0: no interpolation; both interpolaters reset (0x82274A08: stvx128 0 + stb 0 at +0x121B0 / +0x121D0)");

    // ---- a keyed blend ----
    Frame(lCamera, 0.5f, 3);
    Check(giUpdates == 1 && gLastUpdate.mpController == &gDirector.mCameraInterpolationController
              && gLastUpdate.mpCamera == &lCamera && gLastUpdate.mfBlend == 0.5f && Untouched(),
          "K2 blend 0.5: CameraInterpolationController::Update on the director's own controller (+0x121B0) with the "
          "frame camera, and no reset");
    Check(gLastUpdate.mpTo == &gExternalCamera && giExternalReads == 1
              && gpExternalHandle == &gDirector.mArbitrator.mSharedCameraContainer.mGameplayExternal,
          "K3 toward the shared gameplay-EXTERNAL behaviour's produced camera (sub_82212288(this + 0x166A4), the "
          "container's +0x04 handle)");
    Check(gLastUpdate.mpEyeTarget == &gaCars[3].mRaceCarState.mTransform,
          "K4 about the frame car's transform (GetRaceCarInfo()[car] + 0x1F0, r24 @0x82274640)");

    // ---- the bumper selected: still the external camera ----
    gDirector.mArbitrator.mSharedCameraContainer.mbUseGameplayExternal = false;
    gDirector.mArbitrator.mSharedCameraContainer.mbLookbackOverride = true;
    Frame(lCamera, 1.0f, 5);
    Check(giUpdates == 2 && gLastUpdate.mpTo == &gExternalCamera && giBumperReads == 0
              && gLastUpdate.mpEyeTarget == &gaCars[5].mRaceCarState.mTransform,
          "K5 with the bumper camera selected the target is STILL the external camera (the console reads +0x166A4 "
          "unconditionally), and the transform follows the car index");

    // ---- the gate's edges ----
    struct GateCase { f32 mfBlend; bool mbRuns; const char* mpcLabel; };
    const GateCase kaGate[] = {
        { -0.25f, false, "negative" },
        { -0.0f, false, "-0" },
        { std::numeric_limits<f32>::quiet_NaN(), false, "NaN (`ble` is taken: GT is clear when unordered)" },
        { std::numeric_limits<f32>::min(), true, "FLT_MIN" },
        { std::numeric_limits<f32>::infinity(), true, "+inf" },
        { 100.0f, true, "100 (an unscaled key)" },
    };
    bool lbGate = true;
    for (const GateCase& lrCase : kaGate)
    {
        const int liBefore = giUpdates;
        Frame(lCamera, lrCase.mfBlend, 1);
        const bool lbRan = giUpdates == liBefore + 1;
        if (lbRan != lrCase.mbRuns || (lbRan ? !Untouched() : !Reset()))
        {
            lbGate = false;
            std::printf("      gate wrong for %s\n", lrCase.mpcLabel);
        }
    }
    Check(lbGate, "K6 only a blend ABOVE zero interpolates (`fcmpu f0, f29 ; ble`): negative, -0 and NaN reset; FLT_MIN, "
                  "+inf and 100 interpolate");
    Check(gAsserts == 0, "K7 no assert fired");

    std::printf("FxDirector2GameCameraBlend: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
