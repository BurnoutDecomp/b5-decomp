#ifndef DIRTYSDK_LOBBYSETTING_H
#define DIRTYSDK_LOBBYSETTING_H

#include "types.hpp"
#include "lobbyapi.h"   // LobbyApiRefT

// DirtySDK 5.5.3 - lobby/include/lobbysetting.h
// The "settings" lobby module: a local tagfield record of per-account settings (e.g.
// "TELE_NABL", "SPM_EA", "SPM_PART") read and written with Get/SetNumber, loaded from and
// saved to the lobby with the asynchronous Load / Save. PC body: ../src/pc/lobbymiscpc.cpp.
//
// The ref type is the one the CGS DirtySock facade already names
// (CgsNetwork::DirtySock::LobbySettingRefT, forward-declared in CgsServerInterfaceDirtySock.h);
// it is declared in that namespace here too so both spellings are one type.

namespace CgsNetwork
{
    namespace DirtySock
    {
        struct LobbySettingRefT;
    }
}

// Save / load completion callback TYPE: (settings ref, result code, user data). The result
// is 0 on success, otherwise the lobby's error code.
typedef void (LobbySettingCallbackT)(CgsNetwork::DirtySock::LobbySettingRefT* pRef,
                                     s32 iResult, void* pUserData);

#ifdef __cplusplus
extern "C" {
#endif

// Create / destroy a settings module bound to the lobby.
CgsNetwork::DirtySock::LobbySettingRefT* LobbySettingCreate(LobbyApiRefT* pLobbyApi);
void LobbySettingDestroy(CgsNetwork::DirtySock::LobbySettingRefT* pRef);

// Read an integer setting by key, returning iDefault when the key is absent.
s32 LobbySettingGetNumber(CgsNetwork::DirtySock::LobbySettingRefT* pRef,
                          const char* pcKey, s32 iDefault);

// Write an integer setting by key into the local record.
void LobbySettingSetNumber(CgsNetwork::DirtySock::LobbySettingRefT* pRef,
                           const char* pcKey, s32 iValue);

// Begin an asynchronous save of the local record; pCallback is invoked with pUserData when
// the save completes. Returns 1 when the request was accepted, 0 when it was not (a load or
// save is already in progress, or there is no lobby service).
s32 LobbySettingSave(CgsNetwork::DirtySock::LobbySettingRefT* pRef,
                     LobbySettingCallbackT* pCallback, void* pUserData);

// Begin an asynchronous load that replaces the local record; same completion and return
// convention as LobbySettingSave.
s32 LobbySettingLoad(CgsNetwork::DirtySock::LobbySettingRefT* pRef,
                     LobbySettingCallbackT* pCallback, void* pUserData);

#ifdef __cplusplus
}
#endif

#endif // DIRTYSDK_LOBBYSETTING_H
