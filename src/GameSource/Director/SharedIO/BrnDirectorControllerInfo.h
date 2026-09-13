#pragma once

#include "BrnCommonTypes.h"
#include "GameSource/Director/Camera/Utils/BrnDebugController.h"
#include <cstddef>

namespace BrnDirector { namespace DirectorIO {
// DecFIGS BrnDirectorControllerInfo.h:42; ARTIST 82251960 and
// BridgeControllerToDirector 823C0F70 agree on the flag/vector offsets.
struct ControllerInfo
{
    bool mbAnyInput;
    bool mbGameTalkRefreshRequest;
    bool mbCameraButtonHeldDown;
    bool mbCycleCameras;
    bool mbLookback;
    bool mbRequestSloMo;
    bool mbTakeScreenshot;
    bool mbTempBoredOfCamera;
    bool mbTempBoosting;
    bool mbHandbrake;
    Vector2 mCarModifier;
    Vector2 mCameraModifier;
    Camera::Utils::DebugController mDebugController;

    bool IsCycleCameraPressed() const { return mbCycleCameras; }
    bool IsCycleCameraHeld() const { return mbCameraButtonHeldDown; }
    bool IsLookbackHeld() const { return mbLookback; }
    bool IsGameTalkRefreshRequested() const { return mbGameTalkRefreshRequest; }
};
static_assert(offsetof(ControllerInfo, mCarModifier) == 16, "controller car vector");
static_assert(offsetof(ControllerInfo, mCameraModifier) == 32, "controller camera vector");
static_assert(sizeof(ControllerInfo) == 224, "controller snapshot");
using ControlInput = ControllerInfo;
}
using ControllerInfo = DirectorIO::ControllerInfo;
}
