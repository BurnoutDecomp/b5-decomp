#include "GameSource/Network/BrnNetworkServers.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Network/BrnNetworkManager.h"

namespace BrnNetwork
{
    namespace
    {
        const char KAC_LOCAL_SERVER_IP[] = "";
        const char KAC_DEV_SERVER_IP[] = "xblburnout08.ea.com";
        const char KAC_TEST_SERVER_IP[] = "sdevlobby03.online.ea.com";
        const char KAC_JUICE_SERVER_IP[] = "stestlobby03.beta.ea.com";
        const char KAC_ARTIST_SERVER_IP[] = "stestotgfe03.pt.abn-iad.ea.com";

        const s32 KI_SERVER_PORT = 21860;
        const s32 KI_DEMO_2_SERVER_PORT = 21880;
    }

    const char* NetworkServers::GetServerIP() const
    {
        CGS_ASSERT(mpcServerIP, "mpcServerIP");
        return mpcServerIP;
    }

    s32 NetworkServers::GetServerPort() const
    {
        CGS_ASSERT(miServerPort > 0, "miServerPort > 0");
        return miServerPort;
    }

    void NetworkServers::SetIPAndPort()
    {
        if (meServerType == CgsNetwork::E_SERVER_TYPE_DEV)
            meServerType = CgsNetwork::E_SERVER_TYPE_JUICE;

        miServerPort = KI_SERVER_PORT;

        switch (meServerType)
        {
        case CgsNetwork::E_SERVER_TYPE_LOCAL:
            mpcServerIP = KAC_LOCAL_SERVER_IP;
            break;

        case CgsNetwork::E_SERVER_TYPE_DEV:
            mpcServerIP = KAC_DEV_SERVER_IP;
            break;

        case CgsNetwork::E_SERVER_TYPE_TEST:
        case CgsNetwork::E_SERVER_TYPE_DEMO_1:
            mpcServerIP = KAC_TEST_SERVER_IP;
            break;

        case CgsNetwork::E_SERVER_TYPE_JUICE:
            mpcServerIP = KAC_JUICE_SERVER_IP;
            break;

        case CgsNetwork::E_SERVER_TYPE_ARTIST:
            mpcServerIP = KAC_ARTIST_SERVER_IP;
            break;

        case CgsNetwork::E_SERVER_TYPE_DEMO_2:
            mpcServerIP = KAC_JUICE_SERVER_IP;
            miServerPort = KI_DEMO_2_SERVER_PORT;
            break;

        default:
            CGS_ASSERT(false, "Unknown server type");
            mpcServerIP = KAC_TEST_SERVER_IP;
            break;
        }
    }

    void NetworkServers::SetServerType(CgsNetwork::EServerType leServerType)
    {
        meServerType = leServerType;
        SetIPAndPort();

        mpNetworkManager->mVersionDisplay.meServerType = meServerType;
        mpNetworkManager->GetServerInterface()->GetConnectionComponent()->DisconnectFromServer();
        mpNetworkManager->mNetworkAdapter.SetServerType(meServerType);
    }
}
