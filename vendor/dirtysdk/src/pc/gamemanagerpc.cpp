// ============================================================================
// gamemanagerpc.cpp -- the game manager (GameManager*) for the PC build.
//
// [PC platform layer] The game manager is lobby-client logic with no server of its own, so
// this module keeps the console module's behaviour: game requests go out through
// LobbyApiRequestCB ('gcre'/'gpsc', 'gjoi', 'gqwk', 'gsta', 'glea'/'gdel'); the lobby's 'game'
// and 'play' events carry the game record (decoded with LobbyApiExtractPlayRecord), which is
// compacted, stored as the current record ('gnfo'), announced with GAMEMANAGER_CBTYPE_PLAYINFO
// and turned into ConnApi calls (ConnApiHost / ConnApiConnect for a new game, ConnApiAddClient /
// ConnApiRemoveClient for member changes, ConnApiStart on 'play'); a 'user' event without a
// game, or 'disc' on the lobby connection, ends the game (GAMEMANAGER_CBTYPE_POST_DEL).
// The manager also sits in front of the ConnApi status callback ('cbfp' / 'cbup'): a session
// event (type 2) with status 3 sends the session string to the lobby ('gset' SESS), status 5
// (session deleted) ends the game; every event is then passed on to the previous callback.
// Offline the lobby never answers or posts anything, so the manager stays idle.
// ============================================================================

#include "gamemanager.h"
#include "lobbyapi.h"
#include "lobbyname.h"
#include "lobbytagfield.h"
#include "connapi.h"
#include "netconn.h"
#include "dirtynet.h"
#include "dirtymem.h"
#include "dirtylib.h"

#include <cstring>

namespace
{
    constexpr s32 FourCC(char a, char b, char c, char d)
    {
        return (s32)(((u32)(u8)a << 24) | ((u32)(u8)b << 16) | ((u32)(u8)c << 8) | (u32)(u8)d);
    }

    const s32 KI_MEMID = FourCC('g', 'm', 'g', 'r');

    // game manager states
    enum
    {
        ST_IDLE      = 0,
        ST_HOST      = 1,
        ST_LOCKED    = 2,   // waiting for ConnApi to go idle
        ST_MIGRATING = 3,
        ST_READY     = 4,
        ST_NEWHOST   = 5,   // we took over as host
        ST_MEMBER    = 6
    };
}

struct GameManagerRefT
{
    LobbyApiRefT*         pLobbyApi;
    ConnApiRefT*          pConnApi;
    s32                   iMemGroup;
    s32                   iLobbyEventCb;
    s32                   iDiscEventCb;
    u16                   uOldGamePort;
    u16                   uOldVoipPort;
    LobbyApiPlayT         LastPlay;          // the record ConnApi was last brought in line with
    LobbyApiPlayT         CurrPlay;          // the latest record ('gnfo')
    u32                   uGameServerAddr;
    s32                   eGameServMode;
    u32                   uGameServConnMode;
    u32                   uGameServFallback;
    ConnApiCallbackT*     pConnApiCallback;   // the ConnApi callback we sit in front of
    void*                 pConnApiUserData;
    char                  strSelfName[16];
    s32                   eState;
    GameManagerCBDataT    CBData;
    GameManagerCallbackT* pCBFunction;
    void*                 pCBUserData;
    u8                    bAutoUpdate;
    u8                    bHostMigration;
    u8                    bGPSGameServ;
    u8                    bInternalServer;
    s8                    iLocked;
    u8                    bStarted;
};

// ---- internal helpers --------------------------------------------------------------------

static void _GameManagerExecCallback(GameManagerRefT* pGameManager, GameManagerCBTypeE eType, s32 iData,
                                     const char* pData)
{
    pGameManager->CBData.eType = eType;
    pGameManager->CBData.iData = iData;
    pGameManager->CBData.pData = pData;
    if (pGameManager->pCBFunction != NULL)
    {
        pGameManager->pCBFunction(pGameManager, &pGameManager->CBData, pGameManager->pCBUserData);
    }
}

// Copy a record, keeping the old AUTH string when the new record carries none.
static void _UpdateRecord(LobbyApiPlayT* pDest, const LobbyApiPlayT* pSource)
{
    char strAuth[64];
    memset(strAuth, 0, sizeof(strAuth));
    if ((pSource->strAuth[0] == 0) && (pDest->strAuth[0] != 0))
    {
        strncpy(strAuth, pDest->strAuth, sizeof(strAuth));
        strAuth[sizeof(strAuth) - 1] = 0;
    }
    memcpy(pDest, pSource, sizeof(*pDest));
    if (strAuth[0] != 0)
    {
        strncpy(pDest->strAuth, strAuth, sizeof(pDest->strAuth));
        pDest->strAuth[sizeof(pDest->strAuth) - 1] = 0;
    }
}

// Is persona pCheckPers a player of pPlay (exact compare)?
static bool _GameManagerPersInPlayRecord(const LobbyApiPlayT* pPlay, const char* pCheckPers)
{
    s32 iPlayer;
    for (iPlayer = 0; iPlayer < pPlay->iCount; ++iPlayer)
    {
        if (strcmp(pPlay->aOpponents[iPlayer].strPers, pCheckPers) == 0)
        {
            break;
        }
    }
    return iPlayer != pPlay->iCount;
}

static void _GameManagerDisconnect(GameManagerRefT* pGameManager)
{
    const bool bInGame = (u8)ConnApiStatus(pGameManager->pConnApi, FourCC('i', 'n', 'g', 'm'), NULL, 0) != 0;
    ConnApiDisconnect(pGameManager->pConnApi);
    pGameManager->eState = ST_IDLE;
    if (pGameManager->uOldGamePort != 0)
    {
        ConnApiControl(pGameManager->pConnApi, FourCC('g', 'p', 'r', 't'), pGameManager->uOldGamePort, 0, NULL);
        pGameManager->uOldGamePort = 0;
    }
    if (pGameManager->uOldVoipPort != 0)
    {
        ConnApiControl(pGameManager->pConnApi, FourCC('v', 'p', 'r', 't'), pGameManager->uOldVoipPort, 0, NULL);
        pGameManager->uOldVoipPort = 0;
    }
    memset(&pGameManager->LastPlay, 0, sizeof(pGameManager->LastPlay));
    pGameManager->bStarted        = 0;
    pGameManager->uGameServerAddr = 0;
    if (!bInGame)
    {
        _GameManagerExecCallback(pGameManager, GAMEMANAGER_CBTYPE_POST_DEL, 0, NULL);
    }
}

// Drop game-server entries, find the host, compact the player table and store the record.
static void _GameManagerProcessPlayRecord(GameManagerRefT* pGameManager, LobbyApiPlayT* pPlay)
{
    s32 iHostIndex = 0;
    s32 iCount = 0;
    for (s32 iPlayer = 0; iPlayer < pPlay->iCount; ++iPlayer)
    {
        const LobbyApiPlayerT* pSource = &pPlay->aOpponents[iPlayer];
        if (pSource->strPers[0] == '@')
        {
            pGameManager->uGameServerAddr = pGameManager->bInternalServer ? pSource->uLocalAddr : pSource->uAddr;
            continue;
        }
        LobbyApiPlayerT* pDest = &pPlay->aOpponents[iCount];
        memcpy(pDest, pSource, sizeof(*pDest));
        if (((pPlay->strHost[0] == '@') && (LobbyNameCmp(pPlay->strGPSHost, pDest->strPers) == 0)) ||
            (LobbyNameCmp(pPlay->strHost, pDest->strPers) == 0))
        {
            strncpy(pPlay->strHost, pDest->strPers, sizeof(pPlay->strHost));
            pPlay->strHost[sizeof(pPlay->strHost) - 1] = 0;
            iHostIndex = iCount;
        }
        ++iCount;
    }
    pPlay->iCount = iCount;
    ConnApiControl(pGameManager->pConnApi, FourCC('h', 's', 't', 'i'), iHostIndex, 0, NULL);
    _UpdateRecord(&pGameManager->CurrPlay, pPlay);
}

static void _GameManagerFillClient(ConnApiUserInfoT* pClient, const LobbyApiPlayerT* pPlayer)
{
    memset(pClient, 0, sizeof(*pClient));
    strncpy(pClient->strName, pPlayer->strPers, sizeof(pClient->strName) - 1);
    strncpy(pClient->DirtyAddr.strMachineAddr, pPlayer->strMachineAddr, sizeof(pClient->DirtyAddr.strMachineAddr) - 1);
    pClient->uAddr      = pPlayer->uAddr;
    pClient->uLocalAddr = pPlayer->uLocalAddr;
    pClient->uClientId  = (u32)NetHash(pPlayer->strPers);
}

// A game we are not yet connected for: host or connect ConnApi, or wait for it to go idle.
// Returns 1 when ConnApi was started.
static s32 _GameManagerCreateGame(GameManagerRefT* pGameManager, LobbyApiPlayT* pPlay)
{
    _GameManagerExecCallback(pGameManager, GAMEMANAGER_CBTYPE_PRE_ADD, 0, NULL);

    if (ConnApiStatus(pGameManager->pConnApi, FourCC('i', 'd', 'l', 'e'), NULL, 0) == 0)
    {
        GameManagerControl(pGameManager, FourCC('l', 'o', 'c', 'k'), 1, 0, NULL);
        _UpdateRecord(&pGameManager->CurrPlay, pPlay);
        pGameManager->eState = ST_LOCKED;
        return 0;
    }

    s32 iGameFlags = ((pPlay->uSysflags & 0x40000) != 0) ? 1 : 0;   // ranked
    if ((pPlay->uSysflags & 0x10000) != 0)
    {
        iGameFlags |= 2;
    }
    if (pPlay->iGameMode != -1)
    {
        pGameManager->eGameServMode = (pPlay->iGameMode != 0) ? 1 : 0;
    }
    ConnApiControl(pGameManager->pConnApi, FourCC('g', 's', 'r', 'v'), (s32)pGameManager->uGameServerAddr,
                   pGameManager->eGameServMode, NULL);
    pGameManager->uOldGamePort = (u16)ConnApiStatus(pGameManager->pConnApi, FourCC('g', 'p', 'r', 't'), NULL, 0);
    pGameManager->uOldVoipPort = (u16)ConnApiStatus(pGameManager->pConnApi, FourCC('v', 'p', 'r', 't'), NULL, 0);
    if ((pPlay->uGamePort != 0) && ((pGameManager->uGameServConnMode & 1) != 0))
    {
        ConnApiControl(pGameManager->pConnApi, FourCC('g', 'p', 'r', 't'), pPlay->uGamePort, 0, NULL);
    }
    if ((pPlay->uVoipPort != 0) && ((pGameManager->uGameServConnMode & 2) != 0))
    {
        ConnApiControl(pGameManager->pConnApi, FourCC('v', 'p', 'r', 't'), pPlay->uVoipPort, 0, NULL);
    }

    ConnApiUserInfoT aClients[9];
    s32 iClient;
    memset(aClients, 0, sizeof(aClients));
    for (iClient = 0; (iClient < pPlay->iCount) && (iClient < 9); ++iClient)
    {
        _GameManagerFillClient(&aClients[iClient], &pPlay->aOpponents[iClient]);
    }

    const bool bOurGame = LobbyNameCmp(pPlay->strHost, pGameManager->strSelfName) == 0;
    pGameManager->eState = bOurGame ? ST_HOST : ST_MEMBER;
    if (bOurGame)
    {
        NetPrintf(("gamemanager: hosting %s game with %d clients\n", (iGameFlags & 1) ? "ranked" : "unranked", iClient));
        ConnApiHost(pGameManager->pConnApi, aClients, iClient, pPlay->iIdent, iGameFlags);
        return 1;
    }
    if (pPlay->strSess[0] == 0)
    {
        NetPrintf(("gamemanager: deferring ConnApi creation as the session string is not yet available\n"));
        return 0;
    }
    ConnApiControl(pGameManager->pConnApi, FourCC('s', 'e', 's', 's'), 0, 0, pPlay->strSess);
    ConnApiControl(pGameManager->pConnApi, FourCC('n', 'o', 'n', 'c'), 0, 0, pPlay->strPlatParams);
    NetPrintf(("gamemanager: connecting to %s game with %d clients\n", (iGameFlags & 1) ? "ranked" : "unranked", iClient));
    ConnApiConnect(pGameManager->pConnApi, aClients, iClient, pPlay->iIdent, iGameFlags);
    return 1;
}

// Bring ConnApi's member list in line with a new record. Returns 1, or 0 when we are no
// longer in the game (the game is then over).
static s32 _GameManagerUpdateGame(GameManagerRefT* pGameManager, LobbyApiPlayT* pPlay)
{
    for (s32 iPlayer = 0; iPlayer < pGameManager->LastPlay.iCount; ++iPlayer)
    {
        const char* pPers = pGameManager->LastPlay.aOpponents[iPlayer].strPers;
        if (_GameManagerPersInPlayRecord(pPlay, pPers))
        {
            continue;
        }
        if (strcmp(pGameManager->strSelfName, pPers) == 0)
        {
            _GameManagerDisconnect(pGameManager);
            return 0;
        }
        NetPrintf(("gamemanager: removing client %s (%d) from game\n", pPers, iPlayer));
        ConnApiRemoveClient(pGameManager->pConnApi, pPers, -1);
    }
    for (s32 iPlayer = 0; iPlayer < pPlay->iCount; ++iPlayer)
    {
        const char* pPers = pPlay->aOpponents[iPlayer].strPers;
        if (_GameManagerPersInPlayRecord(&pGameManager->LastPlay, pPers))
        {
            continue;
        }
        ConnApiUserInfoT ClientInfo;
        _GameManagerFillClient(&ClientInfo, &pPlay->aOpponents[iPlayer]);
        NetPrintf(("gamemanager: adding client %s to game\n", pPers));
        ConnApiAddClient(pGameManager->pConnApi, &ClientInfo);
    }
    return 1;
}

static void _GameManagerEventGame(GameManagerRefT* pGameManager, LobbyApiPlayT* pPlay)
{
    NetPrintf(("gamemanager: got 'game' event\n"));
    if (pPlay->strName[0] == 0)
    {
        _GameManagerDisconnect(pGameManager);
        return;
    }
    if (pGameManager->bStarted)
    {
        pGameManager->bStarted = 0;
    }

    const bool bHostChanged = (pGameManager->LastPlay.iCount > 0) &&
                              (LobbyNameCmp(pPlay->strHost, pGameManager->LastPlay.strHost) != 0);
    if ((pGameManager->eState != ST_MIGRATING) &&
        ((pGameManager->uGameServerAddr != 0) ||
         (ConnApiStatus(pGameManager->pConnApi, FourCC('p', 'e', 'e', 'r'), NULL, 0) != 0)) &&
        bHostChanged)
    {
        // peer-hosted game with a new host: migrate
        if ((LobbyNameCmp(pPlay->strHost, pGameManager->strSelfName) != 0) &&
            _GameManagerPersInPlayRecord(pPlay, pGameManager->strSelfName))
        {
            pGameManager->eState = ST_MIGRATING;
        }
        else if (LobbyNameCmp(pPlay->strHost, pGameManager->strSelfName) == 0)
        {
            ConnApiControl(pGameManager->pConnApi, FourCC('m', 'i', 'g', 'r'), 1, 0, "");
            pGameManager->eState = ST_NEWHOST;
        }
    }
    else if (bHostChanged)
    {
        // a new host without peer hosting: start over once ConnApi is idle
        ConnApiDisconnect(pGameManager->pConnApi);
        GameManagerControl(pGameManager, FourCC('l', 'o', 'c', 'k'), 1, 0, NULL);
        pGameManager->eState = ST_LOCKED;
        memset(&pGameManager->LastPlay, 0, sizeof(pGameManager->LastPlay));
        pGameManager->bStarted = 0;
        ConnApiControl(pGameManager->pConnApi, FourCC('g', 'p', 'r', 't'),
                       (pPlay->uGamePort != 0) ? pPlay->uGamePort : pGameManager->uOldGamePort, 0, NULL);
        ConnApiControl(pGameManager->pConnApi, FourCC('v', 'p', 'r', 't'),
                       (pPlay->uVoipPort != 0) ? pPlay->uVoipPort : pGameManager->uOldVoipPort, 0, NULL);
        pPlay->strSess[0] = 0;
        _UpdateRecord(&pGameManager->CurrPlay, pPlay);
        return;
    }

    if (pGameManager->eState == ST_MIGRATING)
    {
        if (pPlay->strSess[0] == 0)
        {
            NetPrintf(("gamemanager: deferring ConnApi migration as the session string is not yet available\n"));
            return;
        }
        ConnApiControl(pGameManager->pConnApi, FourCC('s', 'e', 's', 's'), 0, 0, pPlay->strSess);
        ConnApiControl(pGameManager->pConnApi, FourCC('n', 'o', 'n', 'c'), 0, 0, pPlay->strPlatParams);
        NetPrintf(("gamemanager: completing host migration\n"));
        ConnApiControl(pGameManager->pConnApi, FourCC('m', 'i', 'g', 'r'), 0, 0, pPlay->strSess);
        GameManagerControl(pGameManager, FourCC('l', 'o', 'c', 'k'), 0, 0, NULL);
        pGameManager->eState = ST_MEMBER;
    }

    if (pGameManager->iLocked > 0)
    {
        NetPrintf(("gamemanager: lock mode enabled; caching game update\n"));
        _UpdateRecord(&pGameManager->CurrPlay, pPlay);
        return;
    }

    s32 iResult;
    if (pGameManager->LastPlay.iCount != 0)
    {
        if (pPlay->strSess[0] == 0)
        {
            NetPrintf(("gamemanager: deferring game update as the session string is not yet available\n"));
            return;
        }
        iResult = pGameManager->bStarted ? 1 : _GameManagerUpdateGame(pGameManager, pPlay);
    }
    else
    {
        iResult = _GameManagerCreateGame(pGameManager, pPlay);
    }
    if (iResult == 1)
    {
        _UpdateRecord(&pGameManager->LastPlay, pPlay);
    }
}

// Lobby events: 'game' / 'play' carry the game record, 'user' reports our own record.
static void _GameManagerLobbyEventCallback(LobbyApiRefT* pLobbyApi, LobbyApiMsgT* pMsg, void* pUserData)
{
    GameManagerRefT* pGameManager = (GameManagerRefT*)pUserData;
    LobbyApiUserT Self;
    LobbyApiStatus(pLobbyApi, FourCC('s', 'e', 'l', 'f'), &Self, sizeof(Self));

    if (pMsg->kind == FourCC('u', 's', 'e', 'r'))
    {
        if ((Self.game != 0) || (pGameManager->eState == ST_IDLE))
        {
            return;
        }
        if ((pGameManager->CurrPlay.uSysflags & 0x80000) != 0x80000)
        {
            _GameManagerDisconnect(pGameManager);
        }
        return;
    }
    if ((pMsg->kind != FourCC('g', 'a', 'm', 'e')) && (pMsg->kind != FourCC('p', 'l', 'a', 'y')))
    {
        return;
    }

    LobbyApiPlayT Play;
    LobbyApiExtractPlayRecord(&Play, pMsg->pData);
    const s32 iLastIdent = pGameManager->LastPlay.iIdent;
    if (!(((iLastIdent == Play.iIdent) || (iLastIdent <= 0)) && ((iLastIdent != 0) || (Self.game == Play.iIdent))))
    {
        return;
    }

    _GameManagerProcessPlayRecord(pGameManager, &Play);
    _GameManagerExecCallback(pGameManager, GAMEMANAGER_CBTYPE_PLAYINFO, pMsg->kind, pMsg->pData);

    if ((pGameManager->uGameServerAddr != 0) && (Play.iCount < 1))
    {
        NetPrintf(("gamemanager: ignoring game event for server-hosted game with just the server available, but disconnecting player\n"));
        if (pGameManager->eState != ST_IDLE)
        {
            _GameManagerDisconnect(pGameManager);
        }
        return;
    }
    if (pMsg->kind == FourCC('g', 'a', 'm', 'e'))
    {
        _GameManagerEventGame(pGameManager, &Play);
    }
    if (pMsg->kind == FourCC('p', 'l', 'a', 'y'))
    {
        NetPrintf(("gamemanager: got 'play' event\n"));
        _GameManagerUpdateGame(pGameManager, &Play);
        ConnApiStart(pGameManager->pConnApi, 0);
        pGameManager->bStarted = 1;
    }
}

// The lobby connection dropped: the game is over.
static void _GameManagerLobbyDisconnectCb(LobbyApiRefT* /*pLobbyApi*/, LobbyApiMsgT* pMsg, void* pUserData)
{
    if (pMsg->kind == FourCC('d', 'i', 's', 'c'))
    {
        _GameManagerDisconnect((GameManagerRefT*)pUserData);
    }
}

// ConnApi status changes: session events drive the lobby record, then the previous
// ConnApi callback sees every event.
static void _GameManagerConnApiCallback(ConnApiRefT* pConnApi, ConnApiCbInfoT* pCbInfo, void* pUserData)
{
    GameManagerRefT* pGameManager = (GameManagerRefT*)pUserData;
    if (pCbInfo->eType == 2)
    {
        if (pCbInfo->eNewStatus == 3)
        {
            char strSess[128];
            char strNonce[16];
            char strRequest[256];
            memset(strSess, 0, sizeof(strSess));
            memset(strNonce, 0, sizeof(strNonce));
            ConnApiStatus(pGameManager->pConnApi, FourCC('s', 'e', 's', 's'), strSess, sizeof(strSess));
            ConnApiStatus(pGameManager->pConnApi, FourCC('n', 'o', 'n', 'c'), strNonce, sizeof(strNonce));
            strRequest[0] = 0;
            TagFieldSetString(strRequest, sizeof(strRequest), "SESS", strSess);
            TagFieldSetString(strRequest, sizeof(strRequest), "PLATPARAMS", strNonce);
            if (pGameManager->bStarted)
            {
                TagFieldSetNumber(strRequest, sizeof(strRequest), "FORCE", 1);
            }
            NetPrintf(("gamemanager: updating session info - '%s'\n", strRequest));
            LobbyApiRequestCB(pGameManager->pLobbyApi, FourCC('g', 's', 'e', 't'), strRequest, NULL, NULL);
            if (pGameManager->eState == ST_NEWHOST)
            {
                GameManagerControl(pGameManager, FourCC('l', 'o', 'c', 'k'), 0, 0, NULL);
                pGameManager->eState = ST_HOST;
            }
        }
        if (pCbInfo->eNewStatus == 5)
        {
            NetPrintf(("gamemanager: session has been deleted\n"));
            if (pGameManager->eState != ST_LOCKED)
            {
                _GameManagerExecCallback(pGameManager, GAMEMANAGER_CBTYPE_POST_DEL, 0, NULL);
            }
        }
    }
    if (pGameManager->pConnApiCallback != NULL)
    {
        pGameManager->pConnApiCallback(pConnApi, pCbInfo, pGameManager->pConnApiUserData);
    }
}

static void _GameManagerIdle(void* pData, u32 /*uTick*/)
{
    GameManagerRefT* pGameManager = (GameManagerRefT*)pData;
    if (pGameManager->bAutoUpdate)
    {
        GameManagerUpdate(pGameManager);
    }
}

static s32 _GameManagerHostCb(GameManagerRefT* pGameManager, const char* pParams, LobbyApiCallbackT* pCallback,
                              void* pUserData)
{
    if (pGameManager->eState != ST_IDLE)
    {
        NetPrintf(("gamemanager: can't host game while not in idle state\n"));
        return -1;
    }
    char strParams[512];
    memset(&pGameManager->LastPlay, 0, sizeof(pGameManager->LastPlay));
    pGameManager->bStarted = 0;
    TagFieldDupl(strParams, sizeof(strParams), pParams);
    if (pGameManager->bHostMigration)
    {
        const s32 iSysFlags = TagFieldGetNumber(TagFieldFind(strParams, "SYSFLAGS"), 0);
        TagFieldSetNumber(strParams, sizeof(strParams), "SYSFLAGS", iSysFlags | 0x2000);
    }
    const s32 iKind = pGameManager->bGPSGameServ ? FourCC('g', 'p', 's', 'c') : FourCC('g', 'c', 'r', 'e');
    return LobbyApiRequestCB(pGameManager->pLobbyApi, iKind, strParams, pCallback, pUserData);
}

static s32 _GameManagerJoinCb(GameManagerRefT* pGameManager, const char* pParams, LobbyApiCallbackT* pCallback,
                              void* pUserData)
{
    if (((pGameManager->eState == ST_HOST) || (pGameManager->eState == ST_MEMBER)) &&
        (TagFieldGetNumber(TagFieldFind(pParams, "IDENT"), -1) == pGameManager->LastPlay.iIdent) &&
        (TagFieldGetNumber(TagFieldFind(pParams, "GUESTS"), -1) > 0))
    {
        NetPrintf(("gamemanager: assuming game join request for game we are currently hosting is a guest join\n"));
        return LobbyApiRequestCB(pGameManager->pLobbyApi, FourCC('g', 'j', 'o', 'i'), pParams, pCallback, pUserData);
    }
    if ((u32)pGameManager->eState < 2u)
    {
        memset(&pGameManager->LastPlay, 0, sizeof(pGameManager->LastPlay));
        pGameManager->bStarted = 0;
        return LobbyApiRequestCB(pGameManager->pLobbyApi, FourCC('g', 'j', 'o', 'i'), pParams, pCallback, pUserData);
    }
    NetPrintf(("gamemanager: can't join game while not in idle or host state\n"));
    return -1;
}

static s32 _GameManagerQuickJoinCb(GameManagerRefT* pGameManager, const char* pParams, LobbyApiCallbackT* pCallback,
                                   void* pUserData)
{
    if (pGameManager->eState == ST_IDLE)
    {
        memset(&pGameManager->LastPlay, 0, sizeof(pGameManager->LastPlay));
        pGameManager->bStarted = 0;
    }
    else if (pGameManager->eState != ST_HOST)
    {
        NetPrintf(("gamemanager: can't join game while not in idle or host state\n"));
        return -1;
    }
    return LobbyApiRequestCB(pGameManager->pLobbyApi, FourCC('g', 'q', 'w', 'k'), pParams, pCallback, pUserData);
}

static s32 _GameManagerStartCb(GameManagerRefT* pGameManager, const char* pParams, LobbyApiCallbackT* pCallback,
                               void* pUserData)
{
    if (pGameManager->eState != ST_HOST)
    {
        NetPrintf(("gamemanager: can't start game while not in hosting state\n"));
        return -1;
    }
    return LobbyApiRequestCB(pGameManager->pLobbyApi, FourCC('g', 's', 't', 'a'), pParams, pCallback, pUserData);
}

static s32 _GameManagerLeaveCb(GameManagerRefT* pGameManager, const char* pParams, LobbyApiCallbackT* pCallback,
                               void* pUserData)
{
    if (pGameManager->eState == ST_IDLE)
    {
        NetPrintf(("gamemanager: can't leave game while in idle state\n"));
        return -1;
    }
    char strGuest[16];
    TagFieldGetString(TagFieldFind(pParams, "GUEST"), strGuest, sizeof(strGuest), "");
    const s32 iKind = ((strGuest[0] != 0) || pGameManager->bHostMigration || (pGameManager->eState != ST_HOST))
                          ? FourCC('g', 'l', 'e', 'a')
                          : FourCC('g', 'd', 'e', 'l');
    return LobbyApiRequestCB(pGameManager->pLobbyApi, iKind, pParams, pCallback, pUserData);
}

// ---- public API --------------------------------------------------------------------------

extern "C" GameManagerRefT* GameManagerCreate(LobbyApiRefT* pLobbyApi, ConnApiRefT* pConnApi)
{
    const s32 iMemGroup = DirtyMemGroupQuery();
    GameManagerRefT* pGameManager = (GameManagerRefT*)DirtyMemAlloc(sizeof(GameManagerRefT), KI_MEMID, iMemGroup);
    if (pGameManager == NULL)
    {
        NetPrintf(("gamemanager: could not allocate module state\n"));
        return NULL;
    }
    memset(pGameManager, 0, sizeof(*pGameManager));
    pGameManager->iMemGroup     = iMemGroup;
    pGameManager->pLobbyApi     = pLobbyApi;
    pGameManager->pConnApi      = pConnApi;
    pGameManager->iLobbyEventCb = LobbyApiSetCallback(pLobbyApi, LOBBYAPI_CBTYPE_EVNT, &_GameManagerLobbyEventCallback,
                                                      pGameManager);
    pGameManager->iDiscEventCb  = LobbyApiSetCallback(pLobbyApi, LOBBYAPI_CBTYPE_CONN, &_GameManagerLobbyDisconnectCb,
                                                      pGameManager);
    ConnApiStatus(pConnApi, FourCC('c', 'b', 'f', 'p'), &pGameManager->pConnApiCallback,
                  sizeof(pGameManager->pConnApiCallback));
    ConnApiStatus(pConnApi, FourCC('c', 'b', 'u', 'p'), &pGameManager->pConnApiUserData,
                  sizeof(pGameManager->pConnApiUserData));
    ConnApiControl(pConnApi, FourCC('c', 'b', 'f', 'p'), 0, 0, (const void*)&_GameManagerConnApiCallback);
    ConnApiControl(pConnApi, FourCC('c', 'b', 'u', 'p'), 0, 0, pGameManager);
    pGameManager->uGameServFallback = 0;
    pGameManager->bAutoUpdate       = 1;
    pGameManager->uGameServConnMode = 3;
    pGameManager->uOldGamePort      = 0;
    pGameManager->uOldVoipPort      = 0;
    NetConnIdleAdd(&_GameManagerIdle, pGameManager);
    return pGameManager;
}

extern "C" void GameManagerDestroy(GameManagerRefT* pGameManager)
{
    NetPrintf(("gamemanager: destroy\n"));
    NetConnIdleDel(&_GameManagerIdle, pGameManager);
    ConnApiControl(pGameManager->pConnApi, FourCC('c', 'b', 'f', 'p'), 0, 0, (const void*)pGameManager->pConnApiCallback);
    ConnApiControl(pGameManager->pConnApi, FourCC('c', 'b', 'u', 'p'), 0, 0, pGameManager->pConnApiUserData);
    LobbyApiClearCallback(pGameManager->pLobbyApi, pGameManager->iLobbyEventCb);
    LobbyApiClearCallback(pGameManager->pLobbyApi, pGameManager->iDiscEventCb);
    DirtyMemFree(pGameManager, KI_MEMID, pGameManager->iMemGroup);
}

extern "C" void GameManagerOnline(GameManagerRefT* pGameManager, const char* pSelfName)
{
    strncpy(pGameManager->strSelfName, pSelfName, sizeof(pGameManager->strSelfName));
    pGameManager->strSelfName[sizeof(pGameManager->strSelfName) - 1] = 0;
}

extern "C" void GameManagerSetCallback(GameManagerRefT* pGameManager, GameManagerCallbackT* pCB, void* pUserData)
{
    pGameManager->pCBFunction = pCB;
    pGameManager->pCBUserData = pUserData;
}

extern "C" s32 GameManagerRequestCb(GameManagerRefT* pGameManager, s32 iKind, const char* pParams,
                                    LobbyApiCallbackT* pCallback, void* pUserData)
{
    if (iKind == FourCC('g', 'c', 'r', 'e'))
    {
        return _GameManagerHostCb(pGameManager, pParams, pCallback, pUserData);
    }
    if (iKind == FourCC('g', 'j', 'o', 'i'))
    {
        return _GameManagerJoinCb(pGameManager, pParams, pCallback, pUserData);
    }
    if (iKind == FourCC('g', 'q', 'w', 'k'))
    {
        return _GameManagerQuickJoinCb(pGameManager, pParams, pCallback, pUserData);
    }
    if (iKind == FourCC('g', 's', 't', 'a'))
    {
        return _GameManagerStartCb(pGameManager, pParams, pCallback, pUserData);
    }
    if (iKind == FourCC('g', 'l', 'e', 'a'))
    {
        return _GameManagerLeaveCb(pGameManager, pParams, pCallback, pUserData);
    }
    return LobbyApiRequestCB(pGameManager->pLobbyApi, iKind, pParams, pCallback, pUserData);
}

extern "C" s32 GameManagerStatus(GameManagerRefT* pGameManager, s32 iSelect, void* pBuf, s32 iBufSize)
{
    if (iSelect == FourCC('g', 'n', 'f', 'o'))
    {
        if ((pBuf != NULL) && (iBufSize == (s32)sizeof(LobbyApiPlayT)))
        {
            memcpy(pBuf, &pGameManager->CurrPlay, sizeof(LobbyApiPlayT));
            return 0;
        }
        return -1;
    }
    if (iSelect == FourCC('g', 's', 'r', 'v'))
    {
        return pGameManager->bGPSGameServ;
    }
    if (iSelect == FourCC('g', 's', 'g', 'a'))
    {
        return pGameManager->uGameServerAddr != 0;
    }
    if (iSelect == FourCC('i', 'd', 'l', 'e'))
    {
        return pGameManager->eState == ST_IDLE;
    }
    if (iSelect == FourCC('m', 'g', 'r', 't'))
    {
        return pGameManager->bHostMigration;
    }
    return -1;
}

extern "C" s32 GameManagerControl(GameManagerRefT* pGameManager, s32 iControl, s32 iValue, s32 iValue2, void* pValue)
{
    if (iControl == FourCC('a', 'u', 't', 'o'))
    {
        pGameManager->bAutoUpdate = (u8)iValue;
        return 0;
    }
    if (iControl == FourCC('g', 's', 'r', 'v'))
    {
        pGameManager->bGPSGameServ  = (u8)iValue;
        pGameManager->eGameServMode = iValue2;
        return 0;
    }
    if (iControl == FourCC('g', 's', 'v', '2'))
    {
        pGameManager->uGameServConnMode = (u32)iValue;
        pGameManager->uGameServFallback = (u32)iValue2;
        return ConnApiControl(pGameManager->pConnApi, FourCC('g', 's', 'v', '2'), iValue, iValue2, pValue);
    }
    if (iControl == FourCC('i', 's', 'r', 'v'))
    {
        pGameManager->bInternalServer = (u8)iValue;
        return 0;
    }
    if (iControl == FourCC('l', 'o', 'c', 'k'))
    {
        if ((pGameManager->iLocked == 1) && (iValue == 0))
        {
            NetPrintf(("gamemanager: unlocking game object state and processing most recent game update\n"));
            pGameManager->iLocked = 0;
            if (pGameManager->CurrPlay.iIdent != 0)
            {
                _GameManagerEventGame(pGameManager, &pGameManager->CurrPlay);
            }
            return 0;
        }
        if ((pGameManager->iLocked == 0) && (iValue == 1))
        {
            NetPrintf(("gamemanager: locking game object state\n"));
            pGameManager->iLocked = 1;
            memset(&pGameManager->CurrPlay, 0, sizeof(pGameManager->CurrPlay));
            return 0;
        }
        if ((pGameManager->iLocked > 0) || (iValue == 1))
        {
            pGameManager->iLocked = (s8)(pGameManager->iLocked + ((iValue == 0) ? -1 : 1));
            NetPrintf(("gamemanager: refcounting game object lock state to %d\n", pGameManager->iLocked));
        }
        return 0;
    }
    if (iControl == FourCC('m', 'g', 'r', 't'))
    {
        pGameManager->bHostMigration = (u8)iValue;
        return 0;
    }
    return -1;
}

extern "C" void GameManagerUpdate(GameManagerRefT* pGameManager)
{
    if ((pGameManager->eState == ST_LOCKED) &&
        (ConnApiStatus(pGameManager->pConnApi, FourCC('i', 'd', 'l', 'e'), NULL, 0) != 0))
    {
        GameManagerControl(pGameManager, FourCC('l', 'o', 'c', 'k'), 0, 0, NULL);
        if (pGameManager->eState == ST_LOCKED)
        {
            pGameManager->eState = ST_READY;
        }
    }
}
