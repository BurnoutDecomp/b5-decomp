#define NOMINMAX
#include <Windows.h>
#include <intrin.h>
#include <cstdio>
#include <type_traits>
#include "types.hpp"

namespace rw { namespace audio { namespace core {
#include "rw_audio_core_clock_source.inc"
} } }

namespace ClockMocks
{
    u64 guCounter;
    BOOL WINAPI Counter(LARGE_INTEGER* lpCounter)
    {
        lpCounter->QuadPart = static_cast<LONGLONG>(guCounter);
        return TRUE;
    }
    BOOL WINAPI Frequency10MHz(LARGE_INTEGER* lpFrequency)
    {
        lpFrequency->QuadPart = 10000000;
        return TRUE;
    }
    BOOL WINAPI FrequencyOne(LARGE_INTEGER* lpFrequency)
    {
        lpFrequency->QuadPart = 1;
        return TRUE;
    }
    BOOL WINAPI FrequencyMax(LARGE_INTEGER* lpFrequency)
    {
        lpFrequency->QuadPart = 0x7FFFFFFFFFFFFFFFll;
        return TRUE;
    }
}

// Each complete production body has its own immutable frequency cache.
#define QueryPerformanceCounter ClockMocks::Counter
#define QueryPerformanceFrequency ClockMocks::Frequency10MHz
namespace Clock10MHz {
#include "rw_audio_core_clock_source.inc"
}
#undef QueryPerformanceFrequency
#define QueryPerformanceFrequency ClockMocks::FrequencyOne
namespace ClockOne {
#include "rw_audio_core_clock_source.inc"
}
#undef QueryPerformanceFrequency
#define QueryPerformanceFrequency ClockMocks::FrequencyMax
namespace ClockMax {
#include "rw_audio_core_clock_source.inc"
}
#undef QueryPerformanceFrequency
#undef QueryPerformanceCounter

struct ClockCase
{
    int miClock;
    u64 muCounter;
    u32 muExpected;
};
#include "rw_audio_core_clock_cases.inc"

static int siChecks, siFailures;
static void Check(bool lbCondition, const char* lpcName)
{
    ++siChecks;
    if (!lbCondition)
    {
        ++siFailures;
        std::printf("FAIL %s\n", lpcName);
    }
}

int main()
{
    static_assert(std::is_same<decltype(rw::audio::core::GetCpuCycle()), u32>::value,
                  "The original counter return domain is u32");

    // GetTickCount64 is an independent millisecond wall-clock contract, not
    // another evaluation of the production QPC scaling expression.
    const ULONGLONG luWallStart = GetTickCount64();
    const u32 luTickStart = rw::audio::core::GetCpuCycle();
    Sleep(750);
    const u32 luTickEnd = rw::audio::core::GetCpuCycle();
    const ULONGLONG luWallEnd = GetTickCount64();
    const u32 luTicks = luTickEnd - luTickStart;
    const double ldMilliseconds = static_cast<double>(luWallEnd - luWallStart);
    const double ldRate = ldMilliseconds > 0.0 ? luTicks * 1000.0 / ldMilliseconds : 0.0;
    std::printf("Independent wall-time: %.0f ms, %u ticks, %.3f ticks/second\n",
                ldMilliseconds, luTicks, ldRate);
    Check(ldRate >= 49875000.0 * 0.96 && ldRate <= 49875000.0 * 1.04,
          "native clock advances at the original 49,875,000 Hz rate (4% coarse-clock tolerance)");

#ifndef RWAC_CLOCK_RATE_ONLY
    for (const ClockCase& lrCase : KAC_CLOCK_CASES)
    {
        ClockMocks::guCounter = lrCase.muCounter;
        const u32 luActual = lrCase.miClock == 0 ? Clock10MHz::GetCpuCycle()
            : lrCase.miClock == 1 ? ClockOne::GetCpuCycle() : ClockMax::GetCpuCycle();
        ++siChecks;
        if (luActual != lrCase.muExpected)
        {
            ++siFailures;
            std::printf("FAIL clock=%d counter=%llu: got=%u expected=%u\n",
                        lrCase.miClock, lrCase.muCounter, luActual, lrCase.muExpected);
        }
    }
#endif

    std::printf("RwAudioCoreClockRateRegression: %d checks, %d failures\n", siChecks, siFailures);
    return siFailures ? 1 : 0;
}
