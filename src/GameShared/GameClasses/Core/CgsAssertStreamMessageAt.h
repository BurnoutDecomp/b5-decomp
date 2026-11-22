#pragma once

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"

namespace rw { namespace core { namespace debug { namespace detail { struct DebugCriticalSection; } } } }

namespace CgsDev
{
namespace Assert
{
    extern rw::core::debug::detail::DebugCriticalSection gAssertMutex;

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
