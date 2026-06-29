#ifndef DIRTYSDK_LOBBYFINDUSER_H
#define DIRTYSDK_LOBBYFINDUSER_H

#include "types.hpp"
#include "lobbyapi.h"   // LobbyApiRefT

// DirtySDK 5.5.3 - core/include/lobbyfinduser.h
// The "find user" lobby module: an asynchronous lookup of a user record by persona
// name. Only the prototypes the reconstructed CGS player-info component calls are
// declared here; signatures mirror the DirtySDK sources (lobbyfinduser.c) as recovered
// from the X360 ARTIST call sites.

// Opaque "find user" module handle.
struct LobbyFindUserT;

// Opaque lobby user record handed to a find-user completion callback (the third
// argument). The CGS component only ever forwards it by pointer to
// ServerInterfacePlayerInfoDataBase::SerialiseFromUser, so the layout is not modelled
// here.
struct LobbyApiUserT;

// Find-user completion callback TYPE: (user data, persona name, resolved user record).
// The DirtySDK convention is a function type (not a pointer); call sites pass the
// address of a static member function.
typedef void (LobbyFindUserCallbackT)(void* pUserData, const char* pcName,
                                       LobbyApiUserT* pUser);

#ifdef __cplusplus
extern "C" {
#endif

// Create a find-user module bound to the supplied lobby API ref. Returns the module
// handle (0 on failure).
LobbyFindUserT* LobbyFindUserCreate(LobbyApiRefT* pLobbyApi);

// Destroy a find-user module previously created with LobbyFindUserCreate.
void LobbyFindUserDestroy(LobbyFindUserT* pFindUser);

// Configure the module's request pacing: uiMinRequestInterval is the minimum time (ms)
// between successive lookups; iCacheExpiry is how long (ms) a cached result stays valid.
void LobbyFindUserSetParams(LobbyFindUserT* pFindUser, u32 uiMinRequestInterval,
                            s32 iCacheExpiry);

// Cancel any in-flight lookup on the module.
void LobbyFindUserCancel(LobbyFindUserT* pFindUser);

// Begin an asynchronous lookup of pcName. pCallback is invoked with pUserData (and the
// resolved record) when the lookup completes. iFlags / iWhatever are request options.
// Returns 0 when the request was accepted, or a negative error code:
//   -1 ('invp' invalid player) / -2 ('many' too many requests) / -3 ('soon' too soon).
s32 LobbyFindUser(LobbyFindUserT* pFindUser, const char* pcName,
                  LobbyFindUserCallbackT* pCallback, void* pUserData,
                  s32 iFlags, s32 iWhatever);

#ifdef __cplusplus
}
#endif

#endif // DIRTYSDK_LOBBYFINDUSER_H
