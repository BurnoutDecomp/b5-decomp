#ifndef DIRTYSDK_LOBBYAPI_H
#define DIRTYSDK_LOBBYAPI_H

#include "types.hpp"

// DirtySDK 5.5.3 - core/include/lobbyapi.h
// The lobby API ref + the two query entry points the CGS server-info component uses.
// LobbyApiRefT is the opaque lobby connection handle. Only the prototypes the
// reconstructed CGS code calls are declared here; signatures mirror the DirtySDK
// sources (lobbyapi.c).

struct LobbyApiRefT;   // opaque

// A completed lobby request message delivered to a request callback. Field layout
// mirrors the DirtySDK source (core/include/lobbyapi.h): the X360 components read the
// result fourcc at +0x0C (`code`) and the response payload pointer at +0x10 (`pData`).
struct LobbyApiMsgT
{
    s32         id;      // +0x00
    s32         type;    // +0x04
    s32         kind;    // +0x08
    s32         code;    // +0x0C  result fourcc ('new0'+idx on success, error code otherwise)
    const char* pData;   // +0x10  response tagfield record / payload text
    void*       pMisc;   // +0x14
};

// Request-completion callback TYPE: (lobby ref, completed message, user data). The
// DirtySDK convention is a function type (not a pointer); call sites use
// `LobbyApiCallbackT*`.
typedef void (LobbyApiCallbackT)(LobbyApiRefT* pLobbyApi, LobbyApiMsgT* pMsg, void* pUserData);

#ifdef __cplusplus
extern "C" {
#endif

// Query a status item, filling pBuf (e.g. the 'self' tagfield record). Returns a
// status/length code.
s32 LobbyApiStatus(LobbyApiRefT* pLobbyApi, s32 iSelect, void* pBuf, s32 iBufLen);

// Query an info item, returning a pointer to the relevant tagfield record blob.
const void* LobbyApiInfo(LobbyApiRefT* pLobbyApi, s32 iSelect);

// Issue an asynchronous request with the assembled tagfield record pRequest. iKind is
// the request fourcc; pCallback is invoked with pUserCBData when the response arrives.
s32 LobbyApiRequestCB(LobbyApiRefT* pLobbyApi, s32 iKind, const char* pRequest,
                      LobbyApiCallbackT* pCallback, void* pUserCBData);

// Register a callback on a lobby channel (e.g. the chat channel 1). Returns a callback
// handle (>= 0) the caller stores to later clear it; pCallback is invoked with pUserData
// for each message that arrives on iChannel.
s32 LobbyApiSetCallback(LobbyApiRefT* pLobbyApi, s32 iChannel,
                        LobbyApiCallbackT* pCallback, void* pUserData);

// Clear a previously-registered channel callback (iCallback is the handle returned by
// LobbyApiSetCallback).
s32 LobbyApiClearCallback(LobbyApiRefT* pLobbyApi, s32 iCallback);

// Disconnect from the lobby (DirtySDK 5.5.3 core/source/lobby/lobbyapi.c:1477,
// `extern void LobbyApiDisconnect(LobbyApiRefT*, int32_t)`). iCode is the disconnect
// reason code (0 == normal/no-reason on the CgsNetworkAdapterX360 duplicate-login kick
// call site).
void LobbyApiDisconnect(LobbyApiRefT* pLobbyApi, s32 iCode);

#ifdef __cplusplus
}
#endif

#endif // DIRTYSDK_LOBBYAPI_H
