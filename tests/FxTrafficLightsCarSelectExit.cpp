// crash parity FX-TRAFFICLIGHTS (2026-09-25): the junkyard exit's action 77 carries the exit spawn position.
// run_fxtrafficlights_carselect_exit.py extracts the PRODUCTION CarSelectManager::UpdateExitState @0x82398C20 and the
// file-local helpers it reads (exit_state_locals.inc), and compiles the body as a member of ExitFixture, which
// carries the manager's members with their real types (decltype) and stand-ins for the three pointers it calls
// through. The actions go into a REAL CgsModule::VariableEventQueue<13312,16> (== GameStateModuleIO::GameActionQueue).
// The console's record (0x82398D40..0x82398D88): +0x00 = maSpawnLocations[4]->mPosition (16 bytes), +0x10 = 0,
// id 77, 32 bytes.
#include "GameSource/GameState/CarSelect/BrnCarSelectManager.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/GameState/Progression/BrnProgressionCarData.h"
#include "SharedClasses/Trigger/BrnSpawnLocation.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstring>
#include <vector>

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

namespace BrnGameState
{
#include "exit_state_locals.inc"

    // The three pointers UpdateExitState calls through, as recording stand-ins with the called members' shapes.
    struct FakeProgressionManager
    {
        unsigned muDriveThrusDirty = 0, muRivals = 0;
        void SetDriveThrusDirtyFlag() { ++muDriveThrusDirty; }
        void RequestUpdateRivals() { ++muRivals; }
    };
    struct FakeGameStateModule
    {
        unsigned muUnpauses = 0, muCarChanges = 0;
        s32      miUnpauseReason = -1;
        bool     mbOnline = false;
        void RequestUnpause(s32 liReason, GameStateModuleIO::GameActionQueue*) { ++muUnpauses; miUnpauseReason = liReason; }
        bool IsOnlineGameMode() { return mbOnline; }
        void OnPlayerCarChange(CgsID, CgsID, GameStateModuleIO::GameActionQueue*, bool) { ++muCarChanges; }
    };

    struct ExitFixture
    {
        typedef CarSelectManager M;
        static const M::State E_STATE_NONE    = M::E_STATE_NONE;
        static const M::State E_STATE_EXITING = M::E_STATE_EXITING;

        decltype(M::meState)               meState;
        decltype(M::mfStateTimer)          mfStateTimer;
        decltype(M::mJunkyardId)           mJunkyardId;
        decltype(M::maSpawnLocations)      maSpawnLocations;
        decltype(M::mDesiredCarId)         mDesiredCarId;
        decltype(M::mbWaitingForStreaming) mbWaitingForStreaming;
        decltype(M::mbInCarModScreen)      mbInCarModScreen;
        HostPointer<FakeProgressionManager> mpProgressionManager;
        HostPointer<FakeGameStateModule>    mpGameStateModule;
        const BrnProgression::CarData*      mpDesiredCarData;

        const BrnProgression::CarData* GetProfileCarData(CgsID&) const { return mpDesiredCarData; }

        void UpdateExitState(GameStateModuleIO::GameActionQueue* lpActionQueue);
    };

#include "exit_state_body.inc"
}

using namespace BrnGameState;

static void Check(bool lbPass, const char* lpcWhat)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("FAIL: %s\n", lpcWhat);
    }
}

struct Posted
{
    s32                      miType;
    s32                      miSize;
    std::vector<unsigned char> maBytes;
};

static std::vector<Posted> Drain(const GameStateModuleIO::GameActionQueue& lrQueue)
{
    std::vector<Posted> laPosted;
    const CgsModule::Event* lpEvent = nullptr;
    s32 liSize = 0;
    s32 liType = lrQueue.GetFirstEvent(&lpEvent, &liSize);
    while (lpEvent != nullptr)
    {
        Posted lPosted;
        lPosted.miType = liType;
        lPosted.miSize = liSize;
        const unsigned char* lpBytes = reinterpret_cast<const unsigned char*>(lpEvent);
        lPosted.maBytes.assign(lpBytes, lpBytes + liSize);
        laPosted.push_back(lPosted);
        liType = lrQueue.GetNextEvent(lpEvent, &lpEvent, &liSize);
    }
    return laPosted;
}

static int IndexOf(const std::vector<Posted>& laPosted, s32 liType)
{
    for (size_t i = 0; i < laPosted.size(); ++i)
    {
        if (laPosted[i].miType == liType)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

static bool SameFloat(const unsigned char* lpBytes, f32 lfExpected)
{
    f32 lfValue;
    std::memcpy(&lfValue, lpBytes, sizeof(lfValue));
    return std::memcmp(&lfValue, &lfExpected, sizeof(lfValue)) == 0;
}

static std::vector<Posted> RunExit(ExitFixture& lrFixture, const BrnTrigger::SpawnLocation& lrExit)
{
    static GameStateModuleIO::GameActionQueue sQueue;
    sQueue.Construct();
    lrFixture.maSpawnLocations[4].Set(&lrExit);
    lrFixture.meState = ExitFixture::E_STATE_EXITING;
    lrFixture.UpdateExitState(&sQueue);
    return Drain(sQueue);
}

int main()
{
    static BrnProgression::CarData sCar;
    std::memset(&sCar, 0, sizeof(sCar));
    static FakeProgressionManager sProgression;
    static FakeGameStateModule    sGameState;

    static ExitFixture sFixture;
    std::memset(&sFixture, 0, sizeof(sFixture));
    sFixture.mpProgressionManager.Set(&sProgression);
    sFixture.mpGameStateModule.Set(&sGameState);
    sFixture.mpDesiredCarData = &sCar;
    sFixture.mDesiredCarId    = 0x1122334455667788ull;

    // The exit spawn location: the junkyard road the harness drives off (baseline_boot_drive), exact in f32.
    static BrnTrigger::SpawnLocation sExit;
    std::memset(&sExit, 0, sizeof(sExit));
    sExit.mPosition  = Vector3{ 3040.75f, -5.8125f, -1937.875f, 1.5f };   // w != 0: all 16 bytes travel
    sExit.mDirection = Vector3{ 0.0f, 0.0f, -1.0f, 0.0f };

    // ---- 1. the post ----------------------------------------------------------------------------------------------
    const std::vector<Posted> laFirst = RunExit(sFixture, sExit);
    const int liExit = IndexOf(laFirst, 77);
    Check(liExit >= 0, "UpdateExitState posts action 77 (0x82398D74 li r5, 0x4D)");
    if (liExit < 0)
    {
        std::printf("FxTrafficLightsCarSelectExit: %u checks, %u failures\n", gChecks + 15, gFailures + 15);
        return 1;
    }
    const Posted& lrExit = laFirst[liExit];
    Check(lrExit.miSize == 32, "action 77 is 32 bytes (0x82398D70 li r6, 0x20)");
    Check(lrExit.maBytes.size() >= 32, "the record's 32 bytes are in the queue");

    unsigned char lacPosition[16];
    std::memcpy(lacPosition, &sExit.mPosition, sizeof(lacPosition));
    Check(std::memcmp(lrExit.maBytes.data(), lacPosition, 16) == 0,
          "+0x00..+0x0F are the exit spawn location's mPosition, all 16 bytes (0x82398D6C lvx128 / 0x82398D80 stvx128)");
    Check(SameFloat(&lrExit.maBytes[0], 3040.75f), "+0x00 x == 3040.75 (the spawn x, not 0)");
    Check(SameFloat(&lrExit.maBytes[4], -5.8125f), "+0x04 y == -5.8125");
    Check(SameFloat(&lrExit.maBytes[8], -1937.875f), "+0x08 z == -1937.875");
    Check(lrExit.maBytes[16] == 0, "+0x10 mbOnlineCarSelect == false (0x82398D84 stb r31 == 0)");
    bool lbTailZero = true;
    for (int i = 17; i < 32; ++i)
    {
        lbTailZero = lbTailZero && (lrExit.maBytes[i] == 0);
    }
    Check(lbTailZero, "+0x11..+0x1F are zero (the PC value-initialises what the console leaves as stack bytes)");

    const GameStateModuleIO::CarSelectExitAction* lpRecord =
        reinterpret_cast<const GameStateModuleIO::CarSelectExitAction*>(lrExit.maBytes.data());
    Check(std::memcmp(&lpRecord->mExitSpawnLocation, &sExit.mPosition, 16) == 0 && !lpRecord->mbOnlineCarSelect,
          "read back as CarSelectExitAction (the traffic's arm 77 view): mExitSpawnLocation == mPosition, offline");

    // ---- 2. the post order: 76 -> 70 -> 77 -> 99 (0x82398CF8 / 0x82398D3C / 0x82398D88 / 0x82398DA0) ----------------
    const int li76 = IndexOf(laFirst, 76), li70 = IndexOf(laFirst, 70), li99 = IndexOf(laFirst, 99);
    Check(li76 >= 0 && li70 > li76 && liExit > li70 && li99 > liExit,
          "the post order is car-mod screen (76), reset driver (70), exit (77), junkyard entered (99)");
    Check(IndexOf(laFirst, 0) > li99, "the reset-player-car record (0) follows");

    // ---- 3. a second exit from another spawn point: the record follows the spawn location ------------------------
    static BrnTrigger::SpawnLocation sOtherExit;
    std::memset(&sOtherExit, 0, sizeof(sOtherExit));
    sOtherExit.mPosition = Vector3{ -812.5f, 21.25f, 640.0f, 0.0f };
    sFixture.mDesiredCarId = 0x0102030405060708ull;
    const std::vector<Posted> laSecond = RunExit(sFixture, sOtherExit);
    const int liSecond = IndexOf(laSecond, 77);
    Check(liSecond >= 0 && SameFloat(&laSecond[liSecond].maBytes[0], -812.5f)
          && SameFloat(&laSecond[liSecond].maBytes[4], 21.25f) && SameFloat(&laSecond[liSecond].maBytes[8], 640.0f),
          "a second exit carries the second spawn location's position");

    // ---- 4. streaming still pending: nothing is posted ------------------------------------------------------------
    sFixture.mbWaitingForStreaming = true;
    const std::vector<Posted> laWaiting = RunExit(sFixture, sExit);
    Check(laWaiting.empty(), "while mbWaitingForStreaming the exit posts nothing (the early return)");
    sFixture.mbWaitingForStreaming = false;

    // ---- 5. the rest of the exit still runs --------------------------------------------------------------------------
    Check(sGameState.muUnpauses == 2 && sGameState.miUnpauseReason == KI_PAUSE_REASON_CAR_SELECT,
          "each completed exit releases the car-select pause reason once");
    Check(gAsserts == 0, "no assert fired");

    std::printf("FxTrafficLightsCarSelectExit: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
