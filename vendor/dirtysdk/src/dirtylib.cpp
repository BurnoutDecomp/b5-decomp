// DirtySDK dirtysock -- platform library debug print (NetPrintfCode).
//
// The game build keeps DirtySDK's NetPrintf enabled; the text goes to the debugger
// output unless a debug hook is installed and swallows it. NetPrintfHook, the only
// writer of the hook, is not part of the build, so the hook is always unset.

#include "dirtylib.h"

#include <cstdarg>
#include <cstdio>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static void* _NetLib_pDebugParm = NULL;
static s32 (*_NetLib_pDebugHook)(void* pParm, const char* pText) = NULL;

extern "C" s32 NetPrintfCode(const char* pFormat, ...)
{
    va_list pFmtArgs;
    char strText[4096];
    const char* pText = strText;

    va_start(pFmtArgs, pFormat);
    if ((pFormat[0] == '%') && (pFormat[1] == 's') && (pFormat[2] == 0))
    {
        // a lone "%s" prints its argument directly
        pText = va_arg(pFmtArgs, const char*);
    }
    else
    {
        vsprintf(strText, pFormat, pFmtArgs);
    }
    va_end(pFmtArgs);

    if ((_NetLib_pDebugHook == NULL) || (_NetLib_pDebugHook(_NetLib_pDebugParm, pText) != 0))
    {
        OutputDebugStringA(pText);
    }
    return 0;
}
