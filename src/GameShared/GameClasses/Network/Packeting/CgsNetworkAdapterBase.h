#pragma once

// ===================================================================================
// CgsNetwork::NetworkAdapterPrepareParams -- owning header
//   b5-decomp/src/GameShared/GameClasses/Network/Packeting/CgsNetworkAdapterBase.h
//
// The prepare-params block passed (by pointer) into the network-adapter Prepare chain
// (CgsNetworkAdapterBase.h in the X360 build); populated by Construct @ 0x82581330 and
// consumed by BrnNetwork::BrnNetworkManager::Prepare.
//
// LAYOUT (X360 asm @ 0x82581330 Construct -- five word stores, used size 0x14 / 20B):
//   +0x00  the 6th Construct argument (title id)   -- muTitleID
//   +0x04  leServerType (E_SERVER_TYPE_*)           -- meServerType
//   +0x08  lpHeapMalloc                             -- mpHeapMalloc
//   +0x0C  lpNetworkManager                         -- mpNetworkManager
//   +0x10  the 5th Construct argument               -- mpServerInterface
//
// Construct bounds-checks leServerType in [E_SERVER_TYPE_LOCAL, E_SERVER_TYPE_COUNT) and
// non-null lpHeapMalloc / lpNetworkManager, then writes the five words in this order. The
// word at +0x00 is the platform title id (the network manager passes a 32-bit immediate),
// and the word at +0x10 is the server interface the base Prepare copies into its
// mpServerInterface. The struct is non-polymorphic (Construct takes `this` as a plain
// pointer; no vtable store).
//
// E_SERVER_TYPE_LOCAL == 0, E_SERVER_TYPE_COUNT == 7 (from the asm bounds blt 0 / blt 7).
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/CgsNetworkConstants.h"   // CgsNetwork::EServerType
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"   // CgsNetwork::ServerInterfaceDirtySock (mpServerInterface's real type)

#include "netgamelink.h"   // NetGameLinkRefT, NetGamePacketT

namespace CgsMemory
{
    class HeapMalloc;
}

namespace CgsNetwork
{
    struct NetworkAdapterPrepareParams;   // defined below; Prepare() takes it by pointer.

    // --- How a NetworkPlayer reaches its peer --------------------------------------------
    // The address block a NetworkPlayer sends and receives through (passed BY VALUE into
    // SendTo / ReceiveFrom); NetworkPlayer and each PlayersConnectionManager entry carry a
    // copy. PlayersConnectionManager::UpdatePlayerList is its only filler: once a peer's
    // ConnApi game connection is active it copies the peer's ConnApiClientT fields into
    // it. 0x70 console bytes. This is the canonical home for the type.
    //
    // Host note: mpNetGameLink is 8 bytes on the host (4 on the console), so every member
    // after it sits 4 bytes later and the host block is larger than 0x70. Offsets below are
    // the console's; read and write the block only by member name.
    struct ConnectionData
    {
        NetGameLinkRefT* mpNetGameLink;          // +0x00  the peer's game link (null = no link)
        s32              miIPAddress;            // +0x04  external address (ConnApi UserInfo.uAddr)
        s32              miLocalIPAddress;       // +0x08  internal address (UserInfo.uLocalAddr)
        u32              muGamePort;             // +0x0C  UserInfo.uLocalGamePort
        u32              muVoipPort;             // +0x10  UserInfo.uLocalVoipPort
        u32              muLocalGamePort;        // +0x14  GameInfo.uLocalPort
        u32              muMnglGamePort;         // +0x18  GameInfo.uMnglPort (demangled port)
        u32              muLocalVoipPort;        // +0x1C  VoipInfo.uLocalPort
        u32              muMnglVoipPort;         // +0x20  VoipInfo.uMnglPort
        u32              muConnApiClientId;      // +0x24  UserInfo.uClientId
        // How the game and voice traffic reach this peer: bit 1 of the matching ConnApi
        // connection's uConnFlags (the lobby status writer copies both words out).
        s32              meGameConnectionType;   // +0x28
        s32              meVoipConnectionType;   // +0x2C
        // Zeroed by Clear, never written by the filler (UpdatePlayerList copies whatever
        // its stack block held there).
        u8               maReserved30[0x40];     // +0x30 .. +0x6F

        // SendTo asserts "lConnectionData.IsValid()" on a null link; the receive loop skips
        // a player whose link is null.
        bool IsValid() const { return mpNetGameLink != nullptr; }

        // Inlined into NetworkPlayer's Construct / Prepare / Release: no link, both
        // addresses and the client id -1, every port and connection type 0, tail zeroed.
        void Clear()
        {
            mpNetGameLink        = nullptr;
            miIPAddress          = -1;
            miLocalIPAddress     = -1;
            muConnApiClientId    = static_cast<u32>(-1);
            muGamePort           = 0;
            muVoipPort           = 0;
            muLocalGamePort      = 0;
            muMnglGamePort       = 0;
            muLocalVoipPort      = 0;
            muMnglVoipPort       = 0;
            meGameConnectionType = 0;
            meVoipConnectionType = 0;
            for (s32 liIndex = 0; liIndex < static_cast<s32>(sizeof(maReserved30)); ++liIndex)
            {
                maReserved30[liIndex] = 0;
            }
        }
    };

    // --- The network adapter base (UDP/DirtySock send path) ------------------------------
    // The platform-independent half of the adapter: the receive buffer, the send / receive
    // calls onto the peer's game link, and the state the platform adapter shares. The network
    // manager holds the platform adapter by value and calls its lifecycle directly; the only
    // virtual is SetServerType. Member offsets below are the console's (mpNetworkManager +0x08,
    // mpServerInterface +0x0C, meServerType +0x18, mbDuplicateLogin +0x1C; the platform
    // adapter's members begin at +0x28).
    struct NetworkAdapterBase
    {
        // DWARF CgsNetworkAdapterBase.h:132 / :145 -- the adapter status + error enums.
        enum ENetworkStatus
        {
            E_NET_STATUS_NONE  = 0,
            E_NET_STATUS_BUSY  = 1,
            E_NET_STATUS_READY = 2,
            E_NET_STATUS_ERROR = 3,
            E_NET_STATUS_COUNT = 4,
        };

        enum ENetworkError
        {
            E_NET_ERROR_NONE                   = 0,
            E_NET_ERROR_FAILED_TO_UP_INTERFACE = 1,
            E_NET_ERROR_COUNT                  = 2,
        };

        // Plain (non-virtual) lifecycle: the platform adapter calls each of these from its own
        // Construct / Prepare / Update / Release. The platform adapter's only virtual is
        // SetServerType (its vtable has that one slot).
        void           Construct();
        ENetworkStatus Prepare(NetworkAdapterPrepareParams* lpParams);
        // Empty: the platform Update's base-call bracket times nothing.
        void           Update() {}
        bool           Release();

        // Inlined into the network manager's Destruct: forget the manager, the server type,
        // both buffers and the duplicate-login latch.
        void Destruct()
        {
            meServerType     = E_SERVER_TYPE_COUNT;
            mpNetworkManager = nullptr;
            mpRecvBuffer     = nullptr;
            mpHeapMalloc     = nullptr;
            mbDuplicateLogin = false;
        }

        ENetworkError                 GetLastError();

        // Send liLength bytes of lpData to the peer described by lConnectionData. Returns
        // true on success (the pump flags the player paused on the first failure). DWARF :191.
        bool SendTo(void* lpData, s32 liLength, ConnectionData lConnectionData);

        // Pull the next received packet: *lppData points into the receive buffer; returns
        // the packet length (0 when nothing is pending).
        s32  ReceiveFrom(void** lppData, ConnectionData lConnectionData);

        // Inlined at the network manager's disconnect handler (a byte load of the latch).
        bool HadDuplicateLogin() const { return mbDuplicateLogin; }

        // The send-time monitor every adapter shares (registered once, -1 until then).
        static s32 miSendPerfMon;

        // --- layout (X360 asm offsets) ----------------------------------------------------
        // +0x00 is the platform adapter's vtable pointer.
        void* mpMessageSentCallbackFunction; // +0x04
        void* mpNetworkManager;              // +0x08  (NetworkManager*; the concrete Brn manager
                                             //        type -- BrnNetwork::BrnNetworkManager --
                                             //        lives above this header and would cycle back
                                             //        through it, so this member stays void* here
                                             //        and subclasses that need it cast to the named
                                             //        type -- e.g. NetworkAdapterX360::Update casts
                                             //        to BrnNetwork::BrnNetworkManager* and calls
                                             //        its GetLocalUserControllerPort() accessor
                                             //        (the X360 +0x60 active-user index) instead of
                                             //        an offset-cast read)
        ServerInterfaceDirtySock* mpServerInterface;   // +0x0C -- the DirtySock server-interface
                                             //        facade (CgsNetwork::ServerInterface derives
                                             //        from ServerInterfaceDirtySockX360 <-
                                             //        ServerInterfaceDirtySock; see
                                             //        CgsServerInterface.h). Named-typed so
                                             //        subclasses (e.g. NetworkAdapterX360::Update)
                                             //        reach its components via GetConnectionComponent()
                                             //        etc. instead of raw offset casts.
        // The fake-network-conditions block: 4 console bytes that no console function
        // touches (the debug conditions simulator is not built in); kept as storage.
        u8              maNetworkConditions[4]; // +0x10
        ENetworkError   meLastError;         // +0x14  (Construct stores E_NET_ERROR_NONE)
        EServerType     meServerType;        // +0x18
        bool            mbDuplicateLogin;    // +0x1C
        u8              mPad1D[3];           // +0x1D  pad to word
        CgsMemory::HeapMalloc* mpHeapMalloc;   // +0x20
        u8*             mpRecvBuffer;        // +0x24  one NetGamePacketT
        // ...concrete adapter members continue from +0x28.
    };

    struct NetworkAdapter : NetworkAdapterBase
    {
        // Selects which server flavour this adapter talks to; the network servers block
        // forwards the chosen type here through the vtable. Only the platform adapter has a
        // body (its vtable's single slot).
        virtual void SetServerType(EServerType leServerType) = 0;
    };

    struct NetworkAdapterPrepareParams
    {
        enum EServerType
        {
            E_SERVER_TYPE_LOCAL = 0,
            E_SERVER_TYPE_COUNT = 7,
        };

        // @ 0x82581330 -- populate the param block (returns `this`).
        // Param order matches the X360 register order: leServerType, lpHeapMalloc,
        // lpNetworkManager, the server interface (stored at +0x10), then the title id
        // (stored at +0x00).
        NetworkAdapterPrepareParams* Construct(u32 leServerType, void* lpHeapMalloc,
                                               void* lpNetworkManager,
                                               ServerInterfaceDirtySock* lpServerInterface,
                                               u32 luTitleID);

        u32   muTitleID;         // +0x00  the platform title id
        u32   meServerType;      // +0x04  EServerType
        void* mpHeapMalloc;      // +0x08
        void* mpNetworkManager;  // +0x0C
        // +0x10: NetworkAdapterBase::Prepare copies this word into its mpServerInterface.
        ServerInterfaceDirtySock* mpServerInterface;
    };
}
