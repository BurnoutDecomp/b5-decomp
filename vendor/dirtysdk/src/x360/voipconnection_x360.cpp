// DirtySDK 5.5.3 X360 voice-connection module (DWARF voip/voipconnection.c): the peer
// connection list, its packets and socket; ARTIST 0x82B39908..0x82B3AB88.
// XDK size optimization matches ARTIST at 0x82B399E0, 0x82B39A48 and 0x82B39CB8.

#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

#include "voip_x360_internal.h"

namespace
{
const u8 gVoipDisconnectHeader[10] = {
    0x00, 0x08, 0x44, 0x53, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00
};

const u8 gVoipConnectionVoiceHeader[10] = {
    0x00, 0x08, 0x4d, 0x49, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00
};

const u8 gVoipConnectionStartHeader[10] = {
    0x00, 0x5e, 0x43, 0x4f, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00
};

void VoipStoreBigEndian32(u8* pBytes, u32 value)
{
    pBytes[0] = static_cast<u8>(value >> 24);
    pBytes[1] = static_cast<u8>(value >> 16);
    pBytes[2] = static_cast<u8>(value >> 8);
    pBytes[3] = static_cast<u8>(value);
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

        pConnection->eState = VoipConnectionT::ST_DISC;
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
    NetCritKill(&pList->NetCrit);
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

// ARTIST 0x82B39CB8 leaves r3 untouched when the mask is unchanged.
extern "C" void VoipConnectionSetSendMask(VoipConnectionListT* pList, u32 uSendMask)
{
    if (pList->uSendMask != uSendMask)
    {
        NetPrintfCode("voipconnection: updating send mask to 0x%08x\n", uSendMask);
        pList->uSendMask = uSendMask;
    }
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
    pConnection->eState = VoipConnectionT::ST_CONN;
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
    pConnection->eState = VoipConnectionT::ST_ACTV;
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
