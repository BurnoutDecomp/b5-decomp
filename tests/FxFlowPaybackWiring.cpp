// FX-FLOW (crash parity 2026-09-24, NEW-PAYBACK-WIRING): the PRODUCTION
// GameStateModule::CopyInputDataToPaybackManager (GameStateModule_gTD_00.cpp) and the two
// PaybackManager setters it drives, SetTimerInterface / SetDirtyTrickButtonState
// (BrnPaybackManager.cpp), extracted by run_fxflow_payback_wiring.py and run against the REAL
// PaybackManager, the real pre-world input buffer types (BrnGameStateModuleIO.h), the real
// TimerStatusInterface / Random bodies and the extracted aggressor ChangeState / ResetState /
// OnRoundStart.
//
// Checked against the ARTIST asm of CopyInputDataToPaybackManager @0x8239AA78:
//   0x8239AA98..0x8239AB0C  GetTimerStatusInterface (0x8231CE28), then the 48-byte member-wise copy
//                           into PaybackManager +0x00 (lwz/stw +0, lfs/stfs +4/+8, lbz/stb +0xC,
//                           lwz/stw +0x10, lfs/stfs +0x14, twice)             == SetTimerInterface
//   0x8239AB10..0x8239AB58  GetControllerInput (0x823632F8) `lbz 0xC` (mbDirtyTrickPressed) ->
//                           +0x264; the old +0x264 -> +0x265; new && !old && +0x258 != 3 &&
//                           +0x25C == 4 -> ChangeState(5) @0x823919B0      == SetDirtyTrickButtonState
//   OnRoundStart @0x8236D290 reseeds from the copied game frame count (`lwz r8, 0(r3)` 0x8236D308).
#include "GameSource/GameState/BrnGameStateModuleIO.h"
#include "GameSource/GameState/PaybackManager/BrnPaybackManager.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcText, const char*, int) { ++gAsserts; std::fprintf(stderr, "assert: %s\n", lpcText); return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Log
{
    DebugPrint* gpDebugPrint = nullptr;   // the [payback] witnesses stay silent
    StrStreamBase& DebugPrint::operator<<(const char*) { return *this; }
}
namespace Message { u64 gxMessageFilterFlags = 0; }
}

// The two read accessors the copy goes through (bodies in BrnGameStateModuleIO.cpp, not under test:
// their read-lock asserts need the IOBuffer machinery, the member they return is the point).
namespace BrnGameState
{
namespace GameStateModuleIO
{
const TimerStatusInterface* PreWorldInputBuffer::GetTimerStatusInterface() const { return &mTimerStatusInterface; }
const ControllerInput*      PreWorldInputBuffer::GetControllerInput() const      { return &mControllerInput; }
}

// The module, as far as the code under test reaches it: the payback manager pointer (embedded by
// value on the console, a pointer on this build) and the output GUI queue ChangeState posts onto.
class GameStateModule
{
public:
    PaybackManager*                          mpPaybackManager = nullptr;
    CgsModule::VariableEventQueue<18432, 16> mOutputGuiEventQueue;

    CgsModule::VariableEventQueue<18432, 16>* GetOutputGuiEventQueue() { return &mOutputGuiEventQueue; }
    void CopyInputDataToPaybackManager(const GameStateModuleIO::PreWorldInputBuffer* lpPreWorldInputBuffer);
};
}

// The production bodies (or the runner's labelled empty stand-ins).
#include "payback_wiring.inc"

using BrnGameState::PaybackManager;
using BrnGameState::GameStateModule;
namespace GsmIO = BrnGameState::GameStateModuleIO;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

alignas(16) static unsigned char gaBufferStorage[sizeof(GsmIO::PreWorldInputBuffer)];
alignas(16) static unsigned char gaManagerStorage[sizeof(PaybackManager)];

int main()
{
    GsmIO::PreWorldInputBuffer& lrInput = *reinterpret_cast<GsmIO::PreWorldInputBuffer*>(gaBufferStorage);
    std::memset(gaBufferStorage, 0, sizeof(gaBufferStorage));
    PaybackManager& lrPayback = *reinterpret_cast<PaybackManager*>(gaManagerStorage);
    std::memset(gaManagerStorage, 0, sizeof(gaManagerStorage));

    static GameStateModule lModule;
    lModule.mOutputGuiEventQueue.Construct();
    lModule.mpPaybackManager   = &lrPayback;
    lrPayback.mpGameStateModule = &lModule;
    lrPayback.mRdmNumGenerator.Construct();
    lrPayback.meAwardedDirtyTrick = static_cast<BrnNetwork::EPaybackType>(3);

    // The frame's timer block, as BrnGameModule's publish writes it: entry 0 = game, entry 1 = sim.
    GsmIO::TimerStatusInterface::Entry& lrGame = lrInput.mTimerStatusInterface.maEntries[0];
    GsmIO::TimerStatusInterface::Entry& lrSim  = lrInput.mTimerStatusInterface.maEntries[1];
    lrGame.miWord00 = 1234; lrGame.mfValue04 = 1.0f / 60.0f; lrGame.mfValue08 = 2.0f; lrGame.mbFlag0C = 1;
    lrGame.miWord10 = 7;    lrGame.mfValue14 = 0.5f;
    lrSim.miWord00  = 99;   lrSim.mfValue04  = 1.0f / 30.0f; lrSim.mfValue08  = 0.5f; lrSim.mbFlag0C  = 0;
    lrSim.miWord10  = 3;    lrSim.mfValue14  = 0.25f;

    // ---- 1. the timer copy (SetTimerInterface) ------------------------------------------------
    lModule.CopyInputDataToPaybackManager(&lrInput);
    const CgsSystem::TimerStatus* lpGame = lrPayback.mTimerStatusInterface.GetGameTimerStatus();
    const CgsSystem::TimerStatus* lpSim  = lrPayback.mTimerStatusInterface.GetSimTimerStatus();
    Check(lpGame->GetFrameCount() == 1234 && lpGame->IsRunning() &&
          lpGame->GetTime().GetSeconds() == 7 && lpGame->GetTime().GetFraction() == 0.5f,
          "0x8239AAB0..: the game TimerStatus (frame, running, time) reaches PaybackManager +0x00");
    Check(std::fabs(lpGame->GetCurrentTimeStep() - (1.0f / 60.0f) * 2.0f) < 1.0e-7f,
          "the aggressor timer's step (base * multiplier, Update @0x8239AB78) reads the frame's game step");
    Check(lpSim->GetFrameCount() == 99 && !lpSim->IsRunning() && lpSim->GetTime().GetSeconds() == 3 &&
          std::fabs(lpSim->GetCurrentTimeStep() - (1.0f / 30.0f) * 0.5f) < 1.0e-7f,
          "0x8239AAE0..: the sim TimerStatus reaches PaybackManager +0x18");

    // ---- 2. the reseed reads the copied frame count (OnRoundStart @0x8236D308) --------------------
    {
        lrPayback.OnRoundStart();
        CgsNumeric::Random lReference;
        lReference.Construct();
        lReference.SetSeed(static_cast<u64>(static_cast<s64>(1234)));
        bool lbSame = true;
        for (s32 liDraw = 0; liDraw < 4; ++liDraw)
            lbSame = lbSame && (lrPayback.mRdmNumGenerator.RandomUInt() == lReference.RandomUInt());
        Check(lbSame, "OnRoundStart reseeds the award RNG from the copied game frame count (1234), not 0");
    }

    // ---- 3. the dirty-trick button (SetDirtyTrickButtonState) -------------------------------------
    // Holding a trick (aggressor state 4, awarded trick 1): the press edge fires ChangeState(5).
    lrPayback.mePaybackAggressorState = static_cast<PaybackManager::EPaybackAggressorState>(4);
    lrPayback.meAwardedDirtyTrick     = static_cast<BrnNetwork::EPaybackType>(1);
    lrPayback.mfPaybackAggTimer       = 5.0f;
    const s32 liGuiBefore = lModule.mOutputGuiEventQueue.GetLength();
    lrInput.mControllerInput.mbDirtyTrickPressed = true;
    lModule.CopyInputDataToPaybackManager(&lrInput);
    Check(lrPayback.mbDirtyTrickButtonDown && !lrPayback.mbDirtyTrickButtonWasDown,
          "0x8239AB20/24: +0x264 = the press, +0x265 = the previous state");
    Check(lrPayback.mePaybackAggressorState == static_cast<PaybackManager::EPaybackAggressorState>(5) &&
          lrPayback.mfPaybackAggTimer == -1.0f &&
          lModule.mOutputGuiEventQueue.GetLength() == liGuiBefore + 1,
          "press edge in state 4 with a trick -> ChangeState(5) (timer -1.0, GUI 0xB0 posted)");

    // Held: no second edge.
    lrPayback.mePaybackAggressorState = static_cast<PaybackManager::EPaybackAggressorState>(4);
    lModule.CopyInputDataToPaybackManager(&lrInput);
    Check(lrPayback.mbDirtyTrickButtonWasDown &&
          lrPayback.mePaybackAggressorState == static_cast<PaybackManager::EPaybackAggressorState>(4),
          "holding the button is not an edge (old +0x264 set -> no ChangeState)");

    // Released.
    lrInput.mControllerInput.mbDirtyTrickPressed = false;
    lModule.CopyInputDataToPaybackManager(&lrInput);
    Check(!lrPayback.mbDirtyTrickButtonDown && lrPayback.mbDirtyTrickButtonWasDown,
          "releasing: +0x264 = 0, +0x265 = 1");

    // An edge with no awarded trick (+0x258 == 3) does nothing.
    lrInput.mControllerInput.mbDirtyTrickPressed = true;
    lrPayback.meAwardedDirtyTrick = static_cast<BrnNetwork::EPaybackType>(3);
    lModule.CopyInputDataToPaybackManager(&lrInput);
    Check(lrPayback.mePaybackAggressorState == static_cast<PaybackManager::EPaybackAggressorState>(4),
          "press edge with meAwardedDirtyTrick == 3 (none) -> no ChangeState");

    // An edge in another aggressor state does nothing.
    lrInput.mControllerInput.mbDirtyTrickPressed = false;
    lModule.CopyInputDataToPaybackManager(&lrInput);
    lrInput.mControllerInput.mbDirtyTrickPressed = true;
    lrPayback.meAwardedDirtyTrick     = static_cast<BrnNetwork::EPaybackType>(1);
    lrPayback.mePaybackAggressorState = static_cast<PaybackManager::EPaybackAggressorState>(3);
    lModule.CopyInputDataToPaybackManager(&lrInput);
    Check(lrPayback.mePaybackAggressorState == static_cast<PaybackManager::EPaybackAggressorState>(3),
          "press edge in aggressor state 3 -> no ChangeState (the gate is +0x25C == 4)");

    Check(gAsserts == 0, "no assert on the way");

    std::printf("FxFlowPaybackWiring: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
