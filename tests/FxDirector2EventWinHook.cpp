// FX-DIRECTOR2 (crash parity 2026-09-25, CC-16): MainDirector::Update requests the "Event_Win(50)" screen effect for
// an ICE take whose POSTFX_HOOK key is 578869.
//
// The runner (run_fxdirector2_event_win_hook.py) lifts the revision's PRODUCTION statement out of MainDirector::Update
// -- `if (lCamera.GetEffects().muRequestedPostFxId == ...) { Camera::EnsureEffectIsPlaying(...); }`, the console's
// 0x82274524..0x82274554 -- into fxd2_eventwin_block.inc, and its three constants into fxd2_eventwin_const.inc. It
// compiles them inside a fixture that holds the director's EffectInterface span by its name (maEffectInterface).
// EnsureEffectIsPlaying is a recording stand-in (its body, BrnDirectorEffectTrigger.cpp, is shared with the live
// junkyard fades).
//
// Against the ARTIST body (MainDirector::Update @0x82274070):
//   0x82274524  lis r11, 8 ; lwz r10, lCamera +0xE4 (CameraEffects::muRequestedPostFxId) ; ori r11, r11, 0xD535
//   0x82274530  cmplw r10, r11 ; bne -> skip                              -- exactly 578869
//   0x82274538  r3 = &lCamera, r4 = this + 0x33C90 (the EffectInterface), r5 = "Event_Win(50)" (0x8200401C),
//               f1 = flt_82004018 (0x3F400000, 0.75)
//   0x82274554  bl BrnDirector::Camera::EnsureEffectIsPlaying
#include "GameSource/Director/Camera/Camera.h"
#include "GameSource/Director/Utils/BrnDirectorEffectTrigger.h"
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

// ---- recording stand-in ---------------------------------------------------------------------------------------
struct EnsureCall
{
    const void* mpCamera;
    const void* mpSource;
    const char* mpcHook;
    f32         mfBlend;
};
static EnsureCall gLastEnsure;
static int giEnsures = 0;

namespace BrnDirector
{
namespace Camera
{
    void Camera::Construct() {}
    void EnsureEffectIsPlaying(Camera& lrCamera, const EffectInterface& lrSource, const char* lpcHook, f32 lfBlend)
    {
        ++giEnsures;
        gLastEnsure = EnsureCall{ &lrCamera, &lrSource, lpcHook, lfBlend };
    }
}

namespace
{
#include "fxd2_eventwin_const.inc"
}

// The director's EffectInterface span, by its name, and the statement.
struct EventWinFixture
{
    alignas(16) u8 maEffectInterface[0x349D0 - 0x33C90];

    void Run(Camera::Camera& lCamera)
    {
#include "fxd2_eventwin_block.inc"
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
static u32 Bits(f32 lfValue) { u32 luBits; std::memcpy(&luBits, &lfValue, 4); return luBits; }

static EventWinFixture gDirector;

int main()
{
    Camera::Camera lCamera;

    lCamera.GetEffects().muRequestedPostFxId = 578869u;
    gDirector.Run(lCamera);
    Check(giEnsures == 1 && gLastEnsure.mpCamera == &lCamera && gLastEnsure.mpSource == gDirector.maEffectInterface,
          "K1 post-FX id 578869: Camera::EnsureEffectIsPlaying on the frame camera and the director's EffectInterface "
          "(this + 0x33C90)");
    Check(gLastEnsure.mpcHook != nullptr && std::strcmp(gLastEnsure.mpcHook, "Event_Win(50)") == 0,
          "K2 the hook is \"Event_Win(50)\" (the image string at 0x8200401C)");
    Check(Bits(gLastEnsure.mfBlend) == 0x3F400000u, "K3 at blend 0.75 (flt_82004018 == 0x3F400000)");

    const u32 kauOther[] = { 0u, 578868u, 578870u, 0x7BEC6u /* the null post-FX id */, 0x0008D534u, 0xFFFFFFFFu,
                             0x8D5350u };
    bool lbQuiet = true;
    for (u32 luId : kauOther)
    {
        const int liBefore = giEnsures;
        lCamera.GetEffects().muRequestedPostFxId = luId;
        gDirector.Run(lCamera);
        if (giEnsures != liBefore)
        {
            lbQuiet = false;
            std::printf("      requested for id %u\n", luId);
        }
    }
    Check(lbQuiet, "K4 any other id requests nothing (`cmplw ; bne`: exactly 578869)");
    Check(gAsserts == 0, "K5 no assert fired");

    std::printf("FxDirector2EventWinHook: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
