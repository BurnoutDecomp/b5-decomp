#pragma once
#include "types.hpp"
#include <xtl.h>
#include "dirtylib_x360_internal.h"
#include <stddef.h>

struct SocketT
{
    SocketT* next; SocketT* killedNext;
    s32 family, type, protocol;
    s8 opened; u8 pad15; u8 virtualSocket; u8 pad17;
    SOCKET socket;
    u8 pad1c[32]; u16 virtualPort; u8 pad3e[2];
    u32 callbackFlags; s32 callbackTick; u32 callbackInterval;
    void* callbackRef; s32 (*callback)(SocketT*, s32, void*);
    WSAOVERLAPPED overlapped;
    NetCritT recvCrit;
    s32 recvError; s32 fromLength; DWORD recvFlags;
    u8 recvPending; u8 pad9d;
    sockaddr recvFrom; u8 padAE[2];
    s32 recvSize; u8 recvData[1264];
};
struct SocketAddrMapX360 { u32 address, mask, remap; u16 port, remapPort; };
struct SocketNameMapX360 { char match[64], remapName[32]; u32 remapAddress; u16 port, remapPort; };
CGS_STATIC_ASSERT((sizeof(SocketAddrMapX360) == 16), "ARTIST address map");
CGS_STATIC_ASSERT((sizeof(SocketNameMapX360) == 104), "ARTIST name map");
struct DirtyLSPRefT;
struct SocketStateX360
{
    SocketT* sockets; SocketT* killed;
    SocketAddrMapX360 addressMap[32]; SocketNameMapX360 nameMap[32]; u16 virtualPorts[32];
    u32 dnsRemap; s32 memGroup; s32 wsaVersion; u32 reservedF54;
    DirtyLSPRefT* lsp; u32 nextAddress;
    HANDLE volatile recvRun; volatile s32 recvActive; HANDLE recvEvent;
    NetCritT recvEventCrit; void* sendCallback; void* sendRef;
};
CGS_STATIC_ASSERT((offsetof(SocketT, overlapped) == 84), "socket overlapped");
CGS_STATIC_ASSERT((offsetof(SocketT, recvCrit) == 104), "socket recv lock");
CGS_STATIC_ASSERT((offsetof(SocketT, recvSize) == 176), "socket recv size");
CGS_STATIC_ASSERT((offsetof(SocketT, recvFrom) == 158), "socket source");
CGS_STATIC_ASSERT((offsetof(SocketT, recvData) == 180), "socket recv data");
CGS_STATIC_ASSERT((sizeof(SocketT) == 1444), "ARTIST socket size");
CGS_STATIC_ASSERT((offsetof(SocketStateX360, memGroup) == 3916), "socket group");
CGS_STATIC_ASSERT((offsetof(SocketStateX360, recvEventCrit) == 3948), "socket event lock");
CGS_STATIC_ASSERT((sizeof(SocketStateX360) == 3996), "ARTIST module size");
extern SocketStateX360* gSocketStateX360;
// Shared ARTIST 82B2B150 / 82B2B2D8 native socket helpers.
const sockaddr* SocketMapAddress(sockaddr* output, const sockaddr* input);
s32 SocketTranslateError(s32 value);
s32 SocketTranslateErrorCode(s32 value, s32 error);
