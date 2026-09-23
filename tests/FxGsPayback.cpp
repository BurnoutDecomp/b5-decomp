// FX-GS (crash parity 2026-09-23): the PRODUCTION BrnGameState::PaybackManager bodies, extracted
// from src/GameSource/GameState/PaybackManager/BrnPaybackManager.cpp (and the GUI publishers from
// SharedIO/BrnGameStateToGuiIOInterfaces.cpp) by run_fxgs_payback.py, driven through the REAL
// PaybackManager / GameStateToGuiInterface / NetworkToGameStateInterface / DirtyTrickEvent types.
// GameStateModule, OutputBuffer and PreWorldInputBuffer are only forward-declared by the manager's
// header, so this TU supplies small fixtures for them (counting the calls the console makes).
//
// Checked against the ARTIST asm:
//   Update @0x8239AB78            victim jump table 0x8239AD24 = {ADE0, AD38, AD4C, AD5C, AD6C}:
//                                 [2] HandleActivePayback(out), [3] HandleCrashDueToPayback(out),
//                                 [4] HandleSurvivingPayback(out)                        (G12-D8/9/10)
//   HandleActivePayback @0x82397CC8  SetActivePaybackType(+0x254), SetActivePaybackAggressor(+0x240);
//                                 IsRaceCarCrashing(player) -> 3; countdown < 0.0 (flt_82001CC0,
//                                 fcmpu/blt: NaN is not) -> 4; else UpdateCountdown            (D8)
//   HandleCrashDueToPayback @0x82397D80 / HandleSurvivingPayback @0x82397EA0
//                                 -1.0 (flt_820037C8) -> +0x248 and GUI 235 (0xEB, 4 bytes);
//                                 Ending record {+0x240, player, +0x254, survived 0|1} at gui+0x7C;
//                                 SendNetworkDirtyTrickMessage(+0x240, player, +0x254, 4|3);
//                                 action 0xD8 size 1; +0x254 = 3, +0x260 = 0                   (D9/D10)
//   HandleTriggeringPayback @0x82397C08  the gui+0x40 Triggered record {player, victim, trick}
//                                 (0x82397C44..0x82397C74) before the end stores              (G12-D2)
//   ProcessDirtyTrickEventQueue @0x82383CA8  status 2 on the player -> +0x260 = 1, +0x240 = agg,
//                                 mEvent copy; 3 -> Ending survived 1; 4 -> Ending survived 0 and,
//                                 when the aggressor is the player, action 0xD9 {agg, victim}   (G12-D3)
//   Destruct @0x8236D110          debug back-pointer 0 + DebugComponent::Destruct, TimerStatus Clear,
//                                 +0x38 = 0, the ResetState stores, +0x268 = 0; +0x250 untouched (G12-D12)
#include "GameSource/GameState/PaybackManager/BrnPaybackManager.h"
#include "GameSource/GameState/SharedIO/BrnGameStateToGuiIOInterfaces.h"
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"
#include "GameSource/GameState/BrnGameActions.h"      // PaybackActivatedAction (HandleReceivingPayback, G12-D7)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <limits>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0, gDebugDestructs = 0;

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
// Harness-only: the base DebugComponent::Destruct (CgsDebugComponent.cpp, not under test) --
// counted, so the test sees the console's `bl 0x8284CB38` from PaybackManager::Destruct.
void DebugComponent::Destruct() { ++gDebugDestructs; }
}

// Harness-only: the physics body (not under test). HandleWaitForPaybackAggressorToCrash builds the
// interface; nothing below drives the aggressor state that reads it.
void BrnPhysics::Vehicle::CrashingRaceCarInterface::SetFromVehicleOutputInterface(const VehicleOutputInterface*) {}

// ---- fixtures for the three types the manager's header only forward-declares -----------------
namespace BrnGameState
{
class GameStateModule
{
public:
    ::EActiveRaceCarIndex                    mePlayer = ::E_ACTIVE_RACE_CAR_INDEX_2;
    bool                                     mabCrashing[8] = {};
    unsigned                                 muCrashQueries = 0;
    CgsModule::VariableEventQueue<18432, 16> mOutputGuiEventQueue;

    ::EActiveRaceCarIndex GetPlayerActiveRaceCarIndex() { return mePlayer; }
    CgsModule::VariableEventQueue<18432, 16>* GetOutputGuiEventQueue() { return &mOutputGuiEventQueue; }
    bool IsRaceCarCrashing(::EActiveRaceCarIndex leIndex) { ++muCrashQueries; return mabCrashing[leIndex]; }
    bool IsActiveRaceCarStillPresent(::EActiveRaceCarIndex) const { return true; }
};

namespace GameStateModuleIO
{
// The alias BrnGameStateModuleIO.h:347 gives the network interface inside GameStateModuleIO.
typedef BrnNetwork::BrnNetworkModuleIO::NetworkToGameStateInterface NetworkToGameStateInterface;

struct OutputBuffer
{
    GameStateToGuiInterface  mGui;
    GameActionQueue          mActions;          // VariableEventQueue<13312,16>, OutputBuffer +0x04
    BrnNetwork::BrnNetworkModuleIO::GameStateToNetworkInterface mNetwork;   // OutputBuffer +0x4190 (1839b90d: the Update tail Appends the dirty tricks here)
    BrnNetwork::EPaybackType meActivePaybackType      = static_cast<BrnNetwork::EPaybackType>(99);
    ::EActiveRaceCarIndex    meActivePaybackAggressor = static_cast<::EActiveRaceCarIndex>(99);
    unsigned                 muSetType = 0, muSetAggressor = 0;

    GameStateToGuiInterface* GetGameStateToGuiInterface() { return &mGui; }
    GameActionQueue*         GetGameActionQueue() { return &mActions; }
    BrnNetwork::BrnNetworkModuleIO::GameStateToNetworkInterface* GetGameStateToNetworkInterface() { return &mNetwork; }
    CgsModule::VariableEventQueue<13312, 16>* GetGuiOutputQueue() { return &mActions; }
    void SetActivePaybackType(BrnNetwork::EPaybackType leType) { meActivePaybackType = leType; ++muSetType; }
    void SetActivePaybackAggressor(::EActiveRaceCarIndex leAggressor) { meActivePaybackAggressor = leAggressor; ++muSetAggressor; }
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
#include "payback_methods.inc"

using namespace BrnGameState;
using BrnNetwork::BrnNetworkModuleIO::DirtyTrickEvent;
typedef GameStateModuleIO::GameStateToGuiInterface GuiInterface;

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
static GameStateModule                   gModule;
static GameStateModuleIO::OutputBuffer   gOutput;
static GameStateModuleIO::PreWorldInputBuffer gInput;
static CgsModule::EventQueue<TakedownEvent, 8> gTakedowns;

static PaybackManager& Fresh(PaybackManager::EPaybackVictimState leVictim,
                             PaybackManager::EPaybackAggressorState leAggressor)
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
    lr.mePaybackVictimState            = leVictim;
    lr.mePaybackAggressorState         = leAggressor;
    lr.mDebugComponent.mpPaybackManager = &lr;

    gModule.mePlayer = ::E_ACTIVE_RACE_CAR_INDEX_2;
    std::memset(gModule.mabCrashing, 0, sizeof(gModule.mabCrashing));
    gModule.muCrashQueries = 0;
    gModule.mOutputGuiEventQueue.Construct();
    gOutput.mGui.Construct();
    gOutput.mActions.Construct();
    gOutput.mNetwork.GetDirtyTrickQueue()->Construct();
    gOutput.meActivePaybackType      = static_cast<BrnNetwork::EPaybackType>(99);
    gOutput.meActivePaybackAggressor = static_cast<::EActiveRaceCarIndex>(99);
    gOutput.muSetType = gOutput.muSetAggressor = 0;
    gInput.mNetworkToGameStateInterface.GetDirtyTrickQueue()->Construct();
    gTakedowns.Construct();
    return lr;
}

static void Tick(PaybackManager& lr)
{
    lr.Update(&gInput, &gOutput, nullptr, &gTakedowns, GameStateModuleIO::E_MODE_ONLINE_RACE);
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

static f32 CountdownRecord(const void* lp) { f32 lf; std::memcpy(&lf, lp, 4); return lf; }

int main()
{
    const f32 lfNaN = std::numeric_limits<f32>::quiet_NaN();

    // ===================== G12-D8: HandleActivePayback through Update's arm [2] ==================
    {
        PaybackManager& lr = Fresh(PaybackManager::E_PAYBACK_VICTIM_STATE_ACTIVE, PaybackManager::E_PAYBACK_AGGRESSOR_STATE_IDLE);
        gModule.mabCrashing[2] = true;
        Tick(lr);
        Check(gOutput.muSetType == 1 && gOutput.meActivePaybackType == static_cast<BrnNetwork::EPaybackType>(1),
              "D8 arm [2] publishes SetActivePaybackType(+0x254)  @0x82397CEC");
        Check(gOutput.muSetAggressor == 1 && gOutput.meActivePaybackAggressor == ::E_ACTIVE_RACE_CAR_INDEX_5,
              "D8 arm [2] publishes SetActivePaybackAggressor(+0x240)  @0x82397CF8");
        Check(gModule.muCrashQueries == 1, "D8 asks the module IsRaceCarCrashing(player)  @0x82397D0C");
        Check(lr.mePaybackVictimState == PaybackManager::E_PAYBACK_VICTIM_STATE_YOU_CRASHED,
              "D8 player crashing -> victim state 3  @0x82397D1C");
        Check(lr.mfCountdownTimer == 5.0f, "D8 a crash does not tick the countdown");
    }
    {
        PaybackManager& lr = Fresh(PaybackManager::E_PAYBACK_VICTIM_STATE_ACTIVE, PaybackManager::E_PAYBACK_AGGRESSOR_STATE_IDLE);
        lr.mfCountdownTimer = -0.5f;
        Tick(lr);
        Check(lr.mePaybackVictimState == PaybackManager::E_PAYBACK_VICTIM_STATE_YOU_SURVIVED,
              "D8 countdown < 0.0 -> victim state 4  @0x82397D50");
    }
    {
        PaybackManager& lr = Fresh(PaybackManager::E_PAYBACK_VICTIM_STATE_ACTIVE, PaybackManager::E_PAYBACK_AGGRESSOR_STATE_IDLE);
        Tick(lr);   // 5.0 - 0.25 * 1.0
        const void* lpLast = nullptr;
        s32 liSize = 0;
        Check(lr.mePaybackVictimState == PaybackManager::E_PAYBACK_VICTIM_STATE_ACTIVE && lr.mfCountdownTimer == 4.75f,
              "D8 otherwise UpdateCountdown: 5.0 - 0.25 = 4.75, still ACTIVE  @0x82397D60");
        Check(CountType(gModule.mOutputGuiEventQueue, 235, &lpLast, &liSize) == 1 && liSize == 4
                  && lpLast != nullptr && CountdownRecord(lpLast) == 4.75f,
              "D8 ...and the countdown record 235 carries 4.75");
    }
    {
        PaybackManager& lr = Fresh(PaybackManager::E_PAYBACK_VICTIM_STATE_ACTIVE, PaybackManager::E_PAYBACK_AGGRESSOR_STATE_IDLE);
        lr.mfCountdownTimer = lfNaN;
        Tick(lr);
        Check(lr.mePaybackVictimState == PaybackManager::E_PAYBACK_VICTIM_STATE_ACTIVE && lr.mfCountdownTimer == -1.0f,
              "D8 a NaN countdown is NOT complete (fcmpu/blt): UpdateCountdown runs and parks it at -1.0");
    }

    // ===================== G12-D9 / D10: the two terminal arms through Update ====================
    for (int liArm = 0; liArm < 2; ++liArm)
    {
        const bool lbSurvived = (liArm == 1);
        PaybackManager& lr = Fresh(lbSurvived ? PaybackManager::E_PAYBACK_VICTIM_STATE_YOU_SURVIVED
                                              : PaybackManager::E_PAYBACK_VICTIM_STATE_YOU_CRASHED,
                                   PaybackManager::E_PAYBACK_AGGRESSOR_STATE_IDLE);
        Tick(lr);
        const void* lpLast = nullptr;
        s32 liSize = 0;
        Check(lr.mfCountdownTimer == -1.0f,
              lbSurvived ? "D10 +0x248 = -1.0 (flt_820037C8)  @0x82397EC0" : "D9 +0x248 = -1.0 (flt_820037C8)  @0x82397DA0");
        Check(CountType(gModule.mOutputGuiEventQueue, 235, &lpLast, &liSize) == 1 && liSize == 4
                  && lpLast != nullptr && CountdownRecord(lpLast) == -1.0f,
              lbSurvived ? "D10 GUI 235 (0xEB) carries -1.0  @0x82397ED8" : "D9 GUI 235 (0xEB) carries -1.0  @0x82397DB8");
        const GuiInterface::DirtyTrickEndingQueue& lrEnding = gOutput.mGui.mDirtyTrickEndingQueue;
        Check(lrEnding.GetLength() == 1
                  && lrEnding.GetEvent(0).meAggressorActiveRaceCarIndex == ::E_ACTIVE_RACE_CAR_INDEX_5
                  && lrEnding.GetEvent(0).meVictimActiveRaceCarIndex == ::E_ACTIVE_RACE_CAR_INDEX_2
                  && lrEnding.GetEvent(0).meTrickType == static_cast<BrnNetwork::EPaybackType>(1)
                  && lrEnding.GetEvent(0).mbSurvived == lbSurvived,
              lbSurvived ? "D10 gui+0x7C Ending record {+0x240, player, +0x254, survived 1}  @0x82397F14"
                         : "D9 gui+0x7C Ending record {+0x240, player, +0x254, survived 0}  @0x82397DF4");
        Check(CountType(gOutput.mActions, 216, nullptr, &liSize) == 1 && liSize == 1,
              lbSurvived ? "D10 action 0xD8 (PaybackOverAction), 1 byte  @0x82397FA8"
                         : "D9 action 0xD8 (PaybackOverAction), 1 byte  @0x82397E88");
        Check(lr.meActiveDirtyTrickType == PaybackManager::KE_NO_DIRTY_TRICK
                  && lr.mePaybackVictimState == PaybackManager::E_PAYBACK_VICTIM_STATE_IDLE,
              lbSurvived ? "D10 end stores +0x254 = 3, +0x260 = 0  @0x82397FB4" : "D9 end stores +0x260 = 0, +0x254 = 3  @0x82397E90");
        Check(lr.mDirtyTrickOutputQueue.GetLength() == 0, "Update's tail still clears +0x38 (the per-frame outbound queue)");
    }
    // The broadcast (Update's tail clears the outbound queue, so read it straight after the arm).
    for (int liArm = 0; liArm < 2; ++liArm)
    {
        const bool lbSurvived = (liArm == 1);
        PaybackManager& lr = Fresh(lbSurvived ? PaybackManager::E_PAYBACK_VICTIM_STATE_YOU_SURVIVED
                                              : PaybackManager::E_PAYBACK_VICTIM_STATE_YOU_CRASHED,
                                   PaybackManager::E_PAYBACK_AGGRESSOR_STATE_IDLE);
        if (lbSurvived) lr.HandleSurvivingPayback(&gOutput);
        else            lr.HandleCrashDueToPayback(&gOutput);
        const auto& lrOut = lr.mDirtyTrickOutputQueue;
        Check(lrOut.GetLength() == 1
                  && lrOut.GetEvent(0).meAggressorActiveRaceCarIndex == ::E_ACTIVE_RACE_CAR_INDEX_5
                  && lrOut.GetEvent(0).meVictimActiveRaceCarIndex == ::E_ACTIVE_RACE_CAR_INDEX_2
                  && lrOut.GetEvent(0).meDirtyTrickType == static_cast<BrnNetwork::EPaybackType>(1)
                  && static_cast<s32>(lrOut.GetEvent(0).meDirtyTrickStatus) == (lbSurvived ? 3 : 4),
              lbSurvived ? "D10 network message {+0x240, player, +0x254, status 3}  @0x82397F34"
                         : "D9 network message {+0x240, player, +0x254, status 4}  @0x82397E14");
    }

    // ===================== G12-D2: HandleTriggeringPayback's GUI record ==========================
    {
        PaybackManager& lr = Fresh(PaybackManager::E_PAYBACK_VICTIM_STATE_IDLE, PaybackManager::E_PAYBACK_AGGRESSOR_STATE_TRIGGER_DT);
        Tick(lr);
        const GuiInterface::DirtyTrickTriggeredQueue& lrTriggered = gOutput.mGui.mDirtyTrickTriggeredQueue;
        Check(lrTriggered.GetLength() == 1
                  && lrTriggered.GetEvent(0).meAggressorActiveRaceCarIndex == ::E_ACTIVE_RACE_CAR_INDEX_2
                  && lrTriggered.GetEvent(0).meVictimActiveRaceCarIndex == ::E_ACTIVE_RACE_CAR_INDEX_6
                  && lrTriggered.GetEvent(0).meTrickType == static_cast<BrnNetwork::EPaybackType>(2),
              "D2 gui+0x40 Triggered record {player, +0x244 victim, +0x258 trick}  @0x82397C74");
        Check(lr.mePaybackAggressorState == PaybackManager::E_PAYBACK_AGGRESSOR_STATE_IDLE
                  && lr.meAwardedDirtyTrick == PaybackManager::KE_NO_DIRTY_TRICK
                  && lr.mePaybackVictimRaceCarIndex == ::E_ACTIVE_RACE_CAR_INDEX_INVALID,
              "D2 the end stores still follow (state 0, trick 3, victim -1)  @0x82397C94..0x82397CA8");
    }

    // ===================== G12-D3: the inbound dirty-trick drain ================================
    {
        PaybackManager& lr = Fresh(PaybackManager::E_PAYBACK_VICTIM_STATE_IDLE, PaybackManager::E_PAYBACK_AGGRESSOR_STATE_IDLE);
        BrnNetwork::BrnNetworkModuleIO::NetworkToGameStateInterface::DirtyTrickQueue* lpIn =
            gInput.mNetworkToGameStateInterface.GetDirtyTrickQueue();
        DirtyTrickEvent lEvent;
        lEvent.meAggressorActiveRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_1; lEvent.meVictimActiveRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_4;
        lEvent.meDirtyTrickType = static_cast<BrnNetwork::EPaybackType>(0); lEvent.meDirtyTrickStatus = static_cast<BrnNetwork::EDirtyTrickStatus>(1);
        lpIn->AddEvent(lEvent);   // status 1: nothing
        lEvent.meAggressorActiveRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_7; lEvent.meVictimActiveRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_2;
        lEvent.meDirtyTrickType = static_cast<BrnNetwork::EPaybackType>(1); lEvent.meDirtyTrickStatus = static_cast<BrnNetwork::EDirtyTrickStatus>(2);
        lpIn->AddEvent(lEvent);   // status 2 on the player: become the victim
        lEvent.meAggressorActiveRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_4; lEvent.meVictimActiveRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_6;
        lEvent.meDirtyTrickType = static_cast<BrnNetwork::EPaybackType>(0); lEvent.meDirtyTrickStatus = static_cast<BrnNetwork::EDirtyTrickStatus>(3);
        lpIn->AddEvent(lEvent);   // status 3: ending, survived
        lEvent.meAggressorActiveRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_2; lEvent.meVictimActiveRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_3;
        lEvent.meDirtyTrickType = static_cast<BrnNetwork::EPaybackType>(2); lEvent.meDirtyTrickStatus = static_cast<BrnNetwork::EDirtyTrickStatus>(4);
        lpIn->AddEvent(lEvent);   // status 4 by the player: ending, crashed + action 217
        lr.mePaybackAggressorRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;
        Tick(lr);
        Check(lr.mePaybackVictimState == PaybackManager::E_PAYBACK_VICTIM_STATE_TRIGGERED_ON_YOU
                  && lr.mePaybackAggressorRaceCarIndex == ::E_ACTIVE_RACE_CAR_INDEX_7,
              "D3 status 2 on the player: +0x260 = 1, +0x240 = aggressor  @0x82383E18/0x82383E1C");
        Check(lr.mEvent.meAggressorActiveRaceCarIndex == ::E_ACTIVE_RACE_CAR_INDEX_7
                  && lr.mEvent.meVictimActiveRaceCarIndex == ::E_ACTIVE_RACE_CAR_INDEX_2
                  && static_cast<s32>(lr.mEvent.meDirtyTrickStatus) == 2,
              "D3 ...and the 16-byte event copy to +0x1FC  @0x82383E30..0x82383E3C");
        const GuiInterface::DirtyTrickEndingQueue& lrEnding = gOutput.mGui.mDirtyTrickEndingQueue;
        Check(lrEnding.GetLength() == 2 && lrEnding.GetEvent(0).mbSurvived == true
                  && lrEnding.GetEvent(0).meAggressorActiveRaceCarIndex == ::E_ACTIVE_RACE_CAR_INDEX_4
                  && lrEnding.GetEvent(1).mbSurvived == false
                  && lrEnding.GetEvent(1).meVictimActiveRaceCarIndex == ::E_ACTIVE_RACE_CAR_INDEX_3,
              "D3 status 3 / 4 -> Ending records survived 1 / 0  @0x82383E70 / 0x82383EA4");
        const void* lpLast = nullptr;
        s32 liSize = 0;
        s32 laiPair[2] = { -9, -9 };
        const bool lbAction = CountType(gOutput.mActions, 217, &lpLast, &liSize) == 1 && liSize == 8 && lpLast != nullptr;
        if (lbAction) std::memcpy(laiPair, lpLast, sizeof(laiPair));
        Check(lbAction && laiPair[0] == 2 && laiPair[1] == 3,
              "D3 status 4 by the player -> action 0xD9 {aggressor, victim}  @0x82383ED4");
    }

    // ===================== G12-D12: Destruct ======================================================
    {
        PaybackManager& lr = Fresh(PaybackManager::E_PAYBACK_VICTIM_STATE_ACTIVE, PaybackManager::E_PAYBACK_AGGRESSOR_STATE_TRIGGER_DT);
        lr.mTimerStatusInterface.mGameTimerStatus.miFrameCount = 77;
        lr.mTimerStatusInterface.mSimTimerStatus.mfBaseTimeStep = 0.5f;
        DirtyTrickEvent lEvent = {};
        lr.mDirtyTrickOutputQueue.AddEvent(lEvent);
        lr.mbPaybackAwarded = lr.mbDirtyTrickButtonDown = lr.mbDirtyTrickButtonWasDown = true;
        lr.mEvent.meAggressorActiveRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_1;
        lr.mEvent.meVictimActiveRaceCarIndex    = ::E_ACTIVE_RACE_CAR_INDEX_1;
        lr.mEvent.meDirtyTrickType              = static_cast<BrnNetwork::EPaybackType>(0);
        lr.mEvent.meDirtyTrickStatus            = static_cast<BrnNetwork::EDirtyTrickStatus>(0);
        gDebugDestructs = 0;
        lr.Destruct();
        Check(lr.mDebugComponent.mpPaybackManager == nullptr, "D12 debug back-pointer (+0x26C+0xC) = 0  @0x8236D130");
        Check(gDebugDestructs == 1, "D12 DebugComponent::Destruct is called on the component  @0x8236D134");
        Check(lr.mTimerStatusInterface.mGameTimerStatus.miFrameCount == 0
                  && lr.mTimerStatusInterface.mGameTimerStatus.mfTimeStepMultiplier == 1.0f
                  && lr.mTimerStatusInterface.mSimTimerStatus.mfBaseTimeStep == 0.0f,
              "D12 TimerStatusInterface::Clear  @0x8236D13C");
        Check(lr.mDirtyTrickOutputQueue.GetLength() == 0, "D12 +0x38 = 0 (the outbound queue)  @0x8236D148");
        Check(lr.mfPaybackAggTimer == -1.0f && lr.mfCountdownTimer == -1.0f, "D12 +0x24C/+0x248 = -1.0");
        Check(lr.mePaybackAggressorState == PaybackManager::E_PAYBACK_AGGRESSOR_STATE_IDLE
                  && lr.mePaybackVictimState == PaybackManager::E_PAYBACK_VICTIM_STATE_IDLE,
              "D12 +0x25C/+0x260 = 0");
        Check(lr.mePaybackAggressorRaceCarIndex == ::E_ACTIVE_RACE_CAR_INDEX_INVALID
                  && lr.mePaybackVictimRaceCarIndex == ::E_ACTIVE_RACE_CAR_INDEX_INVALID,
              "D12 +0x240/+0x244 = -1");
        Check(lr.meAwardedDirtyTrick == PaybackManager::KE_NO_DIRTY_TRICK
                  && lr.meActiveDirtyTrickType == PaybackManager::KE_NO_DIRTY_TRICK,
              "D12 +0x258/+0x254 = 3 (the X360 sentinel, not the PS3's 4)");
        Check(!lr.mbPaybackAwarded && !lr.mbDirtyTrickButtonDown && !lr.mbDirtyTrickButtonWasDown,
              "D12 +0x264/+0x265/+0x266 = 0");
        Check(lr.mEvent.meAggressorActiveRaceCarIndex == ::E_ACTIVE_RACE_CAR_INDEX_INVALID
                  && lr.mEvent.meVictimActiveRaceCarIndex == ::E_ACTIVE_RACE_CAR_INDEX_INVALID
                  && lr.mEvent.meDirtyTrickType == PaybackManager::KE_NO_DIRTY_TRICK
                  && static_cast<s32>(lr.mEvent.meDirtyTrickStatus) == 5,
              "D12 mEvent = {-1, -1, 3, 5}  @0x8236D184..0x8236D190");
        Check(lr.mfPaybackVictimTimer == 7.5f, "D12 +0x250 is NOT stored");
        Check(lr.mpGameStateModule == nullptr, "D12 +0x268 = 0  @0x8236D194");
    }

    Check(gAsserts == 0, "valid fixtures fire no assert");
    std::printf("FxGsPayback: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
