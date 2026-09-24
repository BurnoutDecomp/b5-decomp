// FX-TAILS-B (crash parity 2026-09-24, item 6): the pass-by chain against the ARTIST machine code.
//
//   CgsSound::Utils::Slope::GetValue(f32, ECurveType)     0x826897F0  (the fsel clamp, then `fmadds`)
//   PassbyEffect::ChooseSampleId                           0x82689108
//   PassbyEffect::UpdatePosition / GetRelativeVelocityMag  0x826BF778 / 0x826BF828
//   PassbyEffect::Attach / UpdateParams / ProcessUpdate    0x826D5280 / 0x826D5068 / 0x826F99C0
//   PassbyEffect::Detach / Prepare / AttachController      0x826F9F10 / 0x826F9860 / 0x82689048
//   PassbyState::Attach / UpdateParams                     0x826D4A98 / 0x826D4B30
//   PassbyStateManager::UpdateParams                       0x826D4D00
//   PassbyStateManager::UpdateDynamicPropBys               0x826A0DD0
//   PassbyStateManager::DynamicPropByCache::Insert         0x826975C8
//   PassbyStateManager::Prepare / Release                  0x826F9748 / 0x826D4CC8
//
// run_fxtailsb_passby.py extracts the PRODUCTION bodies (and the constants they use) from
// CgsSoundUtils.cpp, BrnPassbyEffect.cpp, BrnPassbyState.cpp and BrnPassbyStateManager.cpp and
// compiles them as members of the fixture classes below, whose voice, mixer, states, manager,
// microphones and input buffer record what they are asked to do. CgsSoundUtils.h (Slope, Curve,
// DataPoint), passbybin.h (the schema-pinned layout) and BrnCommonTypes.h (Vector3, EntityId) are
// the real headers. A revision without a body fails the build of this fixture: every numeric
// check then counts as failed (the RED side of the fix is the parent commit, where none of the
// pass-by bodies exist).
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"
#include "GameSource/AttribSys/Generated/classes/passbybin.h"
#include "rw/math/fpu/scalar_operation.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

// ---- the harness ---------------------------------------------------------------------------------
static unsigned guAsserts = 0;
static std::vector<std::string> gaAssertTexts;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcExpression, const char*, int)
{
    ++guAsserts;
    gaAssertTexts.push_back(lpcExpression);
    return 0;
}
void* EndAssert() { return nullptr; }
} }
namespace CgsDev { namespace Log {
struct DebugPrintFixture
{
    template <typename T> DebugPrintFixture& operator<<(const T&) { return *this; }
};
DebugPrintFixture* gpDebugPrint = nullptr;
void WriteToLog(const char*) {}
} }

static std::vector<std::string> gaCalls;
static void Call(const std::string& lrText) { gaCalls.push_back(lrText); }

// ---- the AttribSys runtime the real passbybin.h needs ----------------------------------------------
static u8 gaDefaultArea[0x100];
static const void* gpLastChangeWithDefault = nullptr;
namespace Attrib {
Instance::Instance(Collection* lpCollection, void* lpOwner)
    : mpCollection(lpCollection), mpAttributeData(nullptr), mpOwner(lpOwner), muFlags(0) {}
Instance::~Instance() {}
Collection* Instance::ChangeWithDefault(RefSpec* lpRefSpec)
{
    gpLastChangeWithDefault = lpRefSpec;
    Call("ChangeWithDefault");
    return nullptr;
}
int Instance::GetClass() const { return 0; }
u64 Instance::GetCollection() const { return 0; }
void AssertOnClassCheck(int, int, u64) {}
void* DefaultDataArea(u32) { return gaDefaultArea; }
}

// ---- the engine surface the bodies reach -----------------------------------------------------------
namespace CgsSound { namespace Playback {
struct Name
{
    static u32 MakeHash(const char* lpcText)
    {
        u32 lu = 2166136261u;
        for (; *lpcText; ++lpcText)
            lu = (lu ^ static_cast<u8>(*lpcText)) * 16777619u;
        return lu;
    }
};
} }

namespace Nicotine {
class DMixIO
{
public:
    enum { DMX_VOL = 0, DMX_PITCH = 1, DMX_DEPTH = 2, DMX_AZIM = 3 };
    int maiInput[32] = {};
    int maiOutput[32][8] = {};
    bool mbInputWritten[32] = {};
    void SetDMixInput(int aiSlot, int aiValue) { maiInput[aiSlot] = aiValue; mbInputWritten[aiSlot] = true; }
    int GetDMixOutput(int aiSlot, int aiPreset) { return maiOutput[aiSlot][aiPreset]; }
};
}

namespace CgsDev {
enum PerfMonCpuPage { E_PAGE_FIXTURE = 0 };
namespace PerfMonCpu {
static s32 giAddedPage = -1;
s32 AddMonitor(const char* lpcName, PerfMonCpuPage lePage, bool lbMinimum, f32 lfBudget, bool lbScaled)
{
    giAddedPage = static_cast<s32>(lePage);
    Call(std::string("AddMonitor ") + lpcName + (lbMinimum ? " min" : " nomin") +
         (lfBudget == 1.0f ? " 1.0" : " ?") + (lbScaled ? " scaled" : " unscaled"));
    return 77;
}
void StartMonitor(s32 liHandle) { Call("StartMonitor " + std::to_string(liHandle)); }
void StopMonitor(s32 liHandle) { Call("StopMonitor " + std::to_string(liHandle)); }
}
}

namespace AttribSys { namespace Enums { namespace ePassbyTypes {
enum ePassbyTypes
{
    PassbyAzimuth = 0, PassbyPitch = 1, PassbyCutoff = 2, TrafficSmall = 3, TrafficMedium = 4,
    TrafficLarge = 5, LampPost = 6, Tree = 7, Bridge = 8, Tunnel = 9, Camera = 10, Misc = 11,
    Collision = 12, Overpass = 13, Warehouse = 14, Alley = 15, StaticMetal = 16,
    LargeOverheadObject = 17, PassbyBoostOffset = 18, MaxPassbyTypes = 19,
};
} } }

namespace CgsSound { namespace Logic {
struct Content
{
    bool mbCreated = false;
    bool mbLoaded = false;
    bool IsCreated() const { return mbCreated; }
    bool IsLoaded() const { return mbLoaded; }
    void Destruct() { Call("Content::Destruct"); mbCreated = false; }
};

struct Voice
{
    bool mbReady = true;
    bool mbPlaying = false;
    s32 miIdent = 0;
    s32 GetIdent() const { return miIdent; }
    void Construct(void* lpModule, u32 luUniqueId, u32 luFactory, u32 luSpec)
    {
        (void)lpModule;
        Call("Construct " + std::to_string(luUniqueId) + " " + std::to_string(luFactory) + " " +
             std::to_string(luSpec));
    }
    bool IsReady() const { return mbReady; }
    bool IsPlaying() const { return mbPlaying; }
    void Attach(s32 liSlot, const Content& arContent)
    {
        Call("Attach " + std::to_string(static_cast<u32>(liSlot)) + (arContent.mbCreated ? " bank" : " ?"));
    }
    void Connect(u32 luSend, u32 luIdent) { Call("Connect " + std::to_string(luSend) + " " + std::to_string(luIdent)); }
    s32 Play(s32 liSample) { Call("Play " + std::to_string(liSample)); return 0; }
    s32 Stop() { Call("Stop"); return 0; }
    std::vector<f32> mafGain;
    std::vector<f32> mafParam0;
    std::vector<f32> mafParam1;
    void SetGain(u32 luSend, f32 lfGain, const u32* lpName)
    {
        mafGain.push_back(lfGain);
        Call("SetGain " + std::to_string(luSend) + " " + std::to_string(lpName ? *lpName : 0u));
    }
    void SetParameter(s32 liIndex, f32 lfValue, const u32* lpName)
    {
        (liIndex == 0 ? mafParam0 : mafParam1).push_back(lfValue);
        Call("SetParameter " + std::to_string(liIndex) + " " + std::to_string(lpName ? *lpName : 0u));
    }
};

class StateManager;
class Module;

class State
{
public:
    virtual ~State() {}
    virtual void Attach(void* apvAttachment)
    {
        mpvLastAttachment = apvAttachment;
        mbIsAttached = true;
        ++miBaseAttaches;
    }
    virtual void UpdateParams(f32 afDeltaTime) { mfBaseUpdateDt = afDeltaTime; ++miBaseUpdates; }
    virtual bool Detach() { ++miDetaches; mbIsAttached = false; return true; }
    bool IsAttached() const { return mbIsAttached; }
    StateManager* GetStateManager() const { return mpStateManager; }

    StateManager* mpStateManager = nullptr;
    bool mbIsAttached = false;
    void* mpvLastAttachment = nullptr;
    int miBaseAttaches = 0;
    int miBaseUpdates = 0;
    int miDetaches = 0;
    f32 mfBaseUpdateDt = -1.0f;
};

class EffectBase
{
public:
    enum EAttachState
    {
        E_ATTACH_STATE_NONE = 0, E_ATTACH_STATE_WAITING_FOR_DATA = 1,
        E_ATTACH_STATE_PREPARING = 2, E_ATTACH_STATE_FINISHED = 3,
    };
    virtual ~EffectBase() {}
    bool Attach() { meDetachState = 0; ++mu16AttachCount; ++miBaseAttaches; return true; }
    bool Prepare(State* apState) { mpState = apState; ++miBasePrepares; return true; }
    State* GetStateBase() const { return mpState; }
    Module* GetLogicModule() const { return mpLogicModule; }
    Nicotine::DMixIO* GetDMixIOPtr() const { return mpDynamicMixIo; }
    s32 GetEffectID() const { return (miObjectId >> 4) & 0x7F; }
    EAttachState GetAttachState() const { return meAttachState; }
    u16 GetAttachCount() const { return mu16AttachCount; }
    f32 GetRWACMixerOutputValue(int aiSlot, int aiPreset)
    {
        miRwacSlot = aiSlot;
        miRwacPreset = aiPreset;
        return mfRwacValue;
    }
    void SetMixerInputValue(int aiSlot, int aiValue)
    {
        maiMixerInput[aiSlot] = aiValue;
        Call("SetMixerInputValue " + std::to_string(aiSlot) + " " + std::to_string(aiValue));
    }

    State* mpState = nullptr;
    Module* mpLogicModule = nullptr;
    Nicotine::DMixIO* mpDynamicMixIo = nullptr;
    u16 mu16AttachCount = 0;
    s32 miObjectId = 0;
    f32 mfDeltaTime = 0.0f;
    EAttachState meAttachState = E_ATTACH_STATE_NONE;
    s32 meDetachState = 7;
    int miBaseAttaches = 0;
    int miBasePrepares = 0;
    int miRwacSlot = -1;
    int miRwacPreset = -1;
    f32 mfRwacValue = 0.0f;
    int maiMixerInput[32] = {};
};

class Cgs3dEffectControl : public EffectBase
{
public:
    CgsSound::Utils::DataPoint<Vector3> GetEmitterPosition() const { return mEmitterPosition; }
    virtual void AttachEmitterPosition(const Vector3* apPosition) { mpEmitterPosition = apPosition; }
    CgsSound::Utils::DataPoint<Vector3> mEmitterPosition;
    const Vector3* mpEmitterPosition = nullptr;
};

struct MatrixFixture
{
    Vector3 mPos;
    const Vector3& Pos() const { return mPos; }
};

class MicrophoneSystem
{
public:
    enum EMicPositions { E_MIC_CAMERA = 0, E_MIC_PLAYER = 1 };
    enum EPlayer { E_PLAYER_1 = 0, E_PLAYER_2 = 1 };
    struct Microphone
    {
        MatrixFixture mMatrix;
        Vector3 mVelocity;
        const MatrixFixture& GetMicrophoneMatrix() const { return mMatrix; }
        const Vector3& GetVelocity() const { return mVelocity; }
    };
    Microphone maMicrophones[2][2];
    Microphone* GetMicrophone(EMicPositions aePosition, EPlayer aePlayer)
    {
        return &maMicrophones[aePosition][aePlayer];
    }
};

class Environment
{
public:
    MicrophoneSystem mMicrophoneSystem;
    StateManager* mapStateManagers[16] = {};
    MicrophoneSystem& GetMicrophoneSystem() { return mMicrophoneSystem; }
    StateManager* GetStateManager(s32 liSlot) const { return mapStateManagers[liSlot]; }
};

class Module
{
public:
    virtual ~Module() {}
    Environment mEnvironment;
    u32 muUniqueId = 4242;
    Environment& GetEnvironment() { return mEnvironment; }
    u32 GetUniqueId() { return muUniqueId; }
};

class StateManager
{
public:
    enum EPrepareState
    {
        E_PREPARE_NONE = 0, E_PREPARE_BEGIN = 1, E_PREPARE_UPDATING = 2, E_PREPARE_STATES = 3,
        E_PREPARE_FINISHED = 4, E_PREPARE_RELEASED = 5,
    };
    virtual ~StateManager() {}
    void UpdateParams(f32 afDeltaTime) { Call("StateManager::UpdateParams " + std::to_string(afDeltaTime)); }
    virtual State* GetFreeState(void* apvAttachment)
    {
        Call("GetFreeState");
        (void)apvAttachment;
        for (State* lpState : mapStates)
            if (!lpState->IsAttached())
                return lpState;
        return nullptr;
    }
    bool PrepareStates(s32 liMask, s32 liInstances, s32 liStart)
    {
        Call("PrepareStates " + std::to_string(liMask) + " " + std::to_string(liInstances) + " " +
             std::to_string(liStart));
        return mbStatesPrepared;
    }
    EPrepareState GetPrepareState() const { return mePrepareState; }
    Module* GetLogicModule() const { return mpLogicModule; }

    std::vector<State*> mapStates;
    bool mbStatesPrepared = false;
    EPrepareState mePrepareState = E_PREPARE_NONE;
    f32 mfCurrentTime = 0.0f;
    Module* mpLogicModule = nullptr;
};
} }

namespace BrnWorld { namespace RaceCarEntityModuleIO {
enum EActiveRaceCarIndex { E_ACTIVE_RACE_CAR_INDEX_INVALID = -1, E_ACTIVE_RACE_CAR_INDEX_0 = 0 };
struct RCEntityActiveRaceCarOutputInterface
{
    struct BoostOutputInfo { bool mbIsBoosting; };
    BoostOutputInfo maBoostOutputInfo[8] = {};
    bool mbIsPlayerCarActive = false;
    EActiveRaceCarIndex meIndex = E_ACTIVE_RACE_CAR_INDEX_0;
    bool IsPlayerCarActive() const { return mbIsPlayerCarActive; }
    EActiveRaceCarIndex GetPlayerActiveRaceCarIndex() const { return meIndex; }
    const BoostOutputInfo* GetBoostOutputInfoN(EActiveRaceCarIndex leIndex) const
    {
        return &maBoostOutputInfo[leIndex];
    }
};
} }

namespace BrnWorld {
struct PropEntityID
{
    EntityId mEntityId;
};
}

namespace BrnPhysics { namespace Props {
struct PropUpdateNotification
{
    Vector3 mPosition;
    Vector3 mLinearVelocity;
    Vector3 mAngularVelocity;
    BrnWorld::PropEntityID mEntityId;
    BrnWorld::PropEntityID GetEntityId() const
    {
        if ((mEntityId.mEntityId.muValue >> 24) != 3)
            ++guAsserts;
        return mEntityId;
    }
};
} }

namespace CgsModule {
template <typename T, int N>
struct EventQueue
{
    T maEvents[N];
    s32 miLength = 0;
    s32 GetLength() const { return miLength; }
    const T& GetEvent(s32 liIndex) const { return maEvents[liIndex]; }
};
}

namespace BrnSound { namespace Module { namespace Io {
struct PropUpdateNotificationQueue { int miOpaque; };
class LogicInputBuffer
{
public:
    BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface mVehicles;
    CgsModule::EventQueue<BrnPhysics::Props::PropUpdateNotification, 200> mNotifications;
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* GetVehicleInterface() const
    {
        return &mVehicles;
    }
    const PropUpdateNotificationQueue* GetPropUpdateNotificationQueue() const
    {
        return reinterpret_cast<const PropUpdateNotificationQueue*>(&mNotifications);
    }
    PropUpdateNotificationQueue* GetPropUpdateNotificationQueue()
    {
        ++guAsserts;   // the write-lock overload: reading through it is the wrong accessor
        return reinterpret_cast<PropUpdateNotificationQueue*>(&mNotifications);
    }
};
} } }

namespace Attrib { namespace Gen {
struct burnoutglobaldata
{
    u8 maRefSpecs[19][0x18] = {};
    void* mPassbyBins(u32 luIndex) { return maRefSpecs[luIndex]; }
};
} }

namespace BrnSound { namespace Module {
class SoundLogicModule : public CgsSound::Logic::Module
{
public:
    Attrib::Gen::burnoutglobaldata mGlobalData;
    Io::LogicInputBuffer mInput;
    Attrib::Gen::burnoutglobaldata& GetGlobalData() { return mGlobalData; }
    Io::LogicInputBuffer* GetBrnInputStructure() { return &mInput; }
};
} }

namespace BrnSound { namespace Logic {
struct ResourceRegistrar { enum EType { E_DATA = 0, E_ATTRIBSYS = 1 }; };

class GlobalStateManager : public CgsSound::Logic::StateManager
{
public:
    enum eSubmixVoice { E_SUBMIX_VOICE_COLLISION = 0, E_SUBMIX_VOICE_PASSBY = 1 };
    CgsSound::Logic::Voice maSubmixVoices[2];
    CgsSound::Logic::Voice& GetSubmixVoice(eSubmixVoice aeVoice) { return maSubmixVoices[aeVoice]; }
};

class BrnEffectObject : public CgsSound::Logic::EffectBase
{
public:
    virtual bool Detach() { Call("BrnEffectObject::Detach"); return mbBaseDetachResult; }
    bool mbBaseDetachResult = true;
};
} }

// ---- the pass-by classes, member for member what the bodies use -----------------------------------
namespace BrnSound { namespace Logic { namespace Passby {

static bool gbDiag = false;
static bool PassbyEffectDiag() { return gbDiag; }
static bool PassbySoundDiagEnabled() { return gbDiag; }
static void PassbySoundDiag(const char*) {}
static void PassbySoundDiagWaiting(bool) { Call("waiting"); }

struct Passby3DControl : public CgsSound::Logic::Cgs3dEffectControl {};

class PassbyStateManager : public CgsSound::Logic::StateManager
{
public:
    struct Passby
    {
        typedef AttribSys::Enums::ePassbyTypes::ePassbyTypes EePassbyTypes;
        Passby() {}
        Passby(Vector3 lStaticPos, f32 lfRelativeVelocityMagnitude, EePassbyTypes leType,
               bool lbSuppressBoostBys, f32 lfVolumeModifier);
        Vector3 mStaticPos;
        const CgsSound::Logic::Cgs3dEffectControl* mp3dControl = nullptr;
        f32 mfRelativeVelocityMagnitude = 0.0f;
        EePassbyTypes meType = AttribSys::Enums::ePassbyTypes::TrafficSmall;
        f32 mfVolumeModifier = 1.0f;
        bool mbSuppressBoostBys = false;
    };

    static const u32 KU_DYNAMIC_PROP_CACHE_SIZE = 32;
    struct DynamicPropByCache
    {
        struct Item
        {
            bool mbActive = false;
            f32 mfTimeStamp = 0.0f;
            EntityId mId = {0};
        };
        void Update(f32 lfCurrentTime);
        Item* Insert(f32 lfTimeStamp, const EntityId& lId);
        Item* Find(const EntityId& lId);
        Item maItems[KU_DYNAMIC_PROP_CACHE_SIZE];
    };

    static const u32 KU_MAX_PASSBY_POSTS = 8;
    static const u32 KU_NUMBER_OF_PASSBY_STATES = 8;

    bool PostPassby(const Passby& lrPassby);
    void UpdateParams(f32 lfTimeStep);
    void UpdateDynamicPropBys(f32 lfTimeStep);
    bool Prepare();
    bool Release();
    const CgsSound::Logic::Content& GetSplicerBank() const { return mSplicerBank; }
    void LoadAsset(const char* lpcBundle, const char* lpcName, ResourceRegistrar::EType leType)
    {
        Call(std::string("LoadAsset ") + lpcBundle + (lpcName ? " named" : " null") + " " +
             std::to_string(static_cast<int>(leType)));
    }

    Passby maPostedPassbys[KU_MAX_PASSBY_POSTS];
    u32 muPostedPassbyCount = 0;
    CgsSound::Logic::Content mSplicerBank;
    DynamicPropByCache mDynamicPropCache;
    s32 miCpuMonitor = 5;
};

class PassbyState : public CgsSound::Logic::State
{
public:
    virtual void Attach(void* apvAttachment) override;
    virtual void UpdateParams(f32 afDeltaTime) override;
    const PassbyStateManager::Passby& GetPassbyData() const { return mPassbyData; }
    PassbyStateManager::Passby mPassbyData;
    f32 mfTimeOutTimer = -1.0f;
};

class PassbyEffect : public BrnSound::Logic::BrnEffectObject
{
public:
    enum EPassbyLifetime { E_NOT_PLAYED = 0, E_HAS_PLAYED = 1, E_CULLED = 2 };
    enum ePrepareState { E_PREPARE_STATE_CONSTRUCT_VOICE = 0, E_PREPARE_STATE_CONNECT_VOICE = 1 };

    s32 GetController(s32 aiIndex);
    void AttachController(CgsSound::Logic::EffectBase* apController);
    bool Prepare(CgsSound::Logic::State* apState);
    bool Attach();
    void UpdateParams(f32 afDeltaTime);
    void ProcessUpdate();
    bool Detach();
    u32 ChooseSampleId(const PassbyStateManager::Passby& arPassby, bool abPlayerIsBoosting);
    const PassbyStateManager::Passby& GetPassbyData() const;
    bool UpdatePosition();
    f32 GetRelativeVelocityMag() const;

    static u32 smuSampleIndex;
    CgsSound::Logic::Voice mVoice;
    Attrib::Gen::passbybin mAttribs;
    Vector3 mPosition;
    EPassbyLifetime meLifetime = E_NOT_PLAYED;
    Passby3DControl* mpPassby3DControl = nullptr;
    ePrepareState mePrepareState = E_PREPARE_STATE_CONSTRUCT_VOICE;
    u32 muSampleId = 0;
    f32 mfPitchScale = -1.0f;
    f32 mfVolumeScale = -1.0f;
    bool mbPlayerIsBoosting = false;
    bool mbFirstUpdate = false;
};
u32 PassbyEffect::smuSampleIndex = 0;

} } }

// ---- the production bodies -------------------------------------------------------------------------
namespace CgsSound { namespace Utils {
#include "fxtailsb_passby_curve_bodies.inc"
} }
namespace BrnSound { namespace Logic { namespace Passby {
#include "fxtailsb_passby_effect_bodies.inc"
#include "fxtailsb_passby_state_bodies.inc"
#include "fxtailsb_passby_manager_bodies.inc"
bool PassbyStateManager::PostPassby(const Passby& lrPassby)
#include "fxtailsb_passby_post_body.inc"
} } }

using namespace BrnSound::Logic::Passby;
using CgsSound::Utils::Curve;
namespace ePT = AttribSys::Enums::ePassbyTypes;

// ---- checks ----------------------------------------------------------------------------------------
static int giChecks = 0;
static int giFailures = 0;
static void Check(bool lbCondition, const char* lpcLabel)
{
    ++giChecks;
    if (!lbCondition)
    {
        ++giFailures;
        std::printf("FAIL  %s\n", lpcLabel);
    }
}
static bool Same(f32 a, f32 b) { return std::memcmp(&a, &b, sizeof(f32)) == 0; }
static u32 Bits(f32 a) { u32 lu; std::memcpy(&lu, &a, 4); return lu; }
static f32 FromBits(u32 lu) { f32 lf; std::memcpy(&lf, &lu, 4); return lf; }
static bool Called(const std::string& lrText)
{
    for (const std::string& lr : gaCalls)
        if (lr == lrText)
            return true;
    return false;
}
static int IndexOf(const std::string& lrText)
{
    for (size_t i = 0; i < gaCalls.size(); ++i)
        if (gaCalls[i] == lrText)
            return static_cast<int>(i);
    return -1;
}
static Vector3 V(f32 x, f32 y, f32 z) { Vector3 l; l.SetZero(); l.x = x; l.y = y; l.z = z; return l; }
static const f32 KF_NAN = std::numeric_limits<f32>::quiet_NaN();
static u32 H(const char* lpc) { return CgsSound::Playback::Name::MakeHash(lpc); }

// A pass-by effect wired to a state, a manager, a module and a bin layout.
struct Rig
{
    BrnSound::Module::SoundLogicModule mModule;
    PassbyStateManager mManager;
    PassbyState mState;
    PassbyEffect mEffect;
    Passby3DControl mControl;
    Attrib::Gen::passbybin::_LayoutStruct mLayout;
    Nicotine::DMixIO mDmix;
    Rig()
    {
        std::memset(&mLayout, 0, sizeof(mLayout));
        mManager.mpLogicModule = &mModule;
        mState.mpStateManager = &mManager;
        mEffect.mpState = &mState;
        mEffect.mpLogicModule = &mModule;
        mEffect.mpPassby3DControl = &mControl;
        mEffect.mAttribs.mpAttributeData = &mLayout;
        mEffect.mpDynamicMixIo = &mDmix;
        mEffect.mfDeltaTime = 0.5f;
        mEffect.mPosition.SetZero();
    }
};

static void TestSlope()
{
    // (0 .. 80 mph-ish) -> (0.5 .. 1.5) through E_ONE_MINUS_EQPWR.
    CgsSound::Utils::SlopeParams lParams(0.0f, 80.0f, 0.5f, 1.5f);
    CgsSound::Utils::Slope lSlope(lParams);
    const f32 lfMid = lSlope.GetValue(40.0f, Curve::E_ONE_MINUS_EQPWR);
    Check(Same(lfMid, std::fmaf(1.0f, Curve::GetOutput(0.5f, Curve::E_ONE_MINUS_EQPWR), 0.5f)),
          "Slope::GetValue: (v - min) / (max - min), the curve, fmadds onto the output range");
    Check(Same(lSlope.GetValue(-5.0f, Curve::E_ONE_MINUS_EQPWR),
               std::fmaf(1.0f, Curve::GetOutput(0.0f, Curve::E_ONE_MINUS_EQPWR), 0.5f)),
          "Slope::GetValue: below the input range clamps to fraction 0");
    Check(Same(lSlope.GetValue(500.0f, Curve::E_ONE_MINUS_EQPWR),
               std::fmaf(1.0f, Curve::GetOutput(1.0f, Curve::E_ONE_MINUS_EQPWR), 0.5f)),
          "Slope::GetValue: above the input range clamps to fraction 1");
    Check(Same(lSlope.GetValue(KF_NAN, Curve::E_ONE_MINUS_EQPWR),
               std::fmaf(1.0f, Curve::GetOutput(1.0f, Curve::E_ONE_MINUS_EQPWR), 0.5f)),
          "Slope::GetValue: a NaN input takes the fsel pair's 1.0 (the curve's end)");
    Check(Same(lSlope.GetValue(20.0f, Curve::E_LINEAR), std::fmaf(1.0f, Curve::GetOutput(0.25f, Curve::E_LINEAR), 0.5f)),
          "Slope::GetValue: the curve argument reaches GetOutput (E_LINEAR)");
    // E_ONE_MINUS_EQPWR maps a NaN and 1.0 to the same entry, E_LINEAR does not: the NaN -> 1.0
    // polarity of the fsel pair is visible here.
    Check(Same(lSlope.GetValue(KF_NAN, Curve::E_LINEAR), 1.5f),
          "Slope::GetValue: a NaN input clamps to fraction 1.0 (E_LINEAR -> the maximum output, not NaN)");

    // One rounding: find an input where fmaf and a separate multiply-add disagree.
    CgsSound::Utils::SlopeParams lOdd(0.0f, 80.0f, 0.3f, 1.7f);
    CgsSound::Utils::Slope lOddSlope(lOdd);
    int liFound = 0;
    int liMatches = 0;
    for (int i = 1; i < 8000 && liFound < 40; ++i)
    {
        const f32 lfInput = static_cast<f32>(i) * 0.01f;
        const f32 lfFraction = rw::math::fpu::Clamp((lfInput - 0.0f) / (80.0f - 0.0f), 0.0f, 1.0f);
        const f32 lfOut = Curve::GetOutput(lfFraction, Curve::E_ONE_MINUS_EQPWR);
        volatile f32 lfProduct = (1.7f - 0.3f) * lfOut;
        const f32 lfSplit = lfProduct + 0.3f;
        const f32 lfFused = std::fmaf(1.7f - 0.3f, lfOut, 0.3f);
        if (Same(lfSplit, lfFused))
            continue;
        ++liFound;
        if (Same(lOddSlope.GetValue(lfInput, Curve::E_ONE_MINUS_EQPWR), lfFused))
            ++liMatches;
    }
    Check(liFound > 0 && liMatches == liFound,
          "Slope::GetValue: fused like the console's fmadds on every input where fused and split differ");
}

static void TestChooseSampleId()
{
    Rig r;
    r.mLayout.mFirstPassBy = 10;
    r.mLayout.mLastPassBy = 12;
    r.mLayout.mFirstBoostPassBy = 20;
    r.mLayout.mLastBoostPassBy = 21;
    PassbyEffect::smuSampleIndex = 0;
    const PassbyStateManager::Passby lPassby;
    const u32 a = r.mEffect.ChooseSampleId(lPassby, false);
    const u32 b = r.mEffect.ChooseSampleId(lPassby, false);
    const u32 c = r.mEffect.ChooseSampleId(lPassby, false);
    const u32 d = r.mEffect.ChooseSampleId(lPassby, false);
    Check(a == 10 && b == 11 && c == 12 && d == 10,
          "ChooseSampleId: round robin over mFirstPassBy..mLastPassBy on the shared cursor");
    Check(PassbyEffect::smuSampleIndex == 4, "ChooseSampleId: the cursor advances once per pick");
    const u32 e = r.mEffect.ChooseSampleId(lPassby, true);   // cursor 4 -> 4 % 2 = 0
    const u32 f = r.mEffect.ChooseSampleId(lPassby, true);
    Check(e == 20 && f == 21, "ChooseSampleId: boosting picks mFirstBoostPassBy..mLastBoostPassBy");

    r.mLayout.mFirstPassBy = 30;
    r.mLayout.mLastPassBy = 29;                               // count = 0
    PassbyEffect::smuSampleIndex = 7;
    const u32 g = r.mEffect.ChooseSampleId(lPassby, false);
    Check(g == 30 && PassbyEffect::smuSampleIndex == 7,
          "ChooseSampleId: an empty range (last = first - 1) returns first and leaves the cursor");

    r.mLayout.mFirstPassBy = 0x10005;
    r.mLayout.mLastPassBy = 0x10006;                           // count 2, first16 = 5
    PassbyEffect::smuSampleIndex = 3;
    const u32 h = r.mEffect.ChooseSampleId(lPassby, false);
    Check(h == 6, "ChooseSampleId: the first index is clrlwi'd to 16 bits before the add (3 % 2 + 5)");
}

static void TestUpdatePosition()
{
    Rig r;
    PassbyStateManager::Passby& lrPassby = r.mState.mPassbyData;
    lrPassby.mStaticPos = V(1.0f, 2.0f, 3.0f);
    lrPassby.mp3dControl = nullptr;
    Check(r.mEffect.UpdatePosition() && Same(r.mEffect.mPosition.x, 1.0f) && Same(r.mEffect.mPosition.z, 3.0f),
          "UpdatePosition: no control -> the record's static position, true");

    Passby3DControl lSource;
    lSource.mEmitterPosition.Update(V(7.0f, 8.0f, 9.0f));
    lSource.meAttachState = CgsSound::Logic::EffectBase::E_ATTACH_STATE_PREPARING;
    lSource.mu16AttachCount = 4;
    r.mEffect.mu16AttachCount = 4;
    lrPassby.mp3dControl = &lSource;
    r.mEffect.mPosition = V(0.0f, 0.0f, 0.0f);
    Check(!r.mEffect.UpdatePosition() && Same(r.mEffect.mPosition.x, 0.0f),
          "UpdatePosition: a control not FINISHED -> false, position kept");
    lSource.meAttachState = CgsSound::Logic::EffectBase::E_ATTACH_STATE_FINISHED;
    lSource.mu16AttachCount = 5;
    Check(!r.mEffect.UpdatePosition(), "UpdatePosition: the control's attach count moved on -> false");
    lSource.mu16AttachCount = 4;
    Check(r.mEffect.UpdatePosition() && Same(r.mEffect.mPosition.x, 7.0f) && Same(r.mEffect.mPosition.y, 8.0f),
          "UpdatePosition: same attach -> the control's current emitter position, true");
}

static void TestRelativeVelocity()
{
    Rig r;
    r.mEffect.mPosition = V(10.0f, 0.0f, 0.0f);
    r.mControl.mEmitterPosition.Update(V(9.0f, 0.0f, 0.0f));
    r.mModule.mEnvironment.mMicrophoneSystem.maMicrophones[1][0].mVelocity = V(0.0f, 0.0f, 1.0f);
    r.mModule.mEnvironment.mMicrophoneSystem.maMicrophones[0][0].mVelocity = V(50.0f, 0.0f, 0.0f);
    r.mEffect.mfDeltaTime = 0.5f;
    // source velocity (10 - 9) / 0.5 = 2 along x, minus the PLAYER microphone's (0, 0, 1)
    Check(Same(r.mEffect.GetRelativeVelocityMag(), std::sqrt(5.0f)),
          "GetRelativeVelocityMag: |(pos - control pos) / dt - player-microphone velocity|");
    r.mModule.mEnvironment.mMicrophoneSystem.maMicrophones[1][0].mVelocity = V(2.0f, 0.0f, 0.0f);
    Check(Same(r.mEffect.GetRelativeVelocityMag(), 0.0f), "GetRelativeVelocityMag: a zero vector -> exactly 0");
}

static void TestEffectUpdateParams()
{
    {
        Rig r;
        r.mLayout.VelocityThreshold_Min = 5.0f;
        r.mLayout.PitchCurveNormal.x = 0.8f;  r.mLayout.PitchCurveNormal.y = 1.2f;
        r.mLayout.PitchCurveBoost.x = 0.9f;   r.mLayout.PitchCurveBoost.y = 1.6f;
        r.mLayout.VolumeNormal.x = 0.25f;     r.mLayout.VolumeNormal.y = 1.0f;
        r.mLayout.VolumeBoost.x = 0.5f;       r.mLayout.VolumeBoost.y = 2.0f;
        r.mState.mPassbyData.mp3dControl = nullptr;
        r.mState.mPassbyData.mStaticPos = V(10.0f, 0.0f, 0.0f);
        r.mControl.mEmitterPosition.Update(V(10.0f, 0.0f, 0.0f));
        r.mModule.mEnvironment.mMicrophoneSystem.maMicrophones[1][0].mVelocity = V(-30.0f, 0.0f, 0.0f);
        r.mEffect.mVoice.mbPlaying = true;
        r.mEffect.meLifetime = PassbyEffect::E_NOT_PLAYED;
        r.mEffect.UpdateParams(0.016f);
        const f32 lfSpeed = 30.0f;
        CgsSound::Utils::Slope lPitch(CgsSound::Utils::SlopeParams(0.0f, 80.0f, 0.8f, 1.2f));
        CgsSound::Utils::Slope lVolume(CgsSound::Utils::SlopeParams(0.0f, 80.0f, 0.25f, 1.0f));
        Check(Same(r.mEffect.mfPitchScale, lPitch.GetValue(lfSpeed, Curve::E_ONE_MINUS_EQPWR)),
              "UpdateParams: mfPitchScale = Slope(0, 80, PitchCurveNormal).GetValue(speed, E_ONE_MINUS_EQPWR)");
        Check(Same(r.mEffect.mfVolumeScale, lVolume.GetValue(lfSpeed, Curve::E_ONE_MINUS_EQPWR)),
              "UpdateParams: mfVolumeScale = Slope(0, 80, VolumeNormal).GetValue(speed, E_ONE_MINUS_EQPWR)");
        Check(r.mEffect.meLifetime == PassbyEffect::E_NOT_PLAYED && r.mEffect.mVoice.mafGain.empty(),
              "UpdateParams: a speed above VelocityThreshold_Min does not cull");

        r.mEffect.mbPlayerIsBoosting = true;
        r.mEffect.UpdateParams(0.016f);
        CgsSound::Utils::Slope lPitchB(CgsSound::Utils::SlopeParams(0.0f, 80.0f, 0.9f, 1.6f));
        CgsSound::Utils::Slope lVolumeB(CgsSound::Utils::SlopeParams(0.0f, 80.0f, 0.5f, 2.0f));
        Check(Same(r.mEffect.mfPitchScale, lPitchB.GetValue(lfSpeed, Curve::E_ONE_MINUS_EQPWR)) &&
                  Same(r.mEffect.mfVolumeScale, lVolumeB.GetValue(lfSpeed, Curve::E_ONE_MINUS_EQPWR)),
              "UpdateParams: boosting reads PitchCurveBoost / VolumeBoost");

        r.mModule.mEnvironment.mMicrophoneSystem.maMicrophones[1][0].mVelocity = V(-3.0f, 0.0f, 0.0f);
        r.mEffect.mVoice.mbPlaying = false;
        r.mEffect.UpdateParams(0.016f);
        Check(r.mEffect.meLifetime == PassbyEffect::E_NOT_PLAYED,
              "UpdateParams: below the threshold but not playing -> no cull");
        r.mEffect.mVoice.mbPlaying = true;
        gaCalls.clear();
        r.mEffect.UpdateParams(0.016f);
        Check(r.mEffect.meLifetime == PassbyEffect::E_CULLED && r.mEffect.mVoice.mafGain.size() == 1 &&
                  Same(r.mEffect.mVoice.mafGain[0], 0.0f) && Called("SetGain 0 " + std::to_string(H("Send01"))),
              "UpdateParams: below VelocityThreshold_Min while playing -> SetGain(0, 0.0, Send01), E_CULLED");
        r.mEffect.UpdateParams(0.016f);
        Check(r.mEffect.mVoice.mafGain.size() == 1, "UpdateParams: an already culled pass-by is not culled again");
    }
    {
        Rig r;
        r.mLayout.VelocityThreshold_Min = 5.0f;
        r.mLayout.PitchCurveNormal.x = 0.8f;  r.mLayout.PitchCurveNormal.y = 1.2f;
        r.mState.mPassbyData.mp3dControl = nullptr;
        r.mState.mPassbyData.mStaticPos = V(10.0f, 0.0f, 0.0f);
        r.mControl.mEmitterPosition.Update(V(10.0f, 0.0f, 0.0f));
        r.mEffect.mfDeltaTime = 0.0f;                         // 0 * (1 / 0) -> NaN speed
        r.mEffect.mVoice.mbPlaying = true;
        r.mEffect.UpdateParams(0.016f);
        CgsSound::Utils::Slope lPitch(CgsSound::Utils::SlopeParams(0.0f, 80.0f, 0.8f, 1.2f));
        Check(r.mEffect.meLifetime == PassbyEffect::E_NOT_PLAYED &&
                  Same(r.mEffect.mfPitchScale, lPitch.GetValue(80.0f, Curve::E_ONE_MINUS_EQPWR)),
              "UpdateParams: a NaN speed neither culls (`fcmpu ; bge`) nor breaks the slope (fraction 1)");
    }
    {
        Rig r;
        Passby3DControl lSource;
        lSource.meAttachState = CgsSound::Logic::EffectBase::E_ATTACH_STATE_NONE;
        r.mState.mPassbyData.mp3dControl = &lSource;
        r.mEffect.UpdateParams(0.016f);
        Check(Same(r.mEffect.mfPitchScale, -1.0f) && Same(r.mEffect.mfVolumeScale, -1.0f),
              "UpdateParams: the source let go (UpdatePosition false) -> nothing updated");
    }
}

static void TestProcessUpdate()
{
    {
        Rig r;
        r.mState.mPassbyData.meType = ePT::TrafficLarge;
        r.mState.mPassbyData.mfVolumeModifier = 2.5f;
        r.mEffect.mVoice.mbPlaying = true;
        r.mEffect.meLifetime = PassbyEffect::E_NOT_PLAYED;
        r.mEffect.mfVolumeScale = 0.75f;
        r.mEffect.mfPitchScale = 1.25f;
        r.mEffect.mfRwacValue = 0.5f;
        r.mDmix.maiOutput[0][3] = 9000;     // azimuth
        r.mDmix.maiOutput[1][1] = 5000;     // pitch
        r.mDmix.maiInput[0] = 123;
        gaCalls.clear();
        r.mEffect.ProcessUpdate();
        Check(r.mDmix.mbInputWritten[0] && r.mDmix.maiInput[0] == 0, "ProcessUpdate: mixer input 0 zeroed first");
        Check(r.mEffect.meLifetime == PassbyEffect::E_HAS_PLAYED, "ProcessUpdate: playing -> E_HAS_PLAYED");
        Check(r.mEffect.miRwacSlot == ePT::TrafficLarge && r.mEffect.miRwacPreset == 0,
              "ProcessUpdate: the gain reads mixer output (meType, DMX_VOL)");
        const f32 lfGain = 0.75f * 0.5f * 2.5f;
        Check(r.mEffect.mVoice.mafGain.size() == 1 && Same(r.mEffect.mVoice.mafGain[0], lfGain),
              "ProcessUpdate: gain = mfVolumeScale * mixer volume * mfVolumeModifier");
        const f32 lfPitch = 1.25f * (5000.0f * FromBits(0x39800000u));
        const f32 lfAzimuth = 9000.0f * FromBits(0x3BB400B4u);
        Check(r.mEffect.mVoice.mafParam0.size() == 1 && Same(r.mEffect.mVoice.mafParam0[0], lfPitch),
              "ProcessUpdate: pitch = mfPitchScale * GetDMixOutput(1, 1) * flt_820AA8F4");
        Check(r.mEffect.mVoice.mafParam1.size() == 1 && Same(r.mEffect.mVoice.mafParam1[0], lfAzimuth),
              "ProcessUpdate: azimuth = GetDMixOutput(0, 3) * flt_820AA8EC (0x3BB400B4)");
        const int liGain = IndexOf("SetGain 0 " + std::to_string(H("Send01")));
        const int liPitch = IndexOf("SetParameter 0 " + std::to_string(H("~SplicerPlayerVoice::Pitch~")));
        const int liAzimuth = IndexOf("SetParameter 1 " + std::to_string(H("~SplicerPlayerVoice::Azimuth~")));
        Check(liGain >= 0 && liPitch > liGain && liAzimuth > liPitch,
              "ProcessUpdate: SetGain(0, Send01), then parameter 0 (Pitch), then parameter 1 (Azimuth)");
        r.mEffect.ProcessUpdate();
        Check(r.mEffect.meLifetime == PassbyEffect::E_HAS_PLAYED && r.mState.miDetaches == 0,
              "ProcessUpdate: a playing voice never detaches the state");
    }
    {
        Rig r;
        r.mState.mPassbyData.meType = ePT::Collision;
        r.mState.mPassbyData.mfRelativeVelocityMagnitude = 30.0f;   // clamps to 15
        r.mState.mPassbyData.mfVolumeModifier = 1.0f;
        r.mEffect.mVoice.mbPlaying = true;
        r.mEffect.mfVolumeScale = 1.0f;
        r.mEffect.mfPitchScale = 1.0f;
        r.mDmix.maiOutput[0][3] = 9000;
        r.mDmix.maiOutput[1][1] = 4096;
        r.mEffect.ProcessUpdate();
        const f32 lfBend = std::fmaf(15.0f * FromBits(0x3D888889u), FromBits(0x3ECCCCCEu), FromBits(0x3F4CCCCDu));
        Check(!r.mEffect.mVoice.mafParam1.empty() && Same(r.mEffect.mVoice.mafParam1[0], 0.0f),
              "ProcessUpdate: a Collision pass-by has azimuth 0");
        Check(!r.mEffect.mVoice.mafParam0.empty() && Same(r.mEffect.mVoice.mafParam0[0], lfBend * 1.0f),
              "ProcessUpdate: Collision pitch = fmadds(min(v, 15) * (1/15), 0x3ECCCCCE, 0.8) * pitch");

        r.mState.mPassbyData.mfRelativeVelocityMagnitude = 6.0f;
        r.mEffect.ProcessUpdate();
        const f32 lfBend6 = std::fmaf(6.0f * FromBits(0x3D888889u), FromBits(0x3ECCCCCEu), FromBits(0x3F4CCCCDu));
        Check(r.mEffect.mVoice.mafParam0.size() == 2 && Same(r.mEffect.mVoice.mafParam0[1], lfBend6),
              "ProcessUpdate: Collision below 15 uses the speed itself");
        r.mState.mPassbyData.mfRelativeVelocityMagnitude = KF_NAN;
        r.mEffect.ProcessUpdate();
        Check(r.mEffect.mVoice.mafParam0.size() == 3 && std::isnan(r.mEffect.mVoice.mafParam0[2]),
              "ProcessUpdate: Collision with a NaN speed keeps the NaN (`fsubs ; fsel` picks the speed)");

        // Every speed 0 .. 16 in 1/128 steps: the image constants, one rounding (fmadds). The sweep
        // must contain speeds where a split multiply-add or a 0.4f constant would differ.
        int liDiscriminating = 0;
        int liWrong = 0;
        for (int i = 0; i <= 16 * 128; ++i)
        {
            const f32 lfSpeed = static_cast<f32>(i) / 128.0f;
            r.mState.mPassbyData.mfRelativeVelocityMagnitude = lfSpeed;
            r.mEffect.mVoice.mafParam0.clear();
            r.mEffect.ProcessUpdate();
            const f32 lfClamped = (lfSpeed - 15.0f >= 0.0f) ? 15.0f : lfSpeed;
            const f32 lfScaled = lfClamped * FromBits(0x3D888889u);
            const f32 lfFused = std::fmaf(lfScaled, FromBits(0x3ECCCCCEu), FromBits(0x3F4CCCCDu));
            volatile f32 lfProduct = lfScaled * FromBits(0x3ECCCCCEu);
            volatile f32 lfProduct04 = lfScaled * 0.4f;
            if (!Same(lfProduct + FromBits(0x3F4CCCCDu), lfFused) || !Same(lfProduct04 + 0.8f, lfFused))
                ++liDiscriminating;
            if (r.mEffect.mVoice.mafParam0.size() != 1 || !Same(r.mEffect.mVoice.mafParam0[0], lfFused * 1.0f))
                ++liWrong;
        }
        Check(liDiscriminating > 0 && liWrong == 0,
              "ProcessUpdate: Collision pitch is the fused image-constant bend at every speed 0..16 (step 1/128)");
    }
    {
        Rig r;
        r.mEffect.mpDynamicMixIo = nullptr;
        r.mState.mPassbyData.meType = ePT::TrafficSmall;
        r.mEffect.mVoice.mbPlaying = true;
        r.mEffect.mfPitchScale = 1.0f;
        r.mEffect.ProcessUpdate();
        Check(!r.mEffect.mVoice.mafParam0.empty() && Same(r.mEffect.mVoice.mafParam0[0], 0.0f) &&
                  Same(r.mEffect.mVoice.mafParam1[0], 0.0f),
              "ProcessUpdate: no mixer endpoint -> pitch and azimuth 0.0");
    }
    {
        Rig r;
        r.mEffect.mVoice.mbPlaying = false;
        r.mEffect.meLifetime = PassbyEffect::E_NOT_PLAYED;
        r.mEffect.ProcessUpdate();
        Check(r.mState.miDetaches == 0 && r.mEffect.mVoice.mafGain.empty(),
              "ProcessUpdate: not playing and never played -> wait (no detach)");
        r.mEffect.meLifetime = PassbyEffect::E_HAS_PLAYED;
        r.mEffect.ProcessUpdate();
        Check(r.mState.miDetaches == 1, "ProcessUpdate: finished after playing -> the state detaches");
        r.mEffect.meLifetime = PassbyEffect::E_CULLED;
        r.mEffect.ProcessUpdate();
        Check(r.mState.miDetaches == 2, "ProcessUpdate: finished after a cull -> the state detaches");
    }
}

static void TestAttach()
{
    const s32 kaDucking[19] = { 0, 0, 0, 15000, 15000, 15000, 8000, 8000, 32767, 32767,
                                8000, 8000, 32767, 32767, 32767, 20000, 8000, 20000, 0 };   // 0x82F2CF20..
    {
        Rig r;
        r.mLayout.mFirstPassBy = 4;  r.mLayout.mLastPassBy = 4;
        r.mLayout.mFirstBoostPassBy = 9;  r.mLayout.mLastBoostPassBy = 9;
        r.mState.mPassbyData.meType = ePT::TrafficMedium;
        r.mState.mPassbyData.mp3dControl = nullptr;
        r.mState.mPassbyData.mStaticPos = V(3.0f, 4.0f, 5.0f);
        r.mState.mPassbyData.mbSuppressBoostBys = false;
        r.mModule.mInput.mVehicles.mbIsPlayerCarActive = true;
        r.mModule.mInput.mVehicles.meIndex = BrnWorld::RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_INDEX_0;
        r.mModule.mInput.mVehicles.maBoostOutputInfo[0].mbIsBoosting = true;
        r.mEffect.mu16AttachCount = 6;
        PassbyEffect::smuSampleIndex = 0;
        gaCalls.clear();
        const bool lb = r.mEffect.Attach();
        Check(lb && r.mEffect.miBaseAttaches == 1 && r.mEffect.meDetachState == 0 && r.mEffect.mu16AttachCount == 7,
              "Attach: EffectBase::Attach first (detach state 0, ++attach count)");
        Check(gpLastChangeWithDefault == r.mModule.mGlobalData.maRefSpecs[ePT::TrafficMedium],
              "Attach: mAttribs.ChangeWithDefault(burnoutglobaldata.mPassbyBins(meType))");
        Check(r.mControl.mpEmitterPosition == &r.mEffect.mPosition && Same(r.mEffect.mPosition.y, 4.0f),
              "Attach: the 3D control follows &mPosition, already at the static position");
        Check(r.mEffect.mbPlayerIsBoosting && r.mEffect.muSampleId == 9 && Called("Play 9"),
              "Attach: the player boosting -> the boost range; Play(muSampleId)");
        Check(r.mEffect.mVoice.mafGain.size() == 1 && Same(r.mEffect.mVoice.mafGain[0], 0.0f) &&
                  IndexOf("SetGain 0 " + std::to_string(H("Send01"))) > IndexOf("Play 9"),
              "Attach: the voice starts silent -- SetGain(0, 0.0, Send01) after Play");
        Check(r.mEffect.meLifetime == PassbyEffect::E_NOT_PLAYED && Same(r.mEffect.mfPitchScale, 1.0f) &&
                  Same(r.mEffect.mfVolumeScale, 1.0f) && r.mEffect.mbFirstUpdate,
              "Attach: E_NOT_PLAYED, both scales 1.0, mbFirstUpdate");
        Check(r.mEffect.maiMixerInput[0] == 0x7FFF && r.mEffect.maiMixerInput[1] == 15000,
              "Attach: mixer input 0 = 0x7FFF, input 1 = KAI_PASSBY_DUCKING_ARRAY[TrafficMedium] (15000)");
    }
    {
        int liRight = 0;
        for (int liType = 0; liType < 18; ++liType)
        {
            Rig r;
            r.mState.mPassbyData.meType = static_cast<ePT::ePassbyTypes>(liType);
            r.mEffect.Attach();
            if (r.mEffect.maiMixerInput[1] == kaDucking[liType])
                ++liRight;
        }
        Check(liRight == 18, "Attach: the ducking level of every type matches the image words 0x82F2CF20..");
    }
    {
        Rig r;
        r.mLayout.mFirstPassBy = 4;  r.mLayout.mLastPassBy = 4;
        r.mLayout.mFirstBoostPassBy = 9;  r.mLayout.mLastBoostPassBy = 9;
        r.mState.mPassbyData.mbSuppressBoostBys = true;
        r.mModule.mInput.mVehicles.mbIsPlayerCarActive = true;
        r.mModule.mInput.mVehicles.maBoostOutputInfo[0].mbIsBoosting = true;
        r.mEffect.Attach();
        Check(!r.mEffect.mbPlayerIsBoosting && r.mEffect.muSampleId == 4,
              "Attach: mbSuppressBoostBys -> the normal range even while the player boosts");
        Rig s;
        s.mLayout.mFirstPassBy = 4;  s.mLayout.mLastPassBy = 4;
        s.mLayout.mFirstBoostPassBy = 9;  s.mLayout.mLastBoostPassBy = 9;
        s.mModule.mInput.mVehicles.mbIsPlayerCarActive = false;
        s.mModule.mInput.mVehicles.maBoostOutputInfo[0].mbIsBoosting = true;
        s.mEffect.Attach();
        Check(!s.mEffect.mbPlayerIsBoosting && s.mEffect.muSampleId == 4,
              "Attach: no active player car -> not boosting");
    }
    {
        Rig r;
        Passby3DControl lSource;
        lSource.meAttachState = CgsSound::Logic::EffectBase::E_ATTACH_STATE_FINISHED;
        lSource.mu16AttachCount = 42;
        lSource.mEmitterPosition.Update(V(11.0f, 12.0f, 13.0f));
        r.mState.mPassbyData.mp3dControl = &lSource;
        r.mEffect.mu16AttachCount = 1;
        r.mEffect.Attach();
        Check(r.mEffect.mu16AttachCount == 42 && Same(r.mEffect.mPosition.x, 11.0f),
              "Attach: a FINISHED source control lends its attach count, so UpdatePosition follows it");
    }
    {
        Rig r;
        Passby3DControl lSource;
        lSource.meAttachState = CgsSound::Logic::EffectBase::E_ATTACH_STATE_NONE;
        r.mState.mPassbyData.mp3dControl = &lSource;
        gaCalls.clear();
        const bool lb = r.mEffect.Attach();
        Check(lb && r.mEffect.meLifetime == PassbyEffect::E_CULLED && !Called("Play 0") &&
                  r.mControl.mpEmitterPosition == nullptr,
              "Attach: a source that already let go -> E_CULLED, true, nothing played");
    }
}

static void TestDetachPrepareController()
{
    {
        Rig r;
        r.mEffect.maiMixerInput[1] = 99;
        gaCalls.clear();
        Check(r.mEffect.Detach() && IndexOf("BrnEffectObject::Detach") == 0 && IndexOf("Stop") == 1 &&
                  r.mEffect.maiMixerInput[1] == 0,
              "Detach: BrnEffectObject::Detach, Voice::Stop, mixer input 1 = 0, true");
        r.mEffect.mbBaseDetachResult = false;
        gaCalls.clear();
        Check(!r.mEffect.Detach() && !Called("Stop"), "Detach: the base still detaching -> false, voice untouched");
    }
    {
        Rig r;
        Check(r.mEffect.GetController(0) == 0 && r.mEffect.GetController(1) == -1,
              "GetController: controller 0 is effect 0, the rest -1");
        Passby3DControl lControl;
        lControl.miObjectId = 0x40000;
        r.mEffect.mpPassby3DControl = nullptr;
        r.mEffect.AttachController(&lControl);
        Check(r.mEffect.mpPassby3DControl == &lControl, "AttachController: effect id 0 -> mpPassby3DControl");
        Passby3DControl lOther;
        lOther.miObjectId = 0x40010;
        const unsigned luAsserts = guAsserts;
        r.mEffect.AttachController(&lOther);
        Check(r.mEffect.mpPassby3DControl == &lControl && guAsserts == luAsserts + 1,
              "AttachController: another effect id asserts and is not attached");
    }
    {
        Rig r;
        BrnSound::Logic::GlobalStateManager lGlobal;
        lGlobal.maSubmixVoices[1].miIdent = 0x5151;
        r.mModule.mEnvironment.mapStateManagers[0] = &lGlobal;
        r.mManager.mSplicerBank.mbCreated = true;
        r.mEffect.mpState = nullptr;
        r.mEffect.mVoice.mbReady = false;
        gaCalls.clear();
        Check(!r.mEffect.Prepare(&r.mState) && r.mEffect.mePrepareState == PassbyEffect::E_PREPARE_STATE_CONNECT_VOICE &&
                  r.mEffect.GetStateBase() == &r.mState &&
                  Called("Construct 4242 " + std::to_string(H("~SplicerFactory::SK_NAME~")) + " " +
                         std::to_string(H("SplicerVoiceSpec"))),
              "Prepare: construct the SplicerVoiceSpec voice, then wait for it (CONNECT_VOICE, false)");
        r.mEffect.mVoice.mbReady = true;
        gaCalls.clear();
        Check(r.mEffect.Prepare(&r.mState) && !Called("Construct 4242 " + std::to_string(H("~SplicerFactory::SK_NAME~")) +
                                                      " " + std::to_string(H("SplicerVoiceSpec"))) &&
                  IndexOf("Attach " + std::to_string(H("~SplicerPlayerVoice::Slot~")) + " bank") == 0 &&
                  IndexOf("Connect " + std::to_string(H("Send01")) + " " + std::to_string(0x5151)) == 1,
              "Prepare: ready -> attach the manager's splicer bank, connect Send01 to the PASSBY submix voice");
    }
}

static void TestState()
{
    PassbyState lState;
    PassbyStateManager::Passby lPosted;
    lPosted.meType = ePT::Bridge;
    lPosted.mfRelativeVelocityMagnitude = 33.0f;
    lPosted.mStaticPos = V(1.0f, 1.0f, 1.0f);
    lState.mfTimeOutTimer = 3.0f;
    lState.Attach(&lPosted);
    Check(lState.mPassbyData.meType == ePT::Bridge && Same(lState.mPassbyData.mfRelativeVelocityMagnitude, 33.0f) &&
              Same(lState.mfTimeOutTimer, 0.0f) && lState.mpvLastAttachment == &lPosted && lState.miBaseAttaches == 1,
          "PassbyState::Attach: copy the record, timer 0, then State::Attach(the posted slot)");
    lPosted.meType = ePT::Tree;
    Check(lState.mPassbyData.meType == ePT::Bridge, "PassbyState::Attach: a copy -- the slot's reuse does not reach it");

    lState.UpdateParams(2.0f);
    Check(lState.miBaseUpdates == 1 && Same(lState.mfBaseUpdateDt, 2.0f) && Same(lState.mfTimeOutTimer, 2.0f) &&
              lState.miDetaches == 0,
          "PassbyState::UpdateParams: the base update, then the timer runs while attached");
    lState.UpdateParams(3.0f);
    Check(Same(lState.mfTimeOutTimer, 5.0f) && lState.miDetaches == 0,
          "PassbyState::UpdateParams: exactly KF_TIMEOUT_TIMER (5.0) does not time out (`ble`)");
    lState.UpdateParams(0.25f);
    Check(lState.miDetaches == 1, "PassbyState::UpdateParams: over 5.0 -> Detach");
    lState.UpdateParams(10.0f);
    Check(lState.miDetaches == 1 && Same(lState.mfTimeOutTimer, 5.25f),
          "PassbyState::UpdateParams: not attached -> the timer holds");
    PassbyState lNan;
    lNan.Attach(&lPosted);
    lNan.UpdateParams(KF_NAN);
    Check(lNan.miDetaches == 0, "PassbyState::UpdateParams: a NaN timer does not detach");
}

static void TestManagerUpdate()
{
    {
        BrnSound::Module::SoundLogicModule lModule;
        PassbyStateManager lManager;
        lManager.mpLogicModule = &lModule;
        PassbyState laStates[2];
        lManager.mapStates.push_back(&laStates[0]);
        lManager.mapStates.push_back(&laStates[1]);
        lModule.mInput.mVehicles.mbIsPlayerCarActive = true;
        for (int i = 0; i < 3; ++i)
        {
            PassbyStateManager::Passby lPassby;
            lPassby.meType = static_cast<ePT::ePassbyTypes>(ePT::TrafficSmall + i);
            lManager.PostPassby(lPassby);
        }
        gaCalls.clear();
        lManager.UpdateParams(0.02f);
        Check(laStates[0].mpvLastAttachment == &lManager.maPostedPassbys[0] &&
                  laStates[1].mpvLastAttachment == &lManager.maPostedPassbys[1] &&
                  laStates[0].mPassbyData.meType == ePT::TrafficSmall &&
                  laStates[1].mPassbyData.meType == ePT::TrafficMedium,
              "PassbyStateManager::UpdateParams: each posted record goes to a free state, in order");
        Check(lManager.muPostedPassbyCount == 0, "PassbyStateManager::UpdateParams: the posts are emptied every frame");
        const int liStart = IndexOf("StartMonitor 5");
        const int liBase = IndexOf("StateManager::UpdateParams " + std::to_string(0.02f));
        const int liFree = IndexOf("GetFreeState");
        const int liStop = IndexOf("StopMonitor 5");
        Check(liStart == 0 && liBase == 1 && liFree > liBase && liStop == static_cast<int>(gaCalls.size()) - 1,
              "PassbyStateManager::UpdateParams: monitor, the states' update, then the dispatch, monitor off");
        int liFreeCalls = 0;
        for (const std::string& lr : gaCalls)
            liFreeCalls += lr == "GetFreeState" ? 1 : 0;
        Check(liFreeCalls == 3, "PassbyStateManager::UpdateParams: no free state for the third -> stop there");
    }
    {
        BrnSound::Module::SoundLogicModule lModule;
        PassbyStateManager lManager;
        lManager.mpLogicModule = &lModule;
        PassbyState lState;
        lManager.mapStates.push_back(&lState);
        lModule.mInput.mVehicles.mbIsPlayerCarActive = false;
        PassbyStateManager::Passby lPassby;
        lManager.PostPassby(lPassby);
        lManager.UpdateParams(0.02f);
        Check(lState.miBaseAttaches == 0 && lManager.muPostedPassbyCount == 0,
              "PassbyStateManager::UpdateParams: no active player car -> nothing attached, posts still emptied");
    }
}

static BrnPhysics::Props::PropUpdateNotification Prop(u32 luIndex, Vector3 lPos, Vector3 lVel)
{
    BrnPhysics::Props::PropUpdateNotification l;
    std::memset(&l, 0, sizeof(l));
    l.mPosition = lPos;
    l.mLinearVelocity = lVel;
    l.mEntityId.mEntityId.muValue = (3u << 24) | luIndex;
    return l;
}

static void TestDynamicPropBys()
{
    BrnSound::Module::SoundLogicModule lModule;
    PassbyStateManager lManager;
    lManager.mpLogicModule = &lModule;
    lManager.mfCurrentTime = 100.0f;
    CgsSound::Logic::MicrophoneSystem::Microphone& lrCamera = lModule.mEnvironment.mMicrophoneSystem.maMicrophones[0][0];
    lrCamera.mMatrix.mPos = V(0.0f, 0.0f, 0.0f);
    lrCamera.mVelocity = V(0.0f, 0.0f, 0.0f);
    lModule.mEnvironment.mMicrophoneSystem.maMicrophones[1][0].mVelocity = V(1000.0f, 0.0f, 0.0f);   // not this one
    auto& lrQueue = lModule.mInput.mNotifications;
    lrQueue.maEvents[0] = Prop(1, V(2.0f, 0.0f, 0.0f), V(-20.0f, 0.0f, 0.0f));   // 20 m/s, 2 m: posted
    lrQueue.maEvents[1] = Prop(2, V(0.0f, 2.0f, 0.0f), V(0.0f, -10.0f, 0.0f));   // 10 m/s < 11.176: no
    lrQueue.maEvents[2] = Prop(3, V(0.0f, 0.0f, 3.5f), V(0.0f, 0.0f, -20.0f));   // 20 * 0.15 = 3 < 3.5: no
    lrQueue.miLength = 3;
    const unsigned luAsserts = guAsserts;
    lManager.UpdateDynamicPropBys(0.02f);
    Check(guAsserts == luAsserts, "UpdateDynamicPropBys: the queue is read through the const (read-lock) accessor");
    Check(lManager.muPostedPassbyCount == 1, "UpdateDynamicPropBys: only the fast, close prop is posted");
    const PassbyStateManager::Passby& lr = lManager.maPostedPassbys[0];
    Check(lr.meType == ePT::Camera && lr.mp3dControl == nullptr && !lr.mbSuppressBoostBys &&
              Same(lr.mfVolumeModifier, 1.0f) && Same(lr.mfRelativeVelocityMagnitude, 20.0f) &&
              Same(lr.mStaticPos.x, 2.0f),
          "UpdateDynamicPropBys: Passby(prop position, speed, Camera, false, 1.0)");
    Check(lManager.mDynamicPropCache.maItems[0].mbActive && lManager.mDynamicPropCache.maItems[0].mId.muValue == ((3u << 24) | 1) &&
              Same(lManager.mDynamicPropCache.maItems[0].mfTimeStamp, 100.0f) &&
              !lManager.mDynamicPropCache.maItems[1].mbActive,
          "UpdateDynamicPropBys: the posted prop is cached at mfCurrentTime");

    // The same prop again, 3 s later: refreshed, not re-posted.
    lManager.muPostedPassbyCount = 0;
    lManager.mfCurrentTime = 103.0f;
    lManager.UpdateDynamicPropBys(0.02f);
    Check(lManager.muPostedPassbyCount == 0 && Same(lManager.mDynamicPropCache.maItems[0].mfTimeStamp, 103.0f),
          "UpdateDynamicPropBys: a cached prop is refreshed and not voiced again");

    // Speed exactly at the floor is not enough (`vcmpgtfp.`).
    lrQueue.maEvents[0] = Prop(9, V(0.5f, 0.0f, 0.0f), V(-(0.44704f * 25.0f), 0.0f, 0.0f));
    lrQueue.miLength = 1;
    lManager.UpdateDynamicPropBys(0.02f);
    Check(lManager.muPostedPassbyCount == 0, "UpdateDynamicPropBys: 25 mph exactly is not above the floor");

    // The camera microphone's velocity is what the prop is measured against.
    lrCamera.mVelocity = V(-20.0f, 0.0f, 0.0f);
    lrQueue.maEvents[0] = Prop(10, V(1.0f, 0.0f, 0.0f), V(-20.0f, 0.0f, 0.0f));   // moving WITH the camera
    lManager.UpdateDynamicPropBys(0.02f);
    Check(lManager.muPostedPassbyCount == 0, "UpdateDynamicPropBys: speed is relative to the CAMERA microphone");

    // Insert: the first free slot, asserting on a duplicate.
    PassbyStateManager::DynamicPropByCache lCache;
    EntityId lId = { (3u << 24) | 77 };
    lCache.maItems[0].mbActive = true;
    lCache.maItems[0].mId.muValue = 5;
    PassbyStateManager::DynamicPropByCache::Item* lpItem = lCache.Insert(9.0f, lId);
    Check(lpItem == &lCache.maItems[1] && lpItem->mbActive && Same(lpItem->mfTimeStamp, 9.0f) && lpItem->mId.muValue == lId.muValue,
          "DynamicPropByCache::Insert: the first inactive slot, stamped and active");
    const unsigned luBefore = guAsserts;
    lCache.Insert(10.0f, lId);
    Check(guAsserts == luBefore + 1, "DynamicPropByCache::Insert: \"Find( lEntity ) == 0\" asserts on a duplicate");
    PassbyStateManager::DynamicPropByCache lFull;
    for (u32 i = 0; i < 32; ++i)
        lFull.maItems[i].mbActive = true;
    Check(lFull.Insert(1.0f, lId) == nullptr, "DynamicPropByCache::Insert: a full cache -> 0");
}

static void TestManagerPrepareRelease()
{
    BrnSound::Module::SoundLogicModule lModule;
    PassbyStateManager lManager;
    lManager.mpLogicModule = &lModule;
    gaCalls.clear();
    Check(!lManager.Prepare() && lManager.mePrepareState == CgsSound::Logic::StateManager::E_PREPARE_UPDATING &&
              IndexOf("LoadAsset sound\\splicer\\PassbyAsset.bundle null 0") == 0,
          "Prepare: LoadAsset(\"sound\\\\splicer\\\\PassbyAsset.bundle\", 0, E_DATA), then wait on the bank");
    gaCalls.clear();
    Check(!lManager.Prepare() && gaCalls.size() == 1 && gaCalls[0] == "waiting",
          "Prepare: the bank not loaded yet -> stay in UPDATING (no second LoadAsset)");
    lManager.mSplicerBank.mbCreated = true;
    lManager.mSplicerBank.mbLoaded = true;
    gaCalls.clear();
    Check(!lManager.Prepare() && lManager.miCpuMonitor == 77 && CgsDev::PerfMonCpu::giAddedPage == 14 &&
              IndexOf("AddMonitor Passbys nomin 1.0 scaled") == 0 && IndexOf("PrepareStates 1 8 0") == 1 &&
              lManager.mePrepareState == CgsSound::Logic::StateManager::E_PREPARE_STATES,
          "Prepare: loaded -> the \"Passbys\" monitor (page 14), then PrepareStates(1, 8, 0)");
    lManager.mbStatesPrepared = true;
    Check(lManager.Prepare() && lManager.mePrepareState == CgsSound::Logic::StateManager::E_PREPARE_FINISHED,
          "Prepare: the states prepared -> FINISHED, true");
    Check(lManager.Prepare(), "Prepare: FINISHED stays true");
    lManager.mePrepareState = CgsSound::Logic::StateManager::E_PREPARE_RELEASED;
    lManager.mSplicerBank.mbLoaded = false;
    gaCalls.clear();
    lManager.Prepare();
    Check(IndexOf("LoadAsset sound\\splicer\\PassbyAsset.bundle null 0") == 0,
          "Prepare: RELEASED restarts from the load");
    gaCalls.clear();
    lManager.mSplicerBank.mbCreated = true;
    Check(lManager.Release() && IndexOf("Content::Destruct") == 0, "Release: a constructed bank is destructed; true");
    gaCalls.clear();
    Check(lManager.Release() && gaCalls.empty(), "Release: nothing constructed -> nothing to destruct");
}

static void TestConstants()
{
    Check(Bits(KF_MAX_CAR_SPEED) == 0x42A00000u && Bits(KF_MIN_CAR_SPEED) == 0u,
          "constants: KF_MAX_CAR_SPEED = 0x82F2CF08 (80.0), KF_MIN_CAR_SPEED = .bss 0x82FFB940 (0.0)");
    Check(Bits(KF_MIXER_AZIMUTH_TO_DEGREES) == 0x3BB400B4u && Bits(KF_MIXER_Q12_TO_PITCH) == 0x39800000u,
          "constants: flt_820AA8EC / flt_820AA8F4");
    Check(Bits(KF_COLLISION_SPEED_MAX) == 0x41700000u && Bits(KF_COLLISION_SPEED_SCALE) == 0x3D888889u &&
              Bits(KF_COLLISION_PITCH_RANGE) == 0x3ECCCCCEu && Bits(KF_COLLISION_PITCH_MIN) == 0x3F4CCCCDu,
          "constants: flt_820047C4 / flt_820AA540 / flt_820138AC (0x3ECCCCCE) / flt_820054C8");
    Check(Bits(0.44704f * 25.0f) == Bits(FromBits(0x3EE4E26Du) * FromBits(0x41C80000u)),
          "constants: the prop speed floor is flt_82F31928 * flt_820AA53C in single precision");
    Check(Bits(KF_TIMEOUT_TIMER) == 0x40A00000u, "constants: KF_TIMEOUT_TIMER = flt_820ABCD8 (5.0)");
}

int main()
{
    TestSlope();
    TestChooseSampleId();
    TestUpdatePosition();
    TestRelativeVelocity();
    TestEffectUpdateParams();
    TestProcessUpdate();
    TestAttach();
    TestDetachPrepareController();
    TestState();
    TestManagerUpdate();
    TestDynamicPropBys();
    TestManagerPrepareRelease();
    TestConstants();
    std::printf("FxTailsBPassby: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures == 0 ? 0 : 1;
}
