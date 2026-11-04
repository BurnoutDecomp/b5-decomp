// DirtySDK native ProtoAdvt discovery owner, ARTIST82B3B348..82B3BF80.
#include "protoadvt.h"
#include "dirtynet_x360_internal.h"
#include "dirtymem.h"
#include "dirtylib.h"
#include <stddef.h>
#include <string.h>

struct ProtoAdvtRecord
{
    u8 magic[3], frequency, sequence[4];
    char kind[32], name[32], note[192], address[120];
    u32 expiry; char sourceText[16]; u32 sourceAddress, localAddress, sameHost;
    ProtoAdvtRecord* next;
};
struct ProtoAdvtRefT
{
    NetCritT crit; ProtoAdvtRecord* announced; ProtoAdvtRecord* first; ProtoAdvtRecord* end;
    s32 memGroup, probePending, reserved; SocketT* socket; sockaddr destination; u32 revision;
};
CGS_STATIC_ASSERT((sizeof(ProtoAdvtRecord) == 420), "ARTIST advertisement record");
CGS_STATIC_ASSERT((offsetof(ProtoAdvtRecord, expiry) == 384), "ARTIST packet extent");
CGS_STATIC_ASSERT((offsetof(ProtoAdvtRecord, next) == 416), "ARTIST list link");
CGS_STATIC_ASSERT((sizeof(ProtoAdvtRefT) == 88), "ARTIST advert module");
CGS_STATIC_ASSERT((offsetof(ProtoAdvtRefT, socket) == 64), "ARTIST advert socket");
extern "C" {
SocketT* SocketOpen(s32, s32, s32);
s32 SocketBind(SocketT*, const sockaddr*, s32);
s32 SocketClose(SocketT*);
s32 SocketCallback(SocketT*, s32, s32, void*, s32 (*)(SocketT*, s32, void*));
s32 SocketSendto(SocketT*, const char*, s32, s32, const sockaddr*, s32);
s32 SocketRecvfrom(SocketT*, char*, s32, s32, sockaddr*, s32*);
s32 SocketHost(sockaddr*, s32, const sockaddr*, s32);
char* SockaddrInGetAddrText(const sockaddr*, char*, s32);
const char* SocketInAddrGetText(u32);
s32 ds_stricmp(const char*, const char*);
char* ds_strnzcpy(char*, const char*, s32);
}
static ProtoAdvtRefT* gAdvt;
static s32 gAdvtReferences;
static const s32 ADVT_MEM_ID = 0x70616476;
static u32 AdvtAddress(const sockaddr* address)
{
    const volatile u8* p = reinterpret_cast<const volatile u8*>(address);
    return (u32(p[4]) << 24) | (u32(p[5]) << 16) | (u32(p[6]) << 8) | p[7];
}
static void AdvtSendProbe(ProtoAdvtRefT* ref)
{
    u8 packet[384]; memset(packet, 0, sizeof(packet));
    packet[0] = 'g'; packet[1] = 'E'; packet[2] = 'A'; packet[8] = '?';
    SocketSendto(ref->socket, reinterpret_cast<const char*>(packet), sizeof(packet), 0, &ref->destination, 16);
}

// ARTIST82B3B348: receive, expire, cancel, and rebroadcast under the native lock.
static s32 AdvtUpdate(SocketT*, s32, void* context)
{
    ProtoAdvtRefT* ref = static_cast<ProtoAdvtRefT*>(context);
    if (!NetCritTry(&ref->crit))
    {
        NetPrintfCode("protoadvt: could not acquire critical section\n"); return 0;
    }
    while (ref->socket)
    {
        if (ref->probePending > 0) { ref->probePending = 0; AdvtSendProbe(ref); }
        ProtoAdvtRecord packet;
        sockaddr source, local;
        s32 sourceLength = 16;
        packet.kind[0] = 0;
        if (SocketRecvfrom(ref->socket, reinterpret_cast<char*>(&packet), 384, 0, &source, &sourceLength) <= 0 || !packet.kind[0]) break;
        SocketHost(&local, 16, &source, 16);
        const bool sameHost = local.sa_family == source.sa_family && source.sa_family == AF_INET &&
            memcmp(reinterpret_cast<const u8*>(&source) + 2, reinterpret_cast<const u8*>(&local) + 2, 6) == 0;
        if (packet.magic[0] != 'g' || packet.magic[1] != 'E' || packet.magic[2] != 'A') continue;
        if (packet.kind[0] == '?' && !packet.kind[1])
        {
            u32 soon = NetTick() + 1000;
            for (ProtoAdvtRecord* item = ref->announced; item; item = item->next)
                if (item->expiry > soon) item->expiry = soon;
        }
        else if (packet.name[0])
        {
            ProtoAdvtRecord* freeEntry = 0;
            ProtoAdvtRecord* item = ref->first;
            for (; item != ref->end; ++item)
            {
                if (!freeEntry && !item->name[0]) freeEntry = item;
                if (!ds_stricmp(packet.kind, item->kind) && !ds_stricmp(packet.name, item->name) &&
                    !memcmp(packet.sequence, item->sequence, 4)) break;
            }
            if (item == ref->end && freeEntry)
            {
                item = freeEntry; memset(item, 0, sizeof(*item));
                memcpy(item->sequence, packet.sequence, 4);
                ds_strnzcpy(item->kind, packet.kind, sizeof(item->kind));
                ds_strnzcpy(item->name, packet.name, sizeof(item->name));
                ds_strnzcpy(item->note, packet.note, sizeof(item->note));
                ++ref->revision;
                NetPrintfCode("protoadvt: added new advert name=%s freq=%d addr=%s seqn=%02x%02x%02x%02x\n",
                    item->name, packet.frequency, SocketInAddrGetText(AdvtAddress(&source)));
            }
            if (item != ref->end)
            {
                ds_strnzcpy(item->address, packet.address, sizeof(item->address));
                ds_strnzcpy(item->note, packet.note, sizeof(item->note));
                item->sameHost = sameHost;
                item->expiry = NetTick() + 2000 * packet.frequency + 1000;
                item->sourceAddress = AdvtAddress(&source);
                item->localAddress = AdvtAddress(&local);
                SockaddrInGetAddrText(&source, item->sourceText, sizeof(item->sourceText));
                ++ref->revision;
            }
        }
    }
    const u32 tick = NetTick();
    for (ProtoAdvtRecord* item = ref->first; item != ref->end; ++item)
    {
        if (item->name[0] && (!item->address[0] || !item->expiry || tick > item->expiry))
        {
            NetPrintfCode("protoadvt: expiring advert name=%s\n", item->name);
            item->name[0] = item->kind[0] = 0; ++ref->revision;
        }
    }
    ProtoAdvtRecord** link = &ref->announced;
    while (*link)
    {
        ProtoAdvtRecord* item = *link;
        if (item->expiry) link = &item->next;
        else
        {
            *link = item->next;
            NetPrintfCode("protoadvt: canceling announcement name=%s\n", item->name);
            item->address[0] = 0;
            SocketSendto(ref->socket, reinterpret_cast<const char*>(item), 384, 0, &ref->destination, 16);
            DirtyMemFree(item, ADVT_MEM_ID, ref->memGroup);
        }
    }
    const u32 now = NetTick();
    for (ProtoAdvtRecord* item = ref->announced; item; item = item->next)
    {
        if (now > item->expiry)
        {
            SocketSendto(ref->socket, reinterpret_cast<const char*>(item), 384, 0, &ref->destination, 16);
            item->expiry = now + 1000 * item->frequency;
        }
    }
    NetCritLeave(&ref->crit);
    return 0;
}
extern "C" s32 ProtoAdvtCancel(ProtoAdvtRefT* ref, const char* kind, const char* name)
{
    NetCritEnter(&ref->crit);
    ProtoAdvtRecord* item = ref->announced;
    for (; item; item = item->next)
        if ((!kind || !ds_stricmp(kind, item->kind)) && (!name || !ds_stricmp(name, item->name)))
        { item->expiry = 0; break; }
    NetCritLeave(&ref->crit);
    return item ? 0 : -1;
}
extern "C" ProtoAdvtRefT* ProtoAdvtConstruct(s32 maxRecords)
{
    s32 group = DirtyMemGroupQuery();
    if (gAdvt) { ++gAdvtReferences; return gAdvt; }
    ProtoAdvtRefT* ref = static_cast<ProtoAdvtRefT*>(DirtyMemAlloc(sizeof(*ref), ADVT_MEM_ID, group));
    if (!ref) return 0;
    memset(ref, 0, sizeof(*ref)); ref->memGroup = group;
    if (maxRecords < 4) maxRecords = 4;
    const s32 bytes = maxRecords * sizeof(ProtoAdvtRecord);
    ref->first = static_cast<ProtoAdvtRecord*>(DirtyMemAlloc(bytes, ADVT_MEM_ID, group));
    if (!ref->first) { DirtyMemFree(ref, ADVT_MEM_ID, ref->memGroup); return 0; }
    ref->end = ref->first + maxRecords; memset(ref->first, 0, bytes); ref->revision = 1;
    SocketT* socket = SocketOpen(AF_INET, SOCK_DGRAM, 0);
    if (!socket) { DirtyMemFree(ref, ADVT_MEM_ID, ref->memGroup); return 0; }
    gAdvt = ref; gAdvtReferences = 1;
    NetCritInit(&ref->crit, "protoadvt"); NetCritEnter(&ref->crit);
    sockaddr local; memset(&local, 0, sizeof(local)); local.sa_family = AF_INET;
    volatile u8* localBytes = reinterpret_cast<volatile u8*>(&local);
    localBytes[2] = 0x27; localBytes[3] = 0x0f;
    SocketBind(socket, &local, 16);
    sockaddr broadcast; memset(&broadcast, 0, sizeof(broadcast)); broadcast.sa_family = AF_INET;
    volatile u8* destBytes = reinterpret_cast<volatile u8*>(&broadcast);
    destBytes[2] = 0x27; destBytes[3] = 0x0f;
    destBytes[7] = destBytes[6] = destBytes[5] = destBytes[4] = 0xff;
    memcpy(&ref->destination, &broadcast, 16);
    ref->probePending = 1; ref->socket = socket;
    SocketCallback(socket, 2, 100, ref, AdvtUpdate);
    NetCritLeave(&ref->crit);
    AdvtSendProbe(ref);
    return ref;
}
extern "C" void ProtoAdvtDestroy(ProtoAdvtRefT* ref)
{
    if (!ref) return;
    if (gAdvtReferences > 1) { --gAdvtReferences; return; }
    gAdvt = 0; gAdvtReferences = 0;
    while (ref->announced)
    {
        ProtoAdvtCancel(ref, ref->announced->kind, ref->announced->name);
        AdvtUpdate(ref->socket, 0, ref);
    }
    NetCritEnter(&ref->crit); SocketT* socket = ref->socket; ref->socket = 0; NetCritLeave(&ref->crit);
    SocketClose(socket); NetCritKill(&ref->crit);
    DirtyMemFree(ref->first, ADVT_MEM_ID, ref->memGroup);
    DirtyMemFree(ref, ADVT_MEM_ID, ref->memGroup);
}
extern "C" s32 ProtoAdvtAnnounce(ProtoAdvtRefT* ref, const char* kind, const char* name,
                                  const char* note, const char* address, s32 frequency)
{
    if (!frequency) frequency = 30;
    if (frequency < 2) frequency = 2;
    if (frequency > 250) frequency = 250;
    if (!kind || !*kind) { NetPrintfCode("protoadvt: error, invalid kind\n"); return -1; }
    if (!name || !*name) { NetPrintfCode("protoadvt: error, invalid name\n"); return -2; }
    if (!note) { NetPrintfCode("protoadvt: error, invalid note\n"); return -3; }
    if (!address) { NetPrintfCode("protoadvt: error, invalid addr\n"); return -4; }
    ProtoAdvtRecord* item = ref->announced;
    for (; item; item = item->next)
        if (!ds_stricmp(kind, item->kind) && !ds_stricmp(name, item->name)) break;
    if (!item)
    {
        item = static_cast<ProtoAdvtRecord*>(DirtyMemAlloc(sizeof(*item), ADVT_MEM_ID, ref->memGroup));
        const u32 tick = NetTick(); item->expiry = tick;
        item->magic[0] = 'g'; item->magic[1] = 'E'; item->magic[2] = 'A'; item->frequency = frequency;
        item->sequence[0] = tick >> 24; item->sequence[1] = tick >> 16; item->sequence[2] = tick >> 8; item->sequence[3] = tick;
        ds_strnzcpy(item->kind, kind, sizeof(item->kind)); ds_strnzcpy(item->name, name, sizeof(item->name));
        ds_strnzcpy(item->address, address, sizeof(item->address)); ds_strnzcpy(item->note, note, sizeof(item->note));
        NetPrintfCode("protoadvt: broadcasting new announcement name=%s freq=%d\n", item->name, item->frequency);
        SocketSendto(ref->socket, reinterpret_cast<const char*>(item), 384, 0, &ref->destination, 16);
        item->expiry = NetTick() + 250;
        NetCritEnter(&ref->crit); item->next = ref->announced; ref->announced = item; NetCritLeave(&ref->crit);
    }
    else
    {
        if (ds_stricmp(address, item->address)) { ds_strnzcpy(item->address, address, sizeof(item->address)); item->expiry = NetTick() - 1; }
        if (ds_stricmp(note, item->note)) { ds_strnzcpy(item->note, note, sizeof(item->note)); item->expiry = NetTick() - 1; }
    }
    return 0;
}
extern "C" u32 ProtoAdvtLocate(ProtoAdvtRefT* ref, const char* kind, const char* name, u32* localAddress, u32 fallback)
{
    if (ref)
    {
        for (ProtoAdvtRecord* item = ref->first; item != ref->end; ++item)
        {
            if (item->name[0] && !item->sameHost && (!kind || !*kind || !ds_stricmp(kind, item->kind)) &&
                (!name || !*name || !ds_stricmp(item->name, name)) && item->sourceAddress)
            {
                if (localAddress) *localAddress = item->localAddress;
                return item->sourceAddress;
            }
        }
    }
    return fallback;
}
