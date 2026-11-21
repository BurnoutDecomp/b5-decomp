#pragma once

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"

namespace CgsDev
{
namespace Assert
{
    // CGS_ASSERT holds the assert mutex while this message is streamed.
    struct StreamMessageAt : MessageAt
    {
        StreamMessageAt(const char* lpcExpression, const char* lpcFile, int liLine)
            : MessageAt(lpcExpression, lpcFile, liLine)
        {
        }
    };

    inline void FireAssert(const StreamMessageAt& lrMessage, const char*, int)
    {
        CgsDev::StrStream lStream(gpcMessageBuffer, giMessageBufferSize);
        lStream << lrMessage.mpcExpression;
        FireAssert(gpcMessageBuffer, lrMessage.mpcFile, lrMessage.miLine);
    }
}
}
