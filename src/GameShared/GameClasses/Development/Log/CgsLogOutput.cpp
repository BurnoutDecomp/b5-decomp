#include "GameShared/GameClasses/Development/Log/CgsLogOutput.h"
#include <xtl.h>

namespace CgsDev { namespace Log {
    // ARTIST 0x827DB720: forward directly to the platform debug output.
    void LogOutput::Append(const char* lpcText)
    {
        OutputDebugStringA(lpcText);
    }
}}
