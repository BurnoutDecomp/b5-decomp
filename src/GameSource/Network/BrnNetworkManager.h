#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Network/CgsNetworkConstants.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h"
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"   // BrnNetwork::NetworkPlayerID (committed typedef)
#include "GameSource/Network/BrnServerInterface.h"            // BrnNetwork::BrnServerInterface (embedded by value)
#include "GameShared/GameClasses/Network/Packeting/CgsNetworkAdapterBase.h" // CgsNetwork::NetworkAdapter (canonical home; embedded mNetworkAdapter)

namespace CgsNetwork
{
    // Pointer-only use below (the network-player-id pack/unpack helper takes a Message*);
    // forward-declared to avoid pulling the whole message hierarchy into this header.
    struct Message;
    // Pointer-only return of GetPlayerManager(); forward-declared to avoid pulling the whole
    // player registry into this header.
    struct PlayerManager;
}

namespace BrnNetwork
{
    class NetworkServers;
    struct LiveRevengeRelationship;   // pointer-only (own header)

    // The online live-revenge bookkeeping (per-rival relationship table). MINIMAL SLICE:
    // the BrnNetworkAggressiveDrivingManager TU only needs to look up the mutable relationship
    // for a given network player and read its current point-of-view score; the full layout and
    // the remaining API live in the LiveRevengeManager's own TU.
    // ADDITIVE GROW (BrnNetworkAggressiveDrivingManager TU): declared-only.
    class LiveRevengeManager
    {
    public:
        // X360 BrnNetwork::LiveRevengeManager::GetNonConstRevengeRelation: returns the writable
        // relationship record for lNetworkPlayerID (or nullptr when there is none). Declared-only.
        LiveRevengeRelationship* GetNonConstRevengeRelation(NetworkPlayerID lNetworkPlayerID);
    };
}

namespace CgsNetwork
{
    class VersionDisplay
    {
        friend class BrnNetwork::NetworkServers;

    private:
        const char* mpcVersion;
        EServerType meServerType;
    };

    // NetworkAdapter is the canonical struct homed in CgsNetworkAdapterBase.h (included above);
    // reused by name here as the embedded mNetworkAdapter member. (The earlier stale inline
    // class definition here was an ODR duplicate now that the canonical home is included.)
}

namespace BrnNetwork
{
    class BrnNetworkManager
    {
        friend class NetworkServers;

    public:
        // CgsMessage.h:85 -- per-field (de)serialise status. 0 == success; callers OR the
        // per-field results together into the message's overall result (DWARF
        // BrnNetworkManager.h:373 returns this type).
        typedef u8 PackOrUnpackResult;

        // BrnNetworkManager.h:373 (X360 @ 0x82881xxx static helper): (de)serialise one
        // NetworkPlayerID field through a message's bitstream, mirroring the shared
        // CgsNetwork::PackOrUnpack* field primitives. The X360 call site passes only the
        // message and the field pointer (no manager `this`), so this is a static helper.
        // Returns the per-field status (0 == success).
        static PackOrUnpackResult PackOrUnpack(CgsNetwork::Message* lpMessage,
                                               NetworkPlayerID* lpNetworkPlayerID);

        BrnServerInterface* GetServerInterface()
        {
            return &mServerInterface;
        }

        const BrnServerInterface* GetServerInterface() const
        {
            return &mServerInterface;
        }

        // The embedded NetworkServers sub-object (X360: the debug component reaches it as
        // *(this+271820), i.e. an inlined accessor on the full manager). Declared-only here; the
        // minimal manager slice does not yet materialise the storage (body lands with the full
        // BrnNetworkManager TU).
        NetworkServers* GetNetworkServers();

        // The embedded session player registry (X360: reliable-message registrars take its
        // address as &GetNetworkManager()->mpPlayerManager, e.g.
        // BrnNetwork::SelectedRoutesManager::AddPlayer/RemovePlayer @ 0x8255BDA0/0x8255BEC8).
        // Declared-only here; the minimal manager slice does not yet materialise the storage
        // (body lands with the full BrnNetworkManager TU).
        CgsNetwork::PlayerManager* GetPlayerManager();

        // The running network send-frame counter (X360: *(this+0x3658), read whole then taken
        // modulo 0xFFFF to derive a reliable-message frame id; see
        // BrnNetwork::MarkedManManager::SendMarkedManDataToAll @ 0x82548DCC). Declared-only here;
        // the storage materialises with the full BrnNetworkManager TU.
        u32 GetCurrentFrame() const;

        // The embedded live-revenge manager (X360: AddTakedownEvent reaches it as
        // &GetNetworkManager()->mpLiveRevengeManager). Declared-only; storage lands with the full
        // BrnNetworkManager TU. ADDITIVE GROW (BrnNetworkAggressiveDrivingManager TU).
        LiveRevengeManager* GetLiveRevengeManager();

    private:
        CgsNetwork::VersionDisplay mVersionDisplay;
        CgsNetwork::NetworkAdapter mNetworkAdapter;
        BrnServerInterface mServerInterface;
    };
}
