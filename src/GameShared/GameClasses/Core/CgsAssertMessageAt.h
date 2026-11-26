#pragma once

// RETIRED (D12, 2026-11-26): no call site uses this wrapper any more. Write CGS_ASSERT_AT /
// CGS_ASSERT_STREAM_AT / CGS_FAIL_AT / CGS_FAIL_STREAM_AT directly; a wrapper site compiles
// its FireAssert overload out of line, which is not ARTIST's code. Old code converts as
// CGS_ASSERT(c, CGS_ASSERT_[STREAM_]MESSAGE_AT(m, f, l)) -> CGS_ASSERT[_STREAM]_AT(c, m, f, l).

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

#if defined(CGS_MATCH_ASSERT_LOCATIONS) && CGS_MATCH_ASSERT_LOCATIONS
#define CGS_ASSERT_MESSAGE_AT(msg, artistFile, artistLine) CgsDev::Assert::MessageAt(msg, artistFile, artistLine)
#else
#define CGS_ASSERT_MESSAGE_AT(msg, artistFile, artistLine) (msg)
#endif
