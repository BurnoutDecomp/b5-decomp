// FX-DIRECTOR2 (crash parity 2026-09-25): the ICE scene space follows the published camera.
//
// The runner (run_fxdirector2_ice_scene_space.py) lifts the revision's PRODUCTION statement out of MainDirector::Update
// -- `if (!lCamera.mState.IsFlagSet(...E_FLAG_DONT_UPDATE_SCENESPACE)) { mICESceneSpace = lCamera.mTransform; }`, the
// console's 0x82274A28..0x82274A80 -- into fxd2_scenespace_block.inc and compiles it inside a fixture that holds the
// director's scene-space matrix by its name.
//
// Against the ARTIST body (MainDirector::Update @0x82274070):
//   0x82274A28  ld r11, lCamera +0x140 (the camera state's CURRENT flag set) ; rlwinm r11, r11, 0,18,18 (flag 13)
//   0x82274A4C  bne -> skip                                    -- a scene-space shot keeps the space still
//   0x82274A58  four lvx128 lCamera rows -> stvx128 this + 0x12170 (+0x00 / +0x10 / +0x20 / +0x30)
#include "GameSource/Director/Camera/Camera.h"
#include "GameSource/Director/Camera/BrnCameraState.h"
#include <cstdio>
#include <cstring>

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

namespace BrnDirector
{
namespace Camera
{
    void Camera::Construct() {}
}

// The director's scene-space matrix, by its name, and the statement.
struct SceneSpaceFixture
{
    Matrix44Affine mICESceneSpace;

    void Run(Camera::Camera& lCamera)
    {
#include "fxd2_scenespace_block.inc"
    }
};
}

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("FAIL  %s\n", lpcName);
    }
}

static SceneSpaceFixture gDirector;

static void Stale()
{
    gDirector.mICESceneSpace.xAxis = Vector3{ -1.0f, -2.0f, -3.0f, -4.0f };
    gDirector.mICESceneSpace.yAxis = Vector3{ -5.0f, -6.0f, -7.0f, -8.0f };
    gDirector.mICESceneSpace.zAxis = Vector3{ -9.0f, -10.0f, -11.0f, -12.0f };
    gDirector.mICESceneSpace.wAxis = Vector3{ -13.0f, -14.0f, -15.0f, -16.0f };
}
static bool IsStale()
{
    return gDirector.mICESceneSpace.xAxis.x == -1.0f && gDirector.mICESceneSpace.yAxis.y == -6.0f
        && gDirector.mICESceneSpace.zAxis.z == -11.0f && gDirector.mICESceneSpace.wAxis.w == -16.0f;
}
static bool Follows(const Camera::Camera& lrCamera)
{
    return std::memcmp(&gDirector.mICESceneSpace, &lrCamera.mTransform, sizeof(Matrix44Affine)) == 0;
}

int main()
{
    Camera::Camera lCamera;
    std::memset(&lCamera, 0, sizeof(lCamera));
    lCamera.mTransform.xAxis = Vector3{ 0.0f, 0.0f, -1.0f, 0.0f };
    lCamera.mTransform.yAxis = Vector3{ 0.0f, 1.0f, 0.0f, 0.0f };
    lCamera.mTransform.zAxis = Vector3{ 1.0f, 0.0f, 0.0f, 0.0f };
    lCamera.mTransform.wAxis = Vector3{ 3040.7f, -5.8f, -1937.9f, 1.0f };

    Stale();
    gDirector.Run(lCamera);
    Check(Follows(lCamera), "K1 flag 13 clear: the scene space becomes the frame camera's transform, all four rows "
                            "(0x82274A58..0x82274A80)");

    Stale();
    lCamera.mState.mCurrentFlags.SetBit(Camera::CameraState::E_FLAG_DONT_UPDATE_SCENESPACE);
    gDirector.Run(lCamera);
    Check(IsStale(), "K2 flag 13 set (a scene-space shot, KeyAnimController::UpdateTransformationMatrix): the scene "
                     "space holds still (`rlwinm 0,18,18 ; bne`)");

    Stale();
    lCamera.mState.mCurrentFlags.UnSetBit(Camera::CameraState::E_FLAG_DONT_UPDATE_SCENESPACE);
    lCamera.mState.mCurrentFlags.SetBit(Camera::CameraState::E_FLAG_INTERNAL_CAR_CAMERA);
    lCamera.mState.mCurrentFlags.SetBit(Camera::CameraState::E_FLAG_IS_PICTURE_PARADISE);
    lCamera.mState.mCurrentFlags.SetBit(Camera::CameraState::E_FLAG_NEW_THIS_FRAME);
    gDirector.Run(lCamera);
    Check(Follows(lCamera), "K3 only flag 13 holds it: flags 12, 14 and 6 set still follow");

    Stale();
    lCamera.mState.mCurrentFlags.UnSetAll();
    lCamera.mState.mHeadFlags.SetBit(Camera::CameraState::E_FLAG_DONT_UPDATE_SCENESPACE);
    gDirector.Run(lCamera);
    Check(Follows(lCamera), "K4 the CURRENT flag set is read (camera +0x140), not the head set: a head bit 13 follows");

    Check(gAsserts == 0, "K5 no assert fired");

    std::printf("FxDirector2IceSceneSpace: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
