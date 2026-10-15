// Exercise the production movie/engine sum at the actual source-voice boundary.
#include <Windows.h>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include "GameShared/GameClasses/System/PC/CgsAudioOutputPC.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
namespace CgsDev { namespace Log { StrStreamBase* gpDebugPrint = nullptr; } }
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { std::abort(); }
void* EndAssert() { return nullptr; }
} }
#include "audio_output_transparency.inc"

static unsigned checks, failures;
static void Check(bool value, const char* label)
{
    ++checks;
    if (!value) { ++failures; std::printf("FAIL %s\n", label); }
}
static const f32 engine[] = { 0.25f, 1.25f, -1.25f, 1.25f, -1.25f,
    0.001f, 0.9f, -0.9f, std::numeric_limits<f32>::quiet_NaN(),
    std::numeric_limits<f32>::infinity(), -std::numeric_limits<f32>::infinity(), 0.25f, -0.25f };
static const s16 movie[] = { 16384, -16384, 16384, 16384, -16384, 0, 0, 0, 0, 0, 0, 0, 0 };
static const f32 expected[] = { 0.75f, 0.75f, -0.75f, 1.0f, -1.0f,
    0.001f, 0.9f, -0.9f, 0.0f, 0.0f, 0.0f, 0.25f, -0.25f };
static const char* labels[] = { "normal movie/engine sum is unchanged",
    "positive engine overrange can cancel before saturation", "negative engine overrange can cancel before saturation",
    "positive final overrange saturates", "negative final overrange saturates",
    "quiet samples retain unity gain", "positive samples retain unity gain", "negative samples retain unity gain",
    "NaN cannot reach the device", "positive infinity cannot reach the device", "negative infinity cannot reach the device",
    "left/right samples are independent", "sample signs are preserved" };
static constexpr int caseCount = sizeof(engine) / sizeof(engine[0]);
static void FillEngine(f32* out, int frames, void*)
{
    for (int i = 0; i < frames * 2; ++i) out[i] = engine[i % caseCount];
}
static void FillMovie(s16* out, int frames, void*)
{
    for (int i = 0; i < frames * 2; ++i) out[i] = movie[i % caseCount];
}
int main()
{
    using CgsSystem::AudioOutputPC;
    _putenv_s("BRN_AUDIO_MUTE", "1");
    _putenv_s("BRN_AUDIO_CAPTURE", "");
    _putenv_s("BRN_AUDIO_CAPTURE_ENGINE", "");
    _putenv_s("BRN_ENGINE_MIX_DIAG", "");
    const bool opened = AudioOutputPC::Open(48000, 2, nullptr, nullptr);
    Check(opened, "real muted endpoint opens");
    if (opened)
    {
        // StopEngine waits for the processing pass, so the test owns the buffers
        // while invoking the real submission path and inspecting its float PCM.
        CgsSystem::g_pXAudio2->StopEngine();
        AudioOutputPC::SetFill(&FillMovie, nullptr);
        AudioOutputPC::SetEngineFill(&FillEngine, nullptr);
        CgsSystem::SubmitBuffer(0);
        for (int pattern = 0; pattern < caseCount; ++pattern)
        {
            bool passed = true;
            for (int sample = pattern; sample < CgsSystem::kFrames * 2; sample += caseCount)
                passed &= CgsSystem::g_buf[0][sample] == expected[pattern];
            Check(passed, labels[pattern]);
        }
        AudioOutputPC::ReleasePrimaryFill();
        AudioOutputPC::ReleaseEngineFill();
        AudioOutputPC::Close();
        Check(!AudioOutputPC::IsOpen(), "stopped engine closes without an outstanding callback");
    }
    std::printf("AudioOutputTransparency: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
