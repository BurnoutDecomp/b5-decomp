#ifndef DIRTYSDK_DIRTYLIB_H
#define DIRTYSDK_DIRTYLIB_H

#include "types.hpp"

// DirtySDK 5.5.3 - core/include/dirtylib.h (debug print subset).
// NetPrintf((fmt, ...)) is the SDK's debug print; the game build keeps it enabled and
// routes it through NetPrintfCode, which writes to the debugger output.
// Body: ../src/dirtylib.cpp.

#ifdef __cplusplus
extern "C" {
#endif

s32 NetPrintfCode(const char* pFormat, ...);

#ifdef __cplusplus
}
#endif

#define NetPrintf(_x) NetPrintfCode _x

#endif // DIRTYSDK_DIRTYLIB_H
