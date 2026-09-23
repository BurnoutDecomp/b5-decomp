#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Network/CgsNetworkConstants.h"

namespace BrnNetwork
{
    class BrnNetworkManager;

    class NetworkServers
    {
    public:
        // The owning manager's constructor leaves the selection untouched; Construct brings it up.
        NetworkServers() {}

        // Bring the server selection up (inlined into BrnNetworkManager::Construct): remember
        // the manager, start on the first demo server with no address, then resolve the address
        // and port for it.
        void Construct(BrnNetworkManager* lpNetworkManager)
        {
            mpcServerIP      = nullptr;
            miServerPort     = 0;
            mpNetworkManager = lpNetworkManager;
            meServerType     = CgsNetwork::E_SERVER_TYPE_DEMO_1;
            SetIPAndPort();
        }

        const char* GetServerIP() const;
        s32 GetServerPort() const;
        CgsNetwork::EServerType GetServerType() const { return meServerType; }
        // Store the type, re-resolve the address, mirror the type into the version display,
        // drop the server connection and re-target the network adapter.
        void SetServerType(CgsNetwork::EServerType leServerType);

    private:
        void SetIPAndPort();

        BrnNetworkManager* mpNetworkManager;
        CgsNetwork::EServerType meServerType;
        const char* mpcServerIP;
        s32 miServerPort;
    };
}
