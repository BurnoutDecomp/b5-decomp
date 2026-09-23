#ifndef DIRTYSDK_GAMEMANAGER_H
#define DIRTYSDK_GAMEMANAGER_H

#include "types.hpp"
#include "lobbyapi.h"   // LobbyApiRefT, LobbyApiCallbackT

// DirtySDK 5.5.3 - core/include/lobbygamemanager.h
// The game manager: hosts / joins / leaves lobby games through lobby requests, keeps the
// current game record (the lobby's 'game' and 'play' events) and drives ConnApi from it.
// PC body: ../src/pc/gamemanagerpc.cpp.

// The ConnApi ref as connapi.h declares it (the same type; connapi.h is not pulled in here).
namespace CgsNetwork
{
    namespace DirtySock
    {
        struct ConnApiRefT;
    }
}
typedef CgsNetwork::DirtySock::ConnApiRefT ConnApiRefT;

struct GameManagerRefT;   // opaque

enum GameManagerCBTypeE
{
    GAMEMANAGER_CBTYPE_PRE_ADD  = 0,
    GAMEMANAGER_CBTYPE_POST_ADD = 1,
    GAMEMANAGER_CBTYPE_PRE_DEL  = 2,
    GAMEMANAGER_CBTYPE_POST_DEL = 3,   // the current game is gone
    GAMEMANAGER_CBTYPE_PLAYINFO = 4    // the current game record changed ('gnfo' has it)
};

struct GameManagerCBDataT
{
    GameManagerCBTypeE eType;
    s32                iData;   // PLAYINFO: the lobby event kind
    const char*        pData;   // PLAYINFO: the lobby event payload
};

// Callback TYPE: (game manager, event, user data).
typedef void (GameManagerCallbackT)(GameManagerRefT* pGameManager, GameManagerCBDataT* pCBData, void* pUserData);

#ifdef __cplusplus
extern "C" {
#endif

GameManagerRefT* GameManagerCreate(LobbyApiRefT* pLobbyApi, ConnApiRefT* pConnApi);
void GameManagerDestroy(GameManagerRefT* pGameManager);

// Record the local persona (the lobby 'self' name) once the lobby is online.
void GameManagerOnline(GameManagerRefT* pGameManager, const char* pSelfName);

void GameManagerSetCallback(GameManagerRefT* pGameManager, GameManagerCallbackT* pCB, void* pUserData);

// Game requests: 'gcre' host, 'gjoi' join, 'gqwk' quick join, 'gsta' start, 'glea' leave;
// any other kind goes to LobbyApiRequestCB unchanged. Returns the request id, -1 when the
// manager's state does not allow the request.
s32 GameManagerRequestCb(GameManagerRefT* pGameManager, s32 iKind, const char* pParams,
                         LobbyApiCallbackT* pCallback, void* pUserData);

// Status selects: 'gnfo' copies the current LobbyApiPlayT (iBufSize must be its size) and
// returns 0; 'gsrv' 'gsga' 'idle' 'mgrt' return a value; anything else returns -1.
s32 GameManagerStatus(GameManagerRefT* pGameManager, s32 iSelect, void* pBuf, s32 iBufSize);

// Control selects: 'auto' 'gsrv' 'gsv2' 'isrv' 'lock' 'mgrt'. Returns 0, -1 for an unknown one.
s32 GameManagerControl(GameManagerRefT* pGameManager, s32 iControl, s32 iValue, s32 iValue2, void* pValue);

// Per-frame work; the manager registers itself as a NetConn idle function.
void GameManagerUpdate(GameManagerRefT* pGameManager);

#ifdef __cplusplus
}
#endif

#endif // DIRTYSDK_GAMEMANAGER_H
