// Actual original driver with production IO stack, replay input, timer snapshot
// and serialiser bodies. ReplayModule's producer is observed at its typed boundary.
#include <cstdio>
#include <cstring>
#include <vector>
#include "SharedClasses/BrnSharedConstants.h"
#include "GameSource/Replays/BrnReplayModuleIO.h"
#include "GameSource/Replays/Serialisers/BrnReplayGameModuleSerialiser.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"
#include "GameSource/GameState/BrnGameStateModuleIO.h"
#include "GameSource/Game/BrnGlobalCpuMonitors.h"
#include "GameShared/GameClasses/Module/CgsModuleIOHelper.h"
#include "GameShared/GameClasses/Module/CgsModuleUtils.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"

static unsigned checks, failures, assertions;
static std::vector<unsigned> trace;
static void Check(bool ok, const char* message)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL %s\n", message); }
}
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* message, const char* file, int line)
{ ++assertions; std::printf("ASSERT %s (%s:%d)\n", message, file, line); return 0; }
void* EndAssert() { return nullptr; }
} namespace PerfMonCpu {
void StartMonitor(s32 id) { trace.push_back(100 + id); }
void StopMonitor(s32 id) { trace.push_back(200 + id); }
} namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { u64 gxMessageFilterFlags = 0; } }

using BrnReplays::BaseSerialiser;
using ReplayInput = BrnReplays::ReplayIO::InputBuffer_PreSim;
using ReplayOutput = BrnReplays::ReplayIO::OutputBuffer_PreSim;
static CgsSystem::TimerStatusInterface* expectedTimers;
static BrnGameState::GameStateModuleIO::OutputBuffer* expectedGameState;
static ReplayOutput* expectedReplayOutput;
static BrnReplays::GameModuleSerialiser* expectedSerialiser;
static BrnUpdateSet expectedUpdateSet;
static const u64 actionToken = UINT64_C(0x1234567887654321);
struct ObservedReplayModule
{
    unsigned calls = 0;
    void Update_PreSim(const ReplayInput* input, ReplayOutput* output, BrnUpdateSet set)
    {
        trace.push_back(10); ++calls;
        Check(set == expectedUpdateSet && output == expectedReplayOutput,
              "actual output and original update set reach the pre-simulation producer");
        Check(!input->IsBufferLocked() && !expectedGameState->IsBufferLocked(),
              "driver releases both bridge locks before original producer call");
        Check(!expectedSerialiser->mbLocked && !expectedSerialiser->mbDataReady
              && !expectedSerialiser->mbDataRestored,
              "producer runs before original serialisation and ready/restored publication");
        input->LockForRead();
        Check(std::memcmp(input->GetTimerStatusInterface(), expectedTimers, 48) == 0,
              "bridge forwards the complete original game/sim timer snapshot");
        const CgsModule::Event* event = nullptr; s32 size = 0;
        const s32 type = input->mGameActionQueue.GetFirstEvent(&event, &size);
        Check(input->mGameActionQueue.GetLength() == 1 && type == 5432 && size == 8
              && *reinterpret_cast<const u64*>(event) == actionToken,
              "bridge appends actual game actions once into the newly constructed input");
        input->UnlockForRead();
    }
};
namespace BrnGame {
class BrnGameModule
{
public:
    BrnCpuMonitors mCpuMonitors = {};
    CgsSystem::TimerStatusInterface mTimerStatusInterface;
    BrnReplays::GameModuleSerialiser mGameModuleSerialiser;
    ObservedReplayModule mReplayModule;
    void DoUpdate_ReplaysPreSim(CgsModule::IOBufferStack*, CgsModule::IOBufferStack*,
        const BrnGameState::GameStateModuleIO::OutputBuffer*, ReplayOutput*, BrnUpdateSet);
};
}
#include "game_replay_presim_driver.inc"

int main()
{
    using GSOutput = BrnGameState::GameStateModuleIO::OutputBuffer;
    alignas(16) static unsigned char gsStorage[sizeof(GSOutput)];
    auto* gs = reinterpret_cast<GSOutput*>(gsStorage);
    gs->CgsModule::IOBuffer::Construct();
    auto* actions = reinterpret_cast<BrnGameState::GameStateModuleIO::GameActionQueue*>(gs->mGameActionQueueStorage);
    actions->Construct(); actions->AddEvent(reinterpret_cast<const CgsModule::Event*>(&actionToken), 5432, 8);
    ReplayOutput output;
    output.CgsModule::IOBuffer::Construct();
    BrnGame::BrnGameModule game;
    game.mCpuMonitors.miUT_Replay = 3;
    game.mTimerStatusInterface.Clear();
    auto* gameTime = game.mTimerStatusInterface.GetGameTimerStatus();
    auto* simTime = game.mTimerStatusInterface.GetSimTimerStatus();
    gameTime->miFrameCount = 101; gameTime->mfBaseTimeStep = 0.125f;
    gameTime->mfTimeStepMultiplier = 0.25f; gameTime->mbRunning = true;
    gameTime->mTime.SetSeconds(91); gameTime->mTime.SetFraction(0.25f);
    simTime->miFrameCount = 202; simTime->mfBaseTimeStep = 0.02f;
    simTime->mfTimeStepMultiplier = 0.5f; simTime->mbRunning = false;
    simTime->mTime.SetSeconds(23); simTime->mTime.SetFraction(0.75f);
    const CgsSystem::TimerStatus expectedGame = *gameTime;
    const CgsSystem::TimerStatus expectedSim = *simTime;
    game.mGameModuleSerialiser.Construct();
    unsigned char bytes[1024]; std::memset(bytes, 0x5A, sizeof(bytes));
    game.mGameModuleSerialiser.SetBuffer(bytes);
    game.mGameModuleSerialiser.Lock();
    game.mGameModuleSerialiser.SetMode(BaseSerialiser::E_MODE_RECORDING);
    game.mGameModuleSerialiser.Unlock();
    alignas(16) unsigned char stackStorage[32768]; std::memset(stackStorage, 0xA5, sizeof(stackStorage));
    CgsModule::IOBufferStack inputStack, outputStack;
    inputStack.Construct("input"); outputStack.Construct("output");
    inputStack.Prepare(stackStorage, sizeof(stackStorage), 16);
    expectedTimers = &game.mTimerStatusInterface; expectedGameState = gs;
    expectedReplayOutput = &output; expectedSerialiser = &game.mGameModuleSerialiser;
    expectedUpdateSet = static_cast<BrnUpdateSet>(0x128);
    auto run = [&]() {
        trace.clear(); game.mGameModuleSerialiser.SetDataReady(false);
        game.mGameModuleSerialiser.SetDataRestored(false);
        game.DoUpdate_ReplaysPreSim(&inputStack, &outputStack, gs, &output, expectedUpdateSet);
        Check(trace == std::vector<unsigned>({103, 10, 203}), "original monitor/producer/serialization order");
        Check(inputStack.muNumAllocated == 0 && inputStack.muAllocated == 0,
              "original IOHelper destroys its input on the same stack after monitor stop");
        Check(!game.mGameModuleSerialiser.mbLocked && game.mGameModuleSerialiser.mbDataReady
              && game.mGameModuleSerialiser.mbDataRestored, "real serialiser is unlocked and publishes both original flags");
        Check(!gs->IsBufferLocked(), "no source lock remains after the driver");
        Check(std::memcmp(gameTime, &expectedGame, 24) == 0, "SIM serialization never overwrites game timer status");
    };
    run();
    Check(game.mGameModuleSerialiser.GetBufferUsed() == 24 && !std::memcmp(bytes, &expectedSim, 24)
          && bytes[24] == 0x5A, "record exactly the SIM status and no adjacent bytes");
    simTime->miFrameCount = 303;
    const CgsSystem::TimerStatus secondSim = *simTime;
    run();
    Check(game.mGameModuleSerialiser.GetBufferUsed() == 48 && !std::memcmp(bytes + 24, &secondSim, 24)
          && bytes[48] == 0x5A, "second update owns a fresh input and exact next SIM payload");
    game.mGameModuleSerialiser.Lock();
    game.mGameModuleSerialiser.SetMode(BaseSerialiser::E_MODE_PLAYING);
    game.mGameModuleSerialiser.Unlock();
    simTime->miFrameCount = -999;
    run();
    Check(game.mGameModuleSerialiser.GetBufferRead() == 24 && !std::memcmp(simTime, &expectedSim, 24),
          "actual playback restores the SIM status after original producer execution");
    Check(game.mReplayModule.calls == 3 && assertions == 0, "exactly one producer call per driver with real lock discipline");
    std::printf("GameReplayPreSimDriver: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
