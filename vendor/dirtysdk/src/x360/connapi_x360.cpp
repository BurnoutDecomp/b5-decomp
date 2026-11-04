// DirtySDK ConnApi native X360 state and entry points; ARTIST 0x82B15430,
// 0x82B155D8 and 0x82B162B0; declaration names cross-checked with DecFIGS.
#include "connapi.h"
#include "prototunnel.h"
#include "dirtymem.h"
#include "dirtylib.h"
#include "platform.h"
#include "voip.h"
#include <cstddef>
#include <cstring>

struct ProtoMangleRefT;
extern "C" s32 ProtoMangleControl(ProtoMangleRefT*, s32, s32, s32, const void*);
extern "C" VoipRefT* VoipStartup(s32);
extern "C" void VoipSetLocalUser(VoipRefT*, const char*);
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
    u8 bVoipOwner, bHost, bPeerWeb, bVoipEnabled;
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
    if (ref->pVoipRef) VoipSetLocalUser(ref->pVoipRef, addr->strMachineAddr);
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
        ref->bHost = static_cast<u8>(value);
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

// ARTIST lifecycle helpers shared by ConnApiHost/Connect/AddClient/RemoveClient/Start/Update.
CGS_STATIC_ASSERT((sizeof(ProtoTunnelInfoT) == 0x24), "ConnApi tunnel request size");
CGS_STATIC_ASSERT((offsetof(ProtoTunnelInfoT, aRemotePortList) == 0x0c), "ConnApi tunnel ports");
CGS_STATIC_ASSERT((offsetof(ProtoTunnelInfoT, aPortFlags) == 0x1c), "ConnApi tunnel flags");

extern "C" void ProtoMangleConnect(ProtoMangleRefT*, s32, const char*);
extern "C" s32 ProtoMangleComplete(ProtoMangleRefT*, s32*, s32*);
extern "C" void ProtoMangleUpdate(ProtoMangleRefT*);
extern "C" s32 VoipConnect4(VoipRefT*, s32, const char*, u32, u32, u32, u32, u32);
extern "C" s32 VoipRemote(VoipRefT*, s32);
extern "C" void VoipRemove(VoipRefT*, s32);
extern "C" NetGameLinkRefT* NetGameLinkCreate(void*, s32, s32);
struct NetGameUtilRefT;
extern "C" NetGameUtilRefT* NetGameUtilCreate(void);
extern "C" void* NetGameUtilComplete(NetGameUtilRefT*);
extern "C" s32 NetGameUtilConnect(NetGameUtilRefT*, s32, const char*);
extern "C" void NetGameUtilControl(NetGameUtilRefT*, s32, s32);
extern "C" s32 NetGameUtilStatus(NetGameUtilRefT*, s32, void*, s32);

namespace
{
static const char ConnApiTunnelKey[] = ""; // ARTIST passes 0x8324E919, whose first byte is NUL.

void ConnApiSetClientFlags(ConnApiRefT* ref)
{
    for (s32 i = 0; i < ref->ClientList.iNumClients; ++i)
    {
        ConnApiClientT* client = &ref->ClientList.Clients[i];
        client->uConnFlags = ref->uConnFlags;
        if (i != ref->iHostIndex && !ref->bPeerWeb)
            client->uConnFlags &= static_cast<u16>(~1u);
    }
}

void ConnApiInitializeClient(ConnApiRefT* ref, ConnApiClientT* client,
                             const ConnApiUserInfoT* userInfo)
{
    std::memset(client, 0, sizeof(*client));
    std::memcpy(&client->UserInfo, userInfo, sizeof(client->UserInfo));
    client->iVoipConnId = -1;
    client->GameInfo.uMnglPort = ref->uGamePort;
    if (!client->UserInfo.uLocalGamePort)
        client->UserInfo.uLocalGamePort = ref->uGamePort;
    client->GameInfo.uLocalPort = client->UserInfo.uLocalGamePort;
    client->VoipInfo.uMnglPort = ref->uVoipPort;
    if (!client->UserInfo.uLocalVoipPort)
        client->UserInfo.uLocalVoipPort = ref->uVoipPort;
    client->VoipInfo.uLocalPort = client->UserInfo.uLocalVoipPort;
    client->UserInfo.uAddr = 0;
    if (!client->UserInfo.uClientId)
        client->UserInfo.uClientId = ref->uClientId++;
    ds_strnzcpy(client->UserInfo.strTunnelKey,
                client->UserInfo.DirtyAddr.strMachineAddr, 18);
}

void ConnApiInitializeClientList(ConnApiRefT* ref, ConnApiUserInfoT* clientList,
                                 s32 numClients)
{
    s32 maxClients = numClients;
    if (numClients > ref->ClientList.iMaxClients)
    {
        NetPrintfCode("connapi: cannot host %d clients; clamping to %d\n",
                      numClients, ref->ClientList.iMaxClients);
        maxClients = ref->ClientList.iMaxClients;
    }

    ref->iSelf = -1;
    for (s32 i = 0; i < maxClients; ++i)
    {
        ConnApiClientT* client = &ref->ClientList.Clients[i];
        ConnApiInitializeClient(ref, client, &clientList[i]);
        if (std::strcmp(client->UserInfo.strName, ref->strSelfName) == 0)
        {
            std::memcpy(&ref->SelfAddr, &client->UserInfo.DirtyAddr,
                        sizeof(ref->SelfAddr));
            ref->iSelf = i;
        }
    }
    ref->ClientList.iNumClients = maxClients;

    const s32 selfOffset = static_cast<s32>(offsetof(ConnApiRefT, ClientList)) +
                           static_cast<s32>(offsetof(ConnApiClientListT, Clients)) +
                           ref->iSelf * static_cast<s32>(sizeof(ConnApiClientT));
    ConnApiClientT* self = reinterpret_cast<ConnApiClientT*>(
        reinterpret_cast<u8*>(ref) + selfOffset);
    if (!ref->GameServer.UserInfo.uAddr)
    {
        std::memcpy(&ref->GameServer, self, sizeof(ref->GameServer));
        ref->GameServer.UserInfo.uAddr = ref->uGameServAddr;
        ds_strnzcpy(ref->GameServer.UserInfo.strName, "GameServer",
                    sizeof(ref->GameServer.UserInfo.strName));
        ref->GameServer.UserInfo.strTunnelKey[0] = 0;
    }
    if (ref->pVoipRef)
        VoipControl(ref->pVoipRef, 0x636c6964, self->UserInfo.uClientId);
    if (ref->pProtoTunnel)
        ProtoTunnelControl(ref->pProtoTunnel, 0x636c6964,
                           static_cast<s32>(self->UserInfo.uClientId), 0, 0);

    ConnApiSetClientFlags(ref);
    if (ref->iSelf == -1)
        NetPrintfCode("connapi: local user %s not in client list\n", ref->strSelfName);
    NetPrintfCode("connapi: clientlist display\n");
    for (s32 i = 0; i < ref->ClientList.iNumClients; ++i)
    {
        const ConnApiClientT* client = &ref->ClientList.Clients[i];
        NetPrintfCode("connapi:   %d) %s %s <%s>\n", i,
                      SocketInAddrGetText(static_cast<s32>(client->UserInfo.uAddr)),
                      client->UserInfo.strName,
                      client->UserInfo.DirtyAddr.strMachineAddr);
    }
}

s32 ConnApiUpdateSessionParams(ConnApiRefT* ref, s32 isHost)
{
    const ConnApiClientT* host = &ref->ClientList.Clients[ref->iHostIndex];
    ProtoMangleControl(ref->pProtoMangle, 0x686f7374, 0, 0,
                       &host->UserInfo.DirtyAddr);
    if (!isHost)
        ProtoMangleControl(ref->pProtoMangle, 0x6e6f6e63, 0, 0, ref->strNonce);
    if (!ref->bSlotOverride)
    {
        s32 slotsIn = 0;
        s32 slotsOut = 0;
        if (ref->uSessionFlags & 2)
            slotsOut = ref->ClientList.iMaxClients;
        else
            slotsIn = ref->ClientList.iMaxClients;
        ProtoMangleControl(ref->pProtoMangle, 0x736c6f74, slotsIn, slotsOut, 0);
    }
    s32 sessionFlags = static_cast<s32>(ref->uSessionFlags);
    if (ref->uSessionFlags & 2)
        sessionFlags |= 0x200;
    return ProtoMangleControl(ref->pProtoMangle, 0x73666c67,
                              sessionFlags, 0, 0);
}

void ConnApiNotify(ConnApiRefT* ref, s32 clientIndex, s32 type,
                   s32 oldStatus, s32 newStatus)
{
    if (oldStatus != newStatus)
    {
        ConnApiCbInfoT info;
        info.iClientIndex = clientIndex;
        info.eType = type;
        info.eOldStatus = oldStatus;
        info.eNewStatus = newStatus;
        ref->bInCallback = 1;
        ref->pCallback(ref, &info, ref->pUserData);
        ref->bInCallback = 0;
    }
}

bool ConnApiGameServerEnabled(ConnApiRefT* ref, const ConnApiClientT* client,
                              u32 mode)
{
    if (ref->uGameServFallback & mode)
    {
        if (!client)
            return true;
        const ConnApiConnInfoT* info = (mode == 1) ? &client->GameInfo : &client->VoipInfo;
        return (info->uGameServerFallback & mode) != 0;
    }
    return ref->uGameServAddr && (ref->uGameServConnMode & mode);
}

s32 ConnApiGetConnectAddr(ConnApiRefT* ref, ConnApiClientT* client, u32 mode)
{
    const char* connectionType = (mode & 1) ? "game" : "voip";
    if (ConnApiGameServerEnabled(ref, client, mode))
    {
        const char* fallback = (ref->uGameServFallback & mode) ?
            " as fallback " : " ";
        NetPrintfCode("connapi: [%s] using game server address%s to connect to user '%s'\n",
                      connectionType, fallback, client->UserInfo.strName);
        return static_cast<s32>(ref->uGameServAddr);
    }

    bool listenOnLocalAddress;
    if (ref->bPeerWeb)
    {
        const ConnApiClientT* self = &ref->ClientList.Clients[ref->iSelf];
        listenOnLocalAddress = std::strcmp(client->UserInfo.strTunnelKey,
                                            self->UserInfo.strTunnelKey) > 0;
    }
    else
    {
        listenOnLocalAddress = ref->bHost != 0;
    }
    s32 address = listenOnLocalAddress ? 0 : static_cast<s32>(client->UserInfo.uAddr);
    NetPrintfCode("connapi: [%s] using peer address to connect to user '%s'\n",
                  connectionType, client->UserInfo.strName);
    return address;
}

s32 ConnApiMakeConnectionName(ConnApiRefT* ref, ConnApiClientT* client,
                              char* buffer, s32 size)
{
    if (ConnApiGameServerEnabled(ref, client, 1) && ref->eGameServMode == 1)
    {
        ds_strnzcpy(buffer, ref->strSelfName, size);
        return 0x102;
    }

    const ConnApiClientT* self = &ref->ClientList.Clients[ref->iSelf];
    bool remoteFirst;
    if (ref->bPeerWeb)
        remoteFirst = std::strcmp(client->UserInfo.strTunnelKey,
                                  self->UserInfo.strTunnelKey) > 0;
    else
        remoteFirst = ref->bHost != 0;
    const char* first = remoteFirst ? client->UserInfo.strTunnelKey :
                                      self->UserInfo.strTunnelKey;
    const char* second = remoteFirst ? self->UserInfo.strTunnelKey :
                                       client->UserInfo.strTunnelKey;
    ds_snzprintf(buffer, size, "%s-%s", first, second);
    return remoteFirst ? 0x101 : 0x102;
}

s32 ConnApiCheckGameServerFallback(ConnApiRefT* ref, ConnApiClientT* client,
                                   u32 mode)
{
    ConnApiConnInfoT* info = (mode == 1) ? &client->GameInfo : &client->VoipInfo;
    const char* connectionType = (mode & 1) ? "game" : "voip";
    if ((ref->uGameServFallback & ~info->uGameServerFallback &
         ref->uGameServConnMode & mode) != 0)
    {
        NetPrintfCode("connapi: [%s] status=init, connection failed, using gameserver fallback for connection to user '%s'\n",
                      connectionType, client->UserInfo.strName);
        info->uGameServerFallback |= static_cast<u8>(mode);
        return 0;
    }

    const char* fallback = (info->uGameServerFallback & mode) ?
        " after gameserver fallback " : " ";
    NetPrintfCode("connapi: [%s] status=disc, connection failed%sfor connection to user %s\n",
                  connectionType, fallback, client->UserInfo.strName);
    return 5;
}

void ConnApiAllocTunnel(ConnApiRefT* ref, ConnApiClientT* client, s32 address)
{
    ProtoTunnelInfoT info;
    std::memset(&info, 0, sizeof(info));
    info.uRemoteClientId = client->UserInfo.uClientId;
    info.uRemoteAddr = static_cast<u32>(address);
    info.aRemotePortList[0] = ref->uGamePort;
    info.aRemotePortList[1] = ref->uVoipPort;
    info.aPortFlags[0] = 1;
    client->iTunnelId = ProtoTunnelAlloc(ref->pProtoTunnel, &info,
                                         ConnApiTunnelKey);
}

bool ConnApiUpdateGameClient(ConnApiRefT* ref, ConnApiClientT* client,
                             s32 clientIndex)
{
    const s32 oldStatus = client->GameInfo.eStatus;
    if (!(ref->uConnFlags & 1))
    {
        if (client->GameInfo.eStatus == 0 || client->GameInfo.eStatus == 5)
            return false;
        DisconnectGame(ref, client, clientIndex, "connection closed (voiponly)");
    }

    if (!client->GameInfo.eStatus)
    {
        s32 address = ConnApiGetConnectAddr(ref, client, 1);
        const u32 tick = NetTick();
        const char* addressText = SocketInAddrGetText(address);
        NetPrintfCode("connapi: establishing game connection to user %s:%s at %d\n",
                      client->UserInfo.strName, addressText, tick);
        if (ref->bTunnelEnabled && !client->iTunnelId)
            ConnApiAllocTunnel(ref, client, address);
        if (client->iTunnelId > 0)
        {
            const char* tunnelAddress = SocketInAddrGetText(client->iTunnelId);
            NetPrintfCode("connapi: tunnel allocated for client %d; switching to use virtual address %s\n",
                          clientIndex, tunnelAddress);
            address = client->iTunnelId;
        }
        const u32 connectTick = NetTick();
        const char* connectAddressText = SocketInAddrGetText(address);
        NetPrintfCode("connapi: establishing game connection to user %s:%s at %d\n",
                      client->UserInfo.strName, connectAddressText, connectTick);
        DirtyMemGroupEnter(ref->iMemGroup);
        NetGameUtilRefT* util = NetGameUtilCreate();
        client->pGameUtilRef = util;
        DirtyMemGroupLeave();
        if (util)
        {
            if (ref->iGameMinp)
                NetGameUtilControl(util, 0x6d696e70, ref->iGameMinp);
            if (ref->iGameMout)
                NetGameUtilControl(util, 0x6d6f7574, ref->iGameMout);
            if (ref->iGameMwid)
                NetGameUtilControl(util, 0x6d776964, ref->iGameMwid);
            NetGameUtilControl(util, 0x636c6964,
                               ref->ClientList.Clients[ref->iSelf].UserInfo.uClientId);
            NetGameUtilControl(util, 0x72636964, client->UserInfo.uClientId);

            char connectionName[64];
            const s32 connectionType = ConnApiMakeConnectionName(
                ref, client, connectionName, sizeof(connectionName));
            char connectString[128];
            if ((connectionType & 2) || ref->uGameServAddr)
            {
                ds_snzprintf(connectString, sizeof(connectString), "%s:%d:%d#%s",
                             SocketInAddrGetText(address), ref->uGamePort,
                             ref->uGamePort, connectionName);
            }
            else
            {
                ds_snzprintf(connectString, sizeof(connectString), ":%d:%d#%s",
                             ref->uGamePort, ref->uGamePort, connectionName);
            }
            NetGameUtilConnect(util, connectionType, connectString);
            client->GameInfo.eStatus = 1;
            if (static_cast<u32>(address) == ref->uGameServAddr)
                client->GameInfo.uConnFlags |= 2;
            client->uReservedAC = NetTick();
        }
        else
        {
            NetPrintfCode("connapi: unable to allocate util ref for connection %d to user %s\n",
                          clientIndex, client->UserInfo.strName);
            client->GameInfo.eStatus = 5;
        }
    }

    if (client->GameInfo.eStatus == 1)
    {
        void* stream = NetGameUtilComplete(
            static_cast<NetGameUtilRefT*>(client->pGameUtilRef));
        if (stream)
        {
            DirtyMemGroupEnter(ref->iMemGroup);
            client->pGameLinkRef = NetGameLinkCreate(stream, 0, ref->iLinkBufSize);
            DirtyMemGroupLeave();
            if (client->pGameLinkRef)
            {
                NetPrintfCode("connapi: game connection %d to user %s established\n",
                              clientIndex, client->UserInfo.strName);
                if (!ref->uGameServAddr)
                {
                    client->UserInfo.uAddr = static_cast<u32>(NetGameUtilStatus(
                        static_cast<NetGameUtilRefT*>(client->pGameUtilRef),
                        0x70656970, 0, 0));
                }
                client->GameInfo.uMnglPort = static_cast<u16>(NetGameUtilStatus(
                    static_cast<NetGameUtilRefT*>(client->pGameUtilRef),
                    0x70707274, 0, 0));
                client->GameInfo.eStatus = 3;
                client->GameInfo.uConnFlags |= 1;
            }
            else
            {
                NetPrintfCode("connapi: unable to allocate link ref for connection %d to user %s\n",
                              clientIndex, client->UserInfo.strName);
                client->GameInfo.eStatus = 5;
            }
        }
        if (static_cast<s32>(NetTick() - client->uReservedAC) > 10000)
        {
            DisconnectGame(ref, client, clientIndex, "connection timeout");
            client->GameInfo.eStatus = ConnApiCheckGameServerFallback(
                ref, client, 1);
        }
    }

    if (client->GameInfo.eStatus == 3)
    {
        const NetGameLinkStatT* stats = NetGameLinkStatus(client->pGameLinkRef);
        if (stats->isopen)
        {
            const s32 elapsed = static_cast<s32>(stats->tick - stats->rcvdlast);
            if (elapsed > ref->iTimeout)
                DisconnectGame(ref, client, clientIndex, "connection timed out");
        }
        else
        {
            DisconnectGame(ref, client, clientIndex, "connection closed");
        }
    }

    ConnApiNotify(ref, clientIndex, 0, oldStatus, client->GameInfo.eStatus);
    return client->GameInfo.eStatus != 5;
}

bool ConnApiUpdateVoipClient(ConnApiRefT* ref, ConnApiClientT* client,
                             s32 clientIndex)
{
    const s32 oldStatus = client->VoipInfo.eStatus;
    if (!(ref->uConnFlags & 2))
    {
        if (client->VoipInfo.eStatus == 0 || client->VoipInfo.eStatus == 5)
            return false;
        const u32 tick = NetTick();
        NetPrintfCode("connapi: destroying voip connection to user %s: %s at %d\n",
                      client->UserInfo.strName,
                      "connection closed (gameonly)", tick);
        if (client->iVoipConnId >= 0)
        {
            VoipDisconnect(ref->pVoipRef, client->iVoipConnId);
            client->iVoipConnId = -1;
        }
        client->VoipInfo.eStatus = 5;
    }

    if (!client->VoipInfo.eStatus)
    {
        s32 address = ConnApiGetConnectAddr(ref, client, 2);
        const u32 tick = NetTick();
        const char* addressText = SocketInAddrGetText(address);
        NetPrintfCode("connapi: establishing voip connection to user %s:%s at %d\n",
                      client->UserInfo.strName, addressText, tick);
        if (ref->bTunnelEnabled && !client->iTunnelId)
            ConnApiAllocTunnel(ref, client, address);
        if (client->iTunnelId > 0)
        {
            const char* tunnelAddress = SocketInAddrGetText(client->iTunnelId);
            NetPrintfCode("connapixenon: tunnel allocated for client %d; switching to use virtual address %s\n",
                          clientIndex, tunnelAddress);
            address = client->iTunnelId;
        }
        // VoipConnect4 takes the XUID text and the mangle port before the local port.
        const s32 connId = VoipConnect4(ref->pVoipRef, clientIndex,
            client->UserInfo.DirtyAddr.strMachineAddr,
            static_cast<u32>(address), client->VoipInfo.uMnglPort,
            client->VoipInfo.uLocalPort, client->UserInfo.uClientId, 0);
        if (connId < 0)
        {
            NetPrintfCode("connapi: unable to init voip for connection %d to user %s\n",
                          clientIndex, client->UserInfo.strName);
            client->VoipInfo.eStatus = 5;
        }
        else
        {
            client->iVoipConnId = static_cast<s16>(connId);
            client->VoipInfo.eStatus = 1;
            client->VoipInfo.uConnFlags |= 2;
        }
    }

    if (client->VoipInfo.eStatus == 1 &&
        (VoipRemote(ref->pVoipRef, client->iVoipConnId) & 1))
    {
        NetPrintfCode("connapi: voip connection %d to user %s established\n",
                      clientIndex, client->UserInfo.strName);
        client->VoipInfo.eStatus = 3;
        client->VoipInfo.uConnFlags |= 1;
    }

    if ((client->VoipInfo.eStatus == 1 || client->VoipInfo.eStatus == 3) &&
        (VoipRemote(ref->pVoipRef, client->iVoipConnId) & 0x80))
    {
        NetPrintfCode("connapi: voip connection to user %s closed\n",
                      client->UserInfo.strName);
        client->VoipInfo.eStatus = static_cast<u8>(
            ConnApiCheckGameServerFallback(ref, client, 2));
        if (client->VoipInfo.eStatus == 5)
            client->iVoipConnId = -1;
    }

    ConnApiNotify(ref, clientIndex, 1, oldStatus, client->VoipInfo.eStatus);
    return client->VoipInfo.eStatus != 5;
}

void ConnApiRemoveClientEntry(ConnApiRefT* ref, ConnApiClientT* client,
                              s32 clientIndex)
{
    const s32 oldGameStatus = client->GameInfo.eStatus;
    const s32 oldVoipStatus = client->VoipInfo.eStatus;
    ProtoMangleControl(ref->pProtoMangle, 0x72656d76, 0, 0,
                       &client->UserInfo.DirtyAddr);
    DisconnectClient(ref, client, clientIndex, "removal");
    if (ref->bRemoveCallback == 1)
    {
        if (ref->uConnFlags & 1)
            ConnApiNotify(ref, clientIndex, 0, oldGameStatus,
                          client->GameInfo.eStatus);
        if (ref->uConnFlags & 2)
            ConnApiNotify(ref, clientIndex, 1, oldVoipStatus,
                          client->VoipInfo.eStatus);
    }

    if (ref->pVoipRef)
    {
        VoipRemove(ref->pVoipRef, clientIndex);
        const s32 numClients = ref->ClientList.iNumClients;
        if (clientIndex != numClients - 1)
        {
            std::memmove(client, client + 1,
                         sizeof(*client) * (numClients - clientIndex - 1));
        }
        for (s32 i = 0; i < numClients - 1; ++i)
            ref->ClientList.Clients[i].iVoipConnId = static_cast<s16>(i);
    }

    --ref->ClientList.iNumClients;
    ref->iSelf = -1;
    for (s32 i = 0; i < ref->ClientList.iNumClients; ++i)
    {
        if (std::strcmp(ref->ClientList.Clients[i].UserInfo.strName,
                        ref->strSelfName) == 0)
        {
            ref->iSelf = i;
            break;
        }
    }
}

void ConnApiUpdateRemoval(ConnApiRefT* ref)
{
    s32 i = 0;
    while (i < ref->ClientList.iNumClients)
    {
        ConnApiClientT* client = &ref->ClientList.Clients[i];
        if (client->uFlags & 1)
        {
            ConnApiRemoveClientEntry(ref, client, i);
            --i;
        }
        ++i;
    }
}

s32 ConnApiUpdateSessionCreate(ConnApiRefT* ref)
{
    s32 status = ProtoMangleStatus(ref->pProtoMangle, 0x73657373,
                                   ref->strSession, sizeof(ref->strSession));
    if (status > 0)
    {
        NetPrintfCode("connapi: session creation successful\n");
        if (ref->bHost == 1)
            ConnApiNotify(ref, 0, 2, 0, 3);
        ref->eState = (ref->ClientList.iNumClients > 1) ? 2 : 3;
    }
    else if (status < 0)
    {
        NetPrintfCode("connapi: session creation failed\n");
        ref->eState = 0;
    }
    return status;
}

s32 ConnApiUpdateSessionConnect(ConnApiRefT* ref)
{
    if (!ProtoMangleStatus(ref->pProtoMangle, 0x69646c65, 0, 0))
    {
        NetPrintfCode("connapi: waiting for async session create to complete\n");
        return 0;
    }

    s32 oldGameStatus[32];
    s32 oldVoipStatus[56];
    const s32 numClients = ref->ClientList.iNumClients;
    for (s32 i = 0; i < numClients; ++i)
    {
        oldGameStatus[i] = ref->ClientList.Clients[i].GameInfo.eStatus;
        oldVoipStatus[i] = ref->ClientList.Clients[i].VoipInfo.eStatus;
    }

    s32 resolved = 0;
    s32 i = 0;
    for (; i < numClients; ++i)
    {
        if (i == ref->iSelf)
            continue;
        ConnApiClientT* client = &ref->ClientList.Clients[i];
        if (client->UserInfo.uAddr)
        {
            ++resolved;
            continue;
        }
        if (!client->GameInfo.eStatus || !client->VoipInfo.eStatus)
        {
            ProtoMangleConnect(ref->pProtoMangle, ref->uGamePort,
                               client->UserInfo.DirtyAddr.strMachineAddr);
            client->GameInfo.eStatus = 2;
            client->VoipInfo.eStatus = 2;
        }
        if (client->GameInfo.eStatus == 2)
        {
            // ARTIST writes the secure address to UserInfo.uAddr (+0); no port output.
            const s32 complete = ProtoMangleComplete(
                ref->pProtoMangle,
                reinterpret_cast<s32*>(&client->UserInfo.uAddr), 0);
            if (complete > 0)
            {
                ++resolved;
                client->GameInfo.eStatus = 0;
                client->VoipInfo.eStatus = 0;
            }
            else if (complete < 0)
            {
                client->GameInfo.eStatus = 5;
                client->VoipInfo.eStatus = 5;
            }
            else
            {
                break;
            }
        }
    }

    if (i == numClients)
    {
        if (resolved <= 0)
        {
            NetPrintfCode("connapi: unable to demangle any clients\n");
            ref->eState = 0;
        }
        else
        {
            NetPrintfCode("connapi: resolved %d secure addresses\n", resolved);
            ref->eState = ref->bHost ? 3 : 4;
        }
    }

    for (s32 clientIndex = 0; clientIndex < numClients; ++clientIndex)
    {
        ConnApiClientT* client = &ref->ClientList.Clients[clientIndex];
        ConnApiNotify(ref, clientIndex, 0, oldGameStatus[clientIndex],
                      client->GameInfo.eStatus);
        ConnApiNotify(ref, clientIndex, 1, oldVoipStatus[clientIndex],
                      client->VoipInfo.eStatus);
    }
    return 1;
}

s32 ConnApiUpdateConnections(ConnApiRefT* ref)
{
    s32 updated = 0;
    for (s32 i = 0; i < ref->ClientList.iNumClients; ++i)
    {
        ConnApiClientT* client = &ref->ClientList.Clients[i];
        if (i != ref->iSelf &&
            (ref->eState == 3 || (client->uConnFlags & 1)))
        {
            updated += ConnApiUpdateGameClient(ref, client, i);
        }
    }
    if (ref->pVoipRef)
    {
        for (s32 i = 0; i < ref->ClientList.iNumClients; ++i)
        {
            if (i != ref->iSelf)
                updated += ConnApiUpdateVoipClient(ref,
                    &ref->ClientList.Clients[i], i);
        }
    }
    if (ref->bTunnelEnabled)
        ProtoTunnelUpdate(ref->pProtoTunnel);
    return updated;
}
}

extern "C" void ConnApiHost(ConnApiRefT* ref, ConnApiUserInfoT* clientList,
                             s32 numClients, s32, s32 sessionFlags)
{
    if (ref->eState)
    {
        NetPrintfCode("connapi: can't host a game when not in idle state\n");
        return;
    }
    ref->uSessionFlags = static_cast<u32>(sessionFlags);
    ConnApiInitializeClientList(ref, clientList, numClients);
    ConnApiUpdateSessionParams(ref, 1);
    if (ProtoMangleControl(ref->pProtoMangle, 0x73657373,
                           ref->uSessionFlags & 1, 0, 0) == 0)
    {
        ref->bHost = 1;
        ref->eState = 1;
    }
}

extern "C" void ConnApiConnect(ConnApiRefT* ref, ConnApiUserInfoT* clientList,
                                s32 numClients, s32, s32 sessionFlags)
{
    if (ref->eState)
    {
        NetPrintfCode("connapi: can't connect to a game when not in idle state\n");
        return;
    }
    ref->uSessionFlags = static_cast<u32>(sessionFlags);
    ConnApiInitializeClientList(ref, clientList, numClients);
    ConnApiUpdateSessionParams(ref, 0);
    if (ProtoMangleControl(ref->pProtoMangle, 0x73657373,
                           ref->uSessionFlags & 1, 0, ref->strSession) == 0)
    {
        ref->bHost = 0;
        ref->eState = 1;
    }
}

extern "C" void ConnApiAddClient(ConnApiRefT* ref, ConnApiUserInfoT* userInfo)
{
    if (!ref->eState)
    {
        NetPrintfCode("connapi: can't add a connection to a game in idle state\n");
        return;
    }
    if (ref->ClientList.iNumClients == ref->ClientList.iMaxClients)
    {
        NetPrintfCode("connapi: can't add a connection to the game because it is full\n");
        return;
    }

    const s32 clientIndex = ref->ClientList.iNumClients;
    ConnApiClientT* client = &ref->ClientList.Clients[clientIndex];
    ConnApiInitializeClient(ref, client, userInfo);
    NetPrintfCode("connapi: adding client to clientlist\n");
    NetPrintfCode("connapi:   %d) %s %s <%s>\n", clientIndex,
                  SocketInAddrGetText(static_cast<s32>(client->UserInfo.uAddr)),
                  client->UserInfo.strName,
                  client->UserInfo.DirtyAddr.strMachineAddr);
    ref->eState = 2;
    ref->ClientList.iNumClients = clientIndex + 1;
}

extern "C" void ConnApiRemoveClient(ConnApiRefT* ref, const char* clientName,
                                     s32 clientIndex)
{
    if (clientName)
    {
        clientIndex = -1;
        for (s32 i = 0; i < ref->ClientList.iNumClients; ++i)
        {
            if (std::strcmp(ref->ClientList.Clients[i].UserInfo.strName,
                            clientName) == 0)
            {
                clientIndex = i;
                break;
            }
        }
        if (clientIndex < 0)
        {
            NetPrintfCode("connapi: can't find client '%s' in list to remove them\n",
                          clientName);
            return;
        }
    }
    if (clientIndex == ref->iSelf)
    {
        NetPrintfCode("connapi: can't remove self from game\n");
        return;
    }

    ref->ClientList.Clients[clientIndex].uFlags |= 1;
    if (!ref->bInCallback)
        ConnApiUpdateRemoval(ref);
}

extern "C" void ConnApiStart(ConnApiRefT* ref, s32)
{
    if (ref->eState == 3 || ref->eState == 4)
    {
        ProtoMangleControl(ref->pProtoMangle, 0x73747274,
                           ref->ClientList.iNumClients, 0, 0);
    }
    else
    {
        NetPrintfCode("connapi: can't start game when not in hosting or joining state\n");
    }
}

extern "C" void ConnApiUpdate(ConnApiRefT* ref)
{
    ConnApiSetClientFlags(ref);
    if (ref->pProtoMangle)
        ProtoMangleUpdate(ref->pProtoMangle);

    if (ref->eState == 1)
        ConnApiUpdateSessionCreate(ref);
    if (ref->eState == 2)
        ConnApiUpdateSessionConnect(ref);

    if (ref->eState == 3 || ref->eState == 4)
    {
        if (ConnApiGameServerEnabled(ref, 0, 1) && ref->eGameServMode)
        {
            if (ref->ClientList.iNumClients > 0)
                ConnApiUpdateGameClient(ref, &ref->GameServer, 0);
            if (ref->pVoipRef)
            {
                for (s32 i = 0; i < ref->ClientList.iNumClients; ++i)
                {
                    if (i != ref->iSelf)
                        ConnApiUpdateVoipClient(ref,
                            &ref->ClientList.Clients[i], i);
                }
            }
            if (ref->bTunnelEnabled)
                ProtoTunnelUpdate(ref->pProtoTunnel);
        }
        else
        {
            ConnApiUpdateConnections(ref);
        }
    }

    if (ref->eState == 5 &&
        (ProtoMangleStatus(ref->pProtoMangle, 0x69646c65, 0, 0) == 1 ||
         ProtoMangleStatus(ref->pProtoMangle, 0x6661696c, 0, 0) == 1))
    {
        ConnApiNotify(ref, 0, 2, 3, 5);
        ref->eState = 0;
    }
    ConnApiUpdateRemoval(ref);
}
