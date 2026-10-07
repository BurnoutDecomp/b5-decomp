// ARTIST 82652F48 output lifecycle on reused IO-stack storage.
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
#include "replay_presim_output_io.inc"

using BrnReplays::ReplayIO::OutputBuffer_PreSim;
int main()
{
    struct Guarded {
        u64 before[2];
        OutputBuffer_PreSim output;
        u64 after[2];
    } storage;
    for (unsigned char poison : {0xA5, 0x5A})
    {
        std::memset(&storage, poison, sizeof(storage));
        auto& output = storage.output;
        auto& status = output.mStatusInterface;
        auto& requests = output.mGameDataRequestInterface.mRequestQueue;
        CgsModule::VariableEventQueue<4096,16>& gui = output.mGuiEventQueue;
        for (unsigned reuse = 0; reuse < 2; ++reuse)
        {
            unsigned char names[6][256], requestBytes[1024], guiBytes[4096];
            for (unsigned reel = 0; reel < 6; ++reel)
                std::memcpy(names[reel], status.maReels[reel].macName, 256);
            std::memcpy(requestBytes, requests.macData, sizeof(requestBytes));
            std::memcpy(guiBytes, gui.macData, sizeof(guiBytes));
            output.Construct();
            Check(storage.before[0] == storage.before[1] && storage.after[0] == storage.before[0]
                  && storage.after[1] == storage.before[0], "constructor stays inside its allocation");
            Check(!output.IsBufferLocked(), "constructor removes stale IO locks");
            Check(status.mxStatusFlags == 0 && status.miCurrentRecordReel == -1
                  && status.miCurrentPlaybackReel == -1 && status.mfDebugHudAlpha == 0.0f,
                  "real status reset clears flags, indices and trailing float");
            for (unsigned reel = 0; reel < 6; ++reel)
            {
                Check(!status.maReels[reel].mbUsed, "all six original reel use flags are reset");
                Check(std::memcmp(names[reel], status.maReels[reel].macName, 256) == 0,
                      "reel name bytes survive selective construction");
            }
            Check(requests.mbIsConstructed && requests.GetLength() == 0,
                  "real request queue is constructed with no stale records");
            Check(gui.mbIsConstructed && gui.GetLength() == 0,
                  "real GUI queue is constructed with no stale records");
            Check(requests.miBufferWritePos == requests.miFirstEventOffset
                  && (reinterpret_cast<uintptr_t>(requests.macData)+requests.miFirstEventOffset)%16 == 0,
                  "request cursor follows real host alignment");
            Check(gui.miBufferWritePos == gui.miFirstEventOffset
                  && (reinterpret_cast<uintptr_t>(gui.macData)+gui.miFirstEventOffset)%16 == 0,
                  "GUI cursor follows real host alignment");
            Check(std::memcmp(requestBytes, requests.macData, sizeof(requestBytes)) == 0
                  && std::memcmp(guiBytes, gui.macData, sizeof(guiBytes)) == 0,
                  "queue payload bytes survive construction");
            output.LockForWrite();
            const u32 payload = 0x12345678;
            Check(requests.AddEvent(reinterpret_cast<const CgsModule::Event*>(&payload), 21, sizeof(payload))
                  && gui.AddEvent(reinterpret_cast<const CgsModule::Event*>(&payload), 40, sizeof(payload)),
                  "constructed real queues accept records");
            output.UnlockForWrite();
            unsigned char before[sizeof(output)];
            std::memcpy(before, &output, sizeof(output));
            output.Destruct();
            Check(!output.mxStatusFlags.IsBitSet(CgsModule::IOBuffer::eStatusConstructed),
                  "real destructor releases IO-buffer construction state");
            Check(std::memcmp(before+1, reinterpret_cast<const unsigned char*>(&output)+1,
                              sizeof(output)-1) == 0,
                  "ICF-shared destructor preserves status and both queues");
        }
    }
    Check(asserts == 0, "constructor, queue use and destructor satisfy real assertions");
    std::printf("ReplayPreSimOutputIO: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
