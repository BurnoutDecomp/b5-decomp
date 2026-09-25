// crash parity FX-TRAFFICLIGHTS (2026-09-25): the traffic lights cycle.
// The production bodies, extracted by run_fxtrafficlights_cycle.py:
//   TrafficEntityModule::UpdateJunctions @0x82723EA0 and RecalculateActiveHulls' stop-line release walk
//     (0x8274D4B0..0x8274D88C) from BrnTrafficEntityModule.cpp (the walk as the body of ReleaseWalk below);
//   HullRuntime::SetStoplineRed @0x82706630 (BrnTrafficHullRuntime.cpp);
//   TrafficLightManager::GetLightState / Construct / ChangeLightState (BrnTrafficLightManager.cpp) in a SECOND TU
//     with that .cpp's ETrafficLightState home, and the whole BrnTrafficLightRuntimeState.cpp
//     (TrafficLightRuntimeState::Update @0x827515D8 + TrafficLightManager::UpdateHull @0x827517F8) in a THIRD
//     (the two ETrafficLightState homes cannot share a TU -- BL-1 -- exactly as in the game);
//   JunctionLogicBox::GetStateTiming / GetTimeInState / IsLightRed / GetNumLights / GetLight: the header's inlines.
// The module is a stand-in holding exactly the members those bodies read, with doubles for GetHull,
// GetHullRuntime and GetHullRuntimeSafe. The numbers come from the ARTIST asm (see the runner).
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Containers/CgsSet.h"
#include "SharedClasses/Traffic/BrnTrafficHull.h"
#include "SharedClasses/Traffic/Junctions/BrnJunctionLogicBox.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficHullRuntime.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficLightManager.h"
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
    class TrafficEntityModule
    {
    public:
        typedef ::Set<u16, 72> ActiveHullSet;

        void UpdateJunctions();
        void ReleaseWalk(ActiveHullSet* lpOutOldHulls);   // the production block from RecalculateActiveHulls

        // ---- doubles -----------------------------------------------------------------------------------------
        const Hull* GetHull(u32 luIndex) const
        {
            return (luIndex < 8u) ? mapHulls[luIndex] : nullptr;
        }
        HullRuntime* GetHullRuntime(u32 luHull)
        {
            HullRuntime* const lpRuntime = (luHull < 8u) ? mapRuntimes[luHull] : nullptr;
            if (lpRuntime == nullptr)
            {
                gAsserts += "GetHullRuntime: no runtime|";   // the .h 2249 tripwire
            }
            return lpRuntime;
        }
        HullRuntime* GetHullRuntimeSafe(u32 luHull)
        {
            return (luHull < 8u) ? mapRuntimes[luHull] : nullptr;
        }

        // ---- the members the bodies read (names as in BrnTrafficEntityModule.h) --------------------------------
        ActiveHullSet       mActiveHulls;
        f32                 mfDEBUGTrafficLightTimeMultiplier;
        f32                 mfSimTimeSinceLastDecision;
        u32                 mTrafficLightTriggerId;
        bool                mbEnsureTrafficLightDelay;
        TrafficLightManager mTrafficLightManager;

        // ---- double state ------------------------------------------------------------------------------------
        const Hull*  mapHulls[8];
        HullRuntime* mapRuntimes[8];
    };

#include "cycle_bodies.inc"
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

static u32 Bits(f32 lfValue)
{
    u32 luBits;
    std::memcpy(&luBits, &lfValue, sizeof(luBits));
    return luBits;
}

// fmadds (0x82724244) oracle for the exact cases: tenths * 0.1f is exact in f64 (<= 40 bits), and so is its sum
// with a time that is a multiple of 2^-27 below 2^5; the f64 sum rounded to f32 once is the fused result.
static f32 FusedExact(u16 luTenths, f32 lfTime)
{
    return static_cast<f32>(static_cast<f64>(luTenths) * static_cast<f64>(0.1f) + static_cast<f64>(lfTime));
}
static f32 NotFused(u16 luTenths, f32 lfTime)
{
    const f32 lfProduct = static_cast<f32>(luTenths) * 0.1f;
    return lfProduct + lfTime;
}

static TrafficLightRuntimeState& Light(TrafficEntityModule& lrM, u32 luInstance)
{
    return reinterpret_cast<TrafficLightRuntimeState&>(lrM.mTrafficLightManager.maLightStates[luInstance]);
}

// ---- the world ------------------------------------------------------------------------------------------------
// hull 5: junction 0 -- 3 phases of 10 s / 3 s / 25 s (100 / 30 / 250 tenths); light 0 is red in phase 0, both in
//         phase 1, light 1 in phase 2 (masks 0x01 / 0x03 / 0x02). Light 0: stop lines 5:2 and 6:1 (a neighbour's),
//         instances 10 and 11. Light 1: stop line 7:0 (hull 7 has no runtime), instance 12. A third controller,
//         past muNumLights, must never be read (instance 99).
//         junction 1 -- 2 phases of 5 s (50 / 50); light 0 red in phase 0: stop line 5:3, instance 13.
//         Light instances [10, 14); the light-trigger lookup maps trigger 3 to junction 1 and trigger 2 to 0.
// hull 6: junction 0 -- 2 phases of 4 s (40 / 40); light 0 red in phase 0: stop line 6:4, instance 20.
//         Light instances [20, 21).
// hull 7: no junctions, no runtime, never active.
static JunctionLogicBox gaJunctions5[2];
static JunctionLogicBox gaJunctions6[1];
static u8               gauLookup5[4];
static u8               gauLookup6[4];
static Hull             gHull5, gHull6, gHull7;
static HullRuntime      gRuntime5, gRuntime6;

// HullRuntime::Prepare @0x82751438, by hand: no stop line red, every junction at time 0 in its LAST phase.
static void PrepareRuntime(HullRuntime& lrRuntime, const Hull& lrHull, u16 luHull)
{
    std::memset(&lrRuntime, 0, sizeof(lrRuntime));
    for (u32 luJunction = 0; luJunction < lrHull.muNumJunctions; ++luJunction)
    {
        lrRuntime.mafJunctionStateChangeTimes[luJunction] = 0.0f;
        lrRuntime.mauJunctionCurrentStates[luJunction] =
            static_cast<u8>(lrHull.mpaJunctions[luJunction].GetNumStates() - 1);
    }
    lrRuntime.mbPrepared           = true;
    lrRuntime.muHullIndex          = luHull;
    lrRuntime.muNumStoplinesInHull = lrHull.muNumStoplines;
}

static void BuildWorld(TrafficEntityModule& lrM)
{
    std::memset(gaJunctions5, 0, sizeof(gaJunctions5));
    std::memset(gaJunctions6, 0, sizeof(gaJunctions6));
    std::memset(&gHull5, 0, sizeof(gHull5));
    std::memset(&gHull6, 0, sizeof(gHull6));
    std::memset(&gHull7, 0, sizeof(gHull7));

    JunctionLogicBox& lrJ0 = gaJunctions5[0];
    lrJ0.muNumStates = 3;
    lrJ0.muNumLights = 2;
    lrJ0.mauStateTimings[0] = 100;
    lrJ0.mauStateTimings[1] = 30;
    lrJ0.mauStateTimings[2] = 250;
    lrJ0.mauStoppedLightStates[0] = 0x01;
    lrJ0.mauStoppedLightStates[1] = 0x03;
    lrJ0.mauStoppedLightStates[2] = 0x02;
    TrafficLightController& lrA = lrJ0.maTrafficLightControllers[0];
    lrA.muNumStopLines = 2;
    lrA.mauStopLineHulls[0] = 5;  lrA.mauStopLineIds[0] = 2;
    lrA.mauStopLineHulls[1] = 6;  lrA.mauStopLineIds[1] = 1;
    lrA.muNumTrafficLights = 2;
    lrA.mauTrafficLightIds[0] = 10;
    lrA.mauTrafficLightIds[1] = 11;
    TrafficLightController& lrB = lrJ0.maTrafficLightControllers[1];
    lrB.muNumStopLines = 1;
    lrB.mauStopLineHulls[0] = 7;  lrB.mauStopLineIds[0] = 0;
    lrB.muNumTrafficLights = 1;
    lrB.mauTrafficLightIds[0] = 12;
    lrJ0.maTrafficLightControllers[2].muNumTrafficLights = 1;
    lrJ0.maTrafficLightControllers[2].mauTrafficLightIds[0] = 99;

    JunctionLogicBox& lrJ1 = gaJunctions5[1];
    lrJ1.muNumStates = 2;
    lrJ1.muNumLights = 1;
    lrJ1.mauStateTimings[0] = 50;
    lrJ1.mauStateTimings[1] = 50;
    lrJ1.mauStoppedLightStates[0] = 0x01;
    lrJ1.mauStoppedLightStates[1] = 0x00;
    lrJ1.maTrafficLightControllers[0].muNumStopLines = 1;
    lrJ1.maTrafficLightControllers[0].mauStopLineHulls[0] = 5;
    lrJ1.maTrafficLightControllers[0].mauStopLineIds[0] = 3;
    lrJ1.maTrafficLightControllers[0].muNumTrafficLights = 1;
    lrJ1.maTrafficLightControllers[0].mauTrafficLightIds[0] = 13;

    JunctionLogicBox& lrK0 = gaJunctions6[0];
    lrK0.muNumStates = 2;
    lrK0.muNumLights = 1;
    lrK0.mauStateTimings[0] = 40;
    lrK0.mauStateTimings[1] = 40;
    lrK0.mauStoppedLightStates[0] = 0x01;
    lrK0.mauStoppedLightStates[1] = 0x00;
    lrK0.maTrafficLightControllers[0].muNumStopLines = 1;
    lrK0.maTrafficLightControllers[0].mauStopLineHulls[0] = 6;
    lrK0.maTrafficLightControllers[0].mauStopLineIds[0] = 4;
    lrK0.maTrafficLightControllers[0].muNumTrafficLights = 1;
    lrK0.maTrafficLightControllers[0].mauTrafficLightIds[0] = 20;

    std::memset(gauLookup5, 0, sizeof(gauLookup5));
    std::memset(gauLookup6, 0, sizeof(gauLookup6));
    gauLookup5[2] = 0;
    gauLookup5[3] = 1;

    gHull5.muNumJunctions = 2;
    gHull5.mpaJunctions = gaJunctions5;
    gHull5.muNumStoplines = 8;
    gHull5.muFirstTrafficLight = 10;
    gHull5.muLastTrafficLight = 14;
    gHull5.mpaLightTriggerJunctionLookup = gauLookup5;
    gHull6.muNumJunctions = 1;
    gHull6.mpaJunctions = gaJunctions6;
    gHull6.muNumStoplines = 8;
    gHull6.muFirstTrafficLight = 20;
    gHull6.muLastTrafficLight = 21;
    gHull6.mpaLightTriggerJunctionLookup = gauLookup6;
    gHull7.muNumStoplines = 8;

    PrepareRuntime(gRuntime5, gHull5, 5);
    PrepareRuntime(gRuntime6, gHull6, 6);

    std::memset(&lrM, 0, sizeof(lrM));
    lrM.mActiveHulls.Clear();
    lrM.mActiveHulls.Insert(5);
    lrM.mActiveHulls.Insert(6);
    lrM.mapHulls[5] = &gHull5;
    lrM.mapHulls[6] = &gHull6;
    lrM.mapHulls[7] = &gHull7;
    lrM.mapRuntimes[5] = &gRuntime5;
    lrM.mapRuntimes[6] = &gRuntime6;
    lrM.mfDEBUGTrafficLightTimeMultiplier = 1.0f;   // Construct's flt_82001C98
    lrM.mfSimTimeSinceLastDecision = 0.1f;          // the decision step
    lrM.mTrafficLightTriggerId = 0xFFFFFFFFu;
    lrM.mbEnsureTrafficLightDelay = false;
    lrM.mTrafficLightManager.Construct();           // every instance GREEN, no time, no bits
    gAsserts.clear();
}

// ---- the long-run oracle: the console's arithmetic, step by step, in f32 --------------------------------------
struct JunctionModel
{
    const JunctionLogicBox* mpJunction;
    u8  muState;
    f32 mfTime;
};
struct LightModel
{
    u8  muState;   // RED 0, AMBER 1, GREEN 2
    f32 mfTime;
};

static void ModelChangeLight(LightModel& lrLight, bool lbRed)
{
    if (lbRed)
    {
        if (lrLight.muState == 0)
        {
            return;   // ChangeLightState 0x82751940: a RED light stays red
        }
        lrLight.muState = 1;
        lrLight.mfTime  = 2.0f;   // KF_AMBER_TIME flt_820C0F6C
        return;
    }
    lrLight.muState = 2;
}

static void ModelRunDown(LightModel& lrLight, f32 lfDelta)
{
    if (lrLight.muState == 1)
    {
        lrLight.mfTime = lrLight.mfTime - lfDelta;   // fsubs 0x827516C8
        if (!(lrLight.mfTime > 0.0f))                // bgt 0x827516D4
        {
            lrLight.mfTime  = 0.0f;
            lrLight.muState = 0;
        }
    }
}

int main()
{
    TrafficEntityModule lM;

    // ---- A. the first decision frame after Prepare ----------------------------------------------------------------
    BuildWorld(lM);
    lM.UpdateJunctions();
    Check(gRuntime5.mauJunctionCurrentStates[0] == 0 && gRuntime5.mauJunctionCurrentStates[1] == 0
              && gRuntime6.mauJunctionCurrentStates[0] == 0,
          "A1 the first frame expires every Prepare'd junction (0 - 0.1 <= 0) and wraps phase numStates-1 to 0 "
          "((s + 1) % n, divw 0x827241D4)");
    Check(Bits(gRuntime5.mafJunctionStateChangeTimes[0]) == Bits(FusedExact(100, -0.1f))
              && Bits(FusedExact(100, -0.1f)) != Bits(NotFused(100, -0.1f)),
          "A2 the new phase's time is ONE rounding of tenths * 0.1f + time (fmadds f0, f13, f29, f0 @0x82724244): "
          "0x411E6667, where rounding the product first gives 0x411E6666");
    Check(Bits(gRuntime5.mafJunctionStateChangeTimes[1]) == Bits(FusedExact(50, -0.1f))
              && Bits(gRuntime6.mafJunctionStateChangeTimes[0]) == Bits(FusedExact(40, -0.1f)),
          "A3 every junction of every active hull: 5 s and 4 s phases, fused");
    Check(gRuntime5.mabStoplineRedState[2] && gRuntime6.mabStoplineRedState[1] && gRuntime5.mabStoplineRedState[3]
              && gRuntime6.mabStoplineRedState[4] && !gRuntime5.mabStoplineRedState[0],
          "A4 phase 0's red lights set their stop lines red, in their own hull's runtime (5:2, the neighbour's 6:1, "
          "5:3, 6:4) -- SetStoplineRed(mauStopLineIds[s], IsLightRed) 0x827243D0");
    const f32 lfAmberAfterOne = 2.0f - 0.1f;
    Check(Light(lM, 10).muState == 1 && Light(lM, 11).muState == 1 && Light(lM, 13).muState == 1
              && Light(lM, 20).muState == 1 && Bits(Light(lM, 10).mfTimer) == Bits(lfAmberAfterOne)
              && Bits(Light(lM, 20).mfTimer) == Bits(lfAmberAfterOne) && (Light(lM, 10).muFlags & 7u) == 2u,
          "A5 a light turned red goes AMBER for KF_AMBER_TIME 2.0f (ChangeLightState 0x82724414), and the same "
          "frame's UpdateHull (0x827244B0) already ran it down by 0.1 s");
    Check(Light(lM, 12).muState == 2 && (Light(lM, 12).muFlags & 7u) == 4u && Light(lM, 12).mfTimer == 0.0f,
          "A6 a light not red in the phase goes GREEN at once (ChangeLightState(false))");
    Check(Light(lM, 99).muState == 2 && Light(lM, 99).mfTimer == 0.0f,
          "A7 a controller past the junction's muNumLights is never read");
    Check(!gRuntime5.mabStoplineRedState[0] && gAsserts.empty(),
          "A8 a stop line whose hull has no runtime (7:0) is skipped -- GetHullRuntimeSafe NULL, no tripwire");

    // ---- B. 600 decision frames (60 s) against the console's arithmetic -------------------------------------------
    {
        BuildWorld(lM);
        JunctionModel laModel[3] = { { &gaJunctions5[0], 2, 0.0f }, { &gaJunctions5[1], 1, 0.0f },
                                     { &gaJunctions6[0], 1, 0.0f } };
        f32* const lapTimes[3] = { &gRuntime5.mafJunctionStateChangeTimes[0], &gRuntime5.mafJunctionStateChangeTimes[1],
                                   &gRuntime6.mafJunctionStateChangeTimes[0] };
        u8* const lapStates[3] = { &gRuntime5.mauJunctionCurrentStates[0], &gRuntime5.mauJunctionCurrentStates[1],
                                   &gRuntime6.mauJunctionCurrentStates[0] };
        LightModel laLights[3] = { { 2, 0.0f }, { 2, 0.0f }, { 2, 0.0f } };   // instances 10 (J0 L0), 13, 20
        const u32 lauInstances[3] = { 10, 13, 20 };
        u32 luJunctionMismatches = 0, luLightMismatches = 0, luChanges = 0, luRedRuns = 0;
        for (u32 luFrame = 0; luFrame < 600; ++luFrame)
        {
            lM.UpdateJunctions();
            for (u32 luJunction = 0; luJunction < 3; ++luJunction)
            {
                JunctionModel& lrModel = laModel[luJunction];
                lrModel.mfTime = lrModel.mfTime - 0.1f;   // 1.0f * 0.1f, fsubs
                if (!(lrModel.mfTime > 0.0f))
                {
                    ++luChanges;
                    lrModel.muState = static_cast<u8>((lrModel.muState + 1) % lrModel.mpJunction->muNumStates);
                    lrModel.mfTime  = std::fma(static_cast<f32>(lrModel.mpJunction->mauStateTimings[lrModel.muState]),
                                               0.1f, lrModel.mfTime);
                    const bool lbRed = (lrModel.mpJunction->mauStoppedLightStates[lrModel.muState] & 1u) != 0;
                    ModelChangeLight(laLights[luJunction], lbRed);
                }
                if (*lapStates[luJunction] != lrModel.muState || Bits(*lapTimes[luJunction]) != Bits(lrModel.mfTime))
                {
                    ++luJunctionMismatches;
                }
            }
            for (u32 luLight = 0; luLight < 3; ++luLight)
            {
                const u8 luBefore = laLights[luLight].muState;
                ModelRunDown(laLights[luLight], 0.1f);
                luRedRuns += (luBefore == 1 && laLights[luLight].muState == 0) ? 1u : 0u;
                const TrafficLightRuntimeState& lrLight = Light(lM, lauInstances[luLight]);
                if (lrLight.muState != laLights[luLight].muState || Bits(lrLight.mfTimer) != Bits(laLights[luLight].mfTime))
                {
                    ++luLightMismatches;
                }
            }
        }
        std::printf("B: %u phase changes, %u AMBER->RED run-downs over 600 frames\n", luChanges, luRedRuns);
        Check(luJunctionMismatches == 0 && luChanges >= 30,
              "B1 600 decision frames: every junction's phase and time bit-identical to the console's arithmetic "
              "(fsubs, bgt, (s + 1) % n, fmadds)");
        Check(luLightMismatches == 0 && luRedRuns > 5,
              "B2 every light's AMBER runs down to RED on the frame the console's does (UpdateHull -> "
              "TrafficLightRuntimeState::Update), and goes GREEN at once on a green phase");
        Check(gAsserts.empty(), "B3 no tripwire in 600 frames");
    }

    // ---- C. the two deltas -----------------------------------------------------------------------------------------
    BuildWorld(lM);
    lM.mfDEBUGTrafficLightTimeMultiplier = 2.0f;
    lM.UpdateJunctions();
    Check(Bits(gRuntime5.mafJunctionStateChangeTimes[0]) == Bits(FusedExact(100, 0.0f - 2.0f * 0.1f)),
          "C1 the junction timers run on mfDEBUGTrafficLightTimeMultiplier * mfSimTimeSinceLastDecision "
          "(fmuls f30, f0, f13 @0x82723F10)");
    Check(Bits(Light(lM, 10).mfTimer) == Bits(lfAmberAfterOne),
          "C2 UpdateHull runs the lights on the UNSCALED mfSimTimeSinceLastDecision (lfs f1, 0(r16) @0x827244A4)");

    // ---- D. the event-start hold -------------------------------------------------------------------------------------
    BuildWorld(lM);
    lM.UpdateJunctions();
    const f32 lfHeldTime  = gRuntime5.mafJunctionStateChangeTimes[1];
    const u8  luHeldState = gRuntime5.mauJunctionCurrentStates[1];
    const f32 lfOtherTime = gRuntime5.mafJunctionStateChangeTimes[0];
    lM.mbEnsureTrafficLightDelay = true;
    lM.mTrafficLightTriggerId = (5u << 8) | 3u;   // hull 5, trigger 3 -> junction 1
    for (u32 luFrame = 0; luFrame < 60; ++luFrame)
    {
        lM.UpdateJunctions();
    }
    Check(Bits(gRuntime5.mafJunctionStateChangeTimes[1]) == Bits(lfHeldTime)
              && gRuntime5.mauJunctionCurrentStates[1] == luHeldState,
          "D1 mbEnsureTrafficLightDelay: the trigger's junction (lookup[id & 0xFF] in hull (id >> 8) & 0xFFFF) is "
          "skipped, its time frozen (0x82724154..0x82724194)");
    Check(gRuntime5.mafJunctionStateChangeTimes[0] != lfOtherTime && gRuntime6.mauJunctionCurrentStates[0] != 0,
          "D2 the hull's other junction and the other hulls keep running");
    lM.mbEnsureTrafficLightDelay = false;
    lM.UpdateJunctions();
    Check(gRuntime5.mafJunctionStateChangeTimes[1] != lfHeldTime,
          "D3 without the flag the trigger junction runs again (cmplwi 1 ; bne 0x82724178)");
    BuildWorld(lM);
    lM.UpdateJunctions();
    const f32 lfJ0Time = gRuntime5.mafJunctionStateChangeTimes[0];
    const f32 lfJ1Time = gRuntime5.mafJunctionStateChangeTimes[1];
    lM.mbEnsureTrafficLightDelay = true;
    lM.mTrafficLightTriggerId = 0x7F000000u | (5u << 8) | 2u;   // top byte ignored: extrwi 16,8; trigger 2 -> junction 0
    lM.UpdateJunctions();
    Check(gRuntime5.mafJunctionStateChangeTimes[0] == lfJ0Time && gRuntime5.mafJunctionStateChangeTimes[1] != lfJ1Time,
          "D4 the hull is (id >> 8) & 0xFFFF and the junction lookup[id & 0xFF]: trigger 2 holds junction 0, not 1");

    // ---- E. a NaN change time ----------------------------------------------------------------------------------------
    BuildWorld(lM);
    lM.UpdateJunctions();
    gRuntime6.mafJunctionStateChangeTimes[0] = std::numeric_limits<f32>::quiet_NaN();
    lM.UpdateJunctions();
    Check(gRuntime6.mauJunctionCurrentStates[0] == 1 && std::isnan(gRuntime6.mafJunctionStateChangeTimes[0])
              && !gRuntime6.mabStoplineRedState[4],
          "E1 a NaN time is not > 0: it falls through bgt 0x827241B0 and the phase changes (to green here)");

    // ---- F. UpdateHull's range ---------------------------------------------------------------------------------------
    BuildWorld(lM);
    Light(lM, 14).muState = 1;
    Light(lM, 14).mfTimer = 1.0f;
    Light(lM, 9).muState = 1;
    Light(lM, 9).mfTimer = 1.0f;
    lM.UpdateJunctions();
    Check(Light(lM, 14).mfTimer == 1.0f && Light(lM, 9).mfTimer == 1.0f && Light(lM, 14).muState == 1,
          "F1 UpdateHull runs [muFirstTrafficLight, muLastTrafficLight) only (lhz +0xA / +0xC, bge 0x82751878)");

    // ---- G. the stop-line release walk ----------------------------------------------------------------------------------
    BuildWorld(lM);
    lM.UpdateJunctions();   // phase 0 everywhere: 5:2, 6:1, 5:3, 6:4 red
    TrafficEntityModule::ActiveHullSet lOldHulls;
    lOldHulls.Clear();
    lOldHulls.Insert(5);
    lM.mActiveHulls.Clear();
    lM.mActiveHulls.Insert(6);   // hull 5 left, hull 6 stayed
    gAsserts.clear();
    lM.ReleaseWalk(&lOldHulls);
    Check(!gRuntime6.mabStoplineRedState[1],
          "G1 a stop line the departed hull's light controls in a hull still active goes back to not red "
          "(SetStoplineRed(id, false) @0x8274D82C)");
    Check(gRuntime6.mabStoplineRedState[4],
          "G2 the active hull's own junction's stop line is untouched");
    Check(gRuntime5.mabStoplineRedState[2] && gRuntime5.mabStoplineRedState[3],
          "G3 stop lines in hulls not in mActiveHulls are not visited (Contains, Find -1)");
    Check(gAsserts.empty(),
          "G4 no GetHullRuntime on a hull without a runtime (7:0 skipped by Contains)");

    // ---- H. the accessors ----------------------------------------------------------------------------------------------
    Check(gaJunctions5[0].GetStateTiming(2) == 250
              && Bits(gaJunctions5[0].GetTimeInState(1)) == Bits(static_cast<f32>(30) * 0.1f),
          "H1 GetTimeInState(s) = the authored tenths * 0.1f (flt_82004014); GetStateTiming the tenths");
    Check(gaJunctions5[0].IsLightRed(1, 1) && gaJunctions5[0].IsLightRed(1, 0) && !gaJunctions5[0].IsLightRed(2, 0)
              && gaJunctions5[0].IsLightRed(2, 1) && !gaJunctions5[0].IsLightRed(0, 1),
          "H2 IsLightRed(s, l) = (mask[s] & (1 << l)) == (1 << l) (0x827242D4..0x827242F4)");
    gAsserts.clear();
    (void)gaJunctions5[0].GetTimeInState(3);
    Check(gAsserts.find("luState < muNumStates") != std::string::npos, "H3 the h:165 tripwire at muNumStates");
    gAsserts.clear();
    (void)gaJunctions5[1].IsLightRed(0, 1);
    Check(gAsserts.find("luLight < muNumLights") != std::string::npos, "H4 the h:194 tripwire at muNumLights");

    // ---- J. TrafficLightRuntimeState::Update (unchanged, the callee) ----------------------------------------------------
    BuildWorld(lM);
    TrafficLightRuntimeState& lrState = Light(lM, 30);
    lrState.muState = 1;
    lrState.mfTimer = 0.25f;
    lrState.muFlags = 0xFA;
    lrState.Update(0.1f);
    Check(lrState.muState == 1 && Bits(lrState.mfTimer) == Bits(0.25f - 0.1f), "J1 AMBER runs down by the delta");
    lrState.Update(0.1f);
    lrState.Update(0.1f);
    Check(lrState.muState == 0 && lrState.mfTimer == 0.0f && lrState.muFlags == 0xF9,
          "J2 at <= 0: RED, time 0, flags (f & 0xF8) | 1 (rlwimi 0x827516E8)");
    Light(lM, 31).muState = 2;
    Light(lM, 31).mfTimer = 0.5f;
    Light(lM, 31).Update(0.1f);
    Check(Light(lM, 31).muState == 2 && Light(lM, 31).mfTimer == 0.5f, "J3 RED and GREEN are left alone");

    std::printf("FxTrafficLightsCycle: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
