// ============================================================================
// connapipc.cpp -- ConnApi for the PC backend.
//
// [PC platform layer] The console ConnApi negotiated a secure connection (demangling, game
// server fallback, voice) to every session member. The PC keeps the console's member list,
// client entry layout, status codes and callback contract, and opens each game connection as a
// pclan link (lan/pclan_link.cpp):
//   INIT -> CONN   a link to the member's XUID (from its DirtyAddrT) is created
//   CONN -> ACTV   the member greeted us (LINK_HELLO / LINK_ACK); pGameLinkRef is live
//   CONN -> DISC   the member never answered within the session timeout ('time')
//   ACTV -> DISC   the member left (pclan BYE)
// A hosted session gets its session text ('sess') when it is created, as the console's session
// service gave it: the session-info block (session id, host address, key) in the
// ProtoMangleEncodeSession form. The PC block is the host's XUID as the session id, the title
// address XNetGetTitleXnAddr reports, and an all-zero key (the PC links are not keyed).
// Every status change goes through the ConnApiCreate callback exactly once. There are no
// voice connections (no voice devices on the PC): VoipInfo stays INIT and iVoipConnId -1.
// Offline no member list ever arrives (there is no lobby), so the list stays empty.
// ============================================================================

#include "connapi.h"
#include "netconn.h"
#include "protomangle.h"
#include "dirtymem.h"
#include "dirtylib.h"
#include "lan/pclan.h"
#include "lan/pclan_link.h"

#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/System/PC/CgsPcNetIdentity.h"   // CgsPcNetIdentityXuid

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

using namespace CgsNetwork::DirtySock;

// The title's 36-byte network address (CgsXboxLivePC.cpp).
extern "C" u32 XNetGetTitleXnAddr(void* lpXnAddr);

namespace
{
    // DirtyMem module tag ('conn').
    const s32 KI_MEM_MODULE_CONN = 0x636F6E6E;

    // Session states.
    const s32 KI_STATE_IDLE          = 0;
    const s32 KI_STATE_HOSTING       = 3;
    const s32 KI_STATE_JOINED        = 4;
    const s32 KI_STATE_DISCONNECTING = 5;

    // The callback type of the session events (session created on the host, session torn down);
    // it has no name in the type table. The client index these events carry is not meaningful.
    const s32 KI_CBTYPE_SESSION = 2;

    // Client flags.
    const u16 KU_CLIENTFLAG_REMOVE = 1;
    // Connection flags.
    const u16 KU_CONNFLAG_GAME = 1;
    const u16 KU_CONNFLAG_VOIP = 2;

    // Defaults the console module starts with.
    const u16 KU_DEFAULT_VOIP_PORT    = 6000;
    const s32 KI_DEFAULT_LINK_BUFFER  = 1024;
    const s32 KI_DEFAULT_TIMEOUT_MS   = 15000;
    const s32 KI_DEFAULT_TUNNEL_PORT  = 1000;

    // Selectors.
    const s32 KI_SEL_PEER = 0x70656572;   // 'peer'
    const s32 KI_SEL_MWID = 0x6D776964;   // 'mwid'
    const s32 KI_SEL_MNGL = 0x6D6E676C;   // 'mngl'
    const s32 KI_SEL_GSV2 = 0x67737632;   // 'gsv2'
    const s32 KI_SEL_GPRT = 0x67707274;   // 'gprt'
    const s32 KI_SEL_VPRT = 0x76707274;   // 'vprt'
    const s32 KI_SEL_SLOT = 0x736C6F74;   // 'slot'
    const s32 KI_SEL_SFLG = 0x73666C67;   // 'sflg'
    const s32 KI_SEL_SESS = 0x73657373;   // 'sess'
    const s32 KI_SEL_SKIL = 0x736B696C;   // 'skil'
    const s32 KI_SEL_TYPE = 0x74797065;   // 'type'
    const s32 KI_SEL_LBUF = 0x6C627566;   // 'lbuf'
    const s32 KI_SEL_TIME = 0x74696D65;   // 'time'
    const s32 KI_SEL_TUNL = 0x74756E6C;   // 'tunl'
    const s32 KI_SEL_HSTI = 0x68737469;   // 'hsti'
    const s32 KI_SEL_RCBK = 0x7263626B;   // 'rcbk'
    const s32 KI_SEL_MINP = 0x6D696E70;   // 'minp'
    const s32 KI_SEL_MOUT = 0x6D6F7574;   // 'mout'
    const s32 KI_SEL_NONC = 0x6E6F6E63;   // 'nonc'
    const s32 KI_SEL_SELF = 0x73656C66;   // 'self'
    const s32 KI_SEL_HOST = 0x686F7374;   // 'host'
    const s32 KI_SEL_INGM = 0x696E676D;   // 'ingm'
    const s32 KI_SEL_IDLE = 0x69646C65;   // 'idle'
    const s32 KI_SEL_FAIL = 0x6661696C;   // 'fail'
    const s32 KI_SEL_GSRV = 0x67737276;   // 'gsrv'
    const s32 KI_SEL_CBFP = 0x63626670;   // 'cbfp': the status callback
    const s32 KI_SEL_CBUP = 0x63627570;   // 'cbup': the status callback's user data
    const s32 KI_SEL_MIGR = 0x6D696772;   // 'migr'

    const char* const KAPC_STATUS_NAMES[] =
    {
        "CONNAPI_STATUS_INIT", "CONNAPI_STATUS_CONN", "CONNAPI_STATUS_MNGL", "CONNAPI_STATUS_ACTV",
        "CONNAPI_STATUS_CLSE", "CONNAPI_STATUS_DISC", "CONNAPI_STATUS_DEST",
    };
    const char* const KAPC_CBTYPE_NAMES[] = { "CONNAPI_CBTYPE_GAMEEVENT", "CONNAPI_CBTYPE_VOIPEVENT" };

    const char* ConnApiStatusName(s32 iStatus)
    {
        return ((iStatus >= 0) && (iStatus <= CONNAPI_STATUS_DEST)) ? KAPC_STATUS_NAMES[iStatus] : "?";
    }

    s32 giConnApiLogLeft = 48;

    void ConnApiLog(const char* pFormat, ...)
    {
        if (giConnApiLogLeft <= 0)
        {
            return;
        }
        char strText[256];
        int iPrefix = _snprintf_s(strText, sizeof(strText), _TRUNCATE, "[net] connapi ");
        va_list Args;
        va_start(Args, pFormat);
        _vsnprintf_s(strText + iPrefix, sizeof(strText) - iPrefix, _TRUNCATE, pFormat, Args);
        va_end(Args);
        size_t uLen = strlen(strText);
        if (uLen + 1 < sizeof(strText))
        {
            strText[uLen] = '\n';
            strText[uLen + 1] = '\0';
        }
        if (--giConnApiLogLeft == 0)
        {
            strncat_s(strText, sizeof(strText), "[net] connapi (log budget spent)\n", _TRUNCATE);
        }
        CgsDev::Log::WriteToLog(strText);
    }

    void ConnApiCopyString(char* pDst, s32 iDstSize, const char* pSrc)
    {
        if ((pDst == NULL) || (iDstSize <= 0))
        {
            return;
        }
        strncpy_s(pDst, (size_t)iDstSize, (pSrc != NULL) ? pSrc : "", _TRUNCATE);
    }

    // The callback used when ConnApiCreate got none: log the change.
    void ConnApiDefaultCallback(ConnApiRefT* /*pConnApi*/, ConnApiCbInfoT* pCbInfo, void* /*pUserData*/)
    {
        const char* pType = ((pCbInfo->eType >= 0) && (pCbInfo->eType <= CONNAPI_CBTYPE_VOIPEVENT))
                            ? KAPC_CBTYPE_NAMES[pCbInfo->eType] : "";
        NetPrintf(("connapi: client %d) [%s] %s -> %s\n", pCbInfo->iClientIndex, pType,
                   ConnApiStatusName(pCbInfo->eOldStatus), ConnApiStatusName(pCbInfo->eNewStatus)));
    }
}

struct CgsNetwork::DirtySock::ConnApiRefT
{
    ConnApiCallbackT* pCallback;          // never null
    void*             pUserData;
    s32               iMemGroup;
    s32               iNextClientId;      // next id handed to a client that brought none
    u16               uGamePort;          // 'gprt'
    u16               uVoipPort;          // 'vprt'
    u16               uConnFlags;         // 'type': which connections each client gets
    u16               uGameServConnMode;  // 'gsv2' value
    u16               uGameServFallback;  // 'gsv2' second value
    u16               uPad;
    s32               iSessFlags;         // ConnApiHost / ConnApiConnect session flags
    s32               iLinkBufSize;       // 'lbuf'
    s32               iTimeout;           // 'time': how long a game connection may take to come up
    s32               iTunnelPort;        // 'tunl' second value
    s32               iMangleWait;        // 'mwid'
    s32               iMinPorts;          // 'minp'
    s32               iMaxPorts;          // 'mout'
    s32               iSessionFlags;      // 'sflg'
    s32               iPublicSlots;       // 'slot'
    s32               iPrivateSlots;      // 'slot' second value
    s32               iHostIndex;         // 'hsti': the host's index in the list
    u32               uGameServerAddr;    // 'gsrv' (no game server path on this platform)
    s32               iGameServerMode;    // 'gsrv' second value
    s32               eState;             // KI_STATE_*
    s32               iSelf;              // the local player's index in the list
    u8                bPeerToPeer;        // 'peer': every member connects to every member
    u8                bMangle;            // 'mngl'
    u8                bHosting;           // this instance hosts the session
    u8                bInCallback;        // a status callback is running
    u8                bRemoveCallbacks;   // 'rcbk': report the final statuses of a removed client
    u8                bTunnel;            // 'tunl'
    u8                bSlotsSet;          // 'slot' was set
    u8                bSessionPending;    // a new session's creation is reported on the next update
    char              strGameName[32];    // ConnApiOnline
    char              strSelfName[32];    // ConnApiOnline
    DirtyAddrT        SelfAddr;           // ConnApiOnline, refreshed from the member list
    char              strSession[128];    // 'sess'
    u8                aNonce[16];         // 'nonc'
    ConnApiClientListT ClientList;        // must stay last: Clients[] runs past the struct
};

namespace
{
    ConnApiClientT* ConnApiClient(ConnApiRefT* pConnApi, s32 iIndex)
    {
        return &pConnApi->ClientList.Clients[iIndex];
    }

    void ConnApiNotify(ConnApiRefT* pConnApi, s32 iIndex, s32 eType, s32 eOldStatus, s32 eNewStatus)
    {
        if (eOldStatus == eNewStatus)
        {
            return;
        }
        ConnApiCbInfoT CbInfo;
        CbInfo.iClientIndex = iIndex;
        CbInfo.eType        = eType;
        CbInfo.eOldStatus   = eOldStatus;
        CbInfo.eNewStatus   = eNewStatus;
        pConnApi->bInCallback = 1;
        pConnApi->pCallback(pConnApi, &CbInfo, pConnApi->pUserData);
        pConnApi->bInCallback = 0;
    }

    // A client entry from the member description: session ports where the member gave none, an
    // id when it had none, no external address until the connection resolves it, no voice.
    void ConnApiInitClient(ConnApiRefT* pConnApi, ConnApiClientT* pClient, const ConnApiUserInfoT* pUserInfo)
    {
        memset(pClient, 0, sizeof(*pClient));
        pClient->UserInfo    = *pUserInfo;
        pClient->iVoipConnId = -1;
        pClient->GameInfo.uMnglPort  = pConnApi->uGamePort;
        pClient->VoipInfo.uMnglPort  = pConnApi->uVoipPort;
        pClient->GameInfo.uLocalPort = (pUserInfo->uLocalGamePort != 0) ? pUserInfo->uLocalGamePort : pConnApi->uGamePort;
        pClient->VoipInfo.uLocalPort = (pUserInfo->uLocalVoipPort != 0) ? pUserInfo->uLocalVoipPort : pConnApi->uVoipPort;
        pClient->UserInfo.uAddr = 0;
        if (pClient->UserInfo.uClientId == 0)
        {
            pClient->UserInfo.uClientId = (u32)pConnApi->iNextClientId++;
        }
        ConnApiCopyString(pClient->UserInfo.strTunnelKey, 18, pClient->UserInfo.DirtyAddr.strMachineAddr);
    }

    // Which connections each client gets: all of 'type', except that without 'peer' only the
    // host gets a game connection.
    void ConnApiApplyConnFlags(ConnApiRefT* pConnApi)
    {
        for (s32 iIndex = 0; iIndex < pConnApi->ClientList.iNumClients; ++iIndex)
        {
            u16 uFlags = pConnApi->uConnFlags;
            if ((iIndex != pConnApi->iHostIndex) && !pConnApi->bPeerToPeer)
            {
                uFlags &= (u16)~KU_CONNFLAG_GAME;
            }
            ConnApiClient(pConnApi, iIndex)->uConnFlags = uFlags;
        }
    }

    s32 ConnApiFindByName(ConnApiRefT* pConnApi, const char* pName)
    {
        for (s32 iIndex = 0; iIndex < pConnApi->ClientList.iNumClients; ++iIndex)
        {
            if (strcmp(ConnApiClient(pConnApi, iIndex)->UserInfo.strName, pName) == 0)
            {
                return iIndex;
            }
        }
        return -1;
    }

    // Tear down a client's game connection; a live link reports DISC -> DEST.
    void ConnApiCloseGame(ConnApiRefT* pConnApi, s32 iIndex, const char* pReason)
    {
        ConnApiClientT* pClient = ConnApiClient(pConnApi, iIndex);
        const bool bHadLink = (pClient->pGameLinkRef != NULL);
        NetPrintf(("connapi: destroying game connection to user %s: %s\n", pClient->UserInfo.strName, pReason));
        if (bHadLink)
        {
            PcLinkDestroy(pClient->pGameLinkRef);
            pClient->pGameLinkRef = NULL;
        }
        pClient->GameInfo.eStatus = CONNAPI_STATUS_DISC;
        if (bHadLink)
        {
            ConnApiNotify(pConnApi, iIndex, CONNAPI_CBTYPE_GAMEEVENT, CONNAPI_STATUS_DISC, CONNAPI_STATUS_DEST);
        }
    }

    void ConnApiCloseClient(ConnApiRefT* pConnApi, s32 iIndex, const char* pReason)
    {
        ConnApiCloseGame(pConnApi, iIndex, pReason);
        ConnApiClientT* pClient = ConnApiClient(pConnApi, iIndex);
        pClient->iVoipConnId       = -1;
        pClient->VoipInfo.eStatus  = CONNAPI_STATUS_DISC;
    }

    void ConnApiFindSelf(ConnApiRefT* pConnApi)
    {
        pConnApi->iSelf = ConnApiFindByName(pConnApi, pConnApi->strSelfName);
    }

    void ConnApiRemoveIndex(ConnApiRefT* pConnApi, s32 iIndex)
    {
        ConnApiClientT* pClient = ConnApiClient(pConnApi, iIndex);
        const s32 eOldGame = pClient->GameInfo.eStatus;
        const s32 eOldVoip = pClient->VoipInfo.eStatus;
        ConnApiLog("remove %s", pClient->UserInfo.strName);
        ConnApiCloseClient(pConnApi, iIndex, "removal");
        if (pConnApi->bRemoveCallbacks)
        {
            if ((pConnApi->uConnFlags & KU_CONNFLAG_GAME) != 0)
            {
                ConnApiNotify(pConnApi, iIndex, CONNAPI_CBTYPE_GAMEEVENT, eOldGame, pClient->GameInfo.eStatus);
            }
            if ((pConnApi->uConnFlags & KU_CONNFLAG_VOIP) != 0)
            {
                ConnApiNotify(pConnApi, iIndex, CONNAPI_CBTYPE_VOIPEVENT, eOldVoip, pClient->VoipInfo.eStatus);
            }
        }
        const s32 iLast = pConnApi->ClientList.iNumClients - 1;
        if (iIndex != iLast)
        {
            memmove(pClient, pClient + 1, sizeof(ConnApiClientT) * (size_t)(iLast - iIndex));
        }
        pConnApi->ClientList.iNumClients = iLast;
        ConnApiFindSelf(pConnApi);
    }

    void ConnApiProcessRemovals(ConnApiRefT* pConnApi)
    {
        for (s32 iIndex = 0; iIndex < pConnApi->ClientList.iNumClients; ++iIndex)
        {
            if ((ConnApiClient(pConnApi, iIndex)->uFlags & KU_CLIENTFLAG_REMOVE) != 0)
            {
                ConnApiRemoveIndex(pConnApi, iIndex);
                --iIndex;
            }
        }
    }

    // A hosted session's text: the encoded session-info block (8-byte session id at +0x00,
    // 36-byte host address at +0x08, 16-byte key at +0x2C).
    void ConnApiHostSessionText(ConnApiRefT* pConnApi)
    {
        u8 aSessionInfo[0x3C];
        memset(aSessionInfo, 0, sizeof(aSessionInfo));
        const u64 uXuid = CgsPcNetIdentityXuid();
        for (s32 iByte = 0; iByte < 8; ++iByte)
        {
            aSessionInfo[iByte] = (u8)(uXuid >> (8 * (7 - iByte)));
        }
        XNetGetTitleXnAddr(&aSessionInfo[0x08]);
        ProtoMangleEncodeSession(pConnApi->strSession, (s32)sizeof(pConnApi->strSession), aSessionInfo);
        ConnApiLog("session created, session text %d bytes", (s32)strlen(pConnApi->strSession));
    }

    // Build the member list of a new session (ConnApiHost / ConnApiConnect).
    void ConnApiBuildList(ConnApiRefT* pConnApi, const ConnApiUserInfoT* pClientList, s32 iNumClients)
    {
        if (iNumClients > pConnApi->ClientList.iMaxClients)
        {
            NetPrintf(("connapi: cannot host %d clients; clamping to %d\n", iNumClients, pConnApi->ClientList.iMaxClients));
            iNumClients = pConnApi->ClientList.iMaxClients;
        }
        pConnApi->iSelf = -1;
        for (s32 iIndex = 0; iIndex < iNumClients; ++iIndex)
        {
            ConnApiClientT* pClient = ConnApiClient(pConnApi, iIndex);
            ConnApiInitClient(pConnApi, pClient, &pClientList[iIndex]);
            if (strcmp(pClient->UserInfo.strName, pConnApi->strSelfName) == 0)
            {
                pConnApi->SelfAddr = pClient->UserInfo.DirtyAddr;
                pConnApi->iSelf    = iIndex;
            }
        }
        pConnApi->ClientList.iNumClients = iNumClients;
        ConnApiApplyConnFlags(pConnApi);
        if (pConnApi->iSelf == -1)
        {
            NetPrintf(("connapi: local user %s not in client list\n", pConnApi->strSelfName));
        }
        for (s32 iIndex = 0; iIndex < iNumClients; ++iIndex)
        {
            const ConnApiClientT* pClient = ConnApiClient(pConnApi, iIndex);
            ConnApiLog("client %d) %s <%s>%s", iIndex, pClient->UserInfo.strName,
                       pClient->UserInfo.DirtyAddr.strMachineAddr, (iIndex == pConnApi->iSelf) ? " (self)" : "");
        }
    }

    s32 ConnApiStartSession(ConnApiRefT* pConnApi, const ConnApiUserInfoT* pClientList, s32 iNumClients,
                            s32 iSessFlags, bool bHost)
    {
        if (pConnApi->eState != KI_STATE_IDLE)
        {
            NetPrintfCode(bHost ? "connapi: can't host a game when not in idle state\n"
                                : "connapi: can't connect to a game when not in idle state\n");
            return -1;
        }
        pConnApi->iSessFlags = iSessFlags;
        ConnApiBuildList(pConnApi, pClientList, iNumClients);
        pConnApi->bHosting = bHost ? 1 : 0;
        pConnApi->eState   = bHost ? KI_STATE_HOSTING : KI_STATE_JOINED;
        pConnApi->bSessionPending = 1;
        ConnApiLog("%s session with %d clients", bHost ? "hosting" : "joining", pConnApi->ClientList.iNumClients);
        return 0;
    }

    // Advance one client's game connection.
    void ConnApiUpdateGame(ConnApiRefT* pConnApi, s32 iIndex, u32 uNow)
    {
        ConnApiClientT* pClient = ConnApiClient(pConnApi, iIndex);
        if ((iIndex == pConnApi->iSelf) || ((pClient->uConnFlags & KU_CONNFLAG_GAME) == 0))
        {
            return;
        }

        switch (pClient->GameInfo.eStatus)
        {
            case CONNAPI_STATUS_INIT:
            {
                u64 uXuid = 0;
                if (!PcLanActive() || !PcLinkXuidFromAddr(pClient->UserInfo.DirtyAddr.strMachineAddr, &uXuid))
                {
                    ConnApiLog("no route to %s <%s>", pClient->UserInfo.strName, pClient->UserInfo.DirtyAddr.strMachineAddr);
                    pClient->GameInfo.eStatus = CONNAPI_STATUS_DISC;
                    ConnApiNotify(pConnApi, iIndex, CONNAPI_CBTYPE_GAMEEVENT, CONNAPI_STATUS_INIT, CONNAPI_STATUS_DISC);
                    break;
                }
                DirtyMemGroupEnter(pConnApi->iMemGroup);
                pClient->pGameLinkRef = PcLinkCreate(uXuid);
                DirtyMemGroupLeave();
                pClient->GameInfo.eStatus = CONNAPI_STATUS_CONN;
                ConnApiNotify(pConnApi, iIndex, CONNAPI_CBTYPE_GAMEEVENT, CONNAPI_STATUS_INIT, CONNAPI_STATUS_CONN);
                break;
            }

            case CONNAPI_STATUS_CONN:
            {
                NetGameLinkRefT* pLink = pClient->pGameLinkRef;
                PcLinkPoll(pLink);
                if (pLink->bUp)
                {
                    pClient->UserInfo.uAddr      = pLink->Peer.uAddr;
                    pClient->GameInfo.uMnglPort  = pLink->Peer.uPort;
                    pClient->GameInfo.eStatus    = CONNAPI_STATUS_ACTV;
                    ConnApiLog("game connection to %s active", pClient->UserInfo.strName);
                    ConnApiNotify(pConnApi, iIndex, CONNAPI_CBTYPE_GAMEEVENT, CONNAPI_STATUS_CONN, CONNAPI_STATUS_ACTV);
                }
                else if (pLink->bDown || ((s32)(uNow - pLink->uOpenTick) >= pConnApi->iTimeout))
                {
                    ConnApiLog("game connection to %s failed", pClient->UserInfo.strName);
                    PcLinkDestroy(pLink);
                    pClient->pGameLinkRef     = NULL;
                    pClient->GameInfo.eStatus = CONNAPI_STATUS_DISC;
                    ConnApiNotify(pConnApi, iIndex, CONNAPI_CBTYPE_GAMEEVENT, CONNAPI_STATUS_CONN, CONNAPI_STATUS_DISC);
                }
                break;
            }

            case CONNAPI_STATUS_ACTV:
            {
                NetGameLinkRefT* pLink = pClient->pGameLinkRef;
                if (pLink->bDown)
                {
                    ConnApiLog("game connection to %s lost", pClient->UserInfo.strName);
                    PcLinkDestroy(pLink);
                    pClient->pGameLinkRef     = NULL;
                    pClient->GameInfo.eStatus = CONNAPI_STATUS_DISC;
                    ConnApiNotify(pConnApi, iIndex, CONNAPI_CBTYPE_GAMEEVENT, CONNAPI_STATUS_ACTV, CONNAPI_STATUS_DISC);
                }
                break;
            }

            default:
                break;
        }
    }

    void ConnApiIdleProc(void* pData, u32 /*uTick*/)
    {
        ConnApiUpdate(static_cast<ConnApiRefT*>(pData));
    }
}

extern "C" ConnApiRefT* ConnApiCreate(s32 iGamePort, s32 iMaxClients, ConnApiCallbackT* pCallback, void* pUserData)
{
    const s32 iMemGroup = DirtyMemGroupQuery();
    const s32 iExtra    = (iMaxClients > 1) ? (iMaxClients - 1) : 0;
    const s32 iSize     = (s32)(sizeof(ConnApiRefT) + (size_t)iExtra * sizeof(ConnApiClientT));
    ConnApiRefT* pConnApi = static_cast<ConnApiRefT*>(DirtyMemAlloc(iSize, KI_MEM_MODULE_CONN, iMemGroup));
    if (pConnApi == NULL)
    {
        NetPrintf(("connapi: could not allocate module state\n"));
        return NULL;
    }
    memset(pConnApi, 0, (size_t)iSize);

    pConnApi->iMemGroup     = iMemGroup;
    // On the LAN the game connection shares the instance's pclan port.
    pConnApi->uGamePort     = PcLanActive() ? PcLanSelf().uPort : (u16)iGamePort;
    pConnApi->uVoipPort     = KU_DEFAULT_VOIP_PORT;
    pConnApi->pCallback     = (pCallback != NULL) ? pCallback : &ConnApiDefaultCallback;
    pConnApi->pUserData     = pUserData;
    pConnApi->iLinkBufSize  = KI_DEFAULT_LINK_BUFFER;
    pConnApi->iNextClientId = 1;
    pConnApi->uConnFlags    = KU_CONNFLAG_GAME | KU_CONNFLAG_VOIP;
    pConnApi->iTimeout      = KI_DEFAULT_TIMEOUT_MS;
    pConnApi->iTunnelPort   = KI_DEFAULT_TUNNEL_PORT;
    pConnApi->uGameServConnMode = KU_CONNFLAG_GAME | KU_CONNFLAG_VOIP;
    pConnApi->uGameServFallback = 0;
    pConnApi->ClientList.iMaxClients = iMaxClients;

    NetConnIdleAdd(&ConnApiIdleProc, pConnApi);
    return pConnApi;
}

extern "C" void ConnApiDestroy(ConnApiRefT* pConnApi)
{
    if ((pConnApi->eState != KI_STATE_IDLE) && (pConnApi->eState != KI_STATE_DISCONNECTING))
    {
        ConnApiDisconnect(pConnApi);
    }
    NetConnIdleDel(&ConnApiIdleProc, pConnApi);
    for (s32 iIndex = 0; iIndex < pConnApi->ClientList.iNumClients; ++iIndex)
    {
        ConnApiClientT* pClient = ConnApiClient(pConnApi, iIndex);
        PcLinkDestroy(pClient->pGameLinkRef);
        pClient->pGameLinkRef = NULL;
    }
    DirtyMemFree(pConnApi, KI_MEM_MODULE_CONN, pConnApi->iMemGroup);
}

extern "C" void ConnApiOnline(ConnApiRefT* pConnApi, const char* pGameName, const char* pSelfName, const DirtyAddrT* pSelfAddr)
{
    ConnApiCopyString(pConnApi->strGameName, (s32)sizeof(pConnApi->strGameName), pGameName);
    ConnApiCopyString(pConnApi->strSelfName, (s32)sizeof(pConnApi->strSelfName), pSelfName);
    if (pSelfAddr != NULL)
    {
        pConnApi->SelfAddr = *pSelfAddr;
    }
    ConnApiLog("online as %s <%s> port=%u", pConnApi->strSelfName, pConnApi->SelfAddr.strMachineAddr,
               (unsigned)pConnApi->uGamePort);
}

extern "C" s32 ConnApiHost(ConnApiRefT* pConnApi, ConnApiUserInfoT* pClientList, s32 iNumClients, s32 /*iUnused*/, s32 iSessFlags)
{
    return ConnApiStartSession(pConnApi, pClientList, iNumClients, iSessFlags, true);
}

extern "C" s32 ConnApiConnect(ConnApiRefT* pConnApi, ConnApiUserInfoT* pClientList, s32 iNumClients, s32 /*iUnused*/, s32 iSessFlags)
{
    return ConnApiStartSession(pConnApi, pClientList, iNumClients, iSessFlags, false);
}

extern "C" s32 ConnApiAddClient(ConnApiRefT* pConnApi, ConnApiUserInfoT* pUserInfo)
{
    if (pConnApi->eState == KI_STATE_IDLE)
    {
        NetPrintf(("connapi: can't add a connection to a game in idle state\n"));
        return -1;
    }
    if (pConnApi->ClientList.iNumClients == pConnApi->ClientList.iMaxClients)
    {
        NetPrintf(("connapi: can't add a connection to the game because it is full\n"));
        return -1;
    }
    const s32 iIndex = pConnApi->ClientList.iNumClients;
    ConnApiInitClient(pConnApi, ConnApiClient(pConnApi, iIndex), pUserInfo);
    pConnApi->ClientList.iNumClients = iIndex + 1;
    ConnApiApplyConnFlags(pConnApi);
    ConnApiLog("add %d) %s <%s>", iIndex, pUserInfo->strName, pUserInfo->DirtyAddr.strMachineAddr);
    return 0;
}

extern "C" s32 ConnApiRemoveClient(ConnApiRefT* pConnApi, const char* pClientName, s32 iClientIndex)
{
    s32 iIndex = iClientIndex;
    if (pClientName != NULL)
    {
        iIndex = ConnApiFindByName(pConnApi, pClientName);
    }
    if ((iIndex < 0) || (iIndex >= pConnApi->ClientList.iNumClients))
    {
        NetPrintf(("connapi: can't find client '%s' in list to remove them\n", (pClientName != NULL) ? pClientName : ""));
        return -1;
    }
    if (iIndex == pConnApi->iSelf)
    {
        NetPrintf(("connapi: can't remove self from game\n"));
        return -1;
    }
    ConnApiClient(pConnApi, iIndex)->uFlags |= KU_CLIENTFLAG_REMOVE;
    if (!pConnApi->bInCallback)
    {
        ConnApiProcessRemovals(pConnApi);
    }
    return 0;
}

extern "C" s32 ConnApiStart(ConnApiRefT* pConnApi, s32 /*iFlags*/)
{
    if ((pConnApi->eState == KI_STATE_HOSTING) || (pConnApi->eState == KI_STATE_JOINED))
    {
        return 0;
    }
    NetPrintf(("connapi: can't start game when not in hosting or joining state\n"));
    return -1;
}

extern "C" void ConnApiStop(ConnApiRefT* pConnApi)
{
    if (pConnApi->eState == KI_STATE_IDLE)
    {
        NetPrintf(("connapi: can't disconnect when in idle state\n"));
        return;
    }
    if (pConnApi->eState == KI_STATE_DISCONNECTING)
    {
        NetPrintf(("connapi: can't disconnect when already disconnecting\n"));
        return;
    }
    NetPrintf(("connapi: ending current game\n"));
}

extern "C" s32 ConnApiDisconnect(ConnApiRefT* pConnApi)
{
    if (pConnApi->eState == KI_STATE_IDLE)
    {
        NetPrintf(("connapi: can't disconnect when in idle state\n"));
        return -1;
    }
    if (pConnApi->eState == KI_STATE_DISCONNECTING)
    {
        NetPrintf(("connapi: can't disconnect when already disconnecting\n"));
        return -1;
    }
    NetPrintf(("connapi: disconnecting\n"));
    ConnApiLog("disconnecting");
    for (s32 iIndex = 0; iIndex < pConnApi->ClientList.iNumClients; ++iIndex)
    {
        if (iIndex != pConnApi->iSelf)
        {
            ConnApiCloseClient(pConnApi, iIndex, "disconnect");
        }
    }
    pConnApi->ClientList.iNumClients = 0;
    memset(&pConnApi->ClientList.Clients[0], 0, sizeof(ConnApiClientT));
    pConnApi->eState = KI_STATE_DISCONNECTING;
    pConnApi->bSessionPending = 0;
    return 0;
}

extern "C" const ConnApiClientListT* ConnApiGetClientList(ConnApiRefT* pConnApi)
{
    return &pConnApi->ClientList;
}

extern "C" s32 ConnApiControl(ConnApiRefT* pConnApi, s32 iControl, s32 iValue, s32 iValue2, const void* pValue)
{
    switch (iControl)
    {
        case KI_SEL_CBFP:
            // A null callback falls back to the logging one (a null call would fault).
            pConnApi->pCallback = (pValue != NULL)
                                  ? reinterpret_cast<ConnApiCallbackT*>(const_cast<void*>(pValue))
                                  : &ConnApiDefaultCallback;
            return 0;
        case KI_SEL_CBUP: pConnApi->pUserData = const_cast<void*>(pValue); return 0;
        case KI_SEL_PEER: pConnApi->bPeerToPeer = (u8)iValue;  return 0;
        case KI_SEL_MWID: pConnApi->iMangleWait = iValue;      return 0;
        case KI_SEL_MNGL: pConnApi->bMangle     = (u8)iValue;  return 0;
        case KI_SEL_GSV2:
            pConnApi->uGameServConnMode = (u16)iValue;
            pConnApi->uGameServFallback = (u16)iValue2;
            return 0;
        case KI_SEL_GPRT:
            // On the LAN the game connection keeps the instance's pclan port.
            if (!PcLanActive())
            {
                pConnApi->uGamePort = (u16)iValue;
            }
            return 0;
        case KI_SEL_VPRT: pConnApi->uVoipPort   = (u16)iValue; return 0;
        case KI_SEL_SLOT:
            pConnApi->iPublicSlots  = iValue;
            pConnApi->iPrivateSlots = iValue2;
            pConnApi->bSlotsSet     = 1;
            return 0;
        case KI_SEL_SFLG: pConnApi->iSessionFlags = iValue;    return 0;
        case KI_SEL_SESS:
            ConnApiCopyString(pConnApi->strSession, (s32)sizeof(pConnApi->strSession), static_cast<const char*>(pValue));
            return 0;
        case KI_SEL_SKIL:
            // The member's skill result goes to the session service; there is none.
            return 0;
        case KI_SEL_TYPE:
            NetPrintf(("connapi: connflag change from 0x%02x to 0x%02x\n", pConnApi->uConnFlags, iValue));
            pConnApi->uConnFlags = (u16)iValue;
            return 0;
        case KI_SEL_LBUF: pConnApi->iLinkBufSize = iValue;     return 0;
        case KI_SEL_TIME: pConnApi->iTimeout     = iValue;     return 0;
        case KI_SEL_TUNL:
            pConnApi->bTunnel = (u8)iValue;
            if ((iValue != 0) && (iValue2 != 0))
            {
                pConnApi->iTunnelPort = iValue2;
            }
            return 0;
        case KI_SEL_HSTI: pConnApi->iHostIndex = iValue;       return 0;
        case KI_SEL_GSRV:
            pConnApi->uGameServerAddr = (u32)iValue;
            pConnApi->iGameServerMode = iValue2;
            return 0;
        case KI_SEL_MIGR:
            // Host migration: this instance now hosts (or no longer hosts) the session.
            pConnApi->bHosting = (u8)iValue;
            return 0;
        case KI_SEL_RCBK: pConnApi->bRemoveCallbacks = (u8)iValue; return 0;
        case KI_SEL_MINP: pConnApi->iMinPorts = iValue;        return 0;
        case KI_SEL_MOUT: pConnApi->iMaxPorts = iValue;        return 0;
        case KI_SEL_NONC:
            if (pValue != NULL)
            {
                memcpy(pConnApi->aNonce, pValue, sizeof(pConnApi->aNonce));
            }
            return 0;
        default:
            return -1;
    }
}

extern "C" s32 ConnApiStatus(ConnApiRefT* pConnApi, s32 iSelect, void* pBuf, s32 iBufSize)
{
    switch (iSelect)
    {
        case KI_SEL_CBFP:
        case KI_SEL_CBUP:
            if ((pBuf != NULL) && (iBufSize >= (s32)sizeof(void*)))
            {
                void* pCurrent = (iSelect == KI_SEL_CBFP)
                                 ? reinterpret_cast<void*>(pConnApi->pCallback) : pConnApi->pUserData;
                memcpy(pBuf, &pCurrent, sizeof(pCurrent));
                return 0;
            }
            return -1;
        case KI_SEL_GPRT: return pConnApi->uGamePort;
        case KI_SEL_VPRT: return pConnApi->uVoipPort;
        case KI_SEL_SELF: return pConnApi->iSelf;
        case KI_SEL_HOST:
            if ((pBuf != NULL) && (pConnApi->ClientList.iNumClients > 0) &&
                (pConnApi->iHostIndex >= 0) && (pConnApi->iHostIndex < pConnApi->ClientList.iNumClients))
            {
                ConnApiCopyString(static_cast<char*>(pBuf), iBufSize,
                                  ConnApiClient(pConnApi, pConnApi->iHostIndex)->UserInfo.strName);
                return (pConnApi->eState == KI_STATE_HOSTING) ? 1 : 0;
            }
            return -1;
        case KI_SEL_INGM: return (pConnApi->eState != KI_STATE_IDLE) ? 1 : 0;
        case KI_SEL_IDLE: return (pConnApi->eState == KI_STATE_IDLE) ? 1 : 0;
        case KI_SEL_FAIL: return 0;
        case KI_SEL_LBUF: return pConnApi->iLinkBufSize;
        case KI_SEL_MWID: return pConnApi->iMangleWait;
        case KI_SEL_MINP: return pConnApi->iMinPorts;
        case KI_SEL_MOUT: return pConnApi->iMaxPorts;
        case KI_SEL_PEER: return pConnApi->bPeerToPeer;
        case KI_SEL_TYPE: return pConnApi->uConnFlags;
        case KI_SEL_TUNL: return pConnApi->bTunnel;
        case KI_SEL_SFLG: return pConnApi->iSessionFlags;
        case KI_SEL_SESS:
            // The session string, empty until a game session exists.
            ConnApiCopyString(static_cast<char*>(pBuf), iBufSize, pConnApi->strSession);
            return 0;
        case KI_SEL_GSRV:
            // No game server on this platform.
            return -1;
        default:
            return -1;
    }
}

extern "C" void ConnApiUpdate(ConnApiRefT* pConnApi)
{
    if (pConnApi->eState == KI_STATE_DISCONNECTING)
    {
        // The teardown is reported on the update after ConnApiDisconnect.
        ConnApiNotify(pConnApi, 0, KI_CBTYPE_SESSION, CONNAPI_STATUS_ACTV, CONNAPI_STATUS_DISC);
        pConnApi->eState = KI_STATE_IDLE;
    }

    if ((pConnApi->eState == KI_STATE_HOSTING) || (pConnApi->eState == KI_STATE_JOINED))
    {
        // The session exists from the update after ConnApiHost / ConnApiConnect; only the host
        // reports it.
        if (pConnApi->bSessionPending)
        {
            pConnApi->bSessionPending = 0;
            if (pConnApi->bHosting)
            {
                ConnApiHostSessionText(pConnApi);
                ConnApiNotify(pConnApi, 0, KI_CBTYPE_SESSION, CONNAPI_STATUS_INIT, CONNAPI_STATUS_ACTV);
            }
        }
        ConnApiApplyConnFlags(pConnApi);
        const u32 uNow = PcLinkTick();
        for (s32 iIndex = 0; iIndex < pConnApi->ClientList.iNumClients; ++iIndex)
        {
            ConnApiUpdateGame(pConnApi, iIndex, uNow);
        }
    }

    ConnApiProcessRemovals(pConnApi);
}
