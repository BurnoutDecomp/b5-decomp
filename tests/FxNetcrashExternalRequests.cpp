// crash parity FX-NETCRASH (2026-09-25), online hull set piece 2: the traffic un-pause / restart arms of
// TrafficEntityModule::HandleExternalRequests @0x8274B660 (actions 34, 47, 143, 225, 226, 236) and Reset's
// mbActivateOnlineHullsAfterReset replay block (0x8272D470..0x8272D6D8).
//
// run_fxnetcrash_external_requests.py extracts the PRODUCTION bodies of HandleExternalRequests,
// RestartTraffic @0x82708F98, IsPaused @0x82707560, EnterTearingDownState @0x82708168, and the replay
// block out of Reset @0x8272CDA0. It compiles them as members of ExtFixture, which carries the module
// members they reach with the real members' types. Actions go through a REAL
// CgsModule::VariableEventQueue<13312,16>, the same storage InputBuffer_PostPhysics hands the
// module. The replay walks a REAL Pvs (BrnTrafficPvs.cpp is linked) whose per-cell sets are built here.
// The other callees (HandlePrepareForModeAction, HandleStopModeAction, ClearupCrashedTraffic,
// KillAllTrafficInCylinder) are recording doubles. None of them is reached by the actions under test.
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficTrackWitness.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Containers/CgsSet.h"
#include "SharedClasses/Traffic/BrnTrafficPvs.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/System/PC/BrnNetHarnessPC.h"
// crash parity FX-TRAFFICLIGHTS (2026-09-25), additive: the types the arms 30 / 110 / 192 name.
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameSource/GameState/BrnGameEvents.h"
#include "GameSource/GameState/TriggerQueryManager/BrnKillzoneAction.h"
#include <cstdio>
#include <cstring>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0, gGateLogs = 0, gArm47Gates = 0;
static unsigned    gExpectedAsserts = 0;       // the arm-34 tripwire scenario fires one on purpose
static const char* gpcLastAssert    = nullptr; // the message of the last assert that fired

namespace CgsDev
{
namespace Assert
{
    char* gpcMessageBuffer = nullptr;
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int)
    {
        ++gAsserts;
        gpcLastAssert = lpcMessage;
        std::printf("ASSERT: %s\n", lpcMessage);
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { unsigned long long gxMessageFilterFlags = 0; }
}

// The action-236 arm's [nettraf] witness (b5 ca4ac341). The real declaration is included above, so
// this silent definition must keep its signature.
namespace BrnNetHarnessPC
{
    void WitnessTag(const char*, const char*, const char*, ...) {}
}

// crash parity FX-TRAFFICLIGHTS (2026-09-25), additive: arm 30 (STOP_MODE_INTRO) calls these two interface accessors.
// Their production bodies (BrnRCEntityActiveRaceCarOutputInterface.cpp) are not linked here; these are the same two
// lines. This runner's scenarios never post action 30.
namespace BrnWorld
{
namespace RaceCarEntityModuleIO
{
    bool RCEntityActiveRaceCarOutputInterface::IsRaceCarActive(EActiveRaceCarIndex leIndex) const
    {
        return (maxRaceCarFlags[leIndex] & 1) != 0;
    }
    const RCEntityActiveRaceCarOutputInterface::RaceCarState*
    RCEntityActiveRaceCarOutputInterface::GetRaceCarState(EActiveRaceCarIndex leIndex) const
    {
        return &maRaceCarStates[leIndex];
    }
}
}

namespace BrnTraffic
{
    const char* gpcTrafficRemoveReason = nullptr;

namespace
{
    inline void LogMissingLeg_T6(bool& lrbAlreadyLogged, const char* lpcLegNameAndReason)
    {
        lrbAlreadyLogged = true;
        ++gGateLogs;
        if (std::strstr(lpcLegNameAndReason, "action 47 leg") != nullptr)
        {
            ++gArm47Gates;
        }
    }
    CgsDev::Log::DebugPrint* TrafficDiagStream() { return nullptr; }
    CgsDev::Log::DebugPrint* NetCrashDiagStream() { return nullptr; }
    const s32 KI_NETCRASH_HULL_DIAG_MAX_LINES = 48;
}

    struct FakeInput
    {
        typedef CgsModule::VariableEventQueue<13312, 16> GameActionQueueStorage;
        GameActionQueueStorage mQueue;
        const GameActionQueueStorage* GetGameActionQueue() const { return &mQueue; }
        // crash parity FX-TRAFFICLIGHTS (2026-09-25), additive: arm 30's interface (never reached here).
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* GetActiveRaceCarOutputInterface() const
        {
            return nullptr;
        }
    };
    struct FakeOutput
    {
        // crash parity FX-TRAFFICLIGHTS (2026-09-25), additive: arm 192's answer queue (never reached here).
        CgsModule::VariableEventQueue<1536, 16> mGameEvents;
        CgsModule::VariableEventQueue<1536, 16>* GetGameEventQueue() { return &mGameEvents; }
    };

    struct FakeData { Pvs* mpPvs; };
    struct FakeDataPtr
    {
        FakeData* mp;
        FakeData* operator->() { return mp; }
    };

    // Arm 47's first statement (0x8274BD98..0x8274BDA0): TrafficLightManager::SetCountdownValue(this +
    // 0x53790, record[0]). The stand-in records every value it is handed.
    struct FakeLightManager
    {
        unsigned muCalls;
        s32      miLastDisplay;
        void SetCountdownValue(s32 liCountdownDisplay) { ++muCalls; miLastDisplay = liCountdownDisplay; }
    };

    struct ExtFixture
    {
        typedef TrafficEntityModule M;
        typedef M::EState            EState;
        typedef M::ERunningState     ERunningState;
        typedef M::ETearingDownState ETearingDownState;
        static const EState            E_STATE_STARTING_UP       = M::E_STATE_STARTING_UP;
        static const EState            E_STATE_RUNNING           = M::E_STATE_RUNNING;
        static const EState            E_STATE_TEARING_DOWN      = M::E_STATE_TEARING_DOWN;
        static const ERunningState     E_RUNNINGSTATE_INVALID    = M::E_RUNNINGSTATE_INVALID;
        static const ERunningState     E_RUNNINGSTATE_NORMAL     = M::E_RUNNINGSTATE_NORMAL;
        static const ERunningState     E_RUNNINGSTATE_PAUSED     = M::E_RUNNINGSTATE_PAUSED;
        static const ETearingDownState E_TEARINGDOWNSTATE_WIPING = M::E_TEARINGDOWNSTATE_WIPING;

        decltype(M::meState)                                  meState;
        decltype(M::meRunningState)                           meRunningState;
        decltype(M::meRunningStateToUseAfterStartup)          meRunningStateToUseAfterStartup;
        decltype(M::meTearingDownState)                       meTearingDownState;
        decltype(M::meGameMode)                               meGameMode;
        decltype(M::mbIsOnlineGameMode)                       mbIsOnlineGameMode;
        decltype(M::mbAllowDivergentBehaviour)                mbAllowDivergentBehaviour;
        decltype(M::mfBaseDensityScale)                       mfBaseDensityScale;
        decltype(M::mfGameModeDensityScale)                   mfGameModeDensityScale;
        decltype(M::mLocalPlayerPosition)                     mLocalPlayerPosition;
        decltype(M::mbDontCreateVehiclesNearAnyPlayers)       mbDontCreateVehiclesNearAnyPlayers;
        decltype(M::mbDontCreateStaticVehiclesNearAnyPlayers) mbDontCreateStaticVehiclesNearAnyPlayers;
        decltype(M::mbActivateOnlineHullsAfterReset)          mbActivateOnlineHullsAfterReset;
        decltype(M::mau16HullsToActivateAfterReset)           mau16HullsToActivateAfterReset;
        decltype(M::muCurrentlyPredictedHull)                 muCurrentlyPredictedHull;
        decltype(M::meLocalPlayerIndex)                       meLocalPlayerIndex;
        decltype(M::maaRaceCarHulls)                          maaRaceCarHulls;
        FakeDataPtr                                           mpData;
        FakeLightManager                                      mTrafficLightManager;
        // Arm 34's tripwire reads (0x8274BE04 lbzx +0x717E4, 0x8274BE10 lbzx +0x7287E).
        decltype(M::mbNeedToSetUpLightsForEventStart)         mbNeedToSetUpLightsForEventStart;
        decltype(M::mbDEBUGTurnTrafficOff)                    mbDEBUGTurnTrafficOff;
        // crash parity FX-TRAFFICLIGHTS (2026-09-25), additive: what the arms 13 / 30 / 73 / 75 / 77 / 110 / 192 / 244
        // and the post-loop Picture Paradise tail read. This runner's scenarios post none of those actions; their
        // test is run_fxtrafficlights_requests.py. The tail runs after every queue here and only reads
        // mCameraLastFrame's flags and mbInPictureParadise, both zeroed by Fresh, so it does nothing.
        typedef M::EEmptyTrafficPoolState EEmptyTrafficPoolState;
        static const EEmptyTrafficPoolState E_EMPTYTRAFFICPOOLSTATE_IDLE     = M::E_EMPTYTRAFFICPOOLSTATE_IDLE;
        static const EEmptyTrafficPoolState E_EMPTYTRAFFICPOOLSTATE_EMPTYING = M::E_EMPTYTRAFFICPOOLSTATE_EMPTYING;
        static const EEmptyTrafficPoolState E_EMPTYTRAFFICPOOLSTATE_EMPTY    = M::E_EMPTYTRAFFICPOOLSTATE_EMPTY;
        static const EEmptyTrafficPoolState E_EMPTYTRAFFICPOOLSTATE_FILLING  = M::E_EMPTYTRAFFICPOOLSTATE_FILLING;
        decltype(M::meEmptyTrafficPoolState)                   meEmptyTrafficPoolState;
        decltype(M::mbGameModeClearsTraffic)                   mbGameModeClearsTraffic;
        decltype(M::mbAtStartLineSoProtectRaceCarsFromTraffic) mbAtStartLineSoProtectRaceCarsFromTraffic;
        decltype(M::mbGameModeAllowsKillzones)                 mbGameModeAllowsKillzones;
        decltype(M::mbDEBUGEnableKillzones)                    mbDEBUGEnableKillzones;
        decltype(M::mbWaitingForStreaming)                     mbWaitingForStreaming;
        decltype(M::mbTrafficIsHidden)                         mbTrafficIsHidden;
        decltype(M::mbInPictureParadise)                       mbInPictureParadise;
        decltype(M::mfTrafficSimRadius)                        mfTrafficSimRadius;
        decltype(M::muMaxVehiclesToRender)                     muMaxVehiclesToRender;
        decltype(M::mfRenderCullDistanceSq)                    mfRenderCullDistanceSq;
        decltype(M::mbInOfflineCarSelect)                      mbInOfflineCarSelect;
        decltype(M::mCameraLastFrame)                          mCameraLastFrame;
        unsigned muHideCalls = 0, muUnhideCalls = 0, muKillZoneFires = 0;
        void HideAllTraffic() { ++muHideCalls; }
        void UnhideAllTraffic() { ++muUnhideCalls; }
        void FireKillZone(u64) { ++muKillZoneFires; }

        unsigned muPrepareCalls = 0, muStopCalls = 0, muClearupCalls = 0, muCylinderCalls = 0, muTearDowns = 0;

        void HandlePrepareForModeAction(const FakeInput*, const BrnGameState::GameStateModuleIO::PrepareForModeAction*)
        { ++muPrepareCalls; }
        void HandleStopModeAction(const FakeInput*, const BrnGameState::GameStateModuleIO::StopModeAction*)
        { ++muStopCalls; }
        void ClearupCrashedTraffic() { ++muClearupCalls; }
        void KillAllTrafficInCylinder(Vector3, f32, f32, bool) { ++muCylinderCalls; }

        bool IsPaused();
        void RestartTraffic();
        void EnterTearingDownState();
        void HandleExternalRequests(const FakeInput* lpInput, FakeOutput* lpOutput);
        void ReplayOnlineHullSet();
    };

#include "external_requests_bodies.inc"
}

using namespace BrnTraffic;
using namespace BrnGameState::GameStateModuleIO;

static void Check(bool lbPass, const char* lpcWhat)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("FAIL: %s\n", lpcWhat);
    }
}

static void Fresh(ExtFixture& lrModule, FakeInput& lrInput, bool lbOnline, ExtFixture::EState leState,
                  ExtFixture::ERunningState leRunning, ExtFixture::ERunningState leToUse)
{
    std::memset(&lrModule, 0, sizeof(lrModule));
    lrModule.mbIsOnlineGameMode              = lbOnline;
    lrModule.mbAllowDivergentBehaviour       = !lbOnline;
    lrModule.meState                         = leState;
    lrModule.meRunningState                  = leRunning;
    lrModule.meRunningStateToUseAfterStartup = leToUse;
    lrModule.muCurrentlyPredictedHull        = 42;
    for (u32 luSlot = 0; luSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++luSlot)
    {
        lrModule.mau16HullsToActivateAfterReset[luSlot] = KU_INVALID_HULL;
        lrModule.maaRaceCarHulls[luSlot].Construct();
    }
    lrInput.mQueue.Construct();
    lrInput.mQueue.Clear();
}

template <typename T>
static void Post(FakeInput& lrInput, const T& lrRecord, s32 liType)
{
    lrInput.mQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lrRecord), liType, static_cast<s32>(sizeof(T)));
}

struct EmptyAction { u8 mu8Unused; };

static Set<u16, 8> gaCellSets[16];
static Pvs         gPvs;
static FakeData    gData;

int main()
{
    FakeOutput lOutput;

    // ---- 236 RESTART_TRAFFIC, online, RUNNING -------------------------------------------------
    {
        ExtFixture lM; FakeInput lIn;
        Fresh(lM, lIn, true, ExtFixture::E_STATE_RUNNING, ExtFixture::E_RUNNINGSTATE_PAUSED, ExtFixture::E_RUNNINGSTATE_PAUSED);
        RestartTrafficAction lRestart;
        const u16 kau16Hulls[8] = { 10, 11, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };
        std::memcpy(lRestart.mau16ActveHulls, kau16Hulls, sizeof(kau16Hulls));
        Post(lIn, lRestart, E_ACTION_RESTART_TRAFFIC);
        lM.HandleExternalRequests(&lIn, &lOutput);

        Check(lM.meState == ExtFixture::E_STATE_TEARING_DOWN && lM.meTearingDownState == ExtFixture::E_TEARINGDOWNSTATE_WIPING,
              "236 online RUNNING: RestartTraffic tears the traffic down (EnterTearingDownState)");
        Check(lM.meRunningStateToUseAfterStartup == ExtFixture::E_RUNNINGSTATE_NORMAL,
              "236: RestartTraffic comes back NORMAL (stw 0, 0x30C) -- the un-pause");
        Check(lM.mbDontCreateVehiclesNearAnyPlayers && lM.mbDontCreateStaticVehiclesNearAnyPlayers,
              "236: RestartTraffic keeps the respawn off every player (0x725E9 / 0x725EA)");
        Check(lM.mbActivateOnlineHullsAfterReset, "236: mbActivateOnlineHullsAfterReset raised (stb 1, +0x558DF)");
        Check(std::memcmp(lM.mau16HullsToActivateAfterReset, kau16Hulls, sizeof(kau16Hulls)) == 0,
              "236: the eight hulls copied to mau16HullsToActivateAfterReset (+0x558CC)");

        // A second restart while the first is parked replaces the set (the warning print is off).
        RestartTrafficAction lAgain;
        const u16 kau16Again[8] = { 20, 21, 22, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };
        std::memcpy(lAgain.mau16ActveHulls, kau16Again, sizeof(kau16Again));
        lIn.mQueue.Clear();
        Post(lIn, lAgain, E_ACTION_RESTART_TRAFFIC);
        lM.HandleExternalRequests(&lIn, &lOutput);
        Check(lM.mbActivateOnlineHullsAfterReset
              && std::memcmp(lM.mau16HullsToActivateAfterReset, kau16Again, sizeof(kau16Again)) == 0,
              "236 again: the newer hull set replaces the parked one");
    }

    // ---- 236 offline: nothing ------------------------------------------------------------------
    {
        ExtFixture lM; FakeInput lIn;
        Fresh(lM, lIn, false, ExtFixture::E_STATE_RUNNING, ExtFixture::E_RUNNINGSTATE_NORMAL, ExtFixture::E_RUNNINGSTATE_NORMAL);
        RestartTrafficAction lRestart;
        std::memset(lRestart.mau16ActveHulls, 0, sizeof(lRestart.mau16ActveHulls));
        Post(lIn, lRestart, E_ACTION_RESTART_TRAFFIC);
        lM.HandleExternalRequests(&lIn, &lOutput);
        Check(lM.meState == ExtFixture::E_STATE_RUNNING && !lM.mbActivateOnlineHullsAfterReset
              && lM.mau16HullsToActivateAfterReset[0] == KU_INVALID_HULL,
              "236 offline: no restart, no hull set (lbzx +0x717DC gate)");
    }

    // ---- 34 START_PLAYING_MODE: the GO without a countdown record (0x8274BDF0..0x8274BE34) -----
    {
        // The lights were set up (UpdateEventStarts consumed the flag): SetCountdownValue(0), no tripwire.
        ExtFixture lM; FakeInput lIn;
        Fresh(lM, lIn, false, ExtFixture::E_STATE_RUNNING, ExtFixture::E_RUNNINGSTATE_NORMAL, ExtFixture::E_RUNNINGSTATE_NORMAL);
        lM.mTrafficLightManager.miLastDisplay = 7;
        StartPlayingModeAction lStart; std::memset(&lStart, 0, sizeof(lStart));
        Post(lIn, lStart, E_ACTION_START_PLAYING_MODE);
        const unsigned luAsserts = gAsserts;
        lM.HandleExternalRequests(&lIn, &lOutput);
        Check(lM.mTrafficLightManager.muCalls == 1 && lM.mTrafficLightManager.miLastDisplay == 0,
              "34: TrafficLightManager::SetCountdownValue(0) once (li r4,0 ; bl 0x82751750)");
        Check(gAsserts == luAsserts, "34, the lights set up (flag clear): no tripwire (lbzx +0x717E4 ; beq)");
        Check(lM.meState == ExtFixture::E_STATE_RUNNING && lM.meRunningState == ExtFixture::E_RUNNINGSTATE_NORMAL,
              "34: nothing else changes (the arm ends at b 0x8274C0B0)");
    }
    {
        // The flag still up when the mode starts playing, the traffic on: the .cpp 5995 tripwire.
        ExtFixture lM; FakeInput lIn;
        Fresh(lM, lIn, true, ExtFixture::E_STATE_RUNNING, ExtFixture::E_RUNNINGSTATE_NORMAL, ExtFixture::E_RUNNINGSTATE_NORMAL);
        lM.mbNeedToSetUpLightsForEventStart = true;
        StartPlayingModeAction lStart; std::memset(&lStart, 0, sizeof(lStart));
        Post(lIn, lStart, E_ACTION_START_PLAYING_MODE);
        const unsigned luAsserts = gAsserts;
        gpcLastAssert = nullptr;
        lM.HandleExternalRequests(&lIn, &lOutput);
        gExpectedAsserts += gAsserts - luAsserts;
        Check(gAsserts == luAsserts + 1 && gpcLastAssert != nullptr
              && std::strcmp(gpcLastAssert, "!mbNeedToSetUpLightsForEventStart || mbDEBUGTurnTrafficOff") == 0,
              "34, the lights never set up: the .cpp 5995 tripwire fires once (0x8274BE1C..0x8274BE30)");
        Check(lM.mTrafficLightManager.muCalls == 1 && lM.mTrafficLightManager.miLastDisplay == 0,
              "34: SetCountdownValue(0) comes first, whatever the flag");
        Check(lM.mbNeedToSetUpLightsForEventStart, "34: the arm only reads the flag (UpdateEventStarts consumes it)");

        // The debug switch that turns the traffic off silences it (lbzx +0x7287E ; bne).
        ExtFixture lOff; FakeInput lInOff;
        Fresh(lOff, lInOff, true, ExtFixture::E_STATE_RUNNING, ExtFixture::E_RUNNINGSTATE_NORMAL, ExtFixture::E_RUNNINGSTATE_NORMAL);
        lOff.mbNeedToSetUpLightsForEventStart = true;
        lOff.mbDEBUGTurnTrafficOff = true;
        Post(lInOff, lStart, E_ACTION_START_PLAYING_MODE);
        const unsigned luAssertsOff = gAsserts;
        lOff.HandleExternalRequests(&lInOff, &lOutput);
        Check(gAsserts == luAssertsOff && lOff.mTrafficLightManager.muCalls == 1
              && lOff.mTrafficLightManager.miLastDisplay == 0,
              "34 with mbDEBUGTurnTrafficOff: no tripwire, SetCountdownValue(0) still once");
    }

    // ---- 47 SET_COUNTDOWN ----------------------------------------------------------------------
    {
        ExtFixture lM; FakeInput lIn;
        Fresh(lM, lIn, true, ExtFixture::E_STATE_STARTING_UP, ExtFixture::E_RUNNINGSTATE_INVALID, ExtFixture::E_RUNNINGSTATE_PAUSED);
        SetCountdownAction lCountdown; lCountdown.miCountdownDisplay = 3;
        Post(lIn, lCountdown, E_ACTION_SET_COUNTDOWN);
        lM.mTrafficLightManager.muCalls = 0;
        lM.HandleExternalRequests(&lIn, &lOutput);
        Check(lM.meRunningStateToUseAfterStartup == ExtFixture::E_RUNNINGSTATE_NORMAL,
              "47 online STARTING_UP: the start-up will come back NORMAL, not PAUSED (stw 0, 0x30C)");
        Check(lM.mTrafficLightManager.muCalls == 1 && lM.mTrafficLightManager.miLastDisplay == 3,
              "47 online: TrafficLightManager::SetCountdownValue(record[0] = 3) once (0x8274BD98..0x8274BDA0)");
    }
    {
        ExtFixture lM; FakeInput lIn;
        Fresh(lM, lIn, true, ExtFixture::E_STATE_RUNNING, ExtFixture::E_RUNNINGSTATE_PAUSED, ExtFixture::E_RUNNINGSTATE_NORMAL);
        SetCountdownAction lCountdown; lCountdown.miCountdownDisplay = 0;
        Post(lIn, lCountdown, E_ACTION_SET_COUNTDOWN);
        lM.mTrafficLightManager.muCalls = 0;
        lM.HandleExternalRequests(&lIn, &lOutput);
        Check(lM.meRunningState == ExtFixture::E_RUNNINGSTATE_NORMAL,
              "47 online RUNNING+PAUSED: the traffic un-pauses (stw 0, 0x308)");
        Check(lM.mTrafficLightManager.muCalls == 1 && lM.mTrafficLightManager.miLastDisplay == 0,
              "47 online, the GO: SetCountdownValue(0) once, whatever the pause state");
    }
    {
        ExtFixture lM; FakeInput lIn;
        Fresh(lM, lIn, false, ExtFixture::E_STATE_RUNNING, ExtFixture::E_RUNNINGSTATE_PAUSED, ExtFixture::E_RUNNINGSTATE_PAUSED);
        SetCountdownAction lCountdown; lCountdown.miCountdownDisplay = 1;
        Post(lIn, lCountdown, E_ACTION_SET_COUNTDOWN);
        const unsigned luArm47Gates = gArm47Gates;
        lM.mTrafficLightManager.muCalls = 0;
        lM.HandleExternalRequests(&lIn, &lOutput);
        Check(lM.meRunningState == ExtFixture::E_RUNNINGSTATE_PAUSED
              && lM.meRunningStateToUseAfterStartup == ExtFixture::E_RUNNINGSTATE_PAUSED,
              "47 offline: the pause bookkeeping is untouched");
        Check(lM.mTrafficLightManager.muCalls == 1 && lM.mTrafficLightManager.miLastDisplay == 1
              && gArm47Gates == luArm47Gates,
              "47 OFFLINE too: SetCountdownValue(record[0] = 1) once and no named gate -- the console call is "
              "unconditional (REVIEW-I, b43b5c2b)");
    }

    // ---- 143 SHOWTIME_MODE_SWITCH ---------------------------------------------------------------
    {
        ExtFixture lM; FakeInput lIn;
        Fresh(lM, lIn, false, ExtFixture::E_STATE_RUNNING, ExtFixture::E_RUNNINGSTATE_NORMAL, ExtFixture::E_RUNNINGSTATE_NORMAL);
        ShowtimeModeSwitchAction lLeave; std::memset(&lLeave, 0, sizeof(lLeave)); lLeave.mbEnteringShowtime = false;
        Post(lIn, lLeave, E_ACTION_SHOWTIME_MODE_SWITCH);
        lM.HandleExternalRequests(&lIn, &lOutput);
        Check(lM.muCurrentlyPredictedHull == KU_INVALID_HULL, "143 leaving showtime: muCurrentlyPredictedHull = 0xFFFF");

        lM.muCurrentlyPredictedHull = 42;
        ShowtimeModeSwitchAction lEnter; std::memset(&lEnter, 0, sizeof(lEnter)); lEnter.mbEnteringShowtime = true;
        lIn.mQueue.Clear();
        Post(lIn, lEnter, E_ACTION_SHOWTIME_MODE_SWITCH);
        lM.HandleExternalRequests(&lIn, &lOutput);
        Check(lM.muCurrentlyPredictedHull == 42, "143 entering showtime: the predicted hull is kept");
    }

    // ---- 225 / 226 local player gone --------------------------------------------------------------
    {
        const s32 kaiTypes[2] = { E_ACTION_LOCAL_PLAYER_DISCONNECTED, E_ACTION_LOCAL_PLAYER_LEFT_GAME };
        for (s32 liType : kaiTypes)
        {
            ExtFixture lM; FakeInput lIn;
            Fresh(lM, lIn, true, ExtFixture::E_STATE_RUNNING, ExtFixture::E_RUNNINGSTATE_PAUSED, ExtFixture::E_RUNNINGSTATE_PAUSED);
            EmptyAction lEmpty = { 0 };
            Post(lIn, lEmpty, liType);
            lM.HandleExternalRequests(&lIn, &lOutput);
            Check(lM.meState == ExtFixture::E_STATE_TEARING_DOWN
                  && lM.meRunningStateToUseAfterStartup == ExtFixture::E_RUNNINGSTATE_NORMAL
                  && !lM.mbActivateOnlineHullsAfterReset,
                  "225/226 online: RestartTraffic (no hull set)");

            ExtFixture lOff; FakeInput lInOff;
            Fresh(lOff, lInOff, false, ExtFixture::E_STATE_RUNNING, ExtFixture::E_RUNNINGSTATE_NORMAL, ExtFixture::E_RUNNINGSTATE_NORMAL);
            Post(lInOff, lEmpty, liType);
            lOff.HandleExternalRequests(&lInOff, &lOutput);
            Check(lOff.meState == ExtFixture::E_STATE_RUNNING && !lOff.mbDontCreateVehiclesNearAnyPlayers,
                  "225/226 offline: nothing");
        }
    }

    // ---- Reset's replay block ----------------------------------------------------------------------
    {
        // A 4 x 4 grid; cell c's PVS is { 100 + c, 200 + c }.
        for (u32 luCell = 0; luCell < 16; ++luCell)
        {
            gaCellSets[luCell].Construct();
            gaCellSets[luCell].Insert(static_cast<u16>(100 + luCell));
            gaCellSets[luCell].Insert(static_cast<u16>(200 + luCell));
        }
        gPvs.mGridMin       = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
        gPvs.mCellSize      = Vector3{ 1.0f, 1.0f, 1.0f, 0.0f };
        gPvs.mRecipCellSize = Vector3{ 1.0f, 1.0f, 1.0f, 0.0f };
        gPvs.muNumCells_X   = 4;
        gPvs.muNumCells_Z   = 4;
        gPvs.muNumCells     = 16;
        gPvs.mpaHullPvs     = gaCellSets;
        gData.mpPvs         = &gPvs;

        ExtFixture lM; FakeInput lIn;
        Fresh(lM, lIn, true, ExtFixture::E_STATE_STARTING_UP, ExtFixture::E_RUNNINGSTATE_INVALID, ExtFixture::E_RUNNINGSTATE_NORMAL);
        lM.mpData.mp = &gData;
        lM.meLocalPlayerIndex = E_ACTIVE_RACE_CAR_INDEX_2;
        lM.mbActivateOnlineHullsAfterReset = true;
        lM.mau16HullsToActivateAfterReset[0] = 5;
        lM.mau16HullsToActivateAfterReset[2] = 7;
        lM.maaRaceCarHulls[1].Append(99);   // stale: the replay clears every slot
        lM.ReplayOnlineHullSet();

        Check(lM.maaRaceCarHulls[0].GetLength() == 3 && lM.maaRaceCarHulls[0][0] == 5
              && lM.maaRaceCarHulls[0][1] == 105 && lM.maaRaceCarHulls[0][2] == 205,
              "replay: slot 0 = its parked hull, then that hull's PVS (Append, GetHullPvs walk)");
        Check(lM.maaRaceCarHulls[1].GetLength() == 0, "replay: an invalid slot is cleared and stays empty");
        Check(lM.maaRaceCarHulls[2].GetLength() == 3 && lM.maaRaceCarHulls[2][0] == 7,
              "replay: slot 2 = its parked hull and its PVS");
        Check(lM.muCurrentlyPredictedHull == 7, "replay: the local player's hull becomes muCurrentlyPredictedHull");
        Check(!lM.mbActivateOnlineHullsAfterReset, "replay: the flag is consumed (stb 0, +0x558DF)");
        bool lbAllInvalid = true;
        for (u32 luSlot = 0; luSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++luSlot)
        {
            lbAllInvalid = lbAllInvalid && lM.mau16HullsToActivateAfterReset[luSlot] == KU_INVALID_HULL;
        }
        Check(lbAllInvalid, "replay: the parked hulls are reset to 0xFFFF (8 x sth -1)");

        ExtFixture lIdle; FakeInput lIn2;
        Fresh(lIdle, lIn2, false, ExtFixture::E_STATE_STARTING_UP, ExtFixture::E_RUNNINGSTATE_INVALID, ExtFixture::E_RUNNINGSTATE_NORMAL);
        lIdle.mpData.mp = &gData;
        lIdle.mau16HullsToActivateAfterReset[0] = 5;
        lIdle.maaRaceCarHulls[1].Append(99);
        lIdle.ReplayOnlineHullSet();
        Check(lIdle.maaRaceCarHulls[1].GetLength() == 1 && lIdle.mau16HullsToActivateAfterReset[0] == 5,
              "replay: nothing happens without the flag (the offline boot)");
    }

    Check(gAsserts == gExpectedAsserts, "no assert fired but the arm-34 tripwire scenario's own");
    std::printf("FxNetcrashExternalRequests: %u checks, %u failures (%u asserts)\n", gChecks, gFailures, gAsserts);
    return gFailures ? 1 : 0;
}
