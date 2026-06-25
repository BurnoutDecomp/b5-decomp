#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Network/CgsNetworkConstants.h"

namespace BrnNetwork
{
    class BrnNetworkManager;

    class NetworkServers
    {
    public:
        explicit NetworkServers(BrnNetworkManager* lpNetworkManager)
            : mpNetworkManager(lpNetworkManager)
            , meServerType(CgsNetwork::E_SERVER_TYPE_LOCAL)
            , mpcServerIP(nullptr)
            , miServerPort(0)
        {
        }

        const char* GetServerIP() const;
        s32 GetServerPort() const;
        CgsNetwork::EServerType GetServerType() const { return meServerType; }
        void SetServerType(CgsNetwork::EServerType leServerType);

    private:
        void SetIPAndPort();

        BrnNetworkManager* mpNetworkManager;
        CgsNetwork::EServerType meServerType;
        const char* mpcServerIP;
        s32 miServerPort;
    };
}
