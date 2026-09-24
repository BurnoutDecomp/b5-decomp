// FX-VOICEPOOL (crash parity 2026-09-24), second piece: how a crash voice STARTS and what it tells
// the mixer -- CollisionEffect::Attach against the ARTIST machine code.
//
//   CollisionEffect::Attach                        0x826F8218
//   CollisionEffect::InitWork<crashbin>            0x826EB240   DWARF BrnCollisionEffect.h:126
//   CollisionEffect::InitWork<propscrashbin>       0x826EB368
//   CollisionEffect::GetSizeSpecificSettings<T>    0x826AABC8 / 0x826AAC88
//   CollisionEffect::CalculateIntensity            0x82688240   DWARF BrnCollisionEffect.cpp:434
//
// run_fxvoicepool_attach.py extracts the PRODUCTION Attach (and, where the revision has them, the
// three helpers) from BrnCollisionEffect.cpp and compiles them as members of the fixture effect
// below, whose voice, mixer inputs, state, manager and bins record what they are asked to do.
// OutputCollision / InputCollision / the enums are the real headers. A revision without
// CalculateIntensity counts its direct checks as failed (FXVP_HAS_CALCULATE_INTENSITY undefined).
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Sound/Collision/BrnCollisionDataStructures.h"
#include "GameSource/World/BrnEntityTypes.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

static unsigned guAsserts = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcExpression, const char*, int)
{
    ++guAsserts;
    std::printf("assert: %s\n", lpcExpression);
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
} }

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

// ---- the voice records its calls, in order ----------------------------------------------------------
static std::vector<std::string> gaCalls;
namespace CgsSound { namespace Logic {
struct Content { int miTag; };
class StateManager { public: virtual ~StateManager() {} };
class State
{
public:
    StateManager* mpStateManager = nullptr;
    StateManager* GetStateManager() const { return mpStateManager; }
};
struct Voice
{
    s32 miSlot = 0;
    const Content* mpContent = nullptr;
    u32 muGainSend = 0xFFFFFFFFu;
    f32 mfGain = -1.0f;
    u32 muGainName = 0;
    s32 miPlayed = -12345;
    void* GetVoiceObject() const { return const_cast<Voice*>(this); }
    void Attach(s32 liSlotName, const Content& arContent)
    {
        miSlot = liSlotName;
        mpContent = &arContent;
        gaCalls.push_back("attach");
    }
    void SetGain(u32 luSendIndex, f32 lfGain, const u32* lpSendName)
    {
        muGainSend = luSendIndex;
        mfGain = lfGain;
        muGainName = lpSendName ? *lpSendName : 0;
        gaCalls.push_back("gain");
    }
    s32 Play(s32 liParam)
    {
        miPlayed = liParam;
        gaCalls.push_back("play");
        return 0;
    }
};

// The EffectBase surface Attach uses: its own Attach, the owning state, the mixer inputs.
class EffectBase
{
public:
    virtual ~EffectBase() {}
    virtual bool Attach() { ++miBaseAttaches; return true; }
    State* GetStateBase() const { return mpState; }
    void SetMixerInputValue(int aiSlot, int aiValue)
    {
        if (aiSlot == 0) miInput0 = aiValue;
        if (aiSlot == 1) miInput1 = aiValue;
    }
    State* mpState = nullptr;
    int miBaseAttaches = 0;
    int miInput0 = -1;
    int miInput1 = -1;
};
} }

// ---- bins: two authored rows keyed by the collision's bin key -----------------------------------
namespace Attrib { namespace Gen {
struct BinVector { f32 x, y, z; };
template <int KI_KIND>
struct FixtureBin
{
    u64 muKey;
    FixtureBin(u64 luKey, void*) : muKey(luKey) {}
    bool IsValid() const { return true; }
    s32 MixerSlider() const { return KI_KIND == 0 ? 6 : 9; }
    BinVector Volumes() const
    {
        return KI_KIND == 0 ? BinVector{ 1.1f, 1.2f, 1.3f } : BinVector{ 2.1f, 2.2f, 2.3f };
    }
    BinVector Pitch() const
    {
        return KI_KIND == 0 ? BinVector{ 0.7f, 0.8f, 0.9f } : BinVector{ 1.7f, 1.8f, 1.9f };
    }
};
typedef FixtureBin<0> crashbin;
typedef FixtureBin<1> propscrashbin;
} }

namespace BrnSound { namespace Logic { namespace Collision {

enum ECollisionSpliceBankType
{
    E_COLLISION_SPLICE_BANK_COLLISION = 0,
    E_COLLISION_SPLICE_BANK_MAX = 1,
};

class CollisionStateManager : public CgsSound::Logic::StateManager
{
public:
    CgsSound::Logic::Content maBanks[1] = { { 41 } };
    const CgsSound::Logic::Content& GetSplicerBank(ECollisionSpliceBankType aeBank) const
    {
        return maBanks[aeBank];
    }
};

class CollisionState : public CgsSound::Logic::State
{
public:
    OutputCollision mOutput;
    const OutputCollision& GetOutputCollision() const { return mOutput; }
};

struct CollisionControl
{
    CollisionState* mpState = nullptr;
    bool mbFinished = false;
    bool GetCollisionFinished() const { return mbFinished; }
    CollisionState* GetCollisionState() const { return mpState; }
};

struct CollisionEffect : public CgsSound::Logic::EffectBase
{
    struct SizeSpecificSettings
    {
        f32 mfVolume = 1.0f;
        f32 mfPitch = 1.0f;
    };

    CollisionControl* mpCollisionControl = nullptr;
    bool mbFirstUpdate = false;
    bool mbUseAzimuth = false;
    CgsSound::Logic::Voice mCrashVoice;
    f32 mfIntensity = 0.0f;
    s32 meNicotineVolumeSlider = 5;
    s32 meNicotinePitchSlider = 5;
    SizeSpecificSettings mSizeSettings;

    bool Attach();
    template <typename T>
    void InitWork(CollisionState* apState, const OutputCollision& arCollision);
    template <typename T>
    void GetSizeSpecificSettings(const OutputCollision& arCollision, const T& arBin,
                                 SizeSpecificSettings& arSettings) const;
    f32 CalculateIntensity(const OutputCollision& arCollision);
};

#include "fxvoicepool_attach_bodies.inc"

} } }

// ---- checks ----------------------------------------------------------------------------------------
using namespace BrnSound::Logic;
using namespace BrnSound::Logic::Collision;

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
static bool Near(f32 a, f32 b) { return std::fabs(a - b) <= 1e-4f * (std::max)(1.0f, std::fabs(b)); }
static const f32 KF_NAN = std::numeric_limits<f32>::quiet_NaN();

static EntityId Owner(u32 luOwner, u32 luIndex = 0) { EntityId l; l.muValue = (luOwner << 24) | (luIndex << 10); return l; }

struct Rig
{
    CollisionStateManager mMgr;
    CollisionState mState;
    CollisionControl mControl;
    CollisionEffect mEffect;
    Rig()
    {
        mState.mpStateManager = &mMgr;
        mControl.mpState = &mState;
        mEffect.mpState = &mState;
        mEffect.mpCollisionControl = &mControl;
        OutputCollision& o = mState.mOutput;
        o.meBankType = 0;
        o.mePipeline = InputCollision::E_REGULAR;
        o.maEntityID[0] = Owner(BrnWorld::E_ENTITYTYPE_RACECAR);
        o.maEntityID[1] = Owner(BrnWorld::E_ENTITYTYPE_WORLD);
        o.meAction = AttribSys::Enums::eAction::Collision;
        o.meSize = E_SIZE_LARGE;
        o.meFatality = E_FATAL_OFF;
        o.mBinKey = 0x1234ull;
        o.miSampleID = 77;
        o.mNormalizedImpulse.x = 0.5f;
        gaCalls.clear();
    }
    OutputCollision& Out() { return mState.mOutput; }
    int Duck() { mEffect.Attach(); return mEffect.miInput1; }
};

int main()
{
    // ---- CalculateIntensity (0x82688240) ---------------------------------------------------------------
#ifdef FXVP_HAS_CALCULATE_INTENSITY
    {
        Rig r;
        OutputCollision& o = r.Out();
        Check(Near(r.mEffect.CalculateIntensity(o), 25.0f + 75.0f * 0.5f),
              "CalculateIntensity: race car vs the world = (flt_82F2CED0 100 - flt_82F2CEC4 25) * x + 25");
        o.maEntityID[1] = Owner(BrnWorld::E_ENTITYTYPE_RACECAR, 3);
        o.mNormalizedImpulse.x = 1.0f;
        Check(Near(r.mEffect.CalculateIntensity(o), 100.0f), "CalculateIntensity: race car vs race car (flt_82F2CECC) at x = 1 -> 100");
        o.maEntityID[1] = Owner(BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE, 9);
        o.mNormalizedImpulse.x = 0.0f;
        Check(Near(r.mEffect.CalculateIntensity(o), 25.0f), "CalculateIntensity: race car vs traffic (flt_82F2CED4) at x = 0 -> 25");
        o.maEntityID[1] = Owner(BrnWorld::E_ENTITYTYPE_PROP, 1);
        o.mNormalizedImpulse.x = 0.8f;
        Check(r.mEffect.CalculateIntensity(o) == 0.0f, "CalculateIntensity: race car vs a prop owner (3) -> 0");
        o.maEntityID[0] = Owner(BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE, 2);
        o.maEntityID[1] = Owner(BrnWorld::E_ENTITYTYPE_WORLD);
        Check(Near(r.mEffect.CalculateIntensity(o), 25.0f + 75.0f * 0.8f), "CalculateIntensity: traffic vs anything -> flt_82F2CED4 row");
        o.maEntityID[0] = Owner(BrnWorld::E_ENTITYTYPE_WORLD);
        o.maEntityID[1] = Owner(BrnWorld::E_ENTITYTYPE_RACECAR);
        Check(r.mEffect.CalculateIntensity(o) == 0.0f, "CalculateIntensity: the world as A -> 0");
        o.maEntityID[0] = Owner(BrnWorld::E_ENTITYTYPE_PROP, 4);
        Check(r.mEffect.CalculateIntensity(o) == 0.0f, "CalculateIntensity: A owner >= 3 -> 0");
        o.maEntityID[0] = Owner(BrnWorld::E_ENTITYTYPE_RACECAR);
        o.meAction = AttribSys::Enums::eAction::Detach;
        Check(r.mEffect.CalculateIntensity(o) == 0.0f, "CalculateIntensity: any action but Collision -> 0");
        o.meAction = AttribSys::Enums::eAction::Collision;
        o.mePipeline = InputCollision::E_PROP;
        o.mNormalizedImpulse.x = 0.5f;
        Check(Near(r.mEffect.CalculateIntensity(o), 12.0f * 0.5f), "CalculateIntensity: the prop pipeline = flt_82F2CEC8 (12) * x");
    }
#else
    for (int i = 0; i < 9; ++i)
        Check(false, "CalculateIntensity @0x82688240: no body in this revision");
#endif

    // ---- Attach (0x826F8218) / InitWork<T> (0x826EB240 / 0x826EB368) ---------------------------------
    {
        Rig r;
        r.mEffect.Attach();
        const CgsSound::Logic::Voice& v = r.mEffect.mCrashVoice;
        const bool lbOrder = gaCalls.size() == 3 && gaCalls[0] == "attach" && gaCalls[1] == "gain" && gaCalls[2] == "play";
        Check(lbOrder && v.muGainSend == 0 && v.mfGain == 0.0f &&
                  v.muGainName == CgsSound::Playback::Name::MakeHash("Send01"),
              "InitWork: the voice is attached, SILENCED on Send01 (SetGain(0, 0.0) 0x826EB2D4..0x826EB2F4), then played");
        Check(v.miSlot == static_cast<s32>(CgsSound::Playback::Name::MakeHash("~SplicerPlayerVoice::Slot~")) &&
                  v.mpContent == &r.mMgr.maBanks[0] && v.miPlayed == 77,
              "InitWork: the manager's splicer bank on ~SplicerPlayerVoice::Slot~, the output's sample id played");
        Check(r.mEffect.meNicotinePitchSlider == 1 && r.mEffect.meNicotineVolumeSlider == 6,
              "InitWork: pitch slider 1, volume slider = the crash bin's MixerSlider");
        Check(Near(r.mEffect.mSizeSettings.mfVolume, 1.3f) && Near(r.mEffect.mSizeSettings.mfPitch, 0.9f),
              "GetSizeSpecificSettings: Large takes Volumes().z / Pitch().z");
        Check(r.mEffect.miInput0 == 0x7FFF, "Attach: mixer input 0 = 0x7FFF");
        Check(r.mEffect.miInput1 == static_cast<int>((25.0f + 75.0f * 0.5f) * 327.67001f),
              "Attach: mixer input 1 = CalculateIntensity * flt_820B78E0 (327.67) -- a half-strength car-vs-wall hit "
              "sends 20479, not 32767 - x * 327.67 (the PC's inverted value)");
        Check(r.mEffect.mbFirstUpdate && r.mEffect.mbUseAzimuth && r.mEffect.miBaseAttaches == 1,
              "Attach: EffectBase::Attach once, first update armed, azimuth kept outside a fatality start");
    }
    {
        Rig r;
        r.Out().mNormalizedImpulse.x = 1.2f;
        Check(r.Duck() == 32767, "Attach: an intensity above 100 clamps the ducking input at flt_820AD310 (32767)");
    }
    {
        Rig r;
        r.Out().mNormalizedImpulse.x = -0.5f;
        Check(r.Duck() == 0, "Attach: a negative intensity takes 0 (fneg / fsel)");
    }
    {
        Rig r;
        r.Out().mNormalizedImpulse.x = KF_NAN;
        Check(r.Duck() == 32767, "Attach: a NaN intensity survives the first fsel and becomes 32767 at the second");
    }
    {
        Rig r;
        r.Out().meAction = AttribSys::Enums::eAction::Detach;
        r.Out().mNormalizedImpulse.x = 3.0f;
        Check(r.Duck() == 0, "Attach: a Detach (or hinge) collision does not duck the mix (CalculateIntensity 0)");
        Check(r.mEffect.mbUseAzimuth, "Attach: a Detach collision KEEPS its azimuth -- only meFatality == E_FATAL_START drops it");
    }
    {
        Rig r;
        r.Out().meFatality = E_FATAL_START;
        r.mEffect.Attach();
        Check(!r.mEffect.mbUseAzimuth, "Attach: a collision at the fatality start drops the azimuth (0x826F8334..0x826F8340)");
    }
    {
        Rig r;
        r.Out().mePipeline = InputCollision::E_PROP;
        r.Out().meSize = E_SIZE_SMALL;
        r.mEffect.Attach();
        Check(r.mEffect.meNicotineVolumeSlider == 9 && Near(r.mEffect.mSizeSettings.mfVolume, 2.1f) &&
                  Near(r.mEffect.mSizeSettings.mfPitch, 1.7f),
              "InitWork<propscrashbin>: the prop bin's slider, Small takes .x");
        Check(r.mEffect.miInput1 == static_cast<int>(12.0f * 0.5f * 327.67001f),
              "Attach: a prop hit ducks by flt_820AD310-clamped 12 * x * 327.67");
    }
    {
        Rig r;
        r.Out().meSize = E_SIZE_MEDIUM;
        r.mEffect.Attach();
        Check(Near(r.mEffect.mSizeSettings.mfVolume, 1.2f) && Near(r.mEffect.mSizeSettings.mfPitch, 0.8f),
              "GetSizeSpecificSettings: Medium takes .y");
    }
    {
        Rig r;
        r.mControl.mbFinished = true;
        r.mEffect.Attach();
        Check(gaCalls.empty() && r.mEffect.miInput1 == -1 && r.mEffect.mbUseAzimuth && !r.mEffect.mbFirstUpdate,
              "Attach: a finished collision returns before touching the voice or the mixer");
    }

    Check(guAsserts == 0, "no assert fired");
    std::printf("FxVoicepoolAttach: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
