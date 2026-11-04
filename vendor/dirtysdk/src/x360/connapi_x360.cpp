// DirtySDK ConnApi native X360 state and entry points; ARTIST 0x82B15430,
// 0x82B155D8 and 0x82B162B0; declaration names cross-checked with DecFIGS.
#include "connapi.h"
#include "dirtymem.h"
#include "dirtylib.h"
#include "voip.h"
#include <cstddef>
#include <cstring>

struct ProtoMangleRefT;
struct ProtoTunnelRefT;
extern "C" s32 ProtoMangleControl(ProtoMangleRefT*, s32, s32, s32, const void*);
extern "C" ProtoTunnelRefT* ProtoTunnelCreate(s32, s32);
extern "C" VoipRefT* VoipStartup(s32);
extern "C" void VoipSetLocalUser(VoipRefT*, const DirtyAddrT*);
extern "C" char* ds_strnzcpy(char*, const char*, s32);

namespace CgsNetwork { namespace DirtySock {
struct ConnApiRefT
{
    ConnApiCallbackT* pCallback;
    void* pUserData;
    s32 iMemGroup;
    u32 uClientId;
    u16 uGamePort, uVoipPort, uConnFlags, uGameServConnMode, uGameServFallback;
    char strGameName[32];
    u32 uGameServAddr;
    s32 eGameServMode;
    u32 uGameFlags;
    s32 iLinkBufSize;
    ProtoMangleRefT* pProtoMangle;
    ProtoTunnelRefT* pProtoTunnel;
    s32 iTunnelPort;
    VoipRefT* pVoipRef;
    char strSelfName[32];
    DirtyAddrT SelfAddr;
    s32 iSelf;
    u8 bVoipOwner, bDemanglerEnabled, bPeerWeb, bVoipEnabled;
    u8 bInCallback, bRemoveCallback, bAutoUpdate, bSlotOverride, bTunnelEnabled;
    u32 uSessionFlags;
    s32 iTimeout, iGameMinp, iGameMout, iGameMwid;
    char strSession[128];
    char strNonce[16];
    s32 iHostIndex;
    s32 eState;
    ConnApiClientT GameServer;
    ConnApiClientListT ClientList;
};
}}
CGS_STATIC_ASSERT((offsetof(ConnApiRefT, uGamePort) == 0x10), "ConnApi game port");
CGS_STATIC_ASSERT((offsetof(ConnApiRefT, strGameName) == 0x1a), "ConnApi game name");
CGS_STATIC_ASSERT((offsetof(ConnApiRefT, pProtoMangle) == 0x4c), "ConnApi mangle");
CGS_STATIC_ASSERT((offsetof(ConnApiRefT, SelfAddr) == 0x7c), "ConnApi self address");
CGS_STATIC_ASSERT((offsetof(ConnApiRefT, bTunnelEnabled) == 0xc8), "ConnApi tunnel flag");
CGS_STATIC_ASSERT((offsetof(ConnApiRefT, uSessionFlags) == 0xcc), "ConnApi session flags");
CGS_STATIC_ASSERT((offsetof(ConnApiRefT, strSession) == 0xe0), "ConnApi session");
CGS_STATIC_ASSERT((offsetof(ConnApiRefT, strNonce) == 0x160), "ConnApi nonce");
CGS_STATIC_ASSERT((offsetof(ConnApiRefT, eState) == 0x174), "ConnApi state");
CGS_STATIC_ASSERT((offsetof(ConnApiRefT, ClientList) == 0x234), "ConnApi client list");

extern "C" const ConnApiClientListT* ConnApiGetClientList(ConnApiRefT* ref)
{
    return &ref->ClientList;
}

extern "C" void ConnApiStop(ConnApiRefT* ref)
{
    if (!ref->eState)
        NetPrintfCode("connapi: can't disconnect when in idle state\n");
    else if (ref->eState == 5)
        NetPrintfCode("connapi: can't disconnect when already disconnecting\n");
    else
    {
        NetPrintfCode("connapi: ending current game\n");
        ProtoMangleControl(ref->pProtoMangle, 0x73746f70, 0, 0, 0);
    }
}

extern "C" void ConnApiOnline(ConnApiRefT* ref, const char* game,
                              const char* self, const DirtyAddrT* addr)
{
    DirtyMemGroupEnter(ref->iMemGroup);
    if (ref->bTunnelEnabled)
    {
        ref->pProtoTunnel = ProtoTunnelCreate(ref->ClientList.iMaxClients - 1, ref->iTunnelPort);
        if (!ref->pProtoTunnel) ref->bTunnelEnabled = 0;
    }
    if (ref->bVoipEnabled)
    {
        ref->pVoipRef = VoipGetRef();
        if (!ref->pVoipRef)
        {
            NetPrintfCode("connapi: starting VoIP with iMaxClients=%d\n", ref->ClientList.iMaxClients);
            ref->pVoipRef = VoipStartup(ref->ClientList.iMaxClients);
            ref->bVoipOwner = 1;
        }
    }
    if (ref->pVoipRef) VoipControl(ref->pVoipRef, 0x74696d65, ref->iTimeout);
    ProtoMangleControl(ref->pProtoMangle, 0x73656c66, 0, 0, addr);
    if (ref->pVoipRef) VoipSetLocalUser(ref->pVoipRef, addr);
    ds_strnzcpy(ref->strGameName, game, 32);
    ds_strnzcpy(ref->strSelfName, self, 32);
    std::memcpy(&ref->SelfAddr, addr, sizeof(ref->SelfAddr));
    DirtyMemGroupLeave();
}

extern "C" s32 ProtoMangleStatus(ProtoMangleRefT*, s32, void*, s32);

// ARTIST 0x82B155E0.
extern "C" s32 ConnApiStatus(ConnApiRefT* ref, s32 select, void* buffer, s32 size)
{
    switch (select)
    {
    case 0x63626670: // cbfp
        if (buffer && static_cast<u32>(size) >= 4)
        { std::memcpy(buffer, &ref->pCallback, 4); return 0; }
        break;
    case 0x63627570: // cbup
        if (buffer && static_cast<u32>(size) >= 4)
        { std::memcpy(buffer, &ref->pUserData, 4); return 0; }
        break;
    case 0x67707274: return ref->uGamePort; // gprt
    case 0x67736d64: // gsmd
        if ((ref->uGameServFallback & 1) ||
            (ref->uGameServAddr && (ref->uGameServConnMode & 1))) return ref->eGameServMode;
        break;
    case 0x67737276: // gsrv
        if (buffer && static_cast<u32>(size) >= sizeof(ref->GameServer))
        { std::memcpy(buffer, &ref->GameServer, sizeof(ref->GameServer)); return 0; }
        break;
    case 0x686f7374: // host
        if (buffer && ref->ClientList.iNumClients > 0)
        {
            ds_strnzcpy(static_cast<char*>(buffer),
                        ref->ClientList.Clients[ref->iHostIndex].UserInfo.strName, size);
            return ref->eState == 3;
        }
        break;
    case 0x696e676d: return ref->eState != 0; // ingm
    case 0x6c627566: return ref->iLinkBufSize; // lbuf
    case 0x6d696e70: return ref->iGameMinp; // minp
    case 0x6d6f7574: return ref->iGameMwid; // mout, ARTIST loads +0xDC at 0x82B157C8
    case 0x6d776964: return ref->iGameMout; // mwid, ARTIST loads +0xD8 at 0x82B157E0
    case 0x70656572: return ref->bPeerWeb; // peer
    case 0x73656c66: return ref->iSelf; // self
    case 0x73657373: // sess
        if (ref->strSession[0])
        { ds_strnzcpy(static_cast<char*>(buffer), ref->strSession, size); return 0; }
        break;
    case 0x74756e6c: return ref->bTunnelEnabled; // tunl
    case 0x74797065: return ref->uConnFlags; // type
    case 0x76707274: return ref->uVoipPort; // vprt
    }
    return ProtoMangleStatus(ref->pProtoMangle, select, buffer, size);
}

#include "netconn.h"
extern "C" ProtoMangleRefT* ProtoMangleCreate(const char*, s32, const char*, const char*);
extern "C" void ProtoMangleDestroy(ProtoMangleRefT*);
extern "C" void ProtoTunnelDestroy(ProtoTunnelRefT*);

namespace {
// ARTIST 0x82B14348, with strings and pointer-table ordering recovered from the image.
void DefaultCallback(ConnApiRefT*, ConnApiCbInfoT* info, void*)
{
    static const char* const states[] = {
        "CONNAPI_STATUS_INIT", "CONNAPI_STATUS_CONN", "CONNAPI_STATUS_MNGL",
        "CONNAPI_STATUS_ACTV", "CONNAPI_STATUS_CLSE", "CONNAPI_STATUS_DISC",
        "CONNAPI_STATUS_DEST"
    };
    static const char* const types[] = {"CONNAPI_CBTYPE_GAMEEVENT", "CONNAPI_CBTYPE_VOIPEVENT", 0};
    NetPrintfCode("connapi: client %d) [%s] %s -> %s\n", info->iClientIndex,
                  types[info->eType], states[info->eOldStatus], states[info->eNewStatus]);
}

// ARTIST 0x82B16148.
void ConnApiIdle(void* data, u32)
{
    ConnApiRefT* ref = static_cast<ConnApiRefT*>(data);
    if (ref->bAutoUpdate == 1) ConnApiUpdate(ref);
}
}

// ARTIST 0x82B16160.
extern "C" ConnApiRefT* ConnApiCreate(s32 gamePort, s32 maxClients,
                                    ConnApiCallbackT* callback, void* userData)
{
    s32 memGroup = DirtyMemGroupQuery();
    s32 size = static_cast<s32>(188u * static_cast<u32>(maxClients) + 760u);
    ConnApiRefT* ref = static_cast<ConnApiRefT*>(DirtyMemAlloc(size, 0x636f6e6e, memGroup));
    if (!ref)
    {
        NetPrintfCode("connapi: could not allocate module state\n");
        return 0;
    }
    std::memset(ref, 0, size);
    ref->iMemGroup = memGroup;
    ref->pProtoMangle = ProtoMangleCreate(0, 3658, 0, 0);
    if (!ref->pProtoMangle)
    {
        NetPrintfCode("connapi: unable to create protomangle ref\n");
        DirtyMemFree(ref, 0x636f6e6e, memGroup);
        return 0;
    }
    ref->uSessionFlags = ProtoMangleStatus(ref->pProtoMangle, 0x73666c67, 0, 0);
    ref->uGamePort = static_cast<u16>(gamePort);
    ref->ClientList.iMaxClients = maxClients;
    ref->uVoipPort = 6000;
    ref->pCallback = callback ? callback : DefaultCallback;
    ref->pUserData = userData;
    ref->iLinkBufSize = 1024;
    ref->uClientId = 1;
    ref->uConnFlags = 3;
    ref->iTimeout = 15000;
    ref->iTunnelPort = 1000;
    ref->uGameServConnMode = 3;
    ref->uGameServFallback = 0;
    ref->bVoipEnabled = 1;
    ref->bAutoUpdate = 1;
    NetConnIdleAdd(ConnApiIdle, ref);
    return ref;
}

// ARTIST 0x82B163D0.
extern "C" void ConnApiDestroy(ConnApiRefT* ref)
{
    if (ref->eState && ref->eState != 5) ConnApiDisconnect(ref);
    NetConnIdleDel(ConnApiIdle, ref);
    if (ref->pProtoMangle)
    {
        ProtoMangleDestroy(ref->pProtoMangle);
        ref->pProtoMangle = 0;
    }
    for (s32 i = 0; i < ref->ClientList.iNumClients; ++i)
        ref->ClientList.Clients[i].UserInfo.uAddr = 0;
    if (ref->pVoipRef && ref->bVoipOwner == 1) VoipShutdown(ref->pVoipRef);
    if (ref->pProtoTunnel) ProtoTunnelDestroy(ref->pProtoTunnel);
    DirtyMemFree(ref, 0x636f6e6e, ref->iMemGroup);
}

#include "dirtylib_x360_internal.h"
struct NetGameUtilRefT;
extern "C" void NetGameLinkDestroy(NetGameLinkRefT*);
extern "C" void NetGameUtilDestroy(NetGameUtilRefT*);
extern "C" void ProtoTunnelFree(ProtoTunnelRefT*, u32, const char*);

namespace {
// ARTIST 0x82B14210.
void DisconnectGame(ConnApiRefT* ref, ConnApiClientT* client, s32 index, const char* reason)
{
    bool hadConnection = client->uReservedA8 != 0 || client->pGameLinkRef != 0;
    NetPrintfCode("connapi: destroying game connection to user %s: %s\n", client->UserInfo.strName, reason);
    if (client->pGameLinkRef)
    {
        NetGameLinkDestroy(client->pGameLinkRef);
        client->pGameLinkRef = 0;
    }
    if (client->pGameUtilRef)
    {
        NetGameUtilDestroy(static_cast<NetGameUtilRefT*>(client->pGameUtilRef));
        client->pGameUtilRef = 0;
    }
    client->GameInfo.eStatus = 5;
    if (hadConnection)
    {
        ConnApiCbInfoT info;
        info.iClientIndex = index;
        info.eType = 0;
        info.eOldStatus = 5;
        info.eNewStatus = 6;
        ref->bInCallback = 1;
        ref->pCallback(ref, &info, ref->pUserData);
        ref->bInCallback = 0;
    }
}

// ARTIST 0x82B142B8.
void DisconnectClient(ConnApiRefT* ref, ConnApiClientT* client, s32 index, const char* reason)
{
    DisconnectGame(ref, client, index, reason);
    u32 tick = NetTick();
    NetPrintfCode("connapi: destroying voip connection to user %s: %s at %d\n", client->UserInfo.strName, reason, tick);
    if (client->iVoipConnId >= 0)
    {
        VoipDisconnect(ref->pVoipRef, client->iVoipConnId);
        client->iVoipConnId = -1;
    }
    client->VoipInfo.eStatus = 5;
    if (ref->bTunnelEnabled)
    {
        ProtoTunnelFree(ref->pProtoTunnel, static_cast<u32>(client->iTunnelId), 0);
        client->iTunnelId = 0;
    }
}
}

// ARTIST 0x82B154B8. The current public header declares an integer result;
// the X360 return register retains the called routine's result.
extern "C" s32 ConnApiDisconnect(ConnApiRefT* ref)
{
    if (!ref->eState) return NetPrintfCode("connapi: can't disconnect when in idle state\n");
    if (ref->eState == 5) return NetPrintfCode("connapi: can't disconnect when already disconnecting\n");
    NetPrintfCode("connapi: disconnecting\n");
    for (s32 i = 0; i < ref->ClientList.iNumClients; ++i)
        if (i != ref->iSelf) DisconnectClient(ref, &ref->ClientList.Clients[i], i, "disconnect");
    if (ref->uGameServAddr && ref->eGameServMode == 1)
    {
        DisconnectClient(ref, &ref->GameServer, 0, "disconnect");
        std::memset(&ref->GameServer, 0, sizeof(ref->GameServer));
    }
    ref->ClientList.iNumClients = 0;
    std::memset(&ref->ClientList.Clients[0], 0, sizeof(ref->ClientList.Clients[0]));
    s32 result = ProtoMangleControl(ref->pProtoMangle, 0x64656c65, 0, 0, 0);
    ref->eState = 5;
    return result;
}

struct SocketT;
extern "C" s32 SocketControl(SocketT*, s32, s32, void*, void*);
extern "C" const char* SocketInAddrGetText(s32);
extern "C" s32 ProtoTunnelControl(ProtoTunnelRefT*, s32, s32, s32, const void*);

// ARTIST 0x82B158B8.
extern "C" s32 ConnApiControl(ConnApiRefT* ref, s32 control, s32 value,
                              s32 value2, const void* data)
{
    switch (control)
    {
    case 0x6175746f: ref->bAutoUpdate = static_cast<u8>(value); return 0; // auto
    case 0x63626670: ref->pCallback = reinterpret_cast<ConnApiCallbackT*>(const_cast<void*>(data)); return 0;
    case 0x63627570: ref->pUserData = const_cast<void*>(data); return 0;
    case 0x64697374: // dist
        if (value == 0x67737276) { ref->GameServer.uReservedA8 = reinterpret_cast<uintptr_t>(data); return 0; }
        if (value < ref->ClientList.iNumClients)
        { ref->ClientList.Clients[value].uReservedA8 = reinterpret_cast<uintptr_t>(data); return 0; }
        break;
    case 0x67647363: // gdsc
        if (value < ref->ClientList.iNumClients)
            DisconnectGame(ref, &ref->ClientList.Clients[value], value, "user disconnect");
        return 0;
    case 0x67707274: // gprt
        NetPrintfCode("connapi: using game port %d\n", value);
        ref->uGamePort = static_cast<u16>(value);
        return 0;
    case 0x67737276: // gsrv
    {
        u32 address = static_cast<u32>(value);
        if (address && NetConnStatus(0x73656375, 0, 0, 0))
        {
            sockaddr_in from = {};
            sockaddr_in to;
            from.sin_family = 2;
            from.sin_addr.s_addr = address;
            if (!SocketControl(0, 0x6d616472, 0, &from, &to))
            {
                address = to.sin_addr.s_addr;
                NetPrintfCode("connapi: remapping GameServer addr=%s ", SocketInAddrGetText(static_cast<s32>(from.sin_addr.s_addr)));
                NetPrintfCode("to secure addr=%s\n", SocketInAddrGetText(static_cast<s32>(address)));
            }
        }
        NetPrintfCode("connapi: setting gameserver=%s mode=%d\n", SocketInAddrGetText(static_cast<s32>(address)), value2);
        ref->uGameServAddr = address;
        ref->eGameServMode = value2;
        if (ref->pVoipRef) VoipControl(ref->pVoipRef, 0x73657276,
                                      address && (ref->uGameServConnMode & 2) ? 1 : 0);
        return 0;
    }
    case 0x67737632: // gsv2
        NetPrintfCode("connapi: setting gameserver conn mode=%d fallback mode=%d\n", value, value2);
        ref->uGameServConnMode = static_cast<u16>(value);
        ref->uGameServFallback = static_cast<u16>(value2);
        return 0;
    case 0x68737469: ref->iHostIndex = value; return 0; // hsti
    case 0x6c627566: ref->iLinkBufSize = value; return 0; // lbuf
    case 0x6d696772: // migr
        ref->bDemanglerEnabled = static_cast<u8>(value);
        ref->eState = 1;
        ProtoMangleControl(ref->pProtoMangle, control, value, value2, data);
        return 0;
    case 0x6d696e70: ref->iGameMinp = value; return 0;
    case 0x6d6f7574: ref->iGameMout = value; return 0;
    case 0x6d776964: ref->iGameMwid = value; return 0;
    case 0x6e6f6e63: ds_strnzcpy(ref->strNonce, static_cast<const char*>(data), 16); return 0;
    case 0x70656572: // peer
        NetPrintfCode("connapi: peerweb %s\n", value ? "enabled" : "disabled");
        ref->bPeerWeb = static_cast<u8>(value);
        return 0;
    case 0x7263626b: ref->bRemoveCallback = static_cast<u8>(value); return 0;
    case 0x73657373: ds_strnzcpy(ref->strSession, static_cast<const char*>(data), 128); return 0;
    case 0x73666c67: ref->uSessionFlags = value; break; // sflg
    case 0x736b696c: // skil
        ProtoMangleControl(ref->pProtoMangle, 0x73756964, 0, 0,
                           &ref->ClientList.Clients[value].UserInfo.DirtyAddr);
        break;
    case 0x736c6f74: ref->bSlotOverride = 1; break; // slot
    case 0x73746174: // stat
        ProtoMangleControl(ref->pProtoMangle, 0x73756964, 0, 0,
                           &ref->ClientList.Clients[value].UserInfo.DirtyAddr);
        return ProtoMangleControl(ref->pProtoMangle, control, value2, 0, data);
    case 0x74696d65: // time
        ref->iTimeout = value;
        if (ref->pVoipRef) VoipControl(ref->pVoipRef, control, value);
        return 0;
    case 0x7463746c: // tctl
        if (ref->pProtoTunnel) return ProtoTunnelControl(ref->pProtoTunnel, value, value2, 0, data);
        break;
    case 0x74756e6c: // tunl
        ref->bTunnelEnabled = static_cast<u8>(value);
        if (value && value2) ref->iTunnelPort = value2;
        return 0;
    case 0x74797065: // type
        NetPrintfCode("connapi: connflag change from 0x%02x to 0x%02x\n", ref->uConnFlags, value);
        ref->uConnFlags = static_cast<u16>(value);
        return 0;
    case 0x766f6970: // voip
        if (ref->pVoipRef) { VoipControl(ref->pVoipRef, value, value2); return 0; }
        break;
    case 0x76707274: // vprt
        NetPrintfCode("connapi: using voip port %d\n", value);
        ref->uVoipPort = static_cast<u16>(value);
        return 0;
    case 0x76736574: // vset
        ref->bVoipEnabled = static_cast<u8>(value);
        if (!value) ConnApiControl(ref, 0x74797065, 1, 0, 0);
        return 0;
    }
    return ProtoMangleControl(ref->pProtoMangle, control, value, value2, data);
}
