#include "GameShared/GameClasses/Development/Log/CgsLogOutput.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827DB720
//   (CgsDev::Log::LogOutput::Append)
//
// The X360 pseudocode forwarded the text to the platform debug channel
// (OutputDebugStringA). Routed through the unified game log (WriteToLog -> BrnGame.log +
// the debugger output) so every log sink lands in the one log file.

namespace CgsDev
{
    namespace Log
    {
        int LogOutput::Append(const char* lpcText)
        {
            WriteToLog(lpcText);
            return 0;
        }
    }
}
