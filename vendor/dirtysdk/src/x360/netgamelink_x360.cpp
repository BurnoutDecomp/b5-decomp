// DirtySDK 5.5.3 NetGameLink native X360 implementation.
#include "netgamelink.h"
#include "dirtylib_x360_internal.h"
#include "commall_x360_internal.h"
#include "dirtylib.h"
#include "dirtymem.h"
#include <cstddef>
#include <cstring>

struct NetGameLinkRefT
{
    CommRef* m_port;
    s32 m_portowner, m_memgroup;
    u32 m_lastsend, m_lastrecv, m_lastecho, m_lastsync, m_lasthist;
    s32 m_rttavg, m_rttdev;
    NetGameLinkStatT m_stats;
    s32 m_inpbyte, m_inppack, m_inprawb, m_inprawp, m_overhead;
    char* m_buffer;
    s32 m_buflen, m_bufmax;
    NetCritT crit;
    s32 m_process;
    void* m_calldata;
    void (*m_callproc)(void*, s32);
    s32 m_callpend, m_recvprocct, m_sendenabled;
};
CGS_STATIC_ASSERT((offsetof(NetGameLinkStatT, sent) == 0x14), "stat sent");
CGS_STATIC_ASSERT((offsetof(NetGameLinkStatT, sentshow) == 0x24), "stat sent display");
CGS_STATIC_ASSERT((offsetof(NetGameLinkStatT, pingtick) == 0x5c), "stat ping tick");
CGS_STATIC_ASSERT((offsetof(NetGameLinkStatT, pinghist) == 0x70), "stat history");
CGS_STATIC_ASSERT((offsetof(CommRef, Status) == 0x24), "comm status");
CGS_STATIC_ASSERT((offsetof(CommRef, packsent) == 0x60), "comm packet counts");
CGS_STATIC_ASSERT((offsetof(NetGameLinkRefT, m_stats) == 0x28), "link stats");
CGS_STATIC_ASSERT((offsetof(NetGameLinkRefT, m_buffer) == 0x1ac), "link buffer");
CGS_STATIC_ASSERT((offsetof(NetGameLinkRefT, crit) == 0x1b8), "link crit");
CGS_STATIC_ASSERT((offsetof(NetGameLinkRefT, m_recvprocct) == 0x1f0), "link receive processing count");
CGS_STATIC_ASSERT((sizeof(NetGameLinkRefT) == 0x1f8), "ARTIST link allocation");

// ARTIST 0x82B1DBE0.
extern "C" const NetGameLinkStatT* NetGameLinkStatus(NetGameLinkRefT* link)
{
    u32 seq;
    do
    {
        seq = link->m_stats.tickseqn;
        link->m_stats.tick = NetTick();
    }
    while (link->m_stats.tickseqn != seq);
    s32 status = link->m_port->Status(link->m_port);
    link->m_stats.isconn = (status == 2);
    link->m_stats.isopen = (status == 2 || status == 3);
    link->m_stats.aTransport[0] = link->m_port->packsent;
    link->m_stats.aTransport[1] = link->m_port->packrcvd;
    link->m_stats.aTransport[2] = link->m_port->packlost;
    return &link->m_stats;
}

extern "C" u32 SocketHtonl(u32);
extern "C" u16 SocketHtons(u16);
extern "C" u32 SocketNtohl(u32);
extern "C" u16 SocketNtohs(u16);

namespace {
struct PacketSync { u32 repl, echo; s16 late; };

s32 SendPacket(NetGameLinkRefT* ref, NetGamePacketT* pkt, u32 tick)
{
    if (!ref->m_sendenabled) return 1;
    if (pkt->head.len > ref->m_port->maxwid)
    {
        NetPrintfCode("netgamelink: oversized packet send (%d bytes)\n", pkt->head.len);
        return -1;
    }
    bool synced = false;
    if ((pkt->head.kind & 0x40) || tick - ref->m_lastsync > 250)
    {
        PacketSync sync;
        sync.echo = SocketHtonl(tick);
        sync.repl = SocketHtonl(ref->m_lastecho - ref->m_lastrecv + tick);
        sync.late = static_cast<s16>(SocketHtons(static_cast<u16>((ref->m_rttdev + ref->m_rttavg + 1) / 2)));
        std::memcpy(pkt->body.data + pkt->head.len, &sync, 10);
        pkt->head.len += 10;
        pkt->head.kind |= 0x40;
        synced = true;
    }
    pkt->body.data[pkt->head.len] = pkt->head.kind;
    u32 kind = pkt->head.kind & ~0x40u;
    if ((kind < 1 || kind > 8) && pkt->head.kind != 0x40)
        NetPrintfCode("netgamelink: warning -- send unrecognized packet kind %d\n", pkt->head.kind);
    u32 flags = kind == 7 ? 1 : (kind == 8 ? 3 : 0);
    s32 result = ref->m_port->Send(ref->m_port, pkt->body.data, pkt->head.len + 1, flags);
    if (synced) { pkt->head.kind ^= 0x40; pkt->head.len -= 10; }
    if (result > 0)
    {
        ref->m_lastsend = tick;
        if (synced) ref->m_lastsync = tick;
        ref->m_inpbyte += pkt->head.len;
        ++ref->m_inppack;
        ref->m_stats.sentlast = tick;
        ref->m_stats.sent += pkt->head.len;
        if (ref->m_stats.sentshow && tick - ref->m_stats.sentshow > 100)
        { ref->m_stats.sentshow = 0; ref->m_stats.senthide = tick; }
        if (ref->m_stats.senthide && tick - ref->m_stats.senthide > 100)
        { ref->m_stats.senthide = 0; ref->m_stats.sentshow = tick; }
    }
    return result;
}

s32 RecvPacket(NetGameLinkRefT* ref, u32 tick)
{
    s32 capacity = ref->m_bufmax - ref->m_buflen - 8;
    if (capacity <= 0) return 0;
    if (capacity > ref->m_port->maxwid) capacity = ref->m_port->maxwid;
    NetGamePacketT* pkt = reinterpret_cast<NetGamePacketT*>(ref->m_buffer + ref->m_buflen);
    u32 when;
    s32 size = ref->m_port->Recv(ref->m_port, pkt->body.data, capacity, &when);
    if (size <= 0 || size > ref->m_port->maxwid)
    {
        if (size != -7 && size != -6) NetPrintfCode("netgamelink: Recv() returned %d\n", size);
        return 0;
    }
    pkt->head.len = static_cast<u16>(size);
    ref->m_stats.tick = tick;
    ref->m_stats.rcvdlast = tick;
    ++ref->m_stats.tickseqn;
    ref->m_stats.rcvd += pkt->head.len;
    if (ref->m_stats.rcvdshow && tick - ref->m_stats.rcvdshow > 100)
    { ref->m_stats.rcvdshow = 0; ref->m_stats.rcvdhide = tick; }
    if (ref->m_stats.rcvdhide && tick - ref->m_stats.rcvdhide > 100)
    { ref->m_stats.rcvdhide = 0; ref->m_stats.rcvdshow = tick; }
    --pkt->head.len;
    pkt->head.kind = pkt->body.data[pkt->head.len];
    u32 kind = pkt->head.kind & ~0x40u;
    if ((kind < 1 || kind > 8) && pkt->head.kind != 0x40)
        NetPrintfCode("netgamelink: warning -- recv unrecognized packet kind %d\n", pkt->head.kind);
    if (pkt->head.kind & 0x40)
    {
        pkt->head.len -= 10;
        PacketSync sync;
        std::memcpy(&sync, pkt->body.data + pkt->head.len, 10);
        pkt->head.kind ^= 0x40;
        s32 elapsed = when - ref->m_lastrecv;
        if (elapsed < 10) elapsed = 10;
        ref->m_lastrecv = when;
        ref->m_lastecho = SocketNtohl(sync.echo);
        ref->m_stats.late = SocketNtohs(static_cast<u16>(sync.late));
        s32 delta = static_cast<s16>(ref->m_lastrecv - SocketNtohl(sync.repl));
        if (delta >= 0 && delta <= 2500)
        {
            if (elapsed >= 1024) { ref->m_rttavg = delta; ref->m_rttdev = 0; }
            else
            {
                s32 weight = 1024 - elapsed;
                s32 deviate = delta - ref->m_rttavg;
                if (deviate < 0) deviate = -deviate;
                ref->m_rttdev = (ref->m_rttdev * weight + deviate * elapsed) / 1024;
                ref->m_rttavg = (ref->m_rttavg * weight + delta * elapsed) / 1024;
            }
            ref->m_stats.ping = ref->m_rttavg + ref->m_rttdev;
            NetGameLinkHistT* history;
            if (when - ref->m_lasthist < 250)
            {
                history = &ref->m_stats.pinghist[ref->m_stats.pingslot];
                if (delta < history->min) history->min = static_cast<u16>(delta);
                if (delta > history->max) history->max = static_cast<u16>(delta);
                s32 count = history->cnt;
                history->cnt = static_cast<u16>(count + 1);
                history->avg = static_cast<u16>((history->avg * count + delta) / (count + 1));
            }
            else
            {
                ref->m_lasthist = when;
                ref->m_stats.pingslot = (ref->m_stats.pingslot + 1) & 31;
                history = &ref->m_stats.pinghist[ref->m_stats.pingslot];
                history->min = history->max = history->avg = static_cast<u16>(delta);
                history->cnt = 1;
            }
            ref->m_stats.pingtick = when;
        }
    }
    if (pkt->head.kind == 2) ref->m_port->Unconnect(ref->m_port);
    else if (pkt->head.kind)
    {
        pkt->head.size = (pkt->head.len + 11) & 0x7ffc;
        ref->m_buflen += pkt->head.size;
    }
    return 1;
}

void Process(NetGameLinkRefT* ref, u32 tick)
{
    while (ref->m_recvprocct <= 0)
    {
        ref->m_recvprocct = 1;
        s32 result = RecvPacket(ref, tick);
        ref->m_recvprocct = 0;
        if (result <= 0) break;
        ++ref->m_callpend;
    }
    u32 range = tick - ref->m_stats.stattick;
    ref->m_stats.tick = tick;
    ref->m_process = 0;
    ++ref->m_stats.tickseqn;
    if (range >= 1000)
    {
        const u32 bytes = static_cast<u32>(ref->m_port->datasent) - static_cast<u32>(ref->m_inprawb);
        const u32 packets = static_cast<u32>(ref->m_port->packsent) - static_cast<u32>(ref->m_inprawp);
        ref->m_stats.outbps = (1000u * static_cast<u32>(ref->m_inpbyte)) / range;
        ref->m_stats.outrps = (1000u * bytes) / range;
        ref->m_stats.outnps = (1000u * (bytes - static_cast<u32>(ref->m_overhead) + static_cast<u32>(ref->m_port->overhead))) / range;
        ref->m_stats.outpps = (1000u * static_cast<u32>(ref->m_inppack) + 500u) / range;
        ref->m_stats.outrpps = (1000u * packets + 500u) / range;
        ref->m_inppack = 0;
        ref->m_inpbyte = 0;
        ref->m_inprawb = ref->m_port->datasent;
        ref->m_inprawp = ref->m_port->packsent;
        ref->m_stats.stattick = tick;
        ref->m_overhead = ref->m_port->overhead;
    }
    if (ref->m_stats.sentshow && tick - ref->m_stats.sentshow > 100)
    { ref->m_stats.sentshow = 0; ref->m_stats.senthide = tick; }
    if (ref->m_stats.senthide && tick - ref->m_stats.senthide > 100 && tick - ref->m_stats.sentlast < 50)
    { ref->m_stats.senthide = 0; ref->m_stats.sentshow = tick; }
    if (ref->m_stats.rcvdshow && tick - ref->m_stats.rcvdshow > 100)
    { ref->m_stats.rcvdshow = 0; ref->m_stats.rcvdhide = tick; }
    if (ref->m_stats.rcvdhide && tick - ref->m_stats.rcvdhide > 100 && tick - ref->m_stats.rcvdlast < 50)
    { ref->m_stats.rcvdhide = 0; ref->m_stats.rcvdshow = tick; }
    if (tick - ref->m_lastsend > 500)
    {
        s32 queue = ref->m_port->Send(ref->m_port, 0, 0, 0);
        if (queue > 0 && queue < ref->m_port->maxout / 4)
        {
            NetGamePacketT pkt;
            pkt.head.len = 0;
            pkt.head.kind = 0x40;
            SendPacket(ref, &pkt, tick);
        }
    }
}

// ARTIST 0x82B1D980
void NetGameLinkNotify(CommRef* port, s32)
{
    NetGameLinkRefT* ref = static_cast<NetGameLinkRefT*>(port->refptr);
    if (NetCritTry(&ref->crit))
    {
        Process(ref, NetTick());
        if (ref->m_buflen > 0 && ref->m_callproc && ref->m_callpend > 0)
        {
            ref->m_callpend = 0;
            ref->m_callproc(ref->m_calldata, 1);
        }
        NetCritLeave(&ref->crit);
    }
    else
    {
        ++ref->m_process;
    }
}

const s32 NETGAMELINK_MEMID = 0x6e676c6b;
}

// ARTIST 0x82B1DA28
extern "C" NetGameLinkRefT* NetGameLinkCreate(void* pCommRef, s32 owner, s32 buflen)
{
    const s32 group = DirtyMemGroupQuery();
    NetGameLinkRefT* ref = static_cast<NetGameLinkRefT*>(
        DirtyMemAlloc(sizeof(NetGameLinkRefT), NETGAMELINK_MEMID, group));
    if (!ref)
    {
        NetPrintfCode("netgamelink: unable to allocate module state\n");
        return 0;
    }

    std::memset(ref, 0, sizeof(*ref));
    CommRef* port = static_cast<CommRef*>(pCommRef);
    ref->m_memgroup = group;
    ref->m_port = port;
    ref->m_portowner = owner;
    if (buflen < 4096)
        buflen = 4096;
    ref->m_buffer = static_cast<char*>(DirtyMemAlloc(buflen, NETGAMELINK_MEMID, group));
    ref->m_bufmax = buflen;
    ref->m_rttavg = 0;
    ref->m_rttdev = 0;

    const u32 tick = NetTick();
    ref->m_stats.isconn = 0;
    ref->m_stats.isopen = 0;
    ref->m_stats.conn = tick;
    ref->m_stats.senthide = tick;
    ref->m_stats.rcvdhide = tick;
    ref->m_stats.rcvdlast = tick;
    ref->m_stats.sentlast = tick;
    ref->m_stats.stattick = tick;
    for (s32 i = 0; i < 32; ++i)
    {
        NetGameLinkHistT& history = ref->m_stats.pinghist[i];
        history.min = 200;
        history.max = 200;
        history.avg = 200;
        history.cnt = 1;
    }

    NetCritInit(&ref->crit, "netgamelink");
    port->refptr = ref;
    port->Callback(port, NetGameLinkNotify);
    ref->m_sendenabled = 1;
    return ref;
}

// ARTIST 0x82B1DB58
extern "C" void NetGameLinkDestroy(NetGameLinkRefT* ref)
{
    ref->m_port->Callback(ref->m_port, 0);
    if (ref->m_portowner)
        ref->m_port->Destroy(ref->m_port);
    DirtyMemFree(ref->m_buffer, NETGAMELINK_MEMID, ref->m_memgroup);
    NetCritKill(&ref->crit);
    DirtyMemFree(ref, NETGAMELINK_MEMID, ref->m_memgroup);
}

// ARTIST 0x82B1DC90.
extern "C" s32 NetGameLinkSend(NetGameLinkRefT* ref, NetGamePacketT* pkt, s32 len)
{
    s32 consumed = 0;
    u32 tick = NetTick();
    NetCritEnter(&ref->crit);
    while (len > 0)
    {
        if (pkt->head.kind == 2) ref->m_port->Unconnect(ref->m_port);
        else
        {
            s32 result = SendPacket(ref, pkt, tick);
            if (result <= 0)
            {
                if (result) NetPrintfCode("netgamelink: error %d sending packet\n", result);
                break;
            }
        }
        s32 stride = (pkt->head.len + 23) & 0x7ffc;
        consumed += stride;
        pkt->head.size = stride;
        if (len == 1) break;
        len -= stride;
        pkt = reinterpret_cast<NetGamePacketT*>(reinterpret_cast<char*>(pkt) + stride);
    }
    if (ref->m_process > 0)
    {
        NetPrintfCode("netgamelink: processing missed event\n");
        Process(ref, tick);
    }
    NetCritLeave(&ref->crit);
    return consumed;
}

// ARTIST 0x82B1DD78.
extern "C" s32 NetGameLinkRecv(NetGameLinkRefT* ref, NetGamePacketT* buffer, s32 len)
{
    u32 tick = NetTick();
    NetCritEnter(&ref->crit);
    if (!ref->m_buflen)
    {
        while (ref->m_recvprocct <= 0)
        {
            ref->m_recvprocct = 1;
            s32 result = RecvPacket(ref, tick);
            ref->m_recvprocct = 0;
            if (result <= 0) break;
        }
    }
    s32 copied = 0;
    if (ref->m_buflen)
    {
        if (len == 1) len = *reinterpret_cast<s32*>(ref->m_buffer);
        if (ref->m_buflen > len)
        {
            s32 stride = *reinterpret_cast<s32*>(ref->m_buffer);
            if (stride <= len)
            {
                do
                {
                    copied += stride;
                    stride = *reinterpret_cast<s32*>(ref->m_buffer + copied);
                } while (stride + copied <= len);
                if (copied > 0)
                {
                    if (buffer) std::memcpy(buffer, ref->m_buffer, copied);
                    std::memmove(ref->m_buffer, ref->m_buffer + copied, ref->m_buflen - copied);
                    ref->m_buflen -= copied;
                }
            }
        }
        else
        {
            if (buffer) std::memcpy(buffer, ref->m_buffer, ref->m_buflen);
            copied = ref->m_buflen;
            ref->m_buflen = 0;
        }
    }
    NetCritLeave(&ref->crit);
    return copied;
}
