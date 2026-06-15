#include "GameShared/GameClasses/Development/CgsStrStream.h"

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

    // Append text to the buffer, never overflowing it.
    StrStreamBase& StrStream::operator<<(const char* lpcText)
    {
        if (mpcBuffer && lpcText && miBufferSize > 0)
        {
            const size_t luUsed = std::strlen(mpcBuffer);
            const size_t luRoom = (size_t)miBufferSize - 1 - luUsed;
            if (luRoom > 0)
                std::strncat(mpcBuffer, lpcText, luRoom);
        }
        return *this;
    }
}
