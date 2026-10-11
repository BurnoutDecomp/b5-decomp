// Production-body regression. The endpoints record real registration addresses,
// log dispatch and mixer writes; no game/audio device is needed for this check.
#include <cstdio>
#include <cmath>
#include <map>
#include <string>
#include <vector>
#include "GameShared/GameClasses/Development/Log/CgsLogCombined.h"
#include "GameShared/GameClasses/Development/Log/CgsLogNull.h"
#include "GameShared/GameClasses/Development/Log/CgsLogOutput.h"
#include "GameSource/Sound/Debug/BrnSoundDebugStatistics.h"

namespace CgsDev
{
StrStreamBase::StrStreamBase() : mePrintMode(E_PRINTMODE_DECIMAL) {}
namespace Log
{
    StrStreamBase* gpDebugPrint = nullptr;
    std::string sOutput;
    void WriteToLog(const char* p) { sOutput += p; }
}
struct TestStream : StrStreamBase
{
    std::string mText;
    StrStreamBase& operator<<(const char* p) override { mText += p; return *this; }
};
namespace DebugUI
{
    struct StringList { s32 miValue; const char* mpcName; };
    struct Console { TestStream mLog; };
    struct DebugUI { Console mConsole; Console& GetConsole() { return mConsole; } };
}
DebugUI::DebugUI sUI;
struct DebugComponent { void OnActivate() {} };
}

namespace Nicotine { namespace DMixIO { enum { DMX_DEPTH, DMX_FREQ, DMX_VOL }; } }
namespace CgsSound
{
namespace Diag
{
    bool DMixDiagEnabled() { return false; }
    void DMixDiagPrintf(const char*, ...) {}
}
namespace Logic
{
    class Voice
    {
    public:
        f32 mParameters[12] = {};
        f32 mGains[2] = {};
        u32 muParameterWrites[12] = {};
        bool IsReady() const { return true; }
        void SetParameter(s32 i, f32 f, const u32*) { mParameters[i] = f; ++muParameterWrites[i]; }
        void SetGain(s32 i, f32 f, const u32*) { mGains[i] = f; }
    };
}
}
namespace BrnSound
{
namespace Module
{
    class SoundLogicModule
    {
    public:
        CgsSound::Logic::Voice mMaster;
        CgsSound::Logic::Voice& GetMasterVoice() { return mMaster; }
    };
}
namespace Vehicles { namespace Environment { bool KB_SHOW_STATIC_ENVIRONMENT = false; } }
namespace Logic
{
namespace World { bool KB_DEBUG_WORLD_EMITTERS = false; }
extern f32 KF_GLOBAL_GAIN, KF_GLOBAL_LOW_SHELF_GAIN, KF_GLOBAL_LOW_SHELF_FREQ;
extern f32 KF_GLOBAL_LIMITER_THRESHOLD, KF_GLOBAL_LIMITER_RELEASE;
extern f32 KF_GLOBAL_GAIN_ARRAY_CENTRE, KF_GLOBAL_GAIN_ARRAY_FRONT;
extern f32 KF_GLOBAL_GAIN_ARRAY_REAR, KF_GLOBAL_GAIN_ARRAY_SUB;
const u32 KU_MASTER_VOICE_PARAMETERS[12] = {};
const u32 KU_COLLISION_SUBMIX_SENDS[2] = {};
const u32 KU_COLLISION_SUBMIX_PARAMETERS[1] = {};
const u32 KU_SUBMIX_VOICE_SENDS[2] = {};
class GlobalStateManager
{
public:
    enum { E_SUBMIX_VOICE_COLLISION, E_SUBMIX_VOICE_PASSBY };
    CgsSound::Logic::Voice mVoices[2];
    CgsSound::Logic::Voice& GetSubmixVoice(s32 i) { return mVoices[i]; }
};
struct SubmixesEffect
{
    bool mbIsSurround = false;
    bool mbHoldVolumes = false;
    GlobalStateManager* mpStateManager;
    Module::SoundLogicModule* mpLogicModule;
    f32 GetRWACMixerOutputValue(s32 i, s32) const
    {
        return i == 0 ? 0.2f : i == 1 ? 30000.0f : i == 5 ? 0.3f : 0.4f;
    }
    void ProcessUpdate();
    void UpdateStaticPluginParameters(CgsSound::Logic::Voice&);
};
}
namespace Debug
{
void ConsoleChangeCallback(void*, void*);
struct Registration
{
    void* mpValue;
    f32 mfMin = 0, mfMax = 0, mfStep = 0;
    void (*mpCallback)(void*,void*) = nullptr;
    void* mpUserData = nullptr;
    const CgsDev::DebugUI::StringList* mpOptions = nullptr;
};
class DebugComponent : public CgsDev::DebugComponent
{
public:
    enum EConsole { E_AUDIO, E_GAME, E_AUDIO_AND_GAME, E_MAX_CONSOLES };
    CgsDev::Log::LogNull mLogNull;
    CgsDev::Log::LogOutput mLogTty;
    CgsSound::Playback::Module::Module* mpPlaybackModule;
    Module::SoundLogicModule* mpLogicModule;
    CgsDev::DebugUI::StringList maConsoleNames[4];
    s32 meOutput;
    bool mbSpliceAudition, mbSpliceAuditionAutoCycle;
    s32 miSpliceBankIndex, miSpliceIndexTens, miSpliceIndexUnits;
    void* mpSpliceAuditionVoice;
    Statistics mStatistics;
    static CgsDev::Log::LogCombined mLogCombined;
    std::map<std::string, Registration> mRegistrations;
    u32 muConsoleRegistrations = 0;
    DebugComponent();
    void Construct(CgsSound::Playback::Module::Module*, Module::SoundLogicModule*);
    void OnActivate();
    void InstallConsole(EConsole);
    template<class T> void RegisterVariable(T* p, const char* g, const char* n)
    { mRegistrations[std::string(g) + "/" + n].mpValue = p; }
    void RegisterVariable(s32* p, const char* n)
    { ++muConsoleRegistrations; mRegistrations[n].mpValue = p; }
    Registration& For(void* p)
    {
        for (auto& r : mRegistrations) if (r.second.mpValue == p) return r.second;
        std::abort();
    }
    template<class T> void SetRange(T* p, T a, T b) { auto& r = For(p); r.mfMin = (f32)a; r.mfMax = (f32)b; }
    template<class T> void SetStep(T* p, T s) { For(p).mfStep = (f32)s; }
    void SetOptions(s32* p, const CgsDev::DebugUI::StringList* o) { For(p).mpOptions = o; }
    void SetChangeCallback(void* p, void (*f)(void*,void*), void* u)
    { For(p).mpCallback = f; For(p).mpUserData = u; }
    CgsDev::DebugUI::DebugUI& GetUI() { return CgsDev::sUI; }
};
CgsDev::Log::LogCombined DebugComponent::mLogCombined;
}
}

#include "sound_debug_menu.inc"

static int sChecks = 0, sFailures = 0;
static void Check(bool b, const char* n)
{ ++sChecks; if (!b) { ++sFailures; std::printf("FAIL: %s\n", n); } }
static bool Near(f32 a, f32 b) { return std::fabs(a - b) < 0.00001f; }

int main()
{
    using namespace BrnSound;
    Debug::DebugComponent c;
    Module::SoundLogicModule module;
    c.Construct(nullptr, &module);
    Check(c.mpLogicModule == &module && c.mStatistics.mpLogicModule == &module, "constructor module bindings");
    Check(c.meOutput == 0 && !c.mbSpliceAudition && !c.mbSpliceAuditionAutoCycle,
          "constructor console and audition state");
    Check(c.miSpliceBankIndex == 0 && c.miSpliceIndexTens == 0 && c.miSpliceIndexUnits == 0 &&
          c.mpSpliceAuditionVoice == nullptr, "constructor splice cursors");
    const char* labels[] = { "AEMS Voices", "Splicer Voices", "Master Voices", "Submix Voices", "Player Voices" };
    for (int i = 0; i != 5; ++i)
        Check(std::string(c.mStatistics.mVoiceProfiles[i].mpcName) == labels[i], "voice histogram labels");
    c.mStatistics.mVoiceCount.Set(7);
    c.mStatistics.mVoiceCount.Set(3);
    Check(c.mStatistics.mVoiceCount.GetCurrent() == 3 && c.mStatistics.mVoiceCount.GetMax() == 7, "counter current/max");
    c.Construct(nullptr, &module);
    Check(c.mStatistics.mVoiceCount.GetMax() == 7, "Construct preserves existing statistics");
    c.OnActivate();
    Check(c.mRegistrations.size() == 12, "12 recovered live registrations");
    auto& console = c.mRegistrations.at("Console");
    Check(console.mpValue == &c.meOutput && console.mfMin == 0 && console.mfMax == 2,
          "console storage and enum range");
    Check(console.mpCallback == &Debug::ConsoleChangeCallback && console.mpUserData == &c,
          "console callback binding");
    for (int i = 0; i != 3; ++i)
    {
        *static_cast<s32*>(console.mpValue) = i;
        console.mpCallback(console.mpValue, console.mpUserData);
        CgsDev::Log::sOutput.clear();
        CgsDev::sUI.GetConsole().mLog.mText.clear();
        *CgsDev::Log::gpDebugPrint << "game";
        c.mLogCombined << "audio";
        Check(CgsDev::Log::sOutput == (i == 0 ? "audio" : i == 1 ? "game" : "gameaudio"), "console TTY routing");
        Check(CgsDev::sUI.GetConsole().mLog.mText == (i == 1 ? "" : "audio"), "console UI routing");
    }
    c.InstallConsole(Debug::DebugComponent::E_AUDIO); // ignored argument, live enum remains 2
    Check(CgsDev::Log::gpDebugPrint == &c.mLogTty, "ARTIST ignores passed enum");
    c.OnActivate();
    Check(c.muConsoleRegistrations == 1, "one process-lifetime console registration");

    struct Tune { const char* name; f32* p; f32 min, max, step; } tunes[] = {
        { "Master Gain", &Logic::KF_GLOBAL_GAIN, 0, 50, .25f },
        { "LF Shelf Gain", &Logic::KF_GLOBAL_LOW_SHELF_GAIN, 0, 50, .1f },
        { "LF Shelf Freq", &Logic::KF_GLOBAL_LOW_SHELF_FREQ, 0, 120, 2 },
        { "Limiter Thresh", &Logic::KF_GLOBAL_LIMITER_THRESHOLD, -100, 20, 1 },
        { "Limiter Release", &Logic::KF_GLOBAL_LIMITER_RELEASE, 0, 1000, 1 },
        { "GainArray Centre", &Logic::KF_GLOBAL_GAIN_ARRAY_CENTRE, 0, 50, .02f },
        { "GainArray Front", &Logic::KF_GLOBAL_GAIN_ARRAY_FRONT, 0, 50, .02f },
        { "GainArray Rear", &Logic::KF_GLOBAL_GAIN_ARRAY_REAR, 0, 50, .02f },
        { "GainArray Sub", &Logic::KF_GLOBAL_GAIN_ARRAY_SUB, 0, 50, .02f }
    };
    for (auto& t : tunes)
    {
        const auto& r = c.mRegistrations.at(std::string("Global/") + t.name);
        Check(r.mpValue == t.p && Near(r.mfMin,t.min) && Near(r.mfMax,t.max) && Near(r.mfStep,t.step), "shared storage/range/step");
    }
    Logic::GlobalStateManager state;
    Logic::SubmixesEffect effect;
    effect.mpStateManager = &state;
    effect.mpLogicModule = &module;
    effect.ProcessUpdate();
    Check(Near(module.mMaster.mParameters[8], 1.6f), "default master gain consumer");
    for (int i = 0; i != 9; ++i)
        *static_cast<f32*>(c.mRegistrations.at(std::string("Global/") + tunes[i].name).mpValue) = (f32)(i + 2);
    effect.ProcessUpdate();
    Check(Near(module.mMaster.mParameters[8], .8f), "menu master gain reaches ProcessUpdate");
    Check(Near(module.mMaster.mParameters[0],4) && Near(module.mMaster.mParameters[1],3), "menu shelf controls reach mixer");
    Check(Near(module.mMaster.mParameters[9],5) && Near(module.mMaster.mParameters[10],6), "menu limiter controls reach mixer");
    Check(Near(module.mMaster.mParameters[3],7), "menu centre gain reaches mixer");
    Check(Near(module.mMaster.mParameters[2],8) && Near(module.mMaster.mParameters[4],8), "front gain shared by stereo pair");
    Check(Near(module.mMaster.mParameters[5],9) && Near(module.mMaster.mParameters[6],9), "rear gain shared by stereo pair");
    Check(Near(module.mMaster.mParameters[7],10), "menu sub gain reaches mixer");
    effect.mbIsSurround = true;
    effect.mbHoldVolumes = true;
    Logic::KF_GLOBAL_GAIN = 30;
    effect.ProcessUpdate();
    Check(Near(module.mMaster.mParameters[8],.8f), "hold volumes preserves master gain");
    Check(Near(module.mMaster.mParameters[0],56) && Near(module.mMaster.mParameters[1],.3f) &&
          Near(module.mMaster.mParameters[3],1.3f), "surround retains original alternate tunings");
    Check(Near(state.mVoices[0].mParameters[0],96000), "original filter clamp remains");
    Check(c.mRegistrations.at("Emitters/Debug").mpValue == &Logic::World::KB_DEBUG_WORLD_EMITTERS,
          "emitter registration points to actual consumer global");
    Check(c.mRegistrations.at("Passbys&Reverb/Show Static").mpValue == &Vehicles::Environment::KB_SHOW_STATIC_ENVIRONMENT,
          "static passby registration points to actual consumer global");
    std::printf("SoundDebugMenu: %d checks, %d failures\n", sChecks, sFailures);
    return sFailures != 0;
}
