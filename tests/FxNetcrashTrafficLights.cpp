// crash parity FX-NETCRASH (2026-09-25): TrafficLightManager's event-countdown trio, compiled from the
// production BrnTrafficLightManager.cpp against the real BrnTrafficLightManager.h:
//   Construct         @0x82751708  600 x { time 0.0f, bits 0, E_STATE_GREEN } ; time 0.0f ; lights off ; state 3
//   SetCountdownValue @0x82751750  >= 0 lights on ; >= 2 RED ; == 1 AMBER ; == 0 GREEN for flt_82004270 (3.0f)
//                                  unless already GREEN
//   Update            @0x827517A8  GREEN only: time -= dt ; > 0 keeps going ; else time 0, lights off, state 3
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficLightManager.h"
#include "SharedClasses/Traffic/Junctions/BrnTrafficLightCollection.h"   // the .cpp's ETrafficLightState enumerators
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    char* gpcMessageBuffer = nullptr;
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int)
    {
        ++gAsserts;
        std::printf("ASSERT: %s\n", lpcMessage);
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { unsigned long long gxMessageFilterFlags = 0; }
}

namespace BrnTraffic
{
#include "light_bodies.inc"
}

using BrnTraffic::TrafficLightManager;
using BrnTraffic::TrafficLightRuntimeState;

static void Check(bool lbPass, const char* lpcWhat)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("FAIL: %s\n", lpcWhat);
    }
}

static TrafficLightManager* Fresh()
{
    static TrafficLightManager sManager;
    std::memset(&sManager, 0xCD, sizeof(sManager));   // whatever the module's storage held before Reset
    sManager.Construct();
    return &sManager;
}

static const TrafficLightRuntimeState& Record(const TrafficLightManager& lrManager, u32 luIndex)
{
    return reinterpret_cast<const TrafficLightRuntimeState&>(lrManager.maLightStates[luIndex]);
}

int main()
{
    // ---- Construct ----------------------------------------------------------------------------
    {
        TrafficLightManager& lrM = *Fresh();
        bool lbRecords = true;
        for (u32 luIndex = 0; luIndex < BrnTraffic::KU_MAX_TRAFFIC_LIGHT_INSTANCES; ++luIndex)
        {
            const TrafficLightRuntimeState& lrState = Record(lrM, luIndex);
            lbRecords = lbRecords && lrState.mfTimer == 0.0f && lrState.muState == 2 && lrState.muFlags == 0;
        }
        Check(lbRecords, "Construct: all 600 records GREEN (2), no time left, no state bits (0x82751720..0x82751738)");
        Check(!lrM.mbCountdownLights && lrM.meCountdownState == 3 && lrM.mfCountdownRemainingTime == 0.0f,
              "Construct: the countdown is off, state COUNT (3), no time left (0x82751740..0x82751748)");
    }

    // ---- SetCountdownValue: the countdown 3, 2, 1, 0 -----------------------------------------
    {
        TrafficLightManager& lrM = *Fresh();
        lrM.SetCountdownValue(3);
        Check(lrM.mbCountdownLights && lrM.meCountdownState == 0, "SetCountdownValue(3): every light RED (>= 2)");
        lrM.SetCountdownValue(2);
        Check(lrM.mbCountdownLights && lrM.meCountdownState == 0, "SetCountdownValue(2): still RED");
        lrM.SetCountdownValue(1);
        Check(lrM.mbCountdownLights && lrM.meCountdownState == 1, "SetCountdownValue(1): AMBER (beq to the stw of r11 == 1)");
        lrM.SetCountdownValue(0);
        Check(lrM.mbCountdownLights && lrM.meCountdownState == 2 && lrM.mfCountdownRemainingTime == 3.0f,
              "SetCountdownValue(0), the GO: GREEN for KF_COUNTDOWN_RED_TIME = 3.0f (flt_82004270)");

        // Already GREEN: a repeated 0 keeps the time that is left (beqlr 0x8275178C).
        lrM.Update(1.25f);
        lrM.SetCountdownValue(0);
        Check(lrM.meCountdownState == 2 && lrM.mfCountdownRemainingTime == 1.75f,
              "SetCountdownValue(0) while GREEN does not restart the 3 s");
    }
    {
        TrafficLightManager& lrM = *Fresh();
        lrM.SetCountdownValue(-1);
        Check(!lrM.mbCountdownLights && lrM.meCountdownState == 3,
              "SetCountdownValue(-1): nothing at all (blt past the stb, then bnelr)");
        lrM.SetCountdownValue(5);
        Check(lrM.mbCountdownLights && lrM.meCountdownState == 0, "SetCountdownValue(5): any display >= 2 is RED");
    }

    // ---- Update ----------------------------------------------------------------------------------
    {
        TrafficLightManager& lrM = *Fresh();
        lrM.SetCountdownValue(3);
        lrM.mfCountdownRemainingTime = 0.5f;
        lrM.Update(1.0f);
        Check(lrM.mbCountdownLights && lrM.meCountdownState == 0 && lrM.mfCountdownRemainingTime == 0.5f,
              "Update while RED: nothing runs down (bnelr 0x827517BC)");

        lrM.SetCountdownValue(0);
        const f32 lfStep = 1.0f / 60.0f;
        lrM.Update(lfStep);
        Check(lrM.mbCountdownLights && lrM.meCountdownState == 2 && lrM.mfCountdownRemainingTime == 3.0f - lfStep,
              "Update while GREEN: one f32 subtraction of the step (fsubs), still counting down");

        lrM.Update(2.0f);
        lrM.Update(1.0f);
        Check(!lrM.mbCountdownLights && lrM.meCountdownState == 3 && lrM.mfCountdownRemainingTime == 0.0f,
              "Update past zero: time 0, lights off, state COUNT (0x827517DC..0x827517EC)");

        lrM.mfCountdownRemainingTime = 7.0f;
        lrM.Update(1.0f);
        Check(lrM.mfCountdownRemainingTime == 7.0f && !lrM.mbCountdownLights,
              "Update with the countdown off: nothing (beqlr 0x827517B0)");
    }
    {
        TrafficLightManager& lrM = *Fresh();
        lrM.SetCountdownValue(0);
        lrM.Update(std::numeric_limits<f32>::quiet_NaN());
        Check(!lrM.mbCountdownLights && lrM.meCountdownState == 3 && lrM.mfCountdownRemainingTime == 0.0f,
              "Update with a NaN step ends the countdown: bgtlr is not taken on an unordered fcmpu");
    }
    {
        TrafficLightManager& lrM = *Fresh();
        lrM.SetCountdownValue(0);
        lrM.Update(3.0f);
        Check(!lrM.mbCountdownLights && lrM.meCountdownState == 3,
              "Update to exactly zero ends it too (0.0f is not > 0.0f)");
    }

    Check(offsetof(TrafficLightManager, mbCountdownLights) == 0x12C0
              && offsetof(TrafficLightManager, meCountdownState) == 0x12C4
              && offsetof(TrafficLightManager, mfCountdownRemainingTime) == 0x12C8
              && sizeof(TrafficLightManager) == 0x12CC,
          "the countdown members sit at +0x12C0 / +0x12C4 / +0x12C8 after the 600 records; sizeof 0x12CC");

    Check(gAsserts == 0, "no assert fired");
    std::printf("FxNetcrashTrafficLights: %u checks, %u failures (%u asserts)\n", gChecks, gFailures, gAsserts);
    return gFailures ? 1 : 0;
}
