// Native DirtySDK 5.5.3 X360 socket lifecycle and receive worker.
#include "dirtynet_x360_internal.h"
#include "dirtymem.h"
#include "dirtylib.h"
#include <string.h>

extern "C" {
void NetCritKill(NetCritT*);
void NetIdleAdd(void (*)(void*), void*);
void NetIdleDel(void (*)(void*), void*);
void NetIdleDone(void);
void NetLibCreate(s32);
void NetLibDestroy(void);
DirtyLSPRefT* DirtyLSPCreate(void);
void DirtyLSPDestroy(DirtyLSPRefT*);
}
SocketStateX360* gSocketStateX360 = 0;
static const s32 SOCKET_MEM_ID = 0x64736f63;

// ARTIST82B2B5F0: invoke timed callbacks and reclaim deferred closes.
static void SocketIdle(void* context)
{
    SocketStateX360* state = static_cast<SocketStateX360*>(context);
    u32 tick = NetTick();
    NetCritEnter(0);
    for (SocketT* socket = state->sockets; socket; socket = socket->next)
    {
        if (socket->callbackInterval && socket->callback && socket->callbackTick != -1 &&
            tick - static_cast<u32>(socket->callbackTick) > socket->callbackInterval)
        {
            socket->callbackTick = -1;
            socket->callback(socket, 0, socket->callbackRef);
            tick = NetTick();
            socket->callbackTick = tick;
        }
    }
    while (state->killed)
    {
        SocketT* socket = state->killed;
        state->killed = socket->killedNext;
        DirtyMemFree(socket, SOCKET_MEM_ID, state->memGroup);
    }
    NetCritLeave(0);
}

// ARTIST82B2B6B0: notify the receiver after an overlapped completion.
static void SocketRecvComplete(SocketT* socket)
{
    if (socket->type != 1)
    {
        const u32 tick = NetTick();
        u8* bytes = reinterpret_cast<u8*>(&socket->recvFrom) + 8;
        bytes[0] = static_cast<u8>(tick >> 24);
        bytes[1] = static_cast<u8>(tick >> 16);
        bytes[2] = static_cast<u8>(tick >> 8);
        bytes[3] = static_cast<u8>(tick);
    }
    if (socket->callbackTick != -1 && socket->callback && (socket->callbackFlags & 2))
    {
        socket->callbackTick = -1;
        socket->callback(socket, 0, socket->callbackRef);
        socket->callbackTick = NetTick();
    }
}

// ARTIST82B2B750: retain the native asynchronous receive and error state.
static s32 SocketRecvStart(SocketT* socket)
{
    if (socket->recvSize || socket->recvPending == 1 || socket->virtualSocket == 1) return 0;
    WSABUF buffer;
    buffer.len = 1264; buffer.buf = reinterpret_cast<char*>(socket->recvData);
    socket->recvFlags = 0;
    s32 error;
    if (socket->type == SOCK_DGRAM)
    {
        socket->fromLength = 16;
        error = WSARecvFrom(socket->socket, &buffer, 1, reinterpret_cast<DWORD*>(&socket->recvSize),
                            &socket->recvFlags, &socket->recvFrom, &socket->fromLength, &socket->overlapped, 0);
    }
    else
        error = WSARecv(socket->socket, &buffer, 1, reinterpret_cast<DWORD*>(&socket->recvSize),
                        &socket->recvFlags, &socket->overlapped, 0);
    if (error)
    {
        socket->recvSize = error;
        socket->recvError = WSAGetLastError();
        if (socket->recvError == WSA_IO_PENDING) socket->recvPending = 1;
        else NetPrintfCode("dirtynetwin: error %d trying to receive from socket %d\n", socket->recvError, socket->socket);
        return 0;
    }
    const s32 received = socket->recvSize;
    if (received > 0) SocketRecvComplete(socket);
    return received;
}

// ARTIST82B2B860: the event lock serializes wait snapshots with close.
static DWORD WINAPI SocketRecvThread(void*)
{
    SocketStateX360* state = gSocketStateX360;
    NetPrintfCode("dirtynetwin: receive thread started (thid=%d)\n", GetCurrentThreadId());
    state->recvActive = 1;
    HANDLE events[64] = {};
    while (state->recvRun)
    {
        NetCritEnter(0);
        DWORD count = 1;
        events[0] = state->recvEvent;
        for (SocketT* socket = state->sockets; socket && count < 64; socket = socket->next)
        {
            if (socket->type != SOCK_DGRAM && socket->type != 3) continue;
            NetCritEnter(&socket->recvCrit);
            if (socket->recvSize <= 0)
            {
                events[count++] = socket->overlapped.hEvent;
                if (socket->recvPending == 1)
                {
                    if (WSAGetOverlappedResult(socket->socket, &socket->overlapped,
                        reinterpret_cast<DWORD*>(&socket->recvSize), FALSE, &socket->recvFlags))
                    {
                        socket->recvPending = 0;
                        SocketRecvComplete(socket);
                    }
                    else
                    {
                        s32 error = WSAGetLastError();
                        if (error != WSA_IO_INCOMPLETE)
                        {
                            if (error != WSAECONNRESET)
                                NetPrintfCode("dirtynetwin: WSAGetOverlappedResult error %d\n", error);
                            socket->recvPending = 0;
                        }
                    }
                }
                SocketRecvStart(socket);
            }
            NetCritLeave(&socket->recvCrit);
        }
        NetCritEnter(&state->recvEventCrit);
        NetCritLeave(0);
        DWORD selected = WSAWaitForMultipleEvents(count, events, FALSE, WSA_INFINITE, FALSE);
        NetCritLeave(&state->recvEventCrit);
        if (selected < count)
        {
            NetCritEnter(0);
            bool present = false;
            for (SocketT* socket = state->sockets; socket; socket = socket->next)
                if (socket->overlapped.hEvent == events[selected]) { present = true; break; }
            if (selected == 0 || present) WSAResetEvent(events[selected]);
            NetCritLeave(0);
        }
    }
    NetPrintfCode("dirtynetwin: receive thread exit\n");
    state->recvActive = 0;
    return 0;
}

// ARTIST82B2B4C8: unlink under the global lock, wake the waiter, then
// defer destruction until the idle pass has stopped using the old list.
static s32 SocketCloseNative(SocketT* socket, s32 closeSocket)
{
    SocketStateX360* state = gSocketStateX360;
    NetCritEnter(0);
    SocketT** link = &state->sockets;
    while (*link && *link != socket) link = &(*link)->next;
    bool found = *link != 0;
    if (found) *link = socket->next;
    NetCritLeave(0);
    if (!found)
    {
        NetPrintfCode("dirtynetwin: warning, trying to close socket 0x%08x that is not in the socket list\n", socket);
        return -1;
    }
    NetIdleDone();
    NetCritEnter(0);
    WSASetEvent(state->recvEvent);
    NetCritEnter(&state->recvEventCrit);
    NetCritKill(&socket->recvCrit);
    WSACloseEvent(socket->overlapped.hEvent);
    socket->overlapped.hEvent = 0;
    NetCritLeave(&state->recvEventCrit);
    NetCritLeave(0);
    if (closeSocket == 1 && socket->socket != INVALID_SOCKET)
    {
        shutdown(socket->socket, 2);
        closesocket(socket->socket);
    }
    socket->opened = 0;
    socket->socket = INVALID_SOCKET;
    NetCritEnter(0);
    socket->killedNext = state->killed;
    state->killed = socket;
    NetCritLeave(0);
    return 0;
}

// ARTIST82B2BBF0 and 82B2BD20: create/link a real native socket and
// expose the close operation used by the same worker-owned list.
extern "C" SocketT* SocketOpen(s32 family, s32 type, s32 protocol)
{
    SocketStateX360* state = gSocketStateX360;
    SOCKET nativeSocket = socket(family, type, protocol);
    if (nativeSocket == INVALID_SOCKET)
    {
        NetPrintfCode("dirtynetwin: error %d creating socket\n", WSAGetLastError());
        return 0;
    }
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
    NetCritEnter(0);
    entry->next = state->sockets;
    state->sockets = entry;
    NetCritLeave(0);
    return entry;
}

extern "C" s32 SocketClose(SocketT* socket)
{
    return SocketCloseNative(socket, 1);
}

extern "C" s32 SocketCreate(s32 priority)
{
    SocketStateX360* previous = gSocketStateX360;
    s32 group = DirtyMemGroupQuery();
    if (previous)
    {
        return NetPrintfCode("dirtynetwin: SocketCreate() called while module is already active\n");
    }
    NetPrintfCode("dirtynetwin: DirtySock SDK v%d.%d.%d.%d\n", 5, 5, 3, 0);
    SocketStateX360* state = static_cast<SocketStateX360*>(DirtyMemAlloc(sizeof(*state), SOCKET_MEM_ID, group));
    if (!state)
    {
        return NetPrintfCode("dirtynetpc: unable to allocate module state\n");
    }
    memset(state, 0, sizeof(*state));
    gSocketStateX360 = state;
    state->memGroup = group;
    NetLibCreate(priority);
    NetIdleAdd(SocketIdle, state);
    WSADATA data;
    memset(&data, 0, sizeof(data));
    WSAStartup(0x202, &data);
    state->wsaVersion = (data.wVersion >> 8) | ((data.wVersion & 255) << 8);
    state->recvEvent = CreateEventA(0, FALSE, FALSE, 0);
    NetCritInit(&state->recvEventCrit, "recv-event-crit");
    state->recvRun = reinterpret_cast<HANDLE>(1);
    DWORD threadId;
    state->recvRun = CreateThread(0, 0, SocketRecvThread, 0, 0, &threadId);
    if (state->recvRun)
    {
        SetThreadPriority(state->recvRun, priority);
        CloseHandle(state->recvRun);
    }
    else NetPrintfCode("dirtynetwin: error %d creating socket receive thread\n", state->recvRun);
    state->lsp = DirtyLSPCreate();
    state->nextAddress = 0x01000000;
    return reinterpret_cast<s32>(state->lsp);
}

extern "C" s32 SocketDestroy(s32 flags)
{
    SocketStateX360* state = gSocketStateX360;
    if (!state)
    {
        return NetPrintfCode("dirtynetpc: SocketDestroy() called while module is not active\n");
    }
    NetPrintfCode("dirtynetpc: shutting down\n");
    NetIdleDel(SocketIdle, state);
    NetIdleDone();
    DirtyLSPDestroy(state->lsp);
    state->recvRun = 0;
    WSASetEvent(state->recvEvent);
    while (state->recvActive > 0) Sleep(1);
    while (state->sockets) SocketCloseNative(state->sockets, 1);
    SocketIdle(state);
    NetCritKill(&state->recvEventCrit);
    CloseHandle(state->recvEvent);
    DirtyMemFree(state, SOCKET_MEM_ID, state->memGroup);
    gSocketStateX360 = 0;
    NetLibDestroy();
    return flags ? 0 : WSACleanup();
}