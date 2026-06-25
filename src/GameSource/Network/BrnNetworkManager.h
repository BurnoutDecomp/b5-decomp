#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Network/CgsNetworkConstants.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h"

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
        BrnServerInterface* GetServerInterface()
        {
            return &mServerInterface;
        }

        const BrnServerInterface* GetServerInterface() const
        {
            return &mServerInterface;
        }

    private:
        CgsNetwork::VersionDisplay mVersionDisplay;
        CgsNetwork::NetworkAdapter mNetworkAdapter;
        BrnServerInterface mServerInterface;
    };
}
