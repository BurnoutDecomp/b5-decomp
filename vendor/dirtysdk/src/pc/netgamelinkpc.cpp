// ============================================================================
// netgamelinkpc.cpp -- NetGameLink send / receive / status for the PC backend.
//
// [PC platform layer] The console link ran its own sequencing over a secure peer socket. On
// the PC a link is a pclan peer (lan/pclan_link.cpp): a sent packet becomes one PCLAN_DATA
// datagram, a received one waits in the link's receive ring. The packet framing the game sees is
// the console's: head.size = the packet footprint rounded up to 4 bytes, packets laid out
// head.size apart when several are moved in one call.
// ============================================================================

#include "netgamelink.h"
#include "lan/pclan_link.h"

#include <string.h>

namespace
{
    // Header + payload + trailer, rounded up to 4 bytes.
    u32 NetGameLinkPacketSize(u32 uLen)
    {
        return (uLen + 23u) & 0x7FFCu;
    }

    // Per-datagram cost below the link: the pclan header on top of the link framing, and the
    // UDP/IPv4 headers on the wire.
    const u32 KU_TRANSPORT_FRAMING = (u32)(sizeof(PcLanHeaderT) + sizeof(PcLinkDataHeadT));
    const u32 KU_DATAGRAM_OVERHEAD = 28u;

    // Recompute the per-second rates once at least a second has passed since the last sample.
    void NetGameLinkSampleRates(NetGameLinkRefT* pLink, u32 uNow)
    {
        NetGameLinkStatT* pStat = &pLink->Stat;
        const u32 uElapsed = uNow - pStat->stattick;
        if (uElapsed < 1000u)
        {
            return;
        }
        const u32 uRawBytes = pLink->uSampleBytes + (pLink->uSamplePkts * KU_TRANSPORT_FRAMING);
        const u32 uNetBytes = uRawBytes + (pLink->uSamplePkts * KU_DATAGRAM_OVERHEAD);
        pStat->outbps   = (s32)((1000u * pLink->uSampleBytes) / uElapsed);
        pStat->outrps   = (s32)((1000u * uRawBytes) / uElapsed);
        pStat->outnps   = (s32)((1000u * uNetBytes) / uElapsed);
        pStat->outpps   = (s32)((1000u * pLink->uSamplePkts + 500u) / uElapsed);
        pStat->outrpps  = pStat->outpps;
        pStat->stattick = uNow;
        pLink->uSampleBytes = 0;
        pLink->uSamplePkts  = 0;
    }

    void NetGameLinkUpdate(NetGameLinkRefT* pLink, u32 uNow)
    {
        pLink->Stat.tick = uNow;
        pLink->Stat.tickseqn += 1;
        NetGameLinkSampleRates(pLink, uNow);
    }
}

extern "C" s32 NetGameLinkSend(NetGameLinkRefT* pRef, NetGamePacketT* pPacket, s32 iLen)
{
    const u32 uNow = PcLinkTick();
    s32 iSent = 0;
    u8* pCursor = reinterpret_cast<u8*>(pPacket);

    while (iLen > 0)
    {
        NetGamePacketT* pCurrent = reinterpret_cast<NetGamePacketT*>(pCursor);
        const s32 iResult = PcLinkSendData(pRef, pCurrent->body.data, pCurrent->head.len, pCurrent->head.kind);
        if (iResult < 0)
        {
            break;
        }
        const u32 uSize = NetGameLinkPacketSize(pCurrent->head.len);
        pCurrent->head.size = uSize;
        iSent += (s32)uSize;
        pRef->uSampleBytes += pCurrent->head.len;
        pRef->uSamplePkts  += 1;
        if (iLen == 1)
        {
            break;
        }
        iLen    -= (s32)uSize;
        pCursor += uSize;
    }

    NetGameLinkUpdate(pRef, uNow);
    return iSent;
}

extern "C" s32 NetGameLinkRecv(NetGameLinkRefT* pRef, NetGamePacketT* pBuffer, s32 iLen)
{
    NetGameLinkUpdate(pRef, PcLinkTick());

    s32 iCopied = 0;
    u8* pCursor = reinterpret_cast<u8*>(pBuffer);
    while (pRef->iRecvCount > 0)
    {
        const PcLinkPacketT* pQueued = &pRef->aRecv[pRef->iRecvHead];
        const u32 uSize = NetGameLinkPacketSize(pQueued->uLen);
        // One packet when iLen == 1, otherwise as many whole packets as fit.
        if ((iLen != 1) && ((u32)iCopied + uSize > (u32)iLen))
        {
            break;
        }
        if (pBuffer != NULL)
        {
            NetGamePacketHeadT Head;
            Head.size = uSize;
            Head.len  = pQueued->uLen;
            Head.kind = pQueued->uKind;
            Head.pad  = 0;
            memcpy(pCursor, &Head, sizeof(Head));
            memcpy(pCursor + sizeof(Head), pQueued->aData, pQueued->uLen);
            memset(pCursor + sizeof(Head) + pQueued->uLen, 0, uSize - sizeof(Head) - pQueued->uLen);
        }
        iCopied += (s32)uSize;
        pCursor += uSize;
        pRef->iRecvHead   = (pRef->iRecvHead + 1) % PCLINK_RECV_SLOTS;
        pRef->iRecvCount -= 1;
        if (iLen == 1)
        {
            break;
        }
    }
    return iCopied;
}

extern "C" const NetGameLinkStatT* NetGameLinkStatus(NetGameLinkRefT* pRef)
{
    NetGameLinkUpdate(pRef, PcLinkTick());
    pRef->Stat.isconn = (pRef->bUp && !pRef->bDown) ? 1 : 0;
    pRef->Stat.isopen = pRef->bDown ? 0 : 1;
    return &pRef->Stat;
}
