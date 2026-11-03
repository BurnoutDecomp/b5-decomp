#include "GameShared/GameClasses/Development/Log/CgsLogOutput.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <xtl.h>

namespace CgsDev { namespace Log {
    namespace {
        // Reconstructed names: ARTIST initializes separate LogOutput objects
        // at 0x82F31918 and 0x82F31920 for debug and assert output.
        LogOutput gDefaultDebugOutput;
        LogOutput gDefaultAssertOutput;
    }

    StrStreamBase* gpDebugPrint = &gDefaultDebugOutput;
    StrStreamBase* gpAssertPrint = &gDefaultAssertOutput;

    // ARTIST 0x827DB720: forward directly to the platform debug output.
    void LogOutput::Append(const char* lpcText)
    {
        OutputDebugStringA(lpcText);
    }
}}

namespace CgsDev { namespace Message {
    // ARTIST 0x82F31908: the original definition's source TU is unknown.
    u64 gxMessageFilterFlags = 1;
}}
