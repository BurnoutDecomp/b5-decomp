#include "CgsStringUtils.h"

#include <GameShared/GameClasses/Core/CgsAssert.h>
#include <GameShared/GameClasses/Development/CgsStrStream.h>

#include <cctype>
#include <cstdarg>
#include <cstring>
#include <stdio.h>

// ARTIST 0x82815D80 streams its assertion and passes signed char to tolower
// (extsb at 0x82815E30). DecFIGS declares a void return.
void CGSSTRNLOWER(char* pcString, u32 uMaxLen)
{
    CGS_ASSERT_STREAM_AT(pcString, "Passed a null string to CGSSTRNLOWER",
        "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\core\\CgsStringUtils.cpp", 48);

    for (u32 luIndex = 0; *pcString; ++luIndex)
    {
        if (luIndex >= uMaxLen)
            break;
        *pcString = static_cast<char>(tolower(*pcString));
        ++pcString;
    }
}

// DecFIGS declares the wrapper void. ARTIST returns directly after vsnprintf on
// the success path, leaving its result in r3 incidentally; source callers ignore it.
void CgsCore::SPrintf(char* buffer, u32 len, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    const int lNumBytesCopied = vsnprintf(buffer, len, fmt, args);
    va_end(args);

    // ARTIST @0x821F14F8 uses a signed cmpw against the signed length and
    // asserts for result >= len. A negative XDK CRT return stays negative and
    // takes the non-assert path; do not reinterpret it as an unsigned count.
    if (lNumBytesCopied >= static_cast<s32>(len))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lNumBytesCopied<(int32_t)luBytes",
            CGS_ASSERT_LOCATION("..\\..\\..\\GameShared\\GameClasses\\Core/CgsStringUtils.h",
            119));
        CgsDev::Assert::EndAssert();
    }
}

void CgsCore::SnPrintf(char* buffer, u32 len, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, len, fmt, args);
    va_end(args);
}

void CgsCore::StrCpy(char* dest, u32 len, const char* src)
{
    if (len == 0)
        return;

    u32 i = 0;
    for (; i < len - 1 && src[i] != '\0'; ++i)
        dest[i] = src[i];

    dest[i] = '\0';
}

void CgsCore::StrCat(char* dest, u32 len, const char* src)
{
    if (len == 0)
        return;

    u32 destLen = 0;
    while (destLen < len && dest[destLen] != '\0')
        ++destLen;

    u32 i = 0;
    for (; destLen + i < len - 1 && src[i] != '\0'; ++i)
        dest[destLen + i] = src[i];

    dest[destLen + i] = '\0';
}
