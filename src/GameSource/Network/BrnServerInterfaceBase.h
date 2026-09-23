#ifndef BRN_SERVER_INTERFACE_BASE_H
#define BRN_SERVER_INTERFACE_BASE_H

#include "network_defines.h"
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterface.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfo.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceBroadcastMessages.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceHttp.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceServerInfo.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceUsersets.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePingRegions.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceRankings.h"
#include "GameSource/Network/Components/BrnServerInterfaceDownloadableConfig.h"
#include "GameSource/Network/Components/BrnServerInterfaceTelemetry.h"
#include "GameSource/Network/Components/BrnServerInterfaceCustomCommands.h"
#include "GameSource/Network/Debug Components/BrnNetworkServerInterfaceDebugComponent.h"

// ===========================================================================
// BrnNetwork::BrnServerInterfaceBase
//
// The game's server interface: the DirtySock facade plus every server-interface
// component, embedded by value, and the two resumable Prepare / Release stage
// machines that bring them up and down in order.
//
// Console layout (32-bit), read off Construct / Prepare / Release / Destruct and the
// constructor's component-vtable stores:
//   +0x000  CgsNetwork::ServerInterface base          (ends +0xC0)
//   +0x0C0  mConnection          +0x0E0  mPlayerInfo      +0x10C  mBroadcastMessages
//   +0x69C  mHttp                +0x6C8  mServerInfo      +0x6EC  mUsersets
//   +0x708  mPingRegions         +0x7FC  mDownloadableConfig
//   +0xB5C  mTelemetry           +0xB88  mRankings        +0xBA8  mCustomCommands
//   +0xBD4  mpNetworkManager     +0xBD8  mePrepareStage   +0xBDC  meReleaseStage
//   +0xBE0  mServerInterfaceDebugComponent
//   +0xC00  miNetworkServerInterfaceBasePM1 .. +0xC28 PM11
//   sizeof == 0xC2C (the platform layer's mGames starts there)
// The prepare-params component slots Prepare fills confirm the EComponents mapping
// (connection 0, player info 2, broadcast 3, http 4, server info 5, downloadable
// config 6, telemetry 7, rankings 8, custom commands 9, usersets 10, ping regions 11).
// Members are addressed by name; the host layout widens with the pointers.
// ===========================================================================

namespace BrnNetwork
{

namespace BrnNetworkModuleIO
{
    struct PostSimulationInputBuffer;
}

class BrnNetworkManager;

class BrnServerInterfaceBase : public CgsNetwork::ServerInterface
{
public:

    // Stage values are the Prepare jump-table cases (0..13).
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
        E_PREPARESTAGE_USERSETS_COMPONENT,
        E_PREPARESTAGE_PING_REGIONS_COMPONENT,
        E_PREPARESTAGE_DONE
    };

    // Stage values are the Release jump-table cases (0..13); Construct arms DONE (13).
    enum EReleaseStage
    {
        E_RELEASESTAGE_START,
        E_RELEASESTAGE_PING_REGIONS_COMPONENT,
        E_RELEASESTAGE_USERSETS_COMPONENT,
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

    // Scalar deleting destructor (bodied in BrnServerInterfaceBase.cpp).
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
    ServerInterfaceDebugComponent * GetDebugComponent() { return &mServerInterfaceDebugComponent; }

private:

    CgsNetwork::ServerInterfaceConnection mConnection;
    CgsNetwork::ServerInterfacePlayerInfo mPlayerInfo;
    CgsNetwork::ServerInterfaceBroadcastMessages mBroadcastMessages;
    CgsNetwork::ServerInterfaceHttp mHttp;
    CgsNetwork::ServerInterfaceServerInfo mServerInfo;
    CgsNetwork::ServerInterfaceUsersets mUsersets;
    CgsNetwork::ServerInterfacePingRegions mPingRegions;
    BrnServerInterfaceDownloadableConfig mDownloadableConfig;
    BrnServerInterfaceTelemetry mTelemetry;
    CgsNetwork::ServerInterfaceRankings mRankings;
    ServerInterfaceCustomCommands mCustomCommands;

    BrnNetworkManager * mpNetworkManager;

    EPrepareStage mePrepareStage;
    EReleaseStage meReleaseStage;

    ServerInterfaceDebugComponent mServerInterfaceDebugComponent;

    // CPU perf-monitor handles Construct registers and Update brackets each
    // component's update with.
    int32_t miNetworkServerInterfaceBasePM1;     // "Int - CgsBase Update"
    int32_t miNetworkServerInterfaceBasePM2;     // "Int - Conn Update"
    int32_t miNetworkServerInterfaceBasePM3;     // "Int - PlayerInfo Update"
    int32_t miNetworkServerInterfaceBasePM4;     // "Int - Broadcast Update"
    int32_t miNetworkServerInterfaceBasePM5;     // "Int - Http Update"
    int32_t miNetworkServerInterfaceBasePM6;     // "Int - ServerInfo Update"
    int32_t miNetworkServerInterfaceBasePM7;     // "Int - Telemetry Update"
    int32_t miNetworkServerInterfaceBasePM8;     // "Int - Rankings Update"
    int32_t miNetworkServerInterfaceBasePM9;     // "Int - Custom Update"
    int32_t miNetworkServerInterfaceBasePM10;    // "Int - Usersets Update"
    int32_t miNetworkServerInterfaceBasePM11;    // "Int - Debug Update"
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
