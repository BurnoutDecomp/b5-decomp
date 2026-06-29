#include "GameShared/GameClasses/Development/Log/CgsLogCombined.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsDev::Log::LogCombined::AddStream @ 0x82817660
//   CgsDev::Log::LogCombined::Append    @ 0x82817698
//
// LogCombined is a StrStreamBase composite sink: it holds up to KI_MAX_COMBINED_STREAMS child
// stream pointers (array at this+8) and fans every Append() out to each non-null child, streaming
// the text through the child's char* sink (or "<NULLSTRING>" when passed null). AddStream stores
// into the first empty slot, silently dropping once all slots are taken. Being a StrStreamBase
// itself, a LogCombined can be nested inside another.

namespace CgsDev
{
    namespace Log
    {
        // X360 0x82817660. Store lpStream in the first empty slot; drop once all slots are full.
        void LogCombined::AddStream(StrStreamBase* lpStream)
        {
            for (s32 li = 0; li < KI_MAX_COMBINED_STREAMS; ++li)
            {
                if (mapStreams[li] == nullptr)
                {
                    mapStreams[li] = lpStream;
                    return;
                }
            }
        }

        // CgsLogCombined.cpp:74. Drop lpStream from whichever slot holds it.
        void LogCombined::RemoveStream(StrStreamBase* lpStream)
        {
            for (s32 li = 0; li < KI_MAX_COMBINED_STREAMS; ++li)
            {
                if (mapStreams[li] == lpStream)
                {
                    mapStreams[li] = nullptr;
                    return;
                }
            }
        }

        // CgsLogCombined.cpp:123. Clear every child slot.
        void LogCombined::RemoveAllStreams()
        {
            for (s32 li = 0; li < KI_MAX_COMBINED_STREAMS; ++li)
                mapStreams[li] = nullptr;
        }

        // X360 0x82817698. Fan the text out to every non-null child stream's char* sink (the X360
        // calls the child's virtual char* sink; null text becomes "<NULLSTRING>").
        void LogCombined::Append(const char* lpcText)
        {
            const char* lpcOut = lpcText ? lpcText : "<NULLSTRING>";
            for (s32 li = 0; li < KI_MAX_COMBINED_STREAMS; ++li)
            {
                if (mapStreams[li] != nullptr)
                {
                    *mapStreams[li] << lpcOut;
                }
            }
        }
    }
}
