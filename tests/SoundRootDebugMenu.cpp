// The complete production RootSoundModule::Update and DebugAudioMemoryDump bodies
// run against recording module/allocator endpoints. ARTIST826FB238/826AF288 pin
// the expected order independently of the recovered implementation.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "types.hpp"

static std::vector<std::string> sEvents;
static void Event(const char* text) { sEvents.emplace_back(text); }
static int sAssertions = 0;
#define CGS_ASSERT(expression, message) do { if (!(expression)) { ++sAssertions; Event(message); } } while (0)

namespace CgsDev {
namespace Message { u32 gxMessageFilterFlags = 0; }
namespace Log {
    struct Stream {
        std::string mText;
        Stream& operator<<(const char* text) { mText += text; return *this; }
    } sStream;
    Stream* gpDebugPrint = &sStream;
}
namespace PerfMonCpu {
    void StartMonitor(s32) { Event("logic-start"); }
    void StopMonitor(s32) { Event("logic-stop"); }
}
}

namespace rw { namespace audio { namespace core {
struct System {
    u32 muDeferredRingHighWater = 0;
    static System* spLastLocked;
    static void Lock(System* system) { spLastLocked = system; Event("rwac-lock"); }
    static void Unlock(System* system) {
        if (spLastLocked != system) ++sAssertions;
        Event("rwac-unlock");
    }
};
System* System::spLastLocked = nullptr;
} } }

namespace CgsSound {
namespace TestBed {
struct Allocator {
    const char* mName;
    explicit Allocator(const char* name, s32) : mName(name) {}
    void Record(const char* operation) { sEvents.emplace_back(std::string(mName) + operation); }
    void SafeDump() { Record("-safe"); }
    void SortedDump() { Record("-sorted"); }
    void SanityCheck() { Record("-sanity"); }
};
}
namespace Playback {
    rw::audio::core::System sDefaultSystem;
    rw::audio::core::System* GetDefaultRwacSystem() { return &sDefaultSystem; }
    namespace Module { namespace Io {
        struct Queue {};
        struct InputBuffer {
            void LockForWrite() {}
            void UnlockForWrite() {}
        };
        struct OutputBuffer {
            Queue mQueue;
            void LockForRead() {}
            void UnlockForRead() {}
            Queue& GetResourceRequestQueue() { return mQueue; }
            Queue& GetStreamBuffersFreed() { return mQueue; }
        };
    } }
}
}

namespace CgsModule {
struct IOBufferStack {
    template<class T> void CreateIOBuffer(T** buffer, const char*) { *buffer = new T; }
    template<class T> void DestroyIOBuffer(T** buffer) { delete *buffer; *buffer = nullptr; Event("destroy"); }
};
}

enum BrnUpdateSet { E_TEST_UPDATE_SET };
namespace BrnSound { namespace Module {
namespace Io {
    struct LogicOutputBuffer {
        void LockForRead() {}
        void UnlockForRead() {}
    };
    struct RootInputBuffer {};
    struct Queue {
        template<class T> void Append(T&) { Event("resource-append"); }
        template<class T> void AppendArray(T&) { Event("freed-append"); }
    };
    struct Request { Queue mRequestQueue; };
    struct RootOutputBuffer {
        Request mRequest;
        void LockForWrite() {}
        void UnlockForWrite() {}
        Request* GetResourceRequestInterface() { return &mRequest; }
    };
}
struct PlaybackModule {
    void AdvanceTime(f32) { Event("advance"); }
    template<class A, class B> void Update(A*, B*) { Event("playback-update"); }
    void DumpRegistries() { Event("registries"); }
};
struct LogicModule {
    PlaybackModule mPlayback;
    Io::Queue mFreed;
    template<class A, class B> void Update(f32, f32, A*, B*, BrnUpdateSet) { Event("logic-update"); }
    PlaybackModule& GetPlaybackModule() { return mPlayback; }
    Io::Queue& GetFreedStreamBufferIds() { return mFreed; }
};
struct Statistics { void Update() { Event("statistics"); } };
struct DebugComponent { Statistics mStatistics; };
struct RootSoundModule {
    s32 miLogicUpdate = 0;
    LogicModule mLogicModule;
    DebugComponent mDebugComponent;
    rw::audio::core::System* mpSystem = nullptr;
    void BridgeLogicToRoot(const Io::LogicOutputBuffer*, Io::RootOutputBuffer*) { Event("bridge"); }
    void Update(f32, f32, CgsModule::IOBufferStack*, CgsModule::IOBufferStack*,
                Io::RootInputBuffer*, Io::RootOutputBuffer*, BrnUpdateSet);
};
} }

#include "sound_root_debug_menu.inc"

static int sChecks = 0, sFailures = 0;
static void Check(bool result, const char* name) {
    ++sChecks;
    if (!result) { ++sFailures; std::printf("FAIL: %s\n", name); }
}
static std::vector<std::string> DebugTail() {
    for (usize i = 0; i != sEvents.size(); ++i)
        if (sEvents[i] == "statistics") return {sEvents.begin() + i, sEvents.end()};
    return {};
}

int main() {
    BrnSound::Module::RootSoundModule module;
    rw::audio::core::System otherSystem;
    module.mpSystem = &otherSystem;
    CgsModule::IOBufferStack input, output;
    BrnSound::Module::Io::RootInputBuffer rootInput;
    BrnSound::Module::Io::RootOutputBuffer rootOutput;
    auto update = [&]() {
        sEvents.clear();
        module.Update(.016f, .016f, &input, &output, &rootInput, &rootOutput, E_TEST_UPDATE_SET);
    };
    const std::vector<std::string> ordinary = {
        "statistics", "Logic-sanity", "Playback-sanity", "rwac-lock", "Rwac-sanity", "rwac-unlock",
        "destroy", "destroy", "destroy"
    };
    update();
    Check(DebugTail() == ordinary, "statistics and allocator checks retain original per-frame order");
    Check(rw::audio::core::System::spLastLocked == &CgsSound::Playback::sDefaultSystem,
          "allocator check locks actual default RWAC system rather than root member");
    Check(KI_DEBUG_PRINT_AUDIO_ALLOCATIONS == 0, "inactive dump stays inactive");

    const std::vector<std::string> dump = {
        "Rwac-safe", "Playback-safe", "Logic-safe", "Csis-safe",
        "Rwac-sorted", "Playback-sorted", "Logic-sorted", "Csis-sorted",
        "Rwac-sanity", "Playback-sanity", "Logic-sanity", "Csis-sanity"
    };
    KI_DEBUG_PRINT_AUDIO_ALLOCATIONS = 1;
    CgsDev::Message::gxMessageFilterFlags = 1;
    update();
    std::vector<std::string> expected(ordinary.begin(), ordinary.begin() + 6);
    expected.emplace_back("registries");
    expected.insert(expected.end(), dump.begin(), dump.end());
    expected.insert(expected.end(), ordinary.end() - 3, ordinary.end());
    Check(DebugTail() == expected, "one-shot registry/safe/sorted/sanity dump order");
    Check(KI_DEBUG_PRINT_AUDIO_ALLOCATIONS == 0, "memory dump clears original s32 action cell");
    Check(CgsDev::Log::sStream.mText == "--------------------------------------------------------------------------------\nEnd of DebugAudioMemoryDump()\n",
          "original filter-enabled dump footer");
    update();
    Check(DebugTail() == ordinary, "cleared memory action does not repeat on next update");
    sEvents.clear();
    CgsDev::Log::sStream.mText.clear();
    CgsDev::Message::gxMessageFilterFlags = 0;
    DebugAudioMemoryDump();
    Check(sEvents == dump, "message filter does not suppress actual allocator dumps");
    Check(CgsDev::Log::sStream.mText.empty(), "message filter suppresses footer only");
    Check(sAssertions == 0, "complete update receives valid real-shaped endpoints");
    std::printf("SoundRootDebugMenu: %d checks, %d failures\n", sChecks, sFailures);
    return sFailures ? 1 : 0;
}
