#include "lobbyapi_x360_internal.h"

#include "lobbyname.h"
#include "lobbytagfield.h"
#include "dirtylib.h"
#include "dirtymem.h"
#include "protofilter.h"
#include "platform.h"

#include <xtl.h>
#include <xonline.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

extern "C"
{
    void HasherEnumInit(HasherRef* pHash);
    DispListT* HasherEnum(HasherRef* pHash, s32 iValue);
    HasherRef* HasherCreate(s32 iInitial, s32 iHashLength);
    void HasherDestroy(HasherRef* pHash);
    DispListT* HasherFlush(HasherRef* pHash);
    s32 HashNumAdd(HasherRef* pHash, s32 iKey, void* pData);
    void* HashNumFind(HasherRef* pHash, s32 iKey);
    void* HashNumDel(HasherRef* pHash, s32 iKey);
    s32 HashStrAdd(HasherRef* pHash, const char* pKey, void* pData);
    s32 TagFieldGetFlags(const char* pData, s32 iDefault);
    s32 TagFieldGetRaw(const char* pData, char* pBuffer, s32 iBufLen, const char* pDefault);
    s32 TagFieldGetStructure(const char* pData, void* pBuffer, s32 iLength,
                             const char* pPattern);
    s32 TagFieldGetToken(const char* pData, s32 iDefault);
    s32 NetConnQuery(const char* pName, const u8* pXuid);
    u32 NetTick(void* pCommRef);
    u32 CommUDPTick(void* pCommRef);
    void* DirtyErrGetName(s32 iError);
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
    s32 LobbyApiAsyncProcess(LobbyApiRefT* pLobbyApi, const char* pData,
                             s32 iKind, s32 iCode);
    s32 LobbyApiAsyncMoveRoom(LobbyApiRefT* pLobbyApi, const char* pData);
    void LobbyApiExtractUserAddr(LobbyApiUserT* pUser, const char* pData);
    void LobbyApiExtractRoomRecord(LobbyApiRoomRecordPrefixT* pRoom, const char* pData);
    s32 LobbyApiExtractRankRecord(LobbyApiRankRecordT* pRank, const char* pData,
                                  const char* pSelfName);
    void LobbyApiExtractUserSet(LobbyApiUserSetT* pUserSet, const char* pData);
    void LobbyApiExtractUserSetMember(LobbyApiUserSetMemberT* pMember,
                                      const char* pData);
    s32 LobbyApiListObjectUpdate(LobbyApiRefT* pLobbyApi, s32 iListType,
                                 void* pRecord, s32 iOperation);
    s32 LobbyApiPopul(LobbyApiRefT* pLobbyApi, s32 iRoomIdent, s32 iPopulation,
                      s32 iGameCount, s32 iGamePopulation);
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
                        LobbyApiAsyncProcess(pLobbyApi, pData, iKind, iCode);
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

extern "C" s32 LobbyApiPrintfCode(LobbyApiRefT* pLobbyApi, const char* pFormat, ...)
{
    char strMessage[4096];
    const char* pMessage = strMessage;
    s32 iResult = 0;
    va_list Args;
    va_start(Args, pFormat);

    if ((pFormat[0] == '%') && (pFormat[1] == 's') && (pFormat[2] == '\0'))
    {
        pMessage = va_arg(Args, const char*);
    }
    else
    {
        iResult = vsprintf(strMessage, pFormat, Args);
    }
    va_end(Args);

    if (pLobbyApi->debugptr != NULL)
    {
        pLobbyApi->debugptr(pLobbyApi->debugref, pMessage);
    }
    return iResult;
}

extern "C" void LobbyApiExtractRoomRecord(LobbyApiRoomRecordPrefixT* pRoom,
                                             const char* pData)
{
    memset(pRoom, 0, sizeof(*pRoom));
    pRoom->iIdent = TagFieldGetNumber(TagFieldFind(pData, "I"), -1);
    TagFieldGetString(TagFieldFind(pData, "N"), pRoom->strName,
                      sizeof(pRoom->strName), "");
    TagFieldGetString(TagFieldFind(pData, "DN"), pRoom->strDisplayName,
                      sizeof(pRoom->strDisplayName), "");
    TagFieldGetString(TagFieldFind(pData, "H"), pRoom->strHost,
                      sizeof(pRoom->strHost), "");
    TagFieldGetString(TagFieldFind(pData, "D"), pRoom->strDescription,
                      sizeof(pRoom->strDescription), "");
    pRoom->iFlags = TagFieldGetFlags(TagFieldFind(pData, "F"), -1);
    pRoom->iLimit = TagFieldGetNumber(TagFieldFind(pData, "L"), 0);
    pRoom->iPopulation = TagFieldGetNumber(TagFieldFind(pData, "T"), 0);
    pRoom->iGamePopulation = TagFieldGetNumber(TagFieldFind(pData, "GT"), 0);
    pRoom->iGameCount = TagFieldGetNumber(TagFieldFind(pData, "G"), 0);
    pRoom->uLocality = (u32)TagFieldGetToken(TagFieldFind(pData, "LO"), 0);
    ds_snzprintf(pRoom->strPopulation, sizeof(pRoom->strPopulation), "%d",
                 pRoom->iPopulation);
    ds_strnzcpy(pRoom->strPublic,
                ((pRoom->iFlags & 0x10000) == 0) ? "Yes" : "No",
                sizeof(pRoom->strPublic));
    TagFieldGetStructure(TagFieldFind(pData, "C"), pRoom->aStructure,
                         sizeof(pRoom->aStructure), "l*");
}

extern "C" s32 LobbyApiExtractRankRecord(LobbyApiRankRecordT* pRank,
                                            const char* pData,
                                            const char* pSelfName)
{
    memset(pRank, 0, sizeof(*pRank));
    pRank->iRank = TagFieldGetNumber(TagFieldFind(pData, "R"), 0);
    pRank->iPoints = TagFieldGetNumber(TagFieldFind(pData, "P"), 0);
    if (TagFieldGetNumber(TagFieldFind(pData, "O"), 0) != 0)
    {
        pRank->uFlags |= 4;
    }
    TagFieldGetString(TagFieldFind(pData, "N"), pRank->strName,
                      sizeof(pRank->strName), "");
    TagFieldGetRaw(TagFieldFind(pData, "S"), pRank->strStats,
                   sizeof(pRank->strStats), "");
    const s32 iCompare = LobbyNameCmp(pRank->strName, pSelfName);
    if (iCompare == 0)
    {
        pRank->uFlags |= 1;
    }
    return iCompare;
}

extern "C" void LobbyApiExtractUserSet(LobbyApiUserSetT* pUserSet,
                                         const char* pData)
{
    pUserSet->iIdent = TagFieldGetNumber(TagFieldFind(pData, "I"), 0);
    pUserSet->iType = TagFieldGetNumber(TagFieldFind(pData, "T"), 0);
    pUserSet->uSysFlags = (u32)TagFieldGetFlags(TagFieldFind(pData, "SF"), 0);
    pUserSet->uCustFlags = (u32)TagFieldGetFlags(TagFieldFind(pData, "CF"), 0);
    TagFieldGetString(TagFieldFind(pData, "O"), pUserSet->strOwner,
                      sizeof(pUserSet->strOwner), "");
    pUserSet->iSize = TagFieldGetNumber(TagFieldFind(pData, "S"), 0);
    pUserSet->iCount = TagFieldGetNumber(TagFieldFind(pData, "C"), 0);
    TagFieldGetString(TagFieldFind(pData, "N"), pUserSet->strName,
                      sizeof(pUserSet->strName), "");
    TagFieldGetString(TagFieldFind(pData, "D"), pUserSet->strDesc,
                      sizeof(pUserSet->strDesc), "");
    TagFieldGetString(TagFieldFind(pData, "P"), pUserSet->strParams,
                      sizeof(pUserSet->strParams), "");
}

extern "C" void LobbyApiExtractUserSetMember(LobbyApiUserSetMemberT* pMember,
                                               const char* pData)
{
    memset(pMember, 0, sizeof(*pMember));
    pMember->iIdent = TagFieldGetNumber(TagFieldFind(pData, "I"), 0);
    TagFieldGetString(TagFieldFind(pData, "N"), pMember->strName,
                      sizeof(pMember->strName), "");
    pMember->iFlags = TagFieldGetFlags(TagFieldFind(pData, "F"), 0);
    pMember->iGameIdent = TagFieldGetNumber(TagFieldFind(pData, "G"), 0);
    TagFieldGetString(TagFieldFind(pData, "X"), pMember->strAux,
                      sizeof(pMember->strAux), "");
    pMember->uAddr = TagFieldGetAddress(TagFieldFind(pData, "A"), 0);
    pMember->uLocalAddr = TagFieldGetAddress(TagFieldFind(pData, "LA"), 0);
    TagFieldGetString(TagFieldFind(pData, "MA"), pMember->strMachineAddr,
                      sizeof(pMember->strMachineAddr), "");
}

extern "C" s32 LobbyApiListObjectUpdate(LobbyApiRefT* pLobbyApi, s32 iListType,
                                           void* pRecord, s32 iOperation)
{
    LobbyListRefT* pListRef = pLobbyApi->pListRef;
    if ((pListRef != NULL) && (pLobbyApi->bEnableDispElemCb != 0))
    {
        LobbyDispListT* pList = &pListRef->Lists[iListType];
        if (pList->pCallback != NULL)
        {
            LobbyApiMsgT Msg;
            Msg.id = 0;
            Msg.type = LOBBYAPI_CBTYPE_DISP;
            Msg.kind = iListType;
            Msg.code = iOperation;
            Msg.pData = (const char*)pList->pDisp;
            Msg.pMisc = pRecord;
            pList->pCallback(pLobbyApi, &Msg, pList->pUserData);
        }
    }
    return (s32)pLobbyApi;
}

extern "C" s32 LobbyApiPopul(LobbyApiRefT* pLobbyApi, s32 iRoomIdent,
                                s32 iPopulation, s32 iGameCount,
                                s32 iGamePopulation)
{
    LobbyListRefT* pListRef = pLobbyApi->pListRef;
    if (pListRef != NULL)
    {
        LobbyDispListT* pList = &pListRef->Lists[0];
        if (pList->pHash != NULL)
        {
            DispListT* pItem = (DispListT*)HashNumFind(pList->pHash, iRoomIdent);
            if (pItem != NULL)
            {
                LobbyApiRoomRecordPrefixT* pRoom =
                    (LobbyApiRoomRecordPrefixT*)DispListGet(pList->pDisp, pItem);
                if (pRoom != NULL)
                {
                    if (iGameCount == -1)
                    {
                        iGameCount = pRoom->iGameCount;
                    }
                    if (iGamePopulation == -1)
                    {
                        iGamePopulation = pRoom->iGamePopulation;
                    }
                    if ((iPopulation != pRoom->iPopulation) ||
                        (iGameCount != pRoom->iGameCount) ||
                        (iGamePopulation != pRoom->iGamePopulation))
                    {
                        pRoom->iPopulation = iPopulation;
                        pRoom->iGameCount = iGameCount;
                        pRoom->iGamePopulation = iGamePopulation;
                        ds_snzprintf(pRoom->strPopulation,
                                     sizeof(pRoom->strPopulation), "%d", iPopulation);
                        DispListSet(pList->pDisp, pItem, pRoom);
                        LobbyApiListObjectUpdate(pLobbyApi, 0, pRoom, 0);
                    }
                }
            }
        }
    }
    return (s32)pLobbyApi;
}

static const s32 KI_LOBBY_LIST_RECORD_MEMID = 0x6c617363; // lasc

extern "C" s32 LobbyApiAsyncMoveRoom(LobbyApiRefT* pLobbyApi, const char* pData)
{
    const s32 iLobbyIdent = TagFieldGetNumber(TagFieldFind(pData, "LIDENT"), -1);
    if (iLobbyIdent >= 0)
    {
        const s32 iLobbyGameCount = TagFieldGetNumber(TagFieldFind(pData, "LGC"), -1);
        const s32 iLobbyGames = TagFieldGetNumber(TagFieldFind(pData, "LGCOUNT"), -1);
        const s32 iLobbyUsers = TagFieldGetNumber(TagFieldFind(pData, "LCOUNT"), 0);
        LobbyApiPopul(pLobbyApi, iLobbyIdent, iLobbyUsers,
                      iLobbyGames, iLobbyGameCount);
    }

    const s32 iRoomIdent = TagFieldGetNumber(TagFieldFind(pData, "IDENT"), 0);
    const s32 iPopulation = TagFieldGetNumber(TagFieldFind(pData, "COUNT"), 0);
    const s32 iGameCount = TagFieldGetNumber(TagFieldFind(pData, "GCOUNT"), -1);
    const s32 iGamePopulation = TagFieldGetNumber(TagFieldFind(pData, "GC"), -1);
    if (iRoomIdent != pLobbyApi->roomidx)
    {
        char strRoomName[36];
        TagFieldGetString(TagFieldFind(pData, "NAME"), strRoomName,
                          sizeof(strRoomName), "");
        const s32 iRoomFlags = TagFieldGetFlags(TagFieldFind(pData, "FLAGS"), 0);

        pLobbyApi->roomidx = iRoomIdent;
        LobbyApiPopul(pLobbyApi, iRoomIdent, iPopulation,
                      iGameCount, iGamePopulation);
        ds_strnzcpy(pLobbyApi->roomname, strRoomName, sizeof(pLobbyApi->roomname));
        pLobbyApi->roomflags = iRoomFlags;
        LobbyApiGlobalCallback(pLobbyApi, 0, LOBBYAPI_CBTYPE_EVNT,
                               0x6d6f7665, 0, pLobbyApi->roomname); // move
        LobbyApiListFlush(pLobbyApi, 1);
        LobbyApiListFlush(pLobbyApi, 4);
    }
    return 0;
}

static s32 _LobbyApiAsyncPlay(LobbyApiRefT* pLobbyApi, const char* pData, s32 iCode)
{
    LobbyApiGlobalCallback(pLobbyApi, 0, LOBBYAPI_CBTYPE_EVNT,
                           0x706c6179, iCode, pData); // play
    return 0;
}

static s32 _LobbyApiAsyncGame(LobbyApiRefT* pLobbyApi, const char* pData, s32 iCode)
{
    LobbyApiGlobalCallback(pLobbyApi, 0, LOBBYAPI_CBTYPE_EVNT,
                           0x67616d65, iCode, pData); // game
    return 0;
}

static s32 _LobbyApiAsyncChat(LobbyApiRefT* pLobbyApi, const char* pData, s32 iCode)
{
    u32 uKind = 0x63686174; // chat
    const u32 uFlags = (u32)TagFieldGetFlags(TagFieldFind(pData, "F"), 0);
    if ((uFlags & 0x4) != 0)
    {
        uKind = 0x63617374; // cast
    }
    if ((uFlags & 0x10000) != 0)
    {
        uKind = 0x70726976; // priv
    }
    LobbyApiGlobalCallback(pLobbyApi, 0, LOBBYAPI_CBTYPE_CHAT,
                           (s32)uKind, iCode, pData);
    return 0;
}

static s32 _LobbyApiAsyncWho(LobbyApiRefT* pLobbyApi, const char* pData, s32 iCode)
{
    LobbyApiExtractUserRecord(&pLobbyApi->user, pData);
    TagFieldGetStructure(TagFieldFind(pData, "C"), &pLobbyApi->roomcrit,
                         sizeof(pLobbyApi->roomcrit), "l*");
    const s32 iRoomIdent = TagFieldGetNumber(TagFieldFind(pData, "RI"), 0);
    const s32 iPopulation = TagFieldGetNumber(TagFieldFind(pData, "RT"), 0);
    const s32 iGameCount = TagFieldGetNumber(TagFieldFind(pData, "RG"), -1);
    const s32 iGamePopulation = TagFieldGetNumber(TagFieldFind(pData, "RGC"), -1);

    if (pLobbyApi->UserSet.iIdent != pLobbyApi->user.iUserSetIdent)
    {
        memset(&pLobbyApi->UserSet, 0, sizeof(pLobbyApi->UserSet));
    }
    LobbyApiGlobalCallback(pLobbyApi, 0, LOBBYAPI_CBTYPE_EVNT,
                           0x75736572, iCode, pData); // user

    if (iRoomIdent != pLobbyApi->roomidx)
    {
        char strRoomName[36];
        TagFieldGetString(TagFieldFind(pData, "RM"), strRoomName,
                          sizeof(strRoomName), "");
        const s32 iRoomFlags = TagFieldGetFlags(TagFieldFind(pData, "RF"), 0);

        pLobbyApi->roomidx = iRoomIdent;
        LobbyApiPopul(pLobbyApi, iRoomIdent, iPopulation,
                      iGameCount, iGamePopulation);
        ds_strnzcpy(pLobbyApi->roomname, strRoomName, sizeof(pLobbyApi->roomname));
        pLobbyApi->roomflags = iRoomFlags;
        LobbyApiGlobalCallback(pLobbyApi, 0, LOBBYAPI_CBTYPE_EVNT,
                               0x6d6f7665, 0, pLobbyApi->roomname); // move
        LobbyApiListFlush(pLobbyApi, 1);
        LobbyApiListFlush(pLobbyApi, 4);
    }
    return 0;
}

static s32 _LobbyApiAsyncRooms(LobbyApiRefT* pLobbyApi, const char* pData, s32 iCode)
{
    HasherRef* pHash = LobbyApiListGetHash(pLobbyApi, 0);
    if (pHash == NULL)
    {
        return 0;
    }

    DispListRef* pDisp = LobbyApiListGetDisp(pLobbyApi, 0);
    if (TagFieldFind(pData, "N") != NULL)
    {
        LobbyApiRoomRecordPrefixT* pRoom = (LobbyApiRoomRecordPrefixT*)DirtyMemAlloc(
            sizeof(*pRoom), KI_LOBBY_LIST_RECORD_MEMID, pLobbyApi->iMemGroup);
        LobbyApiExtractRoomRecord(pRoom, pData);
        DispListT* pItem = LobbyApiListFindByName(pLobbyApi, 0, pRoom->strName);
        s32 iOperation;
        if (pItem != NULL)
        {
            LobbyApiRoomRecordPrefixT* pOldRoom =
                (LobbyApiRoomRecordPrefixT*)DispListGet(pDisp, pItem);
            if ((pRoom->iIdent == 0) && (pOldRoom->iIdent != 0))
            {
                DirtyMemFree(pRoom, KI_LOBBY_LIST_RECORD_MEMID,
                             pLobbyApi->iMemGroup);
                return 0;
            }
            HashNumDel(pHash, pOldRoom->iIdent);
            DirtyMemFree(pOldRoom, KI_LOBBY_LIST_RECORD_MEMID,
                         pLobbyApi->iMemGroup);
            DispListSet(pDisp, pItem, pRoom);
            HashNumAdd(pHash, pRoom->iIdent, pItem);
            iOperation = 0;
        }
        else
        {
            if (pRoom->iIdent == 0)
            {
                --pLobbyApi->iAutoRoomID;
                pRoom->iIdent = pLobbyApi->iAutoRoomID;
            }
            pItem = DispListAdd(pDisp, pRoom);
            HashNumAdd(pHash, pRoom->iIdent, pItem);
            iOperation = 1;
        }
        LobbyApiListObjectUpdate(pLobbyApi, 0, pRoom, iOperation);
        LobbyApiGlobalCallback(pLobbyApi, 0, LOBBYAPI_CBTYPE_EVNT,
                               0x2b726f6d, iCode, pData); // +rom
        return 0;
    }

    const s32 iRoomIdent = TagFieldGetNumber(TagFieldFind(pData, "I"), -1);
    if (iRoomIdent != 0)
    {
        DispListT* pItem = (DispListT*)HashNumFind(pHash, iRoomIdent);
        if (pItem != NULL)
        {
            LobbyApiRoomRecordPrefixT* pRoom =
                (LobbyApiRoomRecordPrefixT*)DispListGet(pDisp, pItem);
            if ((pRoom->iFlags & 2) != 0)
            {
                HashNumDel(pHash, iRoomIdent);
                --pLobbyApi->iAutoRoomID;
                pRoom->iIdent = pLobbyApi->iAutoRoomID;
                DispListSet(pDisp, pItem, pRoom);
                HashNumAdd(pHash, pRoom->iIdent, pItem);
                LobbyApiListObjectUpdate(pLobbyApi, 0, pRoom, 0);
            }
            else
            {
                HashNumDel(pHash, iRoomIdent);
                pRoom = (LobbyApiRoomRecordPrefixT*)DispListDel(pDisp, pItem);
                LobbyApiListObjectUpdate(pLobbyApi, 0, pRoom, -1);
                DirtyMemFree(pRoom, KI_LOBBY_LIST_RECORD_MEMID,
                             pLobbyApi->iMemGroup);
            }
        }
    }
    return 0;
}

static s32 _LobbyApiAsyncPlayList(LobbyApiRefT* pLobbyApi, const char* pData,
                                  s32 iCode, s32 iListType,
                                  bool bNotifyWhenUnallocated,
                                  bool bRequireNonnegativeDelete)
{
    HasherRef* pHash = LobbyApiListGetHash(pLobbyApi, iListType);
    if (pHash == NULL)
    {
        if (bNotifyWhenUnallocated)
        {
            LobbyApiGlobalCallback(pLobbyApi, 0, LOBBYAPI_CBTYPE_EVNT,
                                   0x676c7374, iCode, pData); // glst
        }
        return 0;
    }

    DispListRef* pDisp = LobbyApiListGetDisp(pLobbyApi, iListType);
    if (TagFieldFind(pData, "NAME") != NULL)
    {
        LobbyApiPlayT* pPlay = (LobbyApiPlayT*)DirtyMemAlloc(
            sizeof(*pPlay), KI_LOBBY_LIST_RECORD_MEMID, pLobbyApi->iMemGroup);
        LobbyApiExtractPlayRecord(pPlay, pData);
        DispListT* pItem = (DispListT*)HashNumFind(pHash, pPlay->iIdent);
        s32 iOperation;
        if (pItem != NULL)
        {
            LobbyApiPlayT* pOldPlay = (LobbyApiPlayT*)DispListGet(pDisp, pItem);
            memcpy(pOldPlay, pPlay, sizeof(*pPlay));
            DispListSet(pDisp, pItem, pOldPlay);
            DirtyMemFree(pPlay, KI_LOBBY_LIST_RECORD_MEMID, pLobbyApi->iMemGroup);
            pPlay = pOldPlay;
            iOperation = 0;
        }
        else
        {
            pItem = DispListAdd(pDisp, pPlay);
            HashNumAdd(pHash, pPlay->iIdent, pItem);
            iOperation = 1;
        }
        LobbyApiListObjectUpdate(pLobbyApi, iListType, pPlay, iOperation);
    }
    else
    {
        const s32 iPlayIdent = TagFieldGetNumber(TagFieldFind(pData, "IDENT"), -1);
        if ((!bRequireNonnegativeDelete) || (iPlayIdent >= 0))
        {
            DispListT* pItem = (DispListT*)HashNumFind(pHash, iPlayIdent);
            if (pItem != NULL)
            {
                HashNumDel(pHash, iPlayIdent);
                LobbyApiPlayT* pPlay = (LobbyApiPlayT*)DispListDel(pDisp, pItem);
                LobbyApiListObjectUpdate(pLobbyApi, iListType, pPlay, -1);
                DirtyMemFree(pPlay, KI_LOBBY_LIST_RECORD_MEMID,
                             pLobbyApi->iMemGroup);
            }
        }
    }
    return 0;
}

static s32 _LobbyApiAsyncRoomsPopulation(LobbyApiRefT* pLobbyApi,
                                         const char* pData, s32 iCode)
{
    (void)iCode;
    if (LobbyApiListGetHash(pLobbyApi, 0) == NULL)
    {
        return 0;
    }

    char strRooms[768];
    char strPopulation[384];
    char strGamePopulation[384];
    TagFieldGetString(TagFieldFind(pData, "Z"), strRooms, sizeof(strRooms), "");
    TagFieldGetString(TagFieldFind(pData, "G"), strPopulation,
                      sizeof(strPopulation), "");
    TagFieldGetString(TagFieldFind(pData, "GC"), strGamePopulation,
                      sizeof(strGamePopulation), "");

    char* pRoomCursor = strRooms;
    char* pPopulationCursor = strPopulation;
    char* pGamePopulationCursor = strGamePopulation;
    while (*pRoomCursor != '\0')
    {
        const s32 iRoomIdent = (s8)*pRoomCursor;
        if (iRoomIdent < '0')
        {
            break;
        }

        s32 iRoomValue = 0;
        s32 iCharacter = (s8)*pRoomCursor;
        do
        {
            if (iCharacter > '9')
            {
                break;
            }
            ++pRoomCursor;
            iRoomValue = (iCharacter & 0xf) + 10 * iRoomValue;
            iCharacter = (s8)*pRoomCursor;
        }
        while (iCharacter >= '0');
        if (iRoomValue == 0)
        {
            break;
        }
        if (*pRoomCursor != '\0')
        {
            ++pRoomCursor;
        }

        s32 iPopulation = 0;
        while (((s8)*pRoomCursor >= '0') && ((s8)*pRoomCursor <= '9'))
        {
            iPopulation = (((s8)*pRoomCursor & 0xf) + 10 * iPopulation);
            ++pRoomCursor;
        }
        if (*pRoomCursor != '\0')
        {
            ++pRoomCursor;
        }

        s32 iGameCount = 0;
        while (((s8)*pPopulationCursor >= '0') &&
               ((s8)*pPopulationCursor <= '9'))
        {
            iGameCount = (((s8)*pPopulationCursor & 0xf) + 10 * iGameCount);
            ++pPopulationCursor;
        }
        if (*pPopulationCursor != '\0')
        {
            ++pPopulationCursor;
        }

        s32 iGamePopulation = 0;
        while (((s8)*pGamePopulationCursor >= '0') &&
               ((s8)*pGamePopulationCursor <= '9'))
        {
            iGamePopulation = (((s8)*pGamePopulationCursor & 0xf) +
                               10 * iGamePopulation);
            ++pGamePopulationCursor;
        }
        if (*pGamePopulationCursor != '\0')
        {
            ++pGamePopulationCursor;
        }

        LobbyApiPopul(pLobbyApi, iRoomValue, iPopulation,
                      iGameCount, iGamePopulation);
    }
    return 0;
}

static s32 _LobbyApiAsyncUsers(LobbyApiRefT* pLobbyApi, const char* pData, s32 iCode)
{
    (void)iCode;
    HasherRef* pHash = LobbyApiListGetHash(pLobbyApi, 1);
    if ((pHash == NULL) || (TagFieldGetNumber(TagFieldFind(pData, "I"), -1) < 0))
    {
        return 0;
    }

    DispListRef* pDisp = LobbyApiListGetDisp(pLobbyApi, 1);
    if (TagFieldFind(pData, "N") != NULL)
    {
        LobbyApiUserT* pUser = (LobbyApiUserT*)DirtyMemAlloc(
            sizeof(*pUser), KI_LOBBY_LIST_RECORD_MEMID, pLobbyApi->iMemGroup);
        LobbyApiExtractUserRecord(pUser, pData);
        DispListT* pItem = (DispListT*)HashNumFind(pHash, pUser->ident);
        s32 iOperation;
        if (pItem != NULL)
        {
            LobbyApiUserT* pOldUser = (LobbyApiUserT*)DispListGet(pDisp, pItem);
            memcpy(pOldUser, pUser, sizeof(*pUser));
            DispListSet(pDisp, pItem, pOldUser);
            DirtyMemFree(pUser, KI_LOBBY_LIST_RECORD_MEMID, pLobbyApi->iMemGroup);
            pUser = pOldUser;
            iOperation = 0;
        }
        else
        {
            pItem = DispListAdd(pDisp, pUser);
            HashNumAdd(pHash, pUser->ident, pItem);
            iOperation = 1;
        }
        if (((u32)pUser->flags & 0x200000) != 0)
        {
            memcpy(&pLobbyApi->user, pUser, sizeof(*pUser));
        }
        LobbyApiListObjectUpdate(pLobbyApi, 1, pUser, iOperation);
    }
    else
    {
        const s32 iUserIdent = TagFieldGetNumber(TagFieldFind(pData, "I"), -1);
        DispListT* pItem = (DispListT*)HashNumFind(pHash, iUserIdent);
        if (pItem != NULL)
        {
            HashNumDel(pHash, iUserIdent);
            LobbyApiUserT* pUser = (LobbyApiUserT*)DispListDel(pDisp, pItem);
            LobbyApiListObjectUpdate(pLobbyApi, 1, pUser, -1);
            DirtyMemFree(pUser, KI_LOBBY_LIST_RECORD_MEMID, pLobbyApi->iMemGroup);
        }
    }

    const s32 iGamePopulation = TagFieldGetNumber(TagFieldFind(pData, "R"), 0);
    const s32 iGameCount = TagFieldGetNumber(TagFieldFind(pData, "RG"), -1);
    const s32 iPopulation = TagFieldGetNumber(TagFieldFind(pData, "RGC"), -1);
    return LobbyApiPopul(pLobbyApi, pLobbyApi->roomidx,
                         iGamePopulation, iGameCount, iPopulation);
}

static s32 _LobbyApiAsyncUserSets(LobbyApiRefT* pLobbyApi, const char* pData,
                                  s32 iCode)
{
    (void)iCode;
    HasherRef* pHash = LobbyApiListGetHash(pLobbyApi, 9);
    if (pHash == NULL)
    {
        return 0;
    }

    LobbyApiUserSetT* pUserSet = (LobbyApiUserSetT*)DirtyMemAlloc(
        sizeof(*pUserSet), KI_LOBBY_LIST_RECORD_MEMID, pLobbyApi->iMemGroup);
    LobbyApiExtractUserSet(pUserSet, pData);
    DispListRef* pDisp = LobbyApiListGetDisp(pLobbyApi, 9);
    DispListT* pItem = (DispListT*)HashStrFind(pHash, pUserSet->strName);
    s32 iOperation;
    if (pItem != NULL)
    {
        LobbyApiUserSetT* pOldSet = (LobbyApiUserSetT*)DispListGet(pDisp, pItem);
        memcpy(pOldSet, pUserSet, sizeof(*pUserSet));
        DispListSet(pDisp, pItem, pOldSet);
        DirtyMemFree(pUserSet, KI_LOBBY_LIST_RECORD_MEMID, pLobbyApi->iMemGroup);
        pUserSet = pOldSet;
        iOperation = 0;
    }
    else
    {
        pItem = DispListAdd(pDisp, pUserSet);
        HashStrAdd(pHash, pUserSet->strName, pItem);
        iOperation = 1;
    }
    LobbyApiListObjectUpdate(pLobbyApi, 9, pUserSet, iOperation);
    return 0;
}

static s32 _LobbyApiAsyncUserSet(LobbyApiRefT* pLobbyApi, const char* pData,
                                 s32 iCode)
{
    const s32 iIdent = TagFieldGetNumber(TagFieldFind(pData, "I"), 0);
    if (TagFieldFind(pData, "N") != NULL)
    {
        if ((pLobbyApi->UserSet.iIdent == 0) ||
            (pLobbyApi->UserSet.iIdent == iIdent))
        {
            LobbyApiExtractUserSet(&pLobbyApi->UserSet, pData);
        }
    }
    else if (pLobbyApi->UserSet.iIdent == iIdent)
    {
        memset(&pLobbyApi->UserSet, 0, sizeof(pLobbyApi->UserSet));
    }
    LobbyApiGlobalCallback(pLobbyApi, 0, LOBBYAPI_CBTYPE_EVNT,
                           0x75736574, iCode, pData); // uset
    return 0;
}

static s32 _LobbyApiAsyncUserSetMembers(LobbyApiRefT* pLobbyApi,
                                        const char* pData, s32 iCode)
{
    (void)iCode;
    HasherRef* pHash = LobbyApiListGetHash(pLobbyApi, 10);
    if (pHash == NULL)
    {
        return 0;
    }

    DispListRef* pDisp = LobbyApiListGetDisp(pLobbyApi, 10);
    if (TagFieldFind(pData, "N") != NULL)
    {
        LobbyApiPrintfCode(pLobbyApi, "lobbyapiasync: received +usm - %s\n", pData);
        LobbyApiUserSetMemberT* pMember = (LobbyApiUserSetMemberT*)DirtyMemAlloc(
            sizeof(*pMember), KI_LOBBY_LIST_RECORD_MEMID, pLobbyApi->iMemGroup);
        LobbyApiExtractUserSetMember(pMember, pData);
        DispListT* pItem = (DispListT*)HashNumFind(pHash, pMember->iIdent);
        s32 iOperation;
        if (pItem != NULL)
        {
            LobbyApiUserSetMemberT* pOldMember =
                (LobbyApiUserSetMemberT*)DispListGet(pDisp, pItem);
            memcpy(pOldMember, pMember, sizeof(*pMember));
            DispListSet(pDisp, pItem, pOldMember);
            DirtyMemFree(pMember, KI_LOBBY_LIST_RECORD_MEMID, pLobbyApi->iMemGroup);
            pMember = pOldMember;
            iOperation = 0;
        }
        else
        {
            pItem = DispListAdd(pDisp, pMember);
            HashNumAdd(pHash, pMember->iIdent, pItem);
            iOperation = 1;
        }
        LobbyApiListObjectUpdate(pLobbyApi, 10, pMember, iOperation);
    }
    else
    {
        const s32 iIdent = TagFieldGetNumber(TagFieldFind(pData, "I"), 0);
        DispListT* pItem = (DispListT*)HashNumFind(pHash, iIdent);
        if (pItem != NULL)
        {
            HashNumDel(pHash, iIdent);
            LobbyApiUserSetMemberT* pMember =
                (LobbyApiUserSetMemberT*)DispListDel(pDisp, pItem);
            LobbyApiListObjectUpdate(pLobbyApi, 10, pMember, -1);
            DirtyMemFree(pMember, KI_LOBBY_LIST_RECORD_MEMID,
                         pLobbyApi->iMemGroup);
        }
    }

    // ARTIST's call target at 0x82AD5228 is the ICF-folded one-instruction
    // accessor lwz r3,4(r3); HasherRef::items is the field at that offset.
    pLobbyApi->UserSet.iCount = pHash->items;
    return pLobbyApi->UserSet.iCount;
}

static s32 _LobbyApiAsyncServerStats(LobbyApiRefT* pLobbyApi,
                                     const char* pData, s32 iCode)
{
    LobbyApiServerStatT* pStats = &pLobbyApi->serverStats;
    pStats->iUpdate = 1;
    pStats->uUsersInLobby = (u32)TagFieldGetNumber(
        TagFieldFind(pData, "UIL"), (s32)pStats->uUsersInLobby);
    pStats->uUsersInRooms = (u32)TagFieldGetNumber(
        TagFieldFind(pData, "UIR"), (s32)pStats->uUsersInRooms);
    pStats->uUsersInGames = (u32)TagFieldGetNumber(
        TagFieldFind(pData, "UIG"), (s32)pStats->uUsersInGames);
    pStats->uGamesInProgress = (u32)TagFieldGetNumber(
        TagFieldFind(pData, "GIP"), (s32)pStats->uGamesInProgress);
    pStats->uGamesCreated = (u32)TagFieldGetNumber(
        TagFieldFind(pData, "GCR"), (s32)pStats->uGamesCreated);
    pStats->uGamesCompleted = (u32)TagFieldGetNumber(
        TagFieldFind(pData, "GCM"), (s32)pStats->uGamesCompleted);
    LobbyApiGlobalCallback(pLobbyApi, 0, LOBBYAPI_CBTYPE_EVNT,
                           0x73746174, iCode, pData); // stat
    return 0;
}

static s32 _LobbyApiAsyncRanks(LobbyApiRefT* pLobbyApi, const char* pData,
                               s32 iCode)
{
    (void)iCode;
    const s32 iListType = pLobbyApi->snapchan;
    if ((iListType < 5) || (iListType > 8))
    {
        return 0;
    }

    HasherRef* pHash = LobbyApiListGetHash(pLobbyApi, iListType);
    if (pHash == NULL)
    {
        return 0;
    }

    LobbyApiRankRecordT* pRank = (LobbyApiRankRecordT*)DirtyMemAlloc(
        sizeof(*pRank), KI_LOBBY_LIST_RECORD_MEMID, pLobbyApi->iMemGroup);
    LobbyApiExtractRankRecord(pRank, pData, pLobbyApi->user.name);
    DispListRef* pDisp = LobbyApiListGetDisp(pLobbyApi, iListType);
    DispListT* pItem = (DispListT*)HashStrFind(pHash, pRank->strName);
    s32 iOperation;
    if (pItem != NULL)
    {
        LobbyApiRankRecordT* pOldRank = (LobbyApiRankRecordT*)DispListGet(pDisp, pItem);
        memcpy(pOldRank, pRank, sizeof(*pRank));
        DispListSet(pDisp, pItem, pOldRank);
        DirtyMemFree(pRank, KI_LOBBY_LIST_RECORD_MEMID, pLobbyApi->iMemGroup);
        pRank = pOldRank;
        iOperation = 0;
    }
    else
    {
        pItem = DispListAdd(pDisp, pRank);
        HashStrAdd(pHash, pRank->strName, pItem);
        iOperation = 1;
    }
    LobbyApiListObjectUpdate(pLobbyApi, iListType, pRank, iOperation);
    return 0;
}

static s32 _LobbyApiAsyncGameList4(LobbyApiRefT* pLobbyApi,
                                   const char* pData, s32 iCode)
{
    return _LobbyApiAsyncPlayList(pLobbyApi, pData, iCode, 4, false, true);
}

static s32 _LobbyApiAsyncGameList3(LobbyApiRefT* pLobbyApi,
                                   const char* pData, s32 iCode)
{
    return _LobbyApiAsyncPlayList(pLobbyApi, pData, iCode, 3, true, false);
}

typedef s32 (LobbyApiAsyncHandlerT)(LobbyApiRefT*, const char*, s32);

struct LobbyApiAsyncEntryT
{
    s32 iRequestKind;
    s32 iEventKind;
    LobbyApiAsyncHandlerT* pHandler;
};

static const LobbyApiAsyncEntryT _LobbyApiAsyncEntries[] =
{
    { 0x2b736573, 0x706c6179, NULL }, // +ses -> play
    { 0x2b6d676d, 0x67616d65, NULL }, // +mgm -> game
    { 0x2b6d7367, 0, _LobbyApiAsyncChat },
    { 0x2b77686f, 0, _LobbyApiAsyncWho },
    { 0x2b726f6d, 0, _LobbyApiAsyncRooms },
    { 0x2b706f70, 0, _LobbyApiAsyncRoomsPopulation },
    { 0x2b757372, 0, _LobbyApiAsyncUsers },
    { 0x2b757373, 0, _LobbyApiAsyncUserSets },
    { 0x2b757374, 0, _LobbyApiAsyncUserSet },
    { 0x2b75736d, 0, _LobbyApiAsyncUserSetMembers },
    { 0x2b736e70, 0, _LobbyApiAsyncRanks },
    { 0x2b61676d, 0, _LobbyApiAsyncGameList4 },
    { 0x2b67616d, 0, _LobbyApiAsyncGameList3 },
    { 0x2b737374, 0, _LobbyApiAsyncServerStats },
    { 0x2b627564, 0x62756464, NULL }, // +bud -> budd
    { 0x2b6d6c67, 0x6d6c6f67, NULL }, // +mlg -> mlog
    { 0x2b6b6f68, 0x6b6f7468, NULL }, // +koh -> koth
    { 0x2b6b696b, 0x6b69636b, NULL }, // +kik -> kick
    { 0x2b63746c, 0x6374726c, NULL }, // +ctl -> ctrl
    { 0x2b696c73, 0x696c7367, NULL }, // +ils -> ilsg
    { 0, 0, NULL }
};

extern "C" s32 LobbyApiAsyncProcess(LobbyApiRefT* pLobbyApi, const char* pData,
                                      s32 iKind, s32 iCode)
{
    const LobbyApiAsyncEntryT* pEntry = _LobbyApiAsyncEntries;
    while (pEntry->iRequestKind != 0)
    {
        if (pEntry->iRequestKind == iKind)
        {
            if (pEntry->pHandler != NULL)
            {
                return pEntry->pHandler(pLobbyApi, pData, iCode);
            }
            LobbyApiGlobalCallback(pLobbyApi, 0, LOBBYAPI_CBTYPE_EVNT,
                                   pEntry->iEventKind, iCode, pData);
            return 0;
        }
        ++pEntry;
    }

    return LobbyApiPrintfCode(
        pLobbyApi,
        "lobbyapi: warning, unhandled async server message '%c%c%c%c'\n",
        ((u32)iKind >> 24) & 0xff, ((u32)iKind >> 16) & 0xff,
        ((u32)iKind >> 8) & 0xff, (u32)iKind & 0xff);
}

CGS_STATIC_ASSERT(sizeof(XOVERLAPPED) == 28, "XDK XOVERLAPPED size");
CGS_STATIC_ASSERT(sizeof(XUSER_CONTEXT) == 8, "XDK XUSER_CONTEXT size");
CGS_STATIC_ASSERT(sizeof(XSESSION_SEARCHRESULT) == 92,
                  "XDK XSESSION_SEARCHRESULT stride");
CGS_STATIC_ASSERT(offsetof(XSESSION_SEARCHRESULT, info) +
                  offsetof(XSESSION_INFO, hostAddress) +
                  offsetof(XNADDR, abEnet) == 0x12,
                  "XDK XNADDR abEnet ARTIST offset");

static const s32 KI_LOBBY_XENON_MEMID = 0x6c617070; // lapp

static s32 _LobbyApiXenonApplyFilter(LobbyApiXenonRefT* pXenon, s32 iKind,
                                     const char* pField, const char* pRequest,
                                     s32 iFilterAction, s32 iCallbackID)
{
    char strProfane[32];
    memset(strProfane, 0, sizeof(strProfane));
    const char* pConfig = (const char*)LobbyApiInfo(pXenon->pLobbyApi, 0x636f6e66); // conf
    TagFieldGetString(TagFieldFind(pConfig, "PROFANE_STRING"), strProfane,
                      sizeof(strProfane), "");
    ProtoFilterControl(pXenon->pProtoFilter, 0x70737472, 0, 0, strProfane); // pstr
    ProtoFilterApplyFilter(pXenon->pProtoFilter, iFilterAction,
                           pXenon->strFilterText);

    char strModifiedRequest[1024];
    TagFieldDupl(strModifiedRequest, sizeof(strModifiedRequest), pRequest);
    TagFieldSetString(strModifiedRequest, sizeof(strModifiedRequest), pField,
                      pXenon->strFilterText);
    const char* pWireRequest = strModifiedRequest;

    if ((u32)iKind == 0x726f6f6d) // room
    {
        char strCategory[32];
        char strName[32];
        char strQualifiedName[64];
        TagFieldGetString(TagFieldFind(strModifiedRequest, "CAT"),
                          strCategory, sizeof(strCategory), "");
        TagFieldGetString(TagFieldFind(strModifiedRequest, "NAME"),
                          strName, sizeof(strName), "");
        ds_snzprintf(strQualifiedName, sizeof(strQualifiedName), "%s.%s",
                     strCategory, strName);

        char strRoomRequest[256];
        TagFieldDupl(strRoomRequest, sizeof(strRoomRequest), strModifiedRequest);
        TagFieldSetString(strRoomRequest, sizeof(strRoomRequest), "NAME",
                          strQualifiedName);
        TagFieldDelete(strRoomRequest, "CAT");
        pWireRequest = strRoomRequest;
        LobbyApiAriesRequest(pXenon->pLobbyApi, iKind, pWireRequest, iCallbackID);
        return 0;
    }

    LobbyApiAriesRequest(pXenon->pLobbyApi, iKind, pWireRequest, iCallbackID);
    return 0;
}

static s32 _LobbyApiXenonQueueFilter(LobbyApiXenonRefT* pXenon, s32 iKind,
                                     const char* pRequest, const char* pField,
                                     s32 iFilterAction, s32 iCallbackID)
{
    s32 iSlot = 0;
    while ((iSlot < 2) && pXenon->aFilterRequests[iSlot].bPending)
    {
        ++iSlot;
    }

    if (iSlot >= 2)
    {
        NetPrintfCode("lobbyapi: error initiating xenon string verification - text will be squelched\n");
        _LobbyApiXenonApplyFilter(pXenon, iKind, pField, pRequest, 1, iCallbackID);
        return 1;
    }

    LobbyApiXenonFilterRequestT* pRequestState = &pXenon->aFilterRequests[iSlot];
    pRequestState->iCallbackID = iCallbackID;
    pRequestState->iKind = iKind;
    TagFieldDupl(pRequestState->strRequest, sizeof(pRequestState->strRequest), pRequest);
    ds_strnzcpy(pRequestState->strField, pField, sizeof(pRequestState->strField));
    pRequestState->bSquelchOnError = (u8)iFilterAction;

    TagFieldGetString(TagFieldFind(pRequest, pField), pXenon->strFilterText,
                      sizeof(pXenon->strFilterText), "");
    NetPrintfCode("lobbyapixenon: [%d] validating field %s in '%c%c%c%c' request\n",
                  iSlot, pField, ((u32)iKind >> 24) & 0xff,
                  ((u32)iKind >> 16) & 0xff, ((u32)iKind >> 8) & 0xff,
                  (u32)iKind & 0xff);
    const s32 iRequestResult = ProtoFilterRequest(
        pXenon->pProtoFilter, pXenon->strFilterText,
        &pRequestState->iRequest);
    if (iRequestResult != -1)
    {
        NetPrintfCode("lobbyapi: error initiating xenon string verification - text will be squelched\n");
        _LobbyApiXenonApplyFilter(pXenon, iKind, pField, pRequest, 1, iCallbackID);
    }
    else
    {
        pRequestState->bPending = 1;
    }
    return 1;
}

static s32 _LobbyApiXenonSearch(LobbyApiXenonRefT* pXenon, s32 iKind,
                                const char* pRequest, s32 iCallbackID)
{
    LobbyApiXenonSearchStateT* pSearch = &pXenon->Search;
    if (pSearch->bPending != 0)
    {
        return 0;
    }

    const char* pConfig = (const char*)LobbyApiInfo(pXenon->pLobbyApi,
                                                    0x636f6e66); // conf
    const char* pMachint = TagFieldFind(pConfig, "MACHINT");
    const s32 iMaxResults = TagFieldGetNumber(pMachint, 5);
    if ((iMaxResults == 0) || (TagFieldFind(pRequest, "MACHINT") != NULL))
    {
        return 0;
    }

    const s32 iRequestMode = TagFieldGetNumber(TagFieldFind(pRequest, "XNMODE"), 0);
    const s32 iMode = TagFieldGetNumber(TagFieldFind(pConfig, "XNMODE"), iRequestMode);
    const s32 iRequestProcedure = TagFieldGetNumber(
        TagFieldFind(pRequest, "XNPROC"), 0);
    const s32 iProcedure = TagFieldGetNumber(TagFieldFind(pConfig, "XNPROC"),
                                             iRequestProcedure);
    const s32 bRank = (TagFieldGetNumber(TagFieldFind(pRequest, "XNRANK"), 1) != 0);
    s32 iContextCount = TagFieldGetNumber(TagFieldFind(pRequest, "XNCONTEXTS"), 0);
    if (iContextCount > 10)
    {
        NetPrintfCode("lobbyapixenon: clamping %d contexts to max %d\n",
                      iContextCount, 10);
        iContextCount = 10;
    }

    ds_strnzcpy(pSearch->strRequest, pRequest, sizeof(pSearch->strRequest));
    char strUserName[16];
    LobbyApiStatus(pXenon->pLobbyApi, 0x70657273, strUserName, sizeof(strUserName));
    const s32 iUserIndex = NetConnQuery(strUserName, 0);
    if (iUserIndex < 0)
    {
        NetPrintfCode("lobbyapixenon: unable to dereference user index for session search - user not currently logged in?\n");
        return 0;
    }

    DWORD uRequiredBufferSize = 0;
    const DWORD uSearchResult = XSessionSearch(
        (DWORD)iProcedure, (DWORD)iUserIndex, (DWORD)iMaxResults,
        0, 0, 0, NULL, &uRequiredBufferSize, NULL, NULL);
    if ((uSearchResult != ERROR_INSUFFICIENT_BUFFER) ||
        (uRequiredBufferSize == 0))
    {
        NetPrintfCode("lobbyapixenon: XSessionSearch() failed: %s\n",
                      DirtyErrGetName((s32)uSearchResult));
        return 0;
    }

    if (uRequiredBufferSize != pSearch->uResultsBufferSize)
    {
        if (pSearch->pResultsBuffer != NULL)
        {
            NetPrintfCode("lobbyapixenon: freeing old search buffer (size=%d)\n",
                          pSearch->uResultsBufferSize);
            DirtyMemFree(pSearch->pResultsBuffer, KI_LOBBY_XENON_MEMID,
                         pXenon->iMemGroup);
        }
        NetPrintfCode("lobbyapixenon: allocating new search buffer (size=%d)\n",
                      uRequiredBufferSize);
        pSearch->pResultsBuffer = DirtyMemAlloc(
            (s32)uRequiredBufferSize, KI_LOBBY_XENON_MEMID, pXenon->iMemGroup);
        pSearch->uResultsBufferSize = uRequiredBufferSize;
    }
    memset(pSearch->pResultsBuffer, 0, pSearch->uResultsBufferSize);

    if (iContextCount <= 0)
    {
        iContextCount = 2;
        pSearch->aContexts[0].uContextID = 0x800a;
        pSearch->aContexts[0].uValue = (u32)!bRank;
        pSearch->aContexts[1].uContextID = 0x800b;
        pSearch->aContexts[1].uValue = (u32)iMode;
    }
    else
    {
        for (s32 iContext = 0; iContext < iContextCount; ++iContext)
        {
            char strContextName[32];
            ds_snzprintf(strContextName, sizeof(strContextName),
                         "XNCONTEXTID%d", iContext);
            pSearch->aContexts[iContext].uContextID = (u32)TagFieldGetNumber(
                TagFieldFind(pRequest, strContextName), 0);
            ds_snzprintf(strContextName, sizeof(strContextName),
                         "XNCONTEXTVALUE%d", iContext);
            pSearch->aContexts[iContext].uValue = (u32)TagFieldGetNumber(
                TagFieldFind(pRequest, strContextName), 0);
        }
    }

    const DWORD uStartResult = XSessionSearch(
        (DWORD)iProcedure, (DWORD)iUserIndex, (DWORD)iMaxResults,
        0, (WORD)iContextCount, NULL,
        (PXUSER_CONTEXT)pSearch->aContexts,
        &pSearch->uResultsBufferSize,
        (PXSESSION_SEARCHRESULT_HEADER)pSearch->pResultsBuffer,
        (PXOVERLAPPED)pSearch->aOverlapped);
    if (uStartResult != ERROR_IO_PENDING)
    {
        NetPrintfCode("lobbyapixenon: XSessionSearch() failed: %s\n",
                      DirtyErrGetName((s32)uStartResult));
        return 0;
    }

    NetPrintfCode("lobbyapixenon: search started\n");
    pSearch->iKind = iKind;
    pSearch->iCallbackID = iCallbackID;
    pSearch->bPending = 1;
    return 1;
}

static s32 _LobbyApiXenonSnoopMessageRequest(LobbyApiXenonRefT* pXenon,
                                              const char* pRequest,
                                              s32 iCallbackID)
{
    const u32 uAttributes = (u32)TagFieldGetFlags(TagFieldFind(pRequest, "ATTR"), 0);
    if ((uAttributes & 8) == 0)
    {
        return 0;
    }
    if ((uAttributes & 0x4000) != 0)
    {
        return 0;
    }

    BOOL bPrivilege = FALSE;
    if (XUserCheckPrivilege(0xff, (XPRIVILEGE_TYPE)0xfc, &bPrivilege) != ERROR_SUCCESS)
    {
        NetPrintfCode("lobbyapixenon: error trying to retrieve communications privilege\n");
        return 0;
    }
    if (!bPrivilege)
    {
        NetPrintfCode("lobbyapixenon: squelching message as user does not have communications privilege\n");
        return -1;
    }

    const char* pConfig = (const char*)LobbyApiInfo(pXenon->pLobbyApi,
                                                    0x636f6e66); // conf
    if (TagFieldGetNumber(TagFieldFind(pConfig, "XNFILT"), 1) == 0)
    {
        return 0;
    }
    return _LobbyApiXenonQueueFilter(pXenon, 0x6d657367, pRequest,
                                     "TEXT", 1, iCallbackID); // msg
}

static s32 _LobbyApiXenonSnoopRoomRequest(LobbyApiXenonRefT* pXenon,
                                           const char* pRequest,
                                           s32 iCallbackID)
{
    if (TagFieldGetNumber(TagFieldFind(pRequest, "IGNPROFANE"), 0) == 1)
    {
        return 0;
    }

    char strModifiedRequest[256];
    const char* pWireRequest = pRequest;
    if (TagFieldFind(pRequest, "CAT") == NULL)
    {
        TagFieldDupl(strModifiedRequest, sizeof(strModifiedRequest), pRequest);
        char strQualifiedName[64];
        TagFieldGetString(TagFieldFind(strModifiedRequest, "NAME"),
                          strQualifiedName, sizeof(strQualifiedName), "");
        char* pSeparator = strchr(strQualifiedName, '.');
        if (pSeparator != NULL)
        {
            *pSeparator = '\0';
            TagFieldSetString(strModifiedRequest, sizeof(strModifiedRequest),
                              "CAT", strQualifiedName);
            TagFieldSetString(strModifiedRequest, sizeof(strModifiedRequest),
                              "NAME", pSeparator + 1);
            pWireRequest = strModifiedRequest;
        }
    }
    return _LobbyApiXenonQueueFilter(pXenon, 0x726f6f6d, pWireRequest,
                                     "NAME", 2, iCallbackID); // room
}

extern "C" LobbyApiXenonRefT* LobbyApiXenonCreate(LobbyApiRefT* pLobbyApi)
{
    const s32 iMemGroup = DirtyMemGroupQuery();
    LobbyApiXenonRefT* pXenon = (LobbyApiXenonRefT*)DirtyMemAlloc(
        sizeof(*pXenon), KI_LOBBY_XENON_MEMID, iMemGroup);
    if (pXenon == NULL)
    {
        NetPrintfCode("lobbyapixenon: unable to allocate module state\n");
        return NULL;
    }

    memset(pXenon, 0, sizeof(*pXenon));
    pXenon->iMemGroup = iMemGroup;
    pXenon->pLobbyApi = pLobbyApi;
    pXenon->pProtoFilter = ProtoFilterCreate();
    return pXenon;
}

extern "C" void LobbyApiXenonDestroy(LobbyApiXenonRefT* pXenon)
{
    if (pXenon->pProtoFilter != NULL)
    {
        ProtoFilterDestroy(pXenon->pProtoFilter);
    }
    if (pXenon->Search.pResultsBuffer != NULL)
    {
        DirtyMemFree(pXenon->Search.pResultsBuffer, KI_LOBBY_XENON_MEMID,
                     pXenon->iMemGroup);
    }
    DirtyMemFree(pXenon, KI_LOBBY_XENON_MEMID, pXenon->iMemGroup);
}

extern "C" void LobbyApiXenonDisconnect(LobbyApiXenonRefT* pXenon)
{
    if (pXenon->pProtoFilter != NULL)
    {
        ProtoFilterRequestCancel(pXenon->pProtoFilter, -1);
    }
    memset(pXenon->aFilterRequests, 0, 2144);
    if (pXenon->Search.bPending != 0)
    {
        if (XCancelOverlapped((PXOVERLAPPED)pXenon->Search.aOverlapped) != 0)
        {
            NetPrintfCode("lobbyapixenon: error cancelling overlapped operation\n");
        }
        pXenon->Search.bPending = 0;
    }
    memset(&pXenon->Search, 0, 2144);
}

extern "C" s32 LobbyApiXenonSnoopRequest(LobbyApiXenonRefT* pXenon, s32 iKind,
                                           const char* pRequest, s32 iCallbackID)
{
    if (((u32)iKind == 0x6771776b) || ((u32)iKind == 0x67737461)) // gqwk/gsta
    {
        return _LobbyApiXenonSearch(pXenon, iKind, pRequest, iCallbackID);
    }
    if ((u32)iKind == 0x6d657367) // msg
    {
        return _LobbyApiXenonSnoopMessageRequest(pXenon, pRequest, iCallbackID);
    }
    if ((u32)iKind == 0x726f6f6d) // room
    {
        return _LobbyApiXenonSnoopRoomRequest(pXenon, pRequest, iCallbackID);
    }
    return 0;
}

static s32 _LobbyApiXenonSnoopMessageResponse(LobbyApiXenonRefT* pXenon,
                                               const char* pData)
{
    BOOL bMuted = FALSE;
    if (((u32)TagFieldGetFlags(TagFieldFind(pData, "F"), 0) & 8) != 0)
    {
        char strUserName[16];
        LobbyApiStatus(pXenon->pLobbyApi, 0x70657273,
                       strUserName, sizeof(strUserName)); // pers
        const s32 iUserIndex = NetConnQuery(strUserName, 0);
        if (iUserIndex >= 0)
        {
            BOOL bPrivilege = FALSE;
            if (XUserCheckPrivilege(0xff, (XPRIVILEGE_TYPE)0xfc,
                                    &bPrivilege) != ERROR_SUCCESS)
            {
                NetPrintfCode("lobbyapixenon: error trying to retrieve communications privilege\n");
            }
            else
            {
                bMuted = (bPrivilege == FALSE);
            }
            if (!bMuted)
            {
                XUID xuidRemoteTalker = 0;
                TagFieldGetBinary(TagFieldFind(pData, "U"),
                                  &xuidRemoteTalker, sizeof(xuidRemoteTalker));
                XUserMuteListQuery((DWORD)iUserIndex, xuidRemoteTalker, &bMuted);
            }
        }
    }

    if (bMuted)
    {
        char strUserName[16];
        TagFieldGetString(TagFieldFind(pData, "N"), strUserName,
                          sizeof(strUserName), "");
        NetPrintfCode("lobbyapi: muting message from user %s\n", strUserName);
    }
    return bMuted ? 1 : 0;
}

extern "C" s32 LobbyApiXenonSnoopResponse(LobbyApiXenonRefT* pXenon,
                                            s32 iKind, s32 iCode,
                                            const char* pData)
{
    (void)iCode;
    if ((u32)iKind == 0x2b6d7367) // +msg
    {
        return _LobbyApiXenonSnoopMessageResponse(pXenon, pData);
    }
    return 0;
}

extern "C" s32 LobbyApiXenonUpdate(LobbyApiXenonRefT* pXenon)
{
    s32 iResult = 0;
    for (s32 iIndex = 0; iIndex < 2; ++iIndex)
    {
        LobbyApiXenonFilterRequestT* pRequest = &pXenon->aFilterRequests[iIndex];
        if (pRequest->bPending != 0)
        {
            const s32 iFilterResult = ProtoFilterResult(
                pXenon->pProtoFilter, pRequest->iRequest);
            if (iFilterResult != -1)
            {
                NetPrintfCode("lobbyapixenon: [%d] message validation complete\n",
                              iIndex);
                const s32 iAction = (iFilterResult != 0)
                    ? pRequest->bSquelchOnError : 0;
                _LobbyApiXenonApplyFilter(pXenon, pRequest->iKind,
                                          pRequest->strField,
                                          pRequest->strRequest, iAction,
                                          pRequest->iCallbackID);
                pRequest->bPending = 0;
            }
        }
    }

    LobbyApiXenonSearchStateT* pSearch = &pXenon->Search;
    if (pSearch->bPending != 0)
    {
        const DWORD uSearchStatus = XGetOverlappedExtendedError(
            (PXOVERLAPPED)pSearch->aOverlapped);
        if (uSearchStatus != ERROR_IO_INCOMPLETE)
        {
            NetPrintfCode("lobbyapixenon: search complete\n");
            XSESSION_SEARCHRESULT_HEADER* pResults =
                (XSESSION_SEARCHRESULT_HEADER*)pSearch->pResultsBuffer;
            const s32 iResultCount = (s32)pResults->dwSearchResults;
            if (iResultCount >= 0)
            {
                TagFieldPrintf(pSearch->strRequest,
                               sizeof(pSearch->strRequest), "~MACHINT=");
                for (s32 iIndex = 0; iIndex < iResultCount; ++iIndex)
                {
                    char strMachineAddress[16];
                    const BYTE* pEthernet =
                        pResults->pResults[iIndex].info.hostAddress.abEnet;
                    ds_snzprintf(strMachineAddress, sizeof(strMachineAddress),
                                 "$%02x%02x%02x%02x%02x%02x",
                                 pEthernet[0], pEthernet[1], pEthernet[2],
                                 pEthernet[3], pEthernet[4], pEthernet[5]);
                    TagFieldPrintf(pSearch->strRequest,
                                   sizeof(pSearch->strRequest), ",%s",
                                   strMachineAddress);
                }
            }
            LobbyApiAriesRequest(pXenon->pLobbyApi, pSearch->iKind,
                                 pSearch->strRequest, pSearch->iCallbackID);
            iResult = 0;
            pSearch->bPending = 0;
        }
    }
    return iResult;
}
