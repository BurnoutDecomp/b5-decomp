#ifndef CGS_LOG_CHANNEL_OUTPUT_H
#define CGS_LOG_CHANNEL_OUTPUT_H

#include "types.hpp"
#include <stdio.h>

namespace CgsDev
{
namespace Log
{
class LogChannelOutput
{
public:
    int Append(int a1, const char *a2);
};

inline int LogChannelOutput::Append(int a1, const char *a2)
{
    int v3 = *(int*)(a1 + 8);
    if ( v3 == -1 )
        return printf("%s", a2);
    else
        return printf("CHANNEL %d: %s", v3, a2);
}

}
}
#endif
