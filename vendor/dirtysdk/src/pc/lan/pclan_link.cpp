// ============================================================================
// pclan_link.cpp -- game links over the PC LAN transport.
//
// [PC platform layer] See pclan_link.h. This file owns the live-link chain and the greeting
// book (members that said hello before this instance opened a link to them); both are pclan
// layer state. The NetGameLink API itself (send / receive / status) is in ../netgamelinkpc.cpp,
// ConnApi drives link lifetime from ../connapipc.cpp.
// ============================================================================

#include "pclan_link.h"

#include "dirtymem.h"

#include "GameShared/GameClasses/System/PC/CgsPcNetIdentity.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace
{
    // DirtyMem module tag of a link ('ngln').
    const s32 KI_MEM_MODULE_LINK = 0x6E676C6E;

    // Members that greeted us before we had a link to them.
    const s32 KI_GREETING_SLOTS = 16;

    struct PcLinkGreetingT
    {
        u64        uXuid;
        u32        uIdent;
        PcLanPeerT Peer;
        bool       bUsed;
    };

    struct PcLinkStateT
    {
        NetGameLinkRefT* pLinks;
        PcLinkGreetingT  aGreetings[KI_GREETING_SLOTS];
        s32              iGreetingNext;
        s32              iLogLeft;
        bool             bAttached;
    };

    PcLinkStateT gPcLink = { NULL, {}, 0, 48, false };

    void PcLinkLog(const char* pFormat, ...)
    {
        if (gPcLink.iLogLeft <= 0)
        {
            return;
        }
        char strText[256];
        int iPrefix = _snprintf_s(strText, sizeof(strText), _TRUNCATE, "[net] link ");
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
        if (--gPcLink.iLogLeft == 0)
        {
            strncat_s(strText, sizeof(strText), "[net] link (log budget spent)\n", _TRUNCATE);
        }
        CgsDev::Log::WriteToLog(strText);
    }

    bool PcLinkSamePeer(const PcLanPeerT* pA, const PcLanPeerT* pB)
    {
        return (pA->uAddr == pB->uAddr) && (pA->uPort == pB->uPort);
    }

    void PcLinkGreetingRemember(u64 uXuid, u32 uIdent, const PcLanPeerT* pPeer)
    {
        for (s32 iSlot = 0; iSlot < KI_GREETING_SLOTS; ++iSlot)
        {
            PcLinkGreetingT* pGreeting = &gPcLink.aGreetings[iSlot];
            if (pGreeting->bUsed && (pGreeting->uXuid == uXuid))
            {
                pGreeting->uIdent = uIdent;
                pGreeting->Peer   = *pPeer;
                return;
            }
        }
        PcLinkGreetingT* pGreeting = &gPcLink.aGreetings[gPcLink.iGreetingNext];
        gPcLink.iGreetingNext = (gPcLink.iGreetingNext + 1) % KI_GREETING_SLOTS;
        pGreeting->uXuid  = uXuid;
        pGreeting->uIdent = uIdent;
        pGreeting->Peer   = *pPeer;
        pGreeting->bUsed  = true;
    }

    const PcLinkGreetingT* PcLinkGreetingFind(u64 uXuid)
    {
        for (s32 iSlot = 0; iSlot < KI_GREETING_SLOTS; ++iSlot)
        {
            const PcLinkGreetingT* pGreeting = &gPcLink.aGreetings[iSlot];
            if (pGreeting->bUsed && (pGreeting->uXuid == uXuid))
            {
                return pGreeting;
            }
        }
        return NULL;
    }

    void PcLinkMarkUp(NetGameLinkRefT* pLink, u32 uIdent, const PcLanPeerT* pPeer)
    {
        const bool bWasUp = pLink->bUp;
        pLink->uPeerIdent = uIdent;
        pLink->Peer       = *pPeer;
        pLink->bResolved  = true;
        pLink->bUp        = true;
        pLink->bDown      = false;
        if (!bWasUp)
        {
            char strAddr[24];
            _snprintf_s(strAddr, sizeof(strAddr), _TRUNCATE, "%u.%u.%u.%u:%u", (pPeer->uAddr >> 24) & 0xFF,
                        (pPeer->uAddr >> 16) & 0xFF, (pPeer->uAddr >> 8) & 0xFF, pPeer->uAddr & 0xFF,
                        pPeer->uPort);
            PcLinkLog("up xuid=%016llX ident=%08X peer=%s", (unsigned long long)pLink->uPeerXuid, uIdent,
                      strAddr);
        }
    }

    void PcLinkSendHello(NetGameLinkRefT* pLink, u8 uType)
    {
        PcLinkHelloT Hello;
        Hello.uXuid  = CgsPcNetIdentityXuid();
        Hello.uIdent = CgsPcNetIdentityLobbyIdent();
        PcLanSendControl(&pLink->Peer, uType, 0, &Hello, (int32_t)sizeof(Hello));
    }

    // LINK_HELLO and LINK_ACK: the member is alive and knows us. HELLO is answered with ACK.
    void PcLinkOnGreeting(void* /*pRef*/, const PcLanPeerT* pFrom, const PcLanHeaderT* pHeader, const uint8_t* pBody)
    {
        if (pHeader->uLen < sizeof(PcLinkHelloT))
        {
            return;
        }
        PcLinkHelloT Hello;
        memcpy(&Hello, pBody, sizeof(Hello));

        PcLanPeerRemember(Hello.uXuid, Hello.uIdent, pFrom);
        PcLinkGreetingRemember(Hello.uXuid, Hello.uIdent, pFrom);

        for (NetGameLinkRefT* pLink = gPcLink.pLinks; pLink != NULL; pLink = pLink->pNext)
        {
            if (pLink->uPeerXuid == Hello.uXuid)
            {
                PcLinkMarkUp(pLink, Hello.uIdent, pFrom);
            }
        }

        if (pHeader->uType == PCLAN_LINK_HELLO)
        {
            PcLinkHelloT Ack;
            Ack.uXuid  = CgsPcNetIdentityXuid();
            Ack.uIdent = CgsPcNetIdentityLobbyIdent();
            PcLanSendControl(pFrom, PCLAN_LINK_ACK, 0, &Ack, (int32_t)sizeof(Ack));
        }
    }

    // DATA: queue the packet on the link to its sender.
    void PcLinkOnData(void* /*pRef*/, const PcLanPeerT* pFrom, const PcLanHeaderT* pHeader, const uint8_t* pBody)
    {
        if (pHeader->uLen < sizeof(PcLinkDataHeadT))
        {
            return;
        }
        PcLinkDataHeadT Head;
        memcpy(&Head, pBody, sizeof(Head));
        if ((Head.uLen > NETGAME_DATAPKT_MAXSIZE) || (sizeof(Head) + Head.uLen > pHeader->uLen))
        {
            return;
        }

        NetGameLinkRefT* pLink = gPcLink.pLinks;
        for (; pLink != NULL; pLink = pLink->pNext)
        {
            if (pLink->bUp && PcLinkSamePeer(&pLink->Peer, pFrom))
            {
                break;
            }
        }
        if (pLink == NULL)
        {
            return;
        }

        if (pLink->iRecvCount == PCLINK_RECV_SLOTS)
        {
            pLink->uRecvDropped += 1;
            return;
        }
        const s32 iSlot = (pLink->iRecvHead + pLink->iRecvCount) % PCLINK_RECV_SLOTS;
        PcLinkPacketT* pPacket = &pLink->aRecv[iSlot];
        pPacket->uLen  = Head.uLen;
        pPacket->uKind = Head.uKind;
        pPacket->uPad  = 0;
        memcpy(pPacket->aData, pBody + sizeof(Head), Head.uLen);
        pLink->iRecvCount += 1;
    }

    // A member left or timed out: its links go down (ConnApi reports the disconnect).
    void PcLinkOnBye(void* /*pRef*/, const PcLanPeerT* pFrom, const PcLanHeaderT* pHeader, const uint8_t* /*pBody*/)
    {
        for (NetGameLinkRefT* pLink = gPcLink.pLinks; pLink != NULL; pLink = pLink->pNext)
        {
            const bool bSameIdent = (pHeader->uSenderIdent != 0) && (pLink->uPeerIdent == pHeader->uSenderIdent);
            const bool bSamePeer  = pLink->bResolved && PcLinkSamePeer(&pLink->Peer, pFrom);
            if (bSameIdent || bSamePeer)
            {
                if (pLink->bUp)
                {
                    PcLinkLog("down xuid=%016llX ident=%08X", (unsigned long long)pLink->uPeerXuid,
                              pLink->uPeerIdent);
                }
                pLink->bUp       = false;
                pLink->bDown     = true;
                pLink->bResolved = false;
            }
        }
        for (s32 iSlot = 0; iSlot < KI_GREETING_SLOTS; ++iSlot)
        {
            PcLinkGreetingT* pGreeting = &gPcLink.aGreetings[iSlot];
            if (pGreeting->bUsed &&
                (((pHeader->uSenderIdent != 0) && (pGreeting->uIdent == pHeader->uSenderIdent)) ||
                 PcLinkSamePeer(&pGreeting->Peer, pFrom)))
            {
                pGreeting->bUsed = false;
            }
        }
    }
}

u32 PcLinkTick()
{
    return (u32)GetTickCount64();
}

bool PcLinkXuidFromAddr(const char* pMachineAddr, u64* pXuid)
{
    if ((pMachineAddr == NULL) || (pMachineAddr[0] != '$'))
    {
        return false;
    }
    u64 uXuid = 0;
    s32 iDigits = 0;
    // The hex digits end at the first other character (a machine address may carry more after
    // them); exactly 16 of them make an XUID.
    for (const char* pDigit = pMachineAddr + 1; ; ++pDigit, ++iDigits)
    {
        const char cDigit = *pDigit;
        u32 uValue;
        if ((cDigit >= '0') && (cDigit <= '9'))
        {
            uValue = (u32)(cDigit - '0');
        }
        else if ((cDigit >= 'a') && (cDigit <= 'f'))
        {
            uValue = (u32)(cDigit - 'a' + 10);
        }
        else if ((cDigit >= 'A') && (cDigit <= 'F'))
        {
            uValue = (u32)(cDigit - 'A' + 10);
        }
        else
        {
            break;
        }
        if (iDigits >= 16)
        {
            return false;
        }
        uXuid = (uXuid << 4) | uValue;
    }
    if (iDigits != 16)
    {
        return false;
    }
    *pXuid = uXuid;
    return true;
}

NetGameLinkRefT* PcLinkCreate(u64 uPeerXuid)
{
    const s32 iMemGroup = DirtyMemGroupQuery();
    NetGameLinkRefT* pLink = (NetGameLinkRefT*)DirtyMemAlloc((s32)sizeof(NetGameLinkRefT), KI_MEM_MODULE_LINK, iMemGroup);
    memset(pLink, 0, sizeof(*pLink));
    pLink->iMemGroup = iMemGroup;
    pLink->uPeerXuid = uPeerXuid;
    pLink->uOpenTick = PcLinkTick();
    pLink->Stat.stattick = pLink->uOpenTick;

    pLink->pNext   = gPcLink.pLinks;
    gPcLink.pLinks = pLink;

    // The member may already have greeted us.
    const PcLinkGreetingT* pGreeting = PcLinkGreetingFind(uPeerXuid);
    if (pGreeting != NULL)
    {
        PcLinkMarkUp(pLink, pGreeting->uIdent, &pGreeting->Peer);
    }
    PcLinkLog("open xuid=%016llX%s", (unsigned long long)uPeerXuid, pLink->bUp ? " (already greeted)" : "");
    PcLinkPoll(pLink);
    return pLink;
}

void PcLinkDestroy(NetGameLinkRefT* pLink)
{
    if (pLink == NULL)
    {
        return;
    }
    for (NetGameLinkRefT** ppLink = &gPcLink.pLinks; *ppLink != NULL; ppLink = &(*ppLink)->pNext)
    {
        if (*ppLink == pLink)
        {
            *ppLink = pLink->pNext;
            break;
        }
    }
    PcLinkLog("close xuid=%016llX dropped=%u", (unsigned long long)pLink->uPeerXuid, pLink->uRecvDropped);
    DirtyMemFree(pLink, KI_MEM_MODULE_LINK, pLink->iMemGroup);
}

void PcLinkPoll(NetGameLinkRefT* pLink)
{
    if ((pLink == NULL) || pLink->bDown || !PcLanActive())
    {
        return;
    }
    if (!pLink->bResolved)
    {
        PcLanPeerT Peer;
        if (!PcLanPeerByXuid(pLink->uPeerXuid, &Peer))
        {
            return;
        }
        pLink->Peer      = Peer;
        pLink->bResolved = true;
    }
    if (pLink->bUp)
    {
        return;
    }
    const u32 uNow = PcLinkTick();
    if ((pLink->uHelloTick == 0) || ((uNow - pLink->uHelloTick) >= PCLINK_HELLO_RETRY_MS))
    {
        pLink->uHelloTick = (uNow != 0) ? uNow : 1;
        PcLinkSendHello(pLink, PCLAN_LINK_HELLO);
    }
}

s32 PcLinkSendData(NetGameLinkRefT* pLink, const u8* pData, u16 uLen, u8 uKind)
{
    if (!pLink->bUp || pLink->bDown || (uLen > NETGAME_DATAPKT_MAXSIZE))
    {
        return -1;
    }
    u8 aBody[sizeof(PcLinkDataHeadT) + NETGAME_DATAPKT_MAXSIZE];
    PcLinkDataHeadT Head;
    Head.uLen  = uLen;
    Head.uKind = uKind;
    Head.uPad  = 0;
    memcpy(aBody, &Head, sizeof(Head));
    memcpy(aBody + sizeof(Head), pData, uLen);
    return PcLanSendData(&pLink->Peer, 0, aBody, (int32_t)(sizeof(Head) + uLen));
}

void PcLinkAttach()
{
    if (gPcLink.bAttached)
    {
        return;
    }
    gPcLink.bAttached = true;
    PcLanRegister(PCLAN_LINK_HELLO, &PcLinkOnGreeting, NULL);
    PcLanRegister(PCLAN_LINK_ACK, &PcLinkOnGreeting, NULL);
    PcLanRegister(PCLAN_DATA, &PcLinkOnData, NULL);
    PcNetConnByeAdd(&PcLinkOnBye, NULL);
}
