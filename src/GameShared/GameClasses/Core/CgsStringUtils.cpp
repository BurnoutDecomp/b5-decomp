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
    CGS_ASSERT(pcString != 0, "Passed a null string to CGSSTRNLOWER");

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
            "..\\..\\..\\GameShared\\GameClasses\\Core/CgsStringUtils.h",
            119);
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
