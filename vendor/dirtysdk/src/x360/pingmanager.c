// Native DirtySDK 5.5.3 protopingmanager.c. State declarations follow DecFIGS and ARTIST.
// ARTIST uses compact loop and slot arithmetic here (82B27BCC and 82B27B40).
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

#include "pingmanager.h"
#include "dirtyaddr.h"
#include "dirtylib.h"
#include "dirtymem.h"
#include <string.h>
#include <stddef.h>
typedef struct ProtoPingRefT ProtoPingRefT;
typedef struct ServerPingRecT ServerPingRecT;
struct ServerPingRecT
{
    PingManagerCallbackT* pServCb;
    void* pServCbData;
    u32 uSeqn;
};
typedef struct PingRecT PingRecT;
struct PingRecT
{
    DirtyAddrT Addr;
    u16 uPing;
    u8 uCreatedTime;
    u8 uTouchedTime;
    PingManagerCallbackT* pCallback;
    void* pCallbackData;
};
struct PingManagerRefT
{
    s32 iMemGroup;
    u32 uMaxRecords, uNumRecords;
    u8 uTime;
    u32 uTickCount, uLastPingTick;
    ServerPingRecT ServerPings[8];
    ProtoPingRefT* pPingRef;
    PingRecT PingCache[1];
};
CGS_STATIC_ASSERT(sizeof(ServerPingRecT) == 12, "ARTIST server record stride");
CGS_STATIC_ASSERT(offsetof(PingManagerRefT, ServerPings) == 0x18, "ARTIST server ping records");
CGS_STATIC_ASSERT(offsetof(PingManagerRefT, pPingRef) == 0x78, "ARTIST ProtoPing owner");
CGS_STATIC_ASSERT(sizeof(PingRecT) == 0x4c, "ARTIST cache record stride");
CGS_STATIC_ASSERT(sizeof(PingManagerRefT) == 0xc8, "ARTIST manager with first cache record");
void PingManagerCancelServerRequest(PingManagerRefT* pRef, u32 uSeqn)
{
    ServerPingRecT* pServerPing = pRef->ServerPings;
    s32 iServerPing;
    for (iServerPing = 0; iServerPing < 8; ++iServerPing, ++pServerPing)
    {
        if (pServerPing->uSeqn == uSeqn || uSeqn == 0)
        {
            pServerPing->pServCb = 0;
            pServerPing->pServCbData = 0;
            pServerPing->uSeqn = 0;
        }
    }
    if (uSeqn)
        NetPrintfCode("pingmanager: could not find server request with seqn=%d to cancel\n", uSeqn);
}

u32 NetTick(void);
ProtoPingRefT* ProtoPingCreate(s32 iMaxPings);
s32 ProtoPingControl(ProtoPingRefT* pRef, s32 iControl, s32 iValue, void* pValue);
s32 ProtoPingRequestServer(ProtoPingRefT* pRef, u32 uAddress, const unsigned char* pData, s32 iLength, s32 iTimeout, s32 iTtl);
// DecFIGS protopingmanager.c:108 and :111. ARTIST accesses the adjacent singleton
// and reference count through one base at 8324EAB8.
static struct
{
    PingManagerRefT* pRef;
    s32 iRefs;
} _PingManager_State;

PingManagerRefT* PingManagerCreate(u32 uMaxRecords)
{
    const s32 iMemGroup = DirtyMemGroupQuery();
    PingManagerRefT* pRef;
    u32 uSize;
    if (_PingManager_State.pRef)
    {
        _PingManager_State.iRefs += 1;
        return _PingManager_State.pRef;
    }
    if (uMaxRecords < 16)
    {
        NetPrintfCode("pingmanager: warning -- create call specifies a max record size that is too small\n");
        uMaxRecords = 16;
    }
    uSize = sizeof(PingManagerRefT) + sizeof(PingRecT) * (uMaxRecords - 1);
    pRef = (PingManagerRefT*)DirtyMemAlloc(uSize, 0x6c706d67, iMemGroup);
    if (!pRef) return 0;
    memset(pRef, 0, uSize);
    pRef->iMemGroup = iMemGroup;
    pRef->uMaxRecords = uMaxRecords;
    pRef->uTickCount = pRef->uLastPingTick = NetTick();
    pRef->pPingRef = ProtoPingCreate(0);
    if (!pRef->pPingRef)
    {
        DirtyMemFree(pRef, 0x6c706d67, pRef->iMemGroup);
        return 0;
    }
    ProtoPingControl(pRef->pPingRef, 0x74696d65, 4, 0);
    _PingManager_State.pRef = pRef;
    _PingManager_State.iRefs = 1;
    return pRef;
}

s32 PingManagerPingServer2(PingManagerRefT* pRef, u32 uAddress, PingManagerCallbackT* pCallback, void* pData)
{
    ServerPingRecT* pServerPing = 0;
    s32 iServerPing;
    for (iServerPing = 0; iServerPing < 8; ++iServerPing)
    {
        if (pRef->ServerPings[iServerPing].uSeqn == 0)
        {
            pServerPing = &pRef->ServerPings[iServerPing];
            break;
        }
    }
    if (!pServerPing)
    {
        NetPrintfCode("pingmanager: server ping record overflow\n");
        return -1;
    }
    pServerPing->pServCb = pCallback;
    pServerPing->pServCbData = pData;
    pServerPing->uSeqn = ProtoPingRequestServer(pRef->pPingRef, uAddress, 0, 0, 0, 0);
    return pServerPing->uSeqn;
}

// ARTIST 82B27BF0 clears the address slot and forwards the callback and data.
s32 PingManagerPingServer(PingManagerRefT* pRef, u32 uAddress,
                                     PingManagerCallbackT* pCallback, void* pData)
{
    (void)uAddress;
    return PingManagerPingServer2(pRef, 0, pCallback, pData);
}

// ARTIST 82B27798; singleton references are shared with PingManagerCreate.
void ProtoPingDestroy(ProtoPingRefT* pRef);
void PingManagerDestroy(PingManagerRefT* pRef)
{
    if (--_PingManager_State.iRefs <= 0)
    {
        ProtoPingDestroy(pRef->pPingRef);
        DirtyMemFree(pRef, 0x6c706d67, pRef->iMemGroup);
        _PingManager_State.pRef = 0;
        _PingManager_State.iRefs = 0;
    }
}

typedef struct ProtoPingResponseT ProtoPingResponseT;
struct ProtoPingResponseT
{
    DirtyAddrT Addr;
    u32 uAddr;
    s16 iPing;
    u16 uSeqn, uNumRetries;
    u8 uType, bServer;
};
s32 ProtoPingRequest(ProtoPingRefT*, DirtyAddrT*, const unsigned char*, s32, s32, s32);
s32 ProtoPingResponse(ProtoPingRefT*, unsigned char*, s32*, ProtoPingResponseT*);
const char* SocketInAddrGetText(u32 uAddr);

// DecFIGS protopingmanager.c:134. ARTIST 82B279DC through 82B27A40
// inlines the fixed-width address lookup into the response handler.
static __forceinline s32 _PingManagerFindRecordByAddress(PingManagerRefT* pRef, DirtyAddrT* pAddr)
{
    u32 uRecord;
    for (uRecord = 0; uRecord < pRef->uNumRecords; ++uRecord)
    {
        if (memcmp(&pRef->PingCache[uRecord].Addr, pAddr, sizeof(*pAddr)) == 0)
            return uRecord;
    }
    return -1;
}

// DecFIGS protopingmanager.c:196. ARTIST inlines both cache invalidation stores.
static void _PingManagerInvalidateRecord(PingManagerRefT* pRef, PingRecT* pRecord)
{
    pRecord->uPing = 0x8000;
    pRecord->uCreatedTime = pRecord->uTouchedTime = (u8)(pRef->uTime - 1);
}

// DecFIGS protopingmanager.c:218. Whole seconds advance the eight-bit cache clock.
static void _PingManagerUpdateTimer(PingManagerRefT* pRef, u32 uCurTick)
{
    const u32 uTimeElapsed = (uCurTick - pRef->uTickCount) / 1000;
    if (uTimeElapsed)
    {
        pRef->uTime += (u8)uTimeElapsed;
        pRef->uTickCount += 1000 * uTimeElapsed;
    }
}

// ARTIST 82B27808. Consume the existing ProtoPing completion queue and update the cache.
void PingManagerUpdate(PingManagerRefT* pRef)
{
    u32 uRecord, uPingCount, uMaxPingIssue;
    ProtoPingResponseT response;
    PingRecT* pPingCache = pRef->PingCache;
    PingRecT* pRecord;
    const u32 uCurTick = NetTick();
    s32 iIndex;
    _PingManagerUpdateTimer(pRef, uCurTick);

    uRecord = 0;
    uMaxPingIssue = (uCurTick - pRef->uLastPingTick) / 100;
    uPingCount = 0;
    for (pRecord = pPingCache; uRecord < pRef->uNumRecords; ++uRecord, ++pRecord)
    {
        if (pRecord->uCreatedTime == pRef->uTime)
        {
            // ARTIST 82B27894 wraps the age at the cache clock width.
            if ((u8)(pRef->uTime - pRecord->uTouchedTime) >= 60)
            {
                memcpy(pRecord, &pPingCache[pRef->uNumRecords - 1], sizeof(*pRecord));
                --pRef->uNumRecords;
            }
            else
            {
                _PingManagerInvalidateRecord(pRef, pRecord);
            }
        }
        if ((pRecord->uPing & 0x8000) != 0 && uPingCount < uMaxPingIssue)
        {
            ProtoPingRequest(pRef->pPingRef, &pRecord->Addr, 0, 0, 0, 0);
            ++uPingCount;
            pRecord->uPing = (pRecord->uPing & 0x0fff) | 0x4000;
            pRecord->uCreatedTime = pRecord->uTouchedTime = (u8)(pRef->uTime - 1);
            pRef->uLastPingTick = uCurTick;
        }
    }
    while (ProtoPingResponse(pRef->pPingRef, 0, 0, &response))
    {
        if (response.bServer == 1)
        {
            s32 iServerPing = 0;
            ServerPingRecT* pServerPing = pRef->ServerPings;
            for (; iServerPing < 8; ++iServerPing, ++pServerPing)
            {
                if (pServerPing->uSeqn == response.uSeqn)
                {
                    if (pServerPing->pServCb)
                        pServerPing->pServCb(&response.Addr, response.iPing, pServerPing->pServCbData);
                    pServerPing->uSeqn = 0;
                    break;
                }
            }
            continue;
        }
        iIndex = _PingManagerFindRecordByAddress(pRef, &response.Addr);
        if (iIndex != -1)
        {
            pRecord = &pPingCache[iIndex];
            if (response.iPing > 0)
            {
                // ARTIST 82B27A58 compares the signed sample, then zero-extends the cache value.
                pRecord->uPing = (response.iPing < 4095) ? (u16)response.iPing : 4095u;
                if (pRecord->pCallback)
                {
                    pRecord->pCallback(&response.Addr, response.iPing, pRecord->pCallbackData);
                    pRecord->pCallback = 0;
                }
            }
            else
            {
                pRecord->uPing = 0x2000;
                if (pRecord->pCallback)
                {
                    pRecord->pCallback(&pRecord->Addr, 0, pRecord->pCallbackData);
                    pRecord->pCallback = 0;
                }
            }
        }
        else
        {
            NetPrintfCode("pingmanager: warning - got a ping response for address %s not in the cache\n",
                          SocketInAddrGetText(response.uAddr));
        }
    }
}
