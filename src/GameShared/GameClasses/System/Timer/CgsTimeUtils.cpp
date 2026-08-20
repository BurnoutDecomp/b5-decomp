#include "types.hpp"

#include <Windows.h>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsSystem::GetSystemTimeMS         @ 0x828D75F0
//   CgsSystem::GetSystemTimerBaseTime  @ 0x828D75A0
//   CgsSystem::GetSystemTimerFrequency @ 0x828D75C8
//
// Thin wrappers over the platform high-resolution timer. GetSystemTimerBaseTime /
// GetSystemTimerFrequency return the low word of the performance counter / frequency.
// GetSystemTimeMS lazily latches the base counter and frequency on first use (guarded
// by the two bits of a shared flags word) and returns elapsed milliseconds since that
// base. The X360 export rendered the final scale as a 128-bit divide; reconstructed as
// the standard (delta * 1000) / frequency.

namespace CgsSystem
{
    namespace
    {
        u32 guTimerFlags = 0;     // dword_830EA8B0
        s64 gBaseCounter = 0;     // qword_830EA8A8
        s64 gFrequency = 0;       // qword_830EA8A0
    }

    u32 GetSystemTimeMS()
    {
        if ((guTimerFlags & 1) == 0)
        {
            guTimerFlags |= 1;
            LARGE_INTEGER liNow;
            QueryPerformanceCounter(&liNow);
            gBaseCounter = liNow.QuadPart;
        }
        if ((guTimerFlags & 2) == 0)
        {
            guTimerFlags |= 2;
            LARGE_INTEGER liFreq;
            QueryPerformanceFrequency(&liFreq);
            gFrequency = liFreq.QuadPart;
        }

        LARGE_INTEGER liNow;
        QueryPerformanceCounter(&liNow);
        return static_cast<u32>((liNow.QuadPart - gBaseCounter) * 1000 / gFrequency);
    }

    // [gateui] THE CONSOLE RETURNS THE FULL 64-BIT COUNTER, not its low word.
    // GetSystemTimerBaseTime @0x828D75A0 is `bl QueryPerformanceCounter` followed by
    // `ld r3, var_10(r1)` -- an eight-byte load of the whole LARGE_INTEGER into the return
    // register (IDA types the return DWORD, which is what produced the `.LowPart` below);
    // GetSystemTimerFrequency @0x828D75C8 is the same shape. Every console consumer that
    // stores a stamp and compares it later does so in 64 bits (HudMessageDirector::
    // FilterAndSendOffMessage @0x825117E4 uses ldx/stdx/cmpld over its u64 table).
    //
    // On this host QueryPerformanceCounter runs at ~10 MHz, so the low word wraps about
    // every 7 minutes: a truncated stamp makes `last + wait > now` true for minutes at a
    // time after every wrap, silently suppressing every rate-limited HUD message. These two
    // are the console's own width and are what a stamp/compare consumer must call.
    // ⚠ The u32 pair below is kept ONLY because nine committed TUs declare it locally
    // (`namespace CgsSystem { u32 GetSystemTimerBaseTime(); }`) and MSVC mangles the return
    // type into the symbol, so widening in place would LNK2019 all of them at once. It is a
    // truncation of the same single timer read, not a second timer path. The end state is to
    // widen the originals and fix those nine declarations -- see the report.
    u64 GetSystemTimerBaseTime64()
    {
        LARGE_INTEGER liCounter;
        QueryPerformanceCounter(&liCounter);
        return static_cast<u64>(liCounter.QuadPart);
    }

    u64 GetSystemTimerFrequency64()
    {
        LARGE_INTEGER liFreq;
        QueryPerformanceFrequency(&liFreq);
        return static_cast<u64>(liFreq.QuadPart);
    }

    u32 GetSystemTimerBaseTime()
    {
        return static_cast<u32>(GetSystemTimerBaseTime64());
    }

    u32 GetSystemTimerFrequency()
    {
        return static_cast<u32>(GetSystemTimerFrequency64());
    }

    // Available physical memory in bytes - the debug RenderMemory readout. The X360 RenderMemory calls
    // GlobalMemoryStatus and formats dwAvailPhys as MB/KB/B; mirrored here (GlobalMemoryStatus saturates
    // near 4GB on large systems, which the MB/KB/B split tolerates - it is a debug number).
    u32 GetAvailablePhysicalMemoryBytes()
    {
        MEMORYSTATUS lStatus;
        lStatus.dwLength = sizeof(lStatus);
        GlobalMemoryStatus(&lStatus);
        return static_cast<u32>(lStatus.dwAvailPhys);
    }
}
