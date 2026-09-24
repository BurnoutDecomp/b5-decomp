// FX-DIRECTOR (crash parity 2026-09-24, conductor item C1): the PRODUCTION "BlackFade_Water" branch of
// BrnDirector::Arbitrator::Update @0x8226ADA0 (NORMAL state), extracted from
// src/GameSource/Director/Arbitrator/BrnDirectorArbitrator.cpp by run_fxdirector_water_fade.py and compiled
// against the real Camera::CameraEffects / Camera::PlayerCrashInfo, with recording stand-ins for its two calls
// (SharedCameraContainer::ForcePrimaryGameplayBehaviourToFinish, Camera::EnsureEffectIsStopped -- both bodied
// elsewhere and not under test here).
//
// Checked against the ARTIST asm (0x8226AF90..0x8226B008, r29 = the frame camera, r30 = the shared info):
//   lwz r11, 0x34(r30) ; lbz r11, 0x27(r11)          PlayerCrashInfo::mbHitWater
//   set:   ld 0x140 / and -3 / std 0x140             camera state flags &= ~2
//          HookNameStringWrapper::Set(camera+0x68, "BlackFade_Water")
//          stfs f31(1.0) camera+0xE8 ; stb 1 camera+0x11F ; stb 0 camera+0x120 ; stw 0 camera+0xE4
//          BehaviourManager::Behaviour(arb+0x38E4) ; stb 1 +0xB5D ; stb 1 +0x29E ; stfs FLT_MAX +0x290
//   clear: EnsureEffectIsStopped(camera, *(r30+0x30), "BlackFade_Water")
#include "GameSource/Director/Camera/BrnCameraEffects.h"
#include "GameSource/Director/Utils/BrnDirectorEffectTrigger.h"
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"
#include <cstdio>
#include <cstring>

static unsigned gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcText, const char*, int) { std::fprintf(stderr, "assert: %s\n", lpcText); return 0; }
    void* EndAssert() { return nullptr; }
}
}

// VehicleInfo (BrnPlayerInfo.h) is not used here; its RaceCarState::Clear lives in a physics TU.
void BrnPhysics::Vehicle::RaceCarState::Clear() {}

namespace BrnDirector
{
namespace Camera
{
    // Stand-in frame camera: the two things the branch touches (the state flag word and the
    // effects block), by the production member names.
    struct Camera
    {
        s32           mState_uFlags;
        CameraEffects mEffects;
        CameraEffects& GetEffects() { return mEffects; }
    };

    static int         giStopCalls   = 0;
    static const char* gpcStopHook   = nullptr;
    static Camera*     gpStopCamera  = nullptr;
    static const EffectInterface* gpStopSource = nullptr;

    // Recording stand-in (production body: BrnDirectorEffectTrigger.cpp).
    void EnsureEffectIsStopped(Camera& lrCamera, const EffectInterface& lrSource, const char* lpcHook)
    {
        ++giStopCalls;
        gpcStopHook  = lpcHook;
        gpStopCamera = &lrCamera;
        gpStopSource = &lrSource;
    }
}

struct SharedCameraContainerStandIn
{
    int miForceCalls = 0;
    void ForcePrimaryGameplayBehaviourToFinish() { ++miForceCalls; }
};

struct SharedInfoStandIn
{
    const Camera::PlayerCrashInfo* mpPlayerCrashInfo;
    const EffectInterface*         mpEffectInterface;
};

struct ArbitratorStandIn
{
    SharedCameraContainerStandIn mSharedCameraContainer;
    void Branch(Camera::Camera& lrCameraInOut, SharedInfoStandIn& lrSharedInfo);
};
}

// void ArbitratorStandIn::Branch(...) { <the production branch, or the runner's labelled stand-in> }
#include "water_fade.inc"

using namespace BrnDirector;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

static EffectInterface gEffects;

int main()
{
    static Camera::Camera lCamera;
    static Camera::PlayerCrashInfo lCrashInfo;
    ArbitratorStandIn lArbitrator;
    SharedInfoStandIn lInfo = { &lCrashInfo, &gEffects };

    // 1. The player hit water: the fade starts and the gameplay camera is re-armed.
    lCrashInfo.Construct();
    lCrashInfo.mbHitWater = true;
    lCamera.mState_uFlags = 0x7;
    lCamera.mEffects.mbHasStopHookNameString    = true;    // a stale stop request
    lCamera.mEffects.muRequestedPostFxId        = 42u;     // a stale post-FX request
    lCamera.mEffects.mfStartHookNameBlendAmount = 0.25f;
    lArbitrator.Branch(lCamera, lInfo);

    Check(lCamera.mState_uFlags == 0x5, "hit water: camera flags &= ~2 (0x8226AFB0..C4), other bits kept");
    Check(lCamera.mEffects.mbHasStartHookNameString, "hit water: start hook requested (stb 1 camera+0x11F)");
    Check(std::strcmp(lCamera.mEffects.mStartHookNameString.mHookNameString, "BlackFade_Water") == 0,
          "hit water: the start hook is \"BlackFade_Water\" (0x8226AFC8)");
    Check(lCamera.mEffects.mfStartHookNameBlendAmount == 1.0f, "hit water: blend 1.0 (flt_82001C98, 0x8226AFD0)");
    Check(!lCamera.mEffects.mbHasStopHookNameString, "hit water: stop hook cleared (stb 0 camera+0x120)");
    Check(lCamera.mEffects.muRequestedPostFxId == 0u, "hit water: post-FX id cleared (stw 0 camera+0xE4)");
    Check(lArbitrator.mSharedCameraContainer.miForceCalls == 1,
          "hit water: the external gameplay behaviour is re-armed once (0x8226AFCC..0x8226AFF4)");
    Check(Camera::giStopCalls == 0, "hit water: EnsureEffectIsStopped is NOT called");

    // 2. No water: the fade is asked to stop, nothing else is touched.
    lCrashInfo.mbHitWater = false;
    lCamera.mState_uFlags = 0x7;
    lArbitrator.Branch(lCamera, lInfo);
    Check(Camera::giStopCalls == 1 && Camera::gpcStopHook != nullptr &&
          std::strcmp(Camera::gpcStopHook, "BlackFade_Water") == 0,
          "no water: EnsureEffectIsStopped(\"BlackFade_Water\") once (0x8226AFFC..0x8226B008)");
    Check(Camera::gpStopCamera == &lCamera && Camera::gpStopSource == &gEffects,
          "no water: on the frame camera and the shared info's effect interface (+0x30)");
    Check(lCamera.mState_uFlags == 0x7 && lArbitrator.mSharedCameraContainer.miForceCalls == 1,
          "no water: flags untouched, no re-arm");

    // 3. Only +0x27 matters: the neighbouring crash-info flags do not start the fade.
    lCrashInfo.mbWrecked = true;
    lCrashInfo.mbHardstopVsWall = true;
    lArbitrator.Branch(lCamera, lInfo);
    Check(Camera::giStopCalls == 2 && lArbitrator.mSharedCameraContainer.miForceCalls == 1,
          "wrecked / hard-stop without water: still the stop arm");

    std::printf("FxDirectorWaterFade: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
