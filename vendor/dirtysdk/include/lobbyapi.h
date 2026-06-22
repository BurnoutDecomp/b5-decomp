#ifndef DIRTYSDK_LOBBYAPI_H
#define DIRTYSDK_LOBBYAPI_H

#include "types.hpp"

// DirtySDK 5.5.3 - core/include/lobbyapi.h
// The lobby API ref + the two query entry points the CGS server-info component uses.
// LobbyApiRefT is the opaque lobby connection handle. Only the prototypes the
// reconstructed CGS code calls are declared here; signatures mirror the DirtySDK
// sources (lobbyapi.c).

struct LobbyApiRefT;   // opaque

#ifdef __cplusplus
extern "C" {
#endif

// Query a status item, filling pBuf (e.g. the 'self' tagfield record). Returns a
// status/length code.
s32 LobbyApiStatus(LobbyApiRefT* pLobbyApi, s32 iSelect, void* pBuf, s32 iBufLen);

// Query an info item, returning a pointer to the relevant tagfield record blob.
const void* LobbyApiInfo(LobbyApiRefT* pLobbyApi, s32 iSelect);

#ifdef __cplusplus
}
#endif

#endif // DIRTYSDK_LOBBYAPI_H
