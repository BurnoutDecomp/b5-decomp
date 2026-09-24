// FX-VOICEPOOL (crash parity 2026-09-24): a crash voice must FINISH -- the collision sound's
// pitch and gain come from the collision EFFECT's own dynamic-mixer endpoint, and with them the
// splice clock reaches the end of every sample, frees its voice and lets the state detach.
//
//   CollisionEffect::GetGain            0x82688138  DWARF BrnCollisionEffect.cpp:231
//   CollisionEffect::GetPitch           0x826881B0
//   SpliceSample::Play / Update / IsPlaying / FreeVoice, Splice::Play / Update / IsPlaying
//                                       0x8268AE78 / 0x826A3A50 / 0x8268B128 / 0x826A3870,
//                                       0x826A3698 / 0x826DB470 / 0x826A3720
//
// run_fxvoicepool_finish.py extracts the two PRODUCTION CollisionEffect bodies from
// BrnCollisionEffect.cpp into the fixture effect below (its EffectBase endpoint, its control and
// the control's endpoint), and includes the PRODUCTION SpliceObjects.cpp whole. The endpoints are
// the real Nicotine::DMixIO (DMixIO.cpp + NFSMixShape.cpp compiled beside this file); only the
// splice objects' link-time collaborators (the SpliceManager pools, the rw audio-core plug-in
// events, the System lock) are recorded stubs here.
#include "types.hpp"
#include "SDKs/EATech/include/Nicotine/DMixIO.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <vector>

// ---- the production splice objects, whole -------------------------------------------------------
#include "fxvoicepool_spliceobjects.inc"

// ---- link-time collaborators of the splice objects (recorded) -----------------------------------
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

static unsigned char gau8SystemToken[64];
static int giStopEvents = 0;      // PlugIn::Event(source stage, 1 = stop)
static int giPlayEvents = 0;      // PlugIn::Event(source stage, 0 = play)
static int giMonoFrees = 0;
static int giStereoFrees = 0;
static int giSpliceAsserts = 0;
static std::vector<void*> gaStopStages;
static int giMonoFreeAtEnd = -1;  // the pools' free counts when the splice stopped playing
static int giStereoFreeAtEnd = -1;

namespace CgsSound { namespace Playback {
rw::audio::core::System* GetDefaultRwacSystem()
{
    return reinterpret_cast<rw::audio::core::System*>(gau8SystemToken);
}
RwacLock::RwacLock(rw::audio::core::System* apSystem) : mpSystem(apSystem) {}
RwacLock::~RwacLock() {}
} }

namespace rw { namespace audio { namespace core {
void RwacSystemLock(System*) {}
void RwacSystemUnlock(System*) {}
int PlugIn::Event(PlugIn* self, int aiEventId, void*)
{
    if (aiEventId == 1) { ++giStopEvents; gaStopStages.push_back(self); }
    if (aiEventId == 0) ++giPlayEvents;
    return 0;
}
PlugIn* PlugIn::SetAttribute(PlugIn* self, int, f32) { return self; }
} } }

SpliceManager* gpSpliceManager = nullptr;

SpliceManager::VoicePluginPair* SpliceManager::VoicePool::AllocateVoicePluginPairToSpliceSample(SpliceSample*)
{
    if (miPooledVoiceStackFreeIndex < 0)
        return nullptr;
    return mapVoicePluginPairsStack[miPooledVoiceStackFreeIndex--];
}
void SpliceManager::VoicePool::FreeVoicePluginPair(VoicePluginPair* apPair)
{
    mapVoicePluginPairsStack[++miPooledVoiceStackFreeIndex] = apPair;
}
void SpliceManager::CreateMonoVoice(VoicePluginPair*) {}
void SpliceManager::CreateStereoVoice(VoicePluginPair*) {}
void SpliceManager::DestroyVoice(VoicePluginPair*) {}
void SpliceManager::FreeMonoVoicePlugInPair(VoicePluginPair* apPair)
{
    ++giMonoFrees;
    mMonoVoicePool.FreeVoicePluginPair(apPair);
}
void SpliceManager::FreeStereoVoicePlugInPair(VoicePluginPair* apPair)
{
    ++giStereoFrees;
    mStereoVoicePool.FreeVoicePluginPair(apPair);
}
void* SpliceManager::Allocate(u32 luSize, const char*) { return std::malloc(luSize); }
void SpliceManager::Free(void* lpMemory) { std::free(lpMemory); }
SPLICE_Data* SpliceManager::FindSplice(SPLICE_TYPE, int) { return nullptr; }

static void SpliceAssertSink(const char* lpcMessage)
{
    ++giSpliceAsserts;
    std::printf("splice assert: %s\n", lpcMessage);
}

// ---- the collision effect: its EffectBase endpoint, its control, the control's endpoint ---------
namespace BrnSound { namespace Logic { namespace Collision {

struct CollisionControl
{
    Nicotine::DMixIO* mpDynamicMixIo = nullptr;   // the CONTROL's EffectBase endpoint
    Nicotine::DMixIO* GetDMixIOPtr() const { return mpDynamicMixIo; }
};

struct CollisionEffect
{
    struct SizeSpecificSettings
    {
        f32 mfVolume = 1.0f;
        f32 mfPitch = 1.0f;
    };

    // EffectBase::mpDynamicMixIo -- the effect's OWN endpoint (X360 EffectBase+0x30 = effect+0x34)
    Nicotine::DMixIO* mpDynamicMixIo = nullptr;
    Nicotine::DMixIO* GetDMixIOPtr() const { return mpDynamicMixIo; }

    CollisionControl* mpCollisionControl = nullptr;
    s32 meNicotineVolumeSlider = 5;
    s32 meNicotinePitchSlider = 5;
    SizeSpecificSettings mSizeSettings;

    f32 GetGain() const;
    f32 GetPitch() const;
};

#include "fxvoicepool_effect_bodies.inc"

} } }

// ---- checks ---------------------------------------------------------------------------------------
using BrnSound::Logic::Collision::CollisionControl;
using BrnSound::Logic::Collision::CollisionEffect;

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
static bool Near(f32 a, f32 b, f32 tolerance = 1e-4f)
{
    return std::fabs(a - b) <= tolerance * (std::max)(1.0f, std::fabs(b));
}

// A real Nicotine endpoint whose output block the mix map would fill: two 16-bit slots per word,
// odd slots in the high half (DMixIO::GetDMixOutput @0x82B44908).
struct Endpoint
{
    int maOutput[16];
    Nicotine::DMixIO mIO;
    Endpoint()
    {
        std::memset(maOutput, 0, sizeof(maOutput));
        mIO.m_pDMixOutputBlock = maOutput;
    }
    void Set(int aiSlot, int aiValue)
    {
        const int liShift = (aiSlot & 1) ? 16 : 0;
        unsigned int& lruWord = reinterpret_cast<unsigned int&>(maOutput[aiSlot >> 1]);
        lruWord = (lruWord & ~(0xFFFFu << liShift)) | ((static_cast<unsigned int>(aiValue) & 0xFFFFu) << liShift);
    }
};

// The collision effect InitWork<crashbin> leaves behind (0x826EB308..0x826EB314): pitch slider 1,
// volume slider = the bin's MixerSlider (6 for the walls in the live runs), size volume 1.6.
struct Rig
{
    Endpoint mEffectEndpoint;
    Endpoint mControlEndpoint;
    CollisionControl mControl;
    CollisionEffect mEffect;
    Rig(bool abEffectEndpoint, bool abControlEndpoint)
    {
        mEffect.mpDynamicMixIo = abEffectEndpoint ? &mEffectEndpoint.mIO : nullptr;
        mControl.mpDynamicMixIo = abControlEndpoint ? &mControlEndpoint.mIO : nullptr;
        mEffect.mpCollisionControl = &mControl;
        mEffect.meNicotinePitchSlider = 1;
        mEffect.meNicotineVolumeSlider = 6;
        mEffect.mSizeSettings.mfVolume = 1.6f;
        mEffect.mSizeSettings.mfPitch = 1.0f;
    }
};

// ---- the splice world: one bank, a mono and a stereo pool, one splice of two samples -------------
struct SpliceWorld
{
    alignas(SpliceManager) unsigned char mau8Manager[sizeof(SpliceManager)];
    rw::audio::core::Voice maVoices[4];
    unsigned char mau8Stages[4][4][16];
    rw::audio::core::PlugIn* mapStages[4][4];
    char mac8SampleData[64];
    s32 maiTableOfContents[4];
    SPLICE_SampleRef maRefs[2];
    SPLICE_Data mData;
    SpliceManager& Manager() { return *reinterpret_cast<SpliceManager*>(mau8Manager); }

    SpliceWorld()
    {
        std::memset(mau8Manager, 0, sizeof(mau8Manager));
        gpSpliceManager = &Manager();
        Manager().mAssertCallbackFunc = &SpliceAssertSink;
        for (int v = 0; v < 4; ++v)
        {
            maVoices[v].mucState = 0;        // active: a splice voice is never expelled on its own
            maVoices[v].miLastFrameCpuTicks = 0;
            for (int s = 0; s < 4; ++s)
                mapStages[v][s] = reinterpret_cast<rw::audio::core::PlugIn*>(mau8Stages[v][s]);
        }
        // Two pooled pairs each: mono (voices 0/1), stereo (voices 2/3).
        SpliceManager::VoicePool* lapPools[2] = { &Manager().mMonoVoicePool, &Manager().mStereoVoicePool };
        for (int p = 0; p < 2; ++p)
        {
            for (int i = 0; i < 2; ++i)
            {
                lapPools[p]->maVoicePluginPairs[i].mpVoice = &maVoices[2 * p + i];
                lapPools[p]->maVoicePluginPairs[i].mppPlugIn = mapStages[2 * p + i];
                lapPools[p]->mapVoicePluginPairsStack[i] = &lapPools[p]->maVoicePluginPairs[i];
            }
            lapPools[p]->muPooledVoiceCount = 2;
            lapPools[p]->miPooledVoiceStackFreeIndex = 1;
        }
        std::memset(mac8SampleData, 0, sizeof(mac8SampleData));
        maiTableOfContents[0] = 0; maiTableOfContents[1] = 16; maiTableOfContents[2] = 32; maiTableOfContents[3] = 48;
        Manager().m_Splices[0].mpSampleData = mac8SampleData;
        Manager().m_Splices[0].mpTableOfContents = maiTableOfContents;

        std::memset(maRefs, 0, sizeof(maRefs));
        // A mono impact sample: base pitch 1, no randomisation, 0.8 s envelope, immediate.
        maRefs[0].muSampleId = 1;
        maRefs[0].mfField04 = 1.0f;
        maRefs[0].mfBasePitch = 1.0f;
        maRefs[0].mfEnvLength = 0.8f;
        // A stereo tail (azimuth -127, KI_STEREO_SAMPLE_AZIMUTH) triggered 0.3 s later, 1.2 s long.
        maRefs[1].muSampleId = 2;
        maRefs[1].mfField04 = 1.0f;
        maRefs[1].mfBasePitch = 1.0f;
        maRefs[1].mfTriggerDelay = 0.3f;
        maRefs[1].mfAzimuth = -127.0f;
        maRefs[1].mfEnvLength = 1.2f;
        std::memset(&mData, 0, sizeof(mData));
        mData.mucNumSampleRefs = 2;
        mData.mfVolumeScale = 1.0f;
        mData.mpSampleRefList = maRefs;
    }
    int FreeMono() { return Manager().mMonoVoicePool.miPooledVoiceStackFreeIndex + 1; }
    int FreeStereo() { return Manager().mStereoVoicePool.miPooledVoiceStackFreeIndex + 1; }
};

// SplicerContentSlot::DoPlay / DoUpdatePlaying (0x826FA648 / 0x826E9D58) drive the splice with the
// voice's Pitch parameter -- the value CollisionEffect::ProcessUpdate sets from GetPitch() every
// frame (0x826BD488..0x826BD4A0). Returns the seconds until Splice::IsPlaying went false, or -1.
static f32 PlayOneImpact(SpliceWorld& arWorld, const CollisionEffect& arEffect, f32 afSeconds)
{
    const f32 lfDt = 1.0f / 60.0f;
    rw::audio::core::PlugIn* lpSubMix = reinterpret_cast<rw::audio::core::PlugIn*>(gau8SystemToken + 32);
    Splice* lpSplice = new Splice(&arWorld.mData, lpSubMix);
    lpSplice->Play(1.0f, arEffect.GetPitch(), 0.0f, 0.0f);
    rw::audio::core::System* lpSystem = CgsSound::Playback::GetDefaultRwacSystem();
    f32 lfFinished = -1.0f;
    for (int liFrame = 1; liFrame <= static_cast<int>(afSeconds / lfDt); ++liFrame)
    {
        lpSplice->Update(lpSystem, 1.0f, arEffect.GetPitch(), 0.0f, lfDt, 0.0f);
        if (!lpSplice->IsPlaying())
        {
            lfFinished = liFrame * lfDt;
            break;
        }
    }
    // Sampled BEFORE the delete: ~Splice would hand back whatever voices a stuck splice still holds.
    giMonoFreeAtEnd = arWorld.FreeMono();
    giStereoFreeAtEnd = arWorld.FreeStereo();
    delete lpSplice;
    return lfFinished;
}

int main()
{
    // ---- GetGain / GetPitch read the EFFECT's endpoint (0x82688138 / 0x826881B0) -----------------
    {
        Rig r(true, false);                         // the mix map gave the effect an endpoint, not the control
        r.mEffectEndpoint.Set(6, 16384);             // volume slider 6 at half scale (Q15)
        r.mEffectEndpoint.Set(1, 0);                 // pitch slider 1 at 0 cents -> 4096 (Q12 unity)
        Check(Near(r.mEffect.GetGain(), 1.6f * 16384.0f * 0.000030518509f),
              "GetGain: mfVolume * the EFFECT endpoint's volume slider (Q15 flt_820AA8F8) -- the effect's "
              "EffectBase::mpDynamicMixIo, read at effect+0x34");
        Check(Near(r.mEffect.GetPitch(), 1.0f),
              "GetPitch: mfPitch * the EFFECT endpoint's pitch slider (0 cents -> 4096 * flt_820AA8F4) = 1");
        r.mEffectEndpoint.Set(1, 1200);             // +1200 cents: an octave up
        Check(Near(r.mEffect.GetPitch(), 2.0f), "GetPitch: +1200 cents on the effect endpoint -> 2.0");
        r.mEffectEndpoint.Set(1, static_cast<int>(static_cast<short>(-1200)) & 0xFFFF);
        Check(Near(r.mEffect.GetPitch(), 0.5f), "GetPitch: -1200 cents (sign-extended) -> 0.5");
    }
    {
        Rig r(true, true);                          // both own endpoints: the effect's must be the one read
        r.mEffectEndpoint.Set(6, 8192);
        r.mEffectEndpoint.Set(1, static_cast<int>(static_cast<short>(-1200)) & 0xFFFF);
        r.mControlEndpoint.Set(6, 32767);
        r.mControlEndpoint.Set(1, 1200);
        Check(Near(r.mEffect.GetGain(), 1.6f * 8192.0f * 0.000030518509f),
              "GetGain: with both endpoints live the effect's volume is read, not the control's");
        Check(Near(r.mEffect.GetPitch(), 0.5f),
              "GetPitch: with both endpoints live the effect's pitch is read, not the control's");
    }
    {
        Rig r(false, true);                         // only the control has an endpoint
        r.mControlEndpoint.Set(6, 32767);
        r.mControlEndpoint.Set(1, 0);
        Check(r.mEffect.GetGain() == 0.0f,
              "GetGain: an effect with no endpoint of its own is silent (flt_82001CC0), whatever the control holds");
        Check(r.mEffect.GetPitch() == 0.0f,
              "GetPitch: an effect with no endpoint of its own has pitch 0 (flt_82001CC0 * the size pitch)");
    }
    {
        Rig r(true, false);
        r.mEffectEndpoint.Set(1, 1200);
        r.mEffect.mSizeSettings.mfPitch = 0.0f;
        Check(Near(r.mEffect.GetPitch(), 2.0f),
              "GetPitch: a size pitch of 0.0 is taken as 1.0 (0x82688210 fcmpu / 0x8268821C flt_82001C98)");
        r.mEffect.mSizeSettings.mfPitch = -0.0f;
        Check(Near(r.mEffect.GetPitch(), 2.0f), "GetPitch: -0.0 compares equal to 0.0 (fcmpu) -> 1.0");
        r.mEffect.mSizeSettings.mfPitch = 1.5f;
        Check(Near(r.mEffect.GetPitch(), 3.0f), "GetPitch: any other size pitch scales the endpoint's");
        r.mEffect.mSizeSettings.mfPitch = std::numeric_limits<f32>::quiet_NaN();
        Check(r.mEffect.GetPitch() != r.mEffect.GetPitch(), "GetPitch: a NaN size pitch is not 0 (unordered) and stays NaN");
    }

    // ---- the finish chain: the splice clock runs on that pitch --------------------------------------
    {
        SpliceWorld lWorld;
        Rig r(true, false);
        r.mEffectEndpoint.Set(6, 16384);
        r.mEffectEndpoint.Set(1, 0);
        const int liStopsBefore = giStopEvents;
        const f32 lfEnd = PlayOneImpact(lWorld, r.mEffect, 30.0f);
        Check(lfEnd > 0.0f,
              "the crash splice FINISHES: with the effect endpoint's unity pitch its samples reach "
              "(mfBasePitch / mLocPitch) * mfEnvLength and Splice::IsPlaying goes false (the slot publishes STOPPED)");
        Check(lfEnd > 1.45f && lfEnd < 1.6f,
              "it finishes when the last sample's envelope ends (0.3 s trigger delay + 1.2 s envelope)");
        Check(giStopEvents - liStopsBefore == 2,
              "each sample ends with PlugIn::Event(source stage, 1) -- the SndPlayer1 stop (0x826A3CF0)");
        Check(giMonoFreeAtEnd == 2 && giStereoFreeAtEnd == 2 && giMonoFrees == 1 && giStereoFrees == 1,
              "each finished sample hands its voice back (FreeVoice 0x826A3870: mono and stereo pools whole "
              "again while the splice object still exists) -- the next impact gets a voice");

        // The next seven impacts on the same pools: every one finishes, none leaks a voice.
        int liFinished = 0;
        for (int i = 0; i < 7; ++i)
        {
            if (PlayOneImpact(lWorld, r.mEffect, 30.0f) > 0.0f && giMonoFreeAtEnd == 2 && giStereoFreeAtEnd == 2)
                ++liFinished;
        }
        Check(liFinished == 7, "impacts 2..8 all finish and return their voices (no pool saturation)");

        r.mEffectEndpoint.Set(1, 1200);
        const f32 lfFast = PlayOneImpact(lWorld, r.mEffect, 30.0f);
        Check(lfFast > 0.0f && Near(lfFast, 0.3f + 0.6f, 0.03f),
              "an octave-up pitch runs the splice clock twice as fast (time += pitch * dt): the tail ends "
              "0.6 s after its 0.3 s trigger");
    }

    Check(guAsserts == 0 && giSpliceAsserts == 0, "no assert fired");
    std::printf("FxVoicepoolFinish: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
