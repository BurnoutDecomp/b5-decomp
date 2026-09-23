// ============================================================================
// netconnpc.cpp -- NetConn for the PC backend.
//
// [PC platform layer] The console NetConn brought up the Xbox network stack and the LIVE
// service connection. On the PC there is no service: offline (BP_LAN unset) the stack answers
// like a console with no network (not online, cable unplugged, no gamertag); with BP_LAN=1 it
// starts the pclan socket and reports itself online. Either way NetConnIdle pumps every
// registered idle handler (ConnApi and the other DirtySDK modules run from there), exactly as
// the console does. Like the console module, NetConn keeps one process-wide state block.
//
// This file also owns the PCLAN_BYE fan-out (lan/pclan_link.h): pclan has one handler per
// message type, so every PC module that must hear about a departed peer registers here.
// ============================================================================

#include "netconn.h"
#include "lan/pclan.h"
#include "lan/pclan_link.h"

#include "GameShared/GameClasses/System/PC/CgsPcNetIdentity.h"

#include <stdlib.h>
#include <string.h>

namespace
{
    // Four-character selectors.
    const s32 KI_SEL_ONLN = 0x6F6E6C6E;   // 'onln'
    const s32 KI_SEL_CONN = 0x636F6E6E;   // 'conn'
    const s32 KI_SEL_PLUG = 0x706C7567;   // 'plug'
    const s32 KI_SEL_GTAG = 0x67746167;   // 'gtag'
    const s32 KI_SEL_OPEN = 0x6F70656E;   // 'open'
    const s32 KI_SEL_MACX = 0x6D616378;   // 'macx'
    const s32 KI_SEL_FRRT = 0x66727274;   // 'frrt'
    const s32 KI_SEL_SERV = 0x73657276;   // 'serv'
    const s32 KI_SEL_XLSP = 0x786C7370;   // 'xlsp'
    const s32 KI_SEL_TSRV = 0x74737276;   // 'tsrv'

    // 'conn' while the service connection is up.
    const s32 KI_CONN_ONLINE = 0x2B6F6E6C;   // '+onl'

    // Service ids the console connect string maps to.
    const u32 KU_SERVICE_DEV  = 0x45410000u;   // "dev"
    const u32 KU_SERVICE_PROD = 0x45410004u;   // "prod"
    // The service 'xlsp' / 'tsrv' add.
    const s32 KI_SERVICE_TITLE = 0x45410005;

    const s32 KI_MAX_SERVICES  = 4;
    const s32 KI_IDLE_SLOTS    = 32;
    const s32 KI_BYE_SLOTS     = 4;
    // Idle handlers run at most this often.
    const u32 KU_IDLE_PERIOD_MS = 5u;

    struct NetConnIdleEntryT
    {
        NetConnIdleProcT* pProc;
        void*             pData;
    };

    struct NetConnByeEntryT
    {
        PcLanHandlerT* pHandler;
        void*          pRef;
    };

    struct NetConnStateT
    {
        bool              bStarted;         // NetConnStartup ran
        bool              bConnected;       // NetConnConnect succeeded (LAN)
        s32               iConnState;       // 'conn' word
        s32               iConnectTimeout;  // 'frrt'
        u32               uServiceId;       // NetConnConnect's service id
        s32               iNumServices;     // 'serv' entries
        s32               aServices[KI_MAX_SERVICES];
        u32               uNextIdleTick;
        NetConnIdleEntryT aIdle[KI_IDLE_SLOTS];
        NetConnByeEntryT  aBye[KI_BYE_SLOTS];
        char              strMac[14];       // NetConnMAC text
    };

    NetConnStateT gNetConn = {};

    // pclan's single PCLAN_BYE handler: hand the departure to every registered module.
    void NetConnByeFanOut(void* /*pRef*/, const PcLanPeerT* pFrom, const PcLanHeaderT* pHeader, const uint8_t* pBody)
    {
        for (s32 iSlot = 0; iSlot < KI_BYE_SLOTS; ++iSlot)
        {
            const NetConnByeEntryT Entry = gNetConn.aBye[iSlot];
            if (Entry.pHandler != NULL)
            {
                Entry.pHandler(Entry.pRef, pFrom, pHeader, pBody);
            }
        }
    }

    // The six adapter-address bytes: none offline, the low 48 bits of the XUID on the LAN.
    bool NetConnMacBytes(u8 aMac[6])
    {
        if (!PcLanActive())
        {
            memset(aMac, 0, 6);
            return false;
        }
        const u64 uXuid = CgsPcNetIdentityXuid();
        for (s32 iByte = 0; iByte < 6; ++iByte)
        {
            aMac[iByte] = (u8)(uXuid >> (8 * (5 - iByte)));
        }
        return true;
    }
}

bool PcNetConnByeAdd(PcLanHandlerT* pHandler, void* pRef)
{
    s32 iFree = -1;
    for (s32 iSlot = 0; iSlot < KI_BYE_SLOTS; ++iSlot)
    {
        if ((gNetConn.aBye[iSlot].pHandler == pHandler) && (gNetConn.aBye[iSlot].pRef == pRef))
        {
            return true;
        }
        if ((gNetConn.aBye[iSlot].pHandler == NULL) && (iFree < 0))
        {
            iFree = iSlot;
        }
    }
    if (iFree < 0)
    {
        return false;
    }
    gNetConn.aBye[iFree].pHandler = pHandler;
    gNetConn.aBye[iFree].pRef     = pRef;
    return true;
}

void PcNetConnByeDel(PcLanHandlerT* pHandler, void* pRef)
{
    for (s32 iSlot = 0; iSlot < KI_BYE_SLOTS; ++iSlot)
    {
        if ((gNetConn.aBye[iSlot].pHandler == pHandler) && (gNetConn.aBye[iSlot].pRef == pRef))
        {
            gNetConn.aBye[iSlot].pHandler = NULL;
            gNetConn.aBye[iSlot].pRef     = NULL;
        }
    }
}

extern "C" s32 NetConnStartup(const char* /*pParams*/)
{
    gNetConn.bStarted      = true;
    gNetConn.bConnected    = false;
    gNetConn.iConnState    = 0;
    gNetConn.uNextIdleTick = 0;
    gNetConn.strMac[0]     = '\0';

    PcLanRegister(PCLAN_BYE, &NetConnByeFanOut, NULL);
    PcLinkAttach();
    PcLanStartup();
    return 0;
}

extern "C" s32 NetConnShutdown(u32 uShutdownFlags)
{
    if (!gNetConn.bStarted)
    {
        return -1;
    }
    for (s32 iSlot = 0; iSlot < KI_IDLE_SLOTS; ++iSlot)
    {
        gNetConn.aIdle[iSlot].pProc = NULL;
        gNetConn.aIdle[iSlot].pData = NULL;
    }
    gNetConn.uNextIdleTick = 0;
    // Bit 1 keeps the network up across the shutdown.
    if ((uShutdownFlags & 2u) == 0)
    {
        NetConnDisconnect();
        PcLanShutdown();
    }
    gNetConn.bStarted = false;
    return 0;
}

extern "C" s32 NetConnConnect(const NetConfigRecT* /*pConfig*/, const char* pOption)
{
    gNetConn.iConnState = 0;
    if (!PcLanActive())
    {
        return 0;
    }

    u32 uServiceId;
    if ((pOption == NULL) || (strstr(pOption, "dev") != NULL))
    {
        uServiceId = KU_SERVICE_DEV;
    }
    else if (strstr(pOption, "prod") != NULL)
    {
        uServiceId = KU_SERVICE_PROD;
    }
    else
    {
        uServiceId = (u32)strtoul(pOption, NULL, 16);
    }
    if (uServiceId == 0)
    {
        return -2;
    }
    gNetConn.uServiceId = uServiceId;
    gNetConn.bConnected = true;
    gNetConn.iConnState = KI_CONN_ONLINE;
    return 1;
}

extern "C" s32 NetConnDisconnect(void)
{
    gNetConn.bConnected = false;
    gNetConn.iConnState = 0;
    return 0;
}

extern "C" s32 NetConnControl(s32 iControl, s32 iValue, s32 /*iValue2*/, void* /*pValue*/, void* /*pValue2*/)
{
    if (!gNetConn.bStarted)
    {
        return -1;
    }
    if (iControl == KI_SEL_FRRT)
    {
        gNetConn.iConnectTimeout = iValue;
        return 0;
    }
    if (iControl == KI_SEL_SERV)
    {
        if (gNetConn.iNumServices < KI_MAX_SERVICES)
        {
            gNetConn.aServices[gNetConn.iNumServices++] = iValue;
        }
        return 0;
    }
    if ((iControl == KI_SEL_XLSP) || (iControl == KI_SEL_TSRV))
    {
        return NetConnControl(KI_SEL_SERV, KI_SERVICE_TITLE, 0, NULL, NULL);
    }
    return -1;
}

extern "C" s32 NetConnStatus(s32 iKind, s32 /*iData*/, void* pBuf, s32 iBufSize)
{
    if (iKind == KI_SEL_OPEN)
    {
        return gNetConn.bStarted ? 1 : 0;
    }
    if (!gNetConn.bStarted)
    {
        return -1;
    }
    if (iKind == KI_SEL_ONLN)
    {
        return PcLanActive() ? 1 : 0;
    }
    if (iKind == KI_SEL_CONN)
    {
        return gNetConn.iConnState;
    }
    if (iKind == KI_SEL_PLUG)
    {
        return PcLanActive() ? 1 : 0;
    }
    if (iKind == KI_SEL_GTAG)
    {
        // The persona of the signed-in user (every user index is the one PC identity).
        if (!PcLanActive() || (pBuf == NULL) || (iBufSize < 16))
        {
            return 0;
        }
        char* pName = static_cast<char*>(pBuf);
        memset(pName, 0, 16);
        strncpy_s(pName, 16, CgsPcNetIdentityName(), _TRUNCATE);
        return 1;
    }
    if (iKind == KI_SEL_MACX)
    {
        u8 aMac[6];
        if (!NetConnMacBytes(aMac) || (pBuf == NULL) || (iBufSize < 6))
        {
            return 0;
        }
        memcpy(pBuf, aMac, 6);
        return 1;
    }
    return -1;
}

extern "C" void NetConnIdle(void)
{
    PcLanIdle();
    if (!gNetConn.bStarted)
    {
        return;
    }
    const u32 uNow = PcLinkTick();
    if ((s32)(uNow - gNetConn.uNextIdleTick) < 0)
    {
        return;
    }
    gNetConn.uNextIdleTick = uNow + KU_IDLE_PERIOD_MS;
    for (s32 iSlot = 0; iSlot < KI_IDLE_SLOTS; ++iSlot)
    {
        const NetConnIdleEntryT Entry = gNetConn.aIdle[iSlot];
        if (Entry.pProc != NULL)
        {
            Entry.pProc(Entry.pData, uNow);
        }
    }
}

extern "C" const char* NetConnMAC(void)
{
    static const char KAC_HEX[] = "0123456789abcdef";
    u8 aMac[6];
    NetConnMacBytes(aMac);
    gNetConn.strMac[0] = '$';
    for (s32 iByte = 0; iByte < 6; ++iByte)
    {
        gNetConn.strMac[1 + 2 * iByte] = KAC_HEX[aMac[iByte] >> 4];
        gNetConn.strMac[2 + 2 * iByte] = KAC_HEX[aMac[iByte] & 15];
    }
    gNetConn.strMac[13] = '\0';
    return gNetConn.strMac;
}

extern "C" s32 NetConnIdleAdd(NetConnIdleProcT* pProc, void* pData)
{
    for (s32 iSlot = 0; iSlot < KI_IDLE_SLOTS; ++iSlot)
    {
        if (gNetConn.aIdle[iSlot].pProc == NULL)
        {
            gNetConn.aIdle[iSlot].pProc = pProc;
            gNetConn.aIdle[iSlot].pData = pData;
            return 0;
        }
    }
    return -1;
}

extern "C" s32 NetConnIdleDel(NetConnIdleProcT* pProc, void* pData)
{
    for (s32 iSlot = 0; iSlot < KI_IDLE_SLOTS; ++iSlot)
    {
        if ((gNetConn.aIdle[iSlot].pProc == pProc) && (gNetConn.aIdle[iSlot].pData == pData))
        {
            gNetConn.aIdle[iSlot].pProc = NULL;
            gNetConn.aIdle[iSlot].pData = NULL;
            return 0;
        }
    }
    return -1;
}
