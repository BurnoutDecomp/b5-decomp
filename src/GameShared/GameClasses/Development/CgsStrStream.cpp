#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "rw/core/stdc/stdc.h"
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

        // XDK exposes the formatter used by AppendFormat as vsnprintf.
        int Format(char* lpBuffer, size_t luSize, const char* lpcFormat, ...)
        {
            va_list lArgs;
            va_start(lArgs, lpcFormat);
            const int liResult = vsnprintf(lpBuffer, luSize, lpcFormat, lArgs);
            va_end(lArgs);
            return liResult;
        }
    }

    StrStreamBase::StrStreamBase()
        : mePrintMode(E_PRINTMODE_DECIMAL)
    {
    }

    StrStreamBase& StrStreamBase::operator<<(const char* lpcText)
    {
        Append(lpcText ? lpcText : "<NULLSTRING>");
        return *this;
    }

    // ARTIST821F0E50 / 821F0EC8: 0 decimal, 1/2 hex, other modes silent.
    StrStreamBase& StrStreamBase::operator<<(s32 liValue)
    {
        if (mePrintMode == E_PRINTMODE_DECIMAL)
            AppendFormat("%d", liValue);
        else if (static_cast<u32>(mePrintMode) < 3)
        {
            AppendFormat("0x%X", static_cast<u32>(liValue));
            if (mePrintMode == E_PRINTMODE_HEXONCE) mePrintMode = E_PRINTMODE_DECIMAL;
        }
        return *this;
    }

    StrStreamBase& StrStreamBase::operator<<(u32 luValue)
    {
        if (mePrintMode == E_PRINTMODE_DECIMAL)
            AppendFormat("%u", luValue);
        else if (static_cast<u32>(mePrintMode) < 3)
        {
            AppendFormat("0x%X", luValue);
            if (mePrintMode == E_PRINTMODE_HEXONCE) mePrintMode = E_PRINTMODE_DECIMAL;
        }
        return *this;
    }

    StrStreamBase& StrStreamBase::operator<<(u64 luValue)
    {
        if (mePrintMode == E_PRINTMODE_DECIMAL)
            Append64IntDecimal(luValue);
        else if (static_cast<u32>(mePrintMode) < 3)
        {
            Append64IntHex(luValue);
            if (mePrintMode == E_PRINTMODE_HEXONCE)
                mePrintMode = E_PRINTMODE_DECIMAL;
        }
        return *this;
    }

    // ARTIST821F0F40: hexadecimal formats the original float's raw bits.
    StrStreamBase& StrStreamBase::operator<<(f32 lfValue)
    {
        if (mePrintMode == E_PRINTMODE_DECIMAL)
            AppendFormat("%f", lfValue);
        else if (static_cast<u32>(mePrintMode) < 3)
        {
            u32 luBits;
            std::memcpy(&luBits, &lfValue, sizeof(luBits));
            AppendFormat("0x%X", luBits);
            if (mePrintMode == E_PRINTMODE_HEXONCE) mePrintMode = E_PRINTMODE_DECIMAL;
        }
        return *this;
    }

    StrStreamBase& StrStreamBase::operator<<(void* lpValue)
    {
        char lacBuffer[KI_FORMAT_BUFFER_SIZE];
        Format(lacBuffer, sizeof(lacBuffer), KAC_PADDED_HEX, (unsigned)(size_t)lpValue);
        return *this << lacBuffer;
    }

    StrStreamBase& StrStreamBase::operator<<(PrintMode leMode)
    {
        mePrintMode = leMode;
        return *this;
    }

    StrStreamBase& StrStreamBase::operator<<(bool lbValue)
    {
        return *this << (lbValue ? "true" : "false");
    }

    // 0x82817720 - render a printf-style format into a 256-byte stack buffer (X360 `v18[288]`,
    // formatted with vsnprintf capped at 256) and forward it to the virtual char* sink. The X360
    // guards that the formatted length did not exceed the destination (CgsStringUtils copy guard:
    // `lNumBytesCopied < (int32_t)luBytes`); CGS_ASSERT carries that bound here. The sink call
    // `(*(*this+4))(this, buffer)` is virtual Append.
    void StrStreamBase::AppendFormat(const char* lpcFormat, ...)
    {
        char lacBuffer[256];

        va_list lArgs;
        va_start(lArgs, lpcFormat);
        const int liWritten = ::vsnprintf(lacBuffer, sizeof(lacBuffer), lpcFormat, lArgs);
        va_end(lArgs);

        CGS_ASSERT(liWritten < static_cast<int>(sizeof(lacBuffer)),
                   "lNumBytesCopied<(int32_t)luBytes");

        *this << lacBuffer;
    }

    void StrStreamBase::Append64IntDecimal(u64 luValue)
    {
        char lacBuffer[KI_FORMAT_BUFFER_SIZE];
        rw::core::stdc::ConvertI64ToA(static_cast<s64>(luValue), lacBuffer, 10);
        *this << lacBuffer;
    }

    void StrStreamBase::Append64IntHex(u64 luValue)
    {
        AppendFormat("0x%08X", static_cast<u32>(luValue >> 32));
        AppendFormat("%08X", static_cast<u32>(luValue));
    }

    // ARTIST 0x821F0158 clears the first byte of the caller's buffer.
    StrStream::StrStream(char* lpcBuffer, s32 liBufferSize)
        : mpcBuffer(lpcBuffer)
        , miBufferSize(liBufferSize)
    {
        lpcBuffer[0] = '\0';
    }

    // ARTIST 0x82815E80 scans through the terminator and gives strncat the full remaining count.
    void StrStream::Append(const char* lpcText)
    {
        std::strncat(mpcBuffer, lpcText, miBufferSize - std::strlen(mpcBuffer));
    }

    // @ 0x827DB6E8-loop init. Resets the inline buffer to the empty string (the X360 `stb 0,8(this)`).
    // The StrStreamBase() base ctor already set mePrintMode = E_PRINTMODE_DECIMAL.
    SimpleStrStream::SimpleStrStream()
    {
        macCharBuffer[0] = '\0';
    }

    // 0x8229F6C8 - inline-buffer sink. The X360 substitutes the literal "<NULLSTRING>" for a null
    // pointer (`if (!a2) a2 = "<NULLSTRING>";`) before appending, then forwards to the virtual char*
    // sink. The sink at 0x82815EC8 uses strncat with count 256 minus the used length.
    StrStreamBase& SimpleStrStream::operator<<(const char* lpcText)
    {
        return StrStreamBase::operator<<(lpcText);
    }

    void SimpleStrStream::Append(const char* lpcText)
    {
        std::strncat(macCharBuffer, lpcText, KI_BUFFER_SIZE - std::strlen(macCharBuffer));
    }

    StrStreamBase& SimpleStrStream::operator<<(s32 liValue)
    {
        return StrStreamBase::operator<<(liValue);
    }

    // Decimal mode prints two decimals; the hex modes take the base formatting.
    StrStreamBase& SimpleStrStream::operator<<(f32 lfValue)
    {
        return StrStreamBase::operator<<(lfValue);
    }
}
