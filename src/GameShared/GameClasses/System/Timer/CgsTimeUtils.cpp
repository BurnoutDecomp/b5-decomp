#include "GameShared/GameClasses/System/Timer/CgsTimeUtils.h"

#include <xtl.h>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsSystem::GetSystemTimeMS         @ 0x828D75F0
//   CgsSystem::GetSystemTimerBaseTime  @ 0x828D75A0
//   CgsSystem::GetSystemTimerFrequency @ 0x828D75C8
//
// GetSystemTimerBaseTime / GetSystemTimerFrequency return the complete high-resolution
// counter and frequency, matching the eight-byte loads at the console entry points.
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

    // ARTIST 0x828D75A0 calls QueryPerformanceCounter then loads its complete qword with
    // `ld r3, var_10(r1)`. The return type is therefore u64 despite IDA's DWORD prototype.
    u64 GetSystemTimerBaseTime()
    {
        LARGE_INTEGER liCounter;
        QueryPerformanceCounter(&liCounter);
        return static_cast<u64>(liCounter.QuadPart);
    }

    // ARTIST 0x828D75C8 has the same qword return shape for QueryPerformanceFrequency.
    u64 GetSystemTimerFrequency()
    {
        LARGE_INTEGER liFreq;
        QueryPerformanceFrequency(&liFreq);
        return static_cast<u64>(liFreq.QuadPart);
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
