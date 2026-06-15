#include "GameShared/GameClasses/Development/Log/CgsLogChannelOutput.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cstdio>

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x8229FB20
//   (CgsDev::Log::LogChannelOutput::Append)
//
// The X360 pseudocode printf'd the text (channel-prefixed when miChannel != -1) to the
// console. Routed through the unified game log (WriteToLog -> BrnGame.log + debugger output)
// so every log sink lands in the one log file. The channel id lives at byte offset 8 of the
// output object (miChannel); -1 means "unchannelled" (no CHANNEL prefix).

namespace CgsDev
{
    namespace Log
    {
        int LogChannelOutput::Append(const char* lpcText)
        {
            if (miChannel == -1)
            {
                WriteToLog(lpcText);
            }
            else
            {
                char lacPrefix[32];
                std::snprintf(lacPrefix, sizeof(lacPrefix), "CHANNEL %d: ", miChannel);
                WriteToLog(lacPrefix);
                WriteToLog(lpcText);
            }
            return 0;
        }
    }
}
