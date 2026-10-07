// Real replay pre-sim input construction on reused IO-stack storage.
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <initializer_list>
#include "GameSource/Replays/BrnReplayModuleIO.h"

static unsigned checks, failures, asserts;
static void Check(bool value, const char* label)
{
    ++checks;
    if (!value) { ++failures; std::printf("FAIL %s\n", label); }
}
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++asserts; return 0; }
void* EndAssert() { return nullptr; }
} }
namespace CgsDev { namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { u64 gxMessageFilterFlags = 0; } }
#include "replay_presim_io.inc"

using BrnReplays::ReplayIO::InputBuffer_PreSim;
static void CheckTimer(const CgsSystem::TimerStatus& timer)
{
    Check(timer.GetFrameCount()==0, "reused timer frame is cleared");
    Check(timer.GetBaseTimeStep()==0.0f && timer.GetTimeStepMultiplier()==1.0f,
          "timer reset retains the original identity multiplier");
    Check(!timer.IsRunning(), "reused timer is stopped");
    Check(timer.GetTime().GetSeconds()==0 && timer.GetTime().GetFraction()==0.0f,
          "both timer time words are reset");
}
int main()
{
    struct Guarded {
        u64 before[2];
        InputBuffer_PreSim input;
        u64 after[2];
    } storage;
    for (unsigned char poison : {0xA5, 0x5A})
    {
        std::memset(&storage, poison, sizeof(storage));
        unsigned char pad[sizeof(storage.input.mPadInput)];
        std::memcpy(pad, &storage.input.mPadInput, sizeof(pad));
        for (unsigned reuse=0;reuse<2;++reuse)
        {
            storage.input.Construct();
            Check(storage.before[0]==storage.before[1] && storage.after[0]==storage.before[0]
                  && storage.after[1]==storage.before[0], "constructor stays inside its allocation");
            Check(!storage.input.IsBufferLocked(), "constructor removes stale IO locks");
            auto& queue=storage.input.mGameActionQueue;
            Check(queue.mbIsConstructed && queue.GetLength()==0,
                  "real game-action queue is constructed with no stale events");
            Check(queue.miBufferWritePos==queue.miFirstEventOffset &&
                  (reinterpret_cast<uintptr_t>(queue.macData)+queue.miFirstEventOffset)%16==0,
                  "queue cursor follows real host record alignment");
            Check(std::memcmp(pad, &storage.input.mPadInput, sizeof(pad))==0,
                  "unpublished pad-input payload survives selective construction");
            storage.input.LockForRead();
            const auto* timers=storage.input.GetTimerStatusInterface();
            CheckTimer(*timers->GetGameTimerStatus());
            CheckTimer(*timers->GetSimTimerStatus());
            storage.input.UnlockForRead();
            const u32 payload=0x12345678;
            Check(queue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&payload),7,sizeof(payload)),
                  "constructed real queue accepts a game-action record");
            storage.input.LockForWrite();
        }
    }
    Check(asserts==0, "constructor and reuse satisfy real IO/queue assertions");
    std::printf("ReplayPreSimIO: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
