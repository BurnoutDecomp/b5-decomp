// FX-GS2 (crash parity 2026-09-23): the PRODUCTION BrnGameState::PaybackManager bodies, extracted from
// src/GameSource/GameState/PaybackManager/BrnPaybackManager.cpp (and the GUI publishers from
// SharedIO/BrnGameStateToGuiIOInterfaces.cpp) by run_fxgs2_payback.py, driven through Update with the
// REAL PaybackManager / GameStateToGuiInterface / NetworkToGameStateInterface / GameStateToNetworkInterface
// types. GameStateModule, OutputBuffer and PreWorldInputBuffer are only forward-declared by the
// manager's header, so this TU supplies small fixtures for them. The physics body
// CrashingRaceCarInterface::SetFromVehicleOutputInterface (not under test) is a fixture that copies
// this test's per-slot "crashing" flags into the interface, so IsCrashing -- the header-inline body
// the arms poll -- reads real data.
//
// Checked against the ARTIST asm:
//   Update @0x8239AB78   aggressor jump table 0x8239AC2C = {AD00, AC44, AC54, AC64, AC7C, AC8C}
//                        [1] HandleWaitForPaybackAggressorToCrash(vehicle output = r21)
//   HandleWaitForPaybackAggressorToCrash @0x823977F0
//                        SetFromVehicleOutputInterface on a stack copy (0x8239783C); lbzx
//                        crashing[player] (0x8239784C); beq -> return; else -1.0 (flt_820037C8)
//                        -> +0x24C, 2 -> +0x25C, 0 -> +0x266, AddEvent(outGui, &1, 0xB0, 4)   (G12-D1)
#include "GameSource/GameState/PaybackManager/BrnPaybackManager.h"
#include "GameSource/GameState/SharedIO/BrnGameStateToGuiIOInterfaces.h"
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <limits>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++gAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }   // the [payback] witness stays silent
namespace Message { u64 gxMessageFilterFlags = 0; }      // VariableEventQueue::OutputQueueContents
}

// ---- the physics fixture: this frame's per-slot "is crashing" flags --------------------------
// The console's SetFromVehicleOutputInterface (0x823625C0) fills the 8 flag bytes from the vehicle
// output; here it copies gabCrashing and records which vehicle output it was handed.
static bool gabCrashing[8] = {};
static unsigned gSetFromCalls = 0;
static const BrnPhysics::Vehicle::VehicleOutputInterface* gpSetFromArg = nullptr;
void BrnPhysics::Vehicle::CrashingRaceCarInterface::SetFromVehicleOutputInterface(const VehicleOutputInterface* lpOutput)
{
    ++gSetFromCalls;
    gpSetFromArg = lpOutput;
    for (s32 liIndex = 0; liIndex < 8; ++liIndex)
        mabCrashingRaceCars[liIndex] = gabCrashing[liIndex];
}
// Update only passes the pointer through; the fixture above never dereferences it.
alignas(16) static unsigned char gaVehicleOutputToken[16];
static const BrnPhysics::Vehicle::VehicleOutputInterface* const gpVehicleOutput =
    reinterpret_cast<const BrnPhysics::Vehicle::VehicleOutputInterface*>(gaVehicleOutputToken);

// ---- fixtures for the three types the manager's header only forward-declares -----------------
namespace BrnGameState
{
class GameStateModule
{
public:
    ::EActiveRaceCarIndex                    mePlayer = ::E_ACTIVE_RACE_CAR_INDEX_2;
    bool                                     mabModuleCrashing[8] = {};
    CgsModule::VariableEventQueue<18432, 16> mOutputGuiEventQueue;

    ::EActiveRaceCarIndex GetPlayerActiveRaceCarIndex() { return mePlayer; }
    CgsModule::VariableEventQueue<18432, 16>* GetOutputGuiEventQueue() { return &mOutputGuiEventQueue; }
    bool IsRaceCarCrashing(::EActiveRaceCarIndex leIndex) { return mabModuleCrashing[leIndex]; }
    bool IsActiveRaceCarStillPresent(::EActiveRaceCarIndex) const { return true; }
};

namespace GameStateModuleIO
{
// The alias BrnGameStateModuleIO.h gives the network interface inside GameStateModuleIO.
typedef BrnNetwork::BrnNetworkModuleIO::NetworkToGameStateInterface NetworkToGameStateInterface;

struct OutputBuffer
{
    GameStateToGuiInterface  mGui;
    GameActionQueue          mActions;          // VariableEventQueue<13312,16>, OutputBuffer +0x04
    BrnNetwork::BrnNetworkModuleIO::GameStateToNetworkInterface mNetwork;   // OutputBuffer +0x4190
    BrnNetwork::EPaybackType meActivePaybackType      = static_cast<BrnNetwork::EPaybackType>(99);
    ::EActiveRaceCarIndex    meActivePaybackAggressor = static_cast<::EActiveRaceCarIndex>(99);

    GameStateToGuiInterface* GetGameStateToGuiInterface() { return &mGui; }
    GameActionQueue*         GetGameActionQueue() { return &mActions; }
    CgsModule::VariableEventQueue<13312, 16>* GetGuiOutputQueue() { return &mActions; }
    BrnNetwork::BrnNetworkModuleIO::GameStateToNetworkInterface* GetGameStateToNetworkInterface() { return &mNetwork; }
    void SetActivePaybackType(BrnNetwork::EPaybackType leType) { meActivePaybackType = leType; }
    void SetActivePaybackAggressor(::EActiveRaceCarIndex leAggressor) { meActivePaybackAggressor = leAggressor; }
};

struct PreWorldInputBuffer
{
    BrnNetwork::BrnNetworkModuleIO::NetworkToGameStateInterface mNetworkToGameStateInterface;
    const BrnNetwork::BrnNetworkModuleIO::NetworkToGameStateInterface* GetNetworkToGameStateInterface() const
    {
        return &mNetworkToGameStateInterface;
    }
};
}
}

// The production bodies under test (or, for a revision that lacks one, the runner's labelled
// empty stand-in -- the defect state: the console calls it, the revision does nothing).
#include "payback2_methods.inc"

using namespace BrnGameState;
typedef PaybackManager PM;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

// Heap-free, ctor-free storage: the manager embeds a debug component whose ctor/vtable live in the
// debug system (not under test); every member the bodies touch is set explicitly below.
alignas(16) static unsigned char gaManagerStorage[sizeof(PaybackManager)];
static GameStateModule                        gModule;
static GameStateModuleIO::OutputBuffer        gOutput;
static GameStateModuleIO::PreWorldInputBuffer gInput;
static CgsModule::EventQueue<TakedownEvent, 8> gTakedowns;

static PaybackManager& Fresh(PM::EPaybackVictimState leVictim, PM::EPaybackAggressorState leAggressor)
{
    std::memset(gaManagerStorage, 0, sizeof(gaManagerStorage));
    PaybackManager& lr = *reinterpret_cast<PaybackManager*>(gaManagerStorage);
    lr.mTimerStatusInterface.Clear();
    lr.mTimerStatusInterface.mGameTimerStatus.mfBaseTimeStep       = 0.25f;
    lr.mTimerStatusInterface.mGameTimerStatus.mfTimeStepMultiplier = 1.0f;
    lr.mDirtyTrickOutputQueue.Construct();
    lr.mpGameStateModule               = &gModule;
    lr.mePaybackAggressorRaceCarIndex  = ::E_ACTIVE_RACE_CAR_INDEX_5;
    lr.mePaybackVictimRaceCarIndex     = ::E_ACTIVE_RACE_CAR_INDEX_6;
    lr.meActiveDirtyTrickType          = static_cast<BrnNetwork::EPaybackType>(1);
    lr.meAwardedDirtyTrick             = static_cast<BrnNetwork::EPaybackType>(2);
    lr.mfCountdownTimer                = 5.0f;
    lr.mfPaybackAggTimer               = -1.0f;
    lr.mfPaybackVictimTimer            = 7.5f;
    lr.mbPaybackAwarded                = true;   // so a store of 0 is observable
    lr.mePaybackVictimState            = leVictim;
    lr.mePaybackAggressorState         = leAggressor;
    lr.mDebugComponent.mpPaybackManager = &lr;

    gModule.mePlayer = ::E_ACTIVE_RACE_CAR_INDEX_2;
    std::memset(gModule.mabModuleCrashing, 0, sizeof(gModule.mabModuleCrashing));
    gModule.mOutputGuiEventQueue.Construct();
    gOutput.mGui.Construct();
    gOutput.mActions.Construct();
    gOutput.mNetwork.GetDirtyTrickQueue()->Construct();
    gInput.mNetworkToGameStateInterface.GetDirtyTrickQueue()->Construct();
    gTakedowns.Construct();
    std::memset(gabCrashing, 0, sizeof(gabCrashing));
    gSetFromCalls = 0;
    gpSetFromArg  = nullptr;
    return lr;
}

static void Tick(PaybackManager& lr)
{
    lr.Update(&gInput, &gOutput, gpVehicleOutput, &gTakedowns, GameStateModuleIO::E_MODE_ONLINE_RACE);
}

// Count / fetch events of one type in a variable event queue.
template <s32 N>
static s32 CountType(const CgsModule::VariableEventQueue<N, 16>& lrQueue, s32 liType, const void** lppLast, s32* lpiSize)
{
    s32 liCount = 0;
    const CgsModule::Event* lpEvent = nullptr;
    s32 liSize = 0;
    s32 liEventType = lrQueue.GetFirstEvent(&lpEvent, &liSize);
    while (lpEvent != nullptr)
    {
        if (liEventType == liType)
        {
            ++liCount;
            if (lppLast) *lppLast = lpEvent;
            if (lpiSize) *lpiSize = liSize;
        }
        liEventType = lrQueue.GetNextEvent(lpEvent, &lpEvent, &liSize);
    }
    return liCount;
}

static s32 ReadS32(const void* lp) { s32 li; std::memcpy(&li, lp, 4); return li; }

// Exactly one type-176 (0xB0) record, 4 bytes, carrying 1: the aggressor ChangeState's "show".
static bool OneShowRecord()
{
    const void* lpLast = nullptr;
    s32 liSize = 0;
    return CountType(gModule.mOutputGuiEventQueue, 176, &lpLast, &liSize) == 1 && liSize == 4
        && lpLast != nullptr && ReadS32(lpLast) == 1;
}

int main()
{
    // ===================== G12-D1: HandleWaitForPaybackAggressorToCrash through arm [1] ============
    {
        PaybackManager& lr = Fresh(PM::E_PAYBACK_VICTIM_STATE_IDLE, PM::E_PAYBACK_AGGRESSOR_STATE_WAIT_AWARD_PAYBACK);
        gabCrashing[2] = true;   // the player's slot
        Tick(lr);
        Check(gSetFromCalls == 1 && gpSetFromArg == gpVehicleOutput,
              "D1 the arm snapshots the vehicle output Update was handed  @0x8239783C");
        Check(lr.mePaybackAggressorState == PM::E_PAYBACK_AGGRESSOR_STATE_AWARD_DT,
              "D1 player crashing -> aggressor state 2  @0x82397874");
        Check(lr.mfPaybackAggTimer == -1.0f, "D1 ...+0x24C = -1.0 (flt_820037C8)  @0x82397870");
        Check(!lr.mbPaybackAwarded, "D1 ...+0x266 = 0  @0x82397878");
        Check(OneShowRecord(), "D1 ...and one GUI 176 (0xB0) record, 4 bytes, value 1  @0x82397890");
        Check(lr.mePaybackVictimRaceCarIndex == ::E_ACTIVE_RACE_CAR_INDEX_6
                  && lr.meAwardedDirtyTrick == static_cast<BrnNetwork::EPaybackType>(2),
              "D1 the arm stores nothing else (victim index, awarded trick untouched)");
    }
    {
        PaybackManager& lr = Fresh(PM::E_PAYBACK_VICTIM_STATE_IDLE, PM::E_PAYBACK_AGGRESSOR_STATE_WAIT_AWARD_PAYBACK);
        gabCrashing[5] = true;   // another car is crashing, not the player
        Tick(lr);
        Check(lr.mePaybackAggressorState == PM::E_PAYBACK_AGGRESSOR_STATE_WAIT_AWARD_PAYBACK && lr.mbPaybackAwarded,
              "D1 player not crashing -> stays in state 1 (the flag is read at the PLAYER's slot)");
        Check(CountType(gModule.mOutputGuiEventQueue, 176, nullptr, nullptr) == 0 && lr.mfPaybackAggTimer == 0.0f,
              "D1 ...no GUI 176, and the timer only took Update's -1.0 -> 0.0 step  @0x8239ABC4");
        gabCrashing[2] = true;   // next frame the player crashes
        Tick(lr);
        Check(lr.mePaybackAggressorState == PM::E_PAYBACK_AGGRESSOR_STATE_AWARD_DT && lr.mfPaybackAggTimer == -1.0f,
              "D1 the next frame's crash advances it (state 2, timer -1.0)");
    }
    {
        PaybackManager& lr = Fresh(PM::E_PAYBACK_VICTIM_STATE_IDLE, PM::E_PAYBACK_AGGRESSOR_STATE_WAIT_AWARD_PAYBACK);
        gModule.mePlayer = ::E_ACTIVE_RACE_CAR_INDEX_5;
        gabCrashing[5] = true;
        Tick(lr);
        Check(lr.mePaybackAggressorState == PM::E_PAYBACK_AGGRESSOR_STATE_AWARD_DT,
              "D1 the byte is indexed by GetPlayerActiveRaceCarIndex (player 5, slot 5 crashing -> state 2)");
    }

    Check(gAsserts == 0, "valid fixtures fire no assert");
    std::printf("FxGs2Payback: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
