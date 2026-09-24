// FX-EMITTER (crash parity 2026-09-24): the world emitter's per-frame mix -- EmitterEffect::ProcessUpdate
// against the ARTIST machine code.
//
//   BrnSound::Logic::World::EmitterEffect::ProcessUpdate   0x826E6BC8   (DWARF BrnEmitterEffect.cpp:156)
//
// The console measures the listener (module +0x29E0) against the entity through the entity radius
// (W & 0xFFFF, a reciprocal `fdivs 1.0,R`), clamps t = d/R to [0, 1] (two `fsel`s) and scales the
// mixer volume by Curve::GetOutput(1 - t, E_ONE_MINUS_EQPWR) -- inlined at 0x826E6D70..0x826E6DE4:
// the CgsSoundUtils.cpp:161 input assert, `fmsubs f0,f30,511,511 ; fctiwz ; slwi 2 ; subf` into the
// quarter-sine table at 0x82F2D920, `fsubs f0,1.0,f0`. The PC used a linear 1 - t (and a radius > 0
// guard of its own), and dropped the `mi16PitchOutput < E_WORLD_EMITTER_EFFECT_OUTPUT_AZIMUTH` assert
// (l.185, `cmpwi cr6,r9,3`).
//
// run_fxemitter_process_update.py extracts the PRODUCTION ProcessUpdate body (and the file's anonymous
// namespace, which names the azimuth output) and compiles it against a fixture effect whose mixer,
// voice and Curve record what they are asked. StaticSoundEntity is the REAL BrnStaticSoundMap.h.
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "rw/math/vpu/vector3_operation.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SharedClasses/Sound/World/BrnStaticSoundMap.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

static unsigned guAsserts = 0;
static std::vector<std::string> gaAssertTexts;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcExpression, const char*, int) { ++guAsserts; gaAssertTexts.push_back(lpcExpression); return 0; }
void* EndAssert() { return nullptr; }
} }

namespace CgsSound { namespace Playback {
struct Name
{
    static uintptr_t MakeHash(const char* lpcText)
    {
        if (!lpcText || !*lpcText)
            return 0;
        u32 lu = 2166136261u;
        for (; *lpcText; ++lpcText)
            lu = (lu ^ static_cast<u8>(*lpcText)) * 16777619u;
        return lu;
    }
};
} }

// The console's Curve::GetOutput @0x82689698, E_ONE_MINUS_EQPWR arm: 1 - table[-(int)(x*511 - 511)],
// the 512-entry table at 0x82F2D920 being sin(i * pi / 1024) (to ~5e-6). Recorded per call.
namespace CgsSound { namespace Utils {
struct Curve
{
    enum ECurveType
    {
        E_LINEAR             = 0,
        E_POWER              = 1,
        E_EQ_PWR_SQ          = 2,
        E_ONE_MINUS_EQPWR    = 3,
        E_ONE_MINUS_EQPWR_SQ = 4,
    };
    static std::vector<std::pair<f32, int>> saCalls;
    static f32 Console(f32 lfInput, ECurveType leCurve)
    {
        if (leCurve != E_ONE_MINUS_EQPWR)
            return -1.0f;
        const int liIndex = -static_cast<int>(lfInput * 511.0f - 511.0f);
        return 1.0f - static_cast<f32>(std::sin(liIndex * 3.14159265358979323846 / 1024.0));
    }
    static f32 GetOutput(f32 lfInput, ECurveType leCurve)
    {
        saCalls.push_back(std::make_pair(lfInput, static_cast<int>(leCurve)));
        return Console(lfInput, leCurve);
    }
};
std::vector<std::pair<f32, int>> Curve::saCalls;
} }

namespace CgsSound { namespace Logic {
class Module { public: virtual ~Module() {} };

struct VoiceWrapper
{
    std::vector<std::pair<s32, f32>> maParameters;
    std::vector<std::pair<u32, f32>> maGains;
    int miUpdates = 0;
    void SetParameter(s32 liIndex, f32 lfValue, const u32*) { maParameters.push_back(std::make_pair(liIndex, lfValue)); }
    void SetGain(u32 luSend, f32 lfGain, const u32*) { maGains.push_back(std::make_pair(luSend, lfGain)); }
    void Update() { ++miUpdates; }
};

struct EffectBase
{
    Module* mpLogicModule = nullptr;
    std::map<std::pair<int, int>, f32> maMixer;   // (output slot, preset) -> scaled mixer value
    std::vector<std::pair<int, int>> maReads;
    f32 GetRWACMixerOutputValue(int liSlot, int liPreset)
    {
        maReads.push_back(std::make_pair(liSlot, liPreset));
        std::map<std::pair<int, int>, f32>::const_iterator lIt = maMixer.find(std::make_pair(liSlot, liPreset));
        return lIt == maMixer.end() ? 0.0f : lIt->second;
    }
};
} }

namespace BrnSound { namespace Module {
struct FrameInformationFixture
{
    struct PlayerTransform
    {
        Vector3 mPos;
        Vector3 Pos() const { return mPos; }
    } mPlayerTransform;
};
struct SoundLogicModule : public CgsSound::Logic::Module
{
    FrameInformationFixture mFrame;
    FrameInformationFixture& GetFrameInformation() { return mFrame; }
};
} }

namespace BrnSound { namespace Logic { namespace World {

#include "fxemitter_process_update_names.inc"

struct EmitterEffect : public CgsSound::Logic::EffectBase
{
    CgsSound::Logic::VoiceWrapper mVoice;
    Vector3 mPos;
    s16 mi16PitchOutput = 1;
    BrnSound::World::StaticSoundEntity mEntity;
    const BrnSound::World::StaticSoundEntity& GetSoundEntity() const { return mEntity; }
    void ProcessUpdate();
};

#include "fxemitter_process_update_body.inc"

} } }

namespace
{
unsigned guChecks = 0;
unsigned guFailures = 0;

void Check(bool lbPass, const char* lpcLabel)
{
    ++guChecks;
    if (!lbPass)
        ++guFailures;
    std::printf("%s %s\n", lbPass ? "ok   " : "FAIL ", lpcLabel);
}

bool Near(f32 lfA, f32 lfB) { return std::fabs(lfA - lfB) <= 1.0e-5f; }

struct Frame
{
    f32 mfGain0;
    f32 mfGain1;
    bool mbOneGainEach;
    std::vector<std::pair<f32, int>> maCurve;
    std::vector<std::pair<s32, f32>> maParameters;
    int miUpdates;
    unsigned muAsserts;
    std::vector<std::string> maAssertTexts;
};

// One frame of an emitter at (100, 0, 200), radius luRadius, with the listener lfDistance away along +x.
Frame Update(f32 lfDistance, u32 luRadius, s16 li16PitchOutput = 1)
{
    BrnSound::Module::SoundLogicModule lModule;
    lModule.mFrame.mPlayerTransform.mPos = Vector3{ 100.0f + lfDistance, 0.0f, 200.0f, 0.0f };

    BrnSound::Logic::World::EmitterEffect lEffect;
    lEffect.mpLogicModule = &lModule;
    lEffect.mi16PitchOutput = li16PitchOutput;
    lEffect.mEntity.mPosPlus.x = 100.0f;
    lEffect.mEntity.mPosPlus.y = 0.0f;
    lEffect.mEntity.mPosPlus.z = 200.0f;
    const u32 luPacked = (29u << 16) | luRadius;
    std::memcpy(&lEffect.mEntity.mPosPlus.w, &luPacked, sizeof(luPacked));
    lEffect.maMixer[std::make_pair(0, 0)] = 0.8f;    // volume
    lEffect.maMixer[std::make_pair(4, 0)] = 0.3f;    // reverb
    lEffect.maMixer[std::make_pair(1, 1)] = 1.1f;    // pitch
    lEffect.maMixer[std::make_pair(2, 1)] = 1.3f;    // Doppler pitch
    lEffect.maMixer[std::make_pair(3, 3)] = 45.0f;   // azimuth

    CgsSound::Utils::Curve::saCalls.clear();
    guAsserts = 0;
    gaAssertTexts.clear();
    lEffect.ProcessUpdate();

    Frame lFrame;
    lFrame.mbOneGainEach = lEffect.mVoice.maGains.size() == 2 && lEffect.mVoice.maGains[0].first == 0 &&
                           lEffect.mVoice.maGains[1].first == 1;
    lFrame.mfGain0 = lFrame.mbOneGainEach ? lEffect.mVoice.maGains[0].second : -1.0f;
    lFrame.mfGain1 = lFrame.mbOneGainEach ? lEffect.mVoice.maGains[1].second : -1.0f;
    lFrame.maCurve = CgsSound::Utils::Curve::saCalls;
    lFrame.maParameters = lEffect.mVoice.maParameters;
    lFrame.miUpdates = lEffect.mVoice.miUpdates;
    lFrame.muAsserts = guAsserts;
    lFrame.maAssertTexts = gaAssertTexts;
    return lFrame;
}

f32 ConsoleGain(f32 lfFraction)
{
    return CgsSound::Utils::Curve::Console(1.0f - lfFraction, CgsSound::Utils::Curve::E_ONE_MINUS_EQPWR) * 0.8f;
}
} // namespace

int main()
{
    const u32 luRadius = 173u;   // TRK_UNIT146's TrainStation1 emitter
    const f32 kafFractions[3] = { 0.25f, 0.5f, 0.75f };
    for (f32 lfFraction : kafFractions)
    {
        const Frame lFrame = Update(lfFraction * 173.0f, luRadius);
        char lacLabel[200];
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "t = %.2f: the fall-off is Curve::GetOutput(1 - t, E_ONE_MINUS_EQPWR), called once", lfFraction);
        Check(lFrame.maCurve.size() == 1 && lFrame.maCurve[0].second == 3 &&
              Near(lFrame.maCurve[0].first, 1.0f - lfFraction), lacLabel);
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "t = %.2f: send 0 gain = (1 - sin(t*pi/2)) * volume = %.4f (linear would be %.4f)",
                      lfFraction, ConsoleGain(lfFraction), (1.0f - lfFraction) * 0.8f);
        Check(lFrame.mbOneGainEach && Near(lFrame.mfGain0, ConsoleGain(lfFraction)), lacLabel);
    }

    const Frame lAtEmitter = Update(0.0f, luRadius);
    Check(lAtEmitter.mbOneGainEach && Near(lAtEmitter.mfGain0, 0.8f), "at the emitter: full volume (0.8)");
    const Frame lAtRadius = Update(173.0f, luRadius);
    Check(lAtRadius.mbOneGainEach && Near(lAtRadius.mfGain0, 0.0f), "at the radius: silent");
    const Frame lBeyond = Update(300.0f, luRadius);
    Check(lBeyond.mbOneGainEach && Near(lBeyond.mfGain0, 0.0f), "beyond the radius: t clamps to 1, silent");

    const Frame lMix = Update(40.0f, luRadius);
    Check(lMix.mbOneGainEach && Near(lMix.mfGain1, 0.3f), "send 1 (reverb) = the reverb mixer output, unscaled by distance");
    Check(lMix.maParameters.size() == 2 && lMix.maParameters[0].first == 1 && Near(lMix.maParameters[0].second, 45.0f) &&
          lMix.maParameters[1].first == 0 && Near(lMix.maParameters[1].second, 1.1f),
          "SetParameter(1, azimuth output 3), then SetParameter(0, pitch output mi16PitchOutput)");
    Check(lMix.miUpdates == 1 && lMix.muAsserts == 0, "one voice Update, no assert");

    const Frame lDoppler = Update(40.0f, luRadius, 2);
    Check(lDoppler.maParameters.size() == 2 && Near(lDoppler.maParameters[1].second, 1.3f) && lDoppler.muAsserts == 0,
          "pitch output 2 (Doppler): the pitch comes from mixer output 2, no assert");

    const Frame lBadPitch = Update(40.0f, luRadius, 3);
    Check(lBadPitch.muAsserts == 1 && lBadPitch.maAssertTexts.size() == 1 &&
          lBadPitch.maAssertTexts[0] == "mi16PitchOutput < E_WORLD_EMITTER_EFFECT_OUTPUT_AZIMUTH",
          "pitch output 3: `mi16PitchOutput < E_WORLD_EMITTER_EFFECT_OUTPUT_AZIMUTH` fires (l.185, cmpwi 3)");

    std::printf("FxEmitterProcessUpdate: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
