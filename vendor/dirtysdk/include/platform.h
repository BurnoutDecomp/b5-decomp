#ifndef DIRTYSDK_PLATFORM_H
#define DIRTYSDK_PLATFORM_H

#include "types.hpp"

#include <cstdarg>
#include <ctime>

// DirtySDK 5.5.3 - platform/platform.h (string + time helpers subset).
// Bodies: ../src/plat-str.cpp, ../src/plat-time.cpp.

#ifdef __cplusplus
extern "C" {
#endif

// printf into pBuffer (iLength bytes incl. terminator); always terminates. Returns the
// number of characters written, 0 when the output did not fit.
s32 ds_vsnzprintf(char* pBuffer, s32 iLength, const char* pFormat, va_list Args);
s32 ds_snzprintf(char* pBuffer, s32 iLength, const char* pFormat, ...);

// Seconds since 1970 (UTC).
u32 ds_timeinsecs(void);

// Break an epoch into a struct tm (UTC; tm_wday is not filled in). Returns tm.
struct tm* ds_secstotime(struct tm* tm, u32 elap);

// Inverse of ds_secstotime (tm_year .. tm_sec are matched); 0 when no epoch matches.
u32 ds_timetosecs(const struct tm* tm);

#ifdef __cplusplus
}
#endif

#endif // DIRTYSDK_PLATFORM_H
