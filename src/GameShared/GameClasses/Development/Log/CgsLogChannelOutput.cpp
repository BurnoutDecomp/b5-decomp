#include "GameShared/GameClasses/Development/Log/CgsLogChannelOutput.h"
#include <stdio.h>

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x8229FB20
//   (CgsDev::Log::LogChannelOutput::Append)
//
// The X360 body calls the native CRT printf directly. The channel id lives at byte offset 8
// (miChannel); -1 selects the unchannelled format.

namespace CgsDev
{
    namespace Log
    {
        // X360 0x8229FB20. The StrStreamBase char* sink writes directly with printf.
        void LogChannelOutput::Append(const char* lpcText)
        {
            if (miChannel == -1)
            {
                printf("%s", lpcText);
            }
            else
            {
                printf("CHANNEL %d: %s", miChannel, lpcText);
            }
        }
    }
}
