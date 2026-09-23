// ============================================================================
// lobbyapipc.cpp -- the lobby connection (LobbyApi*) for the PC build.
//
// [PC platform layer] Not console code. The console's LobbyApi talked to the online lobby
// server; the PC has none. This module keeps the console's local behaviour -- the callback
// table (ids are index | 0x1000), the status / info selects, the display lists, the request
// ids -- and answers requests itself:
//   offline (no BP_LAN): no connection. Requests fail (-1), no event is ever posted, the
//                        'self' record stays zero and 'stat' stays 'offl'.
//   LAN (PcLanActive()): the connect / login requests of LobbyLogin succeed locally with the
//                        PC identity (CgsPcNetIdentity.h); game requests go to the LAN lobby
//                        authority (lan/pclan_lobby.cpp); the 'news' config request is
//                        answered with code 'new0'+index and its own request record
//                        ("NAME=<index>": not empty, so the download succeeds, and without
//                        config keys, so every config value keeps its default); a
//                        request for a record only the stats server holds gets the
//                        'time' code (see KAI_SERVER_ONLY_QUERIES); every other
//                        request succeeds with an empty record.
// Every response and event is delivered from LobbyApiUpdate, on the game thread.
// ============================================================================

#include "lobbyapi.h"
#include "lobbydisplist.h"
#include "lobbytagfield.h"
#include "dirtymem.h"
#include "dirtyaddr.h"
#include "dirtylib.h"

#include "lan/pclan.h"
#include "lan/pclan_lobby.h"

#include "GameShared/GameClasses/System/PC/CgsPcNetIdentity.h"

#include <windows.h>   // GetTickCount

#include <cstring>

namespace
{
    // fourcc helper
    constexpr s32 FourCC(char a, char b, char c, char d)
    {
        return (s32)(((u32)(u8)a << 24) | ((u32)(u8)b << 16) | ((u32)(u8)c << 8) | (u32)(u8)d);
    }

    const s32 KI_MEMID_REF    = FourCC('l', 'a', 'p', 'i');
    const s32 KI_MEMID_RECORD = FourCC('l', 'l', 's', 't');   // list records (console tag)
    const s32 KI_MEMID_TEXT   = FourCC('l', 'm', 's', 'g');

    // lobby states ('stat')
    const s32 KI_STATE_OFFLINE = FourCC('o', 'f', 'f', 'l');
    const s32 KI_STATE_ACCOUNT = FourCC('a', 'c', 'c', 't');   // connected, not logged in
    const s32 KI_STATE_IDLE    = FourCC('i', 'd', 'l', 'e');   // logged in

    // requests answered here
    const s32 KI_KIND_AUTH    = FourCC('a', 'u', 't', 'h');
    const s32 KI_KIND_NEWS    = FourCC('n', 'e', 'w', 's');
    const s32 KI_CODE_NEWS    = FourCC('n', 'e', 'w', '0');   // + the requested index
    const s32 KI_CODE_TIMEOUT = FourCC('t', 'i', 'm', 'e');

    // Queries whose only successful answer is a record the stats server keeps: 'rrlc' (the local
    // player's road-rules scores; its reply always opens with the two id fields). A LAN has no
    // stats server, so these get what the console's LobbyApi posts when the server never
    // answers, the 'time' code, at once instead of after the request timeout. An empty record
    // is a valid answer to the other stats requests (an upload ack, an empty list) and they keep
    // the default.
    const s32 KAI_SERVER_ONLY_QUERIES[] =
    {
        FourCC('r', 'r', 'l', 'c'),
    };

    bool _IsServerOnlyQuery(s32 iKind)
    {
        for (s32 iQuery = 0; iQuery < (s32)(sizeof(KAI_SERVER_ONLY_QUERIES) / sizeof(KAI_SERVER_ONLY_QUERIES[0])); ++iQuery)
        {
            if (KAI_SERVER_ONLY_QUERIES[iQuery] == iKind)
            {
                return true;
            }
        }
        return false;
    }

    // events
    const s32 KI_EVENT_CONN = FourCC('c', 'o', 'n', 'n');
    const s32 KI_EVENT_DISC = FourCC('d', 'i', 's', 'c');
    const s32 KI_EVENT_IDLE = FourCC('i', 'd', 'l', 'e');

    const s32 KI_NUM_CALLBACKS   = 32;
    const s32 KI_CALLBACK_ID_BIT = 0x1000;
    const s32 KI_NUM_REQUESTS    = 32;
    const s32 KI_NUM_DELIVERIES  = 64;
    const s32 KI_NUM_LISTS       = LOBBYAPI_LIST_MAX + 1;
    const s32 KI_LIST_RECORDS    = 32;
    const u32 KU_DEFAULT_TIMEOUT = 30000;   // ms, until 'rtim' says otherwise

    struct CallbackT
    {
        s32                iType;       // LobbyApiCBTypeE, -1 == free
        LobbyApiCallbackT* pCallback;
        void*              pUserData;
    };

    struct RequestT
    {
        s32                iId;         // 0 == free
        s32                iKind;
        LobbyApiCallbackT* pCallback;
        void*              pUserData;
        u32                uStart;      // GetTickCount at issue
    };

    // A response or event waiting for LobbyApiUpdate.
    struct DeliveryT
    {
        s32                iType;       // LOBBYAPI_CBTYPE_RESP: to pCallback only
        s32                iId;
        s32                iKind;
        s32                iCode;
        LobbyApiCallbackT* pCallback;
        void*              pUserData;
        char*              pText;       // DirtyMemAlloc'd, never null once queued
    };

    struct ListT
    {
        DispListRef*       pDisp;
        LobbyApiCallbackT* pCallback;
        void*              pUserData;
        s32                iCount;
        DispListT*         aItems[KI_LIST_RECORDS];
    };
}

struct LobbyApiRefT
{
    s32              iMemGroup;
    s32              iState;           // 'stat'
    u32              bSuspended;       // 'susp'
    u32              uTimeout;         // 'rtim'
    s32              iDebugFlags;      // 'dbgj'
    s32              iRequestSeqn;
    u32              bLanBound;        // attached to the LAN lobby authority
    char             strClient[512];   // the LobbyApiCreate tagfield ('slus' 'vers')
    char             strPartition[64]; // 'part'
    char             strAcctName[20];  // 'acct' 'user'
    char             strAuthResult[4]; // 'autd'
    char             strConfig[4];     // 'conf': no downloaded config on PC
    char             strPersResult[4]; // 'perd'
    LobbyApiUserT    User;             // 'self'
    LobbyApiUserSetT UserSet;
    CallbackT        aCallbacks[KI_NUM_CALLBACKS];
    RequestT         aRequests[KI_NUM_REQUESTS];
    DeliveryT        aDeliveries[KI_NUM_DELIVERIES];
    s32              iDeliveryHead;
    s32              iDeliveryCount;
    ListT            aLists[KI_NUM_LISTS];
};

// ---- internal helpers --------------------------------------------------------------------

// Attach to the LAN lobby authority once the transport is up (it may come up after the
// lobby ref was created).
static bool _LobbyApiLanBound(LobbyApiRefT* pLobbyApi)
{
    if (!PcLanActive())
    {
        return false;
    }
    if (!pLobbyApi->bLanBound)
    {
        PcLanLobbyAttach(pLobbyApi);
        pLobbyApi->bLanBound = 1;
    }
    return true;
}

static char* _LobbyApiDupText(LobbyApiRefT* pLobbyApi, const char* pText)
{
    if (pText == NULL)
    {
        pText = "";
    }
    const s32 iLen = (s32)strlen(pText);
    char* pCopy = (char*)DirtyMemAlloc(iLen + 1, KI_MEMID_TEXT, pLobbyApi->iMemGroup);
    if (pCopy != NULL)
    {
        memcpy(pCopy, pText, iLen + 1);
    }
    return pCopy;
}

static void _LobbyApiQueue(LobbyApiRefT* pLobbyApi, s32 iType, s32 iId, s32 iKind, s32 iCode,
                           LobbyApiCallbackT* pCallback, void* pUserData, const char* pText)
{
    if (pLobbyApi->iDeliveryCount >= KI_NUM_DELIVERIES)
    {
        NetPrintf(("lobbyapipc: delivery queue full; '%c%c%c%c' dropped\n",
                   (iKind >> 24) & 0xFF, (iKind >> 16) & 0xFF, (iKind >> 8) & 0xFF, iKind & 0xFF));
        return;
    }
    char* pCopy = _LobbyApiDupText(pLobbyApi, pText);
    if (pCopy == NULL)
    {
        return;
    }
    DeliveryT* pDelivery =
        &pLobbyApi->aDeliveries[(pLobbyApi->iDeliveryHead + pLobbyApi->iDeliveryCount) % KI_NUM_DELIVERIES];
    pDelivery->iType     = iType;
    pDelivery->iId       = iId;
    pDelivery->iKind     = iKind;
    pDelivery->iCode     = iCode;
    pDelivery->pCallback = pCallback;
    pDelivery->pUserData = pUserData;
    pDelivery->pText     = pCopy;
    pLobbyApi->iDeliveryCount += 1;
}

static void _LobbyApiDeliver(LobbyApiRefT* pLobbyApi, DeliveryT* pDelivery)
{
    LobbyApiMsgT Msg;
    Msg.id    = pDelivery->iId;
    Msg.type  = pDelivery->iType;
    Msg.kind  = pDelivery->iKind;
    Msg.code  = pDelivery->iCode;
    Msg.pData = pDelivery->pText;
    Msg.pMisc = NULL;

    if (pDelivery->iType == LOBBYAPI_CBTYPE_RESP)
    {
        if (pDelivery->pCallback != NULL)
        {
            pDelivery->pCallback(pLobbyApi, &Msg, pDelivery->pUserData);
        }
        return;
    }

    for (s32 iCallback = 0; iCallback < KI_NUM_CALLBACKS; ++iCallback)
    {
        CallbackT* pEntry = &pLobbyApi->aCallbacks[iCallback];
        if ((pEntry->iType == pDelivery->iType) && (pEntry->pCallback != NULL))
        {
            Msg.id = iCallback | KI_CALLBACK_ID_BIT;
            pEntry->pCallback(pLobbyApi, &Msg, pEntry->pUserData);
        }
    }
}

static RequestT* _LobbyApiFindRequest(LobbyApiRefT* pLobbyApi, s32 iId)
{
    if (iId <= 0)
    {
        return NULL;
    }
    for (s32 iRequest = 0; iRequest < KI_NUM_REQUESTS; ++iRequest)
    {
        if (pLobbyApi->aRequests[iRequest].iId == iId)
        {
            return &pLobbyApi->aRequests[iRequest];
        }
    }
    return NULL;
}

static void _LobbyApiListFlush(LobbyApiRefT* pLobbyApi, ListT* pList)
{
    for (s32 iItem = 0; iItem < pList->iCount; ++iItem)
    {
        void* pRecord = DispListDel(pList->pDisp, pList->aItems[iItem]);
        if (pRecord != NULL)
        {
            DirtyMemFree(pRecord, KI_MEMID_RECORD, pLobbyApi->iMemGroup);
        }
        pList->aItems[iItem] = NULL;
    }
    pList->iCount = 0;
}

static void _LobbyApiListDestroyAll(LobbyApiRefT* pLobbyApi)
{
    for (s32 iList = 0; iList < KI_NUM_LISTS; ++iList)
    {
        ListT* pList = &pLobbyApi->aLists[iList];
        if (pList->pDisp != NULL)
        {
            _LobbyApiListFlush(pLobbyApi, pList);
            DispListDestroy(pList->pDisp);
        }
        memset(pList, 0, sizeof(*pList));
    }
}

// The PC login: the local user record from the PC identity.
static void _LobbyApiLogin(LobbyApiRefT* pLobbyApi)
{
    const PcLanPeerT Self = PcLanSelf();
    const u64 uXuid = CgsPcNetIdentityXuid();

    memset(&pLobbyApi->User, 0, sizeof(pLobbyApi->User));
    pLobbyApi->User.ident = (s32)CgsPcNetIdentityLobbyIdent();
    strncpy(pLobbyApi->User.name, CgsPcNetIdentityName(), sizeof(pLobbyApi->User.name) - 1);
    pLobbyApi->User.addr       = Self.uAddr;
    pLobbyApi->User.uLocalAddr = Self.uAddr;
    DirtyAddrFromHostAddr(&pLobbyApi->User.MachineAddr, &uXuid);
    strncpy(pLobbyApi->strAcctName, pLobbyApi->User.name, sizeof(pLobbyApi->strAcctName) - 1);
    pLobbyApi->iState = KI_STATE_IDLE;
}

// ---- services for the LAN lobby authority and the login module ---------------------------

void LobbyApiPcComplete(LobbyApiRefT* pLobbyApi, s32 iId, s32 iCode, const char* pData)
{
    RequestT* pRequest = _LobbyApiFindRequest(pLobbyApi, iId);
    if (pRequest == NULL)
    {
        return;
    }
    _LobbyApiQueue(pLobbyApi, LOBBYAPI_CBTYPE_RESP, iId, pRequest->iKind, iCode,
                   pRequest->pCallback, pRequest->pUserData, pData);
    memset(pRequest, 0, sizeof(*pRequest));
}

void LobbyApiPcPostEvent(LobbyApiRefT* pLobbyApi, s32 iType, s32 iKind, s32 iCode, const char* pData)
{
    _LobbyApiQueue(pLobbyApi, iType, 0, iKind, iCode, NULL, NULL, pData);
}

LobbyApiUserT* LobbyApiPcSelf(LobbyApiRefT* pLobbyApi)
{
    return &pLobbyApi->User;
}

void LobbyApiPcListAdd(LobbyApiRefT* pLobbyApi, s32 iListType, const LobbyApiPlayT* pPlay)
{
    if ((iListType < 0) || (iListType >= KI_NUM_LISTS))
    {
        return;
    }
    ListT* pList = &pLobbyApi->aLists[iListType];
    if ((pList->pDisp == NULL) || (pList->iCount >= KI_LIST_RECORDS))
    {
        return;
    }
    LobbyApiPlayT* pRecord =
        (LobbyApiPlayT*)DirtyMemAlloc(sizeof(LobbyApiPlayT), KI_MEMID_RECORD, pLobbyApi->iMemGroup);
    if (pRecord == NULL)
    {
        return;
    }
    memcpy(pRecord, pPlay, sizeof(*pRecord));
    DispListT* pItem = DispListAdd(pList->pDisp, pRecord);
    if (pItem == NULL)
    {
        DirtyMemFree(pRecord, KI_MEMID_RECORD, pLobbyApi->iMemGroup);
        return;
    }
    pList->aItems[pList->iCount++] = pItem;
    DispListChange(pList->pDisp, 1);
}

s32 LobbyApiPcConnect(LobbyApiRefT* pLobbyApi)
{
    if (!PcLanActive())
    {
        return -1;
    }
    pLobbyApi->iState = KI_STATE_ACCOUNT;
    LobbyApiPcPostEvent(pLobbyApi, LOBBYAPI_CBTYPE_CONN, KI_EVENT_CONN, 0, "");
    return 0;
}

// ---- public API --------------------------------------------------------------------------

extern "C" LobbyApiRefT* LobbyApiCreate(const char* pTagField, void* /*pDebugRef*/,
                                        void (* /*pDebugFnc*/)(void*, const char*), s32 /*iRefMemGroup*/)
{
    const s32 iMemGroup = DirtyMemGroupQuery();
    LobbyApiRefT* pLobbyApi = (LobbyApiRefT*)DirtyMemAlloc(sizeof(LobbyApiRefT), KI_MEMID_REF, iMemGroup);
    if (pLobbyApi == NULL)
    {
        return NULL;
    }
    memset(pLobbyApi, 0, sizeof(*pLobbyApi));
    pLobbyApi->iMemGroup = iMemGroup;
    pLobbyApi->iState    = KI_STATE_OFFLINE;
    pLobbyApi->uTimeout  = KU_DEFAULT_TIMEOUT;
    for (s32 iCallback = 0; iCallback < KI_NUM_CALLBACKS; ++iCallback)
    {
        pLobbyApi->aCallbacks[iCallback].iType = -1;
    }
    if (pTagField != NULL)
    {
        TagFieldDupl(pLobbyApi->strClient, sizeof(pLobbyApi->strClient), pTagField);
    }
    _LobbyApiLanBound(pLobbyApi);
    return pLobbyApi;
}

extern "C" void LobbyApiDestroy(LobbyApiRefT* pLobbyApi)
{
    if (pLobbyApi == NULL)
    {
        return;
    }
    if (pLobbyApi->bLanBound)
    {
        PcLanLobbyDetach(pLobbyApi);
    }
    while (pLobbyApi->iDeliveryCount > 0)
    {
        DeliveryT* pDelivery = &pLobbyApi->aDeliveries[pLobbyApi->iDeliveryHead];
        DirtyMemFree(pDelivery->pText, KI_MEMID_TEXT, pLobbyApi->iMemGroup);
        pLobbyApi->iDeliveryHead = (pLobbyApi->iDeliveryHead + 1) % KI_NUM_DELIVERIES;
        pLobbyApi->iDeliveryCount -= 1;
    }
    _LobbyApiListDestroyAll(pLobbyApi);
    DirtyMemFree(pLobbyApi, KI_MEMID_REF, pLobbyApi->iMemGroup);
}

extern "C" s32 LobbyApiControl(LobbyApiRefT* pLobbyApi, s32 iKind, s32 iValue)
{
    s32 iResult = 0;
    if (iKind == FourCC('d', 'b', 'g', 'j'))
    {
        pLobbyApi->iDebugFlags = iValue;
    }
    if (iKind == FourCC('r', 't', 'i', 'm'))
    {
        iResult = (s32)pLobbyApi->uTimeout;
        pLobbyApi->uTimeout = (u32)iValue;
    }
    return iResult;
}

extern "C" void LobbyApiUpdate(LobbyApiRefT* pLobbyApi)
{
    if (pLobbyApi == NULL)
    {
        return;
    }

    if (_LobbyApiLanBound(pLobbyApi))
    {
        PcLanLobbyUpdate(pLobbyApi);

        // a request nobody answered within the request timeout fails with 'time'
        const u32 uNow = GetTickCount();
        for (s32 iRequest = 0; iRequest < KI_NUM_REQUESTS; ++iRequest)
        {
            RequestT* pRequest = &pLobbyApi->aRequests[iRequest];
            if ((pRequest->iId > 0) && ((uNow - pRequest->uStart) > pLobbyApi->uTimeout))
            {
                PcLanLobbyCancel(pLobbyApi, pRequest->iId);
                LobbyApiPcComplete(pLobbyApi, pRequest->iId, KI_CODE_TIMEOUT, "");
            }
        }
    }

    // deliver what is queued now; anything a callback queues waits for the next update
    s32 iCount = pLobbyApi->iDeliveryCount;
    while ((iCount-- > 0) && (pLobbyApi->iDeliveryCount > 0))
    {
        DeliveryT Delivery = pLobbyApi->aDeliveries[pLobbyApi->iDeliveryHead];
        pLobbyApi->iDeliveryHead = (pLobbyApi->iDeliveryHead + 1) % KI_NUM_DELIVERIES;
        pLobbyApi->iDeliveryCount -= 1;
        _LobbyApiDeliver(pLobbyApi, &Delivery);
        DirtyMemFree(Delivery.pText, KI_MEMID_TEXT, pLobbyApi->iMemGroup);
    }

    // idle callbacks run on every update
    LobbyApiMsgT Msg;
    memset(&Msg, 0, sizeof(Msg));
    Msg.type  = LOBBYAPI_CBTYPE_IDLE;
    Msg.kind  = KI_EVENT_IDLE;
    Msg.pData = "";
    for (s32 iCallback = 0; iCallback < KI_NUM_CALLBACKS; ++iCallback)
    {
        CallbackT* pEntry = &pLobbyApi->aCallbacks[iCallback];
        if ((pEntry->iType == LOBBYAPI_CBTYPE_IDLE) && (pEntry->pCallback != NULL))
        {
            Msg.id = iCallback | KI_CALLBACK_ID_BIT;
            pEntry->pCallback(pLobbyApi, &Msg, pEntry->pUserData);
        }
    }
}

extern "C" void LobbyApiSuspend(LobbyApiRefT* pLobbyApi)
{
    if (pLobbyApi->bSuspended == 1)
    {
        NetPrintf(("lobbyapi: trying to suspend when already suspended\n"));
        return;
    }
    _LobbyApiListDestroyAll(pLobbyApi);
    pLobbyApi->bSuspended = 1;
}

extern "C" void LobbyApiResume(LobbyApiRefT* pLobbyApi)
{
    if (pLobbyApi->bSuspended == 0)
    {
        NetPrintf(("lobbyapi: trying to resume when not suspended\n"));
        return;
    }
    pLobbyApi->bSuspended = 0;
}

extern "C" void LobbyApiDisconnect(LobbyApiRefT* pLobbyApi, s32 iCode)
{
    if (!PcLanActive() || (pLobbyApi->iState == KI_STATE_OFFLINE))
    {
        return;
    }
    PcLanLobbyLeave(pLobbyApi);
    memset(&pLobbyApi->User, 0, sizeof(pLobbyApi->User));
    pLobbyApi->iState = KI_STATE_OFFLINE;
    LobbyApiPcPostEvent(pLobbyApi, LOBBYAPI_CBTYPE_CONN, KI_EVENT_DISC, iCode, "");
}

extern "C" s32 LobbyApiSetCallback(LobbyApiRefT* pLobbyApi, s32 iType, LobbyApiCallbackT* pCallback,
                                   void* pUserData)
{
    s32 iCallback;
    for (iCallback = 0; pLobbyApi->aCallbacks[iCallback].iType != -1; )
    {
        if (++iCallback >= KI_NUM_CALLBACKS)
        {
            return -1;
        }
    }
    pLobbyApi->aCallbacks[iCallback].iType     = iType;
    pLobbyApi->aCallbacks[iCallback].pCallback = pCallback;
    pLobbyApi->aCallbacks[iCallback].pUserData = pUserData;
    return iCallback | KI_CALLBACK_ID_BIT;
}

extern "C" s32 LobbyApiClearCallback(LobbyApiRefT* pLobbyApi, s32 iCallbackID)
{
    const u32 uIndex = (u32)iCallbackID & ~(u32)KI_CALLBACK_ID_BIT;
    if (uIndex >= (u32)KI_NUM_CALLBACKS)
    {
        return 0;
    }
    if (pLobbyApi->aCallbacks[uIndex].iType == -1)
    {
        return 0;
    }
    pLobbyApi->aCallbacks[uIndex].iType = -1;
    return 1;
}

extern "C" s32 LobbyApiRequestCB(LobbyApiRefT* pLobbyApi, s32 iKind, const char* pRequest,
                                 LobbyApiCallbackT* pCallback, void* pUserData)
{
    if (!_LobbyApiLanBound(pLobbyApi))
    {
        return -1;
    }

    RequestT* pSlot = NULL;
    for (s32 iRequest = 0; iRequest < KI_NUM_REQUESTS; ++iRequest)
    {
        if (pLobbyApi->aRequests[iRequest].iId == 0)
        {
            pSlot = &pLobbyApi->aRequests[iRequest];
            break;
        }
    }
    if (pSlot == NULL)
    {
        NetPrintf(("lobbyapi: request/response queue overflow\n"));
        return -1;
    }

    // ids run 1..4095
    pLobbyApi->iRequestSeqn = (pLobbyApi->iRequestSeqn % 4095) + 1;
    const s32 iId = pLobbyApi->iRequestSeqn;
    pSlot->iId       = iId;
    pSlot->iKind     = iKind;
    pSlot->pCallback = pCallback;
    pSlot->pUserData = pUserData;
    pSlot->uStart    = GetTickCount();

    if (pRequest == NULL)
    {
        pRequest = "";
    }

    if (iKind == KI_KIND_AUTH)
    {
        _LobbyApiLogin(pLobbyApi);
        LobbyApiPcComplete(pLobbyApi, iId, 0, "");
    }
    else if (iKind == KI_KIND_NEWS)
    {
        const s32 iIndex = TagFieldGetNumber(TagFieldFind(pRequest, "NAME"), 0);
        LobbyApiPcComplete(pLobbyApi, iId, KI_CODE_NEWS + iIndex, pRequest);
    }
    else if (_IsServerOnlyQuery(iKind))
    {
        LobbyApiPcComplete(pLobbyApi, iId, KI_CODE_TIMEOUT, "");
    }
    else if (!PcLanLobbyRequest(pLobbyApi, iId, iKind, pRequest))
    {
        LobbyApiPcComplete(pLobbyApi, iId, 0, "");
    }
    return iId;
}

extern "C" s32 LobbyApiCancelCB(LobbyApiRefT* pLobbyApi, s32 iId)
{
    s32 iResult = 0;
    RequestT* pRequest = _LobbyApiFindRequest(pLobbyApi, iId);
    if (pRequest != NULL)
    {
        if (PcLanActive())
        {
            PcLanLobbyCancel(pLobbyApi, iId);
        }
        memset(pRequest, 0, sizeof(*pRequest));
        iResult = 1;
    }
    // a response already queued is not delivered
    for (s32 iDelivery = 0; iDelivery < pLobbyApi->iDeliveryCount; ++iDelivery)
    {
        DeliveryT* pDelivery = &pLobbyApi->aDeliveries[(pLobbyApi->iDeliveryHead + iDelivery) % KI_NUM_DELIVERIES];
        if ((pDelivery->iType == LOBBYAPI_CBTYPE_RESP) && (pDelivery->iId == iId) && (iId > 0))
        {
            pDelivery->pCallback = NULL;
            iResult = 1;
        }
    }
    return iResult;
}

extern "C" s32 LobbyApiStatus(LobbyApiRefT* pLobbyApi, s32 iSelect, void* pBuf, s32 iBufLen)
{
    // selects answered with a value
    if (iSelect == FourCC('l', 'o', 'g', 'n'))
    {
        return pLobbyApi->User.name[0] != 0;
    }
    if (iSelect == FourCC('s', 't', 'a', 't'))
    {
        return pLobbyApi->iState;
    }
    if (iSelect == FourCC('s', 'u', 's', 'p'))
    {
        return (s32)pLobbyApi->bSuspended;
    }
    if ((iSelect == FourCC('e', 'x', 't', 'n')) || (iSelect == FourCC('h', 'o', 's', 't')) ||
        (iSelect == FourCC('i', 'n', 'a', 't')) || (iSelect == FourCC('l', 'o', 'c', 'l')) ||
        (iSelect == FourCC('m', 'o', 'r', 'e')) || (iSelect == FourCC('r', 'f', 'l', 'g')) ||
        (iSelect == FourCC('s', 'e', 'c', 'u')) || (iSelect == FourCC('s', 'l', 'o', 't')) ||
        (iSelect == FourCC('s', 't', 'i', 'm')))
    {
        return 0;
    }

    // selects answered into the caller's buffer
    if ((pBuf != NULL) && (iBufLen > 0))
    {
        const char* pString = NULL;
        if ((iSelect == FourCC('a', 'c', 'c', 't')) || (iSelect == FourCC('u', 's', 'e', 'r')))
        {
            pString = pLobbyApi->strAcctName;
        }
        if (iSelect == FourCC('p', 'a', 'r', 't'))
        {
            pString = pLobbyApi->strPartition;
        }
        if (iSelect == FourCC('p', 'e', 'r', 's'))
        {
            pString = pLobbyApi->User.name;
        }
        if (pString != NULL)
        {
            strncpy((char*)pBuf, pString, (size_t)iBufLen);
            ((char*)pBuf)[iBufLen - 1] = 0;
            return 0;
        }
        if (iSelect == FourCC('s', 'e', 'l', 'f'))
        {
            if (iBufLen == (s32)sizeof(LobbyApiUserT))
            {
                memcpy(pBuf, &pLobbyApi->User, sizeof(LobbyApiUserT));
                return 0;
            }
            return -1;
        }
        if (iSelect == FourCC('s', 'l', 'u', 's'))
        {
            TagFieldGetString(TagFieldFind(pLobbyApi->strClient, "SLUS"), (char*)pBuf, iBufLen, "");
            return 0;
        }
        if (iSelect == FourCC('v', 'e', 'r', 's'))
        {
            TagFieldGetString(TagFieldFind(pLobbyApi->strClient, "VERS"), (char*)pBuf, iBufLen, "");
            return 0;
        }
    }
    return -1;
}

extern "C" const void* LobbyApiInfo(LobbyApiRefT* pLobbyApi, s32 iSelect)
{
    const void* pInfo = NULL;
    if (iSelect == FourCC('a', 'u', 't', 'd'))
    {
        pInfo = pLobbyApi->strAuthResult;
    }
    if (iSelect == FourCC('c', 'o', 'n', 'f'))
    {
        pInfo = pLobbyApi->strConfig;
    }
    if (iSelect == FourCC('p', 'e', 'r', 'd'))
    {
        pInfo = pLobbyApi->strPersResult;
    }
    return pInfo;
}

extern "C" LobbyApiUserSetT* LobbyApiGetUserSetInfoByIdent(LobbyApiRefT* pLobbyApi, s32 iUserSetIdent)
{
    if (iUserSetIdent != pLobbyApi->UserSet.iIdent)
    {
        return NULL;
    }
    return &pLobbyApi->UserSet;
}

extern "C" DispListRef* LobbyApiListAlloc(LobbyApiRefT* pLobbyApi, s32 iListType, LobbyApiCallbackT* pCallback,
                                          void* pUserData)
{
    if ((iListType < 0) || (iListType >= KI_NUM_LISTS) || (pLobbyApi->bSuspended != 0))
    {
        return NULL;
    }
    ListT* pList = &pLobbyApi->aLists[iListType];
    pList->pCallback = pCallback;
    pList->pUserData = pUserData;
    if (pList->pDisp == NULL)
    {
        DirtyMemGroupEnter(pLobbyApi->iMemGroup);
        pList->pDisp = DispListCreate(100, 100, 0);
        DirtyMemGroupLeave();
    }
    return pList->pDisp;
}

extern "C" void LobbyApiListFlush(LobbyApiRefT* pLobbyApi, s32 iListType)
{
    if (iListType == -1)
    {
        for (s32 iList = 0; iList < KI_NUM_LISTS; ++iList)
        {
            if (pLobbyApi->aLists[iList].pDisp != NULL)
            {
                _LobbyApiListFlush(pLobbyApi, &pLobbyApi->aLists[iList]);
            }
        }
        return;
    }
    if ((iListType < 0) || (iListType >= KI_NUM_LISTS) || (pLobbyApi->aLists[iListType].pDisp == NULL))
    {
        return;
    }
    _LobbyApiListFlush(pLobbyApi, &pLobbyApi->aLists[iListType]);
}

extern "C" void LobbyApiListFree(LobbyApiRefT* pLobbyApi, s32 iListType, DispListRef* pDisp)
{
    if ((iListType < 0) || (iListType >= KI_NUM_LISTS) || (pDisp == NULL))
    {
        return;
    }
    ListT* pList = &pLobbyApi->aLists[iListType];
    if (pList->pDisp == pDisp)
    {
        _LobbyApiListFlush(pLobbyApi, pList);
        DispListDestroy(pDisp);
        memset(pList, 0, sizeof(*pList));
    }
}
