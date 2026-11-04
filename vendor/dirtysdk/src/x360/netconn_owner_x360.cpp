// Native X360 NetConn singleton lifecycle, ARTIST 0x82B12C08/0x82B13A50.
// The idle state machine, status dispatcher, and disconnect body are separate
// unresolved dependencies until the remainder of the native owner is recovered.
#include "netconn_x360_internal.h"
#include "netconn.h"
#include "dirtymem.h"
#include "dirtylib.h"
#include <stdlib.h>
#include <string.h>

extern "C" s32 SocketCreate(s32 iPriority);
extern "C" s32 SocketDestroy(s32 iFlags);
extern "C" u32 NetTick(void);
extern "C" const char* DirtyErrGetName(u32 uError);
extern "C" void NetConnIdleShutdown(void);

NetConnX360State* gNetConnX360 = 0;

extern "C" s32 NetConnStartup(const char* pParams)
{
    s32 iSyslink = 0;
    s32 iInsecure = 0;
    u8 uMaxKey = 4;
    NetConnX360State* pOldState = gNetConnX360;
    s32 iMemGroup = DirtyMemGroupQuery();
    if (pParams == 0)
    {
        pParams = "";
    }
    if (strstr(pParams, "-syslink") != 0)
    {
        NetPrintfCode("netconnxenon: starting up in syslink mode\n");
        iSyslink = 1;
    }
    if (strstr(pParams, "-coop") != 0)
    {
        iInsecure = 1;
    }
    if (strstr(pParams, "-nosecure") != 0)
    {
        iInsecure = 1;
    }
    const char* pMaxKey = strstr(pParams, "-maxkey=");
    if (pMaxKey != 0)
    {
        s32 iMaxKey = strtol(pMaxKey + 8, 0, 10);
        uMaxKey = static_cast<u8>(iMaxKey);
        NetPrintfCode("netconnxenon: setting maxkey=%d\n", iMaxKey);
    }
    if (pOldState != 0)
    {
        NetPrintfCode("netconnxenon: NetConnStartup() called while module is already active\n");
        return -1;
    }
    NetConnX360State* pState = static_cast<NetConnX360State*>(DirtyMemAlloc(0x13430, 0x6e636f6e, iMemGroup));
    if (pState == 0)
    {
        NetPrintfCode("netconnxenon: unable to allocate module state\n");
        return -2;
    }
    memset(pState, 0, 0x13430);
    pState->iMemGroup = iMemGroup;
    pState->bSecure = 1;
    pState->bSyslink = static_cast<u8>(iSyslink);
    XNetStartupParams xnetParams;
    memset(&xnetParams, 0, sizeof(xnetParams));
    xnetParams.cfgSizeOfStruct = 0x0d;
    xnetParams.cfgKeyRegMax = uMaxKey;
    if (iInsecure == 1)
    {
        NetPrintfCode("netconnxenon: starting up in insecure mode\n");
        xnetParams.cfgFlags = 1;
        pState->bSecure = 0;
    }
    s32 iError = XNetStartup(&xnetParams);
    if (iError != 0)
    {
        NetPrintfCode("netconnxenon: unable to start xnet (err=%s)\n", DirtyErrGetName(iError));
        NetPrintfCode("netconnxenon: unable to initialize network stack");
        return -3;
    }
    SocketCreate(2);
    if (pState->bSyslink == 0)
    {
        XOnlineStartup();
    }
    pState->iConnectionState = 1;
    pState->iServiceCount = 1;
    pState->iConnectionIndex = -1;
    NetConnIdleAdd(NetConnX360Idle, pState);
    pState->hNotify = XNotifyCreateListener(0x6f);
    if (pState->hNotify == 0)
    {
        NetPrintfCode("netconnxenon: unable to create live event listener\n");
        return -4;
    }
    RtlInitializeCriticalSection(&pState->CloseLock);
    pState->hDeferredClose = INVALID_HANDLE_VALUE;
    pState->hCloseThread = CreateThread(0, 0,
        NetConnX360CloseThread, pState, 0,
        &pState->uCloseThreadId);
    SetThreadPriority(pState->hCloseThread, 2);
    pState->iFriendRefreshPeriod = 1000;
    pState->uFriendRefreshTick = NetTick();
    u32 uOwner = GetCurrentThreadId();
    gNetConnX360 = pState;
    pState->uOwnerThreadId = uOwner;
    return 0;
}

extern "C" s32 NetConnShutdown(u32 uShutdownFlags)
{
    if (gNetConnX360 == 0)
    {
        return -1;
    }
    NetPrintfCode("Alex: Telling thread to shutdown\n");
    RtlEnterCriticalSection(&gNetConnX360->CloseLock);
    gNetConnX360->iCloseThreadStop = 1;
    RtlLeaveCriticalSection(&gNetConnX360->CloseLock);
    s32 iStop;
    do
    {
        Sleep(5);
        RtlEnterCriticalSection(&gNetConnX360->CloseLock);
        iStop = gNetConnX360->iCloseThreadStop;
        RtlLeaveCriticalSection(&gNetConnX360->CloseLock);
    }
    while (iStop != 0);
    gNetConnX360->hCloseThread = INVALID_HANDLE_VALUE;
    gNetConnX360->uCloseThreadId = 0;
    CloseHandle(gNetConnX360->hNotify);
    NetConnIdleDel(NetConnX360Idle, gNetConnX360);
    NetConnIdleShutdown();
    if ((uShutdownFlags & 2) == 0)
    {
        NetConnDisconnect();
        XOnlineCleanup();
        XNetCleanup();
        SocketDestroy(0);
    }
    DirtyMemFree(gNetConnX360, 0x6e636f6e, gNetConnX360->iMemGroup);
    gNetConnX360 = 0;
    return 0;
}

// ARTIST 0x82B13A48 forwards the duration directly to the XDK Sleep call.
extern "C" void NetConnSleep(s32 iMilliseconds)
{
    Sleep(iMilliseconds);
}
