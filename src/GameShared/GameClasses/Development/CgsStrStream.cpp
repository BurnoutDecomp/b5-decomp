#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (AppendFormat copy guard)

#include <cstdarg>
#include <cstdio>
#include <cstring>

// CgsDev::StrStreamBase / StrStream. The scalar overloads format the value with the
// engine's printf-style KAC_* format strings (here the standard equivalents) honouring the
// current PrintMode, then forward the rendered text to the virtual char* sink. Recovered from
// the DecFIGS DWARF + the X360 StrStream bodies (CgsStrStream.cpp).
namespace CgsDev
{
    namespace
    {
        const char* KAC_INTEGER    = "%d";
        const char* KAC_UNSIGNED   = "%u";
        const char* KAC_PADDED_HEX = "0x%08X";
        const char* KAC_FLOAT      = "%f";
        const s32   KI_FORMAT_BUFFER_SIZE = 64;
    }

    StrStreamBase::StrStreamBase()
        : mePrintMode(E_PRINTMODE_DECIMAL)
    {
    }

    StrStreamBase& StrStreamBase::operator<<(s32 liValue)
    {
        char lacBuffer[KI_FORMAT_BUFFER_SIZE];
        if (mePrintMode == E_PRINTMODE_DECIMAL)
        {
            std::snprintf(lacBuffer, sizeof(lacBuffer), KAC_INTEGER, liValue);
        }
        else
        {
            std::snprintf(lacBuffer, sizeof(lacBuffer), KAC_PADDED_HEX, (unsigned)liValue);
            if (mePrintMode == E_PRINTMODE_HEXONCE)
                mePrintMode = E_PRINTMODE_DECIMAL;
        }
        return *this << lacBuffer;
    }

    StrStreamBase& StrStreamBase::operator<<(u32 luValue)
    {
        char lacBuffer[KI_FORMAT_BUFFER_SIZE];
        if (mePrintMode == E_PRINTMODE_DECIMAL)
        {
            std::snprintf(lacBuffer, sizeof(lacBuffer), KAC_UNSIGNED, luValue);
        }
        else
        {
            std::snprintf(lacBuffer, sizeof(lacBuffer), KAC_PADDED_HEX, luValue);
            if (mePrintMode == E_PRINTMODE_HEXONCE)
                mePrintMode = E_PRINTMODE_DECIMAL;
        }
        return *this << lacBuffer;
    }

    StrStreamBase& StrStreamBase::operator<<(u64 luValue)
    {
        if (mePrintMode == E_PRINTMODE_DECIMAL)
            Append64IntDecimal(luValue);
        else
        {
            Append64IntHex(luValue);
            if (mePrintMode == E_PRINTMODE_HEXONCE)
                mePrintMode = E_PRINTMODE_DECIMAL;
        }
        return *this;
    }

    StrStreamBase& StrStreamBase::operator<<(f32 lfValue)
    {
        char lacBuffer[KI_FORMAT_BUFFER_SIZE];
        std::snprintf(lacBuffer, sizeof(lacBuffer), KAC_FLOAT, lfValue);
        return *this << lacBuffer;
    }

    StrStreamBase& StrStreamBase::operator<<(void* lpValue)
    {
        char lacBuffer[KI_FORMAT_BUFFER_SIZE];
        std::snprintf(lacBuffer, sizeof(lacBuffer), KAC_PADDED_HEX, (unsigned)(size_t)lpValue);
        return *this << lacBuffer;
    }

    StrStreamBase& StrStreamBase::operator<<(PrintMode leMode)
    {
        mePrintMode = leMode;
        return *this;
    }

    // 0x82817720 - render a printf-style format into a 256-byte stack buffer (X360 `v18[288]`,
    // formatted with vsnprintf capped at 256) and forward it to the virtual char* sink. The X360
    // guards that the formatted length did not exceed the destination (CgsStringUtils copy guard:
    // `lNumBytesCopied < (int32_t)luBytes`); CGS_ASSERT carries that bound here. The sink call
    // `(*(*this+4))(this, buffer)` is the virtual operator<<(const char*) override.
    void StrStreamBase::AppendFormat(const char* lpcFormat, ...)
    {
        char lacBuffer[256];

        va_list lArgs;
        va_start(lArgs, lpcFormat);
        const int liWritten = std::vsnprintf(lacBuffer, sizeof(lacBuffer), lpcFormat, lArgs);
        va_end(lArgs);

        CGS_ASSERT(liWritten < static_cast<int>(sizeof(lacBuffer)),
                   "lNumBytesCopied<(int32_t)luBytes");

        *this << lacBuffer;
    }

    void StrStreamBase::Append64IntDecimal(u64 luValue)
    {
        char lacBuffer[KI_FORMAT_BUFFER_SIZE];
        std::snprintf(lacBuffer, sizeof(lacBuffer), "%llu", (unsigned long long)luValue);
        *this << lacBuffer;
    }

    void StrStreamBase::Append64IntHex(u64 luValue)
    {
        char lacBuffer[KI_FORMAT_BUFFER_SIZE];
        std::snprintf(lacBuffer, sizeof(lacBuffer), "0x%016llX", (unsigned long long)luValue);
        *this << lacBuffer;
    }

    // @ 0x821F0158 - construct over a fixed buffer (cleared to the empty string).
    StrStream::StrStream(char* lpcBuffer, s32 liBufferSize)
        : mpcBuffer(lpcBuffer)
        , miBufferSize(liBufferSize)
    {
        if (mpcBuffer && miBufferSize > 0)
            mpcBuffer[0] = '\0';
    }

    // 0x82815E80 - the buffer sink. Walk to the NUL terminator in mpcBuffer, then strncat the new
    // text with the remaining room (miBufferSize - usedLength), exactly as the X360 does
    // (`v3 = *(this+8); while(*v4++); strncat(v3, text, *(this+12) - (v4-v3-1))`).
    void StrStream::Append(const char* lpcText)
    {
        if (mpcBuffer && lpcText && miBufferSize > 0)
        {
            const size_t luUsed = std::strlen(mpcBuffer);
            // X360 store-for-store: strncat count = *(this+12) - (v4-v3-1) = miBufferSize - usedLength
            // (NOT miBufferSize-1; the console passes the full remaining count).
            if ((size_t)miBufferSize > luUsed)
            {
                const size_t luRoom = (size_t)miBufferSize - luUsed;
                std::strncat(mpcBuffer, lpcText, luRoom);
            }
        }
    }

    // operator<<(const char*) forwards to the named sink (Append).
    StrStreamBase& StrStream::operator<<(const char* lpcText)
    {
        Append(lpcText);
        return *this;
    }

    // @ 0x827DB6E8-loop init. Resets the inline buffer to the empty string (the X360 `stb 0,8(this)`).
    // The StrStreamBase() base ctor already set mePrintMode = E_PRINTMODE_DECIMAL.
    SimpleStrStream::SimpleStrStream()
    {
        macCharBuffer[0] = '\0';
    }

    // 0x8229F6C8 - inline-buffer sink. The X360 substitutes the literal "<NULLSTRING>" for a null
    // pointer (`if (!a2) a2 = "<NULLSTRING>";`) before appending, then forwards to the virtual char*
    // sink; here that sink is this same override, which strncats into macCharBuffer without overflow.
    StrStreamBase& SimpleStrStream::operator<<(const char* lpcText)
    {
        if (!lpcText)
            lpcText = "<NULLSTRING>";

        const size_t luUsed = std::strlen(macCharBuffer);
        if ((size_t)KI_BUFFER_SIZE - 1 > luUsed)
        {
            const size_t luRoom = (size_t)KI_BUFFER_SIZE - 1 - luUsed;
            std::strncat(macCharBuffer, lpcText, luRoom);
        }
        return *this;
    }

    StrStreamBase& SimpleStrStream::operator<<(s32 liValue)
    {
        return StrStreamBase::operator<<(liValue);
    }

    StrStreamBase& SimpleStrStream::operator<<(f32 lfValue)
    {
        return StrStreamBase::operator<<(lfValue);
    }
}
