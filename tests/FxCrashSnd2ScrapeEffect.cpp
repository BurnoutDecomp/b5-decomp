// FX-CRASHSND2 (crash parity 2026-09-24, item 2): the scrape VOICE -- ScrapeEffect, the effect a
// collision state with the SCRAPE lifetime runs -- against the ARTIST machine code.
//
//   ScrapeEffect::GetIntensity   0x826BD620   DWARF cpp:360
//   ScrapeEffect::UpdateParams   0x826F8578   DWARF cpp:228
//   ScrapeEffect::ProcessUpdate  0x826BD6F8   DWARF cpp:470
//
// run_fxcrashsnd2_scrape_effect.py extracts the three PRODUCTION bodies and the file's constant
// block from BrnScrapeEffect.cpp and compiles them here as members of a fixture ScrapeEffect whose
// voice, controls, state and collision manager record what they are asked to do. ScrapeInfo,
// FrameInformation and DataPoint are the real headers.
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Sound/Collision/BrnCollisionDataStructures.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <utility>
#include <vector>

static unsigned guAsserts = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++guAsserts; return 0; }
void* EndAssert() { return nullptr; }
} }
namespace CgsDev { namespace Log {
struct DebugPrintFixture { DebugPrintFixture& operator<<(const char*) { return *this; } };
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
    u32 mHash;
    u32 GetValue() const { return mHash; }
};
const Name& AemsFactorySkName() { static const Name l = { 0xAE5F0001u }; return l; }
} }

namespace CgsSound { namespace Logic {
class Module {};
struct Content { int miTag; };
class StateManager { public: virtual ~StateManager() {} };

struct VoiceWrapper
{
    struct CreateParams
    {
        Module* mpLogicModule = nullptr;
        u32 mFactoryName = 0;
        u32 mVoiceSpecName = 0;
        const Content* mpContent = nullptr;
        u32 mSlotName = 0;
        u32 mSendName = 0;
        u32 mSubMixVoiceID = 0;
        s32 miSendIndex = -1;
    };
    CreateParams mLast;
    int miCreates = 0, miPlays = 0, miStops = 0, miUpdates = 0, miGains = 0;
    bool mbLive = false;
    std::vector<std::pair<s32, f32>> maParameters;
    void Update() { ++miUpdates; }
    void Create(const CreateParams& lrParams) { mLast = lrParams; ++miCreates; mbLive = true; }
    void Play(u32) { ++miPlays; }
    void Stop() { ++miStops; }
    void Release() {}
    bool HasLiveVoice() const { return mbLive; }
    void SetParameter(s32 liIndex, f32 lfValue, const u32*) { maParameters.push_back(std::make_pair(liIndex, lfValue)); }
    void SetGain(u32, f32, const u32*) { ++miGains; }
};
} }

namespace BrnSound { namespace Logic { namespace Collision {

struct CollisionState
{
    enum ELifetime { E_NONE = 0, E_COLLISION = 1, E_SCRAPE = 2 };
    CgsSound::Utils::DataPoint<ELifetime> meLifetime;
    f32 mfCurTime = 0.0f;
    f32 mfTimeWeAttached = 0.0f;
    CgsSound::Logic::StateManager* mpStateManager = nullptr;
    const CgsSound::Utils::DataPoint<ELifetime>& GetLifetime() const { return meLifetime; }
    f32 GetCurrentTime() const { return mfCurTime; }
    f32 GetTimeWeAttached() const { return mfTimeWeAttached; }
    CgsSound::Logic::StateManager* GetStateManager() const { return mpStateManager; }
};

struct CollisionControl
{
    CollisionState* mpState = nullptr;
    CgsSound::Utils::DataPoint<ScrapeInfo> mScrapeInfo;
    CollisionState* GetCollisionState() const { return mpState; }
    const CgsSound::Utils::DataPoint<ScrapeInfo>& GetScrapeInfo() const { return mScrapeInfo; }
};

class CollisionStateManager : public CgsSound::Logic::StateManager
{
public:
    FrameInformation mFrame;
    CgsSound::Logic::Content mScrapeBank = { 77 };
    const FrameInformation& GetFrameInformation() const { return mFrame; }
    const CgsSound::Logic::Content& GetScrapeAemsBank() const { return mScrapeBank; }
};

struct Collision3DControl;

#include "fxcrashsnd2_scrape_effect_constants.inc"

struct ScrapeEffect
{
    f32 mfScrapeStartTimeStamp = 0.0f;
    f32 mfParam_AEMS_pitch = 0.0f;
    f32 mfParam_AEMS_volume = 0.0f;
    f32 mfParam_AEMS_friction_stress = 0.0f;
    f32 mfParam_AEMS_normal_stress = 0.0f;
    f32 mfParam_AEMS_time_scraping = 0.0f;
    f32 mfParam_AEMS_material_a = 0.0f;
    u8 miParam_AEMS_material_b = 0;
    f32 mfParam_AEMS_is_Crashing = 0.0f;
    Collision3DControl* mpCollision3DControl = nullptr;
    CollisionControl* mpCollisionControl = nullptr;
    CgsSound::Logic::VoiceWrapper mScrapeVoice;
    bool mbPlaying = false;

    // fixture hooks: the mixer outputs, the material MapScrapeToMaterial answers, the mixer inputs
    f32 mfPitchOut = 111.0f;
    f32 mfGainOut = 222.0f;
    f32 mfAzimuthOut = 333.0f;
    u8 muMaterial = 1;
    CgsSound::Logic::Module mModule;
    std::vector<std::pair<int, int>> maMixerInputs;
    f32 GetPitch() const { return mfPitchOut; }
    f32 GetGain() const { return mfGainOut; }
    f32 GetMixerOutputValue(int, int) { return mfAzimuthOut; }
    void SetMixerInputValue(int aiSlot, int aiValue) { maMixerInputs.push_back(std::make_pair(aiSlot, aiValue)); }
    CgsSound::Logic::Module* GetLogicModule() { return &mModule; }
    u8 MapScrapeToMaterial(const ScrapeInfo&) const { return muMaterial; }

    f32 GetIntensity(CollisionState* apState, const ScrapeInfo& arScrapeInfo);
    void UpdateParams(f32 afDeltaTime);
    void ProcessUpdate();
};

#include "fxcrashsnd2_scrape_effect_bodies.inc"

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
static bool Near(f32 a, f32 b) { return std::fabs(a - b) <= 1e-4f * std::max(1.0f, std::fabs(b)); }
static const f32 KF_NAN = std::numeric_limits<f32>::quiet_NaN();

struct Rig
{
    CollisionStateManager mMgr;
    CollisionState mState;
    CollisionControl mControl;
    ScrapeEffect mEffect;
    Rig()
    {
        mState.mpStateManager = &mMgr;
        mControl.mpState = &mState;
        mEffect.mpCollisionControl = &mControl;
    }
    // A scrape whose relative velocity is (3, 4, 0) -- |v| = 5 -- stamped at lfStamp.
    ScrapeInfo Scrape(f32 lfStamp, f32 lfVelocityScale = 1.0f)
    {
        ScrapeInfo l;
        l.mRelativeVelocity.x = 3.0f * lfVelocityScale;
        l.mRelativeVelocity.y = 4.0f * lfVelocityScale;
        l.mRelativeVelocity.z = 0.0f;
        l.mfTimeStamp = lfStamp;
        l.mfIntensity = 9999.0f;   // the contact impulse the PC used to return
        l.mbValid = true;
        return l;
    }
    f32 Intensity(const ScrapeInfo& lrScrape, f32 lfNow, u8 luMaterial)
    {
        mState.mfCurTime = lfNow;
        mEffect.miParam_AEMS_material_b = luMaterial;
        return mEffect.GetIntensity(&mState, lrScrape);
    }
    void Frame(CollisionState::ELifetime leLifetime, f32 lfNow, const ScrapeInfo& lrScrape)
    {
        mState.meLifetime.Update(leLifetime);
        mState.mfCurTime = lfNow;
        mControl.mScrapeInfo.Update(lrScrape);
        mEffect.UpdateParams(1.0f / 60.0f);
    }
};

int main()
{
    // ---- GetIntensity (0x826BD620) ------------------------------------------------------------------
    {
        Rig r;
        const ScrapeInfo lScrape = r.Scrape(10.0f);
        Check(Near(r.Intensity(lScrape, 10.0f, 0), 5.0f * 250.0f * 2.5f),
              "GetIntensity: |relative velocity| * KF_AEMS_FRICTION_SCALE_UP (250) * KF_WORLD_SCALING_UP (2.5) "
              "against the world outside a fatality -- not the contact impulse");
        Check(Near(r.Intensity(lScrape, 10.0f, 1), 5.0f * 250.0f * 10.0f),
              "GetIntensity: against an AI car (material_b 1) KF_AI_SCALING_UP (10)");
        Check(Near(r.Intensity(lScrape, 10.0f, 2), 5.0f * 250.0f * 10.0f),
              "GetIntensity: against traffic (material_b 2) KF_AI_SCALING_UP (10)");
        Check(Near(r.Intensity(lScrape, 10.0f, 3), 5.0f * 250.0f * 2.5f),
              "GetIntensity: any other material outside a fatality takes the world scaling");
        r.mMgr.mFrame.meFatality.Flush(E_FATAL_ON);
        Check(Near(r.Intensity(lScrape, 10.0f, 0), 5.0f * 250.0f),
              "GetIntensity: in a fatality the world scaling is not applied");
        Check(Near(r.Intensity(lScrape, 10.0f, 1), 5.0f * 250.0f * 10.0f),
              "GetIntensity: in a fatality a car still takes KF_AI_SCALING_UP");
        r.mMgr.mFrame.meFatality.Flush(E_FATAL_OFF);
        Check(Near(r.Intensity(lScrape, 10.125f, 0), 5.0f * 250.0f * 2.5f * 0.5f),
              "GetIntensity: ramps down linearly over KF_RAMP_DOWN_TIME (0.25 s) since the stamp");
        Check(r.Intensity(lScrape, 10.25f, 0) == 0.0f && r.Intensity(lScrape, 11.0f, 0) == 0.0f,
              "GetIntensity: a scrape KF_RAMP_DOWN_TIME old or older is silent");
        Check(r.Intensity(r.Scrape(KF_NAN), 10.0f, 0) == 0.0f,
              "GetIntensity: a NaN age takes the full ramp (fsel) -- silent");
        Check(Near(r.Intensity(r.Scrape(10.1f), 10.0f, 0), 5.0f * 250.0f * 2.5f * 1.4f),
              "GetIntensity: a stamp ahead of the clock ramps UP (1 - (-0.1 / 0.25)), as the fsel does");
        Check(r.Intensity(r.Scrape(10.0f, 0.0f), 10.0f, 0) == 0.0f,
              "GetIntensity: no relative velocity, no intensity (the zero-length vsel)");
    }

    // ---- UpdateParams (0x826F8578) ------------------------------------------------------------------
    {
        Rig r;
        r.mState.mfTimeWeAttached = 10.0f;
        ScrapeInfo lScrape = r.Scrape(10.1f);
        lScrape.mbCrashing = true;

        r.Frame(CollisionState::E_SCRAPE, 10.1f, lScrape);
        Check(r.mEffect.mScrapeVoice.miCreates == 0 && !r.mEffect.mbPlaying,
              "UpdateParams: no scrape voice within KF_TIME_DELAY_BEFORE_SCRAPES (0.15 s) of the attach");
        Check(r.mEffect.mfParam_AEMS_pitch == 111.0f && r.mEffect.mfParam_AEMS_volume == 222.0f &&
                  r.mEffect.mfParam_AEMS_is_Crashing == 32767.0f,
              "UpdateParams: pitch / volume from the mixer outputs, is_Crashing 32767 for a crashing scrape");
        Check(Near(r.mEffect.mfParam_AEMS_friction_stress, 5.0f * 250.0f * 2.5f) &&
                  Near(r.mEffect.mfParam_AEMS_time_scraping, 10.1f),
              "UpdateParams: inside the delay the intensity and time parameters still update (the else leg)");

        r.Frame(CollisionState::E_SCRAPE, 10.2f, lScrape);
        const CgsSound::Logic::VoiceWrapper& lrVoice = r.mEffect.mScrapeVoice;
        Check(lrVoice.miCreates == 1 && lrVoice.miPlays == 1 && r.mEffect.mbPlaying,
              "UpdateParams: past the delay the scrape voice is created and played once");
        Check(lrVoice.mLast.mVoiceSpecName == CgsSound::Playback::Name::MakeHash("AEMS_ScrapeGranulator") &&
                  lrVoice.mLast.mpContent == &r.mMgr.mScrapeBank &&
                  lrVoice.mLast.mSlotName == CgsSound::Playback::Name::MakeHash("AEMS_Slot") &&
                  lrVoice.mLast.mSendName == CgsSound::Playback::Name::MakeHash("Send01") &&
                  lrVoice.mLast.mSubMixVoiceID == 1u && lrVoice.mLast.miSendIndex == 0 &&
                  lrVoice.mLast.mFactoryName == 0xAE5F0001u && lrVoice.mLast.mpLogicModule == &r.mEffect.mModule,
              "UpdateParams: the AEMS_ScrapeGranulator voice on the collision manager's scrape bank, slot AEMS_Slot, Send01");
        Check(r.mEffect.mfParam_AEMS_volume == 32767.0f && r.mEffect.mfParam_AEMS_friction_stress == 0.0f &&
                  r.mEffect.mfParam_AEMS_normal_stress == 0.0f && r.mEffect.mfParam_AEMS_time_scraping == 0.0f &&
                  r.mEffect.mfParam_AEMS_pitch == 4095.0f && r.mEffect.mfParam_AEMS_material_a == 0.0f &&
                  r.mEffect.miParam_AEMS_material_b == 1 && r.mEffect.mfScrapeStartTimeStamp == 10.2f,
              "UpdateParams: the start sets volume 32767, pitch 4095, friction / normal / time / material_a 0, "
              "material_b from MapScrapeToMaterial, the start stamp");

        const ScrapeInfo lNext = r.Scrape(10.25f);
        r.Frame(CollisionState::E_SCRAPE, 10.25f, lNext);
        Check(lrVoice.miCreates == 1 && Near(r.mEffect.mfParam_AEMS_friction_stress, 5.0f * 250.0f * 10.0f) &&
                  Near(r.mEffect.mfParam_AEMS_time_scraping, 0.05f),
              "UpdateParams: while playing friction_stress = the intensity (material_b 1: x10), time = now - start");
        Check(r.mEffect.mfParam_AEMS_normal_stress == 0.0f && r.mEffect.mfParam_AEMS_material_a == 0.0f,
              "UpdateParams: while playing normal_stress and material_a are not written (the PC wrote the "
              "intensity and the orientation)");
        Check(r.mEffect.mfParam_AEMS_is_Crashing == 0.0f, "UpdateParams: is_Crashing follows the scrape (0)");

        r.Frame(CollisionState::E_SCRAPE, 10.25f, r.Scrape(10.25f, 100.0f));
        Check(r.mEffect.mfParam_AEMS_friction_stress == 32767.0f,
              "UpdateParams: friction_stress is clamped to 32767 (fsel)");
        r.Frame(CollisionState::E_SCRAPE, 10.25f, r.Scrape(10.25f, KF_NAN));
        Check(r.mEffect.mfParam_AEMS_friction_stress == 32767.0f,
              "UpdateParams: a NaN intensity gives 32767 (both fsel tests fail)");

        r.Frame(CollisionState::E_NONE, 10.3f, lNext);
        Check(lrVoice.miStops == 1 && r.mEffect.mfParam_AEMS_friction_stress == 0.0f &&
                  r.mEffect.mfParam_AEMS_time_scraping == 0.0f,
              "UpdateParams: leaving the SCRAPE lifetime stops the voice and zeroes friction / time");
        r.Frame(CollisionState::E_NONE, 10.4f, lNext);
        Check(lrVoice.miStops == 1, "UpdateParams: the stop happens on the edge only");
        Check(lrVoice.miUpdates == 7, "UpdateParams: the voice wrapper is updated every frame");

        Rig c;
        c.mState.mfTimeWeAttached = 0.0f;
        c.Frame(CollisionState::E_COLLISION, 5.0f, c.Scrape(5.0f));
        Check(c.mEffect.mScrapeVoice.miCreates == 0 && c.mEffect.mfParam_AEMS_friction_stress == 0.0f,
              "UpdateParams: an impact (COLLISION lifetime) starts no scrape voice");

        Rig n;
        n.mState.mfTimeWeAttached = KF_NAN;
        n.Frame(CollisionState::E_SCRAPE, 5.0f, n.Scrape(5.0f));
        Check(n.mEffect.mScrapeVoice.miCreates == 0,
              "UpdateParams: a NaN attach time never starts the voice (`fcmpu ; bge`)");
    }

    // ---- ProcessUpdate (0x826BD6F8) -----------------------------------------------------------------
    {
        Rig r;
        r.mEffect.mfParam_AEMS_friction_stress = 1234.75f;
        r.mEffect.ProcessUpdate();
        Check(r.mEffect.mScrapeVoice.maParameters.empty() && r.mEffect.mScrapeVoice.miGains == 0,
              "ProcessUpdate: no live voice, no parameters");
        Check(r.mEffect.maMixerInputs.size() == 1 && r.mEffect.maMixerInputs[0].first == 0 &&
                  r.mEffect.maMixerInputs[0].second == 1234,
              "ProcessUpdate: the mixer input 0 takes friction_stress (truncated) even with no live voice");

        r.mEffect.mScrapeVoice.mbLive = true;
        r.mEffect.miParam_AEMS_material_b = 2;
        r.mEffect.mfParam_AEMS_is_Crashing = 32767.0f;
        r.mEffect.ProcessUpdate();
        const std::vector<std::pair<s32, f32>>& la = r.mEffect.mScrapeVoice.maParameters;
        bool lbSix = false, lbSeven = false, lbEight = false;
        for (size_t i = 0; i < la.size(); ++i)
        {
            lbSix |= la[i].first == 6 && la[i].second == 1.0f;
            lbSeven |= la[i].first == 7 && la[i].second == 32767.0f;
            lbEight |= la[i].first == 8 && la[i].second == 333.0f;
        }
        Check(la.size() == 9 && r.mEffect.mScrapeVoice.miGains == 1 && lbSix && lbSeven && lbEight,
              "ProcessUpdate: a live voice takes the nine parameters (material_b 2 sent as 1, azimuth from the mixer) and the gain");
        Check(r.mEffect.maMixerInputs.size() == 2, "ProcessUpdate: and the mixer input again");
    }

    Check(guAsserts == 0, "no assert fired");
    std::printf("FxCrashSnd2ScrapeEffect: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
