// DirtySDK native X360 CommSRP; ARTIST 82B3DD68 and direct lifecycle.
#include "commall_x360_internal.h"
#include "dirtylib_x360_internal.h"
#include "dirtymem.h"
#include "dirtylib.h"
#include <stddef.h>
#include <string.h>

struct CommSRPRef
{
    CommRef common;
    s32 reserved124;
    SocketT* socket;
    sockaddr peer;
    s32 state, connectionId;
    s32 recvStride, recvLimit, recvSize, recvHead, recvTail;
    u8* recvBuffer;
    s32 sendStride, sendSize, sendHead, sendTail;
    u8* sendBuffer;
    u32 lastSend, lastRecv, sendUnreliable, recvUnreliable, sendReliable;
    s32 recvReliable;
    u8 protocol[272];
    NetCritT crit;
    s32 callbackDepth;
    u32 callbackFlags;
    void (*callback)(CommRef*, s32);
};
struct CommSRPPacket { u32 tick; s32 length; u8 code; };
CGS_STATIC_ASSERT((sizeof(CommSRPRef) == 548), "ARTIST CommSRP size");
CGS_STATIC_ASSERT((offsetof(CommSRPRef, common) == 0), "ARTIST CommRef base");
CGS_STATIC_ASSERT((offsetof(CommSRPRef, state) == 148), "ARTIST state");
CGS_STATIC_ASSERT((offsetof(CommSRPRef, recvBuffer) == 176), "ARTIST input buffer");
CGS_STATIC_ASSERT((offsetof(CommSRPRef, sendBuffer) == 196), "ARTIST output buffer");
CGS_STATIC_ASSERT((offsetof(CommSRPRef, crit) == 496), "ARTIST critical section");
CGS_STATIC_ASSERT((offsetof(CommSRPRef, callback) == 544), "ARTIST callback");
static const s32 COMM_SRP_MEM_ID = 0x63737270;
extern "C" {
CommSRPRef* CommSRPConstruct(s32, s32, s32);
s32 SocketClose(SocketT*);
s32 SockaddrInParse2(u32*, s32*, s32*, const char*);
SocketT* SocketOpen(s32, s32, s32);
s32 SocketBind(SocketT*, const sockaddr*, s32);
s32 SocketCallback(SocketT*, s32, s32, void*, s32 (*)(SocketT*, s32, void*));
s32 SocketSendto(SocketT*, const char*, s32, s32, const sockaddr*, s32);
s32 SocketRecvfrom(SocketT*, char*, s32, s32, sockaddr*, s32*);
s32 SocketInfo(SocketT*, s32, s32, void*, s32);
u32 SocketGetLocalAddr(void);
}

// ARTIST 82B3CDE0, immediate packet transmission.
static s32 CommSRPSendImmediate(CommSRPRef* ref, CommSRPPacket* packet)
{
    const s32 size = packet->length + 1;
    s32 sent = SocketSendto(ref->socket, reinterpret_cast<const char*>(&packet->code), size, 0, &ref->peer, 16);
    if (sent != size) { NetPrintfCode("_CommSRPSendImmediate: SocketSendto returned %d\n", sent); return -1; }
    ref->lastSend = NetTick(); ref->common.datasent += size; ++ref->common.packsent;
    return size;
}

static void CommSRPRememberPeer(CommSRPRef* ref, const sockaddr* address)
{
    const u8* bytes = reinterpret_cast<const u8*>(address);
    ref->common.peerip = (u32(bytes[4]) << 24) | (u32(bytes[5]) << 16) | (u32(bytes[6]) << 8) | bytes[7];
    ref->common.peerport = (u16(bytes[2]) << 8) | bytes[3];
    sockaddr local;
    SocketInfo(ref->socket, 0x62696e64, 0, &local, 16);
    ref->common.hostip = SocketGetLocalAddr();
    bytes = reinterpret_cast<const u8*>(&local);
    ref->common.hostport = (u16(bytes[2]) << 8) | bytes[3];
    NetPrintfCode("commsrp: peer=0x%08x:%d, host=0x%08x:%d\n", ref->common.peerip,
        ref->common.peerport, ref->common.hostip, ref->common.hostport);
}
// ARTIST 82B3CE78, setup retransmission.
static void CommSRPProcessSetup(CommSRPRef* ref)
{
    const u8* bytes = reinterpret_cast<const u8*>(&ref->peer);
    if (!(bytes[4] | bytes[5] | bytes[6] | bytes[7]) || !(bytes[2] | bytes[3])) return;
    if (NetTick() - ref->lastSend < 1000) return;
    CommSRPPacket packet; packet.length = 0;
    if (ref->state == 1) packet.code = 16;
    else if (ref->state == 2) packet.code = 17;
    else return;
    CommSRPSendImmediate(ref, &packet);
}
// ARTIST 82B3CF38, connection control packets.
static void CommSRPProcessControl(CommSRPRef* ref, CommSRPPacket* packet, const sockaddr* from)
{
    ref->lastRecv = NetTick() - 1000;
    switch (packet->code)
    {
    case 16:
        if (ref->state == 2) { memcpy(&ref->peer, from, 16); CommSRPRememberPeer(ref, from); ref->state = 3; }
        { CommSRPPacket reply; reply.length = 0; reply.code = 18; CommSRPSendImmediate(ref, &reply); }
        break;
    case 17: if (ref->state == 1) memcpy(&ref->peer, from, 16); break;
    case 18: if (ref->state == 1) { CommSRPRememberPeer(ref, from); ref->state = 3; } break;
    case 19: break;
    case 20: if (ref->state == 3) ref->state = 4; break;
    default: NetPrintfCode("_CommSRPProcessSetup: Unrecognized control packet type %d\n", packet->code); break;
    }
}
// ARTIST 82B3D048, input-ring admission and sequence checking.
static s32 CommSRPProcessData(CommSRPRef* ref, CommSRPPacket* packet)
{
    const u32 code = packet->code;
    if (code >= 64 && code < 128)
    {
        const s32 remaining = (ref->recvSize - ((ref->recvSize - ref->recvTail + ref->recvHead) % ref->recvSize)) / ref->recvStride;
        if (remaining <= ref->recvLimit) return -1;
        ref->recvUnreliable = code - 64;
    }
    else
    {
        if ((ref->recvHead + ref->recvStride) % ref->recvSize == ref->recvTail)
        { NetPrintfCode("_CommSRPProcessData: Reliable packet discarded due to input buffer overrun\n"); return -1; }
        const s32 sequence = code - 128;
        if (sequence != ((ref->recvReliable + 1) & 63)) return ref->recvReliable == sequence;
        ref->recvReliable = sequence;
    }
    CommSRPPacket* dest = reinterpret_cast<CommSRPPacket*>(ref->recvBuffer + ref->recvHead);
    memcpy(dest, packet, ref->recvStride);
    ++ref->callbackDepth;
    ref->recvHead = (ref->recvHead + ref->recvStride) % ref->recvSize;
    ref->callbackFlags |= 1;
    if (ref->common.RecvCallback) ref->common.RecvCallback(&ref->common, &dest->code + 1, dest->length, dest->tick);
    --ref->callbackDepth;
    return 1;
}

// ARTIST 82B3D208: drain input, acknowledge reliable data and release sent packets.
static void CommSRPProcessRecvQueue(CommSRPRef* ref)
{
    s32 processed = 0;
    while (ref->state >= 1 && ref->state <= 3 && processed >= 0)
    {
        CommSRPPacket* packet = reinterpret_cast<CommSRPPacket*>(ref->recvBuffer + ref->recvHead);
        sockaddr from; s32 fromLength = 16;
        s32 length = SocketRecvfrom(ref->socket, reinterpret_cast<char*>(&packet->code), ref->recvStride, 0, &from, &fromLength);
        if (length <= 0)
        {
            if (length < 0) { NetPrintfCode("_CommSRPProcessRecvQueue: Error %d - closing connection\n", length); ref->state = 4; }
            break;
        }
        packet->length = length;
        const u8* bytes = reinterpret_cast<const u8*>(&from);
        packet->tick = (u32(bytes[8]) << 24) | (u32(bytes[9]) << 16) | (u32(bytes[10]) << 8) | bytes[11];
        const u8 code = packet->code;
        if (code >= 16 && code <= 63) CommSRPProcessControl(ref, packet, &from);
        else if (code < 192)
        {
            processed = CommSRPProcessData(ref, packet);
            if (processed == 1)
            {
                const u8 acknowledgedCode = packet->code;
                if (acknowledgedCode >= 128 && acknowledgedCode < 192)
                {
                    CommSRPPacket ack; ack.length = 0; ack.code = acknowledgedCode + 64;
                    CommSRPSendImmediate(ref, &ack);
                }
            }
        }
        else
        {
            CommSRPPacket* sent = reinterpret_cast<CommSRPPacket*>(ref->sendBuffer + ref->sendTail);
            if (code - 192 == sent->code - 128)
                ref->sendTail = (ref->sendTail + ref->sendStride) % ref->sendSize;
        }
    }
}

// ARTIST 82B3D3A0: setup, input processing, keepalive, retransmission, callback.
static void CommSRPPump(CommSRPRef* ref)
{
    if (ref->state == 1 || ref->state == 2) CommSRPProcessSetup(ref);
    CommSRPProcessRecvQueue(ref);
    if (ref->state == 3 && ref->sendHead == ref->sendTail && NetTick() > ref->lastSend + 5000)
    {
        CommSRPPacket keepalive; keepalive.length = 0; keepalive.code = 19;
        CommSRPSendImmediate(ref, &keepalive);
    }
    if (ref->state == 3 && ref->sendHead != ref->sendTail)
    {
        CommSRPPacket* packet = reinterpret_cast<CommSRPPacket*>(ref->sendBuffer + ref->sendTail);
        if (!packet->tick || NetTick() - packet->tick >= 250)
            if (CommSRPSendImmediate(ref, packet) >= 0) packet->tick = NetTick();
    }
    if (!ref->callbackDepth && ref->callbackFlags)
    {
        ++ref->callbackDepth;
        if (ref->callback) ref->callback(&ref->common, ref->callbackFlags);
        ref->callbackFlags = 0; --ref->callbackDepth;
    }
}

// ARTIST 82B3D4D8, serialized socket callback.
static s32 CommSRPSocketCallback(SocketT*, s32, void* data)
{
    CommSRPRef* ref = static_cast<CommSRPRef*>(data);
    if (NetCritTry(&ref->crit)) { CommSRPPump(ref); NetCritLeave(&ref->crit); }
    return 0;
}
// ARTIST 82B3D530, enqueue a reliable packet and run the socket callback.
static s32 CommSRPQueue(CommSRPRef* ref, CommSRPPacket* packet)
{
    packet->tick = 0;
    ref->sendHead = (ref->sendHead + ref->sendStride) % ref->sendSize;
    CommSRPSocketCallback(ref->socket, 0, ref);
    return ((ref->sendSize - ref->sendTail + ref->sendHead) % ref->sendSize) / ref->sendStride;
}
// ARTIST 82B3D7F8: reliable packets are queued; unreliable packets send immediately.
static s32 CommSRPSend(CommRef* common, const void* data, s32 size, u32 flags)
{
    CommSRPRef* ref = reinterpret_cast<CommSRPRef*>(common);
    if (ref->state != 3) return -2;
    if ((ref->sendHead + ref->sendStride) % ref->sendSize == ref->sendTail)
    { NetPrintfCode("commsrp: input queue full\n"); return 0; }
    if (size > ref->sendStride - 11)
    { NetPrintfCode("commsrp: oversized packet send (%d bytes)\n", size); return -6; }
    if (!size) return ((ref->sendSize - ref->sendTail + ref->sendHead) % ref->sendSize) / ref->sendStride + 1;
    CommSRPPacket* packet = reinterpret_cast<CommSRPPacket*>(ref->sendBuffer + ref->sendHead);
    packet->length = size; memcpy(&packet->code + 1, data, size);
    if (flags & 1)
    {
        packet->code = static_cast<u8>(ref->sendUnreliable + 64);
        ref->sendUnreliable = (ref->sendUnreliable + 1) & 63;
        CommSRPSendImmediate(ref, packet);
    }
    else
    {
        packet->code = static_cast<u8>(ref->sendReliable + 128);
        ref->sendReliable = (ref->sendReliable + 1) & 63;
        const s32 result = CommSRPQueue(ref, packet);
        if (result > 0) return result;
    }
    return 1;
}

// ARTIST 82B3D5F8, including the on-wire close packet.
static void CommSRPDestroy(CommRef* common)
{
    CommSRPRef* ref = reinterpret_cast<CommSRPRef*>(common);
    if (ref->state == 3)
    {
        CommSRPPacket packet; packet.length = 0; packet.code = 20;
        CommSRPSendImmediate(ref, &packet); ref->connectionId = 0; ref->state = 4;
    }
    if (ref->socket) { SocketClose(ref->socket); ref->socket = 0; common->sockptr = 0; }
    NetCritKill(&ref->crit);
    DirtyMemFree(ref->recvBuffer, COMM_SRP_MEM_ID, common->memgroup);
    DirtyMemFree(ref->sendBuffer, COMM_SRP_MEM_ID, common->memgroup);
    DirtyMemFree(ref, COMM_SRP_MEM_ID, common->memgroup);
}
static void CommSRPCallback(CommRef* common, void (*callback)(CommRef*, s32))
{
    CommSRPRef* ref = reinterpret_cast<CommSRPRef*>(common);
    ref->callback = callback; ref->callbackFlags |= 2;
}
// ARTIST 82B3D7A8, unnamed export hole reached from the constructor slot.
static s32 CommSRPStatus(CommRef* common)
{
    const s32 state = reinterpret_cast<CommSRPRef*>(common)->state;
    if (state == 1 || state == 2) return 2;
    if (state == 0 || state == 4) return 1;
    return state == 3 ? 3 : 4;
}
static s32 CommSRPResolve(CommRef*, const char*, char*, s32, char)
{ NetPrintfCode("commsrp: resolve functionality not supported\n"); return -1; }
// ARTIST's shared return instruction at the Unresolve slot.
static void CommSRPUnresolve(CommRef*) {}
static s32 CommSRPUnlisten(CommRef* common)
{
    CommSRPRef* ref = reinterpret_cast<CommSRPRef*>(common);
    if (ref->socket) { SocketClose(ref->socket); ref->socket = 0; common->sockptr = 0; }
    ref->state = 0; return 0;
}
static s32 CommSRPUnconnect(CommRef* common)
{
    CommSRPRef* ref = reinterpret_cast<CommSRPRef*>(common);
    if (ref->socket) { SocketClose(ref->socket); ref->socket = 0; common->sockptr = 0; }
    ref->state = 0; return 0;
}
static void CommSRPSetAddress(sockaddr* address, u32 ip, s32 port)
{
    memset(address, 0, sizeof(*address)); address->sa_family = AF_INET;
    u8* bytes = reinterpret_cast<u8*>(address);
    bytes[2] = static_cast<u8>(port >> 8); bytes[3] = static_cast<u8>(port);
    bytes[4] = static_cast<u8>(ip >> 24); bytes[5] = static_cast<u8>(ip >> 16);
    bytes[6] = static_cast<u8>(ip >> 8); bytes[7] = static_cast<u8>(ip);
}
// ARTIST 82B3DA40, including its bind-failure cleanup.
static s32 CommSRPListen(CommRef* common, const char* address)
{
    CommSRPRef* ref = reinterpret_cast<CommSRPRef*>(common);
    if (ref->state || ref->socket) return -2;
    sockaddr local; CommSRPSetAddress(&local, 0, 0);
    u32 ip; s32 localPort, peerPort;
    if ((SockaddrInParse2(&ip, &localPort, &peerPort, address) & 2) != 2) return -3;
    CommSRPSetAddress(&local, 0, localPort);
    ref->sendHead = ref->sendTail = ref->recvHead = ref->recvTail = 0;
    ref->lastSend = NetTick() - 5000; ref->recvReliable = -1;
    ref->socket = SocketOpen(2, 2, 0); common->sockptr = ref->socket;
    if (!ref->socket) return -4;
    const s32 result = SocketBind(ref->socket, &local, 16);
    if (result < 0)
    {
        NetPrintfCode("commsrp: error %d binding socket\n", result);
        SocketClose(ref->socket); ref->socket = 0; common->sockptr = 0; return -5;
    }
    SocketCallback(ref->socket, 2, 100, ref, CommSRPSocketCallback);
    if (ip) CommSRPSetAddress(&ref->peer, ip, peerPort ? peerPort : localPort + 1);
    ref->state = 2; return 0;
}
// ARTIST 82B3DBD8 leaves the open socket owned by the object on bind failure.
static s32 CommSRPConnect(CommRef* common, const char* address)
{
    CommSRPRef* ref = reinterpret_cast<CommSRPRef*>(common);
    if (ref->state || ref->socket) return -2;
    u32 ip; s32 localPort, peerPort;
    if ((SockaddrInParse2(&ip, &localPort, &peerPort, address) & 3) != 3) return -3;
    if (!peerPort) { peerPort = localPort; ++localPort; }
    ref->sendHead = ref->sendTail = ref->recvHead = ref->recvTail = 0;
    ref->lastSend = NetTick() - 5000; ref->recvReliable = -1;
    ref->socket = SocketOpen(2, 2, 0); common->sockptr = ref->socket;
    if (!ref->socket) return -4;
    sockaddr local; CommSRPSetAddress(&local, 0, localPort);
    const s32 result = SocketBind(ref->socket, &local, 16);
    if (result < 0) { NetPrintfCode("commsrp: error %d binding socket\n", result); return -5; }
    CommSRPSetAddress(&ref->peer, ip, peerPort);
    SocketCallback(ref->socket, 2, 100, ref, CommSRPSocketCallback);
    ref->state = 1; return 0;
}
static s32 CommSRPPeek(CommRef* common, void* out, s32 capacity, u32* tick)
{
    CommSRPRef* ref = reinterpret_cast<CommSRPRef*>(common);
    if (ref->recvTail == ref->recvHead) return -7;
    CommSRPPacket* packet = reinterpret_cast<CommSRPPacket*>(ref->recvBuffer + ref->recvTail);
    const s32 size = packet->length - 1;
    memcpy(out, &packet->code + 1, size < capacity ? size : capacity);
    if (tick) *tick = packet->tick;
    return size;
}
static s32 CommSRPRecv(CommRef* common, void* out, s32 capacity, u32* tick)
{
    s32 result = CommSRPPeek(common, out, capacity, tick);
    if (result >= 0)
    {
        CommSRPRef* ref = reinterpret_cast<CommSRPRef*>(common);
        ref->recvTail = (ref->recvTail + ref->recvStride) % ref->recvSize;
    }
    return result;
}
// 82B2A960 is the shared tick entry's branch to NetTick; argument is ignored.
static u32 CommSRPTick(CommRef*) { return NetTick(); }
static CommRef* CommSRPConstructForCommRef(s32 width, s32 input, s32 output)
{
    CommSRPRef* ref = CommSRPConstruct(width, input, output);
    return ref ? &ref->common : 0;
}
extern "C" CommSRPRef* CommSRPConstruct(s32 width, s32 input, s32 output)
{
    const s32 group = DirtyMemGroupQuery();
    CommSRPRef* ref = static_cast<CommSRPRef*>(DirtyMemAlloc(sizeof(CommSRPRef), COMM_SRP_MEM_ID, group));
    if (!ref) return 0;
    memset(ref, 0, sizeof(*ref));
    CommRef& common = ref->common;
    common.memgroup = group; common.maxwid = width; common.maxinp = input; common.maxout = output;
    common.Construct = CommSRPConstructForCommRef; common.Destroy = CommSRPDestroy;
    common.Resolve = CommSRPResolve; common.Unresolve = CommSRPUnresolve;
    common.Listen = CommSRPListen; common.Unlisten = CommSRPUnlisten;
    common.Connect = CommSRPConnect; common.Unconnect = CommSRPUnconnect;
    common.Callback = CommSRPCallback; common.Status = CommSRPStatus; common.Tick = CommSRPTick;
    common.Send = CommSRPSend; common.Peek = CommSRPPeek; common.Recv = CommSRPRecv;
    NetCritInit(&ref->crit, "commsrp");
    const s32 stride = (width + 14) & 0x7ffc;
    ref->recvStride = stride; ref->recvSize = stride * input;
    ref->recvBuffer = static_cast<u8*>(DirtyMemAlloc(ref->recvSize, COMM_SRP_MEM_ID, group));
    ref->sendStride = stride; ref->sendSize = stride * output;
    ref->sendBuffer = static_cast<u8*>(DirtyMemAlloc(ref->sendSize, COMM_SRP_MEM_ID, group));
    ref->recvLimit = input / 8;
    ref->socket = 0; common.sockptr = 0; ref->state = 0; ref->connectionId = 0;
    return ref;
}
