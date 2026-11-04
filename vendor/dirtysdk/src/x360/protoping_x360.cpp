// DirtySDK native X360 ProtoPing: ARTIST82B381F8..82B38C5C.
#include "types.hpp"
#include "dirtymem.h"
#include "dirtylib.h"
#include "dirtyaddr.h"
#include "netconn.h"
#include <xtl.h>
#include <xonline.h>
#include <stddef.h>
#include <string.h>

struct ProtoPingRequestT
{
    ProtoPingRequestT* next; s32 ping; s32 sequence; s32 retries;
    XNQOS* qos; s32 state; s32 type; u32 address;
    XSESSION_INFO session; XSESSION_SEARCHRESULT_HEADER* search;
    XOVERLAPPED overlapped; s32 dataLength; u8 data[256];
};
struct ProtoPingRefT
{
    ProtoPingRequestT* requests; s32 sequence; s32 userIndex;
    XNKID listenKey; s32 listening, probes, memGroup;
};
struct ProtoPingResponseT
{
    DirtyAddrT Addr; u32 uAddr; s16 iPing; u16 uSeqn, uNumRetries; u8 uType, bServer;
};
CGS_STATIC_ASSERT((sizeof(ProtoPingRefT) == 32), "ARTIST ProtoPing module");
CGS_STATIC_ASSERT((sizeof(ProtoPingRequestT) == 384), "ARTIST ProtoPing request");
CGS_STATIC_ASSERT((offsetof(ProtoPingRequestT, session) == 32), "ARTIST session");
CGS_STATIC_ASSERT((offsetof(ProtoPingRequestT, search) == 92), "ARTIST session buffer");
CGS_STATIC_ASSERT((offsetof(ProtoPingRequestT, overlapped) == 96), "ARTIST session async");
CGS_STATIC_ASSERT((offsetof(ProtoPingRequestT, data) == 128), "ARTIST request data");
CGS_STATIC_ASSERT((sizeof(ProtoPingResponseT) == 76), "ARTIST response");
extern "C" { u32 NetTick(void); u32 SocketNtohl(u32); }
static const s32 PING_MEM_ID = 0x70706e67;

// ARTIST82B381F8: append request with original sequence and bounded payload.
static ProtoPingRequestT* PingAllocRequest(ProtoPingRefT* ref, s32 type, const u8* data, s32 length)
{
    ProtoPingRequestT* item = static_cast<ProtoPingRequestT*>(DirtyMemAlloc(sizeof(*item), PING_MEM_ID, ref->memGroup));
    if (!item) { NetPrintfCode("protopingxenon: error allocating request\n"); return 0; }
    memset(item, 0, sizeof(*item));
    item->type = type; item->sequence = ++ref->sequence;
    if (length < 0) length = static_cast<s32>(strlen(reinterpret_cast<const char*>(data))) + 1;
    if (length > 256) { NetPrintfCode("protopingxenon: clamping data (length=%d) to %d bytes\n", length, 256); length = 256; }
    if (length) memcpy(item->data, data, length);
    item->dataLength = length;
    ProtoPingRequestT** tail = &ref->requests;
    while (*tail) tail = &(*tail)->next;
    *tail = item;
    return item;
}

// ARTIST82B38308, export hole: release QoS and pending session search.
static void PingFreeRequest(ProtoPingRefT* ref, ProtoPingRequestT* item)
{
    ProtoPingRequestT** link = &ref->requests;
    while (*link && *link != item) link = &(*link)->next;
    if (!*link) return;
    if (item->qos) XNetQosRelease(item->qos);
    if (item->state == 1)
    {
        XCancelOverlapped(&item->overlapped);
        if (item->search) DirtyMemFree(item->search, PING_MEM_ID, ref->memGroup);
    }
    *link = item->next;
    DirtyMemFree(item, PING_MEM_ID, ref->memGroup);
}

// ARTIST82B383C0: session-id lookup first, then peer QoS query.
static void PingStartPeer(ProtoPingRefT* ref, ProtoPingRequestT* item)
{
    DWORD bytes = 0;
    if (!item->state)
    {
        DWORD result = XSessionSearchByID(item->session.sessionID, ref->userIndex, &bytes, 0, 0);
        if (result != ERROR_INSUFFICIENT_BUFFER)
        {
            NetPrintfCode("protopingxenon: error identifying session\n"); item->state = 4; return;
        }
        item->search = static_cast<XSESSION_SEARCHRESULT_HEADER*>(DirtyMemAlloc(bytes, PING_MEM_ID, ref->memGroup));
        if (!item->search) { NetPrintfCode("protopingxenon: error allocating session result buffer\n"); return; }
        result = XSessionSearchByID(item->session.sessionID, ref->userIndex, &bytes, item->search, &item->overlapped);
        if (result != ERROR_IO_PENDING)
        {
            NetPrintfCode("protopingxenon: error identifying session\n"); item->state = 4; return;
        }
        item->state = 1;
    }
    if (item->state != 1) return;
    DWORD result = XGetOverlappedResult(&item->overlapped, 0, FALSE);
    if (!result)
    {
        if (item->search->dwSearchResults != 1) item->state = 4;
        else
        {
            memcpy(&item->session, &item->search->pResults->info, sizeof(item->session));
            const XNADDR* addresses[] = { &item->session.hostAddress };
            const XNKID* ids[] = { &item->session.sessionID };
            const XNKEY* keys[] = { &item->session.keyExchangeKey };
            result = XNetQosLookup(1, addresses, ids, keys, 0, 0, 0, ref->probes, 0, 0, 0, &item->qos);
            item->state = result ? 4 : 2;
        }
    }
    else if (result != ERROR_IO_INCOMPLETE)
    {
        NetPrintfCode("protopingxenon: error %d getting user session, aborting\n", result);
        item->ping = 0; item->state = 4;
    }
    if (item->state != 1) DirtyMemFree(item->search, PING_MEM_ID, ref->memGroup);
}

// ARTIST82B38578: resolve local, service-id, or mapped server address.
static void PingStartServer(ProtoPingRefT* ref, ProtoPingRequestT* item)
{
    if (item->state) return;
    IN_ADDR address; DWORD service;
    if (!item->address)
    {
        address.s_addr = NetConnStatus(0x686f7374, 0, 0, 0); // host
        service = NetConnStatus(0x73657276, 0, 0, 0); // serv
    }
    else if ((item->address & 0xffff0000u) == 0x45410000u)
    {
        XONLINE_SERVICE_INFO info;
        if (NetConnStatus(0x73657276, item->address, &info, 12) < 0)
        {
            NetPrintfCode("protopingxenon: failed to lookup service 0x%08x\n", item->address);
            item->state = 4; return;
        }
        address.s_addr = SocketNtohl(info.serviceIP.s_addr); service = info.dwServiceID;
    }
    else
    {
        sockaddr input, mapped;
        memset(&input, 0, sizeof(input)); input.sa_family = AF_INET;
        volatile u8* in = reinterpret_cast<volatile u8*>(&input);
        in[7] = item->address; in[6] = item->address >> 8;
        in[5] = item->address >> 16; in[4] = item->address >> 24;
        if (NetConnControl(0x6d616472, 0, 0, &input, &mapped))
        {
            NetPrintfCode("protopingxenon: failed to resolve secure address 0x%08x\n", item->address);
            item->state = 4; return;
        }
        const volatile u8* out = reinterpret_cast<const volatile u8*>(&mapped);
        address.s_addr = (u32(out[4]) << 24) | (u32(out[5]) << 16) | (u32(out[6]) << 8) | out[7];
        service = NetConnStatus(0x73616472, address.s_addr, 0, 0); // sadr
        if (!service) service = NetConnStatus(0x786c7370, 0, 0, 0); // xlsp
    }
    s32 result = XNetQosLookup(0, 0, 0, 0, 1, &address, &service, ref->probes, 0, 0, 0, &item->qos);
    if (result) NetPrintfCode("protopingxenon: [%d] QoS returned error 0x%08x\n", item->sequence, result);
    item->state = result ? 4 : 2;
}
// ARTIST82B38770: progress outstanding native QoS work until one completes.
static ProtoPingRequestT* PingPoll(ProtoPingRefT* ref)
{
    for (ProtoPingRequestT* item = ref->requests; item; item = item->next)
    {
        if (NetConnStatus(0x6f6e6c6e, 0, 0, 0) != 1)
        {
            NetPrintfCode("protopingxenon: warning, attempting to ping while not online\n");
            item->state = 4;
        }
        if (item->state < 2)
        {
            if (item->type == 0) PingStartPeer(ref, item);
            if (item->type == 1) PingStartServer(ref, item);
        }
        if (item->state == 2 && (item->qos->axnqosinfo[0].bFlags & XNET_XNQOSINFO_COMPLETE))
        {
            const XNQOSINFO& info = item->qos->axnqosinfo[0];
            item->ping = info.wRttMedInMsecs;
            if (!item->ping) item->ping = 1;
            else if (item->ping == 0xffff) item->ping = 0;
            item->state = 3;
            NetPrintfCode("protopingxenon: [%d] ping=%d tick=%d\n", item->sequence, item->ping, NetTick());
            item->retries = info.cProbesXmit - info.cProbesRecv;
        }
        if (item->state == 3 || item->state == 4)
        {
            if (item->state == 4) NetPrintfCode("protopingxenon: request %d failed\n", item->sequence);
            return item;
        }
    }
    return 0;
}

extern "C" ProtoPingRefT* ProtoPingCreate(s32 maxPings)
{
    (void)maxPings;
    const s32 group = DirtyMemGroupQuery();
    ProtoPingRefT* ref = static_cast<ProtoPingRefT*>(DirtyMemAlloc(sizeof(*ref), PING_MEM_ID, group));
    if (!ref) return 0;
    memset(ref, 0, sizeof(*ref)); ref->memGroup = group; ref->probes = 1;
    return ref;
}
extern "C" void ProtoPingDestroy(ProtoPingRefT* ref)
{
    if (!ref) NetPrintfCode("protopingxenon: trying to destroy a NULL ref\n");
    while (ref->requests) PingFreeRequest(ref, ref->requests);
    if (ref->listening) XNetQosListen(&ref->listenKey, 0, 0, 0, 16);
    DirtyMemFree(ref, PING_MEM_ID, ref->memGroup);
}
extern "C" s32 ProtoPingControl(ProtoPingRefT* ref, s32 control, s32 value, void* data)
{
    if (control == 0x6e707262) { ref->probes = value; return 0; } // nprb
    if (control == 0x73657373) // sess
    {
        if (ref->listening)
        {
            XNetQosListen(&ref->listenKey, 0, 0, 0, 16);
            memset(&ref->listenKey, 0, sizeof(ref->listenKey)); ref->listening = 0;
        }
        if (data)
        {
            XNetQosListen(static_cast<XNKID*>(data), 0, 0, 0, 1);
            memcpy(&ref->listenKey, data, 8); ref->listening = 1;
        }
        return 0;
    }
    if (control == 0x75696478) { ref->userIndex = value; return 0; } // uidx
    return -1;
}
extern "C" s32 ProtoPingRequest(ProtoPingRefT* ref, DirtyAddrT* address, const u8* data,
                                 s32 length, s32 timeout, s32 ttl)
{
    (void)timeout; (void)ttl;
    ProtoPingRequestT* item = PingAllocRequest(ref, 0, data, length);
    if (!item) return 0;
    memcpy(&item->session.sessionID, address, 8);
    return item->sequence;
}
extern "C" s32 ProtoPingRequestServer(ProtoPingRefT* ref, u32 address, const u8* data,
                                       s32 length, s32 timeout, s32 ttl)
{
    (void)timeout; (void)ttl;
    if (!NetConnStatus(0x73656375, 0, 0, 0)) return 0; // secu
    ProtoPingRequestT* item = PingAllocRequest(ref, 1, data, length);
    if (!item) return 0;
    item->address = address;
    return item->sequence;
}
extern "C" s32 ProtoPingResponse(ProtoPingRefT* ref, u8* data, s32* length, ProtoPingResponseT* response)
{
    ProtoPingRequestT* item = PingPoll(ref);
    if (!item) return 0;
    const s32 ping = item->ping > 0 ? item->ping : -1;
    if (response)
    {
        memset(response, 0, sizeof(*response));
        response->iPing = static_cast<s16>(ping);
        response->uSeqn = static_cast<u16>(item->sequence);
        response->bServer = item->type == 1;
        response->uNumRetries = static_cast<u16>(item->retries);
        memcpy(&response->Addr, &item->session.sessionID, 8);
    }
    if (length)
    {
        if (*length > item->dataLength) *length = item->dataLength;
        if (*length) memcpy(data, item->data, *length);
    }
    PingFreeRequest(ref, item);
    return ping;
}
