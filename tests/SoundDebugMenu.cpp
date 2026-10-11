// Production-body regression. Execute the real activation/options methods and record each
// registration type, label, range, option list, callback, console route, and mixer write.
#include <cstdio>
#include <cmath>
#include <map>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include "GameShared/GameClasses/Development/Log/CgsLogCombined.h"
#include "GameShared/GameClasses/Development/Log/CgsLogNull.h"
#include "GameShared/GameClasses/Development/Log/CgsLogOutput.h"
#include "GameSource/Sound/Debug/BrnSoundDebugStatistics.h"

namespace CgsSound
{
namespace Logic
{
    struct Cgs3dEffectControl
    {
        static void (*spDebugRenderingFunction)(const Cgs3dEffectControl*);
    };
}
namespace Playback
{
    struct Name
    {
        const char* mpcName = "";
        const char* GetCString() const { return mpcName; }
    };
    struct ContentSpec
    {
        Name mName;
        const Name& GetName() const { return mName; }
    };
    struct SplicerContent
    {
        ContentSpec mSpec;
        const ContentSpec& GetContentSpec() const { return mSpec; }
    };
    struct SpliceBankStatistics
    {
        static SpliceBankStatistics* spHead;
        SpliceBankStatistics* mpNext = nullptr;
        const SplicerContent* mpSpliceContent = nullptr;
        u32 muIndex = 0;
    };
    SpliceBankStatistics* SpliceBankStatistics::spHead = nullptr;
}
}

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

namespace CgsDev
{
namespace Assert
{
    int BeginAssert() { return 0; }
    int FireAssert(const char*, const char*, int) { return 0; }
    void* EndAssert() { return nullptr; }
}
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
enum class RegistrationTag { Bool, S32, Float };
enum class OptionsKind { None, Console, Material, Action, SpliceBank };
struct Registration
{
    void* mpValue;
    std::string mGroup;
    std::string mName;
    RegistrationTag mTag = RegistrationTag::S32;
    f32 mfMin = 0, mfMax = 0, mfStep = 0;
    bool mbHasRange = false, mbHasStep = false;
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
    CgsDev::DebugUI::StringList maCollisionMaterialNames[26];
    CgsDev::DebugUI::StringList maCollisionActionNames[8];
    CgsDev::DebugUI::StringList maConsoleNames[4];
    CgsDev::DebugUI::StringList maSpliceBankNames[50];
    s32 meOutput;
    bool mbSpliceAudition, mbSpliceAuditionAutoCycle;
    s32 miSpliceBankIndex, miSpliceIndexTens, miSpliceIndexUnits;
    void* mpSpliceAuditionVoice;
    Statistics mStatistics;
    static CgsDev::Log::LogCombined mLogCombined;
    std::map<std::string, Registration> mRegistrations;
    std::vector<Registration> mRegistrationCalls;
    u32 muConsoleRegistrations = 0;
    DebugComponent();
    void Construct(CgsSound::Playback::Module::Module*, Module::SoundLogicModule*);
    void OnActivate();
    void InstallConsole(EConsole);
    void BuildMaterialNames();
    void BuildActionNames();
    void BuildSplicerBankNames();
    static void Render3dControl(const CgsSound::Logic::Cgs3dEffectControl*) {}
    void Save(const Registration& r)
    {
        const std::string key = r.mGroup.empty() ? r.mName : r.mGroup + "/" + r.mName;
        mRegistrations[key] = r;
    }
    void Record(void* p, RegistrationTag tag, const char* g, const char* n, bool bConsole = false)
    {
        Registration r{};
        r.mpValue = p; r.mTag = tag; r.mGroup = g ? g : ""; r.mName = n ? n : "";
        mRegistrationCalls.push_back(r);
        Save(mRegistrationCalls.back());
        if (bConsole) ++muConsoleRegistrations;
    }
    void RegisterVariable(bool* p, const char* g, const char* n)
    { Record(p, RegistrationTag::Bool, g, n); }
    void RegisterVariable(s32* p, const char* g, const char* n)
    { Record(p, RegistrationTag::S32, g, n); }
    void RegisterVariable(f32* p, const char* g, const char* n)
    { Record(p, RegistrationTag::Float, g, n); }
    void RegisterVariable(s32* p, const char* n)
    { Record(p, RegistrationTag::S32, "", n, true); }
    Registration& For(void* p)
    {
        for (auto it = mRegistrationCalls.rbegin(); it != mRegistrationCalls.rend(); ++it)
            if (it->mpValue == p) return *it;
        std::abort();
    }
    template<class T, class A, class B> void SetRange(T* p, A a, B b)
    { auto& r = For(p); r.mbHasRange = true; r.mfMin = (f32)a; r.mfMax = (f32)b; Save(r); }
    template<class T, class S> void SetStep(T* p, S s)
    { auto& r = For(p); r.mbHasStep = true; r.mfStep = (f32)s; Save(r); }
    void SetOptions(s32* p, const CgsDev::DebugUI::StringList* o)
    { auto& r = For(p); r.mpOptions = o; Save(r); }
    void SetChangeCallback(void* p, void (*f)(void*,void*), void* u)
    { auto& r = For(p); r.mpCallback = f; r.mpUserData = u; Save(r); }
    CgsDev::DebugUI::DebugUI& GetUI() { return CgsDev::sUI; }
};
CgsDev::Log::LogCombined DebugComponent::mLogCombined;
}
}

#include "sound_debug_menu.inc"

// Expected rows are transcribed from the ARTIST 0x826D5CB0 export, in call order.
// The separate Console row comes from the InstallConsole body called by OnActivate.
struct ExpectedSite
{
    u32 muAddress;
    const char* mpcGroup;
    const char* mpcName;
    const char* mpcBacking;
    BrnSound::Debug::RegistrationTag meTag;
    bool mbHasRange;
    f32 mfMin;
    f32 mfMax;
    bool mbHasStep;
    f32 mfStep;
    BrnSound::Debug::OptionsKind meOptions;
};

static const ExpectedSite kArtistSites[] =
{
    { 0x826D5D08u, "Engines", "Max Rpm Display", "&dword_82FFB864", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D5D28u, "Engines", "Engine Info", "&dword_82FFB854", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D5D44u, "Engines", "Closest AI", "&byte_82FFB812", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D5D64u, "Engines", "Spew AI info", "&dword_82F2CCFC", BrnSound::Debug::RegistrationTag::S32, true, -1.0f, 7.0f, true, 1.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D5DA4u, "Engines", "Rpm Info", "&dword_82FFB888", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D5DC0u, "Engines", "Loop Info", "&dword_82FFB88C", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D5DDCu, "Engines", "Loop Data Info", "&dword_82FFB890", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D5DF8u, "Engines", "Pitch Below Decel Min", "&dword_82FFB884", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D5E14u, "Engines", "Boost Info", "&dword_82FFB8A4", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D5E30u, "Engines", "Hold Car in Place", "&dword_82FB7514", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D5E4Cu, "Engines", "Hold Car SPINNING in AIR", "&dword_82FB7518", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D5E6Cu, "Engines", "Spin Speed X", "&flt_82FB751C", BrnSound::Debug::RegistrationTag::Float, false, 0.0f, 0.0f, true, 0.1f, BrnSound::Debug::OptionsKind::None },
    { 0x826D5E8Cu, "Engines", "Spin Speed Y", "&flt_82FB7520", BrnSound::Debug::RegistrationTag::Float, false, 0.0f, 0.0f, true, 0.1f, BrnSound::Debug::OptionsKind::None },
    { 0x826D5EACu, "Engines", "Spin Speed Z", "&flt_82FB7524", BrnSound::Debug::RegistrationTag::Float, false, 0.0f, 0.0f, true, 0.1f, BrnSound::Debug::OptionsKind::None },
    { 0x826D5F04u, "Engines", "Hard Code Ginsu Accel Vol", "&flt_82FFB870", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 1.0f, true, 0.050000001f, BrnSound::Debug::OptionsKind::None },
    { 0x826D5F60u, "Engines", "Hard Code Ginsu Decel Vol", "&flt_82FFB874", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 1.0f, true, 0.050000001f, BrnSound::Debug::OptionsKind::None },
    { 0x826D5FA4u, "Engines", "Hard Code Loop Model Vol", "&flt_82FFB878", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 1.0f, true, 0.050000001f, BrnSound::Debug::OptionsKind::None },
    { 0x826D5FE8u, "Engines", "Hard code ENGINE Vol", "&flt_82F2CC7C", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 1.0f, true, 0.050000001f, BrnSound::Debug::OptionsKind::None },
    { 0x826D602Cu, "Engines", "Hard Code EXHAUST Vol", "&flt_82F2CC80", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 1.0f, true, 0.050000001f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6070u, "Engines", "Hard Code RPM", "&flt_82FFB8A0", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 10000.0f, true, 500.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D60C8u, "Engines", "Shuffle Pitch", "&flt_82F2CD00", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 100.0f, true, 1.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6110u, "Engines", "Rpm Ramping", "&byte_82FFB84B", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D612Cu, "Engines", "Engine Loading", "&dword_82FFB830", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6154u, "Engines Pan", "Panning Distance", "&byte_82F2CCD4", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6170u, "Engines Pan", "Panning Distance", "&flt_82F2CCD8", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 3.0f, true, 0.050000001f, BrnSound::Debug::OptionsKind::None },
    { 0x826D61BCu, "Engines Pan", "Panning Size", "&flt_82F2CCDC", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 3.0f, true, 0.050000001f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6200u, "Engines Pan", "Panning Twist", "&flt_82FFB898", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 360.0f, true, 5.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6250u, "Engines Pan", "Panning CentreLvl", "&flt_82F2CCE0", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 1.0f, true, 0.050000001f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6294u, "Engines Pan", "Panning MainLvl", "&BrnSound::Vehicles::Engines::KF_PANNING_MAINLVL", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 1.0f, true, 0.050000001f, BrnSound::Debug::OptionsKind::None },
    { 0x826D62D8u, "Engines Pan", "Panning LFElvl", "&flt_82F2CCE8", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 1.0f, true, 0.050000001f, BrnSound::Debug::OptionsKind::None },
    { 0x826D631Cu, "Engines", "Decel Ginsu Fade Out", "&unk_82F2CC60", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 1.0f, true, 0.0099999998f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6368u, "Engines", "Decel Ginsu Fade In", "&flt_82F2CC64", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 1.0f, true, 0.0099999998f, BrnSound::Debug::OptionsKind::None },
    { 0x826D63ACu, "Engines", "Idle RPM Interp", "&flt_82F2CC84", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 10000.0f, true, 100.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D63F0u, "Engines", "Clutch: RpmDelta", "&flt_82F2CC28", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 1000.0f, true, 10.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6448u, "Engines", "Clutch: Thresh", "&flt_82F2CC2C", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 5000.0f, true, 100.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6488u, "Engines", "Clutch: Thresh", "&flt_82F2CC2C", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 5000.0f, true, 100.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D64CCu, "Engines", "Reverse Max Speed", "&flt_82F2CD04", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 5000.0f, true, 1.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6510u, "Engines", "Reverse Pitch at 0", "&flt_82F2CD08", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 4.0f, true, 0.1f, BrnSound::Debug::OptionsKind::None },
    { 0x826D655Cu, "Engines", "Reverse Pitch at max speed", "&flt_82F2CD0C", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 4.0f, true, 0.1f, BrnSound::Debug::OptionsKind::None },
    { 0x826D65A8u, "RedLining", "RPM", "&flt_82F2CC3C", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 10000.0f, true, 10.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D65ECu, "RedLining", "Rpm Offset", "&flt_82F2CC40", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 10000.0f, true, 10.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6630u, "RedLining", "Duck Duration HI", "&flt_82F2CC44", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 1.0f, true, 0.0099999998f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6674u, "RedLining", "Duck Duration LOW", "&flt_82F2CC48", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 1.0f, true, 0.0099999998f, BrnSound::Debug::OptionsKind::None },
    { 0x826D66B8u, "RedLining", "Volume Boost", "&flt_82F2CC4C", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 10.0f, true, 0.0099999998f, BrnSound::Debug::OptionsKind::None },
    { 0x826D66FCu, "RedLining", "Volume Duck", "&flt_82F2CC50", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 10.0f, true, 0.0099999998f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6744u, "InAir", "Debug Ramp Launch", "&unk_82FFB8BC", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6764u, "InAir", "Force Launch from Ramp", "&unk_82F2CD88", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 10.0f, true, 0.1f, BrnSound::Debug::OptionsKind::None },
    { 0x826D67A8u, "InAir", "Min Speed", "&flt_82F2F720", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 180.0f, true, 5.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D67ECu, "InAir", "Max Speed", "&flt_82F2F724", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 180.0f, true, 5.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6828u, "InAir", "Slowest Ramp(ms)", "&dword_82F2F728", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 2000.0f, true, 25.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6870u, "InAir", "Fastest Ramp(ms)", "&dword_82F2F72C", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 2000.0f, true, 25.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D68B8u, "Wheels", "Wheel Info", "&dword_82FFB8C0", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D68D4u, "Wheels", "Skid Info", "&dword_82FFB8B0", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D68F0u, "Wheels", "Wheel Side Info", "&unk_82FFB8C4", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6910u, "Wheels", "Surface Types", "&byte_82FFB8BD", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D692Cu, "Wheels", "Ai Skids info", "&byte_82FFB813", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D694Cu, "Collision", "Print Collision Bins", "&unk_82FFB900", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6968u, "Collision", "Collision Info", "&dword_82FFB8D8", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6980u, "Collision", "Collision Bin Logic Verbose", "&dword_82FFB904", BrnSound::Debug::RegistrationTag::S32, true, 0.0f, 3.0f, true, 1.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D69BCu, "Collision", "CollisionCulling", "&dword_82FFB91C", BrnSound::Debug::RegistrationTag::S32, true, 0.0f, 4.0f, true, 1.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D69F8u, "Collision", "Print Detaching", "&dword_82FFB8F0", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6A14u, "Collision", "print Ducking", "&dword_82FFB8E0", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6A2Cu, "Collision", "Print All Inputs", "&dword_82FFB918", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6A44u, "Collision", "No Normal", "&dword_82FFB920", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6A5Cu, "Collision", "No Props", "&dword_82FFB924", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6A74u, "Collision", "No bodyparts", "&dword_82FFB928", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6A8Cu, "Collision", "No Glass", "&dword_82FFB92C", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6AACu, "Collision", "Hinging Tol", "&flt_82F2CEF0", BrnSound::Debug::RegistrationTag::Float, false, 0.0f, 0.0f, true, 0.0099999998f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6AD4u, "Collision", "Hinging Delta", "&flt_82F2CEF4", BrnSound::Debug::RegistrationTag::Float, false, 0.0f, 0.0f, true, 0.0099999998f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6AFCu, "Collision", "Force ID LARGE", "&dword_82FFB8F4", BrnSound::Debug::RegistrationTag::S32, true, 0.0f, 500.0f, true, 10.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6B38u, "Collision", "Force ID SMALL", "&dword_82F2CF00", BrnSound::Debug::RegistrationTag::S32, true, -1.0f, 9.0f, true, 1.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6B74u, "Collision", "Pipeline", "&dword_82F2CF04", BrnSound::Debug::RegistrationTag::S32, true, 0.0f, 1.0f, true, 1.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6BB0u, "Collision", "Play Tons", "&dword_82FFB908", BrnSound::Debug::RegistrationTag::S32, true, 0.0f, 1.0f, true, 1.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6BECu, "Collision", "Fastest thing", "&flt_82F2CEF8", BrnSound::Debug::RegistrationTag::Float, false, 0.0f, 0.0f, true, 50.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6C18u, "Collision", "Heaviest thing", "&flt_82F2CEFC", BrnSound::Debug::RegistrationTag::Float, false, 0.0f, 0.0f, true, 50.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6C44u, "Collision", "Scrape Info", "&dword_82FFB8E8", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6C60u, "Collision", "Scrapes Draw", "&dword_82FFB8EC", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6C78u, "Collision", "Material Filter", "&dword_82FFB8F8", BrnSound::Debug::RegistrationTag::S32, true, 0.0f, 25.0f, false, 0.0f, BrnSound::Debug::OptionsKind::Material },
    { 0x826D6C98u, "Collision", "Action Filter", "&dword_82FFB8FC", BrnSound::Debug::RegistrationTag::S32, true, 0.0f, 7.0f, false, 0.0f, BrnSound::Debug::OptionsKind::Action },
    { 0x826D6CB8u, "Collision", "Render Collisions", "&dword_82FFB8E4", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6CD0u, "Collision", "Orientation Spew", "&dword_82FFB90C", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6CE8u, "Collision", "Draw Orientation Box", "&dword_82FFB910", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6D00u, "Collision", "Spew Prop Resource IDs", "&dword_82FFB914", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6D20u, "Traffic", "Engine Info", "&dword_82FFB840", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6D3Cu, "Traffic", "Draw Attached", "&dword_82FFB844", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6D58u, "Traffic", "Management Info", "&dword_82FFB84C", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6D7Cu, "Passby", "Effect Info", "&dword_82FFB934", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6D98u, "Passby", "Verbose Info", "&dword_82FFB938", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6DB0u, "Passby", "Traffic Cull Info", "&dword_82FFB850", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6DCCu, "Passby", "State Info", "&dword_82FFB930", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6DE8u, "Passby", "Draw Spheres", "&byte_82FFB93C", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6E08u, "Horn", "Draw Sizes", "&byte_82FFB848", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6E20u, "Horn", "Draw Hooting", "&byte_82FFB849", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6E3Cu, "Passby", "Roll Info", "&unk_82FFB858", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6E58u, "Passby", "Roll Info Verbose", "&dword_82FFB85C", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6E78u, "Passbys&Reverb", "Spew Information", "&byte_82FFB8BE", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6E90u, "Passbys&Reverb", "Min Velocity Walls", "&flt_82F2CDF0", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 500.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6EBCu, "Passbys&Reverb", "Min Time Between", "&flt_82F2CDF4", BrnSound::Debug::RegistrationTag::Float, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6ED8u, "Passbys&Reverb", "Show Static", "&byte_82FFB8C8", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6EF0u, "Passbys&Reverb", "Min Velocity Spheres", "&flt_82F2CE0C", BrnSound::Debug::RegistrationTag::Float, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6F0Cu, "Passbys&Reverb", "Debug Reverb", "&byte_82FFB8C9", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6F28u, "Collision", "Crash Stream", "&unk_82FFB860", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6F44u, "Ambience", "Draw Information", "&byte_82FFB8BF", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6F64u, "Presentation", "Spew Messages", "&byte_82FFB8D0", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6F7Cu, "Presentation", "Spew GUI Messages", "&byte_82FFB8D3", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6F9Cu, "Streams", "Speed Threshold", "&flt_82F2CE14", BrnSound::Debug::RegistrationTag::Float, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6FB4u, "Streams", "Speed Crossfade Width", "&flt_82F2CE18", BrnSound::Debug::RegistrationTag::Float, true, 1.0f, 50.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6FE4u, "Streams", "Mute Music", "&byte_82FFB8D2", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D6FFCu, "Streams", "Music HPF open freq", "&flt_82F2CE70", BrnSound::Debug::RegistrationTag::Float, false, 0.0f, 0.0f, true, 100.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D7024u, "Streams", "Music HPF open time", "&flt_82F2CE74", BrnSound::Debug::RegistrationTag::Float, false, 0.0f, 0.0f, true, 0.1f, BrnSound::Debug::OptionsKind::None },
    { 0x826D704Cu, "Streams", "Music HPF close time", "&flt_82F2CE78", BrnSound::Debug::RegistrationTag::Float, false, 0.0f, 0.0f, true, 0.1f, BrnSound::Debug::OptionsKind::None },
    { 0x826D7074u, "Streams", "Music HPF Q", "&flt_82F2CE68", BrnSound::Debug::RegistrationTag::Float, false, 0.0f, 0.0f, true, 0.0099999998f, BrnSound::Debug::OptionsKind::None },
    { 0x826D709Cu, "Streams", "Music LPF Q", "&flt_82F2CE6C", BrnSound::Debug::RegistrationTag::Float, false, 0.0f, 0.0f, true, 0.0099999998f, BrnSound::Debug::OptionsKind::None },
    { 0x826D70C4u, "Streams", "Music HPF Order", "&flt_82F2CE60", BrnSound::Debug::RegistrationTag::Float, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D70DCu, "Streams", "Music LPF Order", "&flt_82F2CE64", BrnSound::Debug::RegistrationTag::Float, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D70F8u, "Emitters", "Debug", "&byte_82FFB8CB", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D7114u, "Deformation", "Spew Info", "&dword_82FFB8AC", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D713Cu, "Global", "Master Gain", "&flt_82F2CE94", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 50.0f, true, 0.25f, BrnSound::Debug::OptionsKind::None },
    { 0x826D7184u, "Global", "LF Shelf Gain", "&flt_82F2CE98", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 50.0f, true, 0.1f, BrnSound::Debug::OptionsKind::None },
    { 0x826D71C8u, "Global", "LF Shelf Freq", "&flt_82F2CE9C", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 120.0f, true, 2.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D7214u, "Global", "Limiter Thresh", "&flt_82FFB8D4", BrnSound::Debug::RegistrationTag::Float, true, -100.0f, 20.0f, true, 1.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D7260u, "Global", "Limiter Release", "&flt_82F2CEA8", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 1000.0f, true, 1.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D72A4u, "Global", "GainArray Centre", "&flt_82F2CEB0", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 50.0f, true, 0.02f, BrnSound::Debug::OptionsKind::None },
    { 0x826D72F0u, "Global", "GainArray Front", "&flt_82F2CEB4", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 50.0f, true, 0.02f, BrnSound::Debug::OptionsKind::None },
    { 0x826D7334u, "Global", "GainArray Rear", "&flt_82F2CEB8", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 50.0f, true, 0.02f, BrnSound::Debug::OptionsKind::None },
    { 0x826D7378u, "Global", "GainArray Sub", "&flt_82F2CEBC", BrnSound::Debug::RegistrationTag::Float, true, 0.0f, 50.0f, true, 0.02f, BrnSound::Debug::OptionsKind::None },
    { 0x826D73BCu, "Splice", "Audition", "&v0->gap1B8[396]", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D73D4u, "Splice", "Auto Cycle", "&v0->gap1B8[397]", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D73ECu, "Splice", "Bank", "&v0->field_348", BrnSound::Debug::RegistrationTag::S32, true, 0.0f, -1.0f, false, 0.0f, BrnSound::Debug::OptionsKind::SpliceBank },
    { 0x826D7410u, "Splice", "Index x1", "v0[1].gap4", BrnSound::Debug::RegistrationTag::S32, true, 0.0f, 512.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D7440u, "Splice", "Index x10", "&v0[1]", BrnSound::Debug::RegistrationTag::S32, true, 0.0f, 512.0f, true, 10.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D7484u, "Statistics", "Main Screen Turn On", "&byte_82FFB93D", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D74A0u, "Statistics", "Main Screen Reset", "&byte_82FFB93E", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D74B8u, "Statistics", "Draw Mixer Graph", "&byte_82FFB93F", BrnSound::Debug::RegistrationTag::Bool, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D74D4u, "Statistics", "Memory Dump", "&dword_82FFB814", BrnSound::Debug::RegistrationTag::S32, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
    { 0x826D74ECu, "Statistics", "Splice Stats Dump", "&dword_82FFB950", BrnSound::Debug::RegistrationTag::S32, false, 0.0f, 0.0f, false, 0.0f, BrnSound::Debug::OptionsKind::None },
};
static const ExpectedSite kConsoleSite =
{
    0x826893B0u, "", "Console", "this->meOutput",
    BrnSound::Debug::RegistrationTag::S32, true, 0.0f, 2.0f,
    false, 0.0f, BrnSound::Debug::OptionsKind::Console
};

static int sChecks = 0, sFailures = 0;
static void Check(bool b, const char* n)
{ ++sChecks; if (!b) { ++sFailures; std::printf("FAIL: %s\n", n); } }
static bool Near(f32 a, f32 b) { return std::fabs(a - b) < 0.00001f; }
static void CheckSite(bool b, const ExpectedSite& site, const char* field)
{
    char message[128];
    std::snprintf(message, sizeof(message), "ARTIST 0x%08X %s", site.muAddress, field);
    Check(b, message);
}

static const char* const kExpectedMaterialNames[25] =
{
    "Nothing", "PlayerCar", "AiCar", "TrafficCar", "World", "BodyPartSmall",
    "BodyPartLarge", "Mirrors", "GlassSmall", "GlassLarge", "Wheels", "Lights",
    "Crane", "Mixer", "Tipper", "NumberPlate", "Seats", "RoofRacks", "Extinguisher",
    "Ladder", "Body", "Suspension", "Exhaust", "TrafficCarMedium", "TrafficCarLarge"
};
static const char* const kExpectedActionNames[7] =
{
    "Collision", "Detach", "Hinging", "HingeOpen", "HingeClose", "Cracking", "Off"
};

static void CheckOptionList(const BrnSound::Debug::Registration& r,
                            BrnSound::Debug::OptionsKind kind)
{
    const CgsDev::DebugUI::StringList* p = r.mpOptions;
    Check(p != nullptr, "ARTIST option builder populated the list");
    if (!p) return;

    const char* const* names = nullptr;
    const s32* values = nullptr;
    static const s32 kMaterialValues[25] =
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 };
    static const s32 kActionValues[7] = { 1, 2, 3, 4, 5, 6, 0 };
    static const s32 kConsoleValues[3] = { 0, 1, 2 };
    static const char* const kConsoleNames[3] = { "Audio Only", "Game Only", "Audio AND Game" };
    static const s32 kSpliceValues[2] = { 0, 1 };
    static const char* const kSpliceNames[2] = { "test-bank-a", "test-bank-b" };
    size_t count = 0;
    switch (kind)
    {
    case BrnSound::Debug::OptionsKind::Console:
        count = 3; names = kConsoleNames; values = kConsoleValues; break;
    case BrnSound::Debug::OptionsKind::Material:
        count = 25; names = kExpectedMaterialNames; values = kMaterialValues; break;
    case BrnSound::Debug::OptionsKind::Action:
        count = 7; names = kExpectedActionNames; values = kActionValues; break;
    case BrnSound::Debug::OptionsKind::SpliceBank:
        count = 2; names = kSpliceNames; values = kSpliceValues; break;
    case BrnSound::Debug::OptionsKind::None:
        Check(false, "option list requested for a row without ARTIST options");
        return;
    }
    for (size_t i = 0; i < count; ++i)
    {
        Check(p[i].miValue == values[i], "ARTIST option value order");
        Check(p[i].mpcName && std::strcmp(p[i].mpcName, names[i]) == 0,
              "ARTIST option label order");
    }
    Check(p[count].mpcName == nullptr, "ARTIST option list has its null sentinel");
}

static void CheckRegistration(const BrnSound::Debug::Registration& actual,
                              const ExpectedSite& expected, bool bConsole = false)
{
    CheckSite(actual.mpValue != nullptr, expected, "registration has backing storage");
    CheckSite(actual.mTag == expected.meTag, expected, "registration type matches original wrapper");
    CheckSite(actual.mGroup == expected.mpcGroup && actual.mName == expected.mpcName,
              expected, "group/label matches original call order");

    f32 expectedMax = expected.mfMax;
    if (expected.meOptions == BrnSound::Debug::OptionsKind::SpliceBank)
        expectedMax = 1.0f; // the fixture supplies two loaded banks; original range is count-1
    CheckSite(actual.mbHasRange == expected.mbHasRange &&
              (!expected.mbHasRange || (Near(actual.mfMin, expected.mfMin) && Near(actual.mfMax, expectedMax))),
              expected, "range matches original calls");
    CheckSite(actual.mbHasStep == expected.mbHasStep &&
              (!expected.mbHasStep || Near(actual.mfStep, expected.mfStep)),
              expected, "step matches original calls");
    CheckSite((actual.mpOptions != nullptr) == (expected.meOptions != BrnSound::Debug::OptionsKind::None),
              expected, "options builder presence matches original call");
    if (expected.meOptions != BrnSound::Debug::OptionsKind::None)
        CheckOptionList(actual, expected.meOptions);
    if (bConsole)
        CheckSite(actual.mpCallback == &BrnSound::Debug::ConsoleChangeCallback && actual.mpUserData != nullptr,
                  expected, "Console callback and user data");
    else
        CheckSite(actual.mpCallback == nullptr && actual.mpUserData == nullptr,
                  expected, "no original callback at this row");
}

static void CheckArtistActivation(const BrnSound::Debug::DebugComponent& component,
                                  size_t firstCall, bool bConsoleIsFirst)
{
    const size_t expectedCount = sizeof(kArtistSites) / sizeof(kArtistSites[0]);
    const size_t offset = bConsoleIsFirst ? 1u : 0u;
    Check(component.mRegistrationCalls.size() >= firstCall + expectedCount + offset,
          "Sound activation records Console plus all 136 ARTIST rows");
    if (component.mRegistrationCalls.size() < firstCall + expectedCount + offset) return;

    if (bConsoleIsFirst)
    {
        CheckRegistration(component.mRegistrationCalls[firstCall], kConsoleSite, true);
        Check(component.mRegistrationCalls[firstCall].mpUserData == &component,
              "Console callback user data is the Sound component");
    }
    for (size_t i = 0; i < expectedCount; ++i)
        CheckRegistration(component.mRegistrationCalls[firstCall + i + offset], kArtistSites[i]);

    // ARTIST calls RegisterVariable twice for the same Clutch threshold cell.
    const size_t clutchFirst = firstCall + offset + 34u;
    const size_t clutchSecond = firstCall + offset + 35u;
    if (component.mRegistrationCalls.size() > clutchSecond)
        Check(std::strcmp(kArtistSites[34].mpcBacking, kArtistSites[35].mpcBacking) == 0 &&
              component.mRegistrationCalls[clutchFirst].mpValue ==
              component.mRegistrationCalls[clutchSecond].mpValue,
              "duplicate Clutch threshold sites retain shared storage");
}

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
    CgsSound::Playback::SplicerContent lContentA, lContentB;
    lContentA.mSpec.mName.mpcName = "test-bank-a";
    lContentB.mSpec.mName.mpcName = "test-bank-b";
    CgsSound::Playback::SpliceBankStatistics lBankA, lBankB;
    lBankA.muIndex = 0; lBankA.mpSpliceContent = &lContentA; lBankA.mpNext = &lBankB;
    lBankB.muIndex = 1; lBankB.mpSpliceContent = &lContentB; lBankB.mpNext = nullptr;
    CgsSound::Playback::SpliceBankStatistics::spHead = &lBankA;

    c.OnActivate();
    CheckArtistActivation(c, 0, true);
    Check(CgsSound::Logic::Cgs3dEffectControl::spDebugRenderingFunction ==
          &Debug::DebugComponent::Render3dControl,
          "OnActivate installs the original 3D-control callback");
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
    const size_t secondActivationStart = c.mRegistrationCalls.size();
    c.OnActivate();
    Check(c.mRegistrationCalls.size() - secondActivationStart == 136,
          "later activation replays 136 ARTIST rows without re-registering Console");
    CheckArtistActivation(c, secondActivationStart, false);
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
