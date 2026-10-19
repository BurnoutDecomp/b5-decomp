#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <initializer_list>
#include "types.hpp"
#define CGS_ASSERT(c,m) do { if (!(c)) std::abort(); } while (0)
namespace CgsDev { namespace Log {
struct Sink { template<class T> Sink& operator<<(const T&) { return *this; } };
Sink* gpDebugPrint = nullptr;
} }
namespace Nicotine { struct DMixIO { enum { DMX_VOL = 0, DMX_PITCH = 1 }; }; }
namespace BrnSound { namespace Vehicles { namespace Engines {
static f32 KF_HARDCODE_RPM = 0, KF_DEBUG_SHUFFLE_PITCH = 1;
static s32 KI_DEBUG_PITCH_BELOW_IDLE_DECEL_GINSU = 0;
static u32 guGinsuPauseName, guSend01Name, guGinsuFrequencyName,
           guGinsuSetPitchName, guGinsuSetShuffleWidthName;
struct HybridControl {
    f32 rpm = 1000;
    struct { f32 AccelGinsu = .75f, DecelGinsu = .5f; } volume;
    f32 GetGinsuRPM() const { return rpm; }
    const decltype(volume)& GetFinalEngineVolume() const { return volume; }
};
struct EngineControl { f32 pitch = 1; f32 GetAudioPitch() const { return pitch; } };
struct Voice {
    f32 parameters[4] = {}, gain = 0;
    void SetParameter(int slot, f32 value, const u32*) { parameters[slot] = value; }
    void SetGain(int, f32 value, const u32*) { gain = value; }
};
struct DualGinsuEffect {
    HybridControl* mpHybridControl;
    EngineControl* mpEngineControl;
    f32 mfMinRpm = 2000, mfDecelMinRpm = 1800;
    struct Point { f32 value; void Update(f32 v) { value = v; } } mfAccelGinsuRPM, mfDecelGinsuRPM;
    Voice mAccelGinsuVoice, mDecelGinsuVoice;
    f32 mixerPitch = 1.25f, mixerVolume = .8f;
    f32 GetRWACMixerOutputValue(int, int type) const { return type == 1 ? mixerPitch : mixerVolume; }
    void UpdateAccelGinsu();
    void UpdateDecelGinsu();
};
} } }
#include "audio_engine_pitch.inc"
static int checks, failures;
static void Near(f32 value, f32 wanted, const char* name) {
    ++checks;
    if (!std::isfinite(value) || std::fabs(value-wanted) > .0001f) { ++failures; std::printf("FAIL %s: %f != %f\n",name,value,wanted); }
}
int main() {
    using namespace BrnSound::Vehicles::Engines;
    HybridControl hybrid; EngineControl engine;
    DualGinsuEffect effect; effect.mpHybridControl = &hybrid; effect.mpEngineControl = &engine;
    for (f32 rpm : {0.f, 1000.f, 1999.f, 2000.f, 4500.f}) {
        hybrid.rpm = rpm; effect.UpdateAccelGinsu();
        Near(effect.mAccelGinsuVoice.parameters[0], rpm < 2000 ? 2000 : rpm, "accel frequency floor");
        Near(effect.mAccelGinsuVoice.parameters[1], 1.25f * (rpm < 2000 ? rpm/2000 : 1), "accel below-minimum pitch");
        Near(effect.mAccelGinsuVoice.gain, .6f, "accel gain unaffected by RPM floor");
        Near(effect.mAccelGinsuVoice.parameters[0] * effect.mAccelGinsuVoice.parameters[1],
             rpm * 1.25f, "RPM is scaled exactly once");
    }
    hybrid.rpm = 900; effect.UpdateDecelGinsu();
    Near(effect.mDecelGinsuVoice.parameters[0], 900, "decel option off retains raw RPM");
    Near(effect.mDecelGinsuVoice.parameters[1], 1.25f, "decel option off retains pitch");
    KI_DEBUG_PITCH_BELOW_IDLE_DECEL_GINSU = 1;
    for (f32 rpm : {900.f, 1800.f, 4000.f}) {
        hybrid.rpm = rpm; effect.UpdateDecelGinsu();
        Near(effect.mDecelGinsuVoice.parameters[0], rpm < 1800 ? 1800 : rpm, "decel frequency floor");
        Near(effect.mDecelGinsuVoice.parameters[1], 1.25f * (rpm < 1800 ? rpm/1800 : 1), "decel option scales pitch");
        Near(effect.mDecelGinsuVoice.gain, .4f, "decel option preserves gain");
    }
    KF_HARDCODE_RPM = 5000; effect.UpdateAccelGinsu();
    Near(effect.mAccelGinsuVoice.parameters[0], 5000, "original RPM override still works");
    std::printf("AudioEnginePitch: %d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
