// Actual camera types, interpolation math, and extracted game-module methods.
// Only the director buffer and optional debug-output endpoint are test seams.
#include "GameSource/Director/Camera/Camera.h"
#include "GameShared/GameClasses/System/Timer/CgsFrameInterpolation.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

namespace CgsDev { namespace Log {
struct Sink { template<class T> Sink& operator<<(const T&) { return *this; } };
Sink* gpDebugPrint = nullptr;
} namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* text, const char*, int)
{ std::fprintf(stderr, "%s\n", text); std::abort(); }
void* EndAssert() { return nullptr; }
} }

using Camera = BrnDirector::Camera::Camera;
using State = BrnDirector::Camera::CameraState;
namespace blend = CgsSystem::FrameInterpolation;
struct Buffer
{
    const Camera* camera = nullptr;
    unsigned locks = 0, unlocks = 0;
    void LockForRead() { ++locks; }
    void UnlockForRead() { ++unlocks; }
    const Camera* GetCameraOutput() const { return camera; }
};
struct TestModule
{
    Buffer* mpDirectorOutputBuffer = nullptr;
    bool mbCurrentTickCameraValid = false, mbPreviousTickCameraValid = false;
    Camera mCurrentTickCamera{}, mPreviousTickCamera{}, mInterpolatedCamera{};
    void LatchDispatchCamera();
    const Camera* GetInterpolatedDispatchCamera();
};
#include "pc_camera_cut_interpolation.inc"

static unsigned checks, failures;
static void Check(bool ok, const char* name)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL %s\n", name); }
}
static bool Near(float a, float b) { return std::fabs(a - b) < 0.0001f; }
static Camera Snapshot(float x, float fov, bool cut = false, bool reversed = false)
{
    Camera camera{};
    camera.mTransform.xAxis.x = reversed ? -1.0f : 1.0f;
    camera.mTransform.yAxis.y = 1;
    camera.mTransform.zAxis.z = reversed ? -1.0f : 1.0f;
    camera.mTransform.wAxis.x = x;
    camera.mTransform.wAxis.w = 1;
    camera.mfFOV = fov;
    camera.mState.SetFlag(State::E_FLAG_NEW_THIS_FRAME, cut);
    camera.mState.SetFlag(State::E_FLAG_TAKEDOWN_CAMERA, true);
    return camera;
}
int main()
{
    _putenv_s("BRN_CAM_INPUT_DIAG", "");
    blend::SetEnabled(true);
    TestModule module;
    Buffer buffer;
    Camera camera = Snapshot(10, 1.0f);
    Check(module.GetInterpolatedDispatchCamera() == nullptr, "no invented camera before first tick");
    module.LatchDispatchCamera();
    module.mpDirectorOutputBuffer = &buffer;
    buffer.camera = &camera;
    blend::SetAlpha(0);
    module.LatchDispatchCamera();
    Check(module.GetInterpolatedDispatchCamera()->GetTransform().wAxis.x == 10,
          "first camera is visible at alpha zero");

    camera = Snapshot(12, 1.4f);
    camera.mEffects.mfSimTimeScale = 0.25f;
    module.LatchDispatchCamera();
    blend::SetAlpha(0.5f);
    const Camera* shown = module.GetInterpolatedDispatchCamera();
    Check(Near(shown->GetTransform().wAxis.x, 11) && Near(shown->GetFOV(), 1.2f),
          "continuous motion and FOV retain interpolation");
    Check(shown->mEffects.mfSimTimeScale == 0.25f
          && shown->mState.IsFlagSet(State::E_FLAG_TAKEDOWN_CAMERA),
          "discrete effects and state come from the current tick");

    camera = Snapshot(200, 0.7f, true, true);
    module.LatchDispatchCamera();
    for (float alpha : {0.0f, 0.25f, 0.5f, 0.99f, 1.0f})
    {
        blend::SetAlpha(alpha);
        shown = module.GetInterpolatedDispatchCamera();
        Check(shown == &module.mCurrentTickCamera
              && shown->GetTransform().wAxis.x == 200
              && shown->GetTransform().xAxis.x == -1 && shown->GetFOV() == 0.7f,
              "hard cut snaps translation, rotation and FOV at every render alpha");
    }
    Check(camera.mState.IsFlagSet(State::E_FLAG_NEW_THIS_FRAME)
          && shown->mState.IsFlagSet(State::E_FLAG_NEW_THIS_FRAME),
          "cut flag survives in producer and published snapshot");
    blend::SetAlpha(0.4f);
    Check(module.GetInterpolatedDispatchCamera()->GetTransform().wAxis.x == 200,
          "render-only frames cannot blend back into the previous shot");

    camera = Snapshot(204, 0.9f, false, true);
    module.LatchDispatchCamera();
    blend::SetAlpha(0.5f);
    shown = module.GetInterpolatedDispatchCamera();
    Check(Near(shown->GetTransform().wAxis.x, 202) && Near(shown->GetFOV(), 0.8f),
          "smooth motion resumes from the new shot on the next tick");

    // No producer-pointer or distance heuristic: an authored small cut is a cut.
    camera = Snapshot(204.1f, 0.91f, true, true);
    module.LatchDispatchCamera();
    shown = module.GetInterpolatedDispatchCamera();
    Check(shown->GetTransform().wAxis.x == camera.GetTransform().wAxis.x
          && shown->GetFOV() == camera.GetFOV(), "small same-producer cut still resets history");
    camera = Snapshot(1000, 1.3f, false, true);
    module.LatchDispatchCamera();
    shown = module.GetInterpolatedDispatchCamera();
    Check(Near(shown->GetTransform().wAxis.x, 602.05f),
          "large continuous motion remains a blend when the director did not cut");

    camera = Snapshot(-20, 1.1f, true);
    module.LatchDispatchCamera();
    camera = Snapshot(30, 0.6f, true);
    module.LatchDispatchCamera();
    Check(module.GetInterpolatedDispatchCamera()->GetTransform().wAxis.x == 30,
          "consecutive cut ticks each discard the old shot");
    buffer.camera = nullptr;
    module.LatchDispatchCamera();
    Check(module.GetInterpolatedDispatchCamera()->GetTransform().wAxis.x == 30
          && buffer.locks == buffer.unlocks, "empty output preserves camera and releases its lock");
    module.mpDirectorOutputBuffer = nullptr;
    module.LatchDispatchCamera();
    blend::SetAlpha(1);
    Check(module.GetInterpolatedDispatchCamera() == &module.mCurrentTickCamera,
          "missing buffer and console-rate endpoint retain the current camera");
    std::printf("PCCameraCutInterpolation: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
