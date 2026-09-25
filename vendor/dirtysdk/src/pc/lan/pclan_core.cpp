// ============================================================================
// pclan_core.cpp -- the PC LAN transport: socket, identity, peer book, control reliability.
//
// [PC platform layer] Not console code. The console reached other players through the online
// lobby service; the PC build has none, so with BP_LAN=1 each instance opens one UDP socket that
// carries discovery, lobby traffic and game data (see pclan.h for the wire and the rules).
// Without BP_LAN=1 PcLanStartup() opens nothing, every send fails and PcLanIdle() does nothing,
// which is exactly the "no network" answer the offline build relies on.
//
// All state below is this layer's own (the DirtySDK refs keep theirs in their ref structs).
// Single-threaded: every entry point runs on the game thread (NetConnIdle pumps it).
// ============================================================================

#include "pclan.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>

// The Windows SDK spells this in mstcpip.h only for some target versions.
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "GameShared/GameClasses/System/PC/CgsPcNetIdentity.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

static_assert(sizeof(PcLanHeaderT) == 20, "PcLanHeaderT is the 20-byte wire header");

namespace
{
    // How long a received control sequence number is remembered for de-duplication. It must
    // outlive the sender's whole retransmit window (PCLAN_PEER_TIMEOUT_MS).
    const uint64_t KU64_DEDUP_KEEP_MS = 2u * PCLAN_PEER_TIMEOUT_MS;
    const int32_t  KI_DEDUP_SLOTS     = 256;

    // Datagrams drained per PcLanIdle call; the rest wait for the next frame.
    const int32_t  KI_MAX_RECV_PER_IDLE = 256;

    // Log budget for this layer (lines, whole process).
    const int32_t  KI_LOG_BUDGET = 64;

    const uint32_t KU_LOOPBACK = 0x7F000001u;   // 127.0.0.1, host order
    const uint32_t KU_BROADCAST = 0xFFFFFFFFu;  // 255.255.255.255

    struct PendingT
    {
        bool        bUsed;
        PcLanPeerT  To;
        uint8_t     uType;
        uint16_t    uSeq;
        uint32_t    uGameIdent;
        uint64_t    uFirstMs;
        uint64_t    uLastMs;
        int32_t     iLen;
        uint8_t    *pBody;      // heap copy of exactly iLen bytes (null when iLen is 0)
    };

    struct PeerEntryT
    {
        bool        bUsed;
        uint64_t    uXuid;
        uint32_t    uIdent;
        PcLanPeerT  Peer;
        uint64_t    uStampMs;
    };

    struct DedupT
    {
        uint32_t    uAddr;
        uint16_t    uPort;
        uint16_t    uSeq;
        uint32_t    uSenderIdent;
        uint64_t    uMs;
    };

    struct HandlerT
    {
        PcLanHandlerT *pHandler;
        void          *pRef;
    };

    struct PcLanStateT
    {
        SOCKET      hSocket;
        bool        bWsaStarted;
        PcLanPeerT  Self;
        uint16_t    uPortBase;
        uint16_t    uNextSeq;
        uint32_t    uSelfIdent;
        PcLanStatsT Stats;
        int32_t     iLogLeft;
        int32_t     iDedupNext;
        HandlerT    aHandlers[PCLAN_NUM_TYPES];
        PendingT    aPending[PCLAN_MAX_PENDING];
        PeerEntryT  aPeers[PCLAN_MAX_PEERS];
        DedupT      aDedup[KI_DEDUP_SLOTS];
    };

    PcLanStateT gPcLan = { INVALID_SOCKET, false };

    const char *const kPcLanTypeNames[PCLAN_NUM_TYPES] =
    {
        "ACK", "DISCOVER", "ADVERT", "JOIN_REQ", "JOIN_RESP", "PLAY", "LOBBYREQ",
        "KICK", "LEAVE", "BYE", "LINK_HELLO", "LINK_ACK", "DATA", "HANDOVER"
    };

    uint64_t PcLanCoreNowMs()
    {
        return GetTickCount64();
    }

    void PcLanCoreLog(const char *pFormat, ...)
    {
        if (gPcLan.iLogLeft <= 0)
        {
            return;
        }
        char strText[256];
        int iPrefix = _snprintf_s(strText, sizeof(strText), _TRUNCATE, "[net] pclan ");
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
        if (--gPcLan.iLogLeft == 0)
        {
            strncat_s(strText, sizeof(strText), "[net] pclan (log budget spent)\n", _TRUNCATE);
        }
        CgsDev::Log::WriteToLog(strText);
    }

    const char *PcLanCoreAddrText(const PcLanPeerT *pPeer, char *pBuf, size_t uBufSize)
    {
        _snprintf_s(pBuf, uBufSize, _TRUNCATE, "%u.%u.%u.%u:%u",
                    (pPeer->uAddr >> 24) & 0xFFu, (pPeer->uAddr >> 16) & 0xFFu,
                    (pPeer->uAddr >> 8) & 0xFFu, pPeer->uAddr & 0xFFu, pPeer->uPort);
        return pBuf;
    }

    bool PcLanCoreSamePeer(const PcLanPeerT *pA, const PcLanPeerT *pB)
    {
        return (pA->uAddr == pB->uAddr) && (pA->uPort == pB->uPort);
    }

    // Close a pending slot and free its body copy.
    void PcLanCoreReleasePending(PendingT *pPending)
    {
        free(pPending->pBody);
        pPending->pBody = NULL;
        pPending->bUsed = false;
    }

    void PcLanCoreReleaseAllPending()
    {
        for (int32_t iSlot = 0; iSlot < PCLAN_MAX_PENDING; ++iSlot)
        {
            PcLanCoreReleasePending(&gPcLan.aPending[iSlot]);
        }
    }

    // BP_LAN_PORT: decimal base of the port window; anything unusable keeps the default.
    uint16_t PcLanCoreReadPortBase()
    {
        const char *pText = getenv("BP_LAN_PORT");
        if ((pText == NULL) || (pText[0] == '\0'))
        {
            return PCLAN_PORT_DEFAULT;
        }
        char *pEnd = NULL;
        long iValue = strtol(pText, &pEnd, 10);
        if ((pEnd == NULL) || (*pEnd != '\0') || (iValue < 1024) || (iValue > (65535 - PCLAN_PORT_WINDOW)))
        {
            return PCLAN_PORT_DEFAULT;
        }
        return (uint16_t)iValue;
    }

    // The address peers on other machines reach us at: the first non-loopback IPv4 address of
    // this host, else loopback.
    uint32_t PcLanCoreFindSelfAddr()
    {
        char strHost[256];
        if (gethostname(strHost, sizeof(strHost)) != 0)
        {
            return KU_LOOPBACK;
        }
        ADDRINFOA Hints;
        memset(&Hints, 0, sizeof(Hints));
        Hints.ai_family = AF_INET;
        Hints.ai_socktype = SOCK_DGRAM;
        ADDRINFOA *pList = NULL;
        if (getaddrinfo(strHost, NULL, &Hints, &pList) != 0)
        {
            return KU_LOOPBACK;
        }
        uint32_t uAddr = KU_LOOPBACK;
        for (ADDRINFOA *pInfo = pList; pInfo != NULL; pInfo = pInfo->ai_next)
        {
            if ((pInfo->ai_family != AF_INET) || (pInfo->ai_addr == NULL))
            {
                continue;
            }
            uint32_t uCandidate = ntohl(((const sockaddr_in *)pInfo->ai_addr)->sin_addr.s_addr);
            if ((uCandidate >> 24) != 127u)
            {
                uAddr = uCandidate;
                break;
            }
        }
        freeaddrinfo(pList);
        return uAddr;
    }

    int32_t PcLanCoreSendRaw(const PcLanPeerT *pTo, uint8_t uType, uint32_t uGameIdent, uint16_t uSeq,
                          const void *pBody, int32_t iLen)
    {
        uint8_t aPacket[sizeof(PcLanHeaderT) + PCLAN_MAX_BODY];
        PcLanHeaderT Header;
        Header.uMagic = PCLAN_MAGIC;
        Header.uVersion = PCLAN_VERSION;
        Header.uType = uType;
        Header.uLen = (uint16_t)iLen;
        Header.uSenderIdent = gPcLan.uSelfIdent;
        Header.uGameIdent = uGameIdent;
        Header.uSeq = uSeq;
        Header.uPad = 0;
        memcpy(aPacket, &Header, sizeof(Header));
        if (iLen > 0)
        {
            memcpy(aPacket + sizeof(Header), pBody, (size_t)iLen);
        }

        sockaddr_in To;
        memset(&To, 0, sizeof(To));
        To.sin_family = AF_INET;
        To.sin_addr.s_addr = htonl(pTo->uAddr);
        To.sin_port = htons(pTo->uPort);
        int iResult = sendto(gPcLan.hSocket, (const char *)aPacket, (int)(sizeof(Header) + iLen), 0,
                             (const sockaddr *)&To, (int)sizeof(To));
        if (iResult == SOCKET_ERROR)
        {
            return -3;
        }
        gPcLan.Stats.uSent += 1;
        return 0;
    }

    void PcLanCoreForgetAddr(const PcLanPeerT *pPeer)
    {
        for (int32_t iPeer = 0; iPeer < PCLAN_MAX_PEERS; ++iPeer)
        {
            if (gPcLan.aPeers[iPeer].bUsed && PcLanCoreSamePeer(&gPcLan.aPeers[iPeer].Peer, pPeer))
            {
                gPcLan.aPeers[iPeer].bUsed = false;
            }
        }
    }

    void PcLanCoreDropPendingTo(const PcLanPeerT *pPeer)
    {
        for (int32_t iSlot = 0; iSlot < PCLAN_MAX_PENDING; ++iSlot)
        {
            if (gPcLan.aPending[iSlot].bUsed && PcLanCoreSamePeer(&gPcLan.aPending[iSlot].To, pPeer))
            {
                PcLanCoreReleasePending(&gPcLan.aPending[iSlot]);
            }
        }
    }

    void PcLanCoreDispatch(const PcLanPeerT *pFrom, const PcLanHeaderT *pHeader, const uint8_t *pBody)
    {
        const HandlerT *pEntry = &gPcLan.aHandlers[pHeader->uType];
        if (pEntry->pHandler != NULL)
        {
            pEntry->pHandler(pEntry->pRef, pFrom, pHeader, pBody);
        }
    }

    // After a BYE (received or synthesised): nothing more is owed to that address.
    void PcLanCoreAfterBye(const PcLanPeerT *pPeer, uint32_t uIdent)
    {
        PcLanCoreDropPendingTo(pPeer);
        PcLanCoreForgetAddr(pPeer);
        if (uIdent != 0)
        {
            PcLanPeerForget(uIdent);
        }
    }

    // True when this (address, ident, seq) was already dispatched inside the keep window; the
    // first sighting is recorded.
    bool PcLanCoreSeenBefore(const PcLanPeerT *pFrom, const PcLanHeaderT *pHeader, uint64_t uNowMs)
    {
        for (int32_t iSlot = 0; iSlot < KI_DEDUP_SLOTS; ++iSlot)
        {
            const DedupT *pEntry = &gPcLan.aDedup[iSlot];
            if ((pEntry->uMs != 0) && ((uNowMs - pEntry->uMs) < KU64_DEDUP_KEEP_MS) &&
                (pEntry->uAddr == pFrom->uAddr) && (pEntry->uPort == pFrom->uPort) &&
                (pEntry->uSeq == pHeader->uSeq) && (pEntry->uSenderIdent == pHeader->uSenderIdent))
            {
                return true;
            }
        }
        DedupT *pNew = &gPcLan.aDedup[gPcLan.iDedupNext];
        gPcLan.iDedupNext = (gPcLan.iDedupNext + 1) % KI_DEDUP_SLOTS;
        pNew->uAddr = pFrom->uAddr;
        pNew->uPort = pFrom->uPort;
        pNew->uSeq = pHeader->uSeq;
        pNew->uSenderIdent = pHeader->uSenderIdent;
        pNew->uMs = (uNowMs != 0) ? uNowMs : 1;
        return false;
    }

    void PcLanCoreOnAck(const PcLanPeerT *pFrom, const PcLanHeaderT *pHeader)
    {
        for (int32_t iSlot = 0; iSlot < PCLAN_MAX_PENDING; ++iSlot)
        {
            PendingT *pPending = &gPcLan.aPending[iSlot];
            // Sequence numbers are ours and unique inside the retransmit window, so seq + port
            // identify the message even when a multi-homed peer answers from another address.
            if (pPending->bUsed && (pPending->uSeq == pHeader->uSeq) && (pPending->To.uPort == pFrom->uPort))
            {
                PcLanCoreReleasePending(pPending);
                gPcLan.Stats.uAcked += 1;
                return;
            }
        }
    }

    void PcLanCoreReceiveOne(const uint8_t *pPacket, int32_t iSize, const PcLanPeerT *pFrom, uint64_t uNowMs)
    {
        PcLanHeaderT Header;
        if (iSize < (int32_t)sizeof(Header))
        {
            gPcLan.Stats.uBad += 1;
            return;
        }
        memcpy(&Header, pPacket, sizeof(Header));
        if ((Header.uMagic != PCLAN_MAGIC) || (Header.uVersion != PCLAN_VERSION) ||
            (Header.uType >= PCLAN_NUM_TYPES) || ((int32_t)Header.uLen != (iSize - (int32_t)sizeof(Header))) ||
            (Header.uLen > PCLAN_MAX_BODY))
        {
            char strFrom[32];
            gPcLan.Stats.uBad += 1;
            PcLanCoreLog("rejected %d-byte datagram from %s", iSize, PcLanCoreAddrText(pFrom, strFrom, sizeof(strFrom)));
            return;
        }

        // Our own broadcast coming back (the sweep never targets our port, but a LAN stack may
        // still loop the limited broadcast to every local listener).
        if ((pFrom->uPort == gPcLan.Self.uPort) && (Header.uSenderIdent == gPcLan.uSelfIdent))
        {
            return;
        }

        gPcLan.Stats.uRecv += 1;
        const uint8_t *pBody = pPacket + sizeof(Header);

        if (Header.uType == PCLAN_ACK)
        {
            PcLanCoreOnAck(pFrom, &Header);
            return;
        }

        if ((Header.uType != PCLAN_DATA) && (Header.uSeq != 0))
        {
            // Every copy is acked (the sender may have lost our previous ack); only the first
            // is dispatched.
            PcLanCoreSendRaw(pFrom, PCLAN_ACK, Header.uGameIdent, Header.uSeq, NULL, 0);
            if (PcLanCoreSeenBefore(pFrom, &Header, uNowMs))
            {
                gPcLan.Stats.uDup += 1;
                return;
            }
        }

        PcLanCoreDispatch(pFrom, &Header, pBody);

        if (Header.uType == PCLAN_BYE)
        {
            PcLanCoreAfterBye(pFrom, Header.uSenderIdent);
        }
    }

    void PcLanCoreDrain(uint64_t uNowMs)
    {
        uint8_t aPacket[sizeof(PcLanHeaderT) + PCLAN_MAX_BODY + 64];
        for (int32_t iCount = 0; iCount < KI_MAX_RECV_PER_IDLE; ++iCount)
        {
            if (gPcLan.hSocket == INVALID_SOCKET)
            {
                return;   // a handler shut the layer down
            }
            sockaddr_in From;
            int iFromLen = (int)sizeof(From);
            int iSize = recvfrom(gPcLan.hSocket, (char *)aPacket, (int)sizeof(aPacket), 0, (sockaddr *)&From, &iFromLen);
            if (iSize == SOCKET_ERROR)
            {
                int iError = WSAGetLastError();
                if ((iError == WSAECONNRESET) || (iError == WSAEMSGSIZE))
                {
                    continue;   // an ICMP echo of an earlier send, or an oversized datagram
                }
                return;         // WSAEWOULDBLOCK: drained
            }
            PcLanPeerT Peer;
            Peer.uAddr = ntohl(From.sin_addr.s_addr);
            Peer.uPort = ntohs(From.sin_port);
            Peer.uPad = 0;
            PcLanCoreReceiveOne(aPacket, iSize, &Peer, uNowMs);
        }
    }

    void PcLanCoreTimeoutPeer(int32_t iSlot)
    {
        PendingT *pPending = &gPcLan.aPending[iSlot];
        PcLanPeerT Peer = pPending->To;
        uint32_t uGameIdent = pPending->uGameIdent;
        uint8_t uType = pPending->uType;

        // Every pending message to that address goes with it, before the handler runs.
        PcLanCoreDropPendingTo(&Peer);

        uint32_t uIdent = 0;
        for (int32_t iPeer = 0; iPeer < PCLAN_MAX_PEERS; ++iPeer)
        {
            if (gPcLan.aPeers[iPeer].bUsed && PcLanCoreSamePeer(&gPcLan.aPeers[iPeer].Peer, &Peer))
            {
                uIdent = gPcLan.aPeers[iPeer].uIdent;
                break;
            }
        }

        char strPeer[32];
        gPcLan.Stats.uTimeouts += 1;
        PcLanCoreLog("peer %s ident=%08X dropped: %s unacked for %d ms, BYE synthesised",
                  PcLanCoreAddrText(&Peer, strPeer, sizeof(strPeer)), uIdent, PcLanTypeName(uType), PCLAN_PEER_TIMEOUT_MS);

        PcLanHeaderT Bye;
        Bye.uMagic = PCLAN_MAGIC;
        Bye.uVersion = PCLAN_VERSION;
        Bye.uType = PCLAN_BYE;
        Bye.uLen = 0;
        Bye.uSenderIdent = uIdent;
        Bye.uGameIdent = uGameIdent;
        Bye.uSeq = 0;
        Bye.uPad = 0;
        PcLanCoreDispatch(&Peer, &Bye, NULL);
        PcLanCoreAfterBye(&Peer, uIdent);
    }

    void PcLanCoreRetransmit(uint64_t uNowMs)
    {
        for (int32_t iSlot = 0; iSlot < PCLAN_MAX_PENDING; ++iSlot)
        {
            if (gPcLan.hSocket == INVALID_SOCKET)
            {
                return;
            }
            PendingT *pPending = &gPcLan.aPending[iSlot];
            if (!pPending->bUsed)
            {
                continue;
            }
            if ((uNowMs - pPending->uFirstMs) >= PCLAN_PEER_TIMEOUT_MS)
            {
                PcLanCoreTimeoutPeer(iSlot);
                continue;
            }
            if ((uNowMs - pPending->uLastMs) >= PCLAN_RETRANSMIT_MS)
            {
                pPending->uLastMs = uNowMs;
                gPcLan.Stats.uResent += 1;
                PcLanCoreSendRaw(&pPending->To, pPending->uType, pPending->uGameIdent, pPending->uSeq,
                              pPending->pBody, pPending->iLen);
            }
        }
    }

    bool PcLanCoreOpenSocket()
    {
        SOCKET hSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (hSocket == INVALID_SOCKET)
        {
            return false;
        }

        BOOL bOn = TRUE;
        setsockopt(hSocket, SOL_SOCKET, SO_BROADCAST, (const char *)&bOn, (int)sizeof(bOn));
        setsockopt(hSocket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char *)&bOn, (int)sizeof(bOn));

        // A send to a loopback port nobody listens on comes back as WSAECONNRESET on the next
        // receive; switch that report off (the drain loop also skips it).
        BOOL bReport = FALSE;
        DWORD uReturned = 0;
        WSAIoctl(hSocket, SIO_UDP_CONNRESET, &bReport, (DWORD)sizeof(bReport), NULL, 0, &uReturned, NULL, NULL);

        u_long uNonBlocking = 1;
        ioctlsocket(hSocket, FIONBIO, &uNonBlocking);

        for (uint16_t uOffset = 0; uOffset < PCLAN_PORT_WINDOW; ++uOffset)
        {
            sockaddr_in Bind;
            memset(&Bind, 0, sizeof(Bind));
            Bind.sin_family = AF_INET;
            Bind.sin_addr.s_addr = htonl(INADDR_ANY);
            Bind.sin_port = htons((u_short)(gPcLan.uPortBase + uOffset));
            if (bind(hSocket, (const sockaddr *)&Bind, (int)sizeof(Bind)) == 0)
            {
                gPcLan.hSocket = hSocket;
                gPcLan.Self.uPort = (uint16_t)(gPcLan.uPortBase + uOffset);
                return true;
            }
        }
        closesocket(hSocket);
        return false;
    }
}

bool PcLanStartup()
{
    if (gPcLan.hSocket != INVALID_SOCKET)
    {
        return true;
    }
    if (!CgsPcNetLanEnabled())
    {
        return false;
    }

    if (!gPcLan.bWsaStarted)
    {
        WSADATA WsaData;
        if (WSAStartup(MAKEWORD(2, 2), &WsaData) != 0)
        {
            return false;
        }
        gPcLan.bWsaStarted = true;
    }

    // Handlers survive a shutdown/startup cycle (their owners registered them); everything else
    // starts clean.
    memset(&gPcLan.Stats, 0, sizeof(gPcLan.Stats));
    PcLanCoreReleaseAllPending();
    memset(gPcLan.aPeers, 0, sizeof(gPcLan.aPeers));
    memset(gPcLan.aDedup, 0, sizeof(gPcLan.aDedup));
    gPcLan.iDedupNext = 0;
    gPcLan.iLogLeft = KI_LOG_BUDGET;
    gPcLan.uSelfIdent = CgsPcNetIdentityLobbyIdent();
    gPcLan.uPortBase = PcLanCoreReadPortBase();
    // A restarted instance must not reuse the sequence numbers its previous life left in a
    // peer's de-duplication window.
    gPcLan.uNextSeq = (uint16_t)((GetTickCount64() ^ (gPcLan.uSelfIdent >> 3)) | 1u);

    if (!PcLanCoreOpenSocket())
    {
        PcLanCoreLog("no free UDP port in %u..%u, LAN stays off", gPcLan.uPortBase,
                  gPcLan.uPortBase + PCLAN_PORT_WINDOW - 1);
        WSACleanup();
        gPcLan.bWsaStarted = false;
        return false;
    }
    gPcLan.Self.uAddr = PcLanCoreFindSelfAddr();
    gPcLan.Self.uPad = 0;

    char strSelf[32];
    PcLanCoreLog("up name=%s xuid=%016llX ident=%08X bound=%s window=%u..%u",
              CgsPcNetIdentityName(), (unsigned long long)CgsPcNetIdentityXuid(), gPcLan.uSelfIdent,
              PcLanCoreAddrText(&gPcLan.Self, strSelf, sizeof(strSelf)), gPcLan.uPortBase,
              gPcLan.uPortBase + PCLAN_PORT_WINDOW - 1);
    return true;
}

void PcLanShutdown()
{
    if (gPcLan.hSocket == INVALID_SOCKET)
    {
        return;
    }
    // Tell every known peer, once, unreliably: there is nobody left to retransmit.
    for (int32_t iPeer = 0; iPeer < PCLAN_MAX_PEERS; ++iPeer)
    {
        if (gPcLan.aPeers[iPeer].bUsed)
        {
            PcLanCoreSendRaw(&gPcLan.aPeers[iPeer].Peer, PCLAN_BYE, 0, 0, NULL, 0);
        }
    }
    PcLanCoreLog("down sent=%u resent=%u recv=%u dup=%u acked=%u timeouts=%u bad=%u",
              gPcLan.Stats.uSent, gPcLan.Stats.uResent, gPcLan.Stats.uRecv, gPcLan.Stats.uDup,
              gPcLan.Stats.uAcked, gPcLan.Stats.uTimeouts, gPcLan.Stats.uBad);
    closesocket(gPcLan.hSocket);
    gPcLan.hSocket = INVALID_SOCKET;
    PcLanCoreReleaseAllPending();
    memset(gPcLan.aPeers, 0, sizeof(gPcLan.aPeers));
    if (gPcLan.bWsaStarted)
    {
        WSACleanup();
        gPcLan.bWsaStarted = false;
    }
}

bool PcLanActive()
{
    return gPcLan.hSocket != INVALID_SOCKET;
}

PcLanPeerT PcLanSelf()
{
    if (gPcLan.hSocket == INVALID_SOCKET)
    {
        PcLanPeerT None = { 0, 0, 0 };
        return None;
    }
    return gPcLan.Self;
}

void PcLanIdle()
{
    if (gPcLan.hSocket == INVALID_SOCKET)
    {
        return;
    }
    const uint64_t uNowMs = PcLanCoreNowMs();
    PcLanCoreDrain(uNowMs);
    PcLanCoreRetransmit(PcLanCoreNowMs());
}

void PcLanRegister(uint8_t uType, PcLanHandlerT *pHandler, void *pRef)
{
    if (uType >= PCLAN_NUM_TYPES)
    {
        return;
    }
    gPcLan.aHandlers[uType].pHandler = pHandler;
    gPcLan.aHandlers[uType].pRef = (pHandler != NULL) ? pRef : NULL;
}

int32_t PcLanSendControl(const PcLanPeerT *pTo, uint8_t uType, uint32_t uGameIdent, const void *pBody, int32_t iLen)
{
    if (gPcLan.hSocket == INVALID_SOCKET)
    {
        return -1;
    }
    if ((pTo == NULL) || (iLen < 0) || (iLen > PCLAN_MAX_BODY) || ((iLen > 0) && (pBody == NULL)) ||
        (uType == PCLAN_ACK) || (uType == PCLAN_DATA) || (uType >= PCLAN_NUM_TYPES))
    {
        return -2;
    }

    int32_t iFree = -1;
    for (int32_t iSlot = 0; iSlot < PCLAN_MAX_PENDING; ++iSlot)
    {
        if (!gPcLan.aPending[iSlot].bUsed)
        {
            iFree = iSlot;
            break;
        }
    }
    if (iFree < 0)
    {
        PcLanCoreLog("pending table full, %s not sent", PcLanTypeName(uType));
        return -4;
    }

    if (gPcLan.uNextSeq == 0)
    {
        gPcLan.uNextSeq = 1;
    }
    // The retransmit copy is sized to the message (a body may be up to PCLAN_MAX_BODY, most are
    // a few dozen bytes), so the table itself stays small.
    uint8_t *pCopy = NULL;
    if (iLen > 0)
    {
        pCopy = (uint8_t *)malloc((size_t)iLen);
        if (pCopy == NULL)
        {
            PcLanCoreLog("out of memory for a %d-byte %s", iLen, PcLanTypeName(uType));
            return -4;
        }
        memcpy(pCopy, pBody, (size_t)iLen);
    }

    const uint16_t uSeq = gPcLan.uNextSeq++;
    const uint64_t uNowMs = PcLanCoreNowMs();

    PendingT *pPending = &gPcLan.aPending[iFree];
    pPending->bUsed = true;
    pPending->To = *pTo;
    pPending->To.uPad = 0;
    pPending->uType = uType;
    pPending->uSeq = uSeq;
    pPending->uGameIdent = uGameIdent;
    pPending->uFirstMs = uNowMs;
    pPending->uLastMs = uNowMs;
    pPending->iLen = iLen;
    pPending->pBody = pCopy;

    // A refused first transmission stays queued: the retransmit carries it, and a peer that
    // stays unreachable times out like any other.
    PcLanCoreSendRaw(pTo, uType, uGameIdent, uSeq, pBody, iLen);
    return 0;
}

int32_t PcLanSendData(const PcLanPeerT *pTo, uint32_t uGameIdent, const void *pBody, int32_t iLen)
{
    if (gPcLan.hSocket == INVALID_SOCKET)
    {
        return -1;
    }
    if ((pTo == NULL) || (iLen < 0) || (iLen > PCLAN_MAX_BODY) || ((iLen > 0) && (pBody == NULL)))
    {
        return -2;
    }
    return PcLanCoreSendRaw(pTo, PCLAN_DATA, uGameIdent, 0, pBody, iLen);
}

int32_t PcLanBroadcast(uint8_t uType, uint32_t uGameIdent, const void *pBody, int32_t iLen)
{
    if (gPcLan.hSocket == INVALID_SOCKET)
    {
        return -1;
    }
    if ((iLen < 0) || (iLen > PCLAN_MAX_BODY) || ((iLen > 0) && (pBody == NULL)) || (uType >= PCLAN_NUM_TYPES))
    {
        return -2;
    }

    // Loopback reaches every instance on this machine; the limited broadcast reaches the LAN
    // (it may be refused on a host with no network, which only matters if loopback failed too).
    int32_t iDelivered = 0;
    const uint32_t aTargets[2] = { KU_LOOPBACK, KU_BROADCAST };
    for (int32_t iTarget = 0; iTarget < 2; ++iTarget)
    {
        for (uint16_t uOffset = 0; uOffset < PCLAN_PORT_WINDOW; ++uOffset)
        {
            PcLanPeerT To;
            To.uAddr = aTargets[iTarget];
            To.uPort = (uint16_t)(gPcLan.uPortBase + uOffset);
            To.uPad = 0;
            if (To.uPort == gPcLan.Self.uPort)
            {
                continue;
            }
            if (PcLanCoreSendRaw(&To, uType, uGameIdent, 0, pBody, iLen) == 0)
            {
                iDelivered += 1;
            }
        }
    }
    return (iDelivered > 0) ? 0 : -3;
}

void PcLanPeerRemember(uint64_t uXuid, uint32_t uIdent, const PcLanPeerT *pPeer)
{
    if ((pPeer == NULL) || ((uXuid == 0) && (uIdent == 0)))
    {
        return;
    }
    const uint64_t uNowMs = PcLanCoreNowMs();

    int32_t iMatch = -1;
    for (int32_t iPeer = 0; iPeer < PCLAN_MAX_PEERS; ++iPeer)
    {
        const PeerEntryT *pEntry = &gPcLan.aPeers[iPeer];
        if (!pEntry->bUsed)
        {
            continue;
        }
        if (((uIdent != 0) && (pEntry->uIdent == uIdent)) || ((uXuid != 0) && (pEntry->uXuid == uXuid)))
        {
            iMatch = iPeer;
            break;
        }
    }
    if (iMatch < 0)
    {
        uint64_t uOldestMs = ~(uint64_t)0;
        for (int32_t iPeer = 0; iPeer < PCLAN_MAX_PEERS; ++iPeer)
        {
            const PeerEntryT *pEntry = &gPcLan.aPeers[iPeer];
            if (!pEntry->bUsed)
            {
                iMatch = iPeer;
                break;
            }
            if (pEntry->uStampMs < uOldestMs)
            {
                uOldestMs = pEntry->uStampMs;
                iMatch = iPeer;
            }
        }
    }

    PeerEntryT *pEntry = &gPcLan.aPeers[iMatch];
    const bool bNew = !pEntry->bUsed || !PcLanCoreSamePeer(&pEntry->Peer, pPeer) ||
                      ((uIdent != 0) && (pEntry->uIdent != uIdent));
    if (!pEntry->bUsed)
    {
        pEntry->uXuid = 0;
        pEntry->uIdent = 0;
    }
    pEntry->bUsed = true;
    if (uXuid != 0)
    {
        pEntry->uXuid = uXuid;
    }
    if (uIdent != 0)
    {
        pEntry->uIdent = uIdent;
    }
    pEntry->Peer = *pPeer;
    pEntry->Peer.uPad = 0;
    pEntry->uStampMs = uNowMs;

    if (bNew)
    {
        char strPeer[32];
        PcLanCoreLog("peer %s ident=%08X xuid=%016llX remembered", PcLanCoreAddrText(pPeer, strPeer, sizeof(strPeer)),
                  pEntry->uIdent, (unsigned long long)pEntry->uXuid);
    }
}

bool PcLanPeerByXuid(uint64_t uXuid, PcLanPeerT *pPeer)
{
    if (uXuid == 0)
    {
        return false;
    }
    for (int32_t iPeer = 0; iPeer < PCLAN_MAX_PEERS; ++iPeer)
    {
        if (gPcLan.aPeers[iPeer].bUsed && (gPcLan.aPeers[iPeer].uXuid == uXuid))
        {
            if (pPeer != NULL)
            {
                *pPeer = gPcLan.aPeers[iPeer].Peer;
            }
            return true;
        }
    }
    return false;
}

bool PcLanPeerByIdent(uint32_t uIdent, PcLanPeerT *pPeer)
{
    if (uIdent == 0)
    {
        return false;
    }
    for (int32_t iPeer = 0; iPeer < PCLAN_MAX_PEERS; ++iPeer)
    {
        if (gPcLan.aPeers[iPeer].bUsed && (gPcLan.aPeers[iPeer].uIdent == uIdent))
        {
            if (pPeer != NULL)
            {
                *pPeer = gPcLan.aPeers[iPeer].Peer;
            }
            return true;
        }
    }
    return false;
}

void PcLanPeerForget(uint32_t uIdent)
{
    if (uIdent == 0)
    {
        return;
    }
    for (int32_t iPeer = 0; iPeer < PCLAN_MAX_PEERS; ++iPeer)
    {
        if (gPcLan.aPeers[iPeer].bUsed && (gPcLan.aPeers[iPeer].uIdent == uIdent))
        {
            gPcLan.aPeers[iPeer].bUsed = false;
        }
    }
}

void PcLanGetStats(PcLanStatsT *pStats)
{
    if (pStats == NULL)
    {
        return;
    }
    *pStats = gPcLan.Stats;
    pStats->uPending = 0;
    for (int32_t iSlot = 0; iSlot < PCLAN_MAX_PENDING; ++iSlot)
    {
        if (gPcLan.aPending[iSlot].bUsed)
        {
            pStats->uPending += 1;
        }
    }
}

const char *PcLanTypeName(uint8_t uType)
{
    return (uType < PCLAN_NUM_TYPES) ? kPcLanTypeNames[uType] : "?";
}
