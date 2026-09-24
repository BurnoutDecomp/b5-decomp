// FX-DIRECTOR (crash parity 2026-09-24): BehaviourRoadRunner::Update @0x82247E98 raises camera flag 27
// (CameraState::E_FLAG_ROAD_FOLLOWING_CAM) on every frame the fly-by runs.
//   0x8224889C  ld   r11, 0x140(r30)      ; lrCamera (+0x138 CameraState) +0x08 mCurrentFlags
//   0x822488A0  oris r11, r11, 0x800      ; | 0x0000000008000000 -- bit 27
//   0x822488A4  std  r11, 0x140(r30)
// The production statement is extracted from src/GameSource/Director/Camera/Behaviours/
// BrnBehaviourRoadRunner.cpp by run_fxdirector_roadrunner_flag.py and run on the REAL CameraState
// behind a stand-in camera (only GetState()). The two early exits that skip it (HasFailed
// @0x82247EC8, Fail(6) @0x8224814C) are checked structurally by the runner. A revision without the
// statement runs an empty tail.
#include "types.hpp"
#include "GameSource/Director/Camera/BrnCameraState.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstdio>
#include <cstring>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcText, const char*, int) { ++gAsserts; std::fprintf(stderr, "assert: %s\n", lpcText); return 0; }
    void* EndAssert() { return nullptr; }
}
}

namespace BrnDirector
{
namespace Camera
{
    struct CameraStandIn
    {
        CameraState mState;
        CameraState& GetState() { return mState; }
    };

    // The tail of BehaviourRoadRunner::Update, under its production parameter name.
    static void RoadRunnerTail(CameraStandIn& lrCamera)
    {
        (void)lrCamera;
#include "fxdirector_roadrunner_tail.inc"
    }
}
}

using namespace BrnDirector::Camera;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

int main()
{
    CameraStandIn lCamera;
    std::memset(&lCamera, 0, sizeof(lCamera));

    // A camera already carrying VALID (1) and CRASH_CAMERA (10) this frame, and a previous set.
    lCamera.mState.mCurrentFlags.SetBit(CameraState::E_FLAG_VALID);
    lCamera.mState.mCurrentFlags.SetBit(CameraState::E_FLAG_CRASH_CAMERA);
    lCamera.mState.mPreviousFlags.SetBit(CameraState::E_FLAG_HIDE_PLAYER);
    const u64 lu64Before     = lCamera.mState.mCurrentFlags.maxBits[0];
    const u64 lu64PrevBefore = lCamera.mState.mPreviousFlags.maxBits[0];

    RoadRunnerTail(lCamera);

    Check(CameraState::E_FLAG_ROAD_FOLLOWING_CAM == 27, "E_FLAG_ROAD_FOLLOWING_CAM is flag 27 (DWARF BrnCameraState.h:36)");
    Check(lCamera.mState.IsFlagSet(CameraState::E_FLAG_ROAD_FOLLOWING_CAM),
          "0x8224889C..A4: the fly-by's frame raises E_FLAG_ROAD_FOLLOWING_CAM on the CURRENT set");
    Check(lCamera.mState.mCurrentFlags.maxBits[0] == (lu64Before | 0x0000000008000000ull),
          "exactly the console's `oris r11, r11, 0x800` on the 64-bit word: bit 0x08000000 ORed in, nothing else");
    Check(lCamera.mState.mPreviousFlags.maxBits[0] == lu64PrevBefore, "the previous set is untouched");
    RoadRunnerTail(lCamera);
    Check(lCamera.mState.mCurrentFlags.maxBits[0] == (lu64Before | 0x0000000008000000ull),
          "every frame, idempotently (an OR, not a toggle)");
    Check(gAsserts == 0, "no assert fired");

    std::printf("FxDirectorRoadRunnerFlag: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
