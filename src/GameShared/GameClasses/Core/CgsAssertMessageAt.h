#pragma once

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsDev
{
namespace Assert
{
    // A CGS_ASSERT message that carries the source location recorded in ARTIST.
    struct MessageAt
    {
        MessageAt(const char* lpcExpression, const char* lpcFile, int liLine)
            : mpcExpression(lpcExpression), mpcFile(lpcFile), miLine(liLine)
        {
        }

        const char* mpcExpression;
        const char* mpcFile;
        int miLine;
    };

    inline void FireAssert(const MessageAt& lrMessage, const char*, int)
    {
        FireAssert(lrMessage.mpcExpression, lrMessage.mpcFile, lrMessage.miLine);
    }
}
}
