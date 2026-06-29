#ifndef DIRTYSDK_LOBBYSETTING_H
#define DIRTYSDK_LOBBYSETTING_H

#include "types.hpp"

// DirtySDK 5.5.3 - core/include/lobbysetting.h
// The "settings" lobby module: a key/value store of per-account settings persisted to
// the lobby back-end (e.g. "TELE_NABL", "SPM_EA", "SPM_PART"). Only the prototypes the
// reconstructed CGS player-info component calls are declared here; signatures mirror the
// DirtySDK sources (lobbysetting.c) as recovered from the X360 ARTIST call sites.
//
// The opaque ref handle is owned by the CGS DirtySock layer
// (CgsNetwork::DirtySock::LobbySettingRefT, declared in CgsServerInterfaceDirtySock.h);
// this header forward-declares it in that namespace so the free-function prototypes are
// well-formed without pulling in the CGS facade header.

namespace CgsNetwork
{
    namespace DirtySock
    {
        struct LobbySettingRefT;
    }
}

// Save / load completion callback TYPE: (settings ref, result code, user data).
typedef void (LobbySettingCallbackT)(CgsNetwork::DirtySock::LobbySettingRefT* pRef,
                                     s32 iResult, void* pUserData);

#ifdef __cplusplus
extern "C" {
#endif

// Read an integer setting by key, returning iDefault when the key is absent.
s32 LobbySettingGetNumber(CgsNetwork::DirtySock::LobbySettingRefT* pRef,
                          const char* pcKey, s32 iDefault);

// Write an integer setting by key.
void LobbySettingSetNumber(CgsNetwork::DirtySock::LobbySettingRefT* pRef,
                           const char* pcKey, s32 iValue);

// Begin an asynchronous save of the modified settings; pCallback is invoked with
// pUserData when the save completes. Returns 0 on success or a negative error.
s32 LobbySettingSave(CgsNetwork::DirtySock::LobbySettingRefT* pRef,
                     LobbySettingCallbackT* pCallback, void* pUserData);

// Begin an asynchronous load of the account settings; pCallback is invoked with
// pUserData when the load completes. Returns 0 on success or a negative error.
s32 LobbySettingLoad(CgsNetwork::DirtySock::LobbySettingRefT* pRef,
                     LobbySettingCallbackT* pCallback, void* pUserData);

#ifdef __cplusplus
}
#endif

#endif // DIRTYSDK_LOBBYSETTING_H
