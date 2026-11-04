// DirtySDK native X360 socket I/O: ARTIST82B2BD28..82B2C29C.
#include "dirtynet_x360_internal.h"
#include "dirtymem.h"
#include "dirtylib.h"
#include <string.h>

static const s32 SOCKET_MEM_ID = 0x64736f63;
static u32 SocketAddressBytes(const sockaddr* address)
{
    const volatile u8* p = reinterpret_cast<const volatile u8*>(address);
    const u32 a = p[4], c = p[6], d = p[7], b = p[5];
    return (a << 24) | (b << 16) | (c << 8) | d;
}
static u16 SocketPortBytes(const sockaddr* address)
{
    const volatile u8* p = reinterpret_cast<const volatile u8*>(address);
    return static_cast<u16>((u16(p[2]) << 8) | p[3]);
}
static void SocketStoreAddress(sockaddr* address, u32 value)
{
    volatile u8* p = reinterpret_cast<volatile u8*>(address);
    p[7] = static_cast<u8>(value); p[6] = static_cast<u8>(value >> 8);
    p[5] = static_cast<u8>(value >> 16); p[4] = static_cast<u8>(value >> 24);
}
static void SocketStoreTimestamp(sockaddr* address, u32 value)
{
    volatile u8* p = reinterpret_cast<volatile u8*>(address);
    p[11] = static_cast<u8>(value); p[10] = static_cast<u8>(value >> 8);
    p[9] = static_cast<u8>(value >> 16); p[8] = static_cast<u8>(value >> 24);
}

// ARTIST82B2BD28: optional local bind override and virtual-port lookup.
extern "C" s32 SocketBind(SocketT* entry, const sockaddr* address, s32 length)
{
    SocketStateX360* state = gSocketStateX360;
    sockaddr overridden;
    if (!SocketAddressBytes(address) && state->reservedF54)
    {
        memcpy(&overridden, address, 16);
        SocketStoreAddress(&overridden, state->reservedF54);
        address = &overridden;
    }
    if (entry->type == SOCK_DGRAM)
    {
        const u16 port = SocketPortBytes(address);
        if (port)
        {
            s32 index = 0;
            while (index < 32 && state->virtualPorts[index] != port) ++index;
            if (index < 32)
            {
                NetPrintfCode("dirtynetwin: making socket bound to port %d virtual\n", port);
                if (entry->socket != INVALID_SOCKET)
                {
                    shutdown(entry->socket, 2); closesocket(entry->socket);
                    entry->socket = INVALID_SOCKET;
                }
                entry->virtualPort = port; entry->virtualSocket = 1;
                return 0;
            }
        }
    }
    s32 result = SocketTranslateError(bind(entry->socket, address, length));
    if (!result && (entry->type == SOCK_DGRAM || entry->type == 3)) WSASetEvent(state->recvEvent);
    return result;
}

// ARTIST82B2BE88: bind to the local address before remapping/connect.
extern "C" s32 SocketConnect(SocketT* entry, const sockaddr* address, s32 length)
{
    SocketStateX360* state = gSocketStateX360;
    sockaddr local;
    memset(&local, 0, sizeof(local)); local.sa_family = AF_INET;
    entry->opened = 0;
    s32 result = SocketBind(entry, &local, 16);
    if (result < 0 && result != -11) return result;
    sockaddr mapped;
    const sockaddr* destination = SocketMapAddress(&mapped, address);
    result = SocketTranslateError(connect(entry->socket, destination, length));
    if (!result && (entry->type == SOCK_DGRAM || entry->type == 3)) WSASetEvent(state->recvEvent);
    return result;
}

extern "C" s32 SocketListen(SocketT* entry, s32 backlog)
{
    return SocketTranslateError(listen(entry->socket, backlog)); // ARTIST82B2BF38
}

// ARTIST82B2B380: wrap an accepted XDK handle in the native 0x5A4 record.
static SocketT* SocketWrapAccepted(SOCKET nativeSocket, s32 family, s32 type, s32 protocol)
{
    SocketStateX360* state = gSocketStateX360;
    SocketT* entry = static_cast<SocketT*>(DirtyMemAlloc(sizeof(*entry), SOCKET_MEM_ID, state->memGroup));
    memset(entry, 0, sizeof(*entry));
    entry->socket = nativeSocket;
    u_long enabled = 1;
    ioctlsocket(nativeSocket, FIONBIO, &enabled);
    if (type == SOCK_DGRAM && protocol != 254)
        setsockopt(nativeSocket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&enabled), 4);
    entry->family = family; entry->type = type; entry->protocol = protocol;
    entry->overlapped.hEvent = WSACreateEvent();
    NetCritInit(&entry->recvCrit, "recvthread");
    NetCritEnter(0); entry->next = state->sockets; state->sockets = entry; NetCritLeave(0);
    return entry;
}
extern "C" SocketT* SocketAccept(SocketT* entry, sockaddr* address, s32* length)
{
    if (entry->socket == INVALID_SOCKET || (address && static_cast<u32>(*length) < 16)) return 0;
    if (entry->family != AF_INET) return 0;
    SOCKET accepted = accept(entry->socket, address, length);
    return accepted == INVALID_SOCKET ? 0 : SocketWrapAccepted(accepted, entry->family, entry->type, entry->protocol);
}

// ARTIST82B2BFF0: callback intercepts only when it returns a positive count.
extern "C" s32 SocketSendto(SocketT* entry, const char* data, s32 length, s32 flags,
                             const sockaddr* destination, s32 destLength)
{
    (void)flags;
    SocketStateX360* state = gSocketStateX360;
    typedef s32 (*SendCallback)(SocketT*, s32, const u8*, s32, const sockaddr*, void*);
    SendCallback callback = reinterpret_cast<SendCallback>(state->sendCallback);
    if (callback)
    {
        s32 result = callback(entry, entry->type, reinterpret_cast<const u8*>(data), length, destination, state->sendRef);
        if (result > 0) return result;
    }
    if (entry->socket == INVALID_SOCKET) return -11;
    if (entry->type == 3) return 0;
    s32 result;
    if (destination)
    {
        sockaddr mapped;
        const sockaddr* actual = SocketMapAddress(&mapped, destination);
        result = sendto(entry->socket, data, length, 0, actual, destLength);
    }
    else result = send(entry->socket, data, length, 0);
    return SocketTranslateError(result);
}

// ARTIST82B2C0C0, an export hole: stream uses direct XDK receive while
// datagram/raw consume the receive worker's single packet under its lock.
extern "C" s32 SocketRecvfrom(SocketT* entry, char* data, s32 length, s32 flags,
                               sockaddr* from, s32* fromLength)
{
    (void)flags;
    SocketStateX360* state = gSocketStateX360;
    s32 received, error;
    if (entry->type != SOCK_DGRAM && entry->type != 3)
    {
        if (entry->socket == INVALID_SOCKET) return -11;
        if (from)
        {
            received = recvfrom(entry->socket, data, length, 0, from, fromLength);
            if (received > 0) SocketStoreTimestamp(from, NetTick());
        }
        else received = recv(entry->socket, data, length, 0);
        error = WSAGetLastError();
    }
    else
    {
        NetCritEnter(&entry->recvCrit);
        received = entry->recvSize;
        if (received && length > 0)
        {
            if (from) { memcpy(from, &entry->recvFrom, 16); *fromLength = 16; }
            if (received > 0)
            {
                if (received > length) received = length;
                memcpy(data, entry->recvData, received);
                entry->recvError = 0;
            }
            entry->recvSize = 0;
        }
        if (entry->recvError != WSA_IO_PENDING)
        {
            entry->recvError = 0;
            WSASetEvent(state->recvEvent);
        }
        error = entry->recvError;
        NetCritLeave(&entry->recvCrit);
    }
    if (!received) return -1;
    return SocketTranslateErrorCode(received, error);
}