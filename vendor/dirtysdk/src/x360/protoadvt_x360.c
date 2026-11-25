// DirtySDK native ProtoAdvt discovery owner, ARTIST82B3B348..82B3BF80.
// XDK size optimization matches ARTIST at 0x82B3BB08.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

#include "protoadvt.h"
#include "dirtylib_x360_internal.h"
#include "dirtymem.h"
#include "dirtylib.h"
#include <stddef.h>
#include <string.h>

typedef struct SocketT SocketT;
// DirtySDK protoadvt.c:42-77. The wire packet is the prefix of each local record.
typedef struct ProtoAdvtPacketT ProtoAdvtPacketT;
struct ProtoAdvtPacketT
{
    u8 ident[3];
    u8 freq;
    u8 seqn[4];
    char kind[32];
    char name[32];
    char note[192];
    char addr[120];
};

typedef struct ServListT ServListT;
struct ServListT
{
    ProtoAdvtPacketT packet;
    u32 timeout;
    char inetdot[16];
    u32 inetval;
    u32 hostval;
    s32 local;
    ServListT* next;
};

struct ProtoAdvtRefT
{
    NetCritT crit; ServListT* announced; ServListT* first; ServListT* end;
    s32 memGroup, probePending, reserved; SocketT* socket; struct sockaddr destination; u32 revision;
};
CGS_STATIC_ASSERT((sizeof(ServListT) == 420), "ARTIST advertisement record");
CGS_STATIC_ASSERT((sizeof(ProtoAdvtPacketT) == 384), "ARTIST packet extent");
CGS_STATIC_ASSERT((offsetof(ServListT, timeout) == 384), "ARTIST record packet prefix");
CGS_STATIC_ASSERT((offsetof(ServListT, next) == 416), "ARTIST list link");
CGS_STATIC_ASSERT((sizeof(ProtoAdvtRefT) == 88), "ARTIST advert module");
CGS_STATIC_ASSERT((offsetof(ProtoAdvtRefT, socket) == 64), "ARTIST advert socket");
SocketT* SocketOpen(s32, s32, s32);
s32 SocketBind(SocketT*, const struct sockaddr*, s32);
s32 SocketClose(SocketT*);
s32 SocketCallback(SocketT*, s32, s32, void*, s32 (*)(SocketT*, s32, void*));
s32 SocketSendto(SocketT*, const char*, s32, s32, const struct sockaddr*, s32);
s32 SocketRecvfrom(SocketT*, char*, s32, s32, struct sockaddr*, s32*);
s32 SocketHost(struct sockaddr*, s32, const struct sockaddr*, s32);
char* SockaddrInGetAddrText(const struct sockaddr*, char*, s32);
const char* SocketInAddrGetText(u32);
s32 ds_stricmp(const char*, const char*);
char* ds_strnzcpy(char*, const char*, s32);
// ARTIST uses one base for the adjacent globals g_count and g_ref (DecFIGS protoadvt.c:110-111).
typedef struct ProtoAdvtGlobals ProtoAdvtGlobals;
struct ProtoAdvtGlobals
{
    s32 g_count;
    ProtoAdvtRefT* g_ref;
};
static ProtoAdvtGlobals g_state;
CGS_STATIC_ASSERT(offsetof(ProtoAdvtGlobals, g_ref) == 4, "ARTIST advertisement globals");
static const s32 ADVT_MEM_ID = 0x70616476;
static u32 AdvtAddress(const struct sockaddr* address)
{
    const volatile u8* p = (const volatile u8*)address;
    return ((u32)p[4] << 24) | ((u32)p[5] << 16) | ((u32)p[6] << 8) | p[7];
}
static void _ProtoAdvtRequestSeed(ProtoAdvtRefT* ref)
{
    ProtoAdvtPacketT packet;
    memset(&packet, 0, sizeof(packet));
    packet.ident[0] = "gEA"[0];
    packet.ident[1] = "gEA"[1];
    packet.ident[2] = "gEA"[2];
    packet.kind[0] = '?';
    SocketSendto(ref->socket, (const char*)&packet, sizeof(packet), 0,
                 &ref->destination, sizeof(ref->destination));
}

// ARTIST82B3B348: receive, expire, cancel, and rebroadcast under the native lock.
static s32 _ProtoAdvtCallback(SocketT* sock, s32 flags, void* context)
{
    ProtoAdvtRefT* ref = (ProtoAdvtRefT*)context;
    u32 tick, now;
    ServListT* item;
    ServListT** link;
    (void)sock;
    (void)flags;
    if (!NetCritTry(&ref->crit))
    {
        NetPrintfCode("protoadvt: could not acquire critical section\n"); return 0;
    }
    while (ref->socket)
    {
        ProtoAdvtPacketT packet;
        struct sockaddr source, local;
        s32 sourceLength = 16;
        s32 sameHost;
        if (ref->probePending > 0) { ref->probePending = 0; _ProtoAdvtRequestSeed(ref); }
        packet.kind[0] = 0;
        if (SocketRecvfrom(ref->socket, (char*)&packet, sizeof(packet), 0, &source, &sourceLength) <= 0 || !packet.kind[0]) break;
        SocketHost(&local, 16, &source, 16);
        sameHost = local.sa_family == source.sa_family && source.sa_family == AF_INET &&
            memcmp((const u8*)&source + 2, (const u8*)&local + 2, 6) == 0;
        if (packet.ident[0] != 'g' || packet.ident[1] != 'E' || packet.ident[2] != 'A') continue;
        if (packet.kind[0] == '?' && !packet.kind[1])
        {
            u32 soon = NetTick() + 1000;
            for (item = ref->announced; item; item = item->next)
                if (item->timeout > soon) item->timeout = soon;
        }
        else if (packet.name[0])
        {
            ServListT* freeEntry = 0;
            ServListT* item = ref->first;
            for (; item != ref->end; ++item)
            {
                if (!freeEntry && !item->packet.name[0]) freeEntry = item;
                if (!ds_stricmp(packet.kind, item->packet.kind) && !ds_stricmp(packet.name, item->packet.name) &&
                    !memcmp(packet.seqn, item->packet.seqn, 4)) break;
            }
            if (item == ref->end && freeEntry)
            {
                item = freeEntry; memset(item, 0, sizeof(*item));
                memcpy(item->packet.seqn, packet.seqn, 4);
                ds_strnzcpy(item->packet.kind, packet.kind, sizeof(item->packet.kind));
                ds_strnzcpy(item->packet.name, packet.name, sizeof(item->packet.name));
                ds_strnzcpy(item->packet.note, packet.note, sizeof(item->packet.note));
                ++ref->revision;
                NetPrintfCode("protoadvt: added new advert name=%s freq=%d addr=%s seqn=%02x%02x%02x%02x\n",
                    item->packet.name, packet.freq, SocketInAddrGetText(AdvtAddress(&source)));
            }
            if (item != ref->end)
            {
                ds_strnzcpy(item->packet.addr, packet.addr, sizeof(item->packet.addr));
                ds_strnzcpy(item->packet.note, packet.note, sizeof(item->packet.note));
                item->local = sameHost;
                item->timeout = NetTick() + 2000 * packet.freq + 1000;
                item->inetval = AdvtAddress(&source);
                item->hostval = AdvtAddress(&local);
                SockaddrInGetAddrText(&source, item->inetdot, sizeof(item->inetdot));
                ++ref->revision;
            }
        }
    }
    tick = NetTick();
    for (item = ref->first; item != ref->end; ++item)
    {
        if (item->packet.name[0] && (!item->packet.addr[0] || !item->timeout || tick > item->timeout))
        {
            NetPrintfCode("protoadvt: expiring advert name=%s\n", item->packet.name);
            item->packet.name[0] = item->packet.kind[0] = 0; ++ref->revision;
        }
    }
    link = &ref->announced;
    while (*link)
    {
        ServListT* item = *link;
        if (item->timeout) link = &item->next;
        else
        {
            *link = item->next;
            NetPrintfCode("protoadvt: canceling announcement name=%s\n", item->packet.name);
            item->packet.addr[0] = 0;
            SocketSendto(ref->socket, (const char*)&item->packet, sizeof(item->packet), 0, &ref->destination, 16);
            DirtyMemFree(item, ADVT_MEM_ID, ref->memGroup);
        }
    }
    now = NetTick();
    for (item = ref->announced; item; item = item->next)
    {
        if (now > item->timeout)
        {
            SocketSendto(ref->socket, (const char*)&item->packet, sizeof(item->packet), 0, &ref->destination, 16);
            item->timeout = now + 1000 * item->packet.freq;
        }
    }
    NetCritLeave(&ref->crit);
    return 0;
}
s32 ProtoAdvtCancel(ProtoAdvtRefT* ref, const char* kind, const char* name)
{
    ServListT* item;
    NetCritEnter(&ref->crit);
    item = ref->announced;
    for (; item; item = item->next)
        if ((!kind || !ds_stricmp(kind, item->packet.kind)) && (!name || !ds_stricmp(name, item->packet.name)))
        { item->timeout = 0; break; }
    NetCritLeave(&ref->crit);
    return item ? 0 : -1;
}
ProtoAdvtRefT* ProtoAdvtConstruct(s32 maxRecords)
{
    s32 group = DirtyMemGroupQuery();
    ProtoAdvtRefT* ref;
    SocketT* socket;
    s32 bytes;
    const u32 zero = 0;
    struct sockaddr_in bindaddr, peeraddr;
    volatile u8 *localBytes, *destBytes;
    if (g_state.g_ref) { ++g_state.g_count; return g_state.g_ref; }
    ref = (ProtoAdvtRefT*)DirtyMemAlloc(sizeof(*ref), ADVT_MEM_ID, group);
    if (!ref) return 0;
    memset(ref, 0, sizeof(*ref)); ref->memGroup = group;
    if (maxRecords < 4) maxRecords = 4;
    bytes = maxRecords * sizeof(ServListT);
    ref->first = (ServListT*)DirtyMemAlloc(bytes, ADVT_MEM_ID, group);
    if (!ref->first) { DirtyMemFree(ref, ADVT_MEM_ID, ref->memGroup); return 0; }
    ref->end = ref->first + maxRecords;
    memset(ref->first, 0, (char*)ref->end - (char*)ref->first);
    ref->revision = 1;
    socket = SocketOpen(AF_INET, SOCK_DGRAM, 0);
    if (!socket) { DirtyMemFree(ref, ADVT_MEM_ID, ref->memGroup); return 0; }
    g_state.g_ref = ref; g_state.g_count = 1;
    NetCritInit(&ref->crit, "protoadvt"); NetCritEnter(&ref->crit);
    bindaddr.sin_port = 0;
    bindaddr.sin_family = AF_INET;
    bindaddr.sin_addr.s_addr = 0;
    memcpy(bindaddr.sin_zero, &zero, sizeof(zero));
    memcpy(bindaddr.sin_zero + sizeof(zero), &zero, sizeof(zero));
    localBytes = (volatile u8*)&bindaddr;
    localBytes[2] = 0x27;
    localBytes[3] = 0x0f;
    SocketBind(socket, (struct sockaddr*)&bindaddr, sizeof(bindaddr));

    peeraddr.sin_addr.s_addr = 0;
    peeraddr.sin_port = 0;
    peeraddr.sin_family = AF_INET;
    memcpy(peeraddr.sin_zero, &zero, sizeof(zero));
    memcpy(peeraddr.sin_zero + sizeof(zero), &zero, sizeof(zero));
    destBytes = (volatile u8*)&peeraddr;
    destBytes[2] = 0x27;
    destBytes[3] = 0x0f;
    destBytes[7] = destBytes[6] = destBytes[5] = destBytes[4] = 0xff;
    memcpy(&ref->destination, &peeraddr, sizeof(peeraddr));
    ref->probePending = 1; ref->socket = socket;
    SocketCallback(socket, 2, 100, ref, _ProtoAdvtCallback);
    NetCritLeave(&ref->crit);
    _ProtoAdvtRequestSeed(ref);
    return ref;
}
void ProtoAdvtDestroy(ProtoAdvtRefT* ref)
{
    SocketT* socket;
    if (!ref) return;
    if (g_state.g_count > 1) { --g_state.g_count; return; }
    g_state.g_ref = 0;
    g_state.g_count = 0;
    while (ref->announced)
    {
        ProtoAdvtCancel(ref, ref->announced->packet.kind, ref->announced->packet.name);
        _ProtoAdvtCallback(ref->socket, 0, ref);
    }
    NetCritEnter(&ref->crit); socket = ref->socket; ref->socket = 0; NetCritLeave(&ref->crit);
    SocketClose(socket); NetCritKill(&ref->crit);
    DirtyMemFree(ref->first, ADVT_MEM_ID, ref->memGroup);
    DirtyMemFree(ref, ADVT_MEM_ID, ref->memGroup);
}
s32 ProtoAdvtAnnounce(ProtoAdvtRefT* ref, const char* kind, const char* name,
                                  const char* note, const char* address, s32 frequency)
{
    ServListT *item, *list;
    u32 tick;
    if (!frequency) frequency = 30;
    if (frequency < 2) frequency = 2;
    if (frequency > 250) frequency = 250;
    if (!kind || !*kind) { NetPrintfCode("protoadvt: error, invalid kind\n"); return -1; }
    if (!name || !*name) { NetPrintfCode("protoadvt: error, invalid name\n"); return -2; }
    if (!note) { NetPrintfCode("protoadvt: error, invalid note\n"); return -3; }
    if (!address) { NetPrintfCode("protoadvt: error, invalid addr\n"); return -4; }
    for (list = ref->announced; list; list = list->next)
    {
        if (!ds_stricmp(kind, list->packet.kind) && !ds_stricmp(name, list->packet.name))
        {
            if (ds_stricmp(address, list->packet.addr))
            {
                ds_strnzcpy(list->packet.addr, address, sizeof(list->packet.addr));
                list->timeout = NetTick() - 1;
            }
            if (ds_stricmp(note, list->packet.note))
            {
                ds_strnzcpy(list->packet.note, note, sizeof(list->packet.note));
                list->timeout = NetTick() - 1;
            }
            return 0;
        }
    }

    item = (ServListT*)DirtyMemAlloc(sizeof(*item), ADVT_MEM_ID, ref->memGroup);
    tick = NetTick();
    item->timeout = tick;
    item->packet.ident[0] = "gEA"[0];
    item->packet.ident[1] = "gEA"[1];
    item->packet.ident[2] = "gEA"[2];
    item->packet.freq = frequency;
    item->packet.seqn[0] = tick >> 24;
    item->packet.seqn[1] = tick >> 16;
    item->packet.seqn[2] = tick >> 8;
    item->packet.seqn[3] = tick;
    ds_strnzcpy(item->packet.kind, kind, sizeof(item->packet.kind));
    ds_strnzcpy(item->packet.name, name, sizeof(item->packet.name));
    ds_strnzcpy(item->packet.addr, address, sizeof(item->packet.addr));
    ds_strnzcpy(item->packet.note, note, sizeof(item->packet.note));
    NetPrintfCode("protoadvt: broadcasting new announcement name=%s freq=%d\n", item->packet.name, item->packet.freq);
    SocketSendto(ref->socket, (const char*)&item->packet, sizeof(item->packet), 0, &ref->destination, 16);
    item->timeout = NetTick() + 250;
    NetCritEnter(&ref->crit);
    item->next = ref->announced;
    ref->announced = item;
    NetCritLeave(&ref->crit);
    return 0;
}
u32 ProtoAdvtLocate(ProtoAdvtRefT* ref, const char* kind, const char* name, u32* localAddress, u32 fallback)
{
    ServListT* item;
    if (ref)
    {
        for (item = ref->first; item != ref->end; ++item)
        {
            if (item->packet.name[0] && !item->local && (!kind || !*kind || !ds_stricmp(kind, item->packet.kind)) &&
                (!name || !*name || !ds_stricmp(item->packet.name, name)) && item->inetval)
            {
                if (localAddress) *localAddress = item->hostval;
                fallback = item->inetval;
                break;
            }
        }
    }
    return fallback;
}
