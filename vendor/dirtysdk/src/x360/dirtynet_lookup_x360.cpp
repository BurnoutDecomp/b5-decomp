#include "dirtynet_x360_internal.h"
#include "dirtylsp.h"
#include "dirtymem.h"
#include "dirtylib.h"
#include <string.h>

extern "C" {
u32 SocketInTextGetAddr(const char*);
const char* SocketInAddrGetText(u32);
char* SockaddrInGetAddrText(const sockaddr*, char*, s32);
char* ds_strnzcpy(char*, const char*, s32);
}
static const s32 SOCKET_MEM_ID = 0x64736f63;

// ARTIST uses byte access for the port and IPv4 fields of public sockaddrs.
static u16 SocketReadPort(const void* value)
{
    const volatile u8* p = static_cast<const volatile u8*>(value);
    return static_cast<u16>((u16(p[2]) << 8) | p[3]);
}
static void SocketWriteAddr(void* value, u32 address)
{
    volatile u8* p = static_cast<volatile u8*>(value);
    p[7] = static_cast<u8>(address);
    p[6] = static_cast<u8>(address >> 8);
    p[5] = static_cast<u8>(address >> 16);
    p[4] = static_cast<u8>(address >> 24);
}
static void SocketWritePort(void* value, u16 port)
{
    volatile u8* p = static_cast<volatile u8*>(value);
    p[3] = static_cast<u8>(port);
    p[2] = static_cast<u8>(port >> 8);
}

// ARTIST82B2AF40: wildcard supported only at the start or end.
static bool SocketNameMatch(const char* name, const char* pattern)
{
    const char* end = pattern + strlen(pattern) - 1;
    if (*pattern == '*')
    {
        const char* tail = name + strlen(name);
        while (--tail != name && end != pattern && *tail == *end) --end;
        if (*end == '*') { ++tail; ++end; }
        return strcmp(tail, end) == 0;
    }
    if (*end == '*')
    {
        while (*name && *pattern && *name == *pattern) { ++name; ++pattern; }
        return *pattern == '*';
    }
    return strcmp(name, pattern) == 0;
}

// ARTIST82B2B0A0: first matching nonempty name-map record.
static SocketNameMapX360* SocketFindName(SocketStateX360* state, const char* name)
{
    SocketNameMapX360* map = state->nameMap;
    while (*map->match)
    {
        if (SocketNameMatch(name, map->match))
        {
            NetPrintfCode("dirtynetwin: name remap %s", name);
            NetPrintfCode(" to %s\n", map->remapAddress ? SocketInAddrGetText(map->remapAddress) : map->remapName);
            return map;
        }
        ++map;
    }
    return 0;
}

// ARTIST82B2B150: name remapping precedes the native address/port map.
const sockaddr* SocketMapAddress(sockaddr* output, const sockaddr* input)
{
    const volatile u8* source = reinterpret_cast<const volatile u8*>(input);
    const u32 a = source[4], b = source[5], c = source[6];
    const u32 portHi = source[2], portLo = source[3], d = source[7];
    u32 address = (a << 24) | (b << 16) | (c << 8) | d;
    const u16 port = static_cast<u16>((portHi << 8) | portLo);
    char text[16];
    SockaddrInGetAddrText(input, text, sizeof(text));
    SocketStateX360* state = gSocketStateX360;
    SocketNameMapX360* named = SocketFindName(state, text);
    if (named)
    {
        if (named->remapAddress) address = named->remapAddress;
        else NetPrintfCode("dirtynetwin: warning -- could not map address %s using name map, as remap address is not yet specified\n", text);
    }
    for (SocketAddrMapX360* map = state->addressMap; map->remap; ++map)
    {
        if (map->address == (map->mask & address) && (!map->port || !port || map->port == port))
        {
            const u16 newPort = map->remapPort ? map->remapPort : port;
            NetPrintfCode("dirtynetwin: addr remap %s:%d", SocketInAddrGetText(address), port);
            NetPrintfCode(" to %s:%d\n", SocketInAddrGetText(map->remap), newPort);
            memcpy(output, input, 16);
            SocketWritePort(output, newPort);
            SocketWriteAddr(output, map->remap);
            return output;
        }
    }
    return input;
}

static s32 SocketLookupDnsDone(HostentT* host)
{
    XNDNS* dns = static_cast<XNDNS*>(host->pDns);
    HANDLE event = static_cast<HANDLE>(host->pPlatformData);
    if (event && WaitForSingleObject(event, 0) == WAIT_OBJECT_0)
    {
        if (dns->iStatus)
        {
            if (dns->iStatus != WSAEINPROGRESS)
            {
                NetPrintfCode("socketlookupdone: failure, err=%d\n", dns->iStatus);
                host->done = -1;
            }
        }
        else
        {
            host->done = 1;
            if (dns->cina)
            {
                host->addr = dns->aina[0].s_addr;
                NetPrintfCode("socketlookupdone: success, addr=%s\n", SocketInAddrGetText(host->addr));
            }
        }
    }
    return host->done;
}
static void SocketLookupDnsFree(HostentT* host)
{
    SocketStateX360* state = gSocketStateX360;
    if (host->pDns) XNetDnsRelease(static_cast<XNDNS*>(host->pDns));
    if (host->pPlatformData) WSACloseEvent(static_cast<HANDLE>(host->pPlatformData));
    DirtyMemFree(host, SOCKET_MEM_ID, state->memGroup);
}

static s32 SocketLookupLspDone(HostentT* host)
{
    SocketStateX360* state = gSocketStateX360;
    if (host->done == 1) return 1;
    s32 result = DirtyLSPQueryDone(state->lsp, host);
    if (result == 1)
    {
        u32 resolved = host->addr;
        host->addr = state->nextAddress++;
        u32 index = 0;
        while (index < 32 && state->addressMap[index].address) ++index;
        if (index < 32)
        {
            for (u32 i = 0; i < 32; ++i)
            {
                SocketNameMapX360& name = state->nameMap[i];
                if (strcmp(host->name, name.remapName) == 0)
                {
                    if (index == 32) break;
                    name.remapAddress = host->addr;
                    SocketAddrMapX360& map = state->addressMap[index++];
                    map.address = host->addr; map.mask = ~0u; map.remap = resolved;
                    map.port = name.port; map.remapPort = name.remapPort;
                }
            }
        }
        if (index == 32) NetPrintfCode("dirtynetwin: warning -- could not add lsp query result to address map\n");
    }
    return result;
}
static void SocketLookupLspFree(HostentT* host)
{
    SocketStateX360* state = gSocketStateX360;
    DirtyLSPQueryFree(state->lsp, host);
    DirtyMemFree(host, SOCKET_MEM_ID, state->memGroup);
}

extern "C" HostentT* SocketLookup(const char* name, s32 timeout)
{
    SocketStateX360* state = gSocketStateX360;
    if (timeout < 0) return 0;
    HostentT* host = static_cast<HostentT*>(DirtyMemAlloc(sizeof(*host), SOCKET_MEM_ID, state->memGroup));
    memset(host, 0, sizeof(*host));
    host->Done = SocketLookupDnsDone; host->Free = SocketLookupDnsFree;
    NetPrintfCode("dirtynetwin: looking up address for '%s'\n", name);
    if (state->dnsRemap)
    {
        SocketNameMapX360* map = SocketFindName(state, name);
        if (map)
        {
            if (map->remapAddress) { host->addr = map->remapAddress; host->done = 1; return host; }
            ds_strnzcpy(host->name, map->remapName, sizeof(host->name));
            s32 error = DirtyLSPQueryStart(state->lsp, host);
            if (!error) { host->Done = SocketLookupLspDone; host->Free = SocketLookupLspFree; return host; }
            NetPrintfCode("dirtynetwin: error %d initiating service query\n", error);
            SocketLookupLspFree(host);
            return 0;
        }
        u32 address = SocketInTextGetAddr(name);
        if (!address)
        {
            NetPrintfCode("dirtynetwin: dns remap %s to %s\n", name, SocketInAddrGetText(state->dnsRemap));
            address = state->dnsRemap;
        }
        host->addr = address; host->done = 1; return host;
    }
    const u32 address = SocketInTextGetAddr(name);
    if (address) { host->addr = address; host->done = 1; return host; }
    host->pPlatformData = WSACreateEvent();
    host->pDns = 0;
    ds_strnzcpy(host->name, name, sizeof(host->name));
    XNDNS* dns = 0;
    s32 error = XNetDnsLookup(host->name, static_cast<HANDLE>(host->pPlatformData), &dns);
    host->pDns = dns;
    if (error)
    {
        NetPrintfCode("dirtynetwin: error %d initiating DNS lookup\n", error);
        WSACloseEvent(static_cast<HANDLE>(host->pPlatformData));
        host->pPlatformData = 0; host->addr = 0; host->done = -1;
    }
    return host;
}
// ARTIST82B2B2D8, translate native socket errors into DirtySock results.
s32 SocketTranslateErrorCode(s32 value, s32 error)
{
    if (value >= 0) return value;
    switch (error)
    {
    case 10035: case 10054: case 997: return 0;
    case 10051: case 10065: return -5;
    case 10057: return -2;
    case 10061: return -6;
    case 10022: return -11;
    default: return -7;
    }
}
s32 SocketTranslateError(s32 value)
{
    return SocketTranslateErrorCode(value, WSAGetLastError());
}
static void SocketFdAdd(fd_set* set, SOCKET nativeSocket)
{
    for (u32 i = 0; i < set->fd_count; ++i) if (set->fd_array[i] == nativeSocket) return;
    if (set->fd_count < 64) set->fd_array[set->fd_count++] = nativeSocket;
}

extern "C" s32 SocketControl(SocketT* socket, s32 option, s32 data, void* data2, void* data3)
{
    SocketStateX360* state = gSocketStateX360;
    switch (option)
    {
    case 0x6c616472: state->reservedF54 = data; return 0; // ladr
    case 0x6d616472: // madr
    {
        sockaddr mapped;
        const sockaddr* result = SocketMapAddress(&mapped, static_cast<const sockaddr*>(data2));
        memcpy(data3, result, 16); return 0;
    }
    case 0x6e646c79: // ndly
        if (!socket || socket->type != 1) return -1;
        return SocketTranslateError(setsockopt(socket->socket, 6, 1, reinterpret_cast<const char*>(&data), 4));
    case 0x706f6c6c: // poll
    {
        timeval timeout; timeout.tv_sec = data / 1000; timeout.tv_usec = data % 1000;
        fd_set reads, errors; reads.fd_count = errors.fd_count = 0;
        s32 count = 0;
        if (data2)
        {
            SocketT** list = static_cast<SocketT**>(data2);
            while (*list && count < 64)
            {
                SocketFdAdd(&reads, (*list)->socket); SocketFdAdd(&errors, (*list)->socket);
                ++list; ++count;
            }
        }
        else
        {
            NetCritEnter(0);
            for (SocketT* item = state->sockets; item && count < 64; item = item->next, ++count)
            {
                SocketFdAdd(&reads, item->socket); SocketFdAdd(&errors, item->socket);
            }
            NetCritLeave(0);
        }
        return select(count, &reads, 0, &errors, &timeout);
    }
    case 0x70757368: // push
        if (!socket) return -1;
        NetCritEnter(&socket->recvCrit);
        if (static_cast<u32>(data) <= 1264)
        {
            if (socket->recvSize > 0) NetPrintfCode("dirtynetwin: warning, overwriting packet data with SocketControl('recv')\n");
            socket->recvSize = data;
            memcpy(socket->recvData, data2, data);
            memcpy(&socket->recvFrom, data3, 16);
            u32 tick = NetTick();
            u8* stamp = reinterpret_cast<u8*>(&socket->recvFrom) + 8;
            stamp[0] = tick >> 24; stamp[1] = tick >> 16; stamp[2] = tick >> 8; stamp[3] = tick;
            NetCritLeave(&socket->recvCrit);
            if (socket->callback && (socket->callbackFlags & 2)) socket->callback(socket, 0, socket->callbackRef);
            return 0;
        }
        NetPrintfCode("dirtynetwin: request to push %d bytes of data discarded (max=%d)\n", data, 1264);
        NetCritLeave(&socket->recvCrit); return -1;
    case 0x72616472: // radr
        return SocketTranslateError(setsockopt(socket->socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&data), 4));
    case 0x73646362: state->sendCallback = reinterpret_cast<void*>(data2); state->sendRef = data3; return 0; // sdcb
    case 0x76616464: // vadd
        for (u32 i = 0; i < 32; ++i) if (!state->virtualPorts[i]) { state->virtualPorts[i] = static_cast<u16>(data); return 0; }
        return -1;
    case 0x7664656c: // vdel
        for (u32 i = 0; i < 32; ++i) if (state->virtualPorts[i] == static_cast<u16>(data)) { state->virtualPorts[i] = 0; return 0; }
        return -1;
    case 0x78646e73: state->dnsRemap = data; return 0; // xdns
    case 0x786d6170: // xmap
    {
        u32 bytes = static_cast<u32>(data) * 16;
        if (bytes > 512) { NetPrintfCode("dirtynetwin: clamping addr map to %d entries\n", 32); bytes = 512; }
        memcpy(state->addressMap, data2, bytes); return 0;
    }
    case 0x786e616d: // xnam
    {
        u32 bytes = static_cast<u32>(data) * 104;
        if (bytes > 3328) { NetPrintfCode("dirtynetwin: clamping name map to %d entries\n", 32); bytes = 3328; }
        memcpy(state->nameMap, data2, bytes); return 0;
    }
    }
    return -1;
}

extern "C" s32 SocketInfo(SocketT* socket, s32 info, s32 data, void* output, s32 length)
{
    SocketStateX360* state = gSocketStateX360;
    if (output) memset(output, 0, length);
    if (!socket)
    {
        if (info == 0x62696e64) // bind
        {
            s32 result = -1;
            NetCritEnter(0);
            for (SocketT* item = state->sockets; item; item = item->next)
            {
                sockaddr_in bound;
                SocketInfo(item, info, 0, &bound, sizeof(bound));
                if (SocketReadPort(&bound) == data) { result = 0; *static_cast<SocketT**>(output) = item; }
            }
            NetCritLeave(0);
            return result;
        }
        if (info == 0x6d617870) return 1264; // maxp
        if (info == 0x786d6170 && output && length > 0)
        {
            if (length > 512) length = 512;
            memcpy(output, state->addressMap, length); return 0;
        }
        if (info == 0x786e616d && output && length > 0)
        {
            if (length > 3328) length = 3328;
            memcpy(output, state->nameMap, length); return 0;
        }
        return -1;
    }
    if (info == 0x62696e64)
    {
        sockaddr* bound = static_cast<sockaddr*>(output);
        if (socket->virtualSocket == 1)
        {
            volatile u8* bytes = reinterpret_cast<volatile u8*>(bound);
            bytes[2] = 0; bytes[3] = 0;
            bound->sa_family = AF_INET;
            for (s32 i = 4; i < 16; ++i) bytes[i] = 0;
            bytes[2] = static_cast<u8>(socket->virtualPort >> 8);
            bytes[3] = static_cast<u8>(socket->virtualPort);
        }
        else getsockname(socket->socket, bound, &length);
        if (socket->type == SOCK_STREAM && SocketReadPort(bound))
        {
            XNADDR local;
            XNetGetTitleXnAddr(&local);
            SocketWriteAddr(bound, local.ina.s_addr);
        }
        return 0;
    }
    if (socket->socket == INVALID_SOCKET) return -7;
    if (info == 0x636f6e6e || info == 0x70656572)
    {
        getpeername(socket->socket, static_cast<sockaddr*>(output), &length);
        return 0;
    }
    if (info == 0x70726f74) return socket->protocol;
    if (info != 0x73746174) return -1;
    if (!socket->opened)
    {
        fd_set writable, errors;
        writable.fd_count = errors.fd_count = 1;
        writable.fd_array[0] = errors.fd_array[0] = socket->socket;
        timeval timeout; timeout.tv_sec = timeout.tv_usec = 0;
        if (select(1, 0, &writable, &errors, &timeout))
        {
            if (errors.fd_count) { NetPrintfCode("dirtynetwin: connection failed\n"); socket->opened = -1; }
            else if (writable.fd_count) { NetPrintfCode("dirtynetwin: connection open\n"); socket->opened = 1; }
        }
    }
    if (socket->opened > 0)
    {
        sockaddr peer; s32 peerLength = 16;
        if (SocketTranslateError(getpeername(socket->socket, &peer, &peerLength)) == -2)
        {
            NetPrintfCode("dirtynetwin: connection closed\n"); socket->opened = -1;
        }
    }
    return socket->opened;
}
