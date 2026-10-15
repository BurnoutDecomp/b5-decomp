#include <cstdio>
#include <cstdlib>
#include "types.hpp"
#define CGS_ASSERT(condition, message) do { if (!(condition)) std::abort(); } while (0)

// Collaborators record the production Notify body's decisions. No mixer or
// routing decisions are reimplemented in this fixture.
namespace CgsSound {
namespace Io {
struct MessageHeader { int id = 5; int GetEventId() const { return id; } };
template<class T> struct Message : MessageHeader { T mData; };
}
namespace Playback { struct Name { static u32 MakeHash(const char*) { return 1; } }; }
namespace Logic {
struct VoiceWrapper {
    struct CreateParams { const void* mpContent = nullptr; };
    int creates = 0, plays = 0; u32 splice = 0;
    void Create(const CreateParams&) { ++creates; }
    void Play(u32 value) { ++plays; splice = value; }
    bool HasLiveVoice() const { return true; }
    void SetParameter(int, float, const u32*) {}
};
}
}
void CgsIDUnCompress(u64, char*) {}
namespace BrnSound {
namespace Debug {
struct DebugComponent { struct Sink { template<class T> Sink& operator<<(const T&) { return *this; } }; static Sink mLogCombined; };
DebugComponent::Sink DebugComponent::mLogCombined;
}
namespace Module {
struct SoundLogicModule { struct { u64 mu64Flags = 0; } mDispatchState; };
}
namespace Logic {
struct GuiAudioEventRecord {
    s32 miComponentType = 2, miAction = 0, miAdditionalInformation = 0;
    u64 mHudMessageId = 1;
};
static int guHudDiagMessages;
static bool KB_HUD_SPEW_MESSAGES = false;
bool HudSoundDiagBudget(int&) { return false; }
void HudSoundDiagPrintf(const char*, ...) {}
struct HUDEffect {
    static const int E_NUM_VOICES = 3;
    Module::SoundLogicModule module;
    struct { bool HasData() const { return true; } u32 NumMappings() const { return 1; } } mHudMessageData;
    u64 mLastReceivedHudMessage = 0;
    GuiAudioEventRecord mLastPlayedEvent;
    float mfTimeSinceLastTrigger = 1, mfRunningTime = 0;
    CgsSound::Logic::VoiceWrapper maVoices[3];
    u8 mau8MixerOutputs[3] = {}, mau8ChokeGroups[3] = {};
    bool retrigger = false;
    int slot = 1, input = 0;
    void* GetLogicModule() { return &module; }
    bool FindEventMapping(decltype(mHudMessageData)&, const GuiAudioEventRecord*, s32& splice,
                          u8& output, u8& choke) { splice = 7; output = 9; choke = 4; return true; }
    bool IsReTrigger(const GuiAudioEventRecord*) { return retrigger; }
    s32 GetFreeVoice(u8) { return slot; }
    void BuildSplicerVoiceParams(CgsSound::Logic::VoiceWrapper::CreateParams& p) { p.mpContent = this; }
    void SetMixerInputValue(int, int v) { input = v; }
    void Notify(const CgsSound::Io::MessageHeader*);
};
}
}
#include "audio_ui_routing.inc"
static int checks, failures;
static void Check(bool ok, const char* name) { ++checks; if (!ok) { ++failures; std::printf("FAIL %s\n", name); } }
int main() {
    using namespace BrnSound::Logic;
    CgsSound::Io::Message<GuiAudioEventRecord> message;
    HUDEffect effect;
    effect.Notify(&message);
    Check(effect.mau8MixerOutputs[1] == 3, "button release selects HUD volume slot 3");
    Check(effect.maVoices[1].splice == 1, "button release splice");
    Check(effect.input == 32767, "HUD activity is published");
    message.mData.miAction = 1;
    effect.module.mDispatchState.mu64Flags = 0x1200000000ull;
    effect.Notify(&message);
    Check(effect.mau8MixerOutputs[1] == 3, "dispatch upper bits cannot choose an audio slot");
    Check(effect.maVoices[1].splice == 2, "button press splice");
    const int played = effect.maVoices[1].plays;
    effect.module.mDispatchState.mu64Flags |= 2;
    effect.Notify(&message);
    Check(effect.maVoices[1].plays == played, "loading flag suppresses GUI sound");
    message.mData.miComponentType = 1;
    effect.Notify(&message);
    Check(effect.mau8MixerOutputs[1] == 9 && effect.mau8ChokeGroups[1] == 4,
          "mapped HUD message retains authored route and choke group");
    Check(effect.maVoices[1].splice == 7, "mapped splice retained");
    std::printf("AudioUiRouting: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
