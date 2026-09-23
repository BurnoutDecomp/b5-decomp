#ifndef DIRTYSDK_PINGMANAGER_H
#define DIRTYSDK_PINGMANAGER_H

#include "types.hpp"

// DirtySDK 5.5.3 - proto/include/protopingmanager.h
// Ping manager: round-trip pings to server addresses, results delivered through a
// callback. The CGS ping-regions component is the only caller. PC body: ../src/pc/pingmanagerpc.cpp.

// Opaque ping manager handle.
struct PingManagerRefT;

// Server ping result callback TYPE: (pinged address record, round-trip ms, user data).
typedef void (PingManagerCallbackT)(void* pAddress, u32 uPing, void* pCallbackData);

#ifdef __cplusplus
extern "C" {
#endif

// Create a ping manager caching up to uMaxRecords results (at least 16); destroy it.
PingManagerRefT* PingManagerCreate(u32 uMaxRecords);
void PingManagerDestroy(PingManagerRefT* pPingManager);

// Ping uServerAddress. Returns the request sequence number (> 0), or -1 when the ping could
// not be sent.
s32  PingManagerPingServer2(PingManagerRefT* pPingManager, u32 uServerAddress,
                            PingManagerCallbackT* pCallback, void* pCallbackData);

// Cancel the server ping request uSeqn (0: all).
void PingManagerCancelServerRequest(PingManagerRefT* pPingManager, u32 uSeqn);

#ifdef __cplusplus
}
#endif

#endif // DIRTYSDK_PINGMANAGER_H
