#include <cstdio>
#include <cstring>
#include "types.hpp"
#include "GameSource/Gui/BrnGuiVideoEvents.h"

// Record the production attract branch's event payloads. Hash collaborators
// return distinct values so a video resource id cannot stand in for a sound id.
namespace CgsResource {
struct ID { static s32 HashString(const u8* name) {
    return std::strcmp(reinterpret_cast<const char*>(name), "attract") == 0
        ? static_cast<s32>(0xBDD7D5B6u) : 0;
} };
}
namespace CgsSound { namespace Playback {
struct Name { static uintptr_t MakeHash(const char* name) {
    return std::strcmp(name, "attract") == 0 ? 0x76543211u : 0;
} };
} }
struct MusicOnMenuStreamEvent {
    s32 hash;
    explicit MusicOnMenuStreamEvent(s32 value) : hash(value) {}
};
struct Recorder {
    int musicCount = 0, videoCount = 0;
    s32 music = -1;
    BrnGui::GuiEventPlayVideo video;
    template<class T> void OutputGuiEvent(const T& value) { Record(value); }
    void Record(const MusicOnMenuStreamEvent& value) { ++musicCount; music = value.hash; }
    void Record(const BrnGui::GuiEventPlayVideo& value) { ++videoCount; video = value; }
} recorder;
static Recorder* mpStateInterface = &recorder;
static const char* const KAAC_ATTRACT_MOVIE[] = { "attract" };
static u32 muCurrentAttractMovieIdx = 0;
#include "audio_attract_startup.inc"

static int checks, failures;
static void Check(bool ok, const char* name) {
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL %s\n", name); }
}
int main() {
    EmitAttract();
    Check(recorder.musicCount == 1 && recorder.music == 0, "clear title music before the attract video");
    Check(recorder.videoCount == 1, "queue one attract video");
    Check(recorder.video.muVideoResourceId == 0xBDD7D5B6ull, "zero-extend the video resource id");
    Check(recorder.video.muSoundStreamName == 0x76543211u, "supply the attract sound-name hash");
    Check(!recorder.video.mbPreload && !recorder.video.mbKeepMemoryWhenFinished &&
          !recorder.video.mbDisableCustomSoundtracks, "preserve the console playback flags");
    std::printf("AudioAttractStartup: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
