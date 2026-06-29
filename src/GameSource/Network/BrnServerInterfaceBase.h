#ifndef BRN_SERVER_INTERFACE_BASE_H
#define BRN_SERVER_INTERFACE_BASE_H

#include "network_defines.h"
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterface.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfo.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceBroadcastMessages.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceHttp.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceServerInfo.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceRankings.h"
#include "GameSource/Network/Components/BrnServerInterfaceDownloadableConfig.h"
#include "GameSource/Network/Components/BrnServerInterfaceTelemetry.h"
#include "GameSource/Network/Components/BrnServerInterfaceCustomCommands.h"
#include "GameSource/Network/Debug Components/BrnNetworkServerInterfaceDebugComponent.h"

namespace BrnNetwork
{

namespace BrnNetworkModuleIO
{
    class PostSimulationInputBuffer;
}

class BrnNetworkManager;

class BrnServerInterfaceBase : public CgsNetwork::ServerInterface
{
public:

    enum EPrepareStage
    {
        E_PREPARESTAGE_START,
        E_PREPARESTAGE_BASECLASS,
        E_PREPARESTAGE_CONNECTION_COMPONENT,
        E_PREPARESTAGE_PLAYER_INFO_COMPONENT,
        E_PREPARESTAGE_BROADCAST_MESSAGES_COMPONENT,
        E_PREPARESTAGE_HTTP_COMPONENT,
        E_PREPARESTAGE_SERVERINFO_COMPONENT,
        E_PREPARESTAGE_DOWNLOADABLECONFIG_COMPONENT,
        E_PREPARESTAGE_TELEMETRY_COMPONENT,
        E_PREPARESTAGE_RANKINGS_COMPONENT,
        E_PREPARESTAGE_CUSTOM_COMMANDS_COMPONENT,
        E_PREPARESTAGE_DONE
    };

    enum EReleaseStage
    {
        E_RELEASESTAGE_START,
        E_RELEASESTAGE_CUSTOM_COMMANDS_COMPONENT,
        E_RELEASESTAGE_RANKINGS_COMPONENT,
        E_RELEASESTAGE_TELEMETRY_COMPONENT,
        E_RELEASESTAGE_DOWNLOADABLECONFIG_COMPONENT,
        E_RELEASESTAGE_SERVERINFO_COMPONENT,
        E_RELEASESTAGE_HTTP_COMPONENT,
        E_RELEASESTAGE_BROADCAST_MESSAGES_COMPONENT,
        E_RELEASESTAGE_PLAYER_INFO_COMPONENT,
        E_RELEASESTAGE_CONNECTION_COMPONENT,
        E_RELEASESTAGE_BASECLASS,
        E_RELEASESTAGE_DONE
    };

    BrnServerInterfaceBase();

    // Scalar deleting destructor @ 0x827E1710 (bodied in BrnServerInterfaceBase.cpp).
    virtual ~BrnServerInterfaceBase();

    virtual void Construct();
    virtual bool Prepare( CgsNetwork::ServerInterfacePrepareParams * lpPrepareParams );
    virtual bool Release();
    virtual void Destruct();
    virtual void Update( const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInput );
    virtual void Suspend( int32_t liUpdateFlags );
    virtual void Resume();
    void OnEvent( CgsNetwork::EServerInterfaceEvent leEvent, void * lpData );

    inline void SetNetworkManager( BrnNetworkManager * lpNetworkManager );
    inline BrnServerInterfaceDownloadableConfig * GetDownloadableConfigComponent();
    inline BrnServerInterfaceTelemetry * GetTelemetryComponent();
    inline ServerInterfaceCustomCommands * GetCustomCommandsComponent();

private:

    CgsNetwork::ServerInterfaceConnection mConnection;
    CgsNetwork::ServerInterfacePlayerInfo mPlayerInfo;
    CgsNetwork::ServerInterfaceBroadcastMessages mBroadcastMessages;
    CgsNetwork::ServerInterfaceHttp mHttp;
    CgsNetwork::ServerInterfaceServerInfo mServerInfo;
    BrnServerInterfaceDownloadableConfig mDownloadableConfig;
    BrnServerInterfaceTelemetry mTelemetry;
    CgsNetwork::ServerInterfaceRankings mRankings;
    ServerInterfaceCustomCommands mCustomCommands;

    BrnNetworkManager * mpNetworkManager;

    EPrepareStage mePrepareStage;
    EReleaseStage meReleaseStage;

    ServerInterfaceDebugComponent mServerInterfaceDebugComponent;
};

inline BrnServerInterfaceBase::BrnServerInterfaceBase()
{
}

inline void BrnServerInterfaceBase::SetNetworkManager( BrnNetworkManager * lpNetworkManager )
{
    mpNetworkManager = lpNetworkManager;
}

inline BrnServerInterfaceDownloadableConfig * BrnServerInterfaceBase::GetDownloadableConfigComponent()
{
    return reinterpret_cast<BrnServerInterfaceDownloadableConfig*>( CgsNetwork::ServerInterface::GetDownloadableConfigComponent() );
}

inline BrnServerInterfaceTelemetry * BrnServerInterfaceBase::GetTelemetryComponent()
{
    return reinterpret_cast<BrnServerInterfaceTelemetry*>( CgsNetwork::ServerInterface::GetTelemetryComponent() );
}

inline ServerInterfaceCustomCommands * BrnServerInterfaceBase::GetCustomCommandsComponent()
{
    return reinterpret_cast<ServerInterfaceCustomCommands*>( CgsNetwork::ServerInterface::GetCustomCommandsComponent() );
}

}

#endif
