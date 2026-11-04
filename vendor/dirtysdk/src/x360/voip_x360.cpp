// DirtySDK 5.5.3 X360 VoIP owner and public selectors; ARTIST 0x82B28FE0,
// 0x82B28FF0, 0x82B290F0, 0x82B291F0, 0x82B29378-0x82B294C8,
// and 0x82B29638. The connection state follows its X360 call edges; the two
// XHV processing-mode constructors remain external providers.
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
    u32 eState;                                      // +0x40
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
extern "C" s32 VoipConnectionStop(VoipConnectionListT*, s32);
extern "C" s32 VoipConnectionStart(VoipConnectionListT*, s32, VoipUserT*,
                                     u32, u32, u32, u32, u32);
extern "C" s32 VoipConnectionFlush(VoipConnectionListT*, s32);
extern "C" void VoipConnectionRemove(VoipConnectionListT*, s32);
extern "C" u32 VoipConnectionRemoveConnFromMask(u32, s32);
extern "C" s32 VoipConnectionSetRecvMask(VoipConnectionListT*, u32);
extern "C" s32 VoipConnectionSetSendMask(VoipConnectionListT*, u32);
extern "C" s32 VoipConnectionSend(VoipConnectionListT*, const void*, s32);
extern "C" s32 VoipConnectionUpdate(VoipConnectionListT*);

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

// X360 ARTIST callees whose bodies are in the private worker / XHV mode lane.
extern "C" void VoipWorkerThread(void* pVoip);                 // sub_82B28E80
extern "C" void VoipIncomingVoice(VoipUserT*, VoipPacketT*, void*); // sub_82B28A28
extern "C" void VoipRegisterRemoteTalker(VoipUserT*, void*, u32);    // sub_82B28A98
extern "C" void VoipSetRemoteTalkerProcessingModes(VoipRefT*, s32);  // sub_82B28B48
extern "C" s32 VoipBindLocalTalker(VoipRefT*, u32);                  // sub_82B28BC0
extern "C" void VoipUpdatePrivileges(VoipRefT*);                    // sub_82B28858
extern "C" void VoipUpdateMuteList(VoipRefT*);                      // sub_82B28798
extern "C" s32 VoipUpdateVoiceActivity(VoipRefT*);                    // sub_82B28688

namespace
{
const s32 kVoipMemModule = 0x766f6970;
VoipRefT* gVoipRef;
s32 gVoipMemGroup;
PFNMICRAWDATAREADY gVoipMicRawDataReady;
const u8 gVoipDisconnectHeader[10] = {
    0x00, 0x08, 0x44, 0x53, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00
};
const u8 gVoipConnectionVoiceHeader[10] = {
    0x00, 0x08, 0x4d, 0x49, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00
};
const u8 gVoipConnectionStartHeader[10] = {
    0x00, 0x5e, 0x43, 0x4f, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00
};

VoipUserT* VoipUserForConnection(VoipConnectionT* pConnection)
{
    const u32 userSlot = static_cast<u32>(pConnection->iLocalUserIndex + 2);
    return reinterpret_cast<VoipUserT*>(
        reinterpret_cast<u8*>(pConnection) + userSlot * 8);
}

void VoipStoreBigEndian32(u8* pBytes, u32 value)
{
    pBytes[0] = static_cast<u8>(value >> 24);
    pBytes[1] = static_cast<u8>(value >> 16);
    pBytes[2] = static_cast<u8>(value >> 8);
    pBytes[3] = static_cast<u8>(value);
}

void VoipConnectionReleaseLock(NetCritT* pCrit)
{
    // NetCritKill at ARTIST 0x82B29A90 clears only this X360 spin-lock word.
    pCrit->lock = 0;
}

s32 VoipConnectionStopRecord(VoipConnectionListT* pList,
                             VoipConnectionT* pConnection, s32 iConnID)
{
    s32 result = 0;
    NetCritEnter(&pList->NetCrit);
    if (pConnection->eState != 0)
    {
        const u16 remotePort = SocketNtohs(pConnection->SendAddr.sin_port);
        const u32 remoteAddress = SocketNtohl(pConnection->SendAddr.sin_addr.s_addr);
        NetPrintfCode("voipconnection: [%d] disconnecting from %s:%d\n",
                      iConnID, SocketInAddrGetText(remoteAddress), remotePort);

        // ARTIST82B39530..60: the disconnect sender logs before building or sending.
        const u16 disconnectPort = SocketNtohs(pConnection->SendAddr.sin_port);
        const u32 disconnectAddress = SocketNtohl(pConnection->SendAddr.sin_addr.s_addr);
        NetPrintfCode("voipconnection: sending disconnect packet to %s:%d\n",
                      SocketInAddrGetText(disconnectAddress), disconnectPort);
        u8 packet[18];
        std::memcpy(packet, gVoipDisconnectHeader, sizeof(gVoipDisconnectHeader));
        VoipStoreBigEndian32(packet + 6, pList->uClientId);
        VoipStoreBigEndian32(packet + 10, pConnection->uClientId);
        VoipStoreBigEndian32(packet + 14, pConnection->uSessionId);
        if (pList->pSocket)
            SocketSendto(pList->pSocket, packet, sizeof(packet), 0,
                         &pConnection->SendAddr, sizeof(pConnection->SendAddr));

        pConnection->eState = 0;
        pList->pRegUserCb(VoipUserForConnection(pConnection), pList->pCbUserData, 0);
        std::memset(pConnection, 0, sizeof(*pConnection));
        pConnection->uRemoteStatus = 0x80;
        pList->uLocalStatus &= ~0x100u;
    }
    NetCritLeave(&pList->NetCrit);
    result = 0;
    return result;
}

s32 VoipConnectionSocketCallback(SocketT*, s32, void* pData)
{
    VoipConnectionListT* pList = static_cast<VoipConnectionListT*>(pData);
    if (NetCritTry(&pList->NetCrit))
    {
        VoipConnectionReceivePackets(pList);
        NetCritLeave(&pList->NetCrit);
    }
    else
        NetPrintfCode("voipconnection: could not acquire connectionlist critical section\n");
    return 0;
}

// The ARTIST target at 0x82AD5078 is the actual one-instruction no-op used by
// VoipStartup as its initial event callback.
void VoipDefaultEventCallback(VoipRefT*, s32, s32, void*)
{
}

u64 VoipDecodeXuid(const char* pUserID)
{
    u64 xuid = 0;
    for (s32 i = 0; i < 16; ++i)
    {
        const s32 character = static_cast<s8>(pUserID[i + 1]);
        s32 base = 0x57;
        if (character >= '0' && character < ':')
            base = '0';
        const s64 digit = static_cast<s8>(character - base);
        xuid = (xuid << 4) | static_cast<u64>(digit);
    }
    return xuid;
}
}

extern "C" s32 VoipMemGroupQuery(void)
{
    return gVoipMemGroup;
}

extern "C" s32 VoipMemGroupSet(s32 iMemGroup)
{
    gVoipMemGroup = iMemGroup;
    return iMemGroup;
}

extern "C" s32 VoipConnectionStartup(VoipConnectionListT* pList,
                                      VoipUserT* pLocalUser,
                                      s32 iMaxPeers)
{
    const u32 allocationSize = 368u * static_cast<u32>(iMaxPeers);
    pList->pConnections = static_cast<VoipConnectionT*>(
        DirtyMemAlloc(static_cast<s32>(allocationSize), kVoipMemModule,
                      VoipMemGroupQuery()));
    if (!pList->pConnections)
    {
        NetPrintfCode("voipconnection: error, unable to allocate connectionlist\n");
        return -1;
    }

    std::memset(pList->pConnections, 0, allocationSize);
    std::memcpy(&pList->aLocalXuids[0], pLocalUser->data, sizeof(u64));
    NetCritInit(&pList->NetCrit, "voipconnection");
    pList->iMaxConnections = iMaxPeers;
    pList->iDataTimeout = 15000;
    // Match ARTIST slw at 0x82B399B8: PPC shifts by the low six bits and
    // returns zero for counts 32..63 before the native subtract-one step.
    const u32 maskShift = static_cast<u32>(iMaxPeers) & 63u;
    const u32 shiftedOne = maskShift < 32 ? (1u << maskShift) : 0;
    const u32 connectionMask = shiftedOne - 1;
    pList->uSendMask = connectionMask;
    pList->uRecvMask = connectionMask;
    return 0;
}

extern "C" void VoipConnectionShutdown(VoipConnectionListT* pList)
{
    DirtyMemFree(pList->pConnections, kVoipMemModule, VoipMemGroupQuery());
    if (pList->pSocket)
        SocketClose(pList->pSocket);
    VoipConnectionReleaseLock(&pList->NetCrit);
    std::memset(pList, 0, 0x90);
}

extern "C" void VoipConnectionSetCallbacks(VoipConnectionListT* pList,
                                            VoipConnectRecvVoiceCbT* pVoiceCb,
                                            VoipConnectRegUserCbT* pRegUserCb,
                                            void* pUserData)
{
    pList->pVoiceCb = pVoiceCb;
    pList->pRegUserCb = pRegUserCb;
    pList->pCbUserData = pUserData;
}

extern "C" s32 VoipConnectionAllocate(VoipConnectionListT* pList)
{
    for (s32 iConnID = 0; iConnID < pList->iMaxConnections; ++iConnID)
    {
        if (pList->pConnections[iConnID].eState == 0)
            return iConnID;
    }
    NetPrintfCode("voipconnection: error, could not allocate a connection\n");
    return -1;
}

extern "C" u32 VoipConnectionRemoveConnFromMask(u32 uMask, s32 iConnID)
{
    // Match ARTIST slw at 0x82B39910, including PPC's zero result at shifts 32..63.
    const u32 shift = static_cast<u32>(iConnID) & 63u;
    const u32 shiftedOne = shift < 32 ? (1u << shift) : 0;
    const u32 lowerMask = shiftedOne - 1;
    return ((uMask >> 1) & ~lowerMask) | (uMask & lowerMask);
}

extern "C" s32 VoipConnectionSetSendMask(VoipConnectionListT* pList,
                                          u32 uSendMask)
{
    if (pList->uSendMask != uSendMask)
    {
        NetPrintfCode("voipconnection: updating send mask to 0x%08x\n", uSendMask);
        pList->uSendMask = uSendMask;
    }
    return 0;
}

extern "C" s32 VoipConnectionSetRecvMask(VoipConnectionListT* pList,
                                          u32 uRecvMask)
{
    if (pList->uRecvMask == uRecvMask)
        return 0;

    for (s32 iConnID = 0; iConnID < pList->iMaxConnections; ++iConnID)
    {
        const u32 bit = 1u << (static_cast<u32>(iConnID) & 31);
        if (!((pList->uRecvMask ^ uRecvMask) & bit))
            continue;

        VoipConnectionT* pConnection = &pList->pConnections[iConnID];
        const bool bRegister = (uRecvMask & bit) != 0;
        if (pConnection->eState == 1 && bRegister)
        {
            NetPrintfCode("voipconnection: [%d] setrecvmask skip register during connect\n",
                          iConnID);
            continue;
        }
        if (pConnection->eState == 0 && bRegister)
        {
            uRecvMask &= ~bit;
            continue;
        }
        pList->pRegUserCb(VoipUserForConnection(pConnection), pList->pCbUserData,
                          bRegister ? 1u : 0u);
    }

    NetPrintfCode("voipconnection: updating recv mask to 0x%08x\n", uRecvMask);
    pList->uRecvMask = uRecvMask;
    return 0;
}

extern "C" s32 VoipConnectionStart(VoipConnectionListT* pList, s32 iConnID,
                                    VoipUserT* pRemoteUser, u32 uAddress,
                                    u32 uConnPort, u32 uBindPort,
                                    u32 uClientId, u32 uSessionId)
{
    if (!pList->pSocket)
    {
        pList->pSocket = SocketOpen(AF_INET, SOCK_DGRAM, 254);
        if (!pList->pSocket)
        {
            NetPrintfCode("voipconnection: error creating socket\n");
            return -1;
        }

        SOCKADDR_IN bindAddress;
        std::memset(&bindAddress, 0, sizeof(bindAddress));
        bindAddress.sin_family = AF_INET;
        bindAddress.sin_port = static_cast<u16>(uBindPort);
        s32 bindResult = SocketBind(pList->pSocket, &bindAddress,
                                    sizeof(bindAddress));
        if (bindResult != 0)
        {
            NetPrintfCode("voipconnection: error %d binding socket to port %d, trying random\n",
                          bindResult, uBindPort);
            bindAddress.sin_port = 0;
            bindResult = SocketBind(pList->pSocket, &bindAddress,
                                    sizeof(bindAddress));
            if (bindResult != 0)
            {
                NetPrintfCode("voipconnection: error %d binding socket\n", bindResult);
                SocketClose(pList->pSocket);
                pList->pSocket = 0;
                return -1;
            }
        }
        SocketInfo(pList->pSocket, 0x62696e64, 0, &bindAddress,
                   sizeof(bindAddress));
        uBindPort = SocketNtohs(bindAddress.sin_port);
        NetPrintfCode("voipconnection: bound socket to port %d\n", uBindPort);
        pList->uBindPort = static_cast<s32>(uBindPort);
        SocketCallback(pList->pSocket, 2, 5000, pList,
                       VoipConnectionSocketCallback);
    }

    if (uBindPort != static_cast<u32>(pList->uBindPort))
    {
        NetPrintfCode("voipconnection: error, only one global bind port is currently supported\n");
        return -2;
    }

    VoipConnectionT* pConnection = &pList->pConnections[iConnID];
    std::memset(pConnection, 0, sizeof(*pConnection));
    pConnection->eState = 1;
    pConnection->SendAddr.sin_family = AF_INET;
    pConnection->SendAddr.sin_port = SocketHtons(static_cast<u16>(uConnPort));
    pConnection->SendAddr.sin_addr.s_addr = SocketHtonl(uAddress);
    pConnection->uClientId = uClientId;
    pConnection->uSessionId = uSessionId;
    pConnection->uRemoteStatus = 0;
    pConnection->uLastRecv = NetTick();
    if (pRemoteUser)
        std::memcpy(&pConnection->aRemoteXuids[0], pRemoteUser->data,
                    sizeof(u64));
    std::memcpy(pConnection->aPacketHeader, gVoipConnectionVoiceHeader,
                sizeof(gVoipConnectionVoiceHeader));

    const u32 bit = 1u << (static_cast<u32>(iConnID) & 31);
    pList->uSendMask |= bit;
    pList->uRecvMask |= bit;
    const u64 xuid = pConnection->aRemoteXuids[0];
    NetPrintfCode("voipconnection: [%d] connecting to user=0x%08x%08x %s:%d clientId=0x%08x\n",
                  iConnID, static_cast<u32>(xuid >> 32), static_cast<u32>(xuid),
                  SocketInAddrGetText(SocketNtohl(pConnection->SendAddr.sin_addr.s_addr)),
                  SocketNtohs(pConnection->SendAddr.sin_port), uClientId);
    return 0;
}

extern "C" s32 VoipConnectionStop(VoipConnectionListT* pList, s32 iConnID)
{
    s32 result = 0;
    if (iConnID == -2)
    {
        for (s32 index = 0; index < pList->iMaxConnections; ++index)
            result = VoipConnectionStopRecord(pList, &pList->pConnections[index], index);
    }
    else if (iConnID < 0 || iConnID >= pList->iMaxConnections)
        return NetPrintfCode("voipconnection: disconnect with iConnID=%d is invalid\n",
                             iConnID);
    else
        result = VoipConnectionStopRecord(pList, &pList->pConnections[iConnID], iConnID);
    return result;
}

extern "C" void VoipConnectionRemove(VoipConnectionListT* pList, s32 iConnID)
{
    if (iConnID < 0 || iConnID >= pList->iMaxConnections)
    {
        NetPrintfCode("voipconnection: removal with iConnID=%d is invalid\n", iConnID);
        return;
    }
    if (iConnID == pList->iMaxConnections - 1)
        return;

    VoipConnectionT* pConnection = &pList->pConnections[iConnID];
    const s32 iConnectionsToMove = pList->iMaxConnections - iConnID - 1;
    const u32 bit = 1u << (static_cast<u32>(iConnID) & 31);
    VoipConnectionSetRecvMask(pList, pList->uRecvMask & ~bit);
    NetPrintfCode("voipconnection: removing connId %d from list\n", iConnID);
    std::memmove(pConnection, pConnection + 1,
                 sizeof(VoipConnectionT) * static_cast<usize>(iConnectionsToMove));
    NetPrintfCode("voipconnection: flags pre-update: send=0x%08x recv=0x%08x, rvoice=0x%08x\n",
                  pList->uSendMask, pList->uRecvMask, pList->uRecvVoice);
    const u32 lowerMask = bit - 1;
    pList->uSendMask = ((pList->uSendMask >> 1) & ~lowerMask) |
                       (pList->uSendMask & lowerMask);
    pList->uRecvMask = ((pList->uRecvMask >> 1) & ~lowerMask) |
                       (pList->uRecvMask & lowerMask);
    pList->uRecvVoice = ((pList->uRecvVoice >> 1) & ~lowerMask) |
                        (pList->uRecvVoice & lowerMask);
    NetPrintfCode("voipconnection: flags post-update: send=0x%08x recv=0x%08x, rvoice=0x%08x\n",
                  pList->uSendMask, pList->uRecvMask, pList->uRecvVoice);
}

extern "C" s32 VoipConnectionSend(VoipConnectionListT* pList,
                                   const void* pData, s32 iDataSize)
{
    const u32 tick = NetTick();
    s32 result = 0;
    for (s32 iConnID = 0; iConnID < pList->iMaxConnections; ++iConnID)
    {
        VoipConnectionT* pConnection = &pList->pConnections[iConnID];
        const u32 bit = 1u << (static_cast<u32>(iConnID) & 31);
        if (pConnection->eState != 2 || !(pList->uSendMask & bit))
            continue;

        pConnection->bBroadband = (iDataSize == 47);
        if (pConnection->uNumDataPackets < 5)
        {
            u8* pVoice = reinterpret_cast<u8*>(pConnection->aPacketHeader);
            pVoice += 17 + pConnection->uNumDataPackets * iDataSize;
            std::memcpy(pVoice, pData, static_cast<usize>(iDataSize));
            ++pConnection->uNumDataPackets;
            pList->uLastVoiceData = tick;
        }
        if (pConnection->uNumDataPackets == 5)
            result = VoipConnectionSendQueuedPacket(pList, pConnection, tick, 0);
        if (pList->bServer == 1)
            break;
    }
    return result;
}

extern "C" s32 VoipConnectionFlush(VoipConnectionListT* pList, s32 iConnID)
{
    VoipConnectionT* pConnection = &pList->pConnections[iConnID];
    const u8 queuedPackets = pConnection->uNumDataPackets;
    const u32 bit = 1u << (static_cast<u32>(iConnID) & 31);
    if (pConnection->eState == 2 && (pList->uSendMask & bit) && queuedPackets)
        VoipConnectionSendQueuedPacket(pList, pConnection, NetTick(), 1);
    else
        return 0;
    return queuedPackets;
}

extern "C" s32 VoipConnectionSendQueuedPacket(VoipConnectionListT* pList,
                                               VoipConnectionT* pConnection,
                                               u32 tick, s32 bFlush)
{
    if (pConnection->uNumDataPackets != 5 && bFlush != 1 &&
        (!pConnection->uNumDataPackets ||
         static_cast<s32>(tick - pList->uLastVoiceData) < 100))
        return 0;

    pConnection->aPacketHeader[5] = static_cast<u8>(pConnection->uSendSequence);
    ++pConnection->uSendSequence;
    VoipStoreBigEndian32(pConnection->aPacketHeader + 10, pList->uSendMask);

    s32 result = 0;
    if (pConnection->bBroadband)
    {
        const s32 dataSize = 17 + 47 * pConnection->uNumDataPackets;
        if (pList->pSocket)
        {
            VoipStoreBigEndian32(pConnection->aPacketHeader + 6,
                                 pList->uClientId);
            result = SocketSendto(pList->pSocket, pConnection->aPacketHeader,
                                  dataSize, 0, &pConnection->SendAddr,
                                  sizeof(pConnection->SendAddr));
        }
    }
    else
    {
        u8 packet[68];
        std::memcpy(packet, pConnection->aPacketHeader, sizeof(packet));
        VoipStoreBigEndian32(packet + 6, pList->uClientId);
        const s32 dataSize = 18 + 10 * pConnection->uNumDataPackets;
        if (pList->pSocket)
            result = SocketSendto(pList->pSocket, packet, dataSize, 0,
                                  &pConnection->SendAddr,
                                  sizeof(pConnection->SendAddr));
    }

    pConnection->iLastSendResult = result;
    if (pList->bServer == 0)
        pConnection->uLastSend = tick;
    pConnection->uLastVoiceSend = tick;
    pList->uLocalStatus |= 0x100;
    pConnection->uNumDataPackets = 0;
    return result;
}

// ARTIST sub_82B39380 sends the X360 connection-start datagram and records its
// timestamp/result on the connection. The fixed prefix and four local/remote
// XUID slots are the data words loaded from 0x82F84290 and the live list.
extern "C" s32 VoipConnectionSendConnectPacket(VoipConnectionListT* pList,
                                                VoipConnectionT* pConnection,
                                                u32 tick)
{
    if (!pConnection->SendAddr.sin_addr.s_addr)
        return 0;

    const s32 iConnID = static_cast<s32>(
        (reinterpret_cast<u8*>(pConnection) -
         reinterpret_cast<u8*>(pList->pConnections)) / sizeof(VoipConnectionT));
    NetPrintfCode("voipconnection: [%d] sending connect packet to %s:%d\n",
                  iConnID,
                  SocketInAddrGetText(SocketNtohl(pConnection->SendAddr.sin_addr.s_addr)),
                  SocketNtohs(pConnection->SendAddr.sin_port));

    u8 packet[96];
    std::memcpy(packet, gVoipConnectionStartHeader,
                sizeof(gVoipConnectionStartHeader));
    VoipStoreBigEndian32(packet + 10, pConnection->uClientId);
    VoipStoreBigEndian32(packet + 14, pConnection->uSessionId);
    std::memcpy(packet + 24, pList->aLocalXuids, sizeof(pList->aLocalXuids));
    std::memcpy(packet + 56, pConnection->aRemoteXuids,
                sizeof(pConnection->aRemoteXuids));
    packet[88] = static_cast<u8>(pList->iLocalUserIndex);
    packet[89] = static_cast<u8>(pConnection->iLocalUserIndex);
    packet[90] = static_cast<u8>(pConnection->iRecvPackets > 0);

    s32 result = 0;
    if (pList->pSocket)
    {
        VoipStoreBigEndian32(packet + 6, pList->uClientId);
        result = SocketSendto(pList->pSocket, packet, sizeof(packet), 0,
                              &pConnection->SendAddr,
                              sizeof(pConnection->SendAddr));
    }
    pConnection->iLastSendResult = result;
    pConnection->uLastSend = tick;
    return result;
}

extern "C" s32 VoipConnectionUpdate(VoipConnectionListT* pList)
{
    NetCritEnter(&pList->NetCrit);
    while (VoipConnectionReceivePackets(pList) > 0)
    {
    }

    bool bAnyActive = false;
    for (s32 index = 0; index < pList->iMaxConnections; ++index)
        if (pList->pConnections[index].eState != 0)
            bAnyActive = true;
    if (!bAnyActive && pList->pSocket)
    {
        NetPrintfCode("voipconnection: stopped last connection, destroying socket\n");
        SocketClose(pList->pSocket);
        pList->pSocket = 0;
    }
    NetCritLeave(&pList->NetCrit);

    pList->uRecvVoice = 0;
    const u32 tick = NetTick();
    s32 result = 0;
    for (s32 index = 0; index < pList->iMaxConnections; ++index)
    {
        VoipConnectionT* pConnection = &pList->pConnections[index];
        if (pConnection->eState == 0)
            continue;

        if (static_cast<u32>(tick - pConnection->uLastSend) > 500)
        {
            if (pConnection->eState == 1)
                result = VoipConnectionSendConnectPacket(pList, pConnection, tick);
            else
            {
                u8 pingPacket[14];
                static const u8 pingHeader[10] = {
                    0x00, 0x08, 0x50, 0x4e, 0x47,
                    0x00, 0x00, 0x00, 0x00, 0x00
                };
                std::memcpy(pingPacket, pingHeader, sizeof(pingHeader));
                VoipStoreBigEndian32(pingPacket + 6, pList->uClientId);
                VoipStoreBigEndian32(pingPacket + 10, pConnection->uClientId);
                result = pList->pSocket
                    ? SocketSendto(pList->pSocket, pingPacket, sizeof(pingPacket),
                                   0, &pConnection->SendAddr,
                                   sizeof(pConnection->SendAddr))
                    : 0;
                pConnection->iLastSendResult = result;
                pConnection->uLastSend = tick;
                pList->uLocalStatus &= ~0x100u;
            }
        }
        result = VoipConnectionSendQueuedPacket(pList, pConnection, tick, 0);
        const s32 timeout = pConnection->eState == 2
            ? pList->iDataTimeout : 10000;
        if (static_cast<s32>(tick - pConnection->uLastRecv) > timeout)
        {
            NetPrintfCode("voipconnection: [%d] timing out connection due to inactivity\n",
                          index);
            result = VoipConnectionStop(pList, index);
        }
        if (pConnection->iLastSendResult == -5)
        {
            NetPrintfCode("voipconnection: [%d] closing connection due to error %d\n",
                          index, pConnection->iLastSendResult);
            result = VoipConnectionStop(pList, index);
        }
        if (pConnection->uRemoteStatus & 2)
            pList->uRecvVoice |= 1u << (static_cast<u32>(index) & 31);
    }
    return result;
}

static u32 VoipLoadBigEndian32(const u8* pBytes)
{
    return (static_cast<u32>(pBytes[0]) << 24) |
           (static_cast<u32>(pBytes[1]) << 16) |
           (static_cast<u32>(pBytes[2]) << 8) |
           static_cast<u32>(pBytes[3]);
}

// ARTIST 0x82B2D2D8 compares sockaddr families and then the family-specific
// address bytes. The receive path uses this result only for equality.
static s32 VoipSocketAddressCompare(const SOCKADDR_IN* pLeft,
                                    const SOCKADDR_IN* pRight)
{
    if (pLeft->sin_family != pRight->sin_family)
        return static_cast<s32>(pLeft->sin_family) -
               static_cast<s32>(pRight->sin_family);

    const u8* pLeftBytes = reinterpret_cast<const u8*>(pLeft) + 2;
    const u8* pRightBytes = reinterpret_cast<const u8*>(pRight) + 2;
    const s32 byteCount = pLeft->sin_family == AF_INET ? 6 : 14;
    for (s32 index = 0; index < byteCount; ++index)
    {
        if (pLeftBytes[index] != pRightBytes[index])
            return static_cast<s32>(pLeftBytes[index]) -
                   static_cast<s32>(pRightBytes[index]);
    }
    return 0;
}

// ARTIST 0x82B39608 transitions a pending connection and registers its user.
static void VoipConnectionActivate(VoipConnectionListT* pList,
                                    VoipConnectionT* pConnection)
{
    s32 iConnID = static_cast<s32>(
        (reinterpret_cast<u8*>(pConnection) -
         reinterpret_cast<u8*>(pList->pConnections)) / static_cast<s32>(sizeof(VoipConnectionT)));
    NetPrintfCode("voipconnection: [%d] connection established\n", iConnID);
    pConnection->eState = 2;
    pConnection->bBroadband = 0;
    pConnection->uRemoteStatus = 1;
    pList->uLastVoiceData = NetTick();
    iConnID = static_cast<s32>(
        (reinterpret_cast<u8*>(pConnection) -
         reinterpret_cast<u8*>(pList->pConnections)) / static_cast<s32>(sizeof(VoipConnectionT)));
    u64 remoteXuid = 0;
    std::memcpy(&remoteXuid, VoipUserForConnection(pConnection)->data, sizeof(remoteXuid));
    NetPrintfCode("voipconnection: [%d] talking to user qwUserID=0x%08x%08x dwUserFlags=0x%08x\n",
                  iConnID, static_cast<u32>(remoteXuid >> 32),
                  static_cast<u32>(remoteXuid), 0);
    pList->pRegUserCb(VoipUserForConnection(pConnection), pList->pCbUserData, 1);
}

// ARTIST sub_82B39E10 parses one datagram for an already configured peer.
static void VoipConnectionProcessPacket(VoipConnectionListT* pList,
                                        VoipConnectionT* pConnection,
                                        const SOCKADDR_IN* pRemoteAddr,
                                        const u8* pPacket,
                                        s32 iPacketSize)
{
    static const u8 pingHeader[3] = { 0x50, 0x4e, 0x47 };
    const s32 iConnID = static_cast<s32>(
        (reinterpret_cast<u8*>(pConnection) -
         reinterpret_cast<u8*>(pList->pConnections)) / sizeof(VoipConnectionT));
    const u32 uClientId = VoipLoadBigEndian32(pPacket + 6);
    const u32 uSessionId = VoipLoadBigEndian32(pPacket + 14);
    const u32 tick = NetTick();

    pConnection->iRecvPackets = static_cast<s32>(
        static_cast<u32>(pConnection->iRecvPackets) + 1);
    pConnection->iRecvData = static_cast<s32>(
        static_cast<u32>(pConnection->iRecvData) +
        static_cast<u32>(iPacketSize));

    if (pConnection->SendAddr.sin_port != pRemoteAddr->sin_port)
    {
        const u16 oldPort = pConnection->SendAddr.sin_port;
        const s32 iPortChange = pConnection->iChangePort;
        pConnection->iChangePort = static_cast<s32>(
            static_cast<u32>(pConnection->iChangePort) + 1);
        NetPrintfCode("voipsocket: [%d] port remap #%d: %d bytes from %d instead of %d\n",
                      iConnID, iPortChange, iPacketSize,
                      pRemoteAddr->sin_port, oldPort);
        pConnection->SendAddr.sin_port = pRemoteAddr->sin_port;
        pConnection->iRecvPackets = 0;
    }
    if (pConnection->SendAddr.sin_addr.s_addr == 0)
        pConnection->SendAddr.sin_addr.s_addr = pRemoteAddr->sin_addr.s_addr;

    if (std::memcmp(pPacket + 2, gVoipConnectionStartHeader + 2, 3) == 0)
    {
        NetPrintfCode("voipconnection: [%d] received conn packet bConnected=%d eState=%d uClientId=0x%08x, uSessionId=0x%08x\n",
                      iConnID, pPacket[0x5a], pConnection->eState,
                      uClientId, uSessionId);
        if (uSessionId == pConnection->uSessionId)
        {
            std::memcpy(pConnection->aRemoteXuids, pPacket + 0x18,
                        sizeof(pConnection->aRemoteXuids));
            pConnection->iLocalUserIndex = static_cast<s8>(pPacket[0x58]);
            if (pPacket[0x5a] != 0 && pConnection->eState == 1)
                VoipConnectionActivate(pList, pConnection);
            pConnection->uLastRecv = tick;
        }
        else
        {
            NetPrintfCode("voipconnection: [%d] ignoring conn packet with sessionId=0x%08x (expecting sessionId=0x%08x)\n",
                          iConnID, uSessionId, pConnection->uSessionId);
        }
        return;
    }

    if (std::memcmp(pPacket + 2, pingHeader, sizeof(pingHeader)) == 0)
    {
        if (pConnection->eState == 1)
            VoipConnectionActivate(pList, pConnection);
        pConnection->uLastRecv = tick;
        pConnection->uRemoteStatus = (pConnection->uRemoteStatus & ~0x6u) | 0x4u;
        return;
    }

    if (std::memcmp(pPacket + 2, gVoipConnectionVoiceHeader + 2, 3) == 0)
    {
        if (pConnection->eState == 1)
            VoipConnectionActivate(pList, pConnection);
        pConnection->uLastRecv = tick;
        pConnection->uRemoteStatus |= 0xe;
        const u32 bit = 1u << (static_cast<u32>(iConnID) & 31);
        if (pList->uRecvMask & bit)
            pList->pVoiceCb(VoipUserForConnection(pConnection),
                            reinterpret_cast<VoipPacketT*>(
                                const_cast<u8*>(pPacket)),
                            pList->pCbUserData);
        return;
    }

    if (std::memcmp(pPacket + 2, gVoipDisconnectHeader + 2, 3) == 0)
    {
        if (uSessionId == pConnection->uSessionId)
        {
            NetPrintfCode("voipconnection: [%d] remote peer 0x%08x has disconnected\n",
                          iConnID, uClientId);
            VoipConnectionStop(pList, iConnID);
        }
        else
        {
            NetPrintfCode("voipconnection: [%d] ignoring disc packet with sessionId=0x%08x (expecting sessionId=0x%08x)\n",
                          iConnID, uSessionId, pConnection->uSessionId);
        }
        return;
    }

    NetPrintfCode("voipconnection: [%d] received unknown packet type 0x%02x%02x%02x\n",
                  iConnID, pPacket[2], pPacket[3], pPacket[4]);
}

// ARTIST 0x82B3A190 matches an unsolicited connection packet to a configured
// remote user before handing it to the same per-peer parser.
static void VoipConnectionHandleIncoming(VoipConnectionListT* pList,
                                          const SOCKADDR_IN* pRemoteAddr,
                                          const u8* pPacket,
                                          s32 iPacketSize)
{
    const u32 uClientId = VoipLoadBigEndian32(pPacket + 6);
    const u32 uSessionId = VoipLoadBigEndian32(pPacket + 14);
    const s32 iUserIndex = static_cast<s8>(pPacket[0x58]);
    const u8* pSelectedXuid = pPacket + (iUserIndex + 3) * 8;
    s32 iConnID = 0;

    for (; iConnID < pList->iMaxConnections; ++iConnID)
    {
        VoipConnectionT* pConnection = &pList->pConnections[iConnID];
        if (std::memcmp(pSelectedXuid, pConnection->aRemoteXuids,
                        sizeof(u64)) == 0 &&
            pConnection->uSessionId == uSessionId)
            break;
    }

    if (iConnID < pList->iMaxConnections)
    {
        VoipConnectionT* pConnection = &pList->pConnections[iConnID];
        std::memcpy(&pConnection->SendAddr, pRemoteAddr, sizeof(pConnection->SendAddr));
        pConnection->uClientId = uClientId;
        VoipUserT* pRemoteUser = VoipUserForConnection(pConnection);
        u64 remoteXuid = 0;
        std::memcpy(&remoteXuid, pRemoteUser->data, sizeof(remoteXuid));
        const u16 remotePort = SocketNtohs(pRemoteAddr->sin_port);
        const u32 remoteAddress = SocketNtohl(pRemoteAddr->sin_addr.s_addr);
        NetPrintfCode("voipconnection: [%d] setting up connection to user 0x%08x%08x (addr=%s:%d, clientId=0x%08x)\n",
                      iConnID, static_cast<u32>(remoteXuid >> 32),
                      static_cast<u32>(remoteXuid),
                      SocketInAddrGetText(remoteAddress), remotePort, uClientId);
        VoipConnectionProcessPacket(pList, pConnection, pRemoteAddr,
                                    pPacket, iPacketSize);
    }
    else
    {
        u64 remoteXuid = 0;
        std::memcpy(&remoteXuid, pSelectedXuid, sizeof(remoteXuid));
        NetPrintfCode("voipconnection: received conn packet from unconnected user 0x%08x%08x\n",
                      static_cast<u32>(remoteXuid >> 32),
                      static_cast<u32>(remoteXuid));
    }
}

extern "C" s32 VoipConnectionReceivePackets(VoipConnectionListT* pList)
{
    if (!pList->pSocket)
        return 0;

    SOCKADDR_IN remoteAddr;
    s32 addressLength = sizeof(remoteAddr);
    u8 packet[0xfc];
    const s32 packetSize = SocketRecvfrom(pList->pSocket, packet,
                                           sizeof(packet), 0, &remoteAddr,
                                           &addressLength);
    if (packetSize <= 0)
        return packetSize;

    const u32 uClientId = VoipLoadBigEndian32(packet + 6);
    s32 iConnID = 0;
    for (; iConnID < pList->iMaxConnections; ++iConnID)
    {
        VoipConnectionT* pConnection = &pList->pConnections[iConnID];
        if (pList->uClientId != 0)
        {
            if (pConnection->uClientId == uClientId)
                break;
        }
        else if (VoipSocketAddressCompare(&remoteAddr,
                                         &pConnection->SendAddr) == 0)
            break;
    }

    if (iConnID < pList->iMaxConnections)
    {
        VoipConnectionProcessPacket(pList, &pList->pConnections[iConnID],
                                    &remoteAddr, packet, packetSize);
    }
    else if (std::memcmp(packet + 2, gVoipConnectionStartHeader + 2, 3) == 0)
    {
        VoipConnectionHandleIncoming(pList, &remoteAddr, packet, packetSize);
    }
    else if (std::memcmp(packet + 2, gVoipDisconnectHeader + 2, 3) != 0)
    {
        NetPrintfCode("voipconnection: ignoring %c%c%c packet from clientId=0x%08x\n",
                      packet[2], packet[3], packet[4], uClientId);
    }
    return packetSize;
}

extern "C" void VoipIncomingVoice(VoipUserT* pRemoteUser,
                                    VoipPacketT* pPacket,
                                    void* pUserData)
{
    VoipRefT* pVoip = static_cast<VoipRefT*>(pUserData);
    const u8* pPacketBytes = reinterpret_cast<const u8*>(pPacket);
    u64 remoteXuid = 0;
    std::memcpy(&remoteXuid, pRemoteUser->data, sizeof(remoteXuid));
    DWORD dataSize = static_cast<DWORD>(pPacketBytes[16] * 10);
    const HRESULT result = pVoip->pXhvEngine->SubmitIncomingChatData(
        remoteXuid, pPacketBytes + 17, &dataSize);
    if (result)
    {
        NetPrintfCode("voip: SubmitIncomingChatData() failed (err=%s)\n",
                      DirtyErrGetName(static_cast<s32>(result)));
    }
}

extern "C" void VoipRegisterRemoteTalker(VoipUserT* pRemoteUser,
                                           void* pUserData,
                                           u32 bRegister)
{
    VoipRefT* pVoip = static_cast<VoipRefT*>(pUserData);
    VoipConnectionListT& list = pVoip->Connectionlist;
    u64 remoteXuid = 0;
    std::memcpy(&remoteXuid, pRemoteUser->data, sizeof(remoteXuid));
    if (remoteXuid == 0)
        return;

    if (bRegister)
    {
        pVoip->pXhvEngine->RegisterRemoteTalker(remoteXuid, 0, 0, 0);
        pVoip->pXhvEngine->StartRemoteProcessingModes(
            remoteXuid, (PXHV_PROCESSING_MODE)list.pVoiceChatMode, 1);
    }
    else
    {
        pVoip->pXhvEngine->SetPlaybackPriority(
            remoteXuid, static_cast<DWORD>(list.iLocalTalker),
            XHV_PLAYBACK_PRIORITY_NEVER);
        pVoip->pXhvEngine->UnregisterRemoteTalker(remoteXuid);
    }
    list.bMuteListDirty = 1;
}

extern "C" void VoipSetRemoteTalkerProcessingModes(VoipRefT* pVoip,
                                                    s32 bStart)
{
    VoipConnectionListT& list = pVoip->Connectionlist;
    for (s32 index = 0; index < list.iMaxConnections; ++index)
    {
        VoipConnectionT* pConnection = &list.pConnections[index];
        if (pConnection->eState == 2)
            VoipRegisterRemoteTalker(VoipUserForConnection(pConnection), pVoip,
                                     static_cast<u32>(bStart));
    }
}

extern "C" s32 VoipBindLocalTalker(VoipRefT* pVoip, u32 dwUserIndex)
{
    VoipConnectionListT& list = pVoip->Connectionlist;
    if (list.iLocalTalker != -1)
    {
        pVoip->pXhvEngine->StopLocalProcessingModes(
            static_cast<DWORD>(list.iLocalTalker),
            (PXHV_PROCESSING_MODE)list.pStartedLocalMode, 1);
        pVoip->pXhvEngine->UnregisterLocalTalker(
            static_cast<DWORD>(list.iLocalTalker));
        list.uLocalStatus &= ~0x50u;
        list.iLocalTalker = -1;
    }

    NetPrintfCode("voip: registering local talker to port %d\n", dwUserIndex);
    if (pVoip->pXhvEngine->RegisterLocalTalker(dwUserIndex) == 0)
    {
        pVoip->pXhvEngine->StartLocalProcessingModes(
            dwUserIndex, (PXHV_PROCESSING_MODE)list.pVoiceChatMode, 1);
        list.iLocalTalker = static_cast<s32>(dwUserIndex);
        list.iLocalUserIndex = static_cast<s32>(dwUserIndex);
        list.pStartedLocalMode = list.pVoiceChatMode;
    }
    return list.iLocalTalker == static_cast<s32>(dwUserIndex);
}

extern "C" void VoipUpdatePrivileges(VoipRefT* pVoip)
{
    VoipConnectionListT& list = pVoip->Connectionlist;
    DWORD result = XUserCheckPrivilege(255,
                                       static_cast<XPRIVILEGE_TYPE>(XPRIVILEGE_COMMUNICATIONS),
                                       reinterpret_cast<PBOOL>(&list.bVoiceAllowed));
    if (result)
        NetPrintfCode("voip: error 0x%08x trying to get XPRIVILEGE_COMMUNICATIONS\n",
                      result);
    result = XUserCheckPrivilege(255,
                                 static_cast<XPRIVILEGE_TYPE>(XPRIVILEGE_COMMUNICATIONS_FRIENDS_ONLY),
                                 reinterpret_cast<PBOOL>(&list.bFriendsOnly));
    if (result)
        NetPrintfCode("voip: error 0x%08x trying to get XPRIVILEGE_COMMUNICATIONS_FRIENDS_ONLY\n",
                      result);

    const char* status = list.bVoiceAllowed ? "enabled" :
        (list.bFriendsOnly ? "friends only" : "disabled");
    NetPrintfCode("voip: voice is %s\n", status);
}

extern "C" void VoipUpdateMuteList(VoipRefT* pVoip)
{
    VoipConnectionListT& list = pVoip->Connectionlist;
    if (!list.bMuteListDirty)
        return;
    for (s32 index = 0; index < list.iMaxConnections; ++index)
    {
        VoipConnectionT* pConnection = &list.pConnections[index];
        if (pConnection->eState != 2)
            continue;
        VoipUserT* pRemoteUser = VoipUserForConnection(pConnection);
        u64 remoteXuid = 0;
        std::memcpy(&remoteXuid, pRemoteUser->data, sizeof(remoteXuid));
        BOOL bMuted = FALSE;
        XUserMuteListQuery(static_cast<DWORD>(list.iLocalTalker), remoteXuid,
                           &bMuted);
        if (pConnection->bMuted != static_cast<u32>(bMuted))
        {
            NetPrintfCode("voip: %d) muting is %s\n", index,
                          bMuted ? "enabled" : "disabled");
            pConnection->bMuted = static_cast<u32>(bMuted);
        }
    }
    list.bMuteListDirty = 0;
}

extern "C" s32 VoipUpdateVoiceActivity(VoipRefT* pVoip)
{
    VoipConnectionListT& list = pVoip->Connectionlist;
    s32 status = NetConnStatus(1718775414, 0, 0, 0);
    if (list.iLastNetConnStatus == status)
        return status;

    const u32 maxPeers = static_cast<u32>(list.iMaxConnections);
    list.uFriendsOnlyMask = maxPeers >= 32
        ? 0xffffffffu : (1u << maxPeers) - 1;
    for (s32 iConnID = 0; iConnID < list.iMaxConnections; ++iConnID)
    {
        VoipConnectionT* pConnection = &list.pConnections[iConnID];
        const u32 bit = 1u << (static_cast<u32>(iConnID) & 31);
        for (DWORD userIndex = 0; userIndex < 4; ++userIndex)
        {
            BOOL bFriendsOnly = FALSE;
            if (XUserCheckPrivilege(userIndex,
                                    static_cast<XPRIVILEGE_TYPE>(XPRIVILEGE_COMMUNICATIONS_FRIENDS_ONLY),
                                    reinterpret_cast<PBOOL>(&bFriendsOnly)) == 0 &&
                bFriendsOnly &&
                NetConnStatus(1769170546, static_cast<s32>(userIndex),
                              VoipUserForConnection(pConnection), 0) == 0)
                list.uFriendsOnlyMask &= ~bit;
        }
    }
    list.iLastNetConnStatus = NetConnStatus(1718775414, 0, 0, 0);
    return list.iLastNetConnStatus;
}

namespace
{
void VoipUpdateRemoteTalkerRouting(VoipRefT* pVoip)
{
    VoipConnectionListT& list = pVoip->Connectionlist;
    for (s32 index = 0; index < list.iMaxConnections; ++index)
    {
        VoipConnectionT* pConnection = &list.pConnections[index];
        if (pConnection->eState != 2)
            continue;

        const u32 bit = 1u << (static_cast<u32>(index) & 31);
        const bool bAllowed = !pConnection->bMuted &&
            (list.bVoiceAllowed ||
             (list.bFriendsOnly && (list.uFriendsOnlyMask & bit) != 0));
        const u32 sendMask = bAllowed && (list.uUserSendMask & bit)
            ? list.uSendMask | bit : list.uSendMask & ~bit;
        const u32 recvMask = bAllowed && (list.uUserRecvMask & bit)
            ? list.uRecvMask | bit : list.uRecvMask & ~bit;
        if (sendMask != list.uSendMask)
            VoipConnectionSetSendMask(&list, sendMask);
        if (recvMask != list.uRecvMask)
            VoipConnectionSetRecvMask(&list, recvMask);

        VoipUserT* pRemoteUser = VoipUserForConnection(pConnection);
        u64 remoteXuid = 0;
        std::memcpy(&remoteXuid, pRemoteUser->data, sizeof(remoteXuid));
        const DWORD priority = (recvMask & bit)
            ? XHV_PLAYBACK_PRIORITY_MAX : XHV_PLAYBACK_PRIORITY_NEVER;
        pVoip->pXhvEngine->SetPlaybackPriority(
            remoteXuid, static_cast<DWORD>(list.iLocalTalker), priority);
    }
}
}

namespace
{
s32 VoipXhvUpdate(VoipRefT* pVoip)
{
    VoipConnectionListT& list = pVoip->Connectionlist;
    ULONG_PTR notificationData[2] = {0, 0};
    DWORD notificationId = 0;
    if (XNotifyGetNext(pVoip->hNotifyListener, 17, &notificationId,
                       notificationData))
    {
        NetPrintfCode("voip: mutelist change->0x%02x\n", notificationData[0]);
        list.bMuteListDirty = 1;
    }
    if (XNotifyGetNext(pVoip->hNotifyListener, 10, &notificationId,
                       notificationData))
        VoipUpdatePrivileges(pVoip);
    VoipUpdateMuteList(pVoip);
    VoipUpdateVoiceActivity(pVoip);
    VoipUpdateRemoteTalkerRouting(pVoip);

    u32 headsetMask = 0;
    for (DWORD userIndex = 0; userIndex < 4; ++userIndex)
    {
        const u32 bit = 1u << userIndex;
        if (pVoip->pXhvEngine->IsHeadsetPresent(userIndex))
            headsetMask |= bit;
        if ((list.uHeadsetMask ^ headsetMask) & bit)
            NetPrintfCode("voip: headset in port %d %s\n", userIndex,
                          (headsetMask & bit) ? "inserted" : "removed");
    }
    const u32 localBit = 1u << (static_cast<u32>(list.iLocalTalker) & 31);
    if (headsetMask & localBit)
        list.uLocalStatus |= 0x40;
    else
        list.uLocalStatus &= ~0x40u;
    list.uHeadsetMask = headsetMask;

    const bool bLoopbackMode = list.pStartedLocalMode ==
        (void*)&XHV_LOOPBACK_MODE;
    const DWORD initialSize = bLoopbackMode ? 47 : 10;
    if (list.iLocalTalker >= 0)
    {
        u8 localVoiceData[96];
        DWORD dataSize = initialSize;
        while (pVoip->pXhvEngine->GetLocalChatData(
                   static_cast<DWORD>(list.iLocalTalker), localVoiceData,
                   &dataSize, 0) >= 0)
        {
            const DWORD expectedSize = list.pStartedLocalMode ==
                (void*)&XHV_LOOPBACK_MODE ? 47 : 10;
            if (dataSize != expectedSize)
            {
                NetPrintfCode("voip: invalid voice data size %d (should be %d)\n",
                              dataSize, expectedSize);
                break;
            }
            VoipConnectionSend(&list, localVoiceData,
                               static_cast<s32>(dataSize));
        }
    }
    return VoipConnectionUpdate(&list);
}
}

extern "C" void VoipWorkerThread(void* pData)
{
    VoipRefT* pVoip = static_cast<VoipRefT*>(pData);
    VoipConnectionListT& list = pVoip->Connectionlist;
    while (!list.iThreadActive)
        Sleep(1);
    list.iThreadActive = 2;
    while (list.iThreadActive)
    {
        RtlEnterCriticalSection(&list.ThreadCrit);
        VoipXhvUpdate(pVoip);
        RtlLeaveCriticalSection(&list.ThreadCrit);
        Sleep(20);
    }
    list.hThread = 0;
}

extern "C" VoipRefT* VoipGetRef(void)
{
    return gVoipRef;
}

// ARTIST 0x82B28F18. NetConn passes the current tick as a second argument; the
// console body observes only the ref and the status transitions below.
extern "C" void VoipIdle(void* pData, u32)
{
    VoipRefT* pVoip = static_cast<VoipRefT*>(pData);
    VoipConnectionListT& list = pVoip->Connectionlist;
    if (list.uLastModeState != list.uHeadsetMask)
    {
        list.pCallback(pVoip, 0, static_cast<s32>(list.uHeadsetMask),
                       list.pCallbackUserData);
        list.uLastModeState = list.uHeadsetMask;
    }
    if (list.uLastRecvVoice != list.uRecvVoice)
    {
        list.pCallback(pVoip, 1, static_cast<s32>(list.uRecvVoice),
                       list.pCallbackUserData);
        list.uLastRecvVoice = list.uRecvVoice;
    }
    if (list.uLastLocalStatus != list.uLocalStatus)
    {
        if ((list.uLastLocalStatus ^ list.uLocalStatus) & 0x100)
            list.pCallback(pVoip, 2, static_cast<s32>(list.uLocalStatus & 0x100),
                           list.pCallbackUserData);
        list.uLastLocalStatus = list.uLocalStatus;
    }
}

// ARTIST 0x82B29638; the worker, connection callbacks and mode factories are
// retained as direct providers so this TU preserves the real XHV/transport edges.
extern "C" VoipRefT* VoipStartup(s32 iMaxPeers)
{
    const s32 iMemGroup = DirtyMemGroupQuery();
    if (gVoipRef)
    {
        NetPrintfCode("voip: module startup called when not in a shutdown state\n");
        return 0;
    }

    VoipRefT* pVoip = static_cast<VoipRefT*>(
        DirtyMemAlloc(sizeof(VoipRefT), kVoipMemModule, iMemGroup));
    if (!pVoip)
    {
        NetPrintfCode("voip: unable to allocate module state\n");
        return 0;
    }
    std::memset(pVoip, 0, sizeof(*pVoip));
    VoipMemGroupSet(iMemGroup);
    pVoip->Connectionlist.uUserSendMask = 0xffffffffu;
    pVoip->Connectionlist.uUserRecvMask = 0xffffffffu;
    pVoip->Connectionlist.uLocalStatus = 0x20;

    u64 initialLocalXuid = 0;
    if (VoipConnectionStartup(&pVoip->Connectionlist,
                              reinterpret_cast<VoipUserT*>(&initialLocalXuid),
                              iMaxPeers) < 0)
    {
        NetPrintfCode("voip: unable to allocate connectionlist\n");
        DirtyMemFree(pVoip, kVoipMemModule, iMemGroup);
        return 0;
    }

    const XHV_PROCESSING_MODE processingModes[2] = {
        XHV_VOICECHAT_MODE,
        XHV_LOOPBACK_MODE
    };
    XHV_INIT_PARAMS params;
    std::memset(&params, 0, sizeof(params));
    params.dwMaxRemoteTalkers = static_cast<DWORD>(iMaxPeers);
    params.dwMaxLocalTalkers = 1;
    params.localTalkerEnabledModes = processingModes;
    params.dwNumLocalTalkerEnabledModes = 2;
    params.remoteTalkerEnabledModes = processingModes;
    params.dwNumRemoteTalkerEnabledModes = 1;
    if (gVoipMicRawDataReady)
    {
        params.bCustomVADProvided = TRUE;
        params.pfnMicrophoneRawDataReady = gVoipMicRawDataReady;
    }
    HANDLE hXhvWorkerThread = 0;
    IXHVEngine* pXhvEngine = 0;
    const HRESULT hrXhv = XHVCreateEngine(&params, &hXhvWorkerThread,
                                          &pXhvEngine);
    if (hrXhv < 0)
    {
        NetPrintfCode("voip: error 0x%08x creating XVH engine\n",
                      static_cast<u32>(hrXhv));
        DirtyMemFree(pVoip, kVoipMemModule, iMemGroup);
        return 0;
    }

    RtlInitializeCriticalSection(&pVoip->Connectionlist.ThreadCrit);
    pVoip->Connectionlist.hThread = CreateThread(
        0, 0, (LPTHREAD_START_ROUTINE)VoipWorkerThread, pVoip, 0,
        &pVoip->Connectionlist.dwThreadId);
    if (!pVoip->Connectionlist.hThread)
    {
        NetPrintfCode("voip: error creating voip thread\n");
        VoipShutdown(pVoip);
        return 0;
    }
    SetThreadPriority(pVoip->Connectionlist.hThread, 2);
    VoipConnectionSetCallbacks(&pVoip->Connectionlist, VoipIncomingVoice,
                               VoipRegisterRemoteTalker, pVoip);
    pVoip->Connectionlist.pVoiceChatMode = (void*)&XHV_VOICECHAT_MODE;
    pVoip->Connectionlist.pStartedLocalMode = (void*)&XHV_VOICECHAT_MODE;
    pVoip->pXhvEngine = pXhvEngine;
    pVoip->Connectionlist.iLockPort = -1;
    pVoip->Connectionlist.iLocalTalker = -1;
    pVoip->hNotifyListener = XNotifyCreateListener(111);
    if (!pVoip->hNotifyListener)
        NetPrintfCode("netconnxenon: unable to create live event listener\n");
    pVoip->Connectionlist.uLastRecvVoice = pVoip->Connectionlist.uRecvVoice;
    pVoip->Connectionlist.uLastModeState = pVoip->Connectionlist.uHeadsetMask;
    pVoip->Connectionlist.pCallback = VoipDefaultEventCallback;
    NetConnIdleAdd(VoipIdle, pVoip);
    gVoipRef = pVoip;
    pVoip->Connectionlist.iThreadActive = 1;
    return pVoip;
}

// ARTIST 0x82B28FF0.
extern "C" void VoipShutdown(VoipRefT* pVoip)
{
    if (!gVoipRef)
    {
        NetPrintfCode("voip: module shutdown called when not in a started state\n");
        return;
    }
    VoipConnectionListT& list = pVoip->Connectionlist;
    while (!list.iThreadActive)
    {
    }
    list.iThreadActive = 0;
    if (list.hThread)
    {
        while (list.hThread)
        {
            NetPrintfCode("voip: waiting for shutdown\n");
            Sleep(1);
        }
    }
    NetConnIdleDel(VoipIdle, pVoip);
    VoipConnectionShutdown(&list);
    if (list.iLocalTalker != -1)
        pVoip->pXhvEngine->UnregisterLocalTalker(list.iLocalTalker);
    pVoip->pXhvEngine->Release();
    if (pVoip->hNotifyListener)
        CloseHandle(pVoip->hNotifyListener);
    DirtyMemFree(pVoip, kVoipMemModule, VoipMemGroupQuery());
    gVoipRef = 0;
}

// ARTIST 0x82B290F0; the XUID text is parsed and compared to the four signed-in users.
extern "C" void VoipSetLocalUser(VoipRefT* pVoip, const char* pUserID)
{
    const u64 xuid = VoipDecodeXuid(pUserID);
    s32 localIndex = 0;
    XUID signedInXuid = 0;
    for (; localIndex < 4; ++localIndex)
    {
        XUserGetXUID(static_cast<DWORD>(localIndex), &signedInXuid);
        if (signedInXuid == xuid)
            break;
    }
    if (localIndex == 4)
    {
        NetPrintfCode("voip: could not find user with XUID=%s\n", pUserID);
        return;
    }

    NetPrintfCode("voip: setting local user to '%s' (idx=%d)\n", pUserID, localIndex);
    pVoip->Connectionlist.aLocalXuids[localIndex] = xuid;
    if (pVoip->Connectionlist.iLocalTalker != localIndex)
    {
        VoipSetRemoteTalkerProcessingModes(pVoip, 0);
        VoipBindLocalTalker(pVoip, static_cast<u32>(localIndex));
        VoipSetRemoteTalkerProcessingModes(pVoip, 1);
    }
    VoipUpdatePrivileges(pVoip);
}

// ARTIST 0x82B291F0; connection start/stop stays delegated to the real list providers.
extern "C" s32 VoipConnect4(VoipRefT* pVoip, s32 iConnID, const char* pUserID,
                            u32 uAddress, u32 uManglePort, u32 uGamePort,
                            u32 uClientId, u32 uSessionId)
{
    VoipConnectionListT& list = pVoip->Connectionlist;
    if (iConnID == -1)
    {
        iConnID = VoipConnectionAllocate(&list);
        if (iConnID < 0)
            return -3;
    }
    else
    {
        if (iConnID < 0 || iConnID >= list.iMaxConnections)
        {
            NetPrintfCode("voip: connect with iConnID=%d is invalid\n", iConnID);
            return -4;
        }
        if (list.pConnections)
        {
            RtlEnterCriticalSection(&list.ThreadCrit);
            VoipConnectionStop(&list, iConnID);
            RtlLeaveCriticalSection(&list.ThreadCrit);
        }
    }

    u64 xuid = 0;
    VoipUserT* pUser = 0;
    if (pUserID)
    {
        xuid = VoipDecodeXuid(pUserID);
        // The X360 caller supplies a pointer to the parsed 64-bit XUID in the
        // opaque user's first bytes; the connection helper owns the record copy.
        pUser = reinterpret_cast<VoipUserT*>(&xuid);
    }
    RtlEnterCriticalSection(&list.ThreadCrit);
    // ARTIST 82B292E8-EC and 82B2932C-30 pass the game port in both port slots.
    VoipConnectionStart(&list, iConnID, pUser, uAddress, uGamePort,
                        uGamePort, uClientId, uSessionId);
    RtlLeaveCriticalSection(&list.ThreadCrit);
    if (pUserID)
        VoipUpdateMuteList(pVoip);
    return iConnID;
}

// ARTIST 0x82B29378.
extern "C" void VoipDisconnect(VoipRefT* pVoip, s32 iConnID)
{
    VoipConnectionListT& list = pVoip->Connectionlist;
    RtlEnterCriticalSection(&list.ThreadCrit);
    VoipConnectionStop(&list, iConnID);
    RtlLeaveCriticalSection(&list.ThreadCrit);
}

// ARTIST 0x82B293B8.
extern "C" void VoipRemove(VoipRefT* pVoip, s32 iConnID)
{
    VoipConnectionListT& list = pVoip->Connectionlist;
    RtlEnterCriticalSection(&list.ThreadCrit);
    VoipConnectionRemove(&list, iConnID);
    list.uUserSendMask = VoipConnectionRemoveConnFromMask(list.uUserSendMask, iConnID);
    list.uUserRecvMask = VoipConnectionRemoveConnFromMask(list.uUserRecvMask, iConnID);
    RtlLeaveCriticalSection(&list.ThreadCrit);
}

// ARTIST 0x82B29420.
extern "C" s32 VoipRemote(VoipRefT* pVoip, s32 iConnID)
{
    VoipConnectionListT& list = pVoip->Connectionlist;
    if (!list.pConnections)
        return static_cast<s32>(0x80000000u);
    if (iConnID == -2)
    {
        u32 flags = 0;
        for (s32 index = 0; index < list.iMaxConnections; ++index)
            flags |= list.pConnections[index].uRemoteStatus;
        return static_cast<s32>(flags);
    }
    if (iConnID < 0 || iConnID >= list.iMaxConnections)
        return static_cast<s32>(0x80000000u);
    return static_cast<s32>(list.pConnections[iConnID].uRemoteStatus);
}

// ARTIST 0x82B29490.
extern "C" u32 VoipLocal(VoipRefT* pVoip)
{
    VoipConnectionListT& list = pVoip->Connectionlist;
    u32 status = list.uLocalStatus;
    if (list.iLockPort >= 0 && list.iLocalTalker != list.iLockPort)
        status &= ~0x40u;
    return status;
}

// ARTIST 0x82B294B8 / 0x82B294C0.
extern "C" void VoipMicrophone(VoipRefT* pVoip, u32 uConnMask)
{
    pVoip->Connectionlist.uUserSendMask = uConnMask;
}

extern "C" void VoipSpeaker(VoipRefT* pVoip, u32 uConnMask)
{
    pVoip->Connectionlist.uUserRecvMask = uConnMask;
}

// ARTIST 0x82B294C8.
extern "C" s32 VoipControl(VoipRefT* pVoip, s32 iControl, s32 iValue)
{
    VoipConnectionListT& list = pVoip->Connectionlist;
    switch (iControl)
    {
    case 0x636c6964: // clid
        list.uClientId = static_cast<u32>(iValue);
        return 0;
    case 0x666c7368: // flsh
        return VoipConnectionFlush(&list, iValue);
    case 0x6c6f636b: // lock
        list.iLockPort = iValue;
        return 0;
    case 0x6d726472: // mrd
        gVoipMicRawDataReady = (PFNMICRAWDATAREADY)iValue;
        return 0;
    case 0x706f7274: // port
        if (list.iLocalTalker != iValue)
        {
            VoipSetRemoteTalkerProcessingModes(pVoip, 0);
            const s32 result = VoipBindLocalTalker(pVoip, static_cast<u32>(iValue));
            VoipSetRemoteTalkerProcessingModes(pVoip, 1);
            return result;
        }
        return 1;
    case 0x7175616c: // qual
        NetPrintfCode("voip: SetQualityLevel not supported without XHV_PREVIEW_LIB\n");
        return 1;
    case 0x73657276: // serv
        NetPrintfCode("voip: setting server mode = %d\n", iValue);
        list.bServer = static_cast<u8>(iValue);
        return 0;
    case 0x74696d65: // time
        list.iDataTimeout = iValue;
        return 0;
    default:
        return -1;
    }
}
