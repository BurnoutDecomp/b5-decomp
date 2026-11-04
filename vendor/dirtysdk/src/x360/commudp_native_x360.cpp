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
u32 CommUDPTick(CommRef*);
s32 CommUDPListen(CommRef*, const char*);
s32 CommUDPConnect(CommRef*, const char*);
s32 CommUDPSend(CommRef*, const void*, s32, u32);
s32 SocketClose(SocketT*);
s32 SocketSendto(SocketT*, const char*, s32, s32, const sockaddr*, s32);
u32 SocketHtonl(u32); u32 SocketNtohl(u32);
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
// Listen, Connect and Send remain real external vtable dependencies.
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
    common.Control = CommUDPControl; common.Tick = CommUDPTick;
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
