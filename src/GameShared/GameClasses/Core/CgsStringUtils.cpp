#include "CgsStringUtils.h"

#include <GameShared/GameClasses/Core/CgsAssert.h>

#include <cctype>
#include <cstdarg>
#include <cstring>
#include <stdio.h>

// In-place lower-cases up to uMaxLen characters of a NUL-terminated string and
// returns the (unchanged) buffer pointer. Recovered from
// CGSSTRNLOWER @ 0x82815D80; the all-caps symbol is kept verbatim so the
// cross-build identity join (callers FontResourceType::FixUp,
// FontCollection::FindFont, CgsAptString::Prepare) still resolves by name.
char* CGSSTRNLOWER(char* pcString, u32 uMaxLen)
{
    CGS_ASSERT(pcString != nullptr, "Passed a null string to CGSSTRNLOWER");

    char* pc = pcString;
    for (u32 i = 0; *pc; ++i)
    {
        if (i >= uMaxLen)
            break;
        *pc = static_cast<char>(tolower(static_cast<unsigned char>(*pc)));
        ++pc;
    }

    return pcString;
}

void CgsCore::SPrintf(char* buffer, u32 len, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    auto result = _vsnprintf(buffer, len, fmt, args);
    if (result == len || result < 0)
    {
        // If the result is equal to len, it means the string was truncated.
        // If result is negative, it indicates an error occurred.
        buffer[len - 1] = '\0'; // Ensure null termination
    }
    va_end(args);
}

void CgsCore::SnPrintf(char* buffer, u32 len, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    auto result = _vsnprintf(buffer, len, fmt, args);
    if (result == len || result < 0)
    {
        // If the result is equal to len, it means the string was truncated.
        // If result is negative, it indicates an error occurred.
        buffer[len - 1] = '\0'; // Ensure null termination
    }
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
