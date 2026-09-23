#include "GameShared/GameClasses/Network/Packeting/CgsNetworkAdapterBase.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Network/CgsNetworkUtils.h"   // IPAddressIntToString

#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::NetworkAdapterPrepareParams::Construct @ 0x82581330
//   (called by BrnNetwork::BrnNetworkManager::Prepare)
//
// Bounds-checks the server type and the two required pointers (CGS_ASSERT trio), then
// writes the five param words store-for-store in the asm's order:
//   stw r25(a6), 0x00 ;  stw r29(leServerType), 0x04 ;  stw r28(lpHeapMalloc), 0x08
//   stw r27(lpNetworkManager), 0x0C ;  stw r26(a5), 0x10
// The server-type guard is the combined "(leServerType >= E_SERVER_TYPE_LOCAL) &&
// (leServerType < E_SERVER_TYPE_COUNT)" range test (the asm splits it into blt 0 / blt 7).

namespace CgsNetwork
{

NetworkAdapterPrepareParams* NetworkAdapterPrepareParams::Construct(
        u32 leServerType, void* lpHeapMalloc, void* lpNetworkManager,
        ServerInterfaceDirtySock* lpServerInterface, u32 luTitleID)
{
    CGS_ASSERT((leServerType >= E_SERVER_TYPE_LOCAL) && (leServerType < E_SERVER_TYPE_COUNT),
               "(leServerType >= E_SERVER_TYPE_LOCAL) && (leServerType < E_SERVER_TYPE_COUNT)");
    CGS_ASSERT(lpHeapMalloc, "lpHeapMalloc");
    CGS_ASSERT(lpNetworkManager, "lpNetworkManager");

    muTitleID        = luTitleID;         // +0x00
    meServerType     = leServerType;      // +0x04
    mpHeapMalloc     = lpHeapMalloc;      // +0x08
    mpNetworkManager = lpNetworkManager;  // +0x0C
    mpServerInterface = lpServerInterface; // +0x10

    return this;
}

// The game's packet kind on the link.
static const u8 KU8_GAME_PACKET_KIND = 7;

s32 NetworkAdapterBase::miSendPerfMon = -1;

// ---- Construct -----------------------------------------------------------------------
// No callback, manager, buffers or server type yet; the shared send monitor is registered
// by the first adapter constructed.
void NetworkAdapterBase::Construct()
{
    mpMessageSentCallbackFunction = nullptr;
    meServerType                  = E_SERVER_TYPE_COUNT;
    meLastError                   = E_NET_ERROR_NONE;
    mpNetworkManager              = nullptr;
    mpHeapMalloc                  = nullptr;
    mpRecvBuffer                  = nullptr;
    mbDuplicateLogin              = false;

    if (miSendPerfMon == -1)
    {
        miSendPerfMon = CgsDev::PerfMonCpu::AddMonitor("NetworkAdapter - Send", CgsDev::E_PMP_18, false, 5.0f, true);
    }
}

// ---- Prepare -------------------------------------------------------------------------
// Take the server type, manager, server interface and heap from the params and allocate the
// one-packet receive buffer.
NetworkAdapterBase::ENetworkStatus NetworkAdapterBase::Prepare(NetworkAdapterPrepareParams* lpPrepareParams)
{
    CGS_ASSERT(lpPrepareParams, "lpPrepareParams");

    meServerType      = static_cast<EServerType>(lpPrepareParams->meServerType);
    mpNetworkManager  = lpPrepareParams->mpNetworkManager;
    mbDuplicateLogin  = false;
    mpServerInterface = lpPrepareParams->mpServerInterface;
    mpHeapMalloc      = static_cast<CgsMemory::HeapMalloc*>(lpPrepareParams->mpHeapMalloc);
    CGS_ASSERT(mpHeapMalloc, "mpHeapMalloc");

    mpRecvBuffer = static_cast<u8*>(mpHeapMalloc->Malloc(static_cast<s32>(sizeof(NetGamePacketT)), 4));

    CGS_ASSERT(mpNetworkManager, "mpNetworkManager");
    CGS_ASSERT(mpServerInterface, "mpServerInterface");
    CGS_ASSERT(mpRecvBuffer, "mpRecvBuffer");
    return E_NET_STATUS_READY;
}

// ---- Release -------------------------------------------------------------------------
bool NetworkAdapterBase::Release()
{
    mpNetworkManager = nullptr;
    meServerType     = E_SERVER_TYPE_COUNT;
    mbDuplicateLogin = false;

    if (mpRecvBuffer != nullptr)
    {
        CGS_ASSERT(mpHeapMalloc, "mpHeapMalloc");
        mpHeapMalloc->Free(mpRecvBuffer);
        mpRecvBuffer = nullptr;
    }

    mpHeapMalloc = nullptr;
    return true;
}

// ---- SendTo --------------------------------------------------------------------------
// Wrap liLength bytes in one game packet and send it down the peer's link. On success the
// message-sent hook sees the data; on failure the peer's address is logged.
bool NetworkAdapterBase::SendTo(void* lpData, s32 liLength, ConnectionData lConnectionData)
{
    CGS_ASSERT(lConnectionData.IsValid(), "lConnectionData.IsValid()");

    NetGamePacketT lPacket;
    CGS_ASSERT((liLength) <= static_cast<int32_t>( sizeof(lPacket.body.data) ),
               "(liLength) <= static_cast<int32_t>( sizeof(lPacket.body.data) )");

    std::memset(&lPacket, 0, sizeof(lPacket));
    lPacket.head.len  = static_cast<u16>(liLength);
    lPacket.head.kind = KU8_GAME_PACKET_KIND;
    std::memcpy(lPacket.body.data, lpData, static_cast<size_t>(liLength));

    CgsDev::PerfMonCpu::StartMonitor(miSendPerfMon);
    const s32 liSent = NetGameLinkSend(lConnectionData.mpNetGameLink, &lPacket, 1);
    CgsDev::PerfMonCpu::StopMonitor(miSendPerfMon);

    if (liSent > 0)
    {
        if (mpMessageSentCallbackFunction != nullptr)
        {
            reinterpret_cast<void (*)(void*)>(mpMessageSentCallbackFunction)(lpData);
        }
        return true;
    }

    char lacIPAddress[32];
    IPAddressIntToString(lacIPAddress, lConnectionData.miIPAddress);
    *CgsDev::Log::gpDebugPrint << "NetGameLinkSend failed to " << lacIPAddress << "\n";
    return false;
}

// ---- ReceiveFrom ---------------------------------------------------------------------
// Pull one packet off the peer's link into the receive buffer: *lppData points at its
// payload, the payload length is returned (0 when nothing is queued or there is no link).
s32 NetworkAdapterBase::ReceiveFrom(void** lppData, ConnectionData lConnectionData)
{
    s32 liLength = 0;
    NetGamePacketT* lpPacket = reinterpret_cast<NetGamePacketT*>(mpRecvBuffer);

    CGS_ASSERT(lppData, "lppData");
    CGS_ASSERT(mpRecvBuffer, "mpRecvBuffer");

    if (lConnectionData.mpNetGameLink == nullptr)
    {
        return 0;
    }

    if (NetGameLinkRecv(lConnectionData.mpNetGameLink, lpPacket, 1) > 0)
    {
        liLength = lpPacket->head.len;
        *lppData = lpPacket->body.data;
    }
    return liLength;
}

}
