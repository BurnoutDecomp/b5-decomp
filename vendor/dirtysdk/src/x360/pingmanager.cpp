// Native DirtySDK 5.5.3 server-request cancellation, ARTIST 82B27BA0.
#include "pingmanager.h"
#include "dirtyaddr.h"
#include "dirtylib.h"
#include "dirtymem.h"
#include <string.h>
#include <stddef.h>
struct ProtoPingRefT;
struct ServerPingRecT { PingManagerCallbackT* pServCb; void* pServCbData; u32 uSeqn; };
struct PingRecT { DirtyAddrT Addr; u16 uPing; u8 uCreatedTime, uTouchedTime; PingManagerCallbackT* pCallback; void* pCallbackData; };
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
typedef char ServerPingsOffset[(offsetof(PingManagerRefT, ServerPings) == 0x18) ? 1 : -1];
extern "C" void PingManagerCancelServerRequest(PingManagerRefT* pRef, u32 uSeqn)
{
    for (u32 i = 0; i < 8; ++i)
    {
        ServerPingRecT& r = pRef->ServerPings[i];
        if (r.uSeqn == uSeqn || !uSeqn)
        {
            r.pServCb = 0;
            r.pServCbData = 0;
            r.uSeqn = 0;
        }
    }
    if (uSeqn) NetPrintfCode("pingmanager: could not find server request with seqn=%d to cancel\n", uSeqn);
}

extern "C" u32 NetTick(void);
extern "C" ProtoPingRefT* ProtoPingCreate(s32 iMaxPings);
extern "C" s32 ProtoPingControl(ProtoPingRefT* pRef, s32 iControl, s32 iValue, void* pValue);
extern "C" s32 ProtoPingRequestServer(ProtoPingRefT* pRef, u32 uAddress, const unsigned char* pData, s32 iLength, s32 iTimeout, s32 iTtl);
static PingManagerRefT* gpPingManager;
static s32 giPingManagerRefs;

extern "C" PingManagerRefT* PingManagerCreate(u32 uMaxRecords)
{
    const s32 iMemGroup = DirtyMemGroupQuery();
    if (gpPingManager) { ++giPingManagerRefs; return gpPingManager; }
    if (uMaxRecords < 16)
    {
        NetPrintfCode("pingmanager: warning -- create call specifies a max record size that is too small\n");
        uMaxRecords = 16;
    }
    const u32 uSize = sizeof(PingManagerRefT) + sizeof(PingRecT) * (uMaxRecords - 1);
    PingManagerRefT* pRef = static_cast<PingManagerRefT*>(DirtyMemAlloc(uSize, 0x6c706d67, iMemGroup));
    if (!pRef) return 0;
    memset(pRef, 0, uSize);
    pRef->iMemGroup = iMemGroup;
    pRef->uMaxRecords = uMaxRecords;
    pRef->uTickCount = pRef->uLastPingTick = NetTick();
    pRef->pPingRef = ProtoPingCreate(0);
    if (!pRef->pPingRef) { DirtyMemFree(pRef, 0x6c706d67, iMemGroup); return 0; }
    ProtoPingControl(pRef->pPingRef, 0x74696d65, 4, 0);
    gpPingManager = pRef;
    giPingManagerRefs = 1;
    return pRef;
}

extern "C" s32 PingManagerPingServer2(PingManagerRefT* pRef, u32 uAddress, PingManagerCallbackT* pCallback, void* pData)
{
    u32 i = 0;
    while (i < 8 && pRef->ServerPings[i].uSeqn) ++i;
    if (i == 8)
    {
        NetPrintfCode("pingmanager: server ping record overflow\n");
        return -1;
    }
    ServerPingRecT& r = pRef->ServerPings[i];
    r.pServCb = pCallback;
    r.pServCbData = pData;
    r.uSeqn = ProtoPingRequestServer(pRef->pPingRef, uAddress, 0, 0, 0, 0);
    return r.uSeqn;
}

// ARTIST 82B27798; singleton references are shared with PingManagerCreate.
extern "C" void ProtoPingDestroy(ProtoPingRefT* pRef);
extern "C" void PingManagerDestroy(PingManagerRefT* pRef)
{
    if (--giPingManagerRefs <= 0)
    {
        ProtoPingDestroy(pRef->pPingRef);
        DirtyMemFree(pRef, 0x6c706d67, pRef->iMemGroup);
        gpPingManager = 0;
        giPingManagerRefs = 0;
    }
}

struct ProtoPingResponseT
{
    DirtyAddrT Addr;
    u32 uAddr;
    s16 iPing;
    u16 uSeqn, uNumRetries;
    u8 uType, bServer;
};
extern "C" s32 ProtoPingRequest(ProtoPingRefT*, DirtyAddrT*, const unsigned char*, s32, s32, s32);
extern "C" s32 ProtoPingResponse(ProtoPingRefT*, unsigned char*, s32*, ProtoPingResponseT*);
extern "C" const char* SocketInAddrGetText(u32 uAddr);

// ARTIST 82B27808; callbacks are delivered from the real asynchronous ping queue.
extern "C" void PingManagerUpdate(PingManagerRefT* pRef)
{
    const u32 uTick = NetTick();
    const u32 uElapsedSeconds = (uTick - pRef->uTickCount) / 1000;
    if (uElapsedSeconds)
    {
        pRef->uTime += static_cast<u8>(uElapsedSeconds);
        pRef->uTickCount += 1000 * uElapsedSeconds;
    }
    const u32 uAllowed = (uTick - pRef->uLastPingTick) / 100;
    u32 uIssued = 0;
    for (u32 i = 0; i < pRef->uNumRecords; ++i)
    {
        PingRecT& r = pRef->PingCache[i];
        if (r.uCreatedTime == pRef->uTime)
        {
            if (static_cast<u8>(pRef->uTime - r.uTouchedTime) < 60)
            {
                r.uPing = 0x8000;
                r.uTouchedTime = r.uCreatedTime = static_cast<u8>(pRef->uTime - 1);
            }
            else
            {
                memcpy(&r, &pRef->PingCache[pRef->uNumRecords - 1], sizeof(r));
                --pRef->uNumRecords;
            }
        }
        if ((r.uPing & 0x8000) && uIssued < uAllowed)
        {
            ProtoPingRequest(pRef->pPingRef, &r.Addr, 0, 0, 0, 0);
            ++uIssued;
            r.uPing = (r.uPing & 0x0fff) | 0x4000;
            r.uTouchedTime = r.uCreatedTime = static_cast<u8>(pRef->uTime - 1);
            pRef->uLastPingTick = uTick;
        }
    }
    ProtoPingResponseT response;
    while (ProtoPingResponse(pRef->pPingRef, 0, 0, &response))
    {
        if (response.bServer == 1)
        {
            for (u32 i = 0; i < 8; ++i)
            {
                ServerPingRecT& r = pRef->ServerPings[i];
                if (r.uSeqn == response.uSeqn)
                {
                    if (r.pServCb) r.pServCb(&response.Addr, response.iPing, r.pServCbData);
                    r.uSeqn = 0;
                    break;
                }
            }
            continue;
        }
        u32 i = 0;
        while (i < pRef->uNumRecords && memcmp(&pRef->PingCache[i].Addr, &response.Addr, sizeof(DirtyAddrT))) ++i;
        if (i == pRef->uNumRecords)
        {
            NetPrintfCode("pingmanager: warning - got a ping response for address %s not in the cache\n", SocketInAddrGetText(response.uAddr));
            continue;
        }
        PingRecT& r = pRef->PingCache[i];
        if (response.iPing > 0)
        {
            r.uPing = response.iPing >= 4095 ? 4095 : response.iPing;
            if (r.pCallback) { r.pCallback(&response.Addr, response.iPing, r.pCallbackData); r.pCallback = 0; }
        }
        else
        {
            r.uPing = 0x2000;
            if (r.pCallback) { r.pCallback(&r.Addr, 0, r.pCallbackData); r.pCallback = 0; }
        }
    }
}
