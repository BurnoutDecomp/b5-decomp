// Production state-machine bodies with bounded request and voice collaborators.
#include <cstdio>
#include <vector>
#include <cmath>
#include "GameSource/Sound/Global/BrnMusicEffect.h"
#include "GameSource/Sound/Streaming/BrnStreamingStateManager.h"
#include "GameShared/GameClasses/Sound/Playback/CgsContent.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
static unsigned checks, failures, assertions, plays;
static bool contentLoaded;
struct Request { bool stop; const void* user; f32 fade; u32 priority; };
static std::vector<Request> requests;
static void Check(bool value, const char* label)
{
    ++checks;
    if (!value) { ++failures; std::printf("FAIL %s\n", label); }
}
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++assertions; return 0; }
void* EndAssert() { return nullptr; }
} }
namespace CgsSound { namespace Logic {
Voice::~Voice() {}
Content::~Content() {}
bool Content::IsLoaded() const { return contentLoaded; }
void Voice::Connect(Ident, Ident) {}
s32 Voice::Attach(s32, Playback::Handle<Playback::Content>*) { return 0; }
void Voice::SetGain(u32, f32, const u32*) {}
s32 Voice::Play(s32) { ++plays; return 0; }
bool Voice::IsPlaying() const { return true; }
VoiceWrapper::~VoiceWrapper() {}
void VoiceWrapper::Release() {}
} }
namespace BrnSound { namespace Logic { namespace Streaming {
void StreamingStateManager::PostStreamRequest(const StreamRequest& r)
{ requests.push_back({false, r.mpAttachment, r.mfLagTolerance, r.mu32Priority}); }
void StreamingStateManager::PostStreamRequest(const StreamStopRequest& r)
{ requests.push_back({true, r.mpAttachment, r.mfFadeOut, 0}); }
} // Streaming
// These interface callbacks are outside this fixture's request-state boundary.
void MusicStream::UpdateVoiceParams(CgsSound::Logic::VoiceWrapper&, f32, f32) {}
} }
#include "audio_state_transitions.inc"

int main()
{
    using BrnSound::Logic::MusicStream;
    MusicStream stream;
    // The request sink mocks only PostStreamRequest; it never dereferences this token.
    stream.mpStreamingManager = reinterpret_cast<BrnSound::Logic::Streaming::StreamingStateManager*>(&requests);
    stream.muPriority = 5;
    stream.Queue(0x12345678, 12);
    stream.Update(0.016f);
    Check(requests.size() == 2 && requests[0].stop && !requests[1].stop,
          "replacement cancels older requests before posting play");
    Check(requests.size() == 2 && requests[0].fade == 0.0f &&
          requests[0].user == &stream && requests[1].user == &stream &&
          requests[1].priority == 5 && requests[1].fade == 0.1f,
          "replacement preserves user, priority, tolerance and immediate stop");
    Check(stream.GetOutputSlot() == 12 && stream.GetState() == MusicStream::E_PLAYING,
          "queued stream advances to its requested output bus");
    requests.clear();
    stream.StopAndUnqueue(0.25f);
    stream.Update(0.016f);
    stream.Update(0.125f);
    Check(requests.empty(), "local fade retains the stream until complete");
    stream.Update(0.125f);
    Check(requests.size() == 1 && requests[0].stop && requests[0].fade == 0.0f,
          "completed fade does not ask the streaming effect to fade a second time");
    Check(stream.GetState() == MusicStream::E_STOPPED, "fade completion frees the local stream state");
    for (u32 previous = 0; previous != 8; ++previous)
        for (u32 current = 0; current != 8; ++current)
        {
            stream.mu32PrevVoiceUpdateStage = previous;
            stream.mu32VoiceUpdateStage = current;
            Check(stream.AUDIO_READY_METHOD() == (previous != current && current == 6),
                  "audio-ready event occurs only on entry to playing");
        }
    using CgsSound::Logic::VoiceWrapper;
    VoiceWrapper voice;
    voice.mCreateParams.mpLogicModule = reinterpret_cast<CgsSound::Logic::Module*>(&requests);
    voice.meUpdateStage = VoiceWrapper::E_UPDATE_STAGE_WAIT;
    voice.mbPlay = true;
    contentLoaded = false;
    voice.Update();
    Check(plays == 0 && voice.GetUpdateStage() == VoiceWrapper::E_UPDATE_STAGE_WAIT,
          "unloaded content never plays");
    contentLoaded = true;
    voice.Update();
    Check(plays == 1 && voice.IsPlaying(), "newly ready content starts in the same logic update");
    voice.meUpdateStage = VoiceWrapper::E_UPDATE_STAGE_WAIT;
    voice.mbPlay = false;
    voice.Update();
    Check(plays == 1 && voice.GetUpdateStage() == VoiceWrapper::E_UPDATE_STAGE_START,
          "loaded content still waits for an explicit play request");
    Check(assertions == 0, "valid transitions preserve assertion contracts");
    std::printf("AudioStateTransitions: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
