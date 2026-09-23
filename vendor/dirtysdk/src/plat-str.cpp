// DirtySDK platform -- string helpers (platform/plat-str.c): the bounded,
// always-terminated printf pair. Only the entry points the tagfield/display-list
// modules reach are reconstructed here.

#include "platform.h"

#include <cstdarg>
#include <cstdio>

// The runtime's _vsnprintf has the semantics the SDK was written against: -1 when the
// output is truncated, no terminator when it fills the count exactly. A result that
// does not fit is reported (and terminated) as an empty string.
extern "C" s32 ds_vsnzprintf(char* pBuffer, s32 iLength, const char* pFormat, va_list Args)
{
    s32 iResult;

    if (--iLength < 0)
    {
        return 0;
    }
    iResult = _vsnprintf(pBuffer, iLength, pFormat, Args);
    if ((iResult > iLength) || (iResult < 0))
    {
        iResult = 0;
    }
    pBuffer[iResult] = 0;
    return iResult;
}

extern "C" s32 ds_snzprintf(char* pBuffer, s32 iLength, const char* pFormat, ...)
{
    s32 iResult;
    va_list args;

    if (--iLength < 0)
    {
        return 0;
    }
    va_start(args, pFormat);
    iResult = _vsnprintf(pBuffer, iLength, pFormat, args);
    va_end(args);
    if ((iResult > iLength) || (iResult < 0))
    {
        iResult = 0;
    }
    pBuffer[iResult] = 0;
    return iResult;
}
