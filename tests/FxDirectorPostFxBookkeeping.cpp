// FX-DIRECTOR (crash parity 2026-09-24): MainDirector::Update's post-FX id bookkeeping
// (X360 lines 850 / 853-866, 0x82275084 and 0x82275094..0x822750E4), extracted from
// src/GameSource/Director/BrnMainDirector.cpp by run_fxdirector_postfx.py and run on the REAL
// Camera::CameraState (flag sets, IsFlagSet, CopyFlagsToPrevious) and the REAL EffectInterface
// (the inlined RegisterStartingEffectWithId), with a two-member stand-in camera
// (mEffects.muRequestedPostFxId / mState).
//
// Checked against the ARTIST asm (r30 = MainDirector, lCamera = sp+0xC0, r27 = 1, r28 = 0):
//   0x82275084  old = mLastCamera.mEffects.muRequestedPostFxId      (read BEFORE the operator=)
//   0x82275090  mLastCamera = lCamera                               (keeps the camera's real id)
//   0x82275098  new == old && !(lCamera flags & 0x40 NEW_THIS_FRAME) -> lCamera id = 0 (0x822750C4)
//   0x822750CC  else if (new != 0) -> interface +0xD37 = 0, +0xD39 = 1, +0xCE8 = new
// A revision without the block runs the pre-fix tail (the flag roll and the copy only).
#include "types.hpp"
#include "GameSource/Director/Camera/BrnCameraState.h"
#include "GameSource/Director/Utils/BrnDirectorEffectTrigger.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // the block's [postfx-id] witness (silent here)
#include <cstdio>
#include <cstdlib>
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
namespace Log
{
    DebugPrint* gpDebugPrint = nullptr;   // the [postfx-id] witness stays silent
}
}

namespace BrnDirector
{
namespace Camera
{
    struct CameraEffectsStandIn
    {
        u32 muRequestedPostFxId;
    };

    // The two members the block touches, under the production accessor names.
    struct Camera
    {
        CameraEffectsStandIn mEffects;
        CameraState          mState;
        CameraEffectsStandIn&       GetEffects()       { return mEffects; }
        const CameraEffectsStandIn& GetEffects() const { return mEffects; }
        CameraState&                GetState()         { return mState; }
        const CameraState&          GetState() const   { return mState; }
    };
}

struct DirectorTailStandIn
{
    Camera::Camera mLastCamera;
    alignas(16) u8 maEffectInterface[sizeof(EffectInterface)];

    void Tail(Camera::Camera& lCamera)
    {
#include "fxdirector_postfx_block.inc"
    }
};
}

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

static DirectorTailStandIn gDirector;
static EffectInterface& Interface() { return *reinterpret_cast<EffectInterface*>(gDirector.maEffectInterface); }

// One director frame's tail: a finalised camera carrying luId, NEW_THIS_FRAME as given.
static Camera::Camera Frame(u32 luId, bool lbNewThisFrame)
{
    Camera::Camera lCamera;
    std::memset(&lCamera, 0, sizeof(lCamera));
    lCamera.mEffects.muRequestedPostFxId = luId;
    if (lbNewThisFrame)
    {
        lCamera.mState.mCurrentFlags.SetBit(Camera::CameraState::E_FLAG_NEW_THIS_FRAME);
    }
    gDirector.Tail(lCamera);
    return lCamera;
}

int main()
{
    const u32 KU_2DFLASH = 575791u;   // MomentPlayerStunt's 2dFlash id (an ICE take's authored hook)
    const u32 KU_OTHER   = 575800u;
    std::memset(&gDirector, 0, sizeof(gDirector));

    // 1. An ICE shot starts publishing its hook: the id changed (0 -> 2dFlash).
    Camera::Camera lPublished = Frame(KU_2DFLASH, false);
    Check(lPublished.mEffects.muRequestedPostFxId == KU_2DFLASH,
          "a changed id is published (the GUI bridge posts 495 once)");
    Check(Interface().HasCurrentEffectId() && Interface().GetCurrentEffectId() == KU_2DFLASH &&
          !Interface().HasCurrentEffectName(),
          "0x822750D4..E4: the interface registers it (+0xD39 = 1, +0xCE8 = id, +0xD37 = 0)");
    Check(gDirector.mLastCamera.mEffects.muRequestedPostFxId == KU_2DFLASH, "mLastCamera keeps the id");

    // 2. The same shot, next frame: unchanged, not new -> the published id is dropped.
    lPublished = Frame(KU_2DFLASH, false);
    Check(lPublished.mEffects.muRequestedPostFxId == 0,
          "0x822750C4: an unchanged id is NOT re-published (no 495 every frame)");
    Check(gDirector.mLastCamera.mEffects.muRequestedPostFxId == KU_2DFLASH,
          "the copy into mLastCamera happened BEFORE the drop (0x82275090 < 0x822750C4): the edge stays armed");

    // 3. A camera's first frame re-publishes even an unchanged id.
    std::memset(gDirector.maEffectInterface, 0, sizeof(gDirector.maEffectInterface));
    lPublished = Frame(KU_2DFLASH, true);
    Check(lPublished.mEffects.muRequestedPostFxId == KU_2DFLASH && Interface().HasCurrentEffectId() &&
          Interface().GetCurrentEffectId() == KU_2DFLASH,
          "E_FLAG_NEW_THIS_FRAME (bit 6, `& 0x40` @0x822750A4) re-publishes and re-registers");

    // 4. The shot ends (id -> 0): changed, but nothing to register.
    lPublished = Frame(0, false);
    Check(lPublished.mEffects.muRequestedPostFxId == 0 && Interface().GetCurrentEffectId() == KU_2DFLASH,
          "a change to 0 registers nothing (cmplwi r10, 0 @0x822750CC); the interface keeps the last id");

    // 5. A name-form effect was current: registering an id takes over from it.
    Interface().mbHasCurrentEffectName = true;
    lPublished = Frame(KU_OTHER, false);
    Check(!Interface().HasCurrentEffectName() && Interface().HasCurrentEffectId() &&
          Interface().GetCurrentEffectId() == KU_OTHER,
          "RegisterStartingEffectWithId drops the name form (+0xD37 = 0) and raises the id form");

    // 6. The flag roll still happens (CopyFlagsToPrevious, 0x82275088).
    {
        Camera::Camera lCamera;
        std::memset(&lCamera, 0, sizeof(lCamera));
        lCamera.mState.mCurrentFlags.SetBit(Camera::CameraState::E_FLAG_CRASH_CAMERA);
        gDirector.Tail(lCamera);
        Camera::Camera lNext;
        std::memset(&lNext, 0, sizeof(lNext));
        gDirector.Tail(lNext);
        Check(lNext.mState.mPreviousFlags.IsBitSet(Camera::CameraState::E_FLAG_CRASH_CAMERA),
              "the published camera's previous flags are last frame's current flags");
    }

    Check(gAsserts == 0, "no assert fired");

    std::printf("FxDirectorPostFxBookkeeping: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
