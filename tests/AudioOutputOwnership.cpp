// Exercise the production XAudio2 sink, its callback publications and drain barriers.
#include <Windows.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <cstdio>
#include <cstdlib>
#include "GameShared/GameClasses/System/PC/CgsAudioOutputPC.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
namespace CgsDev { namespace Log { StrStreamBase* gpDebugPrint = nullptr; } }
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { std::abort(); }
void* EndAssert() { return nullptr; }
} }
#include "audio_output_ownership.inc"

static unsigned checks, failures;
static void Check(bool value, const char* label)
{
    ++checks;
    if (!value) { ++failures; std::printf("FAIL %s\n", label); }
}
static DWORD mainThread;
static std::mutex engineGate;
static std::atomic<unsigned> calls(0), badPairs(0), mainFills(0), primaryCalls(0);
static std::atomic<bool> block(false);
static HANDLE entered, resumeFill;
static int contextA, contextB;
static void FillEngine(f32* out, int frames, void* context)
{
    if (context != &contextA) ++badPairs;
    if (::GetCurrentThreadId() == mainThread) ++mainFills;
    std::lock_guard<std::mutex> lock(engineGate);
    if (block.load()) { ::SetEvent(entered); ::WaitForSingleObject(resumeFill, 5000); }
    ++calls;
    for (int i = 0; i < frames * 2; ++i) out[i] = 1.20710677f;
}
static void FillOther(f32* out, int frames, void* context)
{
    if (context != &contextB) ++badPairs;
    ++calls;
    for (int i = 0; i < frames * 2; ++i) out[i] = 0.0f;
}
static void FillPrimary(s16* out, int frames, void*)
{
    ++primaryCalls;
    for (int i = 0; i < frames * 2; ++i) out[i] = 100;
}
static bool WaitForCalls(unsigned before)
{
    for (unsigned n = 0; n < 500 && calls.load() == before; ++n) ::Sleep(10);
    return calls.load() != before;
}
int main()
{
    using CgsSystem::AudioOutputPC;
    mainThread = ::GetCurrentThreadId();
    _putenv_s("BRN_AUDIO_MUTE", "1");
    _putenv_s("BRN_ENGINE_MIX_DIAG", "1");
    entered = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    resumeFill = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    AudioOutputPC::SetEngineFill(&FillEngine, &contextA);
    {
        // Models the System lock held during master creation. An Open that
        // synchronously invokes the engine fill deadlocks here (the old defect).
        std::lock_guard<std::mutex> lock(engineGate);
        Check(AudioOutputPC::Open(48000, 2, nullptr, nullptr), "real muted endpoint opens");
        Check(mainFills.load() == 0, "device priming never calls the engine producer");
    }
    Check(WaitForCalls(0), "callbacks continue engine production after startup");
    const unsigned before = calls.load();
    AudioOutputPC::SetFill(&FillPrimary, nullptr);
    for (unsigned n = 0; n < 4000; ++n)
    {
        AudioOutputPC::SetEngineFill(&FillOther, &contextB);
        AudioOutputPC::SetEngineFill(&FillEngine, &contextA);
        if ((n & 15) == 0) ::Sleep(1);
    }
    Check(WaitForCalls(before), "engine runs during publication churn");
    AudioOutputPC::ReleasePrimaryFill();
    const unsigned stoppedPrimary = primaryCalls.load();
    Check(WaitForCalls(calls.load()), "primary retirement preserves engine callbacks");
    Check(primaryCalls.load() == stoppedPrimary, "no primary fill can outlive its drain barrier");
    Check(badPairs.load() == 0, "callback and context are always from the same publication");
    block.store(true);
    Check(::WaitForSingleObject(entered, 5000) == WAIT_OBJECT_0, "engine fill entered drain probe");
    std::atomic<bool> drained(false);
    std::thread retire([&] { AudioOutputPC::ReleaseEngineFill(); drained.store(true); });
    ::Sleep(50);
    Check(!drained.load(), "retirement waits for the in-flight engine fill");
    block.store(false); ::SetEvent(resumeFill);
    retire.join();
    Check(drained.load(), "engine retirement completes after the fill exits");
    const unsigned stoppedEngine = calls.load();
    ::Sleep(50);
    Check(calls.load() == stoppedEngine, "no engine fill can outlive its drain barrier");
    AudioOutputPC::Close();
    Check(!AudioOutputPC::IsOpen(), "close drains callbacks and diagnostics");
    Check(mainFills.load() == 0, "engine fills never run on the startup thread");
    ::CloseHandle(entered); ::CloseHandle(resumeFill);
    std::printf("AudioOutputOwnership: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
