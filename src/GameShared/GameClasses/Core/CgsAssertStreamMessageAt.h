#pragma once

// RETIRED (D12, 2026-11-26): no call site uses this wrapper any more. Write CGS_ASSERT_AT /
// CGS_ASSERT_STREAM_AT / CGS_FAIL_AT / CGS_FAIL_STREAM_AT directly; a wrapper site compiles
// its FireAssert overload out of line, which is not ARTIST's code. Old code converts as
// CGS_ASSERT(c, CGS_ASSERT_[STREAM_]MESSAGE_AT(m, f, l)) -> CGS_ASSERT[_STREAM]_AT(c, m, f, l).

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

#define CGS_ASSERT_STREAM_MESSAGE_AT(msg, artistFile, artistLine) \
    CgsDev::Assert::StreamMessageAt(msg, CGS_ASSERT_LOCATION(artistFile, artistLine))
