#ifndef DIRTYSDK_X360_VOIP_X360_INTERNAL_H
#define DIRTYSDK_X360_VOIP_X360_INTERNAL_H

// Private X360 voice types shared by the voice owner (voip_x360.cpp) and the voice-connection
// module (voipconnection_x360.cpp), which the original builds as separate modules (DWARF
// voip/voipconnection.c; ARTIST keeps them apart: Voip* at 0x82B28688.., VoipConnection* at
// 0x82B39908..). The memory-group accessors live in voippriv_x360.c.

#include "voip.h"
#include "dirtylib.h"
#include "dirtymem.h"
#include "netconn.h"
#include "dirtylib_x360_internal.h"
#include <xtl.h>
#include <winsockx.h>
#include <xbox.h>
#include <xaudio.h>
#include <xonline.h>
#include <xhv.h>
#include <cstddef>
#include <cstring>

struct SocketT;
struct VoipPacketT;

struct VoipUserT
{
    u8 data[20];
};

typedef void VoipConnectRecvVoiceCbT(VoipUserT* pRemoteUser,
                                      VoipPacketT* pPacket,
                                      void* pUserData);
typedef void VoipConnectRegUserCbT(VoipUserT* pRemoteUser,
                                   void* pUserData,
                                   u32 bRegister);
typedef void VoipCallbackT(VoipRefT* pVoip, s32 eEvent, s32 iValue, void* pUserData);
typedef s32 VoipProcessingModeFactoryT(s32, s32, s8, s32);

struct VoipConnectionT
{
    SOCKADDR_IN SendAddr;                            // +0x00
    u64 aRemoteXuids[4];                             // +0x10
    s32 iLocalUserIndex;                             // +0x30
    u32 uClientId;                                   // +0x34
    u32 uSessionId;                                  // +0x38
    u32 uRemoteStatus;                               // +0x3c
    // DWARF voipconnection.h:54: an anonymous enum (signed: ARTIST 0x82B39A70 cmpwi).
    enum { ST_DISC = 0, ST_CONN = 1, ST_ACTV = 2 } eState; // +0x40
    u8 aPacketHeader[0x10];                          // +0x44
    u8 uNumDataPackets;                              // +0x54
    u8 aVoicePayload[0xeb];                          // +0x55
    u32 bBroadband;                                  // +0x140
    u32 uLastVoiceSend;                              // +0x144
    u32 uLastSend;                                   // +0x148
    u32 uLastRecv;                                   // +0x14c
    u32 uSendSequence;                               // +0x150
    s32 iChangePort;                                 // +0x154
    s32 iChangeAddr;                                 // +0x158
    s32 iRecvPackets;                                // +0x15c
    s32 iRecvData;                                   // +0x160
    s32 iLastSendResult;                             // +0x164
    u32 bMuted;                                      // +0x168
    u32 uReserved16C;                                // +0x16c
};

struct VoipConnectionListT
{
    SocketT* pSocket;                                 // +0x00
    NetCritT NetCrit;                                 // +0x04
    VoipConnectRecvVoiceCbT* pVoiceCb;                // +0x2c
    VoipConnectRegUserCbT* pRegUserCb;                // +0x30
    void* pCbUserData;                                // +0x34
    s32 iMaxConnections;                              // +0x38
    VoipConnectionT* pConnections;                    // +0x3c
    u32 uSendMask;                                    // +0x40
    u32 uRecvMask;                                    // +0x44
    u32 uRecvVoice;                                   // +0x48 / VoipIdle's status source
    u32 uLocalStatus;                                 // +0x4c / VoipLocal
    u32 uLastVoiceData;                               // +0x50
    u32 uLocalUserPad;                                // +0x54
    u64 aLocalXuids[4];                              // +0x58
    s32 iLocalUserIndex;                              // +0x78
    u32 uClientId;                                    // +0x7c
    s32 uBindPort;                                    // +0x80
    s32 iDataTimeout;                                 // +0x84
    u8 bServer;                                       // +0x88
    u8 _opaque89[7];
    VoipCallbackT* pCallback;                         // +0x90 / VoipRef +0x98
    void* pCallbackUserData;                          // +0x94 / VoipRef +0x9c
    u32 uLastRecvVoice;                              // +0x98 / VoipRef +0xa0
    u32 uLastModeState;                              // +0x9c / VoipRef +0xa4
    u32 uLastLocalStatus;                            // +0xa0 / VoipRef +0xa8
    DWORD dwThreadId;                                 // +0xa4 / VoipRef +0xac
    RTL_CRITICAL_SECTION ThreadCrit;                  // +0xa8 / VoipRef +0xb0
    volatile LONG iThreadActive;                      // +0xc4 / VoipRef +0xcc
    HANDLE hThread;                                   // +0xc8 / VoipRef +0xd0
    u32 uUserSendMask;                                // +0xcc / VoipRef +0xd4
    u32 uUserRecvMask;                                // +0xd0 / VoipRef +0xd8
    u32 uFriendsOnlyMask;                             // +0xd4 / VoipRef +0xdc
    s32 iLocalTalker;                                 // +0xd8 / VoipRef +0xe0
    s32 iLockPort;                                    // +0xdc / VoipRef +0xe4
    void* pVoiceChatMode;                             // +0xe0 / VoipRef +0xe8
    void* pStartedLocalMode;                          // +0xe4 / VoipRef +0xec
    u32 uHeadsetMask;                                 // +0xe8 / VoipRef +0xf0
    u32 bVoiceAllowed;                                // +0xec / VoipRef +0xf4
    u32 bFriendsOnly;                                 // +0xf0 / VoipRef +0xf8
    u32 bMuteListDirty;                               // +0xf4 / VoipRef +0xfc
    s32 iLastNetConnStatus;                           // +0xf8 / VoipRef +0x100
    u32 uReservedFC;                                  // +0xfc / VoipRef +0x104
};

struct VoipRefT
{
    IXHVEngine* pXhvEngine;                           // +0x00
    HANDLE hNotifyListener;                           // +0x04
    VoipConnectionListT Connectionlist;               // +0x08
};

CGS_STATIC_ASSERT((sizeof(VoipUserT) == 0x14), "X360 VoipUserT size");
CGS_STATIC_ASSERT((sizeof(VoipConnectionT) == 0x170), "X360 VoipConnectionT stride");
CGS_STATIC_ASSERT((sizeof(SOCKADDR_IN) == 0x10), "X360 IPv4 socket address size");
CGS_STATIC_ASSERT((offsetof(VoipConnectionT, aRemoteXuids) == 0x10), "X360 remote XUID array offset");
CGS_STATIC_ASSERT((offsetof(VoipConnectionT, uRemoteStatus) == 0x3c), "X360 remote status offset");
CGS_STATIC_ASSERT((offsetof(VoipConnectionT, eState) == 0x40), "X360 connection state offset");
CGS_STATIC_ASSERT((offsetof(VoipConnectionT, uNumDataPackets) == 0x54), "X360 packet count offset");
CGS_STATIC_ASSERT((offsetof(VoipConnectionT, bBroadband) == 0x140), "X360 broadband flag offset");
CGS_STATIC_ASSERT((offsetof(VoipConnectionT, iChangePort) == 0x154), "X360 port-change count offset");
CGS_STATIC_ASSERT((offsetof(VoipConnectionT, iChangeAddr) == 0x158), "X360 address-change count offset");
CGS_STATIC_ASSERT((offsetof(VoipConnectionT, iRecvPackets) == 0x15c), "X360 received-packet count offset");
CGS_STATIC_ASSERT((offsetof(VoipConnectionT, iRecvData) == 0x160), "X360 received-byte count offset");
CGS_STATIC_ASSERT((offsetof(VoipConnectionT, iLastSendResult) == 0x164), "X360 send result offset");
CGS_STATIC_ASSERT((offsetof(VoipConnectionT, bMuted) == 0x168), "X360 mute flag offset");
CGS_STATIC_ASSERT((offsetof(VoipConnectionListT, iMaxConnections) == 0x38), "X360 connection count offset");
CGS_STATIC_ASSERT((offsetof(VoipConnectionListT, pConnections) == 0x3c), "X360 connection array offset");
CGS_STATIC_ASSERT((offsetof(VoipConnectionListT, uLocalStatus) == 0x4c), "X360 local status offset");
CGS_STATIC_ASSERT((offsetof(VoipConnectionListT, aLocalXuids) == 0x58), "X360 local XUID array offset");
CGS_STATIC_ASSERT((offsetof(VoipConnectionListT, iLocalUserIndex) == 0x78), "X360 local user selector offset");
CGS_STATIC_ASSERT((offsetof(VoipConnectionListT, uBindPort) == 0x80), "X360 bind port offset");
CGS_STATIC_ASSERT((offsetof(VoipConnectionListT, ThreadCrit) == 0xa8), "X360 VoIP worker critical section offset");
CGS_STATIC_ASSERT((offsetof(VoipConnectionListT, iThreadActive) == 0xc4), "X360 worker state offset");
CGS_STATIC_ASSERT((offsetof(VoipConnectionListT, iLocalTalker) == 0xd8), "X360 local talker offset");
CGS_STATIC_ASSERT((offsetof(VoipRefT, Connectionlist) == 0x08), "X360 connection list offset");
CGS_STATIC_ASSERT((sizeof(VoipRefT) == 0x108), "X360 VoipRefT allocation size");

extern "C" s32 VoipConnectionStartup(VoipConnectionListT*, VoipUserT*, s32);
extern "C" void VoipConnectionShutdown(VoipConnectionListT*);
extern "C" void VoipConnectionSetCallbacks(VoipConnectionListT*,
                                            VoipConnectRecvVoiceCbT*,
                                            VoipConnectRegUserCbT*, void*);
extern "C" s32 VoipConnectionAllocate(VoipConnectionListT*);
extern "C" void VoipConnectionStop(VoipConnectionListT*, s32);
extern "C" s32 VoipConnectionStart(VoipConnectionListT*, s32, VoipUserT*,
                                     u32, u32, u32, u32, u32);
extern "C" s32 VoipConnectionFlush(VoipConnectionListT*, s32);
extern "C" void VoipConnectionRemove(VoipConnectionListT*, s32);
extern "C" u32 VoipConnectionRemoveConnFromMask(u32, s32);
extern "C" s32 VoipConnectionSetRecvMask(VoipConnectionListT*, u32);
extern "C" void VoipConnectionSetSendMask(VoipConnectionListT*, u32);
extern "C" s32 VoipConnectionSend(VoipConnectionListT*, const void*, s32);
extern "C" void VoipConnectionUpdate(VoipConnectionListT*);

typedef s32 VoipSocketCallbackT(SocketT*, s32, void*);
extern "C" SocketT* SocketOpen(s32, s32, s32);
extern "C" s32 SocketBind(SocketT*, const void*, s32);
extern "C" s32 SocketInfo(SocketT*, s32, s32, void*, s32);
extern "C" s32 SocketCallback(SocketT*, s32, s32, void*, VoipSocketCallbackT*);
extern "C" s32 SocketSendto(SocketT*, const void*, s32, s32, const void*, s32);
extern "C" s32 SocketRecvfrom(SocketT*, void*, s32, s32, void*, s32*);
extern "C" s32 SocketClose(SocketT*);
extern "C" u16 SocketHtons(u16);
extern "C" u32 SocketHtonl(u32);
extern "C" u16 SocketNtohs(u16);
extern "C" u32 SocketNtohl(u32);
extern "C" const char* SocketInAddrGetText(u32);
extern "C" const char* DirtyErrGetName(s32);

// Receive/dispatch at ARTIST 0x82B3A360 is defined with its protocol helpers below.
extern "C" s32 VoipConnectionReceivePackets(VoipConnectionListT*);
extern "C" s32 VoipConnectionSendQueuedPacket(VoipConnectionListT*,
                                               VoipConnectionT*, u32, s32);

// The voice module's memory group (voippriv_x360.c).
extern "C" s32 VoipMemGroupQuery(void);
extern "C" void VoipMemGroupSet(s32 iMemGroup);

static const s32 kVoipMemModule = 0x766f6970;

// A connection's user record: the 8-byte slot after the remote XUIDs that iLocalUserIndex
// selects (an address computation both modules make).
inline VoipUserT* VoipUserForConnection(VoipConnectionT* pConnection)
{
    const u32 userSlot = static_cast<u32>(pConnection->iLocalUserIndex + 2);
    return reinterpret_cast<VoipUserT*>(
        reinterpret_cast<u8*>(pConnection) + userSlot * 8);
}

#endif // DIRTYSDK_X360_VOIP_X360_INTERNAL_H
