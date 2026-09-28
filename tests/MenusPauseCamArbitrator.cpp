// OWNERLIST 2026-09-27, lane L5 MENUS: the PRODUCTION crash-nav arms of BrnDirector::Arbitrator::Update @0x8226ADA0 --
// the NORMAL-state trigger that hands the frame camera to ArbStateCrashNav on a pause, and the CRASH_NAV /
// CRASH_NAV_ICE_CAMERAS states' bodies -- extracted from src/GameSource/Director/Arbitrator/BrnDirectorArbitrator.cpp
// by run_menus_pausecam_arbitrator.py and compiled against recording stand-ins that carry the production names.
//
// Checked against the ARTIST asm:
//   trigger 0x8226B0A0..0x8226B100: lbz GameState+0x101 (mbCrashNavShown) || lbz +0x1B0 (mbDoing100PercentSequence),
//           && !lbz +0xD9 (mbGameIntroFlybyActive) -> mArbStateCrashNav vtable +4 (Prepare), vtable +8 (Update),
//           li 4 / stw arb +0x44F8 (meState = E_STATE_CRASH_NAV_ICE_CAMERAS)
//   case 3 0x8226B3C0..0x8226B3EC: frame camera = GetCurrentState()+0x10; !GameState+0x101 -> meState = 2
//   case 4 0x8226B358..0x8226B3BC: assert "mArbStateCrashNav.IsActive()" (lwz arb +0x4188, :295); vtable +8 (Update);
//           frame camera = arb +0x3920 (the state's camera); lwz +0x4188 == 0 -> meState = 2 (E_STATE_NORMAL)
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>

static unsigned gChecks = 0, gFailures = 0;
static int giAsserts = 0;
static std::string gsCalls;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcText, const char*, int) { ++giAsserts; std::fprintf(stderr, "assert: %s\n", lpcText); return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Log
{
    // The diag lines stream into gpDebugPrint; null here, as in a run without a log.
    struct StreamStandIn
    {
        template <class T> StreamStandIn& operator<<(const T&) { return *this; }
    };
    StreamStandIn* gpDebugPrint = nullptr;
}
}

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnDirector
{
namespace Camera
{
    struct Camera
    {
        int miId;
    };
}

struct GameStateStandIn
{
    bool mbCrashNavShown;             // +0x101
    bool mbDoing100PercentSequence;   // +0x1B0
    bool mbGameIntroFlybyActive;      // +0x0D9
};

struct SharedInfoStandIn
{
    const GameStateStandIn* mpGameState;
};

// Recording stand-in for ArbStateCrashNav: Prepare activates it, Update may release it (as Update's LABEL_50 does).
struct CrashNavStandIn
{
    bool          mbActive           = false;
    bool          mbReleaseOnUpdate  = false;
    Camera::Camera mCamera           = { 44 };
    bool Prepare(SharedInfoStandIn&) { gsCalls += "P"; mbActive = true; return true; }
    void Update(SharedInfoStandIn&)  { gsCalls += "U"; if (mbReleaseOnUpdate) mbActive = false; }
    bool IsActive() const { return mbActive; }
    const Camera::Camera& GetCamera() const { return mCamera; }
};

struct StateStandIn
{
    Camera::Camera mCamera = { 33 };
    const Camera::Camera& GetCamera() const { return mCamera; }
};
struct StateContainerStandIn
{
    StateStandIn mState;
    StateStandIn* GetCurrentState() { return &mState; }
};

struct ArbitratorStandIn
{
    enum EState
    {
        E_STATE_PREPARE = 0, E_STATE_PRE_NORMAL = 1, E_STATE_NORMAL = 2, E_STATE_CRASH_NAV = 3,
        E_STATE_CRASH_NAV_ICE_CAMERAS = 4, E_STATE_CHANGING_TO_ATTRACT_MODE = 5,
    };
    EState                meState = E_STATE_NORMAL;
    CrashNavStandIn       mArbStateCrashNav;
    StateContainerStandIn mStateContainer;

    void Trigger(bool lbPaused, Camera::Camera& lrCameraInOut, SharedInfoStandIn& lrSharedInfo);
    void Step(bool lbPaused, Camera::Camera& lrCameraInOut, SharedInfoStandIn& lrSharedInfo);
};
}

// Trigger(...) { <the production NORMAL-state trigger> }  Step(...) { switch (meState) { <case 3> <case 4> } }
#include "pausecam_arbitrator.inc"

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

int main()
{
    Camera::Camera lCamera = { 1 };

    // 1. The pause: mbCrashNavShown up, no 100% sequence, no intro fly-by.
    {
        GameStateStandIn  lState = { true, false, false };
        SharedInfoStandIn lInfo  = { &lState };
        ArbitratorStandIn lArb;
        gsCalls.clear();
        lArb.Trigger(true, lCamera, lInfo);
        Check(gsCalls == "PU", "pause: Prepare (vtable +4 @0x8226B0E0) then Update (vtable +8 @0x8226B0F8), once each");
        Check(lArb.meState == ArbitratorStandIn::E_STATE_CRASH_NAV_ICE_CAMERAS, "pause: meState = 4 (@0x8226B100)");
        Check(lCamera.miId == 1, "pause: the trigger frame keeps the NORMAL frame camera (the state's camera is taken from the next frame)");
    }
    // 2. The 100% sequence alone also enters.
    {
        GameStateStandIn  lState = { false, true, false };
        SharedInfoStandIn lInfo  = { &lState };
        ArbitratorStandIn lArb;
        gsCalls.clear();
        lArb.Trigger(false, lCamera, lInfo);
        Check(gsCalls == "PU" && lArb.meState == ArbitratorStandIn::E_STATE_CRASH_NAV_ICE_CAMERAS,
              "100% sequence alone: enters (lbz 0x1B0)");
    }
    // 3. The intro fly-by blocks it.
    {
        GameStateStandIn  lState = { true, true, true };
        SharedInfoStandIn lInfo  = { &lState };
        ArbitratorStandIn lArb;
        gsCalls.clear();
        lArb.Trigger(true, lCamera, lInfo);
        Check(gsCalls.empty() && lArb.meState == ArbitratorStandIn::E_STATE_NORMAL, "intro fly-by active: no entry (lbz 0xD9)");
    }
    // 4. Nothing asks: no entry.
    {
        GameStateStandIn  lState = { false, false, false };
        SharedInfoStandIn lInfo  = { &lState };
        ArbitratorStandIn lArb;
        gsCalls.clear();
        lArb.Trigger(false, lCamera, lInfo);
        Check(gsCalls.empty() && lArb.meState == ArbitratorStandIn::E_STATE_NORMAL, "no request: no entry");
    }
    // 5. CRASH_NAV_ICE_CAMERAS while the state is active: tick it and take its camera; stay.
    {
        GameStateStandIn  lState = { true, false, false };
        SharedInfoStandIn lInfo  = { &lState };
        ArbitratorStandIn lArb;
        lArb.meState = ArbitratorStandIn::E_STATE_CRASH_NAV_ICE_CAMERAS;
        lArb.mArbStateCrashNav.mbActive = true;
        gsCalls.clear();
        giAsserts = 0;
        lCamera.miId = 1;
        lArb.Step(true, lCamera, lInfo);
        Check(gsCalls == "U", "case 4: the state is ticked once (vtable +8 @0x8226B398)");
        Check(lCamera.miId == 44, "case 4: the frame camera is the state's (arb +0x3920)");
        Check(lArb.meState == ArbitratorStandIn::E_STATE_CRASH_NAV_ICE_CAMERAS && giAsserts == 0,
              "case 4: stays while the state is active, no assert");
        // ... and the frame it releases itself (Update's LABEL_50 -> Release zeroes meState): back to NORMAL.
        lArb.mArbStateCrashNav.mbReleaseOnUpdate = true;
        lArb.Step(false, lCamera, lInfo);
        Check(lArb.meState == ArbitratorStandIn::E_STATE_NORMAL, "case 4: IsActive() false after the tick -> meState = 2 (@0x8226B3B8)");
        Check(lCamera.miId == 44, "case 4: the releasing frame still takes the state's camera");
    }
    // 6. CRASH_NAV_ICE_CAMERAS entered with an inactive state: the console's assert fires (:295) and it drops out.
    {
        GameStateStandIn  lState = { false, false, false };
        SharedInfoStandIn lInfo  = { &lState };
        ArbitratorStandIn lArb;
        lArb.meState = ArbitratorStandIn::E_STATE_CRASH_NAV_ICE_CAMERAS;
        giAsserts = 0;
        lArb.Step(false, lCamera, lInfo);
        Check(giAsserts == 1, "case 4: an inactive state fires \"mArbStateCrashNav.IsActive()\" (@0x8226B378)");
        Check(lArb.meState == ArbitratorStandIn::E_STATE_NORMAL, "case 4: and falls back to NORMAL");
    }
    // 7. CRASH_NAV (3): the current state's camera; back to NORMAL once the crash nav is hidden.
    {
        GameStateStandIn  lState = { true, false, false };
        SharedInfoStandIn lInfo  = { &lState };
        ArbitratorStandIn lArb;
        lArb.meState = ArbitratorStandIn::E_STATE_CRASH_NAV;
        lCamera.miId = 1;
        lArb.Step(true, lCamera, lInfo);
        Check(lCamera.miId == 33 && lArb.meState == ArbitratorStandIn::E_STATE_CRASH_NAV,
              "case 3: current state's camera; stays while shown");
        lState.mbCrashNavShown = false;
        lArb.Step(true, lCamera, lInfo);
        Check(lArb.meState == ArbitratorStandIn::E_STATE_NORMAL, "case 3: !mbCrashNavShown -> meState = 2 (@0x8226B3EC)");
    }

    std::printf("MenusPauseCamArbitrator: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
