// crash parity FX-NETCRASH (2026-09-25): an event start sets up the lights at its start line.
// The production bodies, extracted by run_fxnetcrash_event_starts.py:
//   TrafficEntityModule::UpdateEventStarts   @0x82743B80  (BrnTrafficEntityModule.cpp)
//   TrafficLightManager::ChangeLightState    @0x827518E0  (BrnTrafficLightManager.cpp) + GetLightState @0x8274F9A0
//     (compiled in a second TU with its own ETrafficLightState home, as in the game)
//   HullRuntime::SetStoplineRed              @0x82706630  (BrnTrafficHullRuntime.cpp)
// against the real Hull / JunctionLogicBox / TrafficLightController / HullRuntime / TrafficLightManager types
// (JunctionLogicBox::GetNumLights / GetLight are the header's inline bodies). The module is a stand-in holding
// exactly the members UpdateEventStarts reads, with RECORDING doubles for GetHull, GetHullRuntimeSafe and
// KillAllTrafficInCylinder.
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Containers/CgsSet.h"
#include "SharedClasses/Traffic/BrnTrafficHull.h"
#include "SharedClasses/Traffic/Junctions/BrnJunctionLogicBox.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficHullRuntime.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficLightManager.h"
#include "rw/math/vpu/vector3_operation.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>

static unsigned gChecks = 0, gFailures = 0;
static std::string gAsserts;

namespace CgsDev
{
namespace Assert
{
    char* gpcMessageBuffer = nullptr;
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int)
    {
        gAsserts += lpcMessage;
        gAsserts += "|";
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { unsigned long long gxMessageFilterFlags = 0; }
}

namespace BrnTraffic
{
    // TrafficLightGotSmashed / GotRestored's diag backing store lives in the same .cpp; not extracted.
    struct KillCall { Vector3 mvCentre; f32 mfRadius; f32 mfHeight; bool mbIncludeStatic; };

    class TrafficEntityModule
    {
    public:
        void UpdateEventStarts();

        // ---- doubles --------------------------------------------------------------------------------
        const Hull* GetHull(u32 luIndex) const
        {
            ++muGetHullCalls;
            muLastHull = luIndex;
            return mpHull;
        }
        HullRuntime* GetHullRuntimeSafe(u32 luHull)
        {
            return (luHull < 4u) ? mapRuntimeForHull[luHull] : nullptr;
        }
        void KillAllTrafficInCylinder(Vector3 lvCentre, f32 lfRadius, f32 lfHeight, bool lbIncludeStatic)
        {
            if (muKills < 8u)
            {
                maKills[muKills] = KillCall{ lvCentre, lfRadius, lfHeight, lbIncludeStatic };
            }
            ++muKills;
        }

        // ---- the members UpdateEventStarts reads (names as in BrnTrafficEntityModule.h) ------------
        ::Set<u16, 72>      mActiveHulls;
        bool                mbAllowDivergentBehaviour;
        Vector3             maEventGridStartPositions[8];
        u8                  muNumberOfParticipantsInCurrentEvent;
        u32                 mTrafficLightTriggerId;
        bool                mbGameModeClearsTraffic;
        bool                mbNeedToSetUpLightsForEventStart;
        TrafficLightManager mTrafficLightManager;

        // ---- double state ---------------------------------------------------------------------------
        const Hull*  mpHull;
        HullRuntime* mapRuntimeForHull[4];
        mutable u32  muGetHullCalls;
        mutable u32  muLastHull;
        u32          muKills;
        KillCall     maKills[8];
    };

#include "event_starts_bodies.inc"
}

using namespace BrnTraffic;

static void Check(bool lbPass, const char* lpcWhat)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("FAIL: %s\n", lpcWhat);
    }
}

static TrafficLightRuntimeState& Light(TrafficEntityModule& lrM, u32 luInstance)
{
    return reinterpret_cast<TrafficLightRuntimeState&>(lrM.mTrafficLightManager.maLightStates[luInstance]);
}

// A hull with two junctions: junction 0 has two lights (light A: stop lines 3 and 7 in hulls 1 and 2, traffic
// lights 10 and 11; light B: stop line 5 in hull 3 (no runtime), traffic light 12); junction 1 has one light
// (no stop lines, traffic light 20).
static JunctionLogicBox gaJunctions[2];
static Hull             gHull;
static HullRuntime      gaRuntime[3];

static void BuildHull()
{
    std::memset(gaJunctions, 0, sizeof(gaJunctions));
    std::memset(&gHull, 0, sizeof(gHull));
    std::memset(gaRuntime, 0, sizeof(gaRuntime));

    JunctionLogicBox& lrJ0 = gaJunctions[0];
    lrJ0.muNumLights = 2;
    TrafficLightController& lrA = lrJ0.maTrafficLightControllers[0];
    lrA.muNumStopLines = 2;
    lrA.mauStopLineIds[0] = 3;   lrA.mauStopLineHulls[0] = 1;
    lrA.mauStopLineIds[1] = 7;   lrA.mauStopLineHulls[1] = 2;
    lrA.muNumTrafficLights = 2;
    lrA.mauTrafficLightIds[0] = 10;
    lrA.mauTrafficLightIds[1] = 11;
    TrafficLightController& lrB = lrJ0.maTrafficLightControllers[1];
    lrB.muNumStopLines = 1;
    lrB.mauStopLineIds[0] = 5;   lrB.mauStopLineHulls[0] = 3;
    lrB.muNumTrafficLights = 1;
    lrB.mauTrafficLightIds[0] = 12;
    // A controller past muNumLights that must never be read.
    lrJ0.maTrafficLightControllers[2].muNumTrafficLights = 1;
    lrJ0.maTrafficLightControllers[2].mauTrafficLightIds[0] = 99;

    JunctionLogicBox& lrJ1 = gaJunctions[1];
    lrJ1.muNumLights = 1;
    lrJ1.maTrafficLightControllers[0].muNumStopLines = 0;
    lrJ1.maTrafficLightControllers[0].muNumTrafficLights = 1;
    lrJ1.maTrafficLightControllers[0].mauTrafficLightIds[0] = 20;

    gHull.muNumJunctions = 2;
    gHull.mpaJunctions   = gaJunctions;

    for (HullRuntime& lrRuntime : gaRuntime)
    {
        lrRuntime.mbPrepared           = true;
        lrRuntime.muNumStoplinesInHull = 16;
    }
}

static void Fresh(TrafficEntityModule& lrM, bool lbOnline)
{
    std::memset(&lrM, 0, sizeof(lrM));
    lrM.mActiveHulls.Clear();
    lrM.mTrafficLightManager.Construct();   // every light GREEN, no time, no bits
    lrM.mbAllowDivergentBehaviour            = !lbOnline;
    lrM.muNumberOfParticipantsInCurrentEvent = 3;
    for (u32 luSlot = 0; luSlot < 3; ++luSlot)
    {
        lrM.maEventGridStartPositions[luSlot] = Vector3{ 100.0f + luSlot, 2.0f, -50.0f - luSlot, 0.0f };
    }
    lrM.mTrafficLightTriggerId          = (0x0123u << 8) | 0x04u;   // hull 0x123, junction byte 4
    lrM.mbGameModeClearsTraffic         = true;
    lrM.mbNeedToSetUpLightsForEventStart = true;
    lrM.mpHull                           = &gHull;
    lrM.mapRuntimeForHull[1]             = &gaRuntime[1];
    lrM.mapRuntimeForHull[2]             = &gaRuntime[2];
    lrM.mapRuntimeForHull[3]             = nullptr;   // hull 3 has no runtime: its stop line is skipped
    gAsserts.clear();
}

int main()
{
    BuildHull();
    TrafficEntityModule lM;

    // ---- nothing to set up ------------------------------------------------------------------------------
    Fresh(lM, false);
    lM.mbNeedToSetUpLightsForEventStart = false;
    lM.mActiveHulls.Insert(0x123);
    lM.UpdateEventStarts();
    Check(lM.muGetHullCalls == 0 && lM.muKills == 0 && Light(lM, 10).muState == 2 && gAsserts.empty(),
          "the flag is clear: nothing happens (0x82743BA0 beq -> return)");

    // ---- the trigger's hull is not active yet: wait, keep the flag ----------------------------------------
    Fresh(lM, false);
    lM.UpdateEventStarts();
    Check(lM.mbNeedToSetUpLightsForEventStart && lM.muGetHullCalls == 0 && Light(lM, 10).muState == 2,
          "the trigger's hull (id >> 8 & 0xFFFF) is not in mActiveHulls: return, the flag stays set (0x82743C64)");

    // ---- offline: the lights and stop lines, no grid clear -----------------------------------------------
    Fresh(lM, false);
    lM.mActiveHulls.Insert(0x123);
    Light(lM, 12).muState = 0;        // light 12 is already RED: ChangeLightState leaves it alone
    Light(lM, 12).mfTimer = 0.25f;
    lM.UpdateEventStarts();
    Check(!lM.mbNeedToSetUpLightsForEventStart, "the set-up happens once: the flag is cleared (stb 0, 0x82743C6C)");
    Check(lM.muGetHullCalls == 1 && lM.muLastHull == 0x123, "GetHull(luHull) with the trigger id's hull");
    Check(lM.muKills == 0, "offline (mbAllowDivergentBehaviour): no grid clear (0x82743C7C bne)");
    Check(gaRuntime[1].mabStoplineRedState[3] && gaRuntime[2].mabStoplineRedState[7],
          "every stop line of the junction's lights goes red in its own hull's runtime (SetStoplineRed, true)");
    Check(!gaRuntime[1].mabStoplineRedState[5] && !gaRuntime[2].mabStoplineRedState[5],
          "a stop line whose hull has no runtime is skipped (GetHullRuntimeSafe NULL)");
    Check(Light(lM, 10).muState == 1 && Light(lM, 10).mfTimer == 2.0f && (Light(lM, 10).muFlags & 7u) == 2u
              && Light(lM, 11).muState == 1 && Light(lM, 20).muState == 1,
          "every light of every junction turns AMBER for KF_AMBER_TIME 2.0f with state bit 2 (ChangeLightState, true)");
    Check(Light(lM, 12).muState == 0 && Light(lM, 12).mfTimer == 0.25f,
          "a light already RED stays RED, its time untouched (0x82751940 beq)");
    Check(Light(lM, 99).muState == 2, "a controller past the junction's muNumLights is never read");
    Check(gAsserts.empty(), "no tripwire on a valid set-up");

    // Run again: the flag is clear now, nothing repeats.
    const u32 luHullCalls = lM.muGetHullCalls;
    lM.UpdateEventStarts();
    Check(lM.muGetHullCalls == luHullCalls, "a second frame does nothing");

    // ---- online: the grid slots are cleared first ----------------------------------------------------------
    BuildHull();
    Fresh(lM, true);
    lM.mActiveHulls.Insert(0x123);
    lM.UpdateEventStarts();
    bool lbKills = lM.muKills == 3;
    for (u32 luSlot = 0; lbKills && luSlot < 3; ++luSlot)
    {
        const KillCall& lrKill = lM.maKills[luSlot];
        lbKills = lrKill.mvCentre.x == 100.0f + luSlot && lrKill.mvCentre.z == -50.0f - luSlot
                  && lrKill.mfRadius == 30.0f && lrKill.mfHeight == 10.0f && lrKill.mbIncludeStatic;
    }
    Check(lbKills, "online: KillAllTrafficInCylinder(grid slot, flt_820BA5E8 30.0f, flt_820BA5E4 10.0f, true) per "
                   "participant");

    // ---- the tripwires --------------------------------------------------------------------------------------
    BuildHull();
    Fresh(lM, false);
    lM.mbGameModeClearsTraffic = false;
    lM.mActiveHulls.Insert(0x123);
    lM.UpdateEventStarts();
    Check(gAsserts.find("mbGameModeClearsTraffic") != std::string::npos,
          "the .cpp 9600 tripwire: the flag without a traffic-clearing mode");

    BuildHull();
    Fresh(lM, false);
    lM.mTrafficLightTriggerId = 0x00FFFF04u;   // middle 16 bits all ones: invalid
    lM.UpdateEventStarts();
    Check(gAsserts.find("mTrafficLightTriggerId.IsValid()") != std::string::npos,
          "the .cpp 9601 tripwire: an invalid trigger id (middle bits all ones)");

    BuildHull();
    Fresh(lM, true);
    lM.maEventGridStartPositions[1].y = std::numeric_limits<f32>::quiet_NaN();
    lM.mActiveHulls.Insert(0x123);
    lM.UpdateEventStarts();
    Check(gAsserts.find("IsValid( maEventGridStartPositions[luParticipant] )") != std::string::npos,
          "the .cpp 9617 tripwire: a NaN grid slot");

    // ---- ChangeLightState's GREEN arm ------------------------------------------------------------------------
    Fresh(lM, false);
    Light(lM, 30).muState = 0;
    Light(lM, 30).mfTimer = 1.5f;
    Light(lM, 30).muFlags = 0xF9;   // high bits kept, the low three replaced
    lM.mTrafficLightManager.ChangeLightState(30, false);
    Check(Light(lM, 30).muState == 2 && Light(lM, 30).muFlags == 0xFC && Light(lM, 30).mfTimer == 1.5f,
          "ChangeLightState(false): GREEN, flags (f & 0xF8) | 4, the time untouched (0x82751978..0x82751990)");
    lM.mTrafficLightManager.ChangeLightState(600, true);
    Check(gAsserts.find("luInstance < KU_MAX_TRAFFIC_LIGHT_INSTANCES") != std::string::npos,
          "ChangeLightState's :391 bound tripwire at 600 (cmplwi 0x258)");

    std::printf("FxNetcrashEventStarts: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
