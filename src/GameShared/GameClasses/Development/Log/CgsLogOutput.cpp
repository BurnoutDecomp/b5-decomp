#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827DB720
//   (CgsDev::Log::LogOutput::Append)
//
// Behaviour-faithful to the X360 pseudocode:
//     return OutputDebugStringA(text);
//
// `this` (a1) is unused; a2 is the C string written to the platform debug
// channel. On the X360 OutputDebugStringA returned a value that was forwarded;
// the Win32 import is `void`, so the PC reconstruction returns 0 after the call.
// (Return-value divergence noted: the original forwarded the OS call result.)

// Win32 import, declared locally to avoid pulling in <windows.h>.
extern "C" __declspec(dllimport) void __stdcall OutputDebugStringA(const char* lpcOutputString);

namespace CgsDev
{
    namespace Log
    {
        struct LogOutput
        {
            int Append(const char* lpcText);
        };

        int LogOutput::Append(const char* lpcText)
        {
            OutputDebugStringA(lpcText);
            return 0;
        }
    }
}
