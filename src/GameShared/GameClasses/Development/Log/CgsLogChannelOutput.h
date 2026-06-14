#ifndef CGS_LOG_CHANNEL_OUTPUT_H
#define CGS_LOG_CHANNEL_OUTPUT_H

#include "types.hpp"

namespace CgsDev
{
namespace Log
{
struct LogChannelOutput
{
    u8  mPad[8];     // [0x00] opaque
    s32 miChannel;   // [0x08] channel id, -1 == unchannelled

    int Append(const char* lpcText);
};
}
}
#endif
