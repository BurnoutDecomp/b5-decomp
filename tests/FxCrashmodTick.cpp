// FX-CRASHMOD (crash parity 2026-09-23), G63-D1: replay the production RaceCarCrash::Tick
// (ARTIST 0x827BF0B8) against RaceCarState fixtures. The console's "is the wreck still moving"
// test reads the owner's RaceCarState+0x340 (0x827BF1E8 `li r11,0x340` / 0x827BF20C
// `lvx128 v13,r3,r11`) -- mAngularVelocity, stored there by UpdateRaceCarState @0x825EC950/54 --
// and compares its length against flt_820CA5C4 (1.5f), next to |mfSpeedMPH| > flt_820CA5C0 (6.5f).
// The body under test is extracted verbatim from BrnRaceCarCrash.cpp by run_fxcrashmod_tick.py,
// together with the interface's GetPlayerActiveRaceCarIndex / GetRaceCarState.
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameSource/World/CrashModule/BrnRaceCarCrash.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameSource/BurnoutConstants.h"
#include "rw/math/vpu/vector3_operation.h"
#undef protected
#undef private
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>

static unsigned assertions = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* message, const char*, int) { ++assertions; std::fprintf(stderr, "ASSERT: %s\n", message); return 0; }
void* EndAssert() { return nullptr; }
} namespace Log { DebugPrint* gpDebugPrint = nullptr; void WriteToLog(const char*) {} }
namespace Message { u64 gxMessageFilterFlags = 0; } }

#include "fxcrashmod_tick_methods.inc"

using namespace BrnWorld;
using BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface;

namespace
{
    struct Outcome
    {
        f32  mfTimeStationary;
        f32  mfSecondsBeforeCleanup;
        f32  mfTimeCrashing;
        s32  miExtensions;
        bool mbEnding;
    };

    const u32 KU_OWNER = 2;

    // One Tick of a wreck owned by race car KU_OWNER, starting 0.3 s before cleanup with a
    // 0.1 s step (so this tick crosses the 0.25 s edge, flt_82003F40) and 2.0 s already
    // stationary (so only a reset of mfTimeStationary can make the extension fire).
    Outcome RunTick(RCEntityActiveRaceCarOutputInterface& lrInterface,
                    Vector3 lLinear, Vector3 lAngular, f32 lfSpeedMPH, bool lbOffline)
    {
        BrnPhysics::Vehicle::RaceCarState& lrState = lrInterface.maRaceCarStates[KU_OWNER];
        lrState.mLinearVelocity  = lLinear;
        lrState.mAngularVelocity = lAngular;
        lrState.mfSpeedMPH       = lfSpeedMPH;

        RaceCarCrash lCrash{};
        lCrash.mRaceCarVolumeInstanceId.muId = static_cast<u64>(0x01000000u | (KU_OWNER << 10)) << 32;
        lCrash.mfSecondsBeforeCleanup  = 0.3f;
        lCrash.mfTimeStationary        = 2.0f;
        lCrash.mfTimeCrashing          = 5.0f;
        lCrash.mi8NumCleanupExtensions = 0;

        bool lbEnding = false;
        lCrash.Tick(0.1f, &lrInterface, false, 10, lbOffline, true, true, &lbEnding);
        return Outcome{ lCrash.mfTimeStationary, lCrash.mfSecondsBeforeCleanup, lCrash.mfTimeCrashing,
                        static_cast<s32>(lCrash.mi8NumCleanupExtensions), lbEnding };
    }
}

int main()
{
    unsigned checks = 0, failures = 0;
    auto Check = [&](bool lbPass, const char* lpcName)
    {
        ++checks;
        if (!lbPass) { ++failures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
    };

    auto lpInterface = std::make_unique<RCEntityActiveRaceCarOutputInterface>();
    lpInterface->mePlayerActiveRaceCarIndex = static_cast<EActiveRaceCarIndex>(KU_OWNER);
    lpInterface->mbIsPlayerCarActive        = true;

    const Vector3 kZero{ 0.0f, 0.0f, 0.0f, 0.0f };

    // Values the float pipeline produces for "not reset" / "no extension" (same f32 ops as the body).
    volatile f32 lfStart = 2.0f, lfStep = 0.1f, lfCleanup = 0.3f, lfCrashing = 5.0f;
    const f32 kfStationaryKept = lfStart + lfStep;      // 2.1f
    const f32 kfCleanupAfter   = lfCleanup - lfStep;    // 0.2f
    const f32 kfCrashingAfter  = lfCrashing + lfStep;   // 5.1f

    // A. Spinning in place: |w| = 3 rad/s, no linear speed. Console: +0x340 length 3 > 1.5 ->
    //    mfTimeStationary = 0 -> on the 0.25 edge the extension fires (1.0f, flt_82001C98) and the
    //    ending message is retracted.
    {
        const Outcome o = RunTick(*lpInterface, kZero, Vector3{ 0.0f, 3.0f, 0.0f, 0.0f }, 0.0f, true);
        Check(o.mfTimeStationary == 0.0f,      "A: spinning wreck (|w|=3) resets mfTimeStationary");
        Check(o.mfSecondsBeforeCleanup == 1.0f, "A: spinning wreck is granted the 1.0 s extension");
        Check(o.miExtensions == 1,              "A: spinning wreck counts one extension");
        Check(!o.mbEnding,                      "A: spinning wreck does not send the ending message");
    }

    // B. Sliding without spin: |v| = 2 m/s, w = 0, mfSpeedMPH = 0. Console: +0x340 length 0 ->
    //    no reset; mfTimeStationary 2.1 >= 1.0 -> no extension; the crash ends.
    {
        const Outcome o = RunTick(*lpInterface, Vector3{ 2.0f, 0.0f, 0.0f, 0.0f }, kZero, 0.0f, true);
        Check(o.mfTimeStationary == kfStationaryKept, "B: linear velocity alone does not reset mfTimeStationary");
        Check(o.mfSecondsBeforeCleanup == kfCleanupAfter, "B: no extension for a non-spinning slide");
        Check(o.miExtensions == 0,                        "B: no extension counted");
        Check(o.mbEnding,                                 "B: the player's crash ending message is sent");
    }

    // C. Boundary: |w| == 1.5 exactly -> the console's `ble` skips the reset (strict >).
    {
        const Outcome o = RunTick(*lpInterface, kZero, Vector3{ 1.5f, 0.0f, 0.0f, 0.0f }, 0.0f, true);
        Check(o.mfTimeStationary == kfStationaryKept, "C: |w| == 1.5 is not > 1.5 (no reset)");
        Check(o.mbEnding,                             "C: |w| == 1.5 -> ending sent");
    }

    // D. Speed arm: |mfSpeedMPH| = |-7| > 6.5 (fabs, flt_820CA5C0) resets and extends.
    {
        const Outcome o = RunTick(*lpInterface, kZero, kZero, -7.0f, true);
        Check(o.mfTimeStationary == 0.0f && o.miExtensions == 1, "D: |speedMPH| 7 > 6.5 resets and extends");
        Check(o.mfTimeCrashing == kfCrashingAfter,               "D: mfTimeCrashing accumulates the step");
    }

    // E. Online: the whole player block is skipped (0x827BF18C beq) -- no accumulation, no reset.
    {
        const Outcome o = RunTick(*lpInterface, kZero, Vector3{ 0.0f, 3.0f, 0.0f, 0.0f }, 0.0f, false);
        Check(o.mfTimeStationary == 2.0f && o.mfTimeCrashing == 5.0f && o.miExtensions == 0 && o.mbEnding,
              "E: online wreck skips the stationary/extension block");
    }

    Check(assertions == 0, "valid fixtures raise no assertion");
    std::printf("FxCrashmodTick: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
