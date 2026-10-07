#include <cstdio>
#include <cstring>
#include <type_traits>
#include "GameSource/Replays/Serialisers/BrnReplayGameModuleSerialiser.h"
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"

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
#include "replay_game_serialiser.inc"

using BrnReplays::BaseSerialiser;
using BrnReplays::GameModuleSerialiser;
static_assert(std::is_base_of<BaseSerialiser, GameModuleSerialiser>::value,
              "the game channel uses the complete canonical base");
static_assert(sizeof(GameModuleSerialiser)==sizeof(BaseSerialiser),
              "the game channel adds no instance payload");
static_assert(std::is_same<decltype(&GameModuleSerialiser::Construct),
                          void (GameModuleSerialiser::*)()>::value,
              "Construct retains its original void declaration");
int main()
{
    struct Guarded {
        u64 before[2];
        GameModuleSerialiser serialiser;
        u64 after[2];
    } storage;
    std::memset(&storage, 0xA5, sizeof(storage));
    auto& serialiser=storage.serialiser;
    serialiser.Construct();
    Check(serialiser.GetId()==5 && serialiser.GetContext()==BrnReplays::E_CONTEXT_NORMAL,
          "original game channel id and context");
    Check(serialiser.GetBufferSize()==1024 && serialiser.GetStaticBufferSize()==1024,
          "original stream and static buffer capacities");
    Check(!std::strcmp(serialiser.GetName(),"GameModule") && !serialiser.SkipModuleSerialise(),
          "original name and module-serialise setting");
    Check(serialiser.GetMode()==BaseSerialiser::E_MODE_IDLE && !serialiser.mbLocked &&
          serialiser.GetBufferUsed()==0 && serialiser.GetBufferRead()==0 &&
          serialiser.mpBuffer==nullptr && serialiser.GetStaticBuffer()==nullptr,
          "real base constructor resets its stream state");
    Check(storage.before[0]==storage.before[1] && storage.after[0]==storage.before[0] &&
          storage.after[1]==storage.before[0], "real base construction stays inside the complete object");

    u8 bytes[1024]; std::memset(bytes,0x5A,sizeof(bytes));
    serialiser.SetBuffer(bytes);
    CgsSystem::TimerStatus sent, received;
    std::memset(&sent,0,sizeof(sent));
    std::memset(&received,0,sizeof(received));
    sent.miFrameCount=1234;
    sent.mfBaseTimeStep=0.02f;
    sent.mfTimeStepMultiplier=0.5f;
    sent.mbRunning=true;
    sent.mTime.SetSeconds(77);
    sent.mTime.SetFraction(0.25f);
    Check(sizeof(sent)==24, "driver SIM TimerStatus payload is exactly 24 bytes");
    serialiser.Lock();
    serialiser.SetMode(BaseSerialiser::E_MODE_RECORDING);
    Check(serialiser.Serialise(&sent,sizeof(sent))==sizeof(sent), "real inherited recording path accepts SIM timer bytes");
    Check(serialiser.GetBufferUsed()==24 && !std::memcmp(bytes,&sent,24) && bytes[24]==0x5A,
          "recording stores exactly the original timer payload");
    serialiser.SetDataReady(true);
    serialiser.SetDataRestored(true);
    Check(serialiser.mbDataReady && serialiser.mbDataRestored,
          "driver flags are the real base ready/restored members");
    serialiser.SetMode(BaseSerialiser::E_MODE_PLAYING);
    Check(serialiser.Serialise(&received,sizeof(received))==24 && !std::memcmp(&sent,&received,24),
          "real inherited playback restores every SIM timer byte");
    Check(serialiser.GetBufferRead()==24 && received.GetFrameCount()==1234 &&
          received.GetCurrentTimeStep()==0.01f && received.GetTime().GetSeconds()==77,
          "playback restores typed timer state and advances its cursor");
    serialiser.SetMode(BaseSerialiser::E_MODE_IDLE);
    std::memset(&received,0x5A,sizeof(received));
    Check(serialiser.Serialise(&received,sizeof(received))==0 &&
          reinterpret_cast<const u8*>(&received)[0]==0x5A,
          "original idle serialise preserves the destination");
    serialiser.Unlock();
    Check(asserts==0, "real base recording/playback lock discipline holds");
    std::printf("ReplayGameModuleSerialiser: %u checks, %u failures\n",checks,failures);
    return failures ? 1 : 0;
}
