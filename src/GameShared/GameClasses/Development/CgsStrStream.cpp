#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "rw/core/stdc/stdc.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

// CgsDev::StrStreamBase / StrStream. The scalar overloads format the value with the
// engine's printf-style KAC_* format strings (here the standard equivalents) honouring the
// current PrintMode, then forward the rendered text to the virtual char* sink. Recovered from
// the DecFIGS DWARF + the X360 StrStream bodies (CgsStrStream.cpp).
namespace CgsDev
{
    // DWARF CgsStrStream.h:163-179, ARTIST .data 0x82F3193C..0x82F31974 (one pointer each).
    const char* StrStreamBase::KAC_INTEGER = "%d";
    const char* StrStreamBase::KAC_UNSIGNED = "%u";
    const char* StrStreamBase::KAC_HEX = "0x%X";
    const char* StrStreamBase::KAC_PADDED_HEX = "0x%08X";
    const char* StrStreamBase::KAC_PADDED_HEX_NO_PREFIX = "%08X";
    const char* StrStreamBase::KAC_FLOAT = "%f";
    const char* StrStreamBase::KAC_CHAR = "%c";
    const char* StrStreamBase::KAC_TRUE = "true";
    const char* StrStreamBase::KAC_FALSE = "false";
    const char* StrStreamBase::KAC_VECTOR2 = "(%f, %f)";
    const char* StrStreamBase::KAC_VECTOR3 = "(%f, %f, %f)";
    const char* StrStreamBase::KAC_VECTOR4 = "(%f, %f, %f, %f)";
    const char* StrStreamBase::KAC_MATRIX33 = "\n(%f, %f, %f)\n(%f, %f, %f)\n(%f, %f, %f)\n";
    const char* StrStreamBase::KAC_MATRIX44 =
        "\n(%f, %f, %f, %f)\n(%f, %f, %f, %f)\n(%f, %f, %f, %f)\n(%f, %f, %f, %f)\n";
    const char* StrStreamBase::KAC_LF = "\n";
    const s32 StrStreamBase::KI_FORMAT_BUFFER_SIZE = 256;

    // The StrStreamBase vtable at 0x82000D00 points to 0x82815E58.
    void StrStreamBase::Reset()
    {
        meCurrentPrintMode = E_PRINTMODE_DECIMAL;
    }

    // ARTIST 0x82817720 formats into 256 bytes, checks the result against the buffer
    // size, and passes the formatted text directly to the virtual sink.
    void StrStreamBase::AppendFormat(const char* lpcFormat, ...)
    {
        char lacBuffer[256];

        va_list lArgs;
        va_start(lArgs, lpcFormat);
        const int liWritten = ::vsnprintf(lacBuffer, sizeof(lacBuffer), lpcFormat, lArgs);
        va_end(lArgs);

        if (liWritten >= static_cast<int>(sizeof(lacBuffer)))
        {
            Assert::BeginAssert();
            Assert::FireAssert("lNumBytesCopied<(int32_t)luBytes",
                               CGS_ASSERT_LOCATION("..\\..\\..\\GameShared\\GameClasses\\Core/CgsStringUtils.h", 96));
            Assert::EndAssert();
        }
        Append(lacBuffer);
    }

    // ARTIST 0x82815E80 scans through the terminator and gives strncat the full remaining count.
    void StrStream::Append(const char* lpcText)
    {
        std::strncat(mpcBuffer, lpcText, miBufferSize - std::strlen(mpcBuffer));
    }

    // @ 0x827DB6E8-loop init. Resets the inline buffer to the empty string (the X360 `stb 0,8(this)`).
    // The StrStreamBase() base ctor already set meCurrentPrintMode = E_PRINTMODE_DECIMAL.
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
