#include "lobbyapi_x360_internal.h"

#include "lobbyname.h"
#include "lobbytagfield.h"
#include "dirtylib.h"
#include "dirtymem.h"
#include "platform.h"

#include <string.h>

extern "C"
{
    void HasherEnumInit(HasherRef* pHash);
    DispListT* HasherEnum(HasherRef* pHash, s32 iValue);
    HasherRef* HasherCreate(s32 iInitial, s32 iHashLength);
    void HasherDestroy(HasherRef* pHash);
    DispListT* HasherFlush(HasherRef* pHash);
    s32 TagFieldGetFlags(const char* pData, s32 iDefault);
    u32 NetTick(void* pCommRef);
    u32 CommUDPTick(void* pCommRef);
    s32 ProtoAriesStatus(ProtoAriesRefT* pProtoAries, s32 iSelect, void* pBuf, s32 iBufLen);
    ProtoAriesRefT* ProtoAriesCreate(s32 iMaxBuffer);
    void ProtoAriesDestroy(ProtoAriesRefT* pProtoAries);
    s32 ProtoAriesSend(ProtoAriesRefT* pProtoAries, s32 iKind, s32 iCode,
                       const char* pBody, s32 iBodyLen);
    s32 ProtoAriesConnect(ProtoAriesRefT* pProtoAries, const char* pServer, u32 uAddress, s32 iPort);
    s32 ProtoAriesUnconnect(ProtoAriesRefT* pProtoAries);
    void ProtoAriesSecure(ProtoAriesRefT* pProtoAries, s32 iSecure);
    void ProtoAriesSetKey(ProtoAriesRefT* pProtoAries, const void* pKey, u32 uKeySize);
    s32 ProtoAriesPeek(ProtoAriesRefT* pProtoAries, s32* pKind, s32* pCode, char** ppBody);
    s32 ProtoAriesRecv(ProtoAriesRefT* pProtoAries, s32* pKind, s32* pCode,
                       char* pBody, s32 iBufSize);
    char* ds_strnzcpy(char* pDst, const char* pSrc, s32 iSize);
    s32 ds_stricmp(const char* pString1, const char* pString2);
    DispListT* HashStrFind(HasherRef* pHash, const char* pKey);
    s32 CryptSSC2StringEncrypt(char* pDst, s32 iLen, const char* pSrc,
                               const u8* pKey, s32 iKeySize, s32 iIterations);
    s32 LobbyApiXenonSnoopRequest(LobbyApiXenonRefT* pXenon, s32 iKind,
                                  const char* pRequest, s32 iCallbackID);
    s32 LobbyApiXenonSnoopResponse(LobbyApiXenonRefT* pXenon, s32 iKind,
                                   s32 iCode, const char* pData);
    LobbyApiXenonRefT* LobbyApiXenonCreate(LobbyApiRefT* pLobbyApi);
    void LobbyApiXenonDestroy(LobbyApiXenonRefT* pXenon);
    void LobbyApiXenonDisconnect(LobbyApiXenonRefT* pXenon);
    s32 LobbyApiXenonUpdate(LobbyApiXenonRefT* pXenon);
    s32 LobbyApiListCreate(LobbyApiRefT* pLobbyApi);
    void LobbyApiListDestroy(LobbyApiRefT* pLobbyApi);
    void LobbyApiAriesRequest(LobbyApiRefT* pLobbyApi, s32 iKind,
                              const char* pRequest, s32 iCallbackID);
    void LobbyApiListUpdate(LobbyApiRefT* pLobbyApi);
    void LobbyApiAsyncProcess(LobbyApiRefT* pLobbyApi, const char* pData);
    void LobbyApiAsyncMoveRoom(LobbyApiRefT* pLobbyApi, const char* pData);
    void LobbyApiExtractUserAddr(LobbyApiUserT* pUser, const char* pData);
    s32 LobbyApiPrintfCode(LobbyApiRefT* pLobbyApi, const char* pFormat, ...);
    void PingManagerUpdate(PingManagerRefT* pPingManager);
    PingManagerRefT* PingManagerCreate(s32 iCapacity);
    void PingManagerDestroy(PingManagerRefT* pPingManager);
    s32 PingManagerPingServer(PingManagerRefT* pPingManager, u32 uAddress,
                              void (*pCallback)(void*, u32, void*), void* pUserData);
    const char* SocketInAddrGetText(u32 uAddress);
}

static void _LobbyApiResponseLog(LobbyApiRefT* pLobbyApi, s32 iKind, s32 iCode,
                                 const u8* pData, const char* pDirection);
static void _LobbyApiPingCallback(void* pPingRef, u32 uResult, void* pUserData);
static void _LobbyApiConnectCallback(LobbyApiRefT* pLobbyApi, LobbyApiMsgT* pMsg,
                                     void* pUserData);

extern "C" DispListRef* LobbyApiListGetDisp(LobbyApiRefT* pLobbyApi, s32 iListType)
{
    LobbyListRefT* pListRef = pLobbyApi->pListRef;
    if (pListRef != NULL)
    {
        return pListRef->Lists[iListType].pDisp;
    }
    return NULL;
}

extern "C" HasherRef* LobbyApiListGetHash(LobbyApiRefT* pLobbyApi, s32 iListType)
{
    LobbyListRefT* pListRef = pLobbyApi->pListRef;
    if (pListRef != NULL)
    {
        return pListRef->Lists[iListType].pHash;
    }
    return NULL;
}

extern "C" DispListT* LobbyApiListFindByName(LobbyApiRefT* pLobbyApi, s32 iListType,
                                              const char* pName)
{
    if ((u32)iListType > 10)
    {
        return NULL;
    }

    LobbyListRefT* pListRef = pLobbyApi->pListRef;
    if (pListRef == NULL)
    {
        return NULL;
    }

    HasherRef* pHash = pListRef->Lists[iListType].pHash;
    if (pHash == NULL)
    {
        return NULL;
    }

    if (iListType != 0)
    {
        return HashStrFind(pHash, pName);
    }

    HasherEnumInit(pHash);
    for (DispListT* pItem = HasherEnum(pHash, 0); pItem != NULL; pItem = HasherEnum(pHash, 0))
    {
        const LobbyApiRoomRecordPrefixT* pRoom =
            (const LobbyApiRoomRecordPrefixT*)DispListGet(pListRef->Lists[0].pDisp, pItem);
        if (ds_stricmp(pRoom->strName, pName) == 0)
        {
            return pItem;
        }
    }
    return NULL;
}

extern "C" s32 LobbyApiListCreate(LobbyApiRefT* pLobbyApi)
{
    const s32 iMemGroup = DirtyMemGroupQuery();
    LobbyListRefT* pListRef = (LobbyListRefT*)DirtyMemAlloc(0xe0, 1819046772, iMemGroup);
    pLobbyApi->pListRef = pListRef;
    if (pListRef != NULL)
    {
        memset(pListRef, 0, sizeof(*pListRef));
        pListRef->iMemGroup = iMemGroup;
    }
    return (s32)pListRef;
}

extern "C" void LobbyApiListFlush(LobbyApiRefT* pLobbyApi, s32 iListType)
{
    LobbyListRefT* pListRef = pLobbyApi->pListRef;
    if (pListRef == NULL)
    {
        return;
    }

    if (iListType == -1)
    {
        for (s32 iIndex = 0; iIndex < 11; ++iIndex)
        {
            LobbyDispListT* pList = &pListRef->Lists[iIndex];
            if (pList->pHash != NULL)
            {
                DispListT* pItem;
                while ((pItem = HasherFlush(pList->pHash)) != NULL)
                {
                    void* pRecord = DispListDel(pList->pDisp, pItem);
                    DirtyMemFree(pRecord, 1819046772, pListRef->iMemGroup);
                }
            }
        }
        return;
    }

    LobbyDispListT* pList = &pListRef->Lists[iListType];
    if (pList->pHash != NULL)
    {
        DispListT* pItem;
        while ((pItem = HasherFlush(pList->pHash)) != NULL)
        {
            void* pRecord = DispListDel(pList->pDisp, pItem);
            DirtyMemFree(pRecord, 1819046772, pListRef->iMemGroup);
        }
    }
}

extern "C" void LobbyApiListDestroy(LobbyApiRefT* pLobbyApi)
{
    LobbyListRefT* pListRef = pLobbyApi->pListRef;
    if (pListRef == NULL)
    {
        return;
    }

    for (s32 iIndex = 0; iIndex < 11; ++iIndex)
    {
        LobbyDispListT* pList = &pListRef->Lists[iIndex];
        if (pList->pHash != NULL)
        {
            LobbyApiPrintfCode(pLobbyApi,
                               "lobbyapi: list type %d not empty on shutdown, destroying\n",
                               iIndex);
            DispListT* pItem;
            while ((pItem = HasherFlush(pList->pHash)) != NULL)
            {
                void* pRecord = DispListDel(pList->pDisp, pItem);
                DirtyMemFree(pRecord, 1819046772, pListRef->iMemGroup);
            }
            HasherDestroy(pList->pHash);
            DispListDestroy(pList->pDisp);
        }
    }
    DirtyMemFree(pListRef, 1819046772, pListRef->iMemGroup);
    pLobbyApi->pListRef = NULL;
}

extern "C" DispListRef* LobbyApiListAlloc(LobbyApiRefT* pLobbyApi, s32 iListType,
                                           LobbyApiCallbackT* pCallback, void* pUserData)
{
    LobbyListRefT* pListRef = pLobbyApi->pListRef;
    if ((pListRef == NULL) || ((u32)iListType > 10))
    {
        return NULL;
    }

    LobbyDispListT* pList = &pListRef->Lists[iListType];
    pList->pCallback = pCallback;
    pList->pUserData = pUserData;
    DirtyMemGroupEnter(pListRef->iMemGroup);
    pList->pHash = HasherCreate(1000, 2000);
    pList->pDisp = DispListCreate(100, 100, 0);
    DirtyMemGroupLeave();
    DispListDataSet(pList->pDisp, pList->pHash);
    return pList->pDisp;
}

extern "C" void LobbyApiListFree(LobbyApiRefT* pLobbyApi, s32 iListType,
                                  DispListRef* pDisp)
{
    LobbyListRefT* pListRef = pLobbyApi->pListRef;
    if ((pListRef == NULL) || ((u32)iListType > 10) || (pDisp == NULL))
    {
        return;
    }

    HasherRef* pHash = (HasherRef*)DispListDataGet(pDisp);
    DispListT* pItem;
    while ((pItem = HasherFlush(pHash)) != NULL)
    {
        void* pRecord = DispListDel(pDisp, pItem);
        DirtyMemFree(pRecord, 1819046772, pListRef->iMemGroup);
    }
    HasherDestroy(pHash);
    DispListDestroy(pDisp);

    LobbyDispListT* pList = &pListRef->Lists[iListType];
    if (pList->pDisp == pDisp)
    {
        pList->pHash = NULL;
        pList->pDisp = NULL;
    }
    pList->pCallback = NULL;
    pList->pUserData = NULL;
}

extern "C" void LobbyApiListUpdate(LobbyApiRefT* pLobbyApi)
{
    LobbyListRefT* pListRef = pLobbyApi->pListRef;
    if ((pListRef == NULL) || (pLobbyApi->bEnableDispElemCb == 1))
    {
        return;
    }

    for (s32 iIndex = 0; iIndex < 11; ++iIndex)
    {
        LobbyDispListT* pList = &pListRef->Lists[iIndex];
        if ((pList->pHash != NULL) && (pList->pCallback != NULL) &&
            (DispListOrder(pList->pDisp) > 0))
        {
            LobbyApiMsgT Msg;
            Msg.id = 0;
            Msg.type = LOBBYAPI_CBTYPE_DISP;
            Msg.kind = iIndex;
            Msg.code = 0;
            Msg.pData = (const char*)pList->pDisp;
            Msg.pMisc = NULL;
            pList->pCallback(pLobbyApi, &Msg, pList->pUserData);
        }
    }
}

static bool _LobbyApiRequestQueuePop(LobbyApiRefT* pLobbyApi,
                                     LobbyCBQueueRefT* pRequest, s32 iKind)
{
    memset(pRequest, 0, sizeof(*pRequest));

    s32 iQueueCount = pLobbyApi->iQueueTail;
    if (iQueueCount > 0)
    {
        s32 iOldest = 0;
        s32 iSmallestSequence = 0x7fffffff;
        for (s32 iIndex = 0; iIndex < iQueueCount; ++iIndex)
        {
            const LobbyCBQueueRefT* pCandidate = &pLobbyApi->CallbackQueue[iIndex];
            if ((pCandidate->iSeqn != -1) && (pCandidate->iSeqn < iSmallestSequence))
            {
                iSmallestSequence = pCandidate->iSeqn;
                iOldest = iIndex;
            }
        }

        const LobbyCBQueueRefT* pCandidate = &pLobbyApi->CallbackQueue[iOldest];
        if ((pCandidate->iKind == iKind) || ((u32)iKind == 0x44515545)) // DQUE
        {
            memcpy(pRequest, pCandidate, sizeof(*pRequest));
            for (s32 iIndex = iOldest; iIndex < iQueueCount - 1; ++iIndex)
            {
                memcpy(&pLobbyApi->CallbackQueue[iIndex],
                       &pLobbyApi->CallbackQueue[iIndex + 1], sizeof(*pRequest));
            }
            memset(&pLobbyApi->CallbackQueue[iQueueCount - 1], 0, sizeof(*pRequest));
            --pLobbyApi->iQueueTail;
        }
    }
    return pLobbyApi->iQueueTail != 0;
}

static void _LobbyApiDumpLines(LobbyApiRefT* pLobbyApi, const u8* pData, s32 iLineLength)
{
    bool bDone = false;
    while (!bDone)
    {
        const u8* pEnd = pData + iLineLength;
        const u8* pSpace = NULL;
        const u8* pEqual = NULL;
        const u8* pScan = pData;
        while ((pScan < pEnd) && (*pScan != 0) && (*pScan != '\n'))
        {
            if ((*pScan == ' ') || (*pScan == '\t'))
            {
                pSpace = pScan;
            }
            if (*pScan == '=')
            {
                pEqual = pScan;
            }
            ++pScan;
        }
        if (pScan == pEnd)
        {
            if (pSpace != NULL)
            {
                pScan = pSpace;
            }
            else if (pEqual != NULL)
            {
                pScan = pEqual;
            }
        }

        const s32 iCopyCount = (s32)(pScan - pData) + 1;
        char strLine[128] = "   ";
        strncpy(strLine + 3, (const char*)pData, iCopyCount);
        const u8 uEnd = *pScan;
        if (uEnd == 0)
        {
            strLine[3 + iCopyCount - 1] = '\n';
            strLine[3 + iCopyCount] = 0;
            bDone = true;
        }
        else if ((uEnd == '\n') || (uEnd == '\r'))
        {
            strLine[3 + iCopyCount - 1] = 0;
        }
        else
        {
            strLine[3 + iCopyCount] = '\n';
            strLine[3 + iCopyCount + 1] = 0;
        }
        LobbyApiPrintfCode(pLobbyApi, "%s", strLine);
        pData = pScan + 1;
    }
}

static void _LobbyApiResponseLog(LobbyApiRefT* pLobbyApi, s32 iKind, s32 iCode,
                                 const u8* pData, const char* pDirection)
{
    if ((pLobbyApi->debugflt != NULL) && (pLobbyApi->debugflt(pLobbyApi->debugref) != 0))
    {
        return;
    }

    if (pData == NULL)
    {
        pData = (const u8*)"";
    }
    if (iCode == 0)
    {
        iCode = 0x2d2d2d2d;
    }
    if (iKind == -1)
    {
        iKind = 0x2a2a2a2a;
        if (iCode == -1)
        {
            iCode = 0x636f6e6e;
        }
        else if ((u32)iCode == 0xfefefefe)
        {
            iCode = 0x64697363;
        }
        else
        {
            iCode = 0x2d2d2d2d;
        }
    }

    s32 iLength = 0;
    bool bBinary = false;
    for (const u8* pScan = pData; *pScan != 0; ++pScan)
    {
        if ((*pScan < 0x20) && (*pScan != '\t') && (*pScan != '\n') && (*pScan != '\r'))
        {
            bBinary = true;
        }
        ++iLength;
    }

    LobbyApiPrintfCode(pLobbyApi, "%s: %c%c%c%c/%c%c%c%c (%d)\n", pDirection,
                      ((u32)iKind >> 24) & 0xff, ((u32)iKind >> 16) & 0xff,
                      ((u32)iKind >> 8) & 0xff, (u32)iKind & 0xff,
                      ((u32)iCode >> 24) & 0xff, ((u32)iCode >> 16) & 0xff,
                      ((u32)iCode >> 8) & 0xff, (u32)iCode & 0xff, iLength);
    if (iLength == 0)
    {
        LobbyApiPrintfCode(pLobbyApi, "   [null data]\n");
    }
    else if (bBinary)
    {
        LobbyApiPrintfCode(pLobbyApi, "   [binary data]\n");
    }
    else
    {
        _LobbyApiDumpLines(pLobbyApi, pData, 80);
    }
}

extern "C" void LobbyApiGlobalCallback(LobbyApiRefT* pLobbyApi, s32 iId, s32 iType,
                                       s32 iKind, s32 iCode, const char* pData)
{
    if ((u32)iType > 5)
    {
        LobbyApiPrintfCode(pLobbyApi, "lobbyapi: invalid global callback type %d\n", iType);
        return;
    }

    LobbyApiMsgT Msg;
    Msg.id = iId;
    Msg.type = iType;
    Msg.kind = iKind;
    Msg.code = iCode;
    Msg.pData = pData;
    Msg.pMisc = NULL;
    for (s32 iIndex = 0; iIndex < 32; ++iIndex)
    {
        LobbyGlobalCBRefT* pCallback = &pLobbyApi->callbacks[iIndex];
        if (pCallback->eType == iType)
        {
            pCallback->pCallback(pLobbyApi, &Msg, pCallback->pUserData);
        }
    }
}

static bool _LobbyApiDisconnect(LobbyApiRefT* pLobbyApi, s32 iCode, const char* pData)
{
    memset(&pLobbyApi->user, 0, sizeof(pLobbyApi->user));
    if (pLobbyApi->state != 0x6f66666c) // offl
    {
        pLobbyApi->state = 0x6f66666c;
        _LobbyApiResponseLog(pLobbyApi, 0x64697363, iCode, (const u8*)pData, "locl");
        LobbyApiGlobalCallback(pLobbyApi, 0, LOBBYAPI_CBTYPE_CONN,
                               0x64697363, iCode, pData);
    }

    LobbyApiXenonDisconnect(pLobbyApi->pXenon);
    ProtoAriesUnconnect(pLobbyApi->net);
    LobbyApiListFlush(pLobbyApi, -1);
    LobbyCBQueueRefT Request;
    while (_LobbyApiRequestQueuePop(pLobbyApi, &Request, 0x44515545)) // DQUE
    {
    }
    pLobbyApi->nextServerPingTime = 0;
    pLobbyApi->serverPingLatency = -1;
    pLobbyApi->nFailedServerPings = 0;
    pLobbyApi->numpingreq = 0;
    pLobbyApi->iCallbackID = 1;
    return false;
}

static void _LobbyApiBeginRedirect(LobbyApiRefT* pLobbyApi)
{
    const bool bAlternatePort = pLobbyApi->strServerName[0] == '*';
    const char* pServerName = bAlternatePort ? pLobbyApi->strServerName + 1
                                              : pLobbyApi->strServerName;
    ProtoAriesUnconnect(pLobbyApi->net);
    if (bAlternatePort &&
        (ProtoAriesStatus(pLobbyApi->net, 0x63727970, NULL, 0) == 0)) // cryp
    {
        ProtoAriesConnect(pLobbyApi->net, pServerName, 0, pLobbyApi->rdirport + 1);
        ProtoAriesSecure(pLobbyApi->net, 1);
    }
    else
    {
        ProtoAriesConnect(pLobbyApi->net, pServerName, 0, pLobbyApi->rdirport);
    }
    LobbyApiPrintfCode(pLobbyApi, "connecting to redir @ %s:%d\n",
                       pLobbyApi->strServerName, pLobbyApi->lobbyport);
    pLobbyApi->state = 0x72646972; // rdir
    const u32 uNow = CommUDPTick(pLobbyApi->net);
    pLobbyApi->uTimeout = uNow + pLobbyApi->uConnTimeout;
    pLobbyApi->uUpdated = 0;
}

static void _LobbyApiUpdatePing(LobbyApiRefT* pLobbyApi, u32 uNow)
{
    if (pLobbyApi->state == 0x6f66666c) // offl
    {
        return;
    }

    if ((pLobbyApi->uUpdated != 0) &&
        ((s32)(uNow - pLobbyApi->uUpdated) > 5000))
    {
        pLobbyApi->uTimeout = pLobbyApi->uTimeout - pLobbyApi->uUpdated + uNow;
    }
    if ((s32)(pLobbyApi->uTimeout - uNow) < 0)
    {
        _LobbyApiDisconnect(pLobbyApi, 0x74696d65, NULL); // time
    }

    if (pLobbyApi->iQueueTail != 0)
    {
        const LobbyCBQueueRefT* pFirstRequest = &pLobbyApi->CallbackQueue[0];
        const s32 iDelay = (s32)(uNow - (u32)pFirstRequest->iTick);
        if (iDelay > (s32)pLobbyApi->uTrnsTimeout)
        {
            LobbyApiPrintfCode(pLobbyApi,
                               "lobbyapi: timing out due to %d ms '%c%c%c%c' transaction delay\n",
                               iDelay,
                               ((u32)pFirstRequest->iKind >> 24) & 0xff,
                               ((u32)pFirstRequest->iKind >> 16) & 0xff,
                               ((u32)pFirstRequest->iKind >> 8) & 0xff,
                               (u32)pFirstRequest->iKind & 0xff);
            _LobbyApiDisconnect(pLobbyApi, 0x74696d65, NULL); // time
        }
    }
    pLobbyApi->uUpdated = uNow;
}

static s32 _LobbyApiConnectResponse(LobbyApiRefT* pLobbyApi, s32 iCallbackID,
                                    s32 iKind, s32 iCode, const char* pData)
{
    if ((iKind == -1) && (iCode == -1))
    {
        char strClientData[256];
        TagFieldDupl(strClientData, sizeof(strClientData), pLobbyApi->strVersion);
        ProtoAriesSend(pLobbyApi->net, 0x40746963, 0, "RC4+MD5-V2", -1);
        ProtoAriesSend(pLobbyApi->net, 0x40646972, 0, strClientData, -1);
    }

    if (iKind == 0x40746963) // @tic
    {
        ProtoAriesSetKey(pLobbyApi->net, pData, (u32)iCallbackID);
    }
    if (iKind != 0x40646972) // @dir
    {
        if (iKind != -1)
        {
            return 0;
        }
        if (iCode != -16843010) // 0xfefefefe
        {
            return 0;
        }
        if (ProtoAriesStatus(pLobbyApi->net, 0x63727970, NULL, 0) != 0) // cryp
        {
            ProtoAriesSetKey(pLobbyApi->net, NULL, 0);
            pLobbyApi->state = 0x696e6974; // init
        }
        else
        {
            _LobbyApiDisconnect(pLobbyApi, 0x72646972, NULL); // rdir
        }
        return -1;
    }

    pLobbyApi->ouraddr = ProtoAriesStatus(pLobbyApi->net, 0x6c616472, NULL, 0); // ladr
    if (TagFieldFind(pData, "DIRECT") == NULL)
    {
        pLobbyApi->lobbyaddr = TagFieldGetAddress(TagFieldFind(pData, "ADDR"), 0);
        pLobbyApi->lobbyport = TagFieldGetNumber(TagFieldFind(pData, "PORT"), 0);
    }
    pLobbyApi->sess = (u32)TagFieldGetNumber(TagFieldFind(pData, "SESS"), 0);
    TagFieldGetString(TagFieldFind(pData, "MASK"), pLobbyApi->mask,
                      sizeof(pLobbyApi->mask), "");
    ProtoAriesUnconnect(pLobbyApi->net);
    if (pLobbyApi->lobbyaddr != 0)
    {
        pLobbyApi->state = 0x636f6e6e; // conn
        if (pLobbyApi->pPingManager != NULL)
        {
            PingManagerPingServer(pLobbyApi->pPingManager, pLobbyApi->lobbyaddr,
                                  _LobbyApiPingCallback, pLobbyApi);
        }
        LobbyApiPrintfCode(pLobbyApi, "connecting to slave @ %s:%d\n",
                           SocketInAddrGetText(pLobbyApi->lobbyaddr), pLobbyApi->lobbyport);
        ProtoAriesConnect(pLobbyApi->net, NULL, pLobbyApi->lobbyaddr, pLobbyApi->lobbyport);
        return -1;
    }

    char strDownMessage[256];
    TagFieldGetString(TagFieldFind(pData, "DOWN"), strDownMessage,
                      sizeof(strDownMessage), "");
    _LobbyApiDisconnect(pLobbyApi, 0x646f776e, strDownMessage); // down
    return 0;
}

static void _LobbyApiConfigCallback(LobbyApiRefT* pLobbyApi, LobbyApiMsgT* pMsg,
                                    void* pUserData)
{
    (void)pUserData;
    if (pMsg->code != 0)
    {
        LobbyApiPrintfCode(pLobbyApi, "_LobbyApiConfigCallback: got unexpected result\n");
    }
    else
    {
        TagFieldDupl(pLobbyApi->strConfig, sizeof(pLobbyApi->strConfig), pMsg->pData);
        const char* pEnd = pMsg->pData;
        while (*pEnd != 0)
        {
            ++pEnd;
        }
        if ((u32)(pEnd - pMsg->pData) > sizeof(pLobbyApi->strConfig))
        {
            NetPrintfCode("lobbyapi: warning -- config file is too large\n");
        }
    }
    LobbyApiGlobalCallback(pLobbyApi, 0, LOBBYAPI_CBTYPE_CONN, 0x636f6e6e, 0, NULL);
}

static void _LobbyApiPingCallback(void* pPingRef, u32 uResult, void* pUserData)
{
    LobbyApiRefT* pLobbyApi = (LobbyApiRefT*)pUserData;
    if (uResult != 0)
    {
        pLobbyApi->serverPingLatency = (s32)uResult;
        pLobbyApi->nextServerPingTime = NetTick(pPingRef) + 30000;
    }
    else
    {
        LobbyApiPrintfCode(pLobbyApi, "server ICMP timeout\n");
        ++pLobbyApi->nFailedServerPings;
        if (pLobbyApi->nFailedServerPings != 3)
        {
            pLobbyApi->nextServerPingTime = NetTick(pPingRef);
            return;
        }
    }
    pLobbyApi->nFailedServerPings = 0;
}

static void _LobbyApiConnectCallback(LobbyApiRefT* pLobbyApi, LobbyApiMsgT* pMsg,
                                     void* pUserData)
{
    LobbyCBQueueRefT* pCallbackData = (LobbyCBQueueRefT*)pUserData;
    pCallbackData->pCallback(pLobbyApi, pMsg, pCallbackData->pUserData);

    const u32 uCallbackIndex = (u32)pCallbackData->iId & 0xffffefff;
    if (uCallbackIndex <= 31)
    {
        LobbyGlobalCBRefT* pCallback = &pLobbyApi->callbacks[uCallbackIndex];
        if (pCallback->eType != LOBBYAPI_CBTYPE_INVALID)
        {
            pCallback->eType = LOBBYAPI_CBTYPE_INVALID;
        }
    }
    DirtyMemFree(pCallbackData, 1818325097, pLobbyApi->iMemGroup);
}

extern "C" void LobbyApiUpdate(LobbyApiRefT* pLobbyApi)
{
    if (pLobbyApi->net == NULL)
    {
        return;
    }

    if (pLobbyApi->state == 0x696e6974) // init
    {
        _LobbyApiBeginRedirect(pLobbyApi);
    }
    if (ProtoAriesStatus(pLobbyApi->net, 0x73746174, NULL, 0) == 4) // stat
    {
        _LobbyApiDisconnect(pLobbyApi, 0x64697363, NULL); // disc
        return;
    }

    const u32 uNow = CommUDPTick(pLobbyApi->net);
    _LobbyApiUpdatePing(pLobbyApi, uNow);
    if (pLobbyApi->uServLastTick == 0)
    {
        pLobbyApi->uServLastTick = uNow;
    }
    const u32 uElapsed = uNow - pLobbyApi->uServLastTick + pLobbyApi->uServTickSum;
    pLobbyApi->uServLastTick = uNow;
    pLobbyApi->uServTickSum = uElapsed;
    pLobbyApi->uServTime += uElapsed / 1000;
    pLobbyApi->uServTickSum = uElapsed % 1000;

    if ((pLobbyApi->state != 0x7465726d) && (pLobbyApi->state != 0x6f66666c)) // term/offl
    {
        if (pLobbyApi->pPingManager != NULL)
        {
            PingManagerUpdate(pLobbyApi->pPingManager);
        }

        s32 iReadSize;
        s32 iKind;
        s32 iCode;
        char* pData;
        char strRecord[0x180];
        iReadSize = ProtoAriesPeek(pLobbyApi->net, &iKind, &iCode, &pData);
        while (iReadSize >= 0)
        {
            pLobbyApi->uTimeout = pLobbyApi->uIdleTimeout + uNow;
            if ((u32)iKind == 0x7e706e67) // ~png
            {
                if (pLobbyApi->serverPingLatency != -1)
                {
                    TagFieldDupl(strRecord, 256, pData);
                    TagFieldSetNumber(strRecord, 256, "TIME", pLobbyApi->serverPingLatency);
                    iReadSize = -1;
                    pData = strRecord;
                }
                const u32 uEpoch = TagFieldGetEpoch(TagFieldFind(pData, "REF"), 0);
                pLobbyApi->uServLastTick = 0;
                pLobbyApi->uServTime = uEpoch;
                pLobbyApi->uServTickSum = 0;
                iCode = 0;
                ProtoAriesSend(pLobbyApi->net, iKind, 0, pData, iReadSize);
            }
            else
            {
                _LobbyApiResponseLog(pLobbyApi, iKind, iCode, (const u8*)pData, "recv");
                LobbyCBQueueRefT Request;
                _LobbyApiRequestQueuePop(pLobbyApi, &Request, iKind);
                LobbyApiGlobalCallback(pLobbyApi, Request.iId, LOBBYAPI_CBTYPE_RESP,
                                       iKind, iCode, pData);

                if (!LobbyApiXenonSnoopResponse(pLobbyApi->pXenon, iKind, iCode, pData))
                {
                    if (Request.pCallback != NULL)
                    {
                        LobbyApiMsgT Msg;
                        Msg.id = Request.iId;
                        Msg.type = LOBBYAPI_CBTYPE_RESP;
                        Msg.kind = iKind;
                        Msg.code = iCode;
                        Msg.pData = pData;
                        Msg.pMisc = NULL;
                        Request.pCallback(pLobbyApi, &Msg, Request.pUserData);
                    }

                    if (pLobbyApi->state == 0x72646972) // rdir
                    {
                        iKind = _LobbyApiConnectResponse(pLobbyApi, iReadSize,
                                                         iKind, iCode, pData);
                        if (iKind < 0)
                        {
                            return;
                        }
                    }

                    if ((iKind == -1) && (iCode == -1))
                    {
                        const s32 iPort = ProtoAriesStatus(pLobbyApi->net, 0x6c707274,
                                                          NULL, 0); // lprt
                        strRecord[0] = 0;
                        TagFieldSetAddress(strRecord, 256, "ADDR", pLobbyApi->ouraddr);
                        TagFieldSetNumber(strRecord, 256, "PORT", iPort);
                        ProtoAriesSend(pLobbyApi->net, 0x61646472, 0, strRecord, -1); // addr
                        strRecord[0] = 0;
                        TagFieldSetBinary(strRecord, 256, "SKEY", "Public Key", 10);
                        ProtoAriesSend(pLobbyApi->net, 0x736b6579, 0, strRecord, -1); // skey

                        pLobbyApi->rankmore = 1;
                        pLobbyApi->state = 0x736b6579; // skey
                        iKind = 0x636f6e6e; // conn
                        iCode = 0;
                        if (pLobbyApi->iQueueTail < 32)
                        {
                            const s32 iQueueIndex = pLobbyApi->iQueueTail;
                            LobbyCBQueueRefT* pQueueEntry = &pLobbyApi->CallbackQueue[iQueueIndex];
                            ++pLobbyApi->iQueueTail;
                            memset(pQueueEntry, 0, sizeof(*pQueueEntry));
                            pQueueEntry->iKind = 0x6e657773; // news
                            pQueueEntry->pCallback = _LobbyApiConfigCallback;
                            pQueueEntry->pUserData = NULL;
                            pQueueEntry->iTick = (s32)CommUDPTick(pLobbyApi->net);
                            pQueueEntry->iId = pLobbyApi->iCallbackID;
                            ++pLobbyApi->iCallbackID;
                            if (pLobbyApi->iCallbackID >= 4096)
                            {
                                pLobbyApi->iCallbackID = 1;
                            }
                            pQueueEntry->iSeqn = -1;

                            const s32 iSnoopResult = LobbyApiXenonSnoopRequest(
                                pLobbyApi->pXenon, 0x6e657773, "NAME=client.cfg", pQueueEntry->iId);
                            if (iSnoopResult >= 0)
                            {
                                if (iSnoopResult == 0)
                                {
                                    LobbyApiAriesRequest(pLobbyApi, 0x6e657773,
                                                         "NAME=client.cfg", pQueueEntry->iId);
                                }
                            }
                            else
                            {
                                LobbyCBQueueRefT FailedRequest;
                                _LobbyApiRequestQueuePop(pLobbyApi, &FailedRequest, 0x6e657773);
                            }
                        }
                        else
                        {
                            LobbyApiPrintfCode(pLobbyApi,
                                               "lobbyapi: request/response queue overflow\n");
                            NetPrintfCode("lobbyapi: request queue overflow; '%c%c%c%c' request discarded\n",
                                          'n', 'e', 'w', 's');
                        }
                    }

                    if ((pLobbyApi->state == 0x736b6579) &&
                        (((u32)iKind == 0x736b6579) || ((u32)iKind == 0x73656c65)) &&
                        (iCode == 0)) // skey/sele
                    {
                        if ((u32)iKind == 0x736b6579)
                        {
                            TagFieldGetBinary(TagFieldFind(pData, "SKEY"),
                                              pLobbyApi->skey, sizeof(pLobbyApi->skey));
                            TagFieldGetString(TagFieldFind(pData, "DP"), pLobbyApi->strPartition,
                                              sizeof(pLobbyApi->strPartition), "");
                        }
                        iCode = 0;
                        iKind = 0x636f6e6e; // conn
                        pLobbyApi->nextServerPingTime = uNow;
                        pLobbyApi->state = 0x61757468; // auth
                    }

                    if ((iKind == -1) && ((u32)iCode == 0xfefefefe) &&
                        (pLobbyApi->state != 0x636f6e6e)) // conn
                    {
                        _LobbyApiDisconnect(pLobbyApi, 0x73657276, NULL); // serv
                    }

                    if ((u32)iKind == 0x61757468) // auth
                    {
                        if (iCode == 0)
                        {
                            pLobbyApi->state = 0x61757468;
                            TagFieldGetString(TagFieldFind(pData, "NAME"), pLobbyApi->acctname,
                                              sizeof(pLobbyApi->acctname), "");
                            pLobbyApi->extaddr = TagFieldGetAddress(TagFieldFind(pData, "ADDR"), 0);
                        }
                        TagFieldDupl(pLobbyApi->strAuthResult,
                                     sizeof(pLobbyApi->strAuthResult), pData);
                    }
                    if ((u32)iKind == 0x61636374) // acct
                    {
                        if (iCode == 0)
                        {
                            pLobbyApi->state = 0x61636374;
                        }
                    }
                    if ((u32)iKind == 0x736e6170) // snap
                    {
                        --pLobbyApi->snapopen;
                        if ((pLobbyApi->snapopen == 0) && (iCode == 0))
                        {
                            pLobbyApi->snapchan = TagFieldGetNumber(
                                TagFieldFind(pData, "CHAN"), 0);
                        }
                    }
                    if ((u32)iKind == 0x70657273) // pers
                    {
                        TagFieldDupl(pLobbyApi->strPersResult,
                                     sizeof(pLobbyApi->strPersResult), pData);
                        const char* pEnd = pData;
                        while (*pEnd != 0)
                        {
                            ++pEnd;
                        }
                        if ((u32)(pEnd - pData) > 0x400)
                        {
                            NetPrintfCode("lobbyapi: warning -- pers response is too large\n");
                        }
                        if (iCode == 0)
                        {
                            memset(&pLobbyApi->user, 0, sizeof(pLobbyApi->user));
                            TagFieldGetString(TagFieldFind(pData, "PERS"),
                                              pLobbyApi->user.name,
                                              sizeof(pLobbyApi->user.name), "");
                            LobbyApiExtractUserAddr(&pLobbyApi->user, pData);
                            TagFieldGetString(TagFieldFind(pData, "LKEY"), pLobbyApi->strLKey,
                                              sizeof(pLobbyApi->strLKey), "");
                            pLobbyApi->uIdleTimeout = (u32)TagFieldGetNumber(
                                TagFieldFind(pData, "IDLE"), (s32)pLobbyApi->uIdleTimeout);
                            pLobbyApi->nextServerPingTime = uNow;
                        }
                    }
                    if (((u32)iKind == 0x73656c65) && (iCode == 0)) // sele
                    {
                        pLobbyApi->rankmore = TagFieldGetNumber(TagFieldFind(pData, "MORE"), 0);
                        pLobbyApi->ranknumb = TagFieldGetNumber(TagFieldFind(pData, "SLOTS"), 0);
                        if (TagFieldGetNumber(TagFieldFind(pData, "STATS"), 0) != 0)
                        {
                            if (pLobbyApi->serverStats.iEnabled == 0)
                            {
                                pLobbyApi->serverStats.iUpdate = 0;
                            }
                            pLobbyApi->serverStats.iEnabled = 1;
                        }
                        else
                        {
                            pLobbyApi->serverStats.iUpdate = 0;
                            pLobbyApi->serverStats.iEnabled = 0;
                        }
                    }

                    if (((u32)iKind & 0xff000000) == 0x2b000000)
                    {
                        LobbyApiAsyncProcess(pLobbyApi, pData);
                    }
                    if ((((u32)iKind == 0x6d6f7665) || ((u32)iKind == 0x726f6f6d)) &&
                        (iCode == 0))
                    {
                        LobbyApiAsyncMoveRoom(pLobbyApi, pData);
                    }
                    if (((u32)iKind & 0xff000000) == 0x24000000)
                    {
                        LobbyApiGlobalCallback(pLobbyApi, 0, LOBBYAPI_CBTYPE_EVNT,
                                               iKind, iCode, pData);
                    }
                }
            }

            ProtoAriesRecv(pLobbyApi->net, NULL, NULL, NULL, 0);
            iReadSize = ProtoAriesPeek(pLobbyApi->net, &iKind, &iCode, &pData);
        }
    }

    LobbyApiListUpdate(pLobbyApi);
    if ((pLobbyApi->nextServerPingTime != 0) && (uNow >= pLobbyApi->nextServerPingTime))
    {
        if (pLobbyApi->pPingManager != NULL)
        {
            PingManagerPingServer(pLobbyApi->pPingManager, pLobbyApi->lobbyaddr,
                                  _LobbyApiPingCallback, pLobbyApi);
        }
        pLobbyApi->nextServerPingTime = uNow + 30000;
    }
    LobbyApiGlobalCallback(pLobbyApi, 0, LOBBYAPI_CBTYPE_IDLE, 0, 0, NULL);
    LobbyApiXenonUpdate(pLobbyApi->pXenon);
}

extern "C" void LobbyApiAriesRequest(LobbyApiRefT* pLobbyApi, s32 iKind,
                                      const char* pRequest, s32 iCallbackID)
{
    _LobbyApiResponseLog(pLobbyApi, iKind, 0, (const u8*)pRequest, "send");

    s32 iIndex = 0;
    while ((iIndex < pLobbyApi->iQueueTail) &&
           (pLobbyApi->CallbackQueue[iIndex].iId != iCallbackID))
    {
        ++iIndex;
    }
    pLobbyApi->CallbackQueue[iIndex].iSeqn = pLobbyApi->iCallbackSeqn;
    ++pLobbyApi->iCallbackSeqn;

    const s32 iResult = ProtoAriesSend(pLobbyApi->net, iKind, 0, pRequest, -1);
    if (iResult != 0)
    {
        NetPrintfCode("lobbyapi: error sending '%c%c%c%c' request to server\n",
                      ((u32)iKind >> 24) & 0xff, ((u32)iKind >> 16) & 0xff,
                      ((u32)iKind >> 8) & 0xff, (u32)iKind & 0xff);
    }
}

extern "C" s32 LobbyApiSetCallback(LobbyApiRefT* pLobbyApi, s32 iType,
                                    LobbyApiCallbackT* pCallback, void* pUserData)
{
    s32 iCallback;
    for (iCallback = 0; pLobbyApi->callbacks[iCallback].eType != LOBBYAPI_CBTYPE_INVALID; )
    {
        if (++iCallback >= 32)
        {
            return -1;
        }
    }

    pLobbyApi->callbacks[iCallback].pCallback = pCallback;
    pLobbyApi->callbacks[iCallback].eType = iType;
    pLobbyApi->callbacks[iCallback].pUserData = pUserData;
    return iCallback | 0x1000;
}

extern "C" s32 LobbyApiClearCallback(LobbyApiRefT* pLobbyApi, s32 iCallbackID)
{
    const u32 iCallback = (u32)iCallbackID & 0xffffefff;
    if (iCallback > 31)
    {
        return 0;
    }

    LobbyGlobalCBRefT* pCallback = &pLobbyApi->callbacks[iCallback];
    if (pCallback->eType == LOBBYAPI_CBTYPE_INVALID)
    {
        return 0;
    }

    pCallback->eType = LOBBYAPI_CBTYPE_INVALID;
    return 1;
}

extern "C" s32 LobbyApiConnect(LobbyApiRefT* pLobbyApi, const char* pServerName,
                                s32 iPort, LobbyApiCallbackT* pCallback,
                                void* pUserData, u32 uConnTimeout)
{
    s32 iCallbackID = 0;
    if (uConnTimeout < 5000)
    {
        uConnTimeout = 5000;
    }
    if (uConnTimeout < 10000)
    {
        NetPrintfCode("lobbyapi: warning, connection timeout of %d msec is below the minimum recommended ten seconds\n",
                      uConnTimeout);
    }

    pLobbyApi->lobbyaddr = 0;
    pLobbyApi->lobbyport = iPort;
    pLobbyApi->rdirport = iPort;
    pLobbyApi->uConnTimeout = uConnTimeout;

    LobbyCBQueueRefT Request;
    while (_LobbyApiRequestQueuePop(pLobbyApi, &Request, 0x44515545)) // DQUE
    {
    }
    pLobbyApi->snapopen = 0;
    pLobbyApi->nextServerPingTime = 0;
    pLobbyApi->nFailedServerPings = 0;
    pLobbyApi->serverPingLatency = -1;

    if (pCallback != NULL)
    {
        LobbyCBQueueRefT* pCallbackData = (LobbyCBQueueRefT*)DirtyMemAlloc(
            sizeof(*pCallbackData), 1818325097, pLobbyApi->iMemGroup);
        if (pCallbackData == NULL)
        {
            return -1;
        }
        memset(pCallbackData, 0, sizeof(*pCallbackData));
        pCallbackData->iKind = 0x6f6e6365; // once
        pCallbackData->iTick = (s32)CommUDPTick(pLobbyApi->net);
        pCallbackData->pCallback = pCallback;
        pCallbackData->pUserData = pUserData;
        iCallbackID = LobbyApiSetCallback(pLobbyApi, LOBBYAPI_CBTYPE_CONN,
                                          _LobbyApiConnectCallback, pCallbackData);
        pCallbackData->iId = iCallbackID;
        if (iCallbackID < 0)
        {
            return -1;
        }
    }

    ds_strnzcpy(pLobbyApi->strServerName, pServerName, sizeof(pLobbyApi->strServerName));
    pLobbyApi->state = 0x696e6974; // init
    return iCallbackID;
}

extern "C" s32 LobbyApiControl(LobbyApiRefT* pLobbyApi, s32 iKind, s32 iValue)
{
    if ((u32)iKind == 0x646f626a) // dobj
    {
        pLobbyApi->bEnableDispElemCb = (u32)iValue;
        return 0;
    }
    if ((u32)iKind == 0x7274696d) // rtim
    {
        const s32 iOldValue = (s32)pLobbyApi->uTrnsTimeout;
        pLobbyApi->uTrnsTimeout = (u32)iValue;
        return iOldValue;
    }
    return 0;
}

extern "C" LobbyApiUserSetT* LobbyApiGetUserSetInfoByIdent(LobbyApiRefT* pLobbyApi,
                                                             s32 iUserSetIdent)
{
    if (pLobbyApi->UserSet.iIdent == iUserSetIdent)
    {
        return &pLobbyApi->UserSet;
    }
    return NULL;
}

extern "C" const void* LobbyApiInfo(LobbyApiRefT* pLobbyApi, s32 iSelect)
{
    if ((u32)iSelect == 0x61757464) // autd
    {
        return pLobbyApi->strAuthResult;
    }
    if ((u32)iSelect == 0x636f6e66) // conf
    {
        return pLobbyApi->strConfig;
    }
    if ((u32)iSelect == 0x6c697374) // list
    {
        return pLobbyApi->pListRef;
    }
    if ((u32)iSelect == 0x6c6b6579) // lkey
    {
        return pLobbyApi->strLKey[0] != 0 ? pLobbyApi->strLKey : NULL;
    }
    if ((u32)iSelect == 0x70657264) // perd
    {
        return pLobbyApi->strPersResult;
    }
    return NULL;
}

extern "C" s32 LobbyApiCancelCB(LobbyApiRefT* pLobbyApi, s32 iCallbackID)
{
    if (iCallbackID <= 0)
    {
        LobbyApiPrintfCode(pLobbyApi,
                           "lobbyapi: callback id %d is invalid and cannot be cancelled",
                           iCallbackID);
        return 0;
    }

    if (((u32)iCallbackID & 0x1000) != 0)
    {
        const u32 iCallback = (u32)iCallbackID & 0xffffefff;
        if (iCallback > 31)
        {
            LobbyApiPrintfCode(pLobbyApi,
                               "_LobbyApiCancelOneShotCB: Invalid callback index %d\n",
                               iCallback);
            return 0;
        }

        LobbyGlobalCBRefT* pCallback = &pLobbyApi->callbacks[iCallback];
        void* pCallbackData = pCallback->pUserData;
        if (pCallback->eType != LOBBYAPI_CBTYPE_INVALID)
        {
            pCallback->eType = LOBBYAPI_CBTYPE_INVALID;
        }
        DirtyMemFree(pCallbackData, 1818325097, pLobbyApi->iMemGroup);
        return 1;
    }

    s32 iFound = 0;
    for (s32 iIndex = 0; iIndex < pLobbyApi->iQueueTail; ++iIndex)
    {
        LobbyCBQueueRefT* pRequest = &pLobbyApi->CallbackQueue[iIndex];
        if (pRequest->iId == iCallbackID)
        {
            iFound = 1;
            pRequest->pCallback = NULL;
        }
    }
    return iFound;
}

extern "C" void LobbyApiDisconnect(LobbyApiRefT* pLobbyApi, s32 iCode)
{
    _LobbyApiDisconnect(pLobbyApi, iCode, NULL);
}

extern "C" void LobbyApiSuspend(LobbyApiRefT* pLobbyApi)
{
    if (pLobbyApi->bSuspended == 1)
    {
        LobbyApiPrintfCode(pLobbyApi, "lobbyapi: trying to suspend when already suspended\n");
        return;
    }

    char strSelect[256];
    TagFieldPrintf(strSelect, sizeof(strSelect),
                   "INGAME=%d MESGS=%d MESGTYPES=%f USERS=%d GAMES=%d MYGAME=%d ROOMS=%d ASYNC=%d USERSETS=%d STATS=%d",
                   1, 1, (s32)0x02010080, 0, 0, 0, 0, 0, 0, 0);
    const u32 uState = (u32)pLobbyApi->state;
    if ((uState == 0x69646c65) || (uState == 0x61757468) ||
        (uState == 0x61636374) || (uState == 0x736b6579))
    {
        LobbyApiRequestCB(pLobbyApi, 0x73656c65, strSelect, NULL, NULL); // sele
    }

    if (pLobbyApi->pPingManager != NULL)
    {
        PingManagerDestroy(pLobbyApi->pPingManager);
        pLobbyApi->pPingManager = NULL;
    }
    if (pLobbyApi->pListRef != NULL)
    {
        LobbyApiListDestroy(pLobbyApi);
    }
    pLobbyApi->nextServerPingTime = 0;
    pLobbyApi->nFailedServerPings = 0;
    pLobbyApi->bSuspended = 1;
    pLobbyApi->serverPingLatency = -1;
}

extern "C" void LobbyApiResume(LobbyApiRefT* pLobbyApi)
{
    if (pLobbyApi->bSuspended == 0)
    {
        LobbyApiPrintfCode(pLobbyApi, "lobbyapi: trying to resume when not suspended\n");
        return;
    }

    pLobbyApi->nextServerPingTime = NetTick(pLobbyApi);
    DirtyMemGroupEnter(pLobbyApi->iMemGroup);
    LobbyApiListCreate(pLobbyApi);
    pLobbyApi->pPingManager = PingManagerCreate(128);

    char strSelect[32];
    strSelect[0] = 0;
    TagFieldSetNumber(strSelect, sizeof(strSelect), "INGAME", 0);
    const u32 uState = (u32)pLobbyApi->state;
    if ((uState == 0x69646c65) || (uState == 0x61757468) ||
        (uState == 0x61636374) || (uState == 0x736b6579))
    {
        LobbyApiRequestCB(pLobbyApi, 0x73656c65, strSelect, NULL, NULL); // sele
    }
    DirtyMemGroupLeave();
    pLobbyApi->bSuspended = 0;
}

static LobbyApiRefT* _LobbyApiCreate2(const char* pTagField, void* pDebugRef,
                                     void (*pDebugFnc)(void*, const char*),
                                     u32 (*pDebugFilter)(void*), s32 iRefMemGroup)
{
    const s32 iMemGroup = DirtyMemGroupQuery();
    if ((TagFieldFind(pTagField, "SKU") == NULL) ||
        (TagFieldFind(pTagField, "SLUS") == NULL) ||
        (TagFieldFind(pTagField, "VERS") == NULL))
    {
        NetPrintfCode("lobbyapi: create error, not all required parameters were supplied\n");
        return NULL;
    }
    if (iRefMemGroup == 0)
    {
        iRefMemGroup = iMemGroup;
    }

    LobbyApiRefT* pLobbyApi = (LobbyApiRefT*)DirtyMemAlloc(
        sizeof(LobbyApiRefT), 1818325097, iRefMemGroup);
    if (pLobbyApi == NULL)
    {
        return NULL;
    }
    memset(pLobbyApi, 0, sizeof(*pLobbyApi));
    pLobbyApi->iMemGroup = iMemGroup;
    pLobbyApi->iRefMemGroup = iRefMemGroup;

    if (pTagField != NULL)
    {
        if (strchr(pTagField, '=') == NULL)
        {
            TagFieldSetString(pLobbyApi->strVersion, sizeof(pLobbyApi->strVersion),
                              "VERS", pTagField);
        }
        else
        {
            TagFieldDupl(pLobbyApi->strVersion, sizeof(pLobbyApi->strVersion), pTagField);
        }
        char strSdkVersion[32];
        ds_snzprintf(strSdkVersion, sizeof(strSdkVersion), "%d.%d.%d.%d", 5, 5, 3, 0);
        TagFieldSetString(pLobbyApi->strVersion, sizeof(pLobbyApi->strVersion),
                          "SDKVERS", strSdkVersion);
        TagFieldSetString(pLobbyApi->strVersion, sizeof(pLobbyApi->strVersion),
                          "BUILDDATE", "Jan 22 2008");
    }

    DirtyMemGroupEnter(iRefMemGroup);
    pLobbyApi->pXenon = LobbyApiXenonCreate(pLobbyApi);
    if (pLobbyApi->pXenon == NULL)
    {
        return NULL;
    }

    pLobbyApi->net = ProtoAriesCreate(0x2000);
    if (pLobbyApi->net == NULL)
    {
        LobbyApiPrintfCode(pLobbyApi, "lobbyapi: unable to create protoaries module\n");
        DirtyMemFree(pLobbyApi, 1818325097, pLobbyApi->iMemGroup);
        return NULL;
    }
    DirtyMemGroupLeave();

    pLobbyApi->pPingManager = PingManagerCreate(128);
    if (pLobbyApi->pPingManager == NULL)
    {
        LobbyApiPrintfCode(pLobbyApi, "lobbyapi: unable to create pingmanager module\n");
    }
    pLobbyApi->debugref = pDebugRef;
    pLobbyApi->debugptr = pDebugFnc;
    pLobbyApi->debugflt = pDebugFilter;
    pLobbyApi->uConnTimeout = 120000;
    pLobbyApi->uIdleTimeout = 60000;
    pLobbyApi->uTrnsTimeout = 30000;
    for (s32 iIndex = 0; iIndex < 32; ++iIndex)
    {
        pLobbyApi->callbacks[iIndex].eType = LOBBYAPI_CBTYPE_INVALID;
    }
    LobbyApiListCreate(pLobbyApi);
    _LobbyApiDisconnect(pLobbyApi, 0, NULL);
    pLobbyApi->uServTime = TagFieldGetEpoch(NULL, 0);
    return pLobbyApi;
}

extern "C" LobbyApiRefT* LobbyApiCreate(const char* pTagField, void* pDebugRef,
                                          void (*pDebugFnc)(void*, const char*),
                                          s32 iRefMemGroup)
{
    return _LobbyApiCreate2(pTagField, pDebugRef, pDebugFnc, NULL, iRefMemGroup);
}

extern "C" void LobbyApiDestroy(LobbyApiRefT* pLobbyApi)
{
    _LobbyApiDisconnect(pLobbyApi, 0x7465726d, NULL); // term
    LobbyApiListDestroy(pLobbyApi);
    if (pLobbyApi->pPingManager != NULL)
    {
        PingManagerDestroy(pLobbyApi->pPingManager);
    }
    ProtoAriesDestroy(pLobbyApi->net);
    LobbyApiXenonDestroy(pLobbyApi->pXenon);
    DirtyMemFree(pLobbyApi, 1818325097, pLobbyApi->iRefMemGroup);
}

extern "C" s32 LobbyApiRequestCB(LobbyApiRefT* pLobbyApi, s32 iKind, const char* pRequest,
                                 LobbyApiCallbackT* pCallback, void* pUserData)
{
    const s32 iState = pLobbyApi->state;
    if ((iState != 0x69646c65) && (iState != 0x61757468) &&
        (iState != 0x61636374) && (iState != 0x736b6579))
    {
        return -1;
    }

    if (((u32)iKind == 0x6d616472) && (pCallback != NULL)) // madr
    {
        HasherRef* pHash = LobbyApiListGetHash(pLobbyApi, 1);
        if (pHash != NULL)
        {
            DispListRef* pDisp = LobbyApiListGetDisp(pLobbyApi, 1);
            const s32 iCount = DispListCount(pDisp);
            char strName[16];
            TagFieldGetString(TagFieldFind(pRequest, "PERS"), strName,
                              sizeof(strName), "");

            for (s32 iIndex = 0; iIndex < iCount; ++iIndex)
            {
                LobbyApiUserT* pUser = (LobbyApiUserT*)DispListIndex(pDisp, iIndex);
                if (LobbyNameCmp(strName, pUser->name) == 0)
                {
                    char strCachedResponse[128];
                    strCachedResponse[0] = 0;
                    TagFieldSetString(strCachedResponse, sizeof(strCachedResponse), "MADDR",
                                      (const char*)&pUser->MachineAddr);

                    LobbyApiMsgT Msg;
                    memset(&Msg, 0, sizeof(Msg));
                    Msg.kind = iKind;
                    Msg.pData = strCachedResponse;
                    NetPrintfCode("lobbyapi: found user '%s' in user list; short-circuiting maddr lookup\n",
                                  pUser->name);
                    pCallback(pLobbyApi, &Msg, pUserData);
                    return 0;
                }
            }
        }
    }

    if (pLobbyApi->iQueueTail >= 32)
    {
        LobbyApiPrintfCode(pLobbyApi, "lobbyapi: request/response queue overflow\n");
        NetPrintfCode("lobbyapi: request queue overflow; '%c%c%c%c' request discarded\n",
                      ((u32)iKind >> 24) & 0xff, ((u32)iKind >> 16) & 0xff,
                      ((u32)iKind >> 8) & 0xff, (u32)iKind & 0xff);
        return -2;
    }

    const s32 iQueueIndex = pLobbyApi->iQueueTail;
    LobbyCBQueueRefT* pQueueEntry = &pLobbyApi->CallbackQueue[iQueueIndex];
    ++pLobbyApi->iQueueTail;
    memset(pQueueEntry, 0, sizeof(*pQueueEntry));
    pQueueEntry->iKind = iKind;
    pQueueEntry->pCallback = pCallback;
    pQueueEntry->pUserData = pUserData;
    pQueueEntry->iTick = (s32)CommUDPTick(pLobbyApi->net);
    pQueueEntry->iId = pLobbyApi->iCallbackID;
    ++pLobbyApi->iCallbackID;
    if (pLobbyApi->iCallbackID >= 4096)
    {
        pLobbyApi->iCallbackID = 1;
    }
    pQueueEntry->iSeqn = -1;

    char strModifiedRequest[0x300];
    const char* pWireRequest = pRequest;
    if (((u32)iKind == 0x61757468) || ((u32)iKind == 0x61636374) ||
        ((u32)iKind == 0x70617373)) // auth/acct/pass
    {
        TagFieldDupl(strModifiedRequest, sizeof(strModifiedRequest), pLobbyApi->strVersion);
        TagFieldMerge(strModifiedRequest, sizeof(strModifiedRequest), pRequest);

        char strPassword[32];
        char* pEncryptedPassword = &strPassword[1];
        pEncryptedPassword[-1] = '~';
        TagFieldGetString(TagFieldFind(strModifiedRequest, "PASS"), pEncryptedPassword,
                          31, "");
        CryptSSC2StringEncrypt(pEncryptedPassword, 31, pEncryptedPassword,
                               (const u8*)pLobbyApi->skey, 16, 16);
        TagFieldSetString(strModifiedRequest, sizeof(strModifiedRequest), "PASS", strPassword);
        pWireRequest = strModifiedRequest;
    }

    if ((u32)iKind == 0x736e6170) // snap
    {
        const s32 iChannel = TagFieldGetNumber(TagFieldFind(pWireRequest, "CHAN"), 0);
        if (((u32)iChannel - 5) <= 3)
        {
            LobbyApiListFlush(pLobbyApi, iChannel);
        }
        pLobbyApi->snapchan = 0;
        ++pLobbyApi->snapopen;
    }

    if ((u32)iKind == 0x6d6f7665) // move
    {
        if (LobbyApiListGetHash(pLobbyApi, 0) != NULL)
        {
            char strRoomName[36];
            TagFieldGetString(TagFieldFind(pWireRequest, "NAME"), strRoomName,
                              sizeof(strRoomName), "");
            if (strRoomName[0] != 0)
            {
                DispListRef* pDisp = LobbyApiListGetDisp(pLobbyApi, 0);
                DispListT* pItem = LobbyApiListFindByName(pLobbyApi, 0, strRoomName);
                if (pItem != NULL)
                {
                    LobbyApiRoomRecordPrefixT* pRoom =
                        (LobbyApiRoomRecordPrefixT*)DispListGet(pDisp, pItem);
                    TagFieldDupl(strModifiedRequest, sizeof(strModifiedRequest), pWireRequest);
                    TagFieldSetNumber(strModifiedRequest, sizeof(strModifiedRequest),
                                      "IDENT", pRoom->iIdent);
                    pWireRequest = strModifiedRequest;
                }
            }
        }
    }

    const s32 iSnoopResult = LobbyApiXenonSnoopRequest(pLobbyApi->pXenon, iKind,
                                                       pWireRequest, pQueueEntry->iId);
    if (iSnoopResult < 0)
    {
        LobbyCBQueueRefT Request;
        _LobbyApiRequestQueuePop(pLobbyApi, &Request, iKind);
        return iSnoopResult;
    }
    if (iSnoopResult == 0)
    {
        LobbyApiAriesRequest(pLobbyApi, iKind, pWireRequest, pQueueEntry->iId);
    }
    return pQueueEntry->iId;
}

extern "C" s32 LobbyApiStatus(LobbyApiRefT* pLobbyApi, s32 iSelect, void* pBuf, s32 iBufLen)
{
    switch ((u32)iSelect)
    {
        case 0x6578746e: // extn
            return (s32)pLobbyApi->extaddr;

        case 0x686f7374: // host
            if (pLobbyApi->net != NULL)
            {
                return ProtoAriesStatus(pLobbyApi->net, 0x61646472, NULL, 0);
            }
            return 0;

        case 0x696e6174: // inat
            if ((pLobbyApi->extaddr != 0) && (pLobbyApi->ouraddr != 0))
            {
                return pLobbyApi->ouraddr != pLobbyApi->extaddr;
            }
            return 0;

        case 0x6c636e74: // lcnt
        {
            if (pLobbyApi->serverStats.iEnabled != 0)
            {
                return (s32)(pLobbyApi->serverStats.uUsersInRooms +
                             pLobbyApi->serverStats.uUsersInLobby);
            }

            DispListRef* pDisp = LobbyApiListGetDisp(pLobbyApi, 0);
            if (pDisp == NULL)
            {
                return -1;
            }

            s32 iPopulation = 0;
            const s32 iCount = DispListCount(pDisp);
            for (s32 iIndex = 0; iIndex < iCount; ++iIndex)
            {
                const LobbyApiRoomRecordPrefixT* pRoom =
                    (const LobbyApiRoomRecordPrefixT*)DispListIndex(pDisp, iIndex);
                iPopulation += pRoom->iPopulation;
            }
            return iPopulation;
        }

        case 0x6c6f636c: // locl
            return (s32)pLobbyApi->ouraddr;

        case 0x6c6f676e: // logn
            return pLobbyApi->user.name[0] != 0;

        case 0x6d6f7265: // more
            return pLobbyApi->rankmore;

        case 0x72666c67: // rflg
            return pLobbyApi->roomflags;

        case 0x726f6f6d: // room
            if ((pBuf != NULL) && (iBufLen > 0))
            {
                ds_strnzcpy((char*)pBuf, pLobbyApi->roomname, iBufLen);
            }
            return pLobbyApi->roomidx;

        case 0x73656375: // secu
            if (pLobbyApi->net != NULL)
            {
                return ProtoAriesStatus(pLobbyApi->net, 0x73656375, pBuf, iBufLen);
            }
            return 0;

        case 0x736c6f74: // slot
            return pLobbyApi->ranknumb;

        case 0x736f636b: // sock
            if (pLobbyApi->net != NULL)
            {
                return ProtoAriesStatus(pLobbyApi->net, 0x736f636b, pBuf, iBufLen);
            }
            return 0;

        case 0x73746174: // stat
            return pLobbyApi->state;

        case 0x7374696d: // stim
            return (s32)pLobbyApi->uServTime;

        case 0x73757370: // susp
            return (s32)pLobbyApi->bSuspended;

        case 0x7469636b: // tick
            return (s32)CommUDPTick(pLobbyApi->net);
    }

    if (pBuf != NULL && iBufLen > 0)
    {
        if ((iSelect == 0x61636374) || (iSelect == 0x75736572)) // acct/user
        {
            ds_strnzcpy((char*)pBuf, pLobbyApi->acctname, iBufLen);
            return 0;
        }
        if (iSelect == 0x70617274) // part
        {
            ds_strnzcpy((char*)pBuf, pLobbyApi->strPartition, iBufLen);
            return 0;
        }
        if (iSelect == 0x70657273) // pers
        {
            ds_strnzcpy((char*)pBuf, pLobbyApi->user.name, iBufLen);
            return 0;
        }
        if (iSelect == 0x73656c66) // self
        {
            if (iBufLen == (s32)sizeof(pLobbyApi->user))
            {
                memcpy(pBuf, &pLobbyApi->user, sizeof(pLobbyApi->user));
                return 0;
            }
            return -1;
        }
        if (iSelect == 0x73657276) // serv
        {
            if (iBufLen == (s32)sizeof(pLobbyApi->serverStats))
            {
                memcpy(pBuf, &pLobbyApi->serverStats, sizeof(pLobbyApi->serverStats));
                return 0;
            }
            return -1;
        }
        if (iSelect == 0x736c7573) // slus
        {
            TagFieldGetString(TagFieldFind(pLobbyApi->strVersion, "SLUS"),
                              (char*)pBuf, iBufLen, "");
            return 0;
        }
        if (iSelect == 0x76657273) // vers
        {
            TagFieldGetString(TagFieldFind(pLobbyApi->strVersion, "VERS"),
                              (char*)pBuf, iBufLen, "");
            return 0;
        }
    }
    return -1;
}

extern "C" LobbyApiUserT* LobbyApiGetUserInfo(LobbyApiRefT* pLobbyApi, const char* pUserName)
{
    if ((pUserName == NULL) || (LobbyNameCmp(pUserName, pLobbyApi->user.name) == 0))
    {
        return &pLobbyApi->user;
    }

    HasherRef* pHash = LobbyApiListGetHash(pLobbyApi, 1);
    if (pHash == NULL)
    {
        return NULL;
    }

    DispListRef* pDisp = LobbyApiListGetDisp(pLobbyApi, 1);
    HasherEnumInit(pHash);
    DispListT* pItem = HasherEnum(pHash, 0);
    if (pItem == NULL)
    {
        return NULL;
    }

    LobbyApiUserT* pUser;
    do
    {
        pUser = (LobbyApiUserT*)DispListGet(pDisp, pItem);
        if (LobbyNameCmp(pUserName, pUser->name) == 0)
        {
            return pUser;
        }
        pItem = HasherEnum(pHash, 0);
    }
    while (pItem != NULL);
    return NULL;
}

extern "C" void LobbyApiColor(LobbyApiColorT* pColor, s32 iColor)
{
    u32 uColor = (u32)((iColor == 0) ? 511 : iColor);
    const u32 uRed = (uColor >> 6) & 7;
    const u32 uGreen = (uColor >> 3) & 7;
    const u32 uBlue = uColor & 7;

    pColor->uRed = (u8)((4 * ((8 * uRed) | uRed)) | (uRed >> 1));
    pColor->uGreen = (u8)((4 * ((8 * uGreen) | uGreen)) | (uGreen >> 1));
    pColor->uBlue = (u8)((4 * ((8 * uBlue) | uBlue)) | (uBlue >> 1));
    pColor->uAlpha = 0xff;
}

extern "C" void LobbyApiExtractUserAddr(LobbyApiUserT* pUser, const char* pData)
{
    pUser->addr = TagFieldGetAddress(TagFieldFind(pData, "A"), 0);
    pUser->uLocalAddr = TagFieldGetAddress(TagFieldFind(pData, "LA"), 0);
    TagFieldGetString(TagFieldFind(pData, "MA"), (char*)&pUser->MachineAddr,
                      sizeof(pUser->MachineAddr), "");
}

extern "C" void LobbyApiExtractUserRecord(LobbyApiUserT* pUser, const char* pData)
{
    memset(pUser, 0, sizeof(*pUser));
    pUser->ident = TagFieldGetNumber(TagFieldFind(pData, "I"), 0);
    TagFieldGetString(TagFieldFind(pData, "N"), pUser->name, sizeof(pUser->name), "");
    pUser->flags = TagFieldGetFlags(TagFieldFind(pData, "F"), 0);
    TagFieldGetString(TagFieldFind(pData, "P"), pUser->ping, sizeof(pUser->ping), "");
    TagFieldGetString(TagFieldFind(pData, "S"), pUser->stat, sizeof(pUser->stat), "");
    TagFieldGetString(TagFieldFind(pData, "X"), pUser->aux, sizeof(pUser->aux), "");
    pUser->game = TagFieldGetNumber(TagFieldFind(pData, "G"), 0);
    pUser->attr = (u32)TagFieldGetFlags(TagFieldFind(pData, "AT"), 0);
    LobbyApiColor(&pUser->Color, TagFieldGetNumber(TagFieldFind(pData, "CL"), 0));
    pUser->uLevel = (u32)TagFieldGetNumber(TagFieldFind(pData, "LV"), 0);
    pUser->uMedals = (u32)TagFieldGetNumber(TagFieldFind(pData, "MD"), 0);
    pUser->rank = TagFieldGetNumber(TagFieldFind(pData, "R"), 0);
    pUser->iUserSetIdent = TagFieldGetNumber(TagFieldFind(pData, "US"), 0);
    pUser->uHwFlags = (u32)TagFieldGetNumber(TagFieldFind(pData, "HW"), 0);
    pUser->iReputation = TagFieldGetNumber(TagFieldFind(pData, "RP"), 0);
    pUser->uLocality = (u32)TagFieldGetToken(TagFieldFind(pData, "LO"), 0);
    TagFieldGetString(TagFieldFind(pData, "CI"), pUser->strClubID,
                      sizeof(pUser->strClubID), "");
    TagFieldGetString(TagFieldFind(pData, "CT"), pUser->strClubTag,
                      sizeof(pUser->strClubTag), "");
    LobbyApiExtractUserAddr(pUser, pData);
}
