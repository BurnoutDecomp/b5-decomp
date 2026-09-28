// ARTIST CreateInstance 826C28A0: the mono Send is DRY, while mpSendWet is zero.
// Route stages retain all source channels; every panner inherits main priority.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <vector>
using u8 = uint8_t; using u32 = uint32_t; using f32 = float; using f64 = double;
#define CGS_ASSERT(test, message) do { if (!(test)) std::abort(); } while (0)
namespace Snd9 {
struct IAemsSamplePlayer {
    enum InputSelector { PLAYER_INPUT_PITCHMULT, PLAYER_INPUT_VOL,
        PLAYER_INPUT_AZIMUTH, PLAYER_INPUT_FXWET0, PLAYER_INPUT_DRYLEVEL,
        PLAYER_INPUT_USER_FIRST };
};
struct AemsPlayerInputAccessor { int GetValueByType(int) const { return 7; } };
}
namespace rw { namespace audio { namespace core {
struct SubMix;
struct RouteConnectEvent { SubMix* mpSubMix; float mfGain0, mfGain1, mfGain2; };
struct PlugInDescRunTime {};
struct PlugIn {
    float value = 1.0f;
    void* target = nullptr;
    RouteConnectEvent route = {};
    static void SetAttribute(PlugIn* p, int, float v) { p->value = v; }
    static void Event(PlugIn*, int, void*) {}
};
struct SubMix : PlugIn {};
struct Send : PlugIn {
    static void EventEvent(Send* p, int, void* args) { p->target = *static_cast<void**>(args); }
};
struct Route : PlugIn {
    static void EventEvent(Route* p, int, void* args) { p->route = *static_cast<RouteConnectEvent*>(args); }
};
struct SndPlayer1 {
    struct FileInfo { u32 numChannels, sampleRate, numSamples; };
    struct PlayLegacyParams {
        double startTime, streamFileOffset;
        const char* pStreamFilePath; void* pRamData;
        u32 streamPoolGuid; float expelMode, requestHandle;
    };
    static void GetFileInfo(void* data, FileInfo* info) { *info = *static_cast<FileInfo*>(data); }
};
struct System {
    static void* Alloc(System*, u32 size, const char*, int, int) { return ::operator new(size); }
};
struct VoiceStageConfig { void* mpContext; PlugInDescRunTime* mpDesc; u32 mFlagAndField8; };
struct Voice {
    float priority = -1.0f;
    std::vector<VoiceStageConfig> stages;
    std::vector<PlugIn*> plugins;
    static Voice* CreateInstance(int, int count, VoiceStageConfig* stages, PlugIn*** out, System*) {
        auto* v = new Voice;
        v->stages.assign(stages, stages + count);
        for (int i = 0; i < count; ++i) v->plugins.push_back(new Send);
        *out = v->plugins.data(); return v;
    }
    static void SetPriority(Voice* v, float p) { v->priority = p; }
};
}}}
namespace CgsSound {
namespace PcmTrace { inline void Register(void*, void*, u32, const char* = nullptr) {} }
namespace Playback {
namespace rwac = rw::audio::core;
struct Voice { void Release() {} u32 GetIdent() { return 7; } };
struct AemsPlayerVoice : Voice {
    rwac::SubMix mix;
    void* GetInternalSubmix() { return &mix; }
};
template<class T> struct Handle { T* object; T* GetObject() { return object; } };
struct Environment {
    AemsPlayerVoice voice;
    Handle<Voice> GetVoice(u32) { return { &voice }; }
};
rwac::System* GetDefaultRwacSystem() { static rwac::System s; return &s; }
struct AemsRWSamplePlayer : Snd9::IAemsSamplePlayer {
    enum PauseState { PAUSESTATE_UNPAUSED, PAUSESTATE_PAUSED };
    static const u8 KU_MAX_SAMPLE_CHANNELS = 6;
    rwac::System* mpRwacSystem = nullptr;
    rwac::Voice* mpVoice = nullptr;
    rwac::Voice* mpPannerVoice[6] = {};
    rwac::PlugIn *mpSndPlayer = nullptr, *mpResample = nullptr,
        *mpSendWet = nullptr, *mpGain = nullptr, *mpPan2D[6] = {};
    float mafPreviousAzimuths[6] = {}, mPitch = 1, mVol = 1,
        mDryLevel = 1, mWetLevel = 0, mRequestHandle = -1;
    double mSampleLength = 0;
    u8 mNumChannels = 0, mNumPannerVoices = 0;
    PauseState mPauseState = PAUSESTATE_UNPAUSED;
    AemsRWSamplePlayer(Environment&, AemsPlayerVoice*) {}
    void Release() {}
    void Pause(); void Unpause();
    void SetInput(InputSelector, int);
};
struct AemsRWSampleFactory {
    struct Config { void* mpInitialValue; uintptr_t muPlugInHandle; u8 mu8ChannelCount; };
    Config maAemsSubMixPlugInConfig[3] = { {nullptr, 8, 1}, {nullptr, 2, 6}, {nullptr, 4, 6} };
    uintptr_t mGainHandle = 1, mPan2DHandle = 2, mRouteHandle = 3, mSendHandle = 4,
        mSndPlayer1Handle = 5, mRechannelHandle = 6, mResampleHandle = 7;
    Environment environment;
    Environment& GetEnvironment() { return environment; }
    Snd9::IAemsSamplePlayer* CreateInstance(void*, int, const int*, const char*, int,
                                            const Snd9::AemsPlayerInputAccessor*);
};
#include "fx_aems_sample_graph.inc"
}}
int main() {
    using namespace CgsSound::Playback;
    using I = Snd9::IAemsSamplePlayer;
    int checks = 0, failures = 0;
    auto check = [&](bool ok, const char* name) {
        ++checks; if (!ok) { ++failures; std::printf("FAIL %s\n", name); }
    };
    auto close = [](float a, float b) { return std::fabs(a - b) < 0.0001f; };
    AemsRWSampleFactory factory;
    Snd9::AemsPlayerInputAccessor accessor;
    const int azimuths[6] = { 1000, -2000, 3000, -4000, 5000, 0 };
    for (u32 channels : {1u, 2u, 6u}) {
        rwac::SndPlayer1::FileInfo info = { channels, 48000, 96000 };
        auto* p = static_cast<AemsRWSamplePlayer*>(factory.CreateInstance(
            &info, 73, azimuths, "fixture", 123, &accessor));
        check(p != nullptr, "sample player constructed");
        check(p->mNumChannels == channels && p->mSampleLength == 2.0, "sample metadata retained");
        check(p->mpSendWet == nullptr, "dry Send must not occupy optional wet Send member");
        check(close(p->mpVoice->priority, 0.73f), "main voice original priority");
        check(p->mNumPannerVoices == (channels == 6 ? 5 : channels), "original panner count");
        if (channels == 1) {
            auto* dry = p->mpVoice->plugins[5];
            check(dry->target == factory.environment.voice.GetInternalSubmix(), "mono dry output connected");
            p->SetInput(I::PLAYER_INPUT_VOL, 32767);
            check(close(p->mpGain->value, 1), "full volume feeds original Gain");
            check(close(dry->value, 1), "zero wet level must not mute dry Send");
            check(close(0.25f * p->mpGain->value * dry->value, 0.25f), "mono nonzero sample reaches dry bus");
            p->SetInput(I::PLAYER_INPUT_FXWET0, 16384);
            check(close(dry->value, 1), "wet control leaves dry output alone");
            p->SetInput(I::PLAYER_INPUT_DRYLEVEL, 16384);
            check(close(p->mpGain->value, 16384.0f / 32767.0f), "dry level applied exactly once");
            p->SetInput(I::PLAYER_INPUT_VOL, 16384);
            check(close(p->mpGain->value, std::pow(16384.0f / 32767.0f, 2)), "volume times dry level");
            check(close(dry->value, 1), "volume must not attenuate Send a second time");
            p->Pause();
            check(p->mpGain->value == 0 && p->mpResample->value == 0, "pause silences gain and stops playback");
            check(dry->value == 1, "pause does not modify dry Send");
            p->Unpause();
            check(close(p->mpGain->value, p->mDryLevel * p->mVol) && p->mpResample->value == 1,
                  "unpause restores gain and pitch");
            check(dry->value == 1, "unpause preserves dry Send");
        } else {
            bool widths = true, priorities = true, routes = true, outputs = true;
            for (u32 c = 0; c < channels; ++c)
                widths &= p->mpVoice->stages[4 + c].mFlagAndField8 == channels;
            for (u32 c = 0; c < p->mNumPannerVoices; ++c) {
                auto* v = p->mpPannerVoice[c];
                priorities &= close(v->priority, p->mpVoice->priority);
                const auto& r = p->mpVoice->plugins[4 + c]->route;
                routes &= r.mpSubMix == v->plugins[0] && r.mfGain0 == c && r.mfGain1 == 0 && r.mfGain2 == 1;
                outputs &= v->plugins[2]->target == factory.environment.voice.GetInternalSubmix();
            }
            check(widths, "Route retains all source channels");
            check(priorities, "panner priorities equal source voice priority");
            check(routes, "each source channel routes to its own mono panner");
            check(outputs, "panners feed dry internal bus");
            if (channels == 6) {
                const auto& r = p->mpVoice->plugins[9]->route;
                check(r.mpSubMix == factory.environment.voice.GetInternalSubmix() &&
                      r.mfGain0 == 5 && r.mfGain1 == 5 && r.mfGain2 == 1,
                      "sixth channel routes directly to LFE");
            }
        }
    }
    std::printf("FxAemsSampleGraph: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
