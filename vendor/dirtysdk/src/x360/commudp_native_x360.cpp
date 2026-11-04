// DirtySDK native X360 CommUDP ownership and lifecycle, ARTIST82B40310.
#include "commall_x360_internal.h"
#include "dirtylib_x360_internal.h"
#include "dirtymem.h"
#include "dirtylib.h"
#include <xtl.h>
#include <stddef.h>
#include <string.h>

struct CommUDPRef
{
    CommRef common;
    CommUDPRef* next; SocketT* socket; sockaddr peer;
    s32 state; u32 connectionId, clientId, remoteId;
    s32 recvStride, recvSize, recvHead, recvTail; u8* recvBuffer;
    u32 recvSequence, recvUnreliable, recvUnused, recvPending;
    s32 sendStride, sendSize, sendHead, sendTail, sendNext; u8* sendBuffer;
    u32 sendSequence, sendUnreliable; s32 sendError; u32 sendTick, recvTick;
    u32 reserved[2]; u32 callbackFlags; void (*callback)(CommRef*, s32);
};
struct CommUDPPacket { s32 length; u32 tick, sequence, ack, clientId, remoteId; };
CGS_STATIC_ASSERT((sizeof(CommRef) == 124), "ARTIST CommRef size");
CGS_STATIC_ASSERT((sizeof(CommUDPRef) == 260), "ARTIST CommUDP size");
CGS_STATIC_ASSERT((offsetof(CommUDPRef, common) == 0), "ARTIST CommRef base offset");
CGS_STATIC_ASSERT((offsetof(CommUDPRef, state) == 148), "ARTIST CommUDP state");
CGS_STATIC_ASSERT((offsetof(CommUDPRef, recvBuffer) == 180), "ARTIST receive buffer");
CGS_STATIC_ASSERT((offsetof(CommUDPRef, sendBuffer) == 220), "ARTIST send buffer");
CGS_STATIC_ASSERT((offsetof(CommUDPRef, callback) == 256), "ARTIST callback");
CGS_STATIC_ASSERT((sizeof(CommUDPPacket) == 24), "ARTIST packet prefix");
NetCritT gCommUDPCrit;
CommUDPRef* gCommUDPList;
s32 gCommUDPWorkPending, gCommUDPGlobalOther;
static const s32 COMMUDP_MEM_ID = 0x63756470;
extern "C" {
u32 CommUDPTick(CommUDPRef*);
s32 CommUDPListen(CommRef*, const char*);
s32 CommUDPConnect(CommRef*, const char*);
s32 CommUDPSend(CommRef*, const void*, s32, u32);
s32 SocketClose(SocketT*);
s32 SocketRecvfrom(SocketT*, char*, s32, s32, sockaddr*, s32*);
u32 SocketGetLocalAddr(void);
const char* SocketInAddrGetText(u32);
SocketT* SocketOpen(s32, s32, s32);
s32 SocketBind(SocketT*, const sockaddr*, s32);
s32 SocketInfo(SocketT*, s32, s32, void*, s32);
s32 SocketCallback(SocketT*, s32, s32, void*, s32 (*)(SocketT*, s32, void*));
s32 SockaddrInParse2(u32*, s32*, s32*, const char*);
s32 SockaddrCompare(const sockaddr*, const sockaddr*);
s32 NetHash(const char*);
// ARTIST82B3EB40: native receive/timer pump.
s32 CommUDPPumpNative(u32);
s32 SocketSendto(SocketT*, const char*, s32, s32, const sockaddr*, s32);
u32 SocketHtonl(u32); u32 SocketNtohl(u32);
}

// CommRef is the offset-zero base of the native CommUDP object.
static u32 CommUDPTickAdapter(CommRef* common)
{
    return CommUDPTick(reinterpret_cast<CommUDPRef*>(common));
}

// The original Unresolve slot is the shared single-instruction return at 82AD5078.
static void CommUDPOriginalUnresolve(CommRef*) {}
extern "C" s32 CommUDPResolve(CommRef*, const char*, char*, s32, char)
{
    NetPrintfCode("commudp: resolve functionality not supported\n"); return -1;
}
extern "C" void CommUDPCallback(CommRef* common, void (*callback)(CommRef*, s32))
{
    CommUDPRef* ref = reinterpret_cast<CommUDPRef*>(common);
    NetCritEnter(&gCommUDPCrit);
    ref->callback = callback; ref->callbackFlags |= 2;
    NetCritLeave(&gCommUDPCrit);
}
extern "C" s32 CommTCPStatus(CommRef* common)
{
    s32 state = reinterpret_cast<CommUDPRef*>(common)->state;
    if (state == 2 || state == 3) return 2;
    if (state == 1 || state == 5) return 1;
    return state == 4 ? 3 : 4;
}
// ARTIST82B3FAD0, export hole: only the two native id selectors.
extern "C" s32 CommUDPControl(CommRef* common, s32 selector, s32 value, void*)
{
    CommUDPRef* ref = reinterpret_cast<CommUDPRef*>(common);
    if (selector == 0x636c6964) { ref->clientId = value; return 0; }
    if (selector == 0x72636964) { ref->remoteId = value; return 0; }
    return -1;
}
extern "C" s32 CommUDPPeek(CommRef* common, void* data, s32 length, u32* tick)
{
    CommUDPRef* ref = reinterpret_cast<CommUDPRef*>(common);
    if (ref->recvTail == ref->recvHead) return -7;
    CommUDPPacket* packet = reinterpret_cast<CommUDPPacket*>(ref->recvBuffer + ref->recvTail);
    if (length > 0)
    {
        if (length < packet->length) return -6;
        memcpy(data, packet + 1, packet->length);
    }
    if (tick) *tick = packet->tick;
    return packet->length;
}
extern "C" s32 CommUDPRecv(CommRef* common, void* data, s32 length, u32* tick)
{
    CommUDPRef* ref = reinterpret_cast<CommUDPRef*>(common);
    s32 result = CommUDPPeek(common, data, length, tick);
    if (result >= 0) ref->recvTail = (ref->recvStride + ref->recvTail) % ref->recvSize;
    return result;
}

// ARTIST82B3DFD8: transmit the 16-byte protocol header plus queued payload.
static s32 CommUDPSendPacket(CommUDPRef* ref, CommUDPPacket* packet, const sockaddr* target, u32 tick)
{
    const s32 size = packet->length + 16;
    packet->sequence = SocketHtonl(packet->sequence); packet->ack = SocketHtonl(packet->ack);
    packet->clientId = SocketHtonl(ref->clientId); packet->remoteId = SocketHtonl(ref->remoteId);
    s32 sent = SocketSendto(ref->socket, reinterpret_cast<const char*>(&packet->sequence), size, 0, target, 16);
    packet->sequence = SocketNtohl(packet->sequence); packet->ack = SocketNtohl(packet->ack);
    if (sent != size)
    {
        NetPrintfCode("commudp: SocketSendto() returned %d\n", sent); ref->sendError = sent; return -1;
    }
    ++ref->common.packsent; ref->common.datasent += size; ref->sendTick = tick;
    s32 header = 32;
    if (ref->common.hostport == 1000 && ref->common.peerport == 1000) header = 28;
    else if (ref->common.hostport > 1000 && ref->common.hostport < 1255 &&
             ref->common.peerport > 1000 && ref->common.peerport < 1255) header = 30;
    ref->common.overhead += (8 - size % 8) % 8 + header + 16;
    if (packet->sequence >= 256) ref->recvPending = 0;
    return sent;
}
// ARTIST82B3E358: combine unsent packets and selected unacknowledged
// payloads in the native global scratch frame before sending.
struct CommUDPAggregate { CommUDPPacket packet; u8 data[1256]; };
static CommUDPAggregate gCommUDPAggregate;
static u32 gCommUDPAggregateThreshold = 0x20000000;
static void CommUDPTransmitPendingNative(CommUDPRef* ref, u32 tick)
{
    s32 budget = 2048;
    for (s32 offset = ref->sendTail; offset != ref->sendNext; offset = (offset + ref->sendStride) % ref->sendSize)
        budget -= reinterpret_cast<CommUDPPacket*>(ref->sendBuffer + offset)->length;
    if (budget < 256 && tick - ref->sendTick > 250) budget = 256;
    if (budget <= 0) return;
    do
    {
        s32 available = SocketInfo(0, 0x6d617870, 0, 0, 0) - 16;
        if (available > budget) available = budget;
        s32 count = 0;
        while (available > 0 && ref->sendNext != ref->sendHead)
        {
            CommUDPPacket* packet = reinterpret_cast<CommUDPPacket*>(ref->sendBuffer + ref->sendNext);
            available -= packet->length + 1;
            if (count > 0 && available <= 0) break;
            ++count;
            ref->sendNext = (ref->sendNext + ref->sendStride) % ref->sendSize;
            if (packet->length > 250 || count >= 8) break;
        }
        if (!count) break;
        s32 offset = (ref->sendNext - ref->sendStride + ref->sendSize) % ref->sendSize;
        CommUDPPacket* packet = reinterpret_cast<CommUDPPacket*>(ref->sendBuffer + offset);
        memcpy(&gCommUDPAggregate, packet, packet->length + 24);
        while (--count > 0)
        {
            offset = (ref->sendSize - ref->sendStride + offset) % ref->sendSize;
            packet = reinterpret_cast<CommUDPPacket*>(ref->sendBuffer + offset);
            if (ref->common.SendCallback) ref->common.SendCallback(&ref->common, packet + 1, packet->length, 0);
            gCommUDPAggregate.packet.sequence += 0x10000000;
            memcpy(gCommUDPAggregate.data + gCommUDPAggregate.packet.length, packet + 1, packet->length);
            gCommUDPAggregate.packet.length += packet->length;
            gCommUDPAggregate.data[gCommUDPAggregate.packet.length++] = static_cast<u8>(packet->length);
        }
        u32 threshold = gCommUDPAggregateThreshold;
        while (offset != ref->sendTail && gCommUDPAggregate.packet.sequence <= threshold)
        {
            offset = (ref->sendSize - ref->sendStride + offset) % ref->sendSize;
            packet = reinterpret_cast<CommUDPPacket*>(ref->sendBuffer + offset);
            if (gCommUDPAggregate.packet.length + packet->length > 64) break;
            if (ref->common.SendCallback)
            {
                ref->common.SendCallback(&ref->common, packet + 1, packet->length, 0);
                threshold = gCommUDPAggregateThreshold;
            }
            gCommUDPAggregate.packet.sequence += 0x10000000;
            memcpy(gCommUDPAggregate.data + gCommUDPAggregate.packet.length, packet + 1, packet->length);
            gCommUDPAggregate.packet.length += packet->length;
            gCommUDPAggregate.data[gCommUDPAggregate.packet.length++] = static_cast<u8>(packet->length);
        }
        gCommUDPAggregateThreshold = offset == ref->sendTail ? 0x20000000 :
            (threshold >= 0x80000000 ? 0xf0000000 : threshold * 2);
        ref->recvUnused = ref->recvSequence;
        gCommUDPAggregate.packet.ack = ref->recvSequence - 1;
        if (CommUDPSendPacket(ref, &gCommUDPAggregate.packet, &ref->peer, tick) < 0) break;
        budget -= gCommUDPAggregate.packet.length;
    } while (budget > 0);
}

// ARTIST82B3EA58: flush each packet from unacknowledged through sendNext.
static s32 CommUDPFlush(CommUDPRef* ref, s32, u32 tick)
{
    s32 count = ((ref->sendHead - ref->sendTail + ref->sendSize) % ref->sendSize) / ref->sendStride;
    NetPrintfCode("commudp: flushing %d packets in send queue\n", count);
    for (s32 offset = ref->sendTail; offset != ref->sendNext; offset = (offset + ref->sendStride) % ref->sendSize)
        CommUDPSendPacket(ref, reinterpret_cast<CommUDPPacket*>(ref->sendBuffer + offset), &ref->peer, tick);
    return count;
}
// ARTIST82B3E150: transmit close frame only from an established state.
static void CommUDPCloseConnection(CommUDPRef* ref, u32 tick)
{
    if (ref->state == 4)
    {
        if (ref->sendNext != ref->sendHead) NetPrintfCode("commudp: unsent data pending\n");
        else if (ref->sendTail != ref->sendHead) NetPrintfCode("commudp: unacked data pending\n");
        CommUDPPacket packet;
        packet.length = 0; packet.sequence = 3; packet.ack = ref->connectionId;
        CommUDPSendPacket(ref, &packet, &ref->peer, tick);
    }
    NetPrintfCode("commudp: closed connection\n"); ref->connectionId = 0; ref->state = 5;
}
static s32 CommUDPDisconnect(CommUDPRef* ref)
{
    u32 tick = NetTick(); CommUDPFlush(ref, 200, tick);
    if (ref->state == 3) { ref->socket = 0; ref->common.sockptr = 0; }
    if (ref->socket)
    {
        CommUDPCloseConnection(ref, tick); SocketClose(ref->socket);
        ref->socket = 0; ref->common.sockptr = 0;
    }
    ref->state = 1; return 0;
}
extern "C" s32 CommUDPUnlisten(CommRef* ref) { return CommUDPDisconnect(reinterpret_cast<CommUDPRef*>(ref)); }
extern "C" s32 CommUDPUnconnect(CommRef* ref) { return CommUDPDisconnect(reinterpret_cast<CommUDPRef*>(ref)); }
extern "C" void CommUDPDestroy(CommRef* common)
{
    CommUDPRef* ref = reinterpret_cast<CommUDPRef*>(common);
    const u32 tick = NetTick(); CommUDPFlush(ref, 200, tick);
    NetCritEnter(&gCommUDPCrit);
    CommUDPRef** link = &gCommUDPList;
    while (*link != ref) link = &(*link)->next;
    *link = ref->next;
    NetCritLeave(&gCommUDPCrit);
    if (ref->state == 4) CommUDPCloseConnection(ref, tick);
    if (ref->socket)
    {
        CommUDPRef* item = gCommUDPList;
        while (item && item->socket != ref->socket) item = item->next;
        if (!item) { SocketClose(ref->socket); ref->socket = 0; ref->common.sockptr = 0; }
    }
    if (!gCommUDPList) NetCritKill(&gCommUDPCrit);
    DirtyMemFree(ref->recvBuffer, COMMUDP_MEM_ID, ref->common.memgroup);
    DirtyMemFree(ref->sendBuffer, COMMUDP_MEM_ID, ref->common.memgroup);
    DirtyMemFree(ref, COMMUDP_MEM_ID, ref->common.memgroup);
}
extern "C" CommUDPRef* CommUDPConstruct(s32 maxWidth, s32 maxInput, s32 maxOutput);
// CommRef's generic constructor slot returns its embedded base; the native
// constructor itself returns the concrete object and may fail with null.
static CommRef* CommUDPConstructForCommRef(s32 maxWidth, s32 maxInput, s32 maxOutput)
{
    CommUDPRef* ref = CommUDPConstruct(maxWidth, maxInput, maxOutput);
    return ref ? &ref->common : 0;
}

// ARTIST82B40310: allocate native rings and register the instance globally.
// The vtable methods below use the native socket and packet paths; the
// asynchronous receive/timer pump remains an external dependency.
extern "C" CommUDPRef* CommUDPConstruct(s32 maxWidth, s32 maxInput, s32 maxOutput)
{
    const s32 group = DirtyMemGroupQuery();
    CommUDPRef* ref = static_cast<CommUDPRef*>(DirtyMemAlloc(sizeof(*ref), COMMUDP_MEM_ID, group));
    if (!ref) return 0;
    memset(ref, 0, sizeof(*ref));
    CommRef& common = ref->common;
    common.memgroup = group;
    common.maxwid = static_cast<u16>(maxWidth);
    common.maxinp = static_cast<u8>(maxInput); common.maxout = static_cast<u8>(maxOutput);
    common.Construct = CommUDPConstructForCommRef;
    common.Destroy = CommUDPDestroy;
    common.Resolve = CommUDPResolve; common.Unresolve = CommUDPOriginalUnresolve;
    common.Listen = CommUDPListen; common.Unlisten = CommUDPUnlisten;
    common.Connect = CommUDPConnect; common.Unconnect = CommUDPUnconnect;
    common.Callback = CommUDPCallback; common.Status = CommTCPStatus;
    common.Control = CommUDPControl; common.Tick = CommUDPTickAdapter;
    common.Send = CommUDPSend; common.Peek = CommUDPPeek; common.Recv = CommUDPRecv;
    const s32 stride = (maxWidth + 27) & 0x7ffc;
    ref->recvStride = stride; ref->recvSize = stride * maxInput;
    ref->recvBuffer = static_cast<u8*>(DirtyMemAlloc(ref->recvSize, COMMUDP_MEM_ID, group));
    ref->sendStride = maxWidth + 24; // original intermediate store, immediately overwritten
    ref->sendStride = stride; ref->sendSize = stride * maxOutput;
    ref->sendBuffer = static_cast<u8*>(DirtyMemAlloc(ref->sendSize, COMMUDP_MEM_ID, common.memgroup));
    ref->socket = 0; common.sockptr = 0; memset(&ref->peer, 0, 16);
    ref->state = 1; ref->connectionId = 0;
    if (!gCommUDPList)
    {
        NetCritInit(&gCommUDPCrit, "commudp-global");
        gCommUDPGlobalOther = 0; gCommUDPWorkPending = 0;
    }
    NetCritEnter(&gCommUDPCrit);
    ref->next = gCommUDPList; gCommUDPList = ref;
    NetCritLeave(&gCommUDPCrit);
    return ref;
}

// ARTIST82B3F5B8: socket callback schedules the native protocol pump.
static s32 CommUDPSocketCallback(SocketT*, s32, void*)
{
    gCommUDPGlobalOther = 1;
    if (NetCritTry(&gCommUDPCrit))
    {
        const u32 tick = NetTick();
        while (CommUDPPumpNative(tick) > 0) {}
        gCommUDPWorkPending = 0;
        NetCritLeave(&gCommUDPCrit);
    }
    else ++gCommUDPWorkPending;
    gCommUDPGlobalOther = 0;
    return 0;
}
static void CommUDPSetAddress(sockaddr* output, u32 address, u16 port)
{
    memset(output, 0, 16); output->sa_family = AF_INET;
    volatile u8* p = reinterpret_cast<volatile u8*>(output);
    p[3] = static_cast<u8>(port); p[2] = static_cast<u8>(port >> 8);
    p[7] = static_cast<u8>(address); p[6] = static_cast<u8>(address >> 8);
    p[5] = static_cast<u8>(address >> 16); p[4] = static_cast<u8>(address >> 24);
}
static u16 CommUDPPort(const sockaddr* address)
{
    const volatile u8* p = reinterpret_cast<const volatile u8*>(address);
    return static_cast<u16>((u16(p[2]) << 8) | p[3]);
}
static void CommUDPResetRings(CommUDPRef* ref)
{
    ref->socket = 0; ref->common.sockptr = 0;
    ref->sendHead = ref->sendTail = ref->sendNext = 0;
    ref->sendSequence = 256; ref->sendUnreliable = 128;
    ref->recvHead = ref->recvTail = 0;
    ref->recvSequence = 256; ref->recvUnreliable = 128; ref->recvPending = 0;
    ref->sendTick = NetTick() - 5000; ref->recvTick = NetTick() - 5000;
}
// ARTIST82B3F7E0: bind listener or reuse another CommUDP socket's port.
static s32 CommUDPAttachListener(CommUDPRef* ref, SocketT* socket, const sockaddr* local)
{
    if (ref->state != 1) { SocketClose(socket); return -2; }
    CommUDPResetRings(ref); memset(&ref->peer, 0, 16);
    sockaddr bound; memset(&bound, 0, 16);
    for (CommUDPRef* item = gCommUDPList; item; item = item->next)
    {
        if (item != ref && item->socket && SocketInfo(item->socket, 0x62696e64, 0, &bound, 16) >= 0 &&
            CommUDPPort(local) == CommUDPPort(&bound))
        {
            ref->socket = ref->common.sockptr = item->socket;
            SocketClose(socket); ref->state = 3; return 0;
        }
    }
    const s32 result = SocketBind(socket, local, 16);
    if (result < 0)
    {
        ref->state = 0; SocketClose(socket);
        return result == -11 ? -9 : -5;
    }
    ref->socket = ref->common.sockptr = socket;
    SocketCallback(socket, 2, 100, 0, CommUDPSocketCallback);
    ref->state = 3; return 0;
}
extern "C" s32 CommUDPListen(CommRef* common, const char* text)
{
    CommUDPRef* ref = reinterpret_cast<CommUDPRef*>(common);
    u32 address; s32 localPort, peerPort;
    if ((SockaddrInParse2(&address, &localPort, &peerPort, text) & 2) != 2) return -3;
    sockaddr local; CommUDPSetAddress(&local, 0, static_cast<u16>(localPort));
    SocketT* socket = SocketOpen(AF_INET, SOCK_DGRAM, 0);
    if (!socket) return -4;
    s32 result = CommUDPAttachListener(ref, socket, &local);
    const char* hash = strchr(text, '#');
    if (hash) ref->connectionId = NetHash(hash + 1);
    NetPrintfCode("commudp: listen err=%d, bind=%d, connident=0x%08x\n", result, localPort, ref->connectionId);
    if (!result && address)
    {
        if (!peerPort) peerPort = localPort + 1;
        NetPrintfCode("commudp: poke address=%08x:%d\n", address, peerPort);
        CommUDPSetAddress(&ref->peer, address, static_cast<u16>(peerPort));
    }
    ref->sendError = 0;
    return result;
}
extern "C" s32 CommUDPConnect(CommRef* common, const char* text)
{
    CommUDPRef* ref = reinterpret_cast<CommUDPRef*>(common);
    u32 address; s32 localPort, peerPort;
    if ((SockaddrInParse2(&address, &localPort, &peerPort, text) & 3) != 3) return -3;
    if (!peerPort) { peerPort = localPort; ++localPort; }
    sockaddr local; CommUDPSetAddress(&local, 0, static_cast<u16>(localPort));
    const char* hash = strchr(text, '#');
    if (hash) ref->connectionId = NetHash(hash + 1);
    NetPrintfCode("commudp: connect addr=%08x, bind=%d, peer=%d connident=0x%08x\n", address, localPort, peerPort, ref->connectionId);
    SocketT* socket = 0;
    for (CommUDPRef* item = gCommUDPList; item; item = item->next)
    {
        if (item != ref && item->socket)
        {
            sockaddr bound;
            if (SocketInfo(item->socket, 0x62696e64, 0, &bound, 16) >= 0 && !SockaddrCompare(&local, &bound))
            { socket = item->socket; break; }
        }
    }
    if (!socket)
    {
        socket = SocketOpen(AF_INET, SOCK_DGRAM, 0);
        if (!socket) return -4;
        s32 result = SocketBind(socket, &local, 16);
        if (result < 0)
        {
            NetPrintfCode("commudp: bind to %d failed with %d\n", localPort, result);
            CommUDPSetAddress(&local, 0, 0);
            result = SocketBind(socket, &local, 16);
            if (result < 0)
            {
                NetPrintfCode("commudp: bind to 0 failed with result %d\n", result);
                SocketClose(socket); return -5;
            }
            SocketInfo(socket, 0x62696e64, 0, &local, 16);
            NetPrintfCode("commudp: bound socket to port %d\n", CommUDPPort(&local));
        }
    }
    sockaddr peer; CommUDPSetAddress(&peer, address, static_cast<u16>(peerPort));
    ref->sendError = 0;
    if (ref->state != 1) { SocketClose(socket); return -2; }
    CommUDPResetRings(ref);
    memcpy(&ref->peer, &peer, 16);
    ref->socket = ref->common.sockptr = socket;
    SocketCallback(socket, 2, 100, 0, CommUDPSocketCallback);
    ref->state = 2; return 0;
}
extern "C" s32 CommUDPSend(CommRef* common, const void* data, s32 length, u32 flags)
{
    CommUDPRef* ref = reinterpret_cast<CommUDPRef*>(common);
    const u32 tick = NetTick();
    if (ref->state != 4) return -2;
    const s32 next = (ref->sendStride + ref->sendHead) % ref->sendSize;
    if (next == ref->sendTail) return 0;
    if (length > ref->sendStride - 24)
    { NetPrintfCode("commudp: oversized packet send (%d bytes)\n", length); return -6; }
    if (!length) return ((ref->sendSize - ref->sendTail + ref->sendHead) % ref->sendSize) / ref->sendStride + 1;
    CommUDPPacket* packet = reinterpret_cast<CommUDPPacket*>(ref->sendBuffer + ref->sendHead);
    packet->length = length; memcpy(packet + 1, data, length); packet->tick = tick;
    if (flags & 1)
    {
        NetCritEnter(&gCommUDPCrit);
        packet->sequence = ref->sendUnreliable; packet->ack = ref->recvSequence - 1;
        sockaddr broadcast;
        const sockaddr* target = &ref->peer;
        if (flags & 2)
        {
            memcpy(&broadcast, &ref->peer, 16);
            volatile u8* p = reinterpret_cast<volatile u8*>(&broadcast);
            p[7] = p[6] = p[5] = p[4] = 0xff;
            target = &broadcast;
        }
        CommUDPSendPacket(ref, packet, target, tick);
        if (++ref->sendUnreliable >= 256) ref->sendUnreliable = 128;
        NetCritLeave(&gCommUDPCrit);
        return 1;
    }
    packet->sequence = ref->sendSequence++; packet->ack = ref->recvSequence - 1;
    ref->sendHead = next;
    s32 queued = (next - ref->sendTail + ref->sendSize) % ref->sendSize / ref->sendStride;
    if (queued < (ref->common.maxout >> 1))
    {
        NetCritEnter(&gCommUDPCrit);
        CommUDPTransmitPendingNative(ref, tick);
        if (gCommUDPWorkPending)
        {
            while (CommUDPPumpNative(tick) > 0) {}
            gCommUDPWorkPending = 0;
        }
        NetCritLeave(&gCommUDPCrit);
    }
    return queued > 0 ? queued : 1;
}

// ARTIST82B3DF00: update host/peer identity from a native handshake.
static void CommUDPRememberPeer(CommUDPRef* ref, const sockaddr* from)
{
    const volatile u8* p = reinterpret_cast<const volatile u8*>(from);
    ref->common.peerip = (u32(p[4]) << 24) | (u32(p[5]) << 16) | (u32(p[6]) << 8) | p[7];
    ref->common.peerport = static_cast<u16>((u16(p[2]) << 8) | p[3]);
    sockaddr bound;
    SocketInfo(ref->socket, 0x62696e64, 0, &bound, 16);
    ref->common.hostip = SocketGetLocalAddr();
    ref->common.hostport = CommUDPPort(&bound);
    NetPrintfCode("commudp: peer=%s:%d ", SocketInAddrGetText(ref->common.peerip), ref->common.peerport);
    NetPrintfCode("commudp: host=%s:%d\n", SocketInAddrGetText(ref->common.hostip), ref->common.hostport);
}

// ARTIST82B3E1F0: native INIT/CONN/POKE/DISC control packets.
static void CommUDPHandleControl(CommUDPRef* ref, CommUDPPacket* packet, const sockaddr* from, u32 tick)
{
    if (packet->ack != ref->connectionId)
    {
        NetPrintfCode("commudp: warning - connident mismatch (expected=0x%08x got=0x%08x)\n", ref->connectionId, packet->ack);
        if (packet->sequence == 1) ref->state = 5;
        return;
    }
    ref->recvTick = tick - 1000;
    if (packet->sequence == 1 || packet->sequence == 5)
    {
        CommUDPRememberPeer(ref, from);
        const volatile u8* p = reinterpret_cast<const volatile u8*>(&ref->peer);
        u32 address = (u32(p[4]) << 24) | (u32(p[5]) << 16) | (u32(p[6]) << 8) | p[7];
        NetPrintfCode("commudp: sending CONN packet to %s:%d connident=0x%08x\n", SocketInAddrGetText(address), CommUDPPort(&ref->peer), ref->connectionId);
        CommUDPPacket reply; reply.length = 0; reply.sequence = 2; reply.ack = ref->connectionId;
        CommUDPSendPacket(ref, &reply, &ref->peer, tick);
    }
    else if (packet->sequence == 2)
    {
        if (ref->state == 2) { CommUDPRememberPeer(ref, from); ref->state = 4; }
    }
    else if (packet->sequence == 3)
    {
        if (ref->state == 4) ref->state = 5;
        NetPrintfCode("commudp: received DISC packet\n");
    }
}

// ARTIST82B3E700, export hole: consume ACKs and restart the send cursor on NAK.
static void CommUDPHandleAck(CommUDPRef* ref, CommUDPPacket* packet, u32 tick)
{
    const bool nak = packet->sequence == 4;
    const u32 ack = nak ? packet->ack - 1 : packet->ack;
    while (ref->sendTail != ref->sendHead)
    {
        CommUDPPacket* queued = reinterpret_cast<CommUDPPacket*>(ref->sendBuffer + ref->sendTail);
        if (ack < queued->sequence) break;
        if (ref->sendNext == ref->sendTail) ref->sendNext = (ref->sendNext + ref->sendStride) % ref->sendSize;
        ref->sendTail = (ref->sendTail + ref->sendStride) % ref->sendSize;
    }
    if (nak) { ref->sendNext = ref->sendTail; CommUDPTransmitPendingNative(ref, tick); }
}

// ARTIST82B3E808, export hole: validate sequence and enqueue the actual packet.
static s32 CommUDPHandleData(CommUDPRef* ref, CommUDPPacket* packet, const u8* data, u32 tick)
{
    const s32 next = (ref->recvHead + ref->recvStride) % ref->recvSize;
    if (next == ref->recvTail) { NetPrintfCode("commudp: input buffer overflow\n"); return 2; }
    if (packet->sequence >= 128 && packet->sequence < 256)
    {
        const s32 freeSlots = (ref->recvSize - ((ref->recvSize - ref->recvTail + ref->recvHead) % ref->recvSize)) / ref->recvStride;
        const u32 difference = (packet->sequence - ref->recvUnreliable) & 127;
        if (packet->sequence > ref->recvUnreliable || difference < 32) ref->common.packlost += difference;
        ref->recvUnreliable = packet->sequence + 1;
        if (ref->recvUnreliable >= 256) ref->recvUnreliable -= 128;
        if (freeSlots <= 4) return 2;
    }
    else
    {
        if (packet->sequence < ref->recvSequence) return 0;
        if (packet->sequence > ref->recvSequence)
        {
            ref->common.packlost += packet->sequence - ref->recvSequence;
            packet->sequence = 4; packet->length = 0; packet->ack = ref->recvSequence;
            CommUDPSendPacket(ref, packet, &ref->peer, tick);
            return -1;
        }
        if (!packet->length) return 0;
    }
    CommUDPPacket* dest = reinterpret_cast<CommUDPPacket*>(ref->recvBuffer + ref->recvHead);
    memcpy(dest, packet, 24); memcpy(dest + 1, data, packet->length);
    ref->recvHead = next; ++ref->reserved[1];
    if (packet->sequence >= 256) { ++ref->recvSequence; ref->recvPending += dest->length; }
    ref->callbackFlags |= 1;
    if (ref->common.RecvCallback) ref->common.RecvCallback(&ref->common, dest + 1, dest->length, dest->tick);
    --ref->reserved[1];
    return 1;
}

// ARTIST82B3EB40: global 0x4FC-byte receive scratch, owned under gCommUDPCrit.
struct CommUDPIncoming { CommUDPPacket packet; u8 data[1252]; };
static CommUDPIncoming gCommUDPIncoming;
static const char* const gCommUDPPacketNames[6] = {
    "RAW_PACKET_INVALID", "RAW_PACKET_INIT", "RAW_PACKET_CONN",
    "RAW_PACKET_DISC", "RAW_PACKET_NAK", "RAW_PACKET_POKE"
};
static u32 CommUDPAddress(const sockaddr* address)
{
    const volatile u8* p = reinterpret_cast<const volatile u8*>(address);
    return (u32(p[4]) << 24) | (u32(p[5]) << 16) | (u32(p[6]) << 8) | p[7];
}
static u32 CommUDPTimestamp(const sockaddr* address)
{
    const volatile u8* p = reinterpret_cast<const volatile u8*>(address);
    return (u32(p[8]) << 24) | (u32(p[9]) << 16) | (u32(p[10]) << 8) | p[11];
}

extern "C" s32 CommUDPPumpNative(u32 tick)
{
    CommUDPPacket* packet = &gCommUDPIncoming.packet;
    sockaddr from; memset(&from, 0, 16);
    packet->length = -1;
    SocketT* socket = 0;
    s32 processed = 0;
    CommUDPRef* listener = 0;
    for (CommUDPRef* item = gCommUDPList; item; item = item->next)
    {
        if (item->socket && item->socket != socket)
        {
            socket = item->socket;
            s32 fromLength = 16;
            s32 received = SocketRecvfrom(socket, reinterpret_cast<char*>(&packet->sequence), 1268, 0, &from, &fromLength);
            if (received > 0)
            {
                packet->length = received - 16;
                packet->tick = CommUDPTimestamp(&from);
                packet->sequence = SocketNtohl(packet->sequence);
                packet->ack = SocketNtohl(packet->ack);
                if (packet->sequence == 1 || packet->sequence == 2 || packet->sequence == 5)
                    NetPrintfCode("commudp: got %s connident=0x%08x len=%d\n", gCommUDPPacketNames[packet->sequence], packet->ack, packet->length);
                packet->clientId = SocketNtohl(packet->clientId);
                packet->remoteId = SocketNtohl(packet->remoteId);
                processed = 1;
                break;
            }
        }
    }
    for (CommUDPRef* ref = gCommUDPList; ref; ref = ref->next)
    {
        tick = NetTick();
        if (!listener && socket == ref->socket && ref->state == 3 && packet->length >= 0 &&
            (packet->sequence == 1 || packet->sequence == 2) && ref->connectionId == packet->ack)
            listener = ref;
        const bool identity = packet->clientId == ref->remoteId && packet->remoteId == ref->clientId;
        if (packet->length >= 0 && ref->state != 3 && ref->state != 5 && socket == ref->socket &&
            !SockaddrCompare(&ref->peer, &from) && identity)
        {
            ref->common.datarcvd += packet->length; ++ref->common.packrcvd;
            if (ref->state == 2 && (packet->sequence == 128 || packet->sequence == 256))
            {
                NetPrintfCode("commudp: transitioning to OPEN state due to received data from peer\n");
                ref->state = 4;
            }
            if (packet->sequence == 1 || packet->sequence == 2 || packet->sequence == 3 || packet->sequence == 5)
            {
                CommUDPHandleControl(ref, packet, &from, tick);
                packet->length = -1;
            }
            else if (ref->state == 4)
            {
                ref->recvTick = packet->tick;
                if (packet->sequence == 4) CommUDPHandleAck(ref, packet, tick);
                else if (packet->sequence <= 0x10000000)
                {
                    CommUDPHandleAck(ref, packet, tick);
                    CommUDPHandleData(ref, packet, gCommUDPIncoming.data, tick);
                }
                else
                {
                    u32 currentRecv = ref->recvSequence;
                    s32 count = packet->sequence >> 28;
                    CommUDPPacket part;
                    part.tick = packet->tick; part.ack = packet->ack;
                    part.sequence = (packet->sequence & 0x0fffffffu) - count;
                    do
                    {
                        s32 size;
                        if (count > 0) { --packet->length; size = gCommUDPIncoming.data[packet->length]; }
                        else size = packet->length;
                        part.length = size; packet->length -= size;
                        CommUDPHandleAck(ref, &part, tick);
                        if (CommUDPHandleData(ref, &part, gCommUDPIncoming.data + packet->length, tick) < 0) count = 0;
                        if (count > 0 && currentRecv != ref->recvSequence) currentRecv = ref->recvSequence;
                        ++part.sequence;
                    } while (--count >= 0);
                }
                packet->length = -1;
            }
            else continue;
        }
        if (ref->state == 2 && tick - ref->sendTick > 1000)
        {
            NetPrintfCode("commudp: sending INIT packet to %s:%d connident=0x%08x clientident=0x%08x\n",
                SocketInAddrGetText(CommUDPAddress(&ref->peer)), CommUDPPort(&ref->peer), ref->connectionId, ref->clientId);
            CommUDPPacket init; init.length = 0; init.sequence = 1; init.ack = ref->connectionId;
            CommUDPSendPacket(ref, &init, &ref->peer, tick);
        }
        if (ref->state == 4 && ref->sendNext != ref->sendHead) CommUDPTransmitPendingNative(ref, tick);
        if (ref->state == 4 && tick - ref->recvTick > 120000 && tick - ref->sendTick < 2000)
        {
            NetPrintfCode("commudp: closing connection due to timeout\n");
            NetPrintfCode("commudp: tick=%d, rtick=%d, stick=%d\n", tick, ref->recvTick, ref->sendTick);
            CommUDPCloseConnection(ref, tick);
        }
        if (ref->sendError == -5 && (ref->state == 4 || ref->state == 2 || ref->state == 3))
        {
            NetPrintfCode("commudp: closing connection as host is unreachable\n");
            NetPrintfCode("commudp: tick=%d, rtick=%d, stick=%d\n", tick, ref->recvTick, ref->sendTick);
            CommUDPCloseConnection(ref, tick); ref->sendError = 0;
        }
        if (ref->state == 3 && ref->peer.sa_family == AF_INET && tick > ref->sendTick + 1000)
        {
            NetPrintfCode("commudp: sending POKE packet to %s:%d connident=0x%08x clientident=0x%08x\n",
                SocketInAddrGetText(CommUDPAddress(&ref->peer)), CommUDPPort(&ref->peer), ref->connectionId, ref->clientId);
            CommUDPPacket poke; poke.length = 0; poke.sequence = 5; poke.ack = ref->connectionId;
            CommUDPSendPacket(ref, &poke, &ref->peer, tick);
        }
        if (!ref->callbackFlags && tick > ref->reserved[0] + 250)
        { ref->reserved[0] = tick; ref->callbackFlags = 4; }
        if (!ref->reserved[1] && ref->callbackFlags)
        {
            ++ref->reserved[1];
            if (ref->callback) ref->callback(&ref->common, ref->callbackFlags);
            ref->callbackFlags = 0; --ref->reserved[1];
        }
        if (ref->state == 4 && ref->sendNext == ref->sendHead)
        {
            const u32 elapsed = tick - ref->sendTick;
            if ((elapsed > 150 && (ref->recvUnused != ref->recvSequence || ref->sendHead != ref->sendTail)) ||
                elapsed > 2500 || ref->recvPending >= 2048)
            {
                ref->recvPending = 0;
                if (ref->sendTail == ref->sendHead)
                {
                    CommUDPPacket ack; ack.length = 0; ack.sequence = ref->sendSequence;
                    ref->recvUnused = ref->recvSequence; ack.ack = ref->recvSequence - 1;
                    CommUDPSendPacket(ref, &ack, &ref->peer, tick);
                }
                else
                {
                    ref->sendNext = (ref->sendNext - ref->sendStride + ref->sendSize) % ref->sendSize;
                    CommUDPTransmitPendingNative(ref, tick);
                }
            }
        }
    }
    if (packet->length >= 0 && from.sa_family == AF_INET &&
        (packet->sequence == 5 || packet->sequence == 1 || packet->sequence == 2))
    {
        NetPrintfCode("commudp: received %s packet from %s:%d\n", gCommUDPPacketNames[packet->sequence],
            SocketInAddrGetText(CommUDPAddress(&from)), CommUDPPort(&from));
        bool mismatched = false;
        CommUDPRef* match = gCommUDPList;
        for (; match; match = match->next)
        {
            if ((match->state == 2 || match->state == 3) && match->peer.sa_family == AF_INET)
            {
                if (match->connectionId == packet->ack) break;
                mismatched = true;
            }
        }
        if (!match && mismatched)
            NetPrintfCode("commudp: ignoring %s packet with connident 0x%08x\n", gCommUDPPacketNames[packet->sequence], packet->ack);
        if (match && socket == match->socket && CommUDPAddress(&match->peer) == CommUDPAddress(&from) &&
            CommUDPPort(&match->peer) != CommUDPPort(&from))
        {
            NetPrintfCode("commudp: changing peer to %s:%d ", SocketInAddrGetText(CommUDPAddress(&from)), CommUDPPort(&from));
            NetPrintfCode("(was expecting %s:%d)\n", SocketInAddrGetText(CommUDPAddress(&match->peer)), CommUDPPort(&match->peer));
            memcpy(&match->peer, &from, 16);
        }
    }
    if (listener && packet->length >= 0 && (packet->sequence == 1 || packet->sequence == 2))
    {
        u32 expected = CommUDPAddress(&listener->peer);
        if (listener->connectionId == packet->ack && (!expected || expected == CommUDPAddress(&from)))
        {
            NetPrintfCode("commudp: transitioning to CONN state after receiving %s packet\n", gCommUDPPacketNames[packet->sequence]);
            listener->state = 2;
            if (!expected) memcpy(&listener->peer, &from, 16);
            CommUDPHandleControl(listener, packet, &from, tick);
        }
    }
    return processed;
}
