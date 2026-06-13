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

    u32 GetSystemTimerBaseTime()
    {
        LARGE_INTEGER liCounter;
        QueryPerformanceCounter(&liCounter);
        return liCounter.LowPart;
    }

    u32 GetSystemTimerFrequency()
    {
        LARGE_INTEGER liFreq;
        QueryPerformanceFrequency(&liFreq);
        return liFreq.LowPart;
    }
}
