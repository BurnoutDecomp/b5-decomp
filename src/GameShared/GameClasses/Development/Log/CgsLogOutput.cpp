#include "GameShared/GameClasses/Development/Log/CgsLogOutput.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827DB720
//   (CgsDev::Log::LogOutput::Append)
//
// Behaviour-faithful to the X360 pseudocode:
//     return OutputDebugStringA(text);
//
// `this` is unused; lpcText is the C string written to the platform debug channel.
// On the X360 OutputDebugStringA returned a value that was forwarded; the Win32
// import is `void`, so the PC reconstruction returns 0 after the call.
// (Return-value divergence noted: the original forwarded the OS call result.)

// Win32 import, declared locally to avoid pulling in <windows.h>.
extern "C" __declspec(dllimport) void __stdcall OutputDebugStringA(const char* lpcOutputString);

namespace CgsDev
{
    namespace Log
    {
        int LogOutput::Append(const char* lpcText)
        {
            OutputDebugStringA(lpcText);
            return 0;
        }
    }
}
