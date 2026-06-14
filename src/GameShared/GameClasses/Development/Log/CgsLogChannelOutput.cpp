#include "GameShared/GameClasses/Development/Log/CgsLogChannelOutput.h"

#include <cstdio>

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x8229FB20
//   (CgsDev::Log::LogChannelOutput::Append)
//
// Behaviour-faithful to the X360 pseudocode:
//     v3 = *(this + 8);
//     if (v3 == -1) return printf("%s", text);
//     else          return printf("CHANNEL %d: %s", v3, text);
//
// The channel id lives at byte offset 8 of the output object (miChannel); -1 means
// "unchannelled" and the line is printed without the CHANNEL prefix.

namespace CgsDev
{
    namespace Log
    {
        int LogChannelOutput::Append(const char* lpcText)
        {
            if (miChannel == -1)
                return printf("%s", lpcText);
            return printf("CHANNEL %d: %s", miChannel, lpcText);
        }
    }
}
