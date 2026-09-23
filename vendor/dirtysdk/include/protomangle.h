#ifndef DIRTYSDK_PROTOMANGLE_H
#define DIRTYSDK_PROTOMANGLE_H

#include "types.hpp"

// DirtySDK 5.5.3 - core/include/protomangle.h (session encoding subset)
// The session text the invite pipeline carries: the 60-byte session-info block (8-byte
// session id at +0x00, 36-byte host address at +0x08, 16-byte key-exchange key at +0x2C)
// packed into printable form.
// Body: ../src/protomangle.cpp.

#ifdef __cplusplus
extern "C" {
#endif

// Encode pSessionInfo into pBuffer (iBufSize bytes): '$', then the host address, the key and
// the session id, each re-packed 7 bits per byte with the top bit set, then a terminator.
void ProtoMangleEncodeSession(char* pBuffer, s32 iBufSize, const void* pSessionInfo);

#ifdef __cplusplus
}
#endif

#endif // DIRTYSDK_PROTOMANGLE_H
