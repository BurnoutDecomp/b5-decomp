// crash parity FX-NETCRASH (2026-09-25), item 3b -- TrafficEntityModule::UpdateTimers @0x82715858, compiled from
// the production BrnTrafficEntityModule.cpp (with the production IsDecisionFrame and the production
// TimerStatusInterface::IsSimTimerFrequency50Hz @0x8230E990) into a stand-in module holding the members it reads.
// The decision-time accumulate is `fmadds f0, f0, f13, f12` at 0x827159E0: the sim block's multiplier (+0x20) *
// its base step (+0x1C) + mfSimTimeSinceLastDecision, with ONE rounding. The stored mfSimTimeStep is the
// separately rounded product (`fmuls` at 0x827158B0) and stays so.
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cmath>
#include <cstdio>
#include <cstring>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0, gGateLogs = 0;

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
}

#include "timer_status.inc"   // the production TimerStatusInterface::IsSimTimerFrequency50Hz

namespace BrnTraffic
{
namespace BrnTrafficIO
{
    struct InputBuffer_PreScene
    {
        CgsSystem::TimerStatusInterface mTimers;
        const CgsSystem::TimerStatusInterface* GetTimerStatusInterface() const { return &mTimers; }
    };
}

    // The named gate UpdateTimers keeps for its unnamed +0x72700 store: counted, nothing else.
    inline void LogMissingLeg_T1(bool& lrbAlreadyLogged, const char*)
    {
        if (!lrbAlreadyLogged)
        {
            lrbAlreadyLogged = true;
            ++gGateLogs;
        }
    }

    struct SimTimeStepVec { f32 x, y, z, w; };

    class TrafficEntityModule
    {
    public:
        enum EState { E_STATE_STARTING_UP, E_STATE_RUNNING };

        void UpdateTimers(const BrnTrafficIO::InputBuffer_PreScene* lpInput);
        bool IsDecisionFrame();

        EState         meState;
        u8             muFramesSinceDecision;        // :697
        bool           mbDecisionFrame;              // :698
        f32            mfSimTimeSinceLastDecision;   // :699
        f32            mfSimTimeStep;                // :700
        f32            mfSimTimeStepMultiplier;      // :701
        SimTimeStepVec mfSimTimeStepVec;             // :706
        bool           mbAllowDivergentBehaviour;    // :726
    };

#include "update_timers.inc"
}

using BrnTraffic::TrafficEntityModule;

static u32 Bits(f32 lfValue)
{
    u32 luBits;
    std::memcpy(&luBits, &lfValue, sizeof(luBits));
    return luBits;
}

static void Check(bool lbPass, const char* lpcWhat)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("FAIL: %s\n", lpcWhat);
    }
}

// A RUNNING module one frame after a decision frame's reset, with the sim timer at (base, multiplier). Offline:
// mbAllowDivergentBehaviour is true, so the online 0.1 s override never runs.
static void Start(TrafficEntityModule& lrModule, BrnTraffic::BrnTrafficIO::InputBuffer_PreScene& lrInput, f32 lfBase,
                  f32 lfMultiplier)
{
    std::memset(&lrModule, 0, sizeof(lrModule));
    lrModule.meState                    = TrafficEntityModule::E_STATE_RUNNING;
    lrModule.mbDecisionFrame            = true;    // the first call clears the accumulator (0x82715880..0x8271588C)
    lrModule.muFramesSinceDecision      = 0;
    lrModule.mfSimTimeSinceLastDecision = 123.0f;  // whatever was left: the decision frame zeroes it
    lrModule.mbAllowDivergentBehaviour  = true;
    std::memset(&lrInput, 0, sizeof(lrInput));
    lrInput.mTimers.mSimTimerStatus.mfBaseTimeStep       = lfBase;
    lrInput.mTimers.mSimTimerStatus.mfTimeStepMultiplier = lfMultiplier;
}

int main()
{
    const f32 lfBase60 = 1.0f / 60.0f;   // 0x3C888889
    TrafficEntityModule                     lModule;
    BrnTraffic::BrnTrafficIO::InputBuffer_PreScene lInput;

    // ---- a 0.75 multiplier at 60 Hz: the product 0.0125f is inexact ------------------------------------------
    Start(lModule, lInput, lfBase60, 0.75f);
    for (int liFrame = 0; liFrame < 4; ++liFrame)
    {
        lModule.UpdateTimers(&lInput);
    }
    Check(!lModule.mbDecisionFrame && lModule.muFramesSinceDecision == 4, "four frames at 60 Hz: no decision yet (6)");
    Check(Bits(lModule.mfSimTimeSinceLastDecision) == 0x3D4CCCCDu,
          "0.75 x 1/60, four steps: the fused accumulate gives 0x3D4CCCCD (fmadds 0x827159E0); the rounded "
          "step added afterwards gives 0x3D4CCCCE");
    Check(Bits(lModule.mfSimTimeStep) == Bits(0.75f * lfBase60),
          "mfSimTimeStep stays the separately rounded product (fmuls 0x827158B0)");

    // ---- a 0.3 multiplier: 1 ulp at the third step ----------------------------------------------------------
    Start(lModule, lInput, lfBase60, 0.3f);
    for (int liFrame = 0; liFrame < 3; ++liFrame)
    {
        lModule.UpdateTimers(&lInput);
    }
    Check(Bits(lModule.mfSimTimeSinceLastDecision) == 0x3C75C291u,
          "0.3 x 1/60, three steps: 0x3C75C291 fused (0x3C75C290 unfused)");

    // ---- each step is exactly one fused operation on the previous value --------------------------------------
    {
        Start(lModule, lInput, lfBase60, 1.1f);
        lModule.UpdateTimers(&lInput);
        bool lbEach = true;
        for (int liFrame = 1; liFrame < 5; ++liFrame)
        {
            const f32 lfBefore = lModule.mfSimTimeSinceLastDecision;
            lModule.UpdateTimers(&lInput);
            lbEach = lbEach && Bits(lModule.mfSimTimeSinceLastDecision) == Bits(std::fma(1.1f, lfBase60, lfBefore));
        }
        Check(lbEach, "1.1 x 1/60: every step == fmaf(multiplier, base, previous)");
        Check(Bits(lModule.mfSimTimeSinceLastDecision) == 0x3DBBBBBDu,
              "1.1 x 1/60, five steps: 0x3DBBBBBD fused (0x3DBBBBBC unfused)");
    }

    // ---- the unit multiplier: the product is exact, nothing changes ------------------------------------------
    Start(lModule, lInput, lfBase60, 1.0f);
    for (int liFrame = 0; liFrame < 5; ++liFrame)
    {
        lModule.UpdateTimers(&lInput);
    }
    {
        f32 lfSum = 0.0f;
        for (int liFrame = 0; liFrame < 5; ++liFrame)
        {
            lfSum += lfBase60;
        }
        Check(Bits(lModule.mfSimTimeSinceLastDecision) == Bits(lfSum),
              "multiplier 1.0: identical to plain additions of the base step");
    }

    // ---- the decision frame: the sixth frame at 60 Hz, the fifth at 50 Hz; the next call starts from 0 --------
    lModule.UpdateTimers(&lInput);
    Check(lModule.mbDecisionFrame && lModule.muFramesSinceDecision == 0, "60 Hz: the sixth frame is a decision frame");
    lModule.UpdateTimers(&lInput);
    Check(Bits(lModule.mfSimTimeSinceLastDecision) == Bits(lfBase60),
          "after a decision frame the accumulate restarts from 0.0f (one step)");

    Start(lModule, lInput, 0.02f, 1.0f);
    for (int liFrame = 0; liFrame < 5; ++liFrame)
    {
        lModule.UpdateTimers(&lInput);
    }
    Check(lModule.mbDecisionFrame, "50 Hz (IsSimTimerFrequency50Hz): the fifth frame is a decision frame");

    Check(gAsserts == 0, "no assert fired");
    std::printf("FxNetcrashUpdateTimers: %u checks, %u failures (%u asserts, %u gate logs)\n", gChecks, gFailures,
                gAsserts, gGateLogs);
    return gFailures ? 1 : 0;
}
