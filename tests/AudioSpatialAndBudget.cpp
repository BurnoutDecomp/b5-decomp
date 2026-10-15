#include <Windows.h>
#include <intrin.h>
#include <cmath>
#include <cstdio>
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Logic/CgsMicrophone.h"
#include "rw/audio/core/PlugIn.h"
#include "rw/audio/core/Voice.h"
#include "rw/audio/core/CpuLoadBalancer.h"
static unsigned checks, failures, assertions, expelled;
static u8 lastReason;
static void Check(bool value, const char* label)
{
    ++checks;
    if (!value) { ++failures; std::printf("FAIL %s\n", label); }
}
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++assertions; return 0; }
void* EndAssert() { return nullptr; }
} }
namespace rw { namespace audio { namespace core {
Voice* Voice::ExpelImmediate(Voice* self, u8 reason)
{ ++expelled; lastReason = reason; self->mucState = 2; return self; }
} } }
namespace CgsSound { namespace Logic {
// The production spatial bodies run against the real microphone and history
// types; only their module and mixer boundary is reduced to this bounded fixture.
struct FixtureMix { s32 values[16] = {}; s32 GetDMixInput(s32 i) const { return values[i]; } };
struct FixtureEnvironment {
    MicrophoneSystem microphones;
    MicrophoneSystem& GetMicrophoneSystem() { return microphones; }
};
struct FixtureModule {
    FixtureEnvironment environment;
    FixtureEnvironment& GetEnvironment() { return environment; }
};
struct FixtureControl {
    FixtureModule module;
    FixtureMix mix;
    bool connected = true;
    const rw::math::vpu::Vector3* mpEmitterPosition = nullptr;
    CgsSound::Utils::DataPoint<f32> mfDistanceToMic[2], mfVelocityToMic[2];
    FixtureModule* GetLogicModule() { return &module; }
    FixtureMix* GetDMixIOPtr() { return connected ? &mix : nullptr; }
    void SetMixerInputValueUnbound(s32 i, s32 value) { mix.values[i] = value; }
    s32 GetPanningAngle(MicrophoneSystem&, MicrophoneSystem::EMicPositions position)
    { return 100 + static_cast<s32>(position); }
    void UpdateDoppler(f32, s32);
    void Generate3DParams(s32);
};
} }
#include "audio_spatial_budget.inc"
int main()
{
    using namespace CgsSound::Logic;
    FixtureControl effect;
    rw::math::vpu::Vector3 emitter;
    emitter.x = 10.0f;
    emitter.y = emitter.z = emitter.w = 0.0f;
    effect.mpEmitterPosition = &emitter;
    auto& mics = effect.module.environment.microphones;
    rw::math::vpu::Matrix44Affine matrix;
    matrix.SetIdentity(); matrix.wAxis.x = 100.0f;
    mics.GetMicrophone(MicrophoneSystem::E_MIC_PLAYER, MicrophoneSystem::E_PLAYER_1)->SetMicrophoneMatrix(matrix);
    matrix.wAxis.x = 3.0f;
    mics.GetMicrophone(MicrophoneSystem::E_MIC_PLAYER, MicrophoneSystem::E_PLAYER_2)->SetMicrophoneMatrix(matrix);
    effect.mfDistanceToMic[1].Flush(10.0f); effect.mfVelocityToMic[1].Flush(1.0f);
    effect.mfDistanceToMic[0].Flush(2.0f); effect.mfVelocityToMic[0].Flush(-1.0f);
    effect.UpdateDoppler(1.0f, 1);
    Check(effect.mix.values[13] == -300 && effect.mix.values[14] == 500,
          "requested player supplies both histories in console slot order");
    Check(static_cast<u32>(effect.mix.values[15]) == 0xC0000000u,
          "both approach/recede transitions set their own console flag");
    Check(effect.mfDistanceToMic[0].GetCurrent() == 7.0f &&
          effect.mfDistanceToMic[1].GetCurrent() == 7.0f,
          "unused player's microphone cannot contaminate Doppler");
    effect.connected = false; emitter.x = 8.0f;
    effect.UpdateDoppler(0.0f, 1);
    Check(effect.mfDistanceToMic[0].GetCurrent() == 5.0f &&
          effect.mfDistanceToMic[1].GetCurrent() == 5.0f &&
          effect.mfVelocityToMic[0].GetCurrent() == -2.0f,
          "disconnected mixer still advances histories with the zero-dt convention");
    effect.connected = true;
    effect.mix.values[3] = -99;
    mics.SetNumberOfPlayers(0); effect.Generate3DParams(0);
    Check(effect.mix.values[3] == -99, "zero-player spatial gate preserves inputs");
    mics.SetNumberOfPlayers(1); effect.Generate3DParams(0);
    Check(effect.mix.values[3] == 100 && effect.mix.values[2] == 101,
          "one-player spatial path updates camera and player inputs");
    const unsigned beforeAssert = assertions;
    mics.SetNumberOfPlayers(2); effect.Generate3DParams(0);
    Check(assertions == beforeAssert + 1, "unsupported two-player path asserts");

    using namespace rw::audio::core;
    System system = {};
    CpuLoadBalancer balancer = {}; balancer.mpSystem = &system;
    Voice voices[3] = {};
    VoiceActiveNode nodes[3] = {};
    for (int i = 0; i != 3; ++i)
    {
        voices[i].mfAverageCpuTicks = 10.0f;
        voices[i].mfPriority = 10.0f + 10.0f * i;
        nodes[i].mpVoice = &voices[i];
    }
    system.mppVoiceListNodes = nodes; system.muActiveVoiceCount = 3;
    system.muFrameCounter = 5;
    CpuLoadBalancer::CullVoices(&balancer, 1000.0f);
    Check(expelled == 2 && voices[0].mucState == 2 && voices[1].mucState == 2 &&
          voices[2].mucState != 2, "culling stops once projected excess is removed");
    Check(lastReason == 2, "overload expulsion retains console reason 2");
    CpuLoadBalancer::CullVoices(&balancer, 0.0f);
    Check(expelled == 2, "zero excess never culls");
    voices[2].mfPriority = 100.0f;
    CpuLoadBalancer::CullVoices(&balancer, 1000.0f);
    Check(expelled == 2, "protected priority floor survives overload");
    LARGE_INTEGER q0, q1, frequency;
    QueryPerformanceFrequency(&frequency); QueryPerformanceCounter(&q0);
    const u32 t0 = GetCpuCycle(); Sleep(20); const u32 t1 = GetCpuCycle();
    QueryPerformanceCounter(&q1);
    const double expected = (q1.QuadPart - q0.QuadPart) * 49875000.0 / frequency.QuadPart;
    Check(std::fabs(static_cast<double>(t1 - t0) - expected) < expected * 0.02,
          "host clock reports console 49.875 MHz timebase ticks");
    std::printf("AudioSpatialAndBudget: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
