#ifndef DIRTYSDK_DIRTYNET_H
#define DIRTYSDK_DIRTYNET_H

#include "types.hpp"

// DirtySDK 5.5.3 - platform/dirtynet.h (hash subset).
// NetHash is the SDK's string hash (the game manager's client ids).
// Body: ../src/dirtynet.cpp.

#ifdef __cplusplus
extern "C" {
#endif

// Hash of a NUL-terminated string: h = (h >> 27) ^ (h << 5) ^ c over the bytes.
s32 NetHash(const char* pString);

#ifdef __cplusplus
}
#endif

#endif // DIRTYSDK_DIRTYNET_H
