#include "GameSource/Network/BrnServerInterfaceBase.h"

#include "GameSource/Network/BrnNetworkManager.h"                       // TriggerEventFromServerInterface
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // CgsDev::PerfMonCpu
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePrepareParams.h" // mapComponents

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::BrnServerInterfaceBase::`scalar deleting destructor' @ 0x827E1710
//
// The X360 codegen at 0x827E1710 is MSVC's scalar-deleting destructor for the
// 11-component server-interface aggregate. As the destructor chain unwinds it
// reinstalls each embedded component sub-object's vtable pointer
// (off_820CDBF8 -- the shared CgsNetwork::ServerInterfaceComponent base vtable)
// at that sub-object's slot, reinstalls this class's own primary vtable
// (off_820CDBD0) at this+0, and finally -- on the delete-expression path
// (flag & 1) -- frees the object:
//
//   result[746] = &off_820CDBF8;   // 0xBA8  mServerInterfaceDebugComponent / trailing
//   result[738] = &off_820CDBF8;   // 0xB88
//   result[727] = &off_820CDBF8;   // 0xB5C
//   result[511] = &off_820CDBF8;   // 0x7FC
//   result[450] = &off_820CDBF8;   // 0x708
//   result[443] = &off_820CDBF8;   // 0x6EC
//   result[434] = &off_820CDBF8;   // 0x6C8
//   result[423] = &off_820CDBF8;   // 0x69C
//   result[ 67] = &off_820CDBF8;   // 0x10C
//   result[ 56] = &off_820CDBF8;   // 0x0E0
//   result[ 48] = &off_820CDBF8;   // 0x0C0
//   *result     =  off_820CDBD0;   // primary vtable
//   if ( flag & 1 ) operator delete(result);
//   return result;
//
// Each `&off_820CDBF8` store is the by-value member dtor of one of the eleven
// embedded components walking back to the shared ServerInterfaceComponent base
// vtable as it tears down; the eleven slots are mConnection / mPlayerInfo /
// mBroadcastMessages / mHttp / mServerInfo / mDownloadableConfig / mTelemetry /
// mRankings / mCustomCommands / mServerInterfaceDebugComponent (plus the base
// ServerInterface sub-object whose own vtable is off_820CDBD0). The compiler
// synthesises that whole vtable walk + the conditional free from this trivial
// out-of-line virtual destructor; only the (empty) body is hand-written, matching
// the established deleting-destructor convention (see CgsServerInterfaceDirtySock.cpp
// and CgsServerInterfaceComponentDtor.cpp). Defining the destructor out-of-line here
// also anchors this class's vtable (off_820CDBD0) to this TU.
//
// The X360 +0xC0.. member offsets are 32-bit-pointer layout facts; they are NOT
// reproduced or static_asserted on a 64-bit host (the vptr + pointers widen to 8
// bytes there, so the embedded component slots land at different byte offsets while
// the by-name member walk -- and the codegen the compiler emits -- is identical).

namespace BrnNetwork
{
    BrnServerInterfaceBase::~BrnServerInterfaceBase()
    {
    }

    // Construct the facade, arm the two stage machines (nothing prepared, release
    // finished), construct every component, bind the debug component, then register the
    // eleven update perf monitors.
    void BrnServerInterfaceBase::Construct()
    {
        CgsNetwork::ServerInterface::Construct();

        mePrepareStage = E_PREPARESTAGE_START;
        meReleaseStage = E_RELEASESTAGE_DONE;

        mConnection.Construct();
        mPlayerInfo.Construct();
        mBroadcastMessages.Construct();
        mHttp.Construct();
        mServerInfo.Construct();
        mDownloadableConfig.Construct();
        mRankings.Construct();
        mCustomCommands.Construct();
        mTelemetry.Construct();
        mUsersets.Construct();
        mPingRegions.Construct();

        mpNetworkManager = NULL;
        mServerInterfaceDebugComponent.Construct( this );

        // AddMonitor( name, page 9, not minimum, 5.0f budget, lib-perf tagged ).
        miNetworkServerInterfaceBasePM1  = CgsDev::PerfMonCpu::AddMonitor( "Int - CgsBase Update",    CgsDev::E_PMP_9, false, 5.0f, true );
        miNetworkServerInterfaceBasePM2  = CgsDev::PerfMonCpu::AddMonitor( "Int - Conn Update",       CgsDev::E_PMP_9, false, 5.0f, true );
        miNetworkServerInterfaceBasePM3  = CgsDev::PerfMonCpu::AddMonitor( "Int - PlayerInfo Update", CgsDev::E_PMP_9, false, 5.0f, true );
        miNetworkServerInterfaceBasePM4  = CgsDev::PerfMonCpu::AddMonitor( "Int - Broadcast Update",  CgsDev::E_PMP_9, false, 5.0f, true );
        miNetworkServerInterfaceBasePM5  = CgsDev::PerfMonCpu::AddMonitor( "Int - Http Update",       CgsDev::E_PMP_9, false, 5.0f, true );
        miNetworkServerInterfaceBasePM6  = CgsDev::PerfMonCpu::AddMonitor( "Int - ServerInfo Update", CgsDev::E_PMP_9, false, 5.0f, true );
        miNetworkServerInterfaceBasePM7  = CgsDev::PerfMonCpu::AddMonitor( "Int - Telemetry Update",  CgsDev::E_PMP_9, false, 5.0f, true );
        miNetworkServerInterfaceBasePM8  = CgsDev::PerfMonCpu::AddMonitor( "Int - Rankings Update",   CgsDev::E_PMP_9, false, 5.0f, true );
        miNetworkServerInterfaceBasePM9  = CgsDev::PerfMonCpu::AddMonitor( "Int - Custom Update",     CgsDev::E_PMP_9, false, 5.0f, true );
        miNetworkServerInterfaceBasePM10 = CgsDev::PerfMonCpu::AddMonitor( "Int - Usersets Update",   CgsDev::E_PMP_9, false, 5.0f, true );
        miNetworkServerInterfaceBasePM11 = CgsDev::PerfMonCpu::AddMonitor( "Int - Debug Update",      CgsDev::E_PMP_9, false, 5.0f, true );

        CGS_ASSERT( miNetworkServerInterfaceBasePM1 >= 0,  "miNetworkServerInterfaceBasePM1 >= 0" );
        CGS_ASSERT( miNetworkServerInterfaceBasePM2 >= 0,  "miNetworkServerInterfaceBasePM2 >= 0" );
        CGS_ASSERT( miNetworkServerInterfaceBasePM3 >= 0,  "miNetworkServerInterfaceBasePM3 >= 0" );
        CGS_ASSERT( miNetworkServerInterfaceBasePM4 >= 0,  "miNetworkServerInterfaceBasePM4 >= 0" );
        CGS_ASSERT( miNetworkServerInterfaceBasePM5 >= 0,  "miNetworkServerInterfaceBasePM5 >= 0" );
        CGS_ASSERT( miNetworkServerInterfaceBasePM6 >= 0,  "miNetworkServerInterfaceBasePM6 >= 0" );
        CGS_ASSERT( miNetworkServerInterfaceBasePM7 >= 0,  "miNetworkServerInterfaceBasePM7 >= 0" );
        CGS_ASSERT( miNetworkServerInterfaceBasePM8 >= 0,  "miNetworkServerInterfaceBasePM8 >= 0" );
        CGS_ASSERT( miNetworkServerInterfaceBasePM9 >= 0,  "miNetworkServerInterfaceBasePM9 >= 0" );
        CGS_ASSERT( miNetworkServerInterfaceBasePM10 >= 0, "miNetworkServerInterfaceBasePM10 >= 0" );
        CGS_ASSERT( miNetworkServerInterfaceBasePM11 >= 0, "miNetworkServerInterfaceBasePM11 >= 0" );
    }

    // Forward every server-interface event to the network manager first, then let the
    // DirtySock facade handle it.
    void BrnServerInterfaceBase::OnEvent( CgsNetwork::EServerInterfaceEvent leEvent, void * lpData )
    {
        mpNetworkManager->TriggerEventFromServerInterface( leEvent, lpData );
        CgsNetwork::ServerInterface::OnEvent( leEvent, lpData );
    }

    // Hand the prepare chain every component (slot = EComponents), then run the resumable
    // stage machine: facade first, then each component in slot order. A stage that is not
    // finished returns false and is re-entered on the next call.
    bool BrnServerInterfaceBase::Prepare( CgsNetwork::ServerInterfacePrepareParams * lpPrepareParams )
    {
        lpPrepareParams->mapComponents[CgsNetwork::E_COMPONENTS_CONNECTION]          = &mConnection;
        lpPrepareParams->mapComponents[CgsNetwork::E_COMPONENTS_PLAYER_INFO]         = &mPlayerInfo;
        lpPrepareParams->mapComponents[CgsNetwork::E_COMPONENTS_BROADCAST_MESSAGES]  = &mBroadcastMessages;
        lpPrepareParams->mapComponents[CgsNetwork::E_COMPONENTS_HTTP]                = &mHttp;
        lpPrepareParams->mapComponents[CgsNetwork::E_COMPONENTS_SERVERINFO]          = &mServerInfo;
        lpPrepareParams->mapComponents[CgsNetwork::E_COMPONENTS_DOWNLOADABLE_CONFIG] = &mDownloadableConfig;
        lpPrepareParams->mapComponents[CgsNetwork::E_COMPONENTS_TELEMETRY]           = &mTelemetry;
        lpPrepareParams->mapComponents[CgsNetwork::E_COMPONENTS_RANKINGS]            = &mRankings;
        lpPrepareParams->mapComponents[CgsNetwork::E_COMPONENTS_CUSTOM_COMMANDS]     = &mCustomCommands;
        lpPrepareParams->mapComponents[CgsNetwork::E_COMPONENTS_USERSETS]            = &mUsersets;
        lpPrepareParams->mapComponents[CgsNetwork::E_COMPONENTS_PING_REGIONS]        = &mPingRegions;

        switch ( mePrepareStage )
        {
        case E_PREPARESTAGE_START:
            mePrepareStage = E_PREPARESTAGE_START;
            // fall through
        case E_PREPARESTAGE_BASECLASS:
            mePrepareStage = E_PREPARESTAGE_BASECLASS;
            if ( !CgsNetwork::ServerInterface::Prepare( lpPrepareParams ) )
            {
                return false;
            }
            // fall through
        case E_PREPARESTAGE_CONNECTION_COMPONENT:
            mePrepareStage = E_PREPARESTAGE_CONNECTION_COMPONENT;
            if ( !mConnection.Prepare( this ) )
            {
                return false;
            }
            // fall through
        case E_PREPARESTAGE_PLAYER_INFO_COMPONENT:
            mePrepareStage = E_PREPARESTAGE_PLAYER_INFO_COMPONENT;
            if ( !mPlayerInfo.Prepare( this ) )
            {
                return false;
            }
            // fall through
        case E_PREPARESTAGE_BROADCAST_MESSAGES_COMPONENT:
            mePrepareStage = E_PREPARESTAGE_BROADCAST_MESSAGES_COMPONENT;
            if ( !mBroadcastMessages.Prepare( this ) )
            {
                return false;
            }
            // fall through
        case E_PREPARESTAGE_HTTP_COMPONENT:
            mePrepareStage = E_PREPARESTAGE_HTTP_COMPONENT;
            if ( !mHttp.Prepare( this ) )
            {
                return false;
            }
            // fall through
        case E_PREPARESTAGE_SERVERINFO_COMPONENT:
            mePrepareStage = E_PREPARESTAGE_SERVERINFO_COMPONENT;
            if ( !mServerInfo.Prepare( this ) )
            {
                return false;
            }
            // fall through
        case E_PREPARESTAGE_DOWNLOADABLECONFIG_COMPONENT:
            mePrepareStage = E_PREPARESTAGE_DOWNLOADABLECONFIG_COMPONENT;
            if ( !mDownloadableConfig.Prepare( this ) )
            {
                return false;
            }
            // fall through
        case E_PREPARESTAGE_TELEMETRY_COMPONENT:
            mePrepareStage = E_PREPARESTAGE_TELEMETRY_COMPONENT;
            if ( !mTelemetry.Prepare( this, false ) )
            {
                return false;
            }
            // fall through
        case E_PREPARESTAGE_RANKINGS_COMPONENT:
            mePrepareStage = E_PREPARESTAGE_RANKINGS_COMPONENT;
            if ( !mRankings.Prepare( this ) )
            {
                return false;
            }
            // fall through
        case E_PREPARESTAGE_CUSTOM_COMMANDS_COMPONENT:
            mePrepareStage = E_PREPARESTAGE_CUSTOM_COMMANDS_COMPONENT;
            if ( !mCustomCommands.Prepare( this ) )
            {
                return false;
            }
            // fall through
        case E_PREPARESTAGE_USERSETS_COMPONENT:
            mePrepareStage = E_PREPARESTAGE_USERSETS_COMPONENT;
            if ( !mUsersets.Prepare( this ) )
            {
                return false;
            }
            // fall through
        case E_PREPARESTAGE_PING_REGIONS_COMPONENT:
            mePrepareStage = E_PREPARESTAGE_PING_REGIONS_COMPONENT;
            if ( !mPingRegions.Prepare( this ) )
            {
                return false;
            }
            // fall through
        case E_PREPARESTAGE_DONE:
            mePrepareStage = E_PREPARESTAGE_DONE;
            mServerInterfaceDebugComponent.Prepare();
            meReleaseStage = E_RELEASESTAGE_START;
            return true;

        default:
            CGS_ASSERT( false, "0" );
            return false;
        }
    }

    // The reverse machine: components in reverse slot order, then the facade. The facade
    // release stage never reports done, so the machine parks there.
    bool BrnServerInterfaceBase::Release()
    {
        switch ( meReleaseStage )
        {
        case E_RELEASESTAGE_START:
            meReleaseStage = E_RELEASESTAGE_START;
            mServerInterfaceDebugComponent.Release();
            // fall through
        case E_RELEASESTAGE_PING_REGIONS_COMPONENT:
            meReleaseStage = E_RELEASESTAGE_PING_REGIONS_COMPONENT;
            if ( !mPingRegions.Release() )
            {
                return false;
            }
            // fall through
        case E_RELEASESTAGE_USERSETS_COMPONENT:
            meReleaseStage = E_RELEASESTAGE_USERSETS_COMPONENT;
            if ( !mUsersets.Release() )
            {
                return false;
            }
            // fall through
        case E_RELEASESTAGE_CUSTOM_COMMANDS_COMPONENT:
            meReleaseStage = E_RELEASESTAGE_CUSTOM_COMMANDS_COMPONENT;
            if ( !mCustomCommands.Release() )
            {
                return false;
            }
            // fall through
        case E_RELEASESTAGE_RANKINGS_COMPONENT:
            meReleaseStage = E_RELEASESTAGE_RANKINGS_COMPONENT;
            if ( !mRankings.Release() )
            {
                return false;
            }
            // fall through
        case E_RELEASESTAGE_TELEMETRY_COMPONENT:
            meReleaseStage = E_RELEASESTAGE_TELEMETRY_COMPONENT;
            if ( !mTelemetry.Release() )
            {
                return false;
            }
            // fall through
        case E_RELEASESTAGE_DOWNLOADABLECONFIG_COMPONENT:
            meReleaseStage = E_RELEASESTAGE_DOWNLOADABLECONFIG_COMPONENT;
            if ( !mDownloadableConfig.Release() )
            {
                return false;
            }
            // fall through
        case E_RELEASESTAGE_SERVERINFO_COMPONENT:
            meReleaseStage = E_RELEASESTAGE_SERVERINFO_COMPONENT;
            if ( !mServerInfo.Release() )
            {
                return false;
            }
            // fall through
        case E_RELEASESTAGE_HTTP_COMPONENT:
            meReleaseStage = E_RELEASESTAGE_HTTP_COMPONENT;
            if ( !mHttp.Release() )
            {
                return false;
            }
            // fall through
        case E_RELEASESTAGE_BROADCAST_MESSAGES_COMPONENT:
            meReleaseStage = E_RELEASESTAGE_BROADCAST_MESSAGES_COMPONENT;
            if ( !mBroadcastMessages.Release() )
            {
                return false;
            }
            // fall through
        case E_RELEASESTAGE_PLAYER_INFO_COMPONENT:
            meReleaseStage = E_RELEASESTAGE_PLAYER_INFO_COMPONENT;
            if ( !mPlayerInfo.Release() )
            {
                return false;
            }
            // fall through
        case E_RELEASESTAGE_CONNECTION_COMPONENT:
            meReleaseStage = E_RELEASESTAGE_CONNECTION_COMPONENT;
            if ( !mConnection.Release() )
            {
                return false;
            }
            // fall through
        case E_RELEASESTAGE_BASECLASS:
            meReleaseStage = E_RELEASESTAGE_BASECLASS;
            if ( !CgsNetwork::ServerInterface::Release() )
            {
                return false;
            }
            // fall through
        case E_RELEASESTAGE_DONE:
            mePrepareStage = E_PREPARESTAGE_START;
            meReleaseStage = E_RELEASESTAGE_DONE;
            return true;

        default:
            CGS_ASSERT( false, "0" );
            return false;
        }
    }

    // Tear every component down (debug component first, rankings ahead of the rest), then the
    // facade, and re-arm both stage machines as Construct left them.
    void BrnServerInterfaceBase::Destruct()
    {
        mServerInterfaceDebugComponent.Destruct();
        mRankings.Destruct();
        mConnection.Destruct();
        mPlayerInfo.Destruct();
        mBroadcastMessages.Destruct();
        mHttp.Destruct();
        mServerInfo.Destruct();
        mDownloadableConfig.Destruct();
        mCustomCommands.Destruct();
        mTelemetry.Destruct();
        mUsersets.Destruct();
        mPingRegions.Destruct();

        CgsNetwork::ServerInterface::Destruct();

        mePrepareStage = E_PREPARESTAGE_START;
        meReleaseStage = E_RELEASESTAGE_DONE;
    }

    // Each component update bracketed by its perf monitor (ping regions has none; the
    // custom-commands bracket is empty).
    void BrnServerInterfaceBase::Update( const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInput )
    {
        CgsDev::PerfMonCpu::StartMonitor( miNetworkServerInterfaceBasePM1 );
        CgsNetwork::ServerInterface::Update();
        CgsDev::PerfMonCpu::StopMonitor( miNetworkServerInterfaceBasePM1 );

        CgsDev::PerfMonCpu::StartMonitor( miNetworkServerInterfaceBasePM2 );
        mConnection.Update();
        CgsDev::PerfMonCpu::StopMonitor( miNetworkServerInterfaceBasePM2 );

        CgsDev::PerfMonCpu::StartMonitor( miNetworkServerInterfaceBasePM3 );
        mPlayerInfo.Update();
        CgsDev::PerfMonCpu::StopMonitor( miNetworkServerInterfaceBasePM3 );

        CgsDev::PerfMonCpu::StartMonitor( miNetworkServerInterfaceBasePM4 );
        mBroadcastMessages.Update();
        CgsDev::PerfMonCpu::StopMonitor( miNetworkServerInterfaceBasePM4 );

        CgsDev::PerfMonCpu::StartMonitor( miNetworkServerInterfaceBasePM5 );
        mHttp.Update();
        CgsDev::PerfMonCpu::StopMonitor( miNetworkServerInterfaceBasePM5 );

        CgsDev::PerfMonCpu::StartMonitor( miNetworkServerInterfaceBasePM6 );
        mServerInfo.Update();
        CgsDev::PerfMonCpu::StopMonitor( miNetworkServerInterfaceBasePM6 );

        CgsDev::PerfMonCpu::StartMonitor( miNetworkServerInterfaceBasePM7 );
        mTelemetry.Update( lpInput );
        CgsDev::PerfMonCpu::StopMonitor( miNetworkServerInterfaceBasePM7 );

        CgsDev::PerfMonCpu::StartMonitor( miNetworkServerInterfaceBasePM8 );
        mRankings.Update();
        CgsDev::PerfMonCpu::StopMonitor( miNetworkServerInterfaceBasePM8 );

        CgsDev::PerfMonCpu::StartMonitor( miNetworkServerInterfaceBasePM9 );
        mCustomCommands.Update();
        CgsDev::PerfMonCpu::StopMonitor( miNetworkServerInterfaceBasePM9 );

        CgsDev::PerfMonCpu::StartMonitor( miNetworkServerInterfaceBasePM10 );
        mUsersets.Update();
        CgsDev::PerfMonCpu::StopMonitor( miNetworkServerInterfaceBasePM10 );

        mPingRegions.Update();

        CgsDev::PerfMonCpu::StartMonitor( miNetworkServerInterfaceBasePM11 );
        mServerInterfaceDebugComponent.Update();
        CgsDev::PerfMonCpu::StopMonitor( miNetworkServerInterfaceBasePM11 );
    }

    // Components first, then the facade (which takes the update-mask flags).
    void BrnServerInterfaceBase::Suspend( int32_t liUpdateFlags )
    {
        mConnection.Suspend();
        mPlayerInfo.Suspend();
        mBroadcastMessages.Suspend();
        mHttp.Suspend();
        mServerInfo.Suspend();
        mDownloadableConfig.Suspend();
        mUsersets.Suspend();
        mRankings.Suspend();
        mPingRegions.Suspend();

        CgsNetwork::ServerInterface::Suspend( liUpdateFlags );
    }

    // The facade first, then the components.
    void BrnServerInterfaceBase::Resume()
    {
        CgsNetwork::ServerInterface::Resume();

        mConnection.Resume();
        mPlayerInfo.Resume();
        mBroadcastMessages.Resume();
        mHttp.Resume();
        mServerInfo.Resume();
        mDownloadableConfig.Resume();
        mUsersets.Resume();
        mRankings.Resume();
        mPingRegions.Resume();
    }
}
