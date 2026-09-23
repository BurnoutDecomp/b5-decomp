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

namespace CgsNetwork
{
    struct NetworkAdapterPrepareParams;   // defined below; Prepare() takes it by pointer.

    // --- The "fake network conditions" buffered-message connection block -----------------
    // The DWARF types the address payload a NetworkPlayer sends through as
    // FakeNetworkConditions::BufferedMessageData::ConnectionData (passed BY VALUE into
    // SendTo). NetworkPlayer carries a copy of it (its mConnectionData member). It is an
    // opaque sized value block here -- this TU only stores/copies it whole; the field-level
    // shape belongs to the FakeNetworkConditions TU. The X360 SendTo glue reads 0x48 (72)
    // bytes of it off the stack copy (5 doublewords), so it is modelled as a 0x70-byte value
    // (the size NetworkPlayer reserves) carried by value. FLAGGED: opaque sized placeholder.
    // This is the canonical home for the type; NetworkPlayer's mConnectionData reuses it.
    struct ConnectionData
    {
        u8  maReserved00[0x28];
        // How the game and voice traffic reach this peer (the lobby status writer copies
        // both words out of a player's connection data).
        s32 meGameConnectionType;   // +0x28
        s32 meVoipConnectionType;   // +0x2C
        u8  maReserved30[0x40];
    };

    // --- The network adapter base (UDP/DirtySock send path) ------------------------------
    // SHAPE/method from the DecFIGS DWARF (CgsNetworkAdapterBase.h:129-222), gated against the
    // ARTIST binary. Polymorphic base: the X360/PS3 concrete adapter overrides Prepare /
    // Update / Release / SetServerType, and CgsNetwork::NetworkManager dispatches through the
    // vtable (the X360 adapter stores a vtable at +0x00, confirmed by the asm). The member
    // layout below is the X360-attested offset map (the asm reads mpNetworkManager @+0x08,
    // mpServerInterface @+0x0C, meServerType @+0x18, mbDuplicateLogin @+0x1C; concrete adapter
    // members begin @+0x28). The fake-network-conditions / last-error region between
    // mpServerInterface and meServerType is NOT individually attested by the X360 binary, so it
    // is reserved as an explicit padding buffer rather than fabricating its field shape -- the
    // FakeNetworkConditions debug block belongs to its own home. FLAGGED: mPad10 is an
    // offset-preserving reserve, not a recovered field layout.
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

        // Construct is a plain initialiser (not virtual on the base; the X360 ctor stores the
        // vtable then calls this). Prepare / Update / Release are virtual (the concrete adapter
        // overrides them). DWARF :153/:159/:168/:171/:174.
        void                          Construct();
        virtual ENetworkStatus        Prepare(NetworkAdapterPrepareParams* lpParams);
        virtual void                  Update();
        virtual bool                  Release();

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

        bool HadDuplicateLogin() const;

        // --- layout (X360 asm offsets) ----------------------------------------------------
        // +0x00 is the vtable pointer (implicit, from the virtuals above).
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
        void*           mpHeapMalloc;        // +0x20
        u8*             mpRecvBuffer;        // +0x24
        // ...concrete adapter members continue from +0x28.
    };

    struct NetworkAdapter : NetworkAdapterBase
    {
        // Selects which server flavour this adapter talks to. DWARF NetworkAdapter.h:
        // `virtual void SetServerType(EServerType)`; the X360 NetworkServers::SetServerType
        // forwards the chosen type here (BrnNetworkServers.cpp). Declared-only; bodied in the
        // adapter's own TU.
        virtual void SetServerType(EServerType leServerType);
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
