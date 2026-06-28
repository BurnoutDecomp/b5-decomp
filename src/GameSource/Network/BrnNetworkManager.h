#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Network/CgsNetworkConstants.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h"
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"   // BrnNetwork::NetworkPlayerID (committed typedef)

namespace CgsNetwork
{
    // Pointer-only use below (the network-player-id pack/unpack helper takes a Message*);
    // forward-declared to avoid pulling the whole message hierarchy into this header.
    struct Message;
}

namespace BrnNetwork
{
    class NetworkServers;

    class BrnServerInterface
    {
    public:
        enum EStatus
        {
            E_STATUS_BUSY = 0,
            E_STATUS_ERROR,
            E_STATUS_IDLE,
            E_STATUS_COUNT
        };

        EStatus GetStatus(s32 liComponent) const;
        bool IsSuspended() const;
        void Suspend(s32 liUpdateFlags);
        void Resume();
        CgsNetwork::ServerInterfaceConnection* GetConnectionComponent();
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

    class NetworkAdapter
    {
    public:
        virtual void SetServerType(EServerType leServerType);
    };
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

    private:
        CgsNetwork::VersionDisplay mVersionDisplay;
        CgsNetwork::NetworkAdapter mNetworkAdapter;
        BrnServerInterface mServerInterface;
    };
}
