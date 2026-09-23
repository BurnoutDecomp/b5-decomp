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
//   HandleWaitingToAwardPayback @0x823978B0 (arm [2] = 0x8239AC54, vehicle output = r21)
//                        the same snapshot (0x823978FC) and read (0x8239790C); bne -> return; else
//                        -1.0 -> +0x24C, 3 -> +0x25C, 0 -> +0x266, AddEvent(outGui, &1, 0xB0, 4)  (G12-D5)
//   HandleAwardingPayback @0x82397970 (arm [3] = 0x8239AC64: r4 = out, r5 = vehicle output, r6 = mode)
//                        !(+0x24C < 1.0 (flt_82001C98)) && !+0x266 (fcmpu/blt: NaN awards) ->
//                        +0x266 = 1; +0x258 = hi32(OLD seed) % 3, seed = seed * 0x5851F42D4C957F2D + 1;
//                        gui+4 NewDirtyTrick {player, +0x244, +0x258} (0x82397A64);
//                        SendNetworkDirtyTrickMessage(player, +0x244, +0x258, 1) (0x82397A8C). Then,
//                        NOT as an else: crashing[player] -> action 0xD3 size 1 (0x82397AE0), +0x258 = 3,
//                        +0x244 = -1, -1.0 -> +0x24C, 0 -> +0x25C / +0x266, GUI 0xB0 {1}            (G12-D6)
//   Update tail          GetGameStateToNetworkInterface (0x8231D800) -> DirtyTrickEvent Append of +0x30
//                        (0x8239ADEC), then +0x38 = 0: the award's message reaches the network queue.
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

// The console's LCG (0x5851F42D4C957F2D, every inlined site) as an independent oracle for the draw.
static const u64 KU_CONSOLE_LCG = 0x5851F42D4C957F2Dull;
static u64 NextSeed(u64 luSeed) { return luSeed * KU_CONSOLE_LCG + 1u; }
static s32 AwardFor(u64 luSeed) { return static_cast<s32>(static_cast<u32>(luSeed >> 32) % 3u); }

typedef BrnNetwork::BrnNetworkModuleIO::GameStateToNetworkInterface::DirtyTrickQueue NetworkQueue;
static const NetworkQueue& NetworkOut() { return *gOutput.mNetwork.GetDirtyTrickQueue(); }
static bool IsMessage(const BrnNetwork::BrnNetworkModuleIO::DirtyTrickEvent& lr, s32 liAggressor, s32 liVictim,
                      s32 liType, s32 liStatus)
{
    return static_cast<s32>(lr.meAggressorActiveRaceCarIndex) == liAggressor
        && static_cast<s32>(lr.meVictimActiveRaceCarIndex) == liVictim
        && static_cast<s32>(lr.meDirtyTrickType) == liType
        && static_cast<s32>(lr.meDirtyTrickStatus) == liStatus;
}
static bool IsNewDirtyTrick(s32 liIndex, s32 liAggressor, s32 liVictim, s32 liType)
{
    const GameStateModuleIO::GameStateToGuiInterface::NewDirtyTrickQueue& lrQueue = gOutput.mGui.mNewDirtyTrickQueue;
    return liIndex < lrQueue.GetLength()
        && static_cast<s32>(lrQueue.GetEvent(liIndex).meAggressorActiveRaceCarIndex) == liAggressor
        && static_cast<s32>(lrQueue.GetEvent(liIndex).meVictimActiveRaceCarIndex) == liVictim
        && static_cast<s32>(lrQueue.GetEvent(liIndex).meTrickType) == liType;
}
static bool RingUntouched(const PaybackManager& lr)
{
    for (u32 luSlot = 0; luSlot < 8; ++luSlot)
        if (lr.mRdmNumGenerator.mauIntegerBuffer[luSlot] != 0u)
            return false;
    return lr.mRdmNumGenerator.muOldestBufferIndex == 0u;
}

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

    // ===================== G12-D5: HandleWaitingToAwardPayback through arm [2] =====================
    {
        PaybackManager& lr = Fresh(PM::E_PAYBACK_VICTIM_STATE_IDLE, PM::E_PAYBACK_AGGRESSOR_STATE_AWARD_DT);
        Tick(lr);   // nobody crashing: the player's crash is over
        Check(gSetFromCalls == 1 && gpSetFromArg == gpVehicleOutput,
              "D5 the arm snapshots the vehicle output Update was handed  @0x823978FC");
        Check(lr.mePaybackAggressorState == PM::E_PAYBACK_AGGRESSOR_STATE_READY_TO_TRIGGER,
              "D5 player not crashing -> aggressor state 3 (`li r10, 3`)  @0x82397934");
        Check(lr.mfPaybackAggTimer == -1.0f, "D5 ...+0x24C = -1.0 (flt_820037C8)  @0x82397930");
        Check(!lr.mbPaybackAwarded, "D5 ...+0x266 = 0  @0x82397938");
        Check(OneShowRecord(), "D5 ...and one GUI 176 (0xB0) record, 4 bytes, value 1  @0x82397950");
    }
    {
        PaybackManager& lr = Fresh(PM::E_PAYBACK_VICTIM_STATE_IDLE, PM::E_PAYBACK_AGGRESSOR_STATE_AWARD_DT);
        gabCrashing[2] = true;   // still crashing
        Tick(lr);
        Check(lr.mePaybackAggressorState == PM::E_PAYBACK_AGGRESSOR_STATE_AWARD_DT && lr.mbPaybackAwarded
                  && lr.mfPaybackAggTimer == 0.0f && CountType(gModule.mOutputGuiEventQueue, 176, nullptr, nullptr) == 0,
              "D5 player still crashing -> stays in state 2, nothing stored or posted (bne)  @0x82397914");
    }
    {
        PaybackManager& lr = Fresh(PM::E_PAYBACK_VICTIM_STATE_IDLE, PM::E_PAYBACK_AGGRESSOR_STATE_AWARD_DT);
        gabCrashing[5] = true;   // another car is crashing; the player is not
        Tick(lr);
        Check(lr.mePaybackAggressorState == PM::E_PAYBACK_AGGRESSOR_STATE_READY_TO_TRIGGER,
              "D5 the byte is the PLAYER's slot (slot 5 crashing, player 2 clear -> state 3)");
    }
    {
        PaybackManager& lr = Fresh(PM::E_PAYBACK_VICTIM_STATE_IDLE, PM::E_PAYBACK_AGGRESSOR_STATE_WAIT_AWARD_PAYBACK);
        gabCrashing[2] = true;
        Tick(lr);                                  // 1 -> 2 (the crash starts)
        const bool lbTwo = lr.mePaybackAggressorState == PM::E_PAYBACK_AGGRESSOR_STATE_AWARD_DT;
        Tick(lr);                                  // still crashing: stays 2
        const bool lbStillTwo = lr.mePaybackAggressorState == PM::E_PAYBACK_AGGRESSOR_STATE_AWARD_DT;
        gabCrashing[2] = false;
        Tick(lr);                                  // the crash is over: 2 -> 3
        Check(lbTwo && lbStillTwo && lr.mePaybackAggressorState == PM::E_PAYBACK_AGGRESSOR_STATE_READY_TO_TRIGGER
                  && lr.mfPaybackAggTimer == -1.0f,
              "D1+D5 chain: crash starts 1 -> 2, holds while crashing, ends 2 -> 3");
    }

    // ===================== G12-D6: HandleAwardingPayback through arm [3] ===========================
    const f32 lfNaN = std::numeric_limits<f32>::quiet_NaN();
    const u64 luSeedA = 0x0123456789ABCDEFull;   // hi32 0x01234567 % 3 == 1
    const u64 luSeedB = 0xFFFFFFFE00000000ull;   // hi32 0xFFFFFFFE % 3 == 2
    {
        PaybackManager& lr = Fresh(PM::E_PAYBACK_VICTIM_STATE_IDLE, PM::E_PAYBACK_AGGRESSOR_STATE_READY_TO_TRIGGER);
        lr.mbPaybackAwarded = false;
        lr.mfPaybackAggTimer = 0.75f;   // Update's advance makes it 1.0: the gate opens (1.0 is not < 1.0)
        lr.mRdmNumGenerator.muSeed = luSeedA;
        Tick(lr);
        Check(gSetFromCalls == 1 && gpSetFromArg == gpVehicleOutput,
              "D6 the arm snapshots the vehicle output Update was handed  @0x823979BC");
        Check(lr.mbPaybackAwarded, "D6 timer 1.0, not yet awarded -> +0x266 = 1  @0x823979E8");
        Check(static_cast<s32>(lr.meAwardedDirtyTrick) == AwardFor(luSeedA) && AwardFor(luSeedA) == 1,
              "D6 +0x258 = hi32(OLD seed) % 3 (mulhwu 0xAAAAAAAB)  @0x82397A3C");
        Check(lr.mRdmNumGenerator.muSeed == NextSeed(luSeedA) && RingUntouched(lr),
              "D6 ...ONE LCG step (mulld 0x5851F42D4C957F2D, +1), the float ring untouched  @0x82397A1C");
        Check(gOutput.mGui.mNewDirtyTrickQueue.GetLength() == 1 && IsNewDirtyTrick(0, 2, 6, 1),
              "D6 gui+4 NewDirtyTrick record {player, +0x244, +0x258}  @0x82397A64");
        Check(NetworkOut().GetLength() == 1 && IsMessage(NetworkOut().GetEvent(0), 2, 6, 1, 1)
                  && lr.mDirtyTrickOutputQueue.GetLength() == 0,
              "D6 network message {player, victim, trick, AVAILABLE 1} reaches the interface via Update's Append  @0x82397A8C/0x8239ADEC");
        Check(lr.mePaybackAggressorState == PM::E_PAYBACK_AGGRESSOR_STATE_READY_TO_TRIGGER && lr.mfPaybackAggTimer == 1.0f
                  && CountType(gModule.mOutputGuiEventQueue, 176, nullptr, nullptr) == 0
                  && CountType(gOutput.mActions, 211, nullptr, nullptr) == 0,
              "D6 the award does not change state or timer and posts no 176 / 0xD3");
    }
    {
        PaybackManager& lr = Fresh(PM::E_PAYBACK_VICTIM_STATE_IDLE, PM::E_PAYBACK_AGGRESSOR_STATE_READY_TO_TRIGGER);
        lr.mbPaybackAwarded = false;
        lr.mfPaybackAggTimer = 0.5f;    // -> 0.75: still < 1.0
        lr.mRdmNumGenerator.muSeed = luSeedA;
        Tick(lr);
        Check(!lr.mbPaybackAwarded && lr.mRdmNumGenerator.muSeed == luSeedA && gOutput.mGui.mNewDirtyTrickQueue.GetLength() == 0
                  && NetworkOut().GetLength() == 0 && lr.mePaybackAggressorState == PM::E_PAYBACK_AGGRESSOR_STATE_READY_TO_TRIGGER,
              "D6 timer 0.75 < 1.0 -> no award, no draw (blt)  @0x823979D4");
    }
    {
        PaybackManager& lr = Fresh(PM::E_PAYBACK_VICTIM_STATE_IDLE, PM::E_PAYBACK_AGGRESSOR_STATE_READY_TO_TRIGGER);
        lr.mbPaybackAwarded = true;     // already awarded
        lr.mfPaybackAggTimer = 2.0f;
        lr.mRdmNumGenerator.muSeed = luSeedA;
        Tick(lr);
        Check(lr.mRdmNumGenerator.muSeed == luSeedA && lr.meAwardedDirtyTrick == static_cast<BrnNetwork::EPaybackType>(2)
                  && gOutput.mGui.mNewDirtyTrickQueue.GetLength() == 0 && NetworkOut().GetLength() == 0,
              "D6 already awarded (+0x266 != 0) -> no second award (bne)  @0x823979E0");
    }
    {
        PaybackManager& lr = Fresh(PM::E_PAYBACK_VICTIM_STATE_IDLE, PM::E_PAYBACK_AGGRESSOR_STATE_READY_TO_TRIGGER);
        lr.mbPaybackAwarded = false;
        lr.mfPaybackAggTimer = lfNaN;
        lr.mRdmNumGenerator.muSeed = luSeedB;
        Tick(lr);
        Check(lr.mbPaybackAwarded && gOutput.mGui.mNewDirtyTrickQueue.GetLength() == 1,
              "D6 a NaN timer awards (fcmpu unordered: blt not taken)  @0x823979D4");
        Check(static_cast<s32>(lr.meAwardedDirtyTrick) == AwardFor(luSeedB) && AwardFor(luSeedB) == 2,
              "D6 a second seed draws trick 2 (0xFFFFFFFE % 3)");
    }
    {
        PaybackManager& lr = Fresh(PM::E_PAYBACK_VICTIM_STATE_IDLE, PM::E_PAYBACK_AGGRESSOR_STATE_READY_TO_TRIGGER);
        lr.mbPaybackAwarded = false;
        lr.mfPaybackAggTimer = 0.25f;   // gate closed
        lr.mRdmNumGenerator.muSeed = luSeedA;
        gabCrashing[2] = true;          // the player crashes again
        Tick(lr);
        s32 liSize = -1;
        Check(CountType(gOutput.mActions, 211, nullptr, &liSize) == 1 && liSize == 1,
              "D6 player crashing -> action 0xD3 (PaybackLostAction), 1 byte  @0x82397AE0");
        Check(lr.meAwardedDirtyTrick == PM::KE_NO_DIRTY_TRICK && lr.mePaybackVictimRaceCarIndex == ::E_ACTIVE_RACE_CAR_INDEX_INVALID,
              "D6 ...+0x258 = 3, +0x244 = -1  @0x82397B04/0x82397B08");
        Check(lr.mePaybackAggressorState == PM::E_PAYBACK_AGGRESSOR_STATE_IDLE && lr.mfPaybackAggTimer == -1.0f
                  && !lr.mbPaybackAwarded && OneShowRecord(),
              "D6 ...ChangeState(0): +0x24C = -1.0, +0x25C = 0, +0x266 = 0, GUI 176 {1}  @0x82397B00..0x82397B24");
        Check(lr.mRdmNumGenerator.muSeed == luSeedA && gOutput.mGui.mNewDirtyTrickQueue.GetLength() == 0 && NetworkOut().GetLength() == 0,
              "D6 ...with the gate closed nothing is awarded");
    }
    {
        PaybackManager& lr = Fresh(PM::E_PAYBACK_VICTIM_STATE_IDLE, PM::E_PAYBACK_AGGRESSOR_STATE_READY_TO_TRIGGER);
        lr.mbPaybackAwarded = false;
        lr.mfPaybackAggTimer = 0.75f;   // gate opens this frame ...
        lr.mRdmNumGenerator.muSeed = luSeedA;
        gabCrashing[2] = true;          // ... and the player is crashing: both blocks run
        Tick(lr);
        Check(gOutput.mGui.mNewDirtyTrickQueue.GetLength() == 1 && IsNewDirtyTrick(0, 2, 6, 1)
                  && NetworkOut().GetLength() == 1 && IsMessage(NetworkOut().GetEvent(0), 2, 6, 1, 1),
              "D6 award and loss in one frame: the award's record + message go out first (not an else)  @0x82397A90");
        Check(lr.mePaybackAggressorState == PM::E_PAYBACK_AGGRESSOR_STATE_IDLE && lr.meAwardedDirtyTrick == PM::KE_NO_DIRTY_TRICK
                  && !lr.mbPaybackAwarded && CountType(gOutput.mActions, 211, nullptr, nullptr) == 1,
              "D6 ...then the loss resets it (state 0, trick 3, +0x266 = 0, one 0xD3)");
    }
    {
        PaybackManager& lr = Fresh(PM::E_PAYBACK_VICTIM_STATE_IDLE, PM::E_PAYBACK_AGGRESSOR_STATE_READY_TO_TRIGGER);
        lr.mbPaybackAwarded = false;
        lr.mfPaybackAggTimer = 0.75f;
        lr.mRdmNumGenerator.muSeed = luSeedB;
        Tick(lr);
        Check(static_cast<s32>(lr.meAwardedDirtyTrick) == 2 && IsNewDirtyTrick(0, 2, 6, 2)
                  && NetworkOut().GetLength() == 1 && IsMessage(NetworkOut().GetEvent(0), 2, 6, 2, 1),
              "D6 the drawn trick is what the record and the message carry (seed B -> 2)");
    }
    {
        // The whole aggressor chain through Update: state 2 ends its crash -> 3 (timer -1.0); then
        // 0.0, 0.25, 0.5, 0.75 -> no award; 1.0 -> award, on exactly the fifth tick in state 3.
        PaybackManager& lr = Fresh(PM::E_PAYBACK_VICTIM_STATE_IDLE, PM::E_PAYBACK_AGGRESSOR_STATE_AWARD_DT);
        lr.mRdmNumGenerator.muSeed = luSeedA;
        Tick(lr);   // 2 -> 3
        bool lbEarly = false;
        for (int liTick = 0; liTick < 4; ++liTick)
        {
            Tick(lr);
            lbEarly = lbEarly || lr.mbPaybackAwarded;
        }
        const bool lbNotYet = !lbEarly && lr.mfPaybackAggTimer == 0.75f;
        Tick(lr);
        Check(lbNotYet && lr.mbPaybackAwarded && lr.mfPaybackAggTimer == 1.0f && static_cast<s32>(lr.meAwardedDirtyTrick) == 1,
              "D5+D6 chain: 3 entered at -1.0, award exactly when Update's timer reaches 1.0");
    }

    Check(gAsserts == 0, "valid fixtures fire no assert");
    std::printf("FxGs2Payback: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
