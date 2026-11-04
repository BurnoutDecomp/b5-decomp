// Native X360 NetConn connection state machine and public control operations.
#include "netconn_x360_internal.h"
#include "netconn.h"
#include "dirtymem.h"
#include "dirtylib.h"
#include <stdlib.h>
#include <string.h>

extern "C" u32 NetTick(void);
extern "C" const char* DirtyErrGetName(u32 uError);
extern "C" const char* SocketInAddrGetText(u32 uAddress);
extern "C" s32 SocketControl(void* pSocket, s32 iOption, s32 iData, void* pData2, void* pData3);

// ARTIST 0x82B11DF0, service discovery and socket address mapping.
static s32 NetConnX360DiscoverServices(NetConnX360State* pState)
{
    for (u32 i = 0; i < static_cast<u32>(pState->iServiceCount); ++i)
    {
        XONLINE_SERVICE_INFO* pService = &pState->aServices[i];
        DWORD uError = XOnlineGetServiceInfo(pService->dwServiceID, pService);
        if (uError != 0)
        {
            NetPrintfCode("netconnxenon: error %s getting service info for service #%d (id=0x%08x)\n",
                          DirtyErrGetName(uError), i, pService->dwServiceID);
        }
        else
        {
            NetPrintfCode("netconnxenon: service 0x%08x is available\n", pService->dwServiceID);
        }
    }
    if (pState->aServices[0].serviceIP.s_addr == 0) return -1;
    NetPrintfCode("netconnxenon: sg serviceID: 0x%08x\n", pState->aServices[0].dwServiceID);
    NetPrintfCode("netconnxenon: sg secureIP: %s\n", SocketInAddrGetText(pState->aServices[0].serviceIP.s_addr));
    memset(&pState->AddressMap, 0, sizeof(pState->AddressMap));
    pState->AddressMap.uStart = 0;
    pState->AddressMap.uEnd = 0;
    pState->AddressMap.uRemap = pState->aServices[0].serviceIP.s_addr;
    SocketControl(0, 0x786d6170, 1, &pState->AddressMap, 0);
    SocketControl(0, 0x78646e73, 0x7f000001, 0, 0);
    return 0;
}

// ARTIST 0x82B12690. This is the callback registered by NetConnStartup and
// also invoked synchronously from owner-thread status queries.
void NetConnX360Idle(void* pContext, u32 uTick)
{
    NetConnX360State* pState = static_cast<NetConnX360State*>(pContext);
    DWORD uEvent;
    ULONG_PTR uParam;
    pState->uAddressStatus = XNetGetTitleXnAddr(&pState->LocalAddress);
    if (XNotifyGetNext(pState->hNotify, 0x02000002, &uEvent, &uParam))
    {
        NetPrintfCode("netconnxenon: invite accept event\n");
        ++pState->iInviteCount;
    }
    NetConnX360RefreshFriends(pState, uTick);

    if (pState->iConnectionState == 1)
    {
        pState->iConnectionStatus = NetConnX360AddressStatus(pState, pState->uAddressStatus);
        if ((pState->bSyslink == 1) && (pState->iConnectionStatus == 0x2b697370) &&
            (pState->bKeyRegistered == 0))
        {
            XNetCreateKey(&pState->LocalKeyId, &pState->LocalKey);
            XNetRegisterKey(&pState->LocalKeyId, &pState->LocalKey);
            pState->bKeyRegistered = 1;
        }
        return;
    }
    if (pState->iConnectionState == 2)
    {
        if ((static_cast<u32>(pState->iConnectionStatus) & 0xff000000u) != 0x2d000000u)
        {
            if (((XNetGetEthernetLinkStatus() & 1) == 0) ||
                ((pState->bSecure == 1) && (NetConnX360DiscoverServices(pState) < 0)))
            {
                pState->iConnectionStatus = 0x2d737663;
            }
            else
            {
                NetConnX360RefreshUsers(pState);
                pState->bOnline = 1;
                pState->iConnectionStatus = 0x2b78626c;
            }
        }
        if (pState->iConnectionStatus == 0x2b78626c)
        {
            if (pState->bSecure != 0)
            {
                s32 iError = XNetConnect(pState->aServices[0].serviceIP);
                if (iError != 0)
                {
                    NetPrintfCode("netconnxenon: error %s trying to connect to security gateway\n", DirtyErrGetName(iError));
                    pState->iConnectionState = 0;
                    pState->iConnectionStatus = 0x2d676174;
                }
                // ARTIST overwrites state with 3 even on the error branch.
                pState->iConnectionState = 3;
            }
            else pState->iConnectionState = 4;
        }
        return;
    }
    if (pState->iConnectionState == 3)
    {
        DWORD uStatus = XNetGetConnectStatus(pState->aServices[0].serviceIP);
        if (uStatus == 2)
        {
            pState->iConnectionState = 4;
            pState->bGatewayConnected = 1;
            NetPrintfCode("netconnxenon: connected to gateway\n");
        }
        else if (uStatus == 3)
        {
            NetPrintfCode("netconnxenon: secureIP connection lost\n");
            pState->iConnectionState = 0;
            pState->iConnectionStatus = 0x2d737663;
        }
        return;
    }
    if (static_cast<u32>(pState->iConnectionState) >= 5) return;
    if (pState->iConnectionState == 4)
    {
        s32 iResult = NetConnX360UpdateUserInfo(pState);
        if (iResult > 0) pState->iConnectionStatus = 0x2b6f6e6c;
        if (iResult == 0) return;
        pState->iConnectionState = 0;
    }
    if (pState->bOnline == 0) return;
    if (XNotifyGetNext(pState->hNotify, 0x02000001, &uEvent, &uParam))
    {
        NetPrintfCode("netconnxenon: connection change->%s\n", DirtyErrGetName(uParam));
        if (uParam != 0x001510f0)
        {
            switch (uParam)
            {
            case 0x80151000: pState->iConnectionStatus = 0x2d6e6574; break;
            case 0x80151001: case 0x80151100: case 0x80151101: case 0x80151102:
                pState->iConnectionStatus = 0x2d737663; break;
            case 0x80151002: case 0x80151007: pState->iConnectionStatus = 0x2d757064; break;
            case 0x80151003: pState->iConnectionStatus = 0x2d627379; break;
            case 0x80151004: pState->iConnectionStatus = 0x2d6c7374; break;
            case 0x80151005: pState->iConnectionStatus = 0x2d647570; break;
            case 0x80151006: pState->iConnectionStatus = 0x2d696e76; break;
            case 0x001510f1: pState->iConnectionStatus = 0x2d6c6f66; break;
            default: pState->iConnectionStatus = 0x2d78626c; break;
            }
            pState->bOnline = 0;
        }
    }
    if (XNotifyGetNext(pState->hNotify, 10, &uEvent, &uParam))
    {
        NetPrintfCode("netconnxenon: signin change->0x%02x\n", uParam);
        NetConnX360RefreshUsers(pState);
    }
    if (XNotifyGetNext(pState->hNotify, 14, &uEvent, &uParam))
    {
        NetPrintfCode("netconnxenon: profile change->0x%02x\n", uParam);
        NetConnX360RefreshUsers(pState);
    }
    if (NetConnX360UpdateUserInfo(pState) < 0) return;
    if (pState->pQos != 0)
    {
        XNQOSINFO* pInfo = &pState->pQos->axnqosinfo[0];
        if ((pInfo->bFlags & 9) != 0)
        {
            pState->iDownstream = pInfo->dwDnBitsPerSec;
            pState->iUpstream = pInfo->dwUpBitsPerSec;
            pState->iRtt = pInfo->wRttMedInMsecs;
            NetPrintfCode("netconnxenon: Qos update (up=%d, dn=%d, mrtt=%d)\n",
                          pState->iUpstream / 1024, pState->iDownstream / 1024, pState->iRtt);
        }
        if ((pInfo->bFlags & 1) != 0)
        {
            XNetQosRelease(pState->pQos);
            pState->pQos = 0;
        }
    }
    else if (pState->bQosRequested == 1)
    {
        s32 iResult = XNetQosServiceLookup(0, 0, &pState->pQos);
        NetPrintfCode("netconnxenon: XNetQosServiceLookup() result=%d\n", iResult);
        pState->bQosRequested = 0;
    }
}

extern "C" s32 NetConnQuery(const char* pName, const void* pXuid)
{
    NetConnX360State* pState = gNetConnX360;
    NetConnX360Idle(pState, NetTick());
    if (pName != 0)
    {
        for (s32 i = 0; i < 4; ++i) if (strcmp(pName, pState->aUsers[i].strName) == 0) return i;
    }
    else if (pXuid != 0)
    {
        for (s32 i = 0; i < 4; ++i)
            if ((memcmp(pXuid, &pState->aUsers[i].xuid, 8) == 0) && (pState->aUsers[i].xuid != 0)) return i;
    }
    return -1;
}

extern "C" s32 NetConnConnect(const NetConfigRecT* /*pConfig*/, const char* pOption)
{
    NetConnX360State* pState = gNetConnX360;
    pState->iConnectionStatus = 0;
    pState->bQosRequested = 1;
    pState->iUpstream = pState->iRtt - 1;
    pState->iDownstream = pState->iRtt - 1;
    if (pState->bAddressReady == 0) return -1;
    s32 iService;
    if ((pOption == 0) || (strstr(pOption, "dev") != 0)) iService = 0x45410000;
    else if (strstr(pOption, "prod") != 0) iService = 0x45410004;
    else iService = strtol(pOption, 0, 16);
    pState->aServices[0].dwServiceID = iService;
    if (iService != 0)
    {
        pState->iConnectionState = 2;
        return 1;
    }
    NetPrintfCode("netconnxenon: invalid serviceID '%s' passed to NetConnConnect()\n", pOption);
    return -2;
}

extern "C" s32 NetConnDisconnect(void)
{
    NetConnX360State* pState = gNetConnX360;
    if (pState->pQos != 0)
    {
        XNetQosRelease(pState->pQos);
        pState->pQos = 0;
    }
    if (pState->bAccountBusy == 1)
    {
        XCancelOverlapped(&pState->AccountOverlapped);
        pState->bAccountBusy = 0;
    }
    for (s32 i = 0; i < 4; ++i)
    {
        if (pState->aUsers[i].pInfo != 0)
            DirtyMemFree(pState->aUsers[i].pInfo, 0x6e636f6e, pState->iMemGroup);
        memset(&pState->aUsers[i], 0, sizeof(pState->aUsers[i]));
    }
    XNetUnregisterKey(&pState->LocalKeyId);
    pState->iConnectionStatus = 0;
    pState->iConnectionIndex = -1;
    pState->iConnectionState = 1;
    return 0;
}

extern "C" s32 NetConnControl(s32 iControl, s32 iValue, s32 /*iValue2*/, void* pValue, void* pValue2)
{
    NetConnX360State* pState = gNetConnX360;
    if (pState == 0)
    {
        NetPrintfCode("netconnxenon: warning - calling NetConnControl() while module is not initialized\n");
        return -1;
    }
    if ((iControl == 0x66727274) && (pState->iFriendRefreshPeriod != iValue))
    {
        pState->iFriendRefreshPeriod = iValue;
        pState->uFriendRefreshTick = NetTick();
    }
    if (iControl == 0x73657276)
    {
        if (static_cast<u32>(pState->iServiceCount) < 4)
        {
            pState->aServices[pState->iServiceCount++].dwServiceID = iValue;
            return 0;
        }
        NetPrintfCode("netconnxenon: attempt to add service %d exceeds max allowed services\n", iValue);
    }
    if ((iControl == 0x7469746c) || (iControl == 0x786c7370))
    {
        NetConnControl(0x73657276, 0x45410805, 0, 0, 0);
        return 0;
    }
    return SocketControl(0, iControl, iValue, pValue, pValue2);
}
