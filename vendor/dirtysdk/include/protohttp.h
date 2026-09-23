#ifndef DIRTYSDK_PROTOHTTP_H
#define DIRTYSDK_PROTOHTTP_H

#include "types.hpp"

// DirtySDK 5.5.3 - proto/include/protohttp.h
// HTTP(S) client: one request at a time per ref, pumped by ProtoHttpUpdate, polled with
// ProtoHttpStatus and drained with ProtoHttpRecvAll. The CGS HTTP component (terms of
// service download) is the only caller. PC body: ../src/pc/protopc.cpp.

// Opaque HTTP module handle.
struct ProtoHttpRefT;

// ProtoHttpStatus selectors (four-character codes).
#define PROTOHTTP_STATUS_DONE     (('d' << 24) | ('o' << 16) | ('n' << 8) | 'e')   // 1 done, 0 pending, -1 failed
#define PROTOHTTP_STATUS_DATA     (('d' << 24) | ('a' << 16) | ('t' << 8) | 'a')   // body bytes buffered
#define PROTOHTTP_STATUS_CODE     (('c' << 24) | ('o' << 16) | ('d' << 8) | 'e')   // HTTP result code, -1 none
#define PROTOHTTP_STATUS_TIME     (('t' << 24) | ('i' << 16) | ('m' << 8) | 'e')   // 1 when the request timed out
#define PROTOHTTP_STATUS_BODY     (('b' << 24) | ('o' << 16) | ('d' << 8) | 'y')   // body size; -2 header pending, -1 failed

// ProtoHttpRecv / ProtoHttpRecvAll results.
#define PROTOHTTP_RECVDONE        (-1)   // the whole body has been received
#define PROTOHTTP_RECVFAIL        (-2)   // the request failed

#ifdef __cplusplus
extern "C" {
#endif

// Create / destroy an HTTP module with a receive buffer of iBufSize bytes (at least 4096).
ProtoHttpRefT* ProtoHttpCreate(s32 iBufSize);
void ProtoHttpDestroy(ProtoHttpRefT* pState);

// Start a GET of pUrl (bHeadOnly != 0: HEAD). Returns 0 when the request was started,
// negative when it could not be.
s32  ProtoHttpGet(ProtoHttpRefT* pState, const char* pUrl, u32 bHeadOnly);

// Abandon the current request.
void ProtoHttpAbort(ProtoHttpRefT* pState);

// Pump the current request.
void ProtoHttpUpdate(ProtoHttpRefT* pState);

// Query the request (PROTOHTTP_STATUS_*); -1 for an unknown selector or a failed request.
s32  ProtoHttpStatus(ProtoHttpRefT* pState, s32 iSelect, void* pBuffer, s32 iBufSize);

// Copy the whole body into pBuffer (at most iBufSize-1 bytes plus a terminator). Returns the
// body length once complete, 0 while it is still arriving, negative on failure.
s32  ProtoHttpRecvAll(ProtoHttpRefT* pState, char* pBuffer, s32 iBufSize);

#ifdef __cplusplus
}
#endif

#endif // DIRTYSDK_PROTOHTTP_H
