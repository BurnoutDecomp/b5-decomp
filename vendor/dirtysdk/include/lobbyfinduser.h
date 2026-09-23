#ifndef DIRTYSDK_LOBBYFINDUSER_H
#define DIRTYSDK_LOBBYFINDUSER_H

#include "types.hpp"
#include "lobbyapi.h"   // LobbyApiRefT

// DirtySDK 5.5.3 - lobby/include/lobbyfinduser.h
// The "find user" lobby module: an asynchronous lookup of a user record by persona name
// (answered from the lobby's user list, a small cache, or a lobby request). The CGS
// player-info component is the only caller. PC body: ../src/pc/lobbymiscpc.cpp.

// Opaque "find user" module handle.
struct LobbyFindUserT;

// The lobby user record handed to a find-user completion callback (defined with the lobby
// API; this module only passes it on).
struct LobbyApiUserT;

// Find-user completion callback TYPE: (user data, persona name, resolved user record). The
// record is NULL when the lookup failed.
typedef void (LobbyFindUserCallbackT)(void* pUserData, const char* pcName, LobbyApiUserT* pUser);

#ifdef __cplusplus
extern "C" {
#endif

// Create / destroy a find-user module bound to the lobby.
LobbyFindUserT* LobbyFindUserCreate(LobbyApiRefT* pLobbyApi);
void LobbyFindUserDestroy(LobbyFindUserT* pFindUser);

// Request pacing: uRate is the minimum time (ms, at least 250) between two lobby requests;
// iCacheExpire is how long (ms) a cached record stays valid.
void LobbyFindUserSetParams(LobbyFindUserT* pFindUser, u32 uRate, s32 iCacheExpire);

// Cancel the in-flight lookup, if any.
void LobbyFindUserCancel(LobbyFindUserT* pFindUser);

// Look up pcName. Returns 0 when the lookup completed or was sent (pCallback reports the
// result), -1 when a lookup is already in flight or the request could not be sent, -2 when
// the previous request was too recent, -3 on a missing argument.
s32 LobbyFindUser(LobbyFindUserT* pFindUser, const char* pcName,
                  LobbyFindUserCallbackT* pCallback, void* pUserData,
                  s32 bOnlineOnly, s32 bUseCache);

#ifdef __cplusplus
}
#endif

#endif // DIRTYSDK_LOBBYFINDUSER_H
