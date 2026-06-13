#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsDev::Log::LogCombined::AddStream @ 0x82817660
//   CgsDev::Log::LogCombined::Append    @ 0x82817698
//
// LogCombined is a composite log sink: it holds up to four child Stream pointers
// (array at this+8) and fans every Append() out to each non-null child, calling
// the child's Append virtual (vtable slot 1, +4) with the text (or "<NULLSTRING>"
// when passed null). AddStream stores into the first empty slot, silently
// dropping once all four are taken. Being a Stream itself, a LogCombined can be
// nested inside another.

namespace CgsDev
{
    namespace Log
    {
        class Stream
        {
        public:
            virtual void Flush() = 0;                       // vtable slot 0 (+0)
            virtual int  Append(const char* pcString) = 0;  // vtable slot 1 (+4)

        protected:
            u32 muReserved;   // [+4] Stream base bookkeeping (untouched here)
        };

        class LogCombined : public Stream
        {
            static const u32 KU_MAX_STREAMS = 4;

            Stream* mapStreams[KU_MAX_STREAMS];   // [+8]

        public:
            void Flush() override {}
            int  Append(const char* pcString) override;

            LogCombined* AddStream(Stream* pStream);
        };

        LogCombined* LogCombined::AddStream(Stream* pStream)
        {
            u32 uCount = 0;
            for (Stream** ppStream = mapStreams; *ppStream != nullptr; ++ppStream)
            {
                if (++uCount >= KU_MAX_STREAMS)
                    return this;   // all slots full - drop
            }

            mapStreams[uCount] = pStream;
            return this;
        }

        int LogCombined::Append(const char* pcString)
        {
            int iResult = 0;

            Stream** ppStream = mapStreams;
            for (u32 i = 0; i < KU_MAX_STREAMS; ++i, ++ppStream)
            {
                Stream* pStream = *ppStream;
                if (pStream)
                {
                    const char* pcText = pcString ? pcString : "<NULLSTRING>";
                    iResult = pStream->Append(pcText);
                }
            }

            return iResult;
        }
    }
}
