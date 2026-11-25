#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT at ARTIST locations
#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Network/BrnNetworkManager.cpp"
#include "GameSource/Network/BrnNetworkManager.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"
#include "GameSource/Network/BrnServerInterface.h"                                          // GetTelemetryComponent
#include "GameSource/Network/Components/BrnServerInterfaceTelemetry.h"                      // BrnServerInterfaceTelemetry
#include "GameShared/GameClasses/Core/CgsAssert.h"                                          // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                                     // CgsCore::SPrintf
#include "GameShared/GameClasses/Network/CgsNetworkConstants.h"                             // KI_MAX_TELEMETRY_DATA_SIZE
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceTelemetry.h"

// ============================================================================================
// BrnNetwork::BrnNetworkManager -- the online hub.
//
// The console functions of the class are declared in BrnNetworkManager.h with their
// reconstructed signatures, and the class layout is homed there by name. Most bodies live in
// the partfiles beside this one.
// ============================================================================================

namespace BrnNetwork
{
    // ARTIST 0x82582128. Serialise a NetworkPlayerID over the full signed 32-bit range.
    BrnNetworkManager::PackOrUnpackResult BrnNetworkManager::PackOrUnpack(
        CgsNetwork::Message* lpMessage, NetworkPlayerID* lpNetworkPlayerID)
    {
        s32 liValue = *lpNetworkPlayerID;
        const PackOrUnpackResult lxResult = lpMessage->PackOrUnpack(&liValue, -0x7FFFFFFF - 1, 0x7FFFFFFF);
        *lpNetworkPlayerID = liValue;
        return lxResult;
    }

    // ----------------------------------------------------------------------------------------
    // BrnNetworkManager::CaptureTelemetryEvent (integer payload)
    //
    // The integer form of the telemetry hook: print the value as decimal text into a
    // telemetry-sized buffer and record the event with that text as its payload.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::CaptureTelemetryEvent(ETelemetryHook leHook, s32 liData)
    {
        CGS_ASSERT(GetServerInterface(), CGS_ASSERT_MESSAGE_AT(
            "GetServerInterface()",
            __FILE__, 3121 /* ARTIST 0x825821B4 */));
        CGS_ASSERT(GetServerInterface()->GetTelemetryComponent(), CGS_ASSERT_MESSAGE_AT(
            "GetServerInterface()->GetTelemetryComponent()",
            __FILE__, 3122 /* ARTIST 0x825821DC */));

        char lacData[CgsNetwork::KI_MAX_TELEMETRY_DATA_SIZE];
        CgsCore::SPrintf(lacData, sizeof(lacData), "%i", liData);
        GetServerInterface()->GetTelemetryComponent()->CaptureEvent(leHook, lacData);
    }
}

// FOLDED FROM BrnNetworkManager_wN1_01.cpp
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT at ARTIST locations

#include "GameSource/Network/BrnNetworkManager.h"
#include "GameSource/Network/BrnNetworkPlayer.h"                             // BrnNetworkPlayerConstructParams
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"   // CgsDev::PerfMonCpu::AddMonitor
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePrepareParams.h"  // Prepare

// ============================================================================================
// BrnNetwork::BrnNetworkManager -- lifecycle (partfile of BrnNetworkManager.cpp).
//
// Bodied here: the constructor, Construct, Prepare, Release and Destruct.
// ============================================================================================

namespace BrnNetwork
{
    namespace
    {
        // The largest message and the largest reliable message the network heap is sized for;
        // Construct cross-checks the registered message types against them.
        const s32 KI_MAX_MESSAGE_SIZE          = 1968;
        const s32 KI_MAX_RELIABLE_MESSAGE_SIZE = 568;

        // The platform title id the network adapter is prepared with.
        const u32 KU_TITLE_ID = 0x45410806;

        // The texture compressor's uncompressed and compressed buffer sizes.
        const s32 KI_TEXTURE_COMPRESSOR_UNCOMPRESSED_SIZE = 76800;
        const s32 KI_TEXTURE_COMPRESSOR_COMPRESSED_SIZE   = 9600;
    }

    // ----------------------------------------------------------------------------------------
    // BrnNetworkManager::BrnNetworkManager
    //
    // Only the sub-objects' own constructors run (base, server interface, every manager, the
    // seven network players, the menu data and the clocks); Construct seeds every field.
    // ----------------------------------------------------------------------------------------
    BrnNetworkManager::BrnNetworkManager()
    {
    }

    // ----------------------------------------------------------------------------------------
    // BrnNetworkManager::Construct
    //
    // Check the message registry against the network heap's message sizes, build the player
    // manager's per-slot tables (the seven network players and their menu data, the eighth
    // menu slot being the local selections), construct the CgsNetwork base, the server
    // interface and every game-side manager in order, bring the server selection up, then
    // reset the session scalars and register the per-subsystem CPU monitors.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::Construct(BrnNetworkModule* lpNetworkModule, bool lbEnableJuice)
    {
        // The console also streams the size of every message type to the network dev log
        // here; that stream has no home in the tree.
        const s32 liMaxMessageSize = GetMaxMessageSize(false);
        CGS_ASSERT(liMaxMessageSize == KI_MAX_MESSAGE_SIZE, "Max message size is ");

        const s32 liMaxReliableMessageSize = GetMaxMessageSize(true);
        CGS_ASSERT(liMaxReliableMessageSize == KI_MAX_RELIABLE_MESSAGE_SIZE,
                   "Largest reliable message size: ");

        // Each slot's player-manager field is left for the player manager to fill.
        BrnNetworkPlayerConstructParams           laPlayerConstructParams[CgsNetwork::PlayerManager::KI_MAX_PLAYERS];
        CgsNetwork::PlayerManagerConstructParams  lPlayerManagerConstructParams;
        CgsNetwork::NetworkManagerConstructParams lConstructParams;
        lConstructParams.Construct(&lPlayerManagerConstructParams, SyncTimeClientReadyCallback, this);

        for (s32 i = 0; i < CgsNetwork::PlayerManager::KI_MAX_PLAYERS; ++i)
        {
            laPlayerConstructParams[i].mpTimeManager   = GetTimeManager();
            laPlayerConstructParams[i].mpNetworkModule = lpNetworkModule;
            lPlayerManagerConstructParams.mapConstructParams[i] = &laPlayerConstructParams[i];

            if (i < KI_MAX_NETWORK_PLAYERS)
            {
                lPlayerManagerConstructParams.mapPlayerList[i] = &maNetworkPlayer[i];
                lPlayerManagerConstructParams.mapMenuData[i]   = &maNetworkPlayerMenuData[i];
            }
            else
            {
                lPlayerManagerConstructParams.mapMenuData[i] = &mLocalPlayerMenuSelections;
            }
        }

        CgsNetwork::NetworkManager::Construct(&lConstructParams);

        mePrepareStage = E_PREPARESTAGE_START;
        meReleaseStage = E_RELEASESTAGE_DONE;
        mServerInterface.Construct();
        mServerInterface.SetNetworkManager(this);
        // The server type is read before mNetworkServers is constructed below.
        mServerInterface.GetDebugComponent()->SetNetworkManager(this);
        mServerInterface.GetDebugComponent()->SetServerType(mNetworkServers.GetServerType());

        CGS_ASSERT(lpNetworkModule, "lpNetworkModule");
        mpNetworkModule = lpNetworkModule;

        mLoginManager.Construct(lpNetworkModule);
        mLaunchManager.Construct(this);
        mConnectionManager.Construct(this);
        mMatchMakingManager.Construct(this);
        mSuspensionManager.Construct(this);
        mPostRoundManager.Construct(this);
        mBuddyManager.Construct(mpNetworkModule, &mServerInterface);
        mStandingsManager.Construct(mpNetworkModule, GetTimeManager());
        mTrafficManager.Construct(lpNetworkModule, GetPlayerManager(), GetTimeManager());
        mStatsManager.Construct(lpNetworkModule);
        mScoreboardManager.Construct();
        mAggressiveDrivingManager.Construct(lpNetworkModule, GetPlayerManager(), GetTimeManager());
        mDirtyTrickManager.Construct(lpNetworkModule, GetPlayerManager(), GetTimeManager());
        mImageManager.Construct(lpNetworkModule, GetPlayerManager(), GetTimeManager(), &mTextureCompressor);
        mLiveRevengeManager.Construct(lpNetworkModule);
        mCamera.Construct(this, &mTextureCompressor);
        mTextureCompressor.Construct();
        mGamerPictureManager.Construct(this, &mTextureCompressor);
        mNetworkNotificationManager.Construct(lpNetworkModule);
        mNetworkInviteManager.Construct(lpNetworkModule);
        mSelectedRoutesManager.Construct(lpNetworkModule, GetTimeManager());
        mMarkedManManager.Construct(this);
        mStateManager.Construct(lpNetworkModule);
        mRoadRulesManager.Construct();
        mChallengeSuccessManager.Construct(lpNetworkModule, GetPlayerManager(), GetTimeManager());
        mGamerCardManager.Construct(&mServerInterface);
        mEventScoresManager.Construct(lpNetworkModule, &mServerInterface);
        mAutoLoginManager.Construct(lpNetworkModule, &mServerInterface);
        mTeamSelectionManager.Construct(this);

        mNetworkServers.Construct(this);

        GetStartTimeManager()->SetGapTillStartTime(CgsSystem::Time(1.0f));

        mTime.SetFloatVal(0.0f);
        mfTimeStep              = 0.0f;
        miFrameNum              = 0;
        meLocalConsoleFrameRate = CgsSystem::E_FRAMERATE_UNKNOWN;
        mPlayerIDStatsGet       = -1;
        mLastStatsSentToOnlinePlayStamp.SetFloatVal(0.0f);
        miRoundNumber           = -1;
        mbHasFever              = false;
        mbIsDeveloper           = false;

        if (lbEnableJuice)
        {
            mNetworkServers.SetServerType(CgsNetwork::E_SERVER_TYPE_ARTIST);
        }

        mFreeBurnCarID                         = 0;
        mfField86A90                           = 0.0f;
        meCurrentDistrict                      = BrnWorld::E_DISTRICT_CRISTAL_SUMMIT;
        mFreeBurnWheelID                       = 0;
        mu16CarColourIndex                     = 0;
        mu16PaintFinishIndex                   = 0;
        mbNotifyGameOfDisconnectAfterDiskError = false;
        mbDiskReadErrorThisFrame               = false;

        miPackTextureToSendToGuiPM          = CgsDev::PerfMonCpu::AddMonitor("Man - PackTextureToSendToGui",    CgsDev::E_PMP_8,  false, 5.0f, false);
        miCameraUpdatePM                    = CgsDev::PerfMonCpu::AddMonitor("Man - UpdateCamera",              CgsDev::E_PMP_8,  false, 5.0f, true);
        miGamerPicUpdatPM                   = CgsDev::PerfMonCpu::AddMonitor("Man - UpdateGamerPic",            CgsDev::E_PMP_8,  false, 5.0f, true);
        miCGSNetworkAfterSimPM              = CgsDev::PerfMonCpu::AddMonitor("Man - CGSNetwork",                CgsDev::E_PMP_18, false, 5.0f, true);
        miNetworkManagerSelectedRoutesPM    = CgsDev::PerfMonCpu::AddMonitor("Man - SelectedRoutes",            CgsDev::E_PMP_18, false, 5.0f, true);
        miNetworkManagerOutputPlayerStatsPM = CgsDev::PerfMonCpu::AddMonitor("Man - OutputPlayerStats",         CgsDev::E_PMP_18, false, 5.0f, true);
        miNetworkManagerPostUpdatePM        = CgsDev::PerfMonCpu::AddMonitor("Man - PostUpdate (SendMessages)", CgsDev::E_PMP_18, false, 5.0f, true);
        miNetworkManagerMiscPM              = CgsDev::PerfMonCpu::AddMonitor("Man - Misc",                      CgsDev::E_PMP_9,  false, 5.0f, true);
        miNetworkServerInterfacePM          = CgsDev::PerfMonCpu::AddMonitor("Man - ServerInt",                 CgsDev::E_PMP_9,  false, 5.0f, true);
        miNetworkBuddyPM                    = CgsDev::PerfMonCpu::AddMonitor("Man - Buddy",                     CgsDev::E_PMP_9,  false, 5.0f, true);
        miNetworkTrafficPM                  = CgsDev::PerfMonCpu::AddMonitor("Man - Traffic",                   CgsDev::E_PMP_9,  false, 5.0f, true);
        miNetworkAggressiveDrivingPM        = CgsDev::PerfMonCpu::AddMonitor("Man - Agg Driv",                  CgsDev::E_PMP_9,  false, 5.0f, true);
        miNetworkDirtyTrickPM               = CgsDev::PerfMonCpu::AddMonitor("Man - Dirty Trick",               CgsDev::E_PMP_9,  false, 5.0f, true);
        miNetworkNotificationPM             = CgsDev::PerfMonCpu::AddMonitor("Man - Notification",              CgsDev::E_PMP_9,  false, 5.0f, true);
        miNetworkInvitePM                   = CgsDev::PerfMonCpu::AddMonitor("Man - Invites",                   CgsDev::E_PMP_9,  false, 5.0f, true);
        miNetworkStatsPM                    = CgsDev::PerfMonCpu::AddMonitor("Man - Stats",                     CgsDev::E_PMP_9,  false, 5.0f, true);
        miNetworkLiveRevengePM              = CgsDev::PerfMonCpu::AddMonitor("Man - Live Rev",                  CgsDev::E_PMP_9,  false, 5.0f, true);
        miNetworkScoreboardPM               = CgsDev::PerfMonCpu::AddMonitor("Man - Scoreboards",               CgsDev::E_PMP_9,  false, 5.0f, true);
        miNetworkRoutesManagerPM            = CgsDev::PerfMonCpu::AddMonitor("Man - Selected Routes",           CgsDev::E_PMP_9,  false, 5.0f, true);
        miNetworkStateManagerPM             = CgsDev::PerfMonCpu::AddMonitor("Man - State Man",                 CgsDev::E_PMP_9,  false, 5.0f, true);
        miNetworkRoadRulesManagerPM         = CgsDev::PerfMonCpu::AddMonitor("Man - Road Rules Man",            CgsDev::E_PMP_9,  false, 5.0f, true);
        miNetworkChallengeManagerPM         = CgsDev::PerfMonCpu::AddMonitor("Man - Chall Success Man",         CgsDev::E_PMP_9,  false, 5.0f, true);

        CGS_ASSERT(miPackTextureToSendToGuiPM >= 0,   "miPackTextureToSendToGuiPM >= 0");
        CGS_ASSERT(miCameraUpdatePM >= 0,             "miCameraUpdatePM >= 0");
        CGS_ASSERT(miGamerPicUpdatPM >= 0,            "miGamerPicUpdatPM >= 0");
        CGS_ASSERT(miCGSNetworkAfterSimPM >= 0,       "miCGSNetworkAfterSimPM >= 0");
        CGS_ASSERT(miNetworkManagerMiscPM >= 0,       "miNetworkManagerMiscPM >= 0");
        CGS_ASSERT(miNetworkServerInterfacePM >= 0,   "miNetworkServerInterfacePM >= 0");
        CGS_ASSERT(miNetworkBuddyPM >= 0,             "miNetworkBuddyPM >= 0");
        CGS_ASSERT(miNetworkTrafficPM >= 0,           "miNetworkTrafficPM >= 0");
        CGS_ASSERT(miNetworkAggressiveDrivingPM >= 0, "miNetworkAggressiveDrivingPM >= 0");
        CGS_ASSERT(miNetworkDirtyTrickPM >= 0,        "miNetworkDirtyTrickPM >= 0");
        CGS_ASSERT(miNetworkNotificationPM >= 0,      "miNetworkNotificationPM >= 0");
        CGS_ASSERT(miNetworkInvitePM >= 0,            "miNetworkInvitePM >= 0");
        CGS_ASSERT(miNetworkStatsPM >= 0,             "miNetworkStatsPM >= 0");
        CGS_ASSERT(miNetworkLiveRevengePM >= 0,       "miNetworkLiveRevengePM >= 0");
        CGS_ASSERT(miNetworkScoreboardPM >= 0,        "miNetworkScoreboardPM >= 0");
        CGS_ASSERT(miNetworkRoutesManagerPM >= 0,     "miNetworkRoutesManagerPM >= 0");
        CGS_ASSERT(miNetworkStateManagerPM >= 0,      "miNetworkStateManagerPM >= 0");
        CGS_ASSERT(miNetworkRoadRulesManagerPM >= 0,  "miNetworkRoadRulesManagerPM >= 0");
        CGS_ASSERT(miNetworkChallengeManagerPM >= 0,  "miNetworkChallengeManagerPM >= 0");
    }

    // ----------------------------------------------------------------------------------------
    // BrnNetworkManager::Prepare
    //
    // Staged bring-up (33 stages), each resumed where the last call left off: the CgsNetwork
    // base (build banner, adapter, player manager), the server interface, then every game-side
    // manager. Returns false while a stage is still busy and true once every stage is ready,
    // when the session scalars are reset.
    // ----------------------------------------------------------------------------------------
    bool BrnNetworkManager::Prepare(CgsSystem::EFrameRate leLocalConsoleFrameRate,
                                    const BrnHW::LaunchData* lpLaunchData,
                                    CgsMemory::HeapMalloc* lpHeapMalloc)
    {
        switch (mePrepareStage)
        {
        case E_PREPARESTAGE_START:
            mePrepareStage = E_PREPARESTAGE_START;
            // The console also switches the shared network debug stream on here; that stream
            // has no home in the tree.
            meLocalConsoleFrameRate = leLocalConsoleFrameRate;
            mServerInterface.SetMemoryBuffer(lpHeapMalloc);
            // fall through

        case E_PREPARESTAGE_BASE_NETWORK_MANAGER:
        {
            mePrepareStage = E_PREPARESTAGE_BASE_NETWORK_MANAGER;

            CgsNetwork::NetworkManagerPrepareParams::VersionDisplayPrepareParams lVersionDisplayParams;
            lVersionDisplayParams.Construct("BURNOUT5/31", 2, mNetworkServers.GetServerType());

            CgsNetwork::NetworkAdapterPrepareParams lNetworkAdapterParams;
            lNetworkAdapterParams.Construct(mNetworkServers.GetServerType(), lpHeapMalloc, this,
                                            &mServerInterface, KU_TITLE_ID);

            CgsNetwork::NetworkManagerPrepareParams::PlayerManagerPrepareParams lPlayerManagerParams;
            lPlayerManagerParams.Construct(&mServerInterface, 0, 0, 0, lpHeapMalloc);

            CgsNetwork::NetworkManagerPrepareParams lPrepareParams;
            lPrepareParams.Construct(&lVersionDisplayParams, &lNetworkAdapterParams,
                                     &lPlayerManagerParams, leLocalConsoleFrameRate);

            const CgsNetwork::EManagerReturnCode lReturnCode = CgsNetwork::NetworkManager::Prepare(&lPrepareParams);
            if (lReturnCode == CgsNetwork::E_MANAGER_STATUS_BUSY)
            {
                return false;
            }
            CGS_ASSERT(lReturnCode == CgsNetwork::E_MANAGER_STATUS_READY, CGS_ASSERT_MESSAGE_AT(
                "lReturnCode == CgsNetwork::E_MANAGER_STATUS_READY",
                __FILE__, 430 /* ARTIST 0x8259ADE8 */));

            GetPlayerManager()->RegisterEventCallback(PlayerManagerEventCallback, this);
        }
            // fall through

        case E_PREPARESTAGE_SERVER_INTERFACE:
        {
            mePrepareStage = E_PREPARESTAGE_SERVER_INTERFACE;

            CgsNetwork::ServerInterfacePrepareParams lServerInterfaceParams;
            lServerInterfaceParams.Construct();
            lServerInterfaceParams.mLobbyParams.miLanguage     = 0;
            lServerInterfaceParams.mLobbyParams.mpcVersion     = "BURNOUT5/31";
            lServerInterfaceParams.mLobbyParams.mpcSKU         = "XBL2";
            lServerInterfaceParams.mLobbyParams.mpcSLUS        = "07604772/US";
            lServerInterfaceParams.mConnAPIParams.miPort       = 1000;
            lServerInterfaceParams.mConnAPIParams.miMaxPlayers = 8;
            lServerInterfaceParams.mConnAPIParams.mbPeerToPeer = true;
            if (!mServerInterface.Prepare(&lServerInterfaceParams))
            {
                return false;
            }
        }
            // fall through

        case E_PREPARESTAGE_LOGIN_MANAGER:
            mePrepareStage = E_PREPARESTAGE_LOGIN_MANAGER;
            if (!mLoginManager.Prepare()) return false;
            // fall through
        case E_PREPARESTAGE_LAUNCH_MANAGER:
            mePrepareStage = E_PREPARESTAGE_LAUNCH_MANAGER;
            if (!mLaunchManager.Prepare()) return false;
            // fall through
        case E_PREPARESTAGE_CONNECTION_MANAGER:
            mePrepareStage = E_PREPARESTAGE_CONNECTION_MANAGER;
            if (!mConnectionManager.Prepare()) return false;
            // fall through
        case E_PREPARESTAGE_MATCHMAKING_MANAGER:
            mePrepareStage = E_PREPARESTAGE_MATCHMAKING_MANAGER;
            if (!mMatchMakingManager.Prepare()) return false;
            // fall through
        case E_PREPARESTAGE_SUSPENSION_MANAGER:
            mePrepareStage = E_PREPARESTAGE_SUSPENSION_MANAGER;
            if (!mSuspensionManager.Prepare()) return false;
            // fall through
        case E_PREPARESTAGE_POSTROUND_MANAGER:
            mePrepareStage = E_PREPARESTAGE_POSTROUND_MANAGER;
            if (!mPostRoundManager.Prepare()) return false;
            // fall through
        case E_PREPARESTAGE_CAMERA:
            mePrepareStage = E_PREPARESTAGE_CAMERA;
            if (!mCamera.Prepare(lpHeapMalloc)) return false;
            // fall through
        case E_PREPARESTAGE_TEXTURE_COMPRESSOR:
            mePrepareStage = E_PREPARESTAGE_TEXTURE_COMPRESSOR;
            if (!mTextureCompressor.Prepare(lpHeapMalloc, KI_TEXTURE_COMPRESSOR_UNCOMPRESSED_SIZE,
                                            KI_TEXTURE_COMPRESSOR_COMPRESSED_SIZE)) return false;
            // fall through
        case E_PREPARESTAGE_BUDDY_MANAGER:
            mePrepareStage = E_PREPARESTAGE_BUDDY_MANAGER;
            if (!mBuddyManager.Prepare(lpLaunchData)) return false;
            // fall through
        case E_PREPARESTAGE_STANDINGS_MANAGER:
            mePrepareStage = E_PREPARESTAGE_STANDINGS_MANAGER;
            if (!mStandingsManager.Prepare()) return false;
            // fall through
        case E_PREPARESTAGE_TRAFFIC_SYNC_MANAGER:
            mePrepareStage = E_PREPARESTAGE_TRAFFIC_SYNC_MANAGER;
            if (!mTrafficManager.Prepare()) return false;
            // fall through
        case E_PREPARESTAGE_STATS_MANAGER:
            mePrepareStage = E_PREPARESTAGE_STATS_MANAGER;
            if (!mStatsManager.Prepare()) return false;
            // fall through
        case E_PREPARESTAGE_SCOREBOARD_MANAGER:
            mePrepareStage = E_PREPARESTAGE_SCOREBOARD_MANAGER;
            if (!mScoreboardManager.Prepare(&mServerInterface, mpNetworkModule)) return false;
            // fall through
        case E_PREPARESTAGE_AGGRESSIVE_DRIVING_MANAGER:
            mePrepareStage = E_PREPARESTAGE_AGGRESSIVE_DRIVING_MANAGER;
            if (!mAggressiveDrivingManager.Prepare()) return false;
            // fall through
        case E_PREPARESTAGE_LIVE_REVENGE_MANAGER:
            mePrepareStage = E_PREPARESTAGE_LIVE_REVENGE_MANAGER;
            if (!mLiveRevengeManager.Prepare(lpHeapMalloc)) return false;
            // fall through
        case E_PREPARESTAGE_DIRTY_TRICK_MANAGER:
            mePrepareStage = E_PREPARESTAGE_DIRTY_TRICK_MANAGER;
            if (!mDirtyTrickManager.Prepare()) return false;
            // fall through
        case E_PREPARESTAGE_IMAGE_MANAGER:
            mePrepareStage = E_PREPARESTAGE_IMAGE_MANAGER;
            if (!mImageManager.Prepare(lpHeapMalloc)) return false;
            // fall through
        case E_PREPARESTAGE_GAMER_PICTURE_MANAGER:
            mePrepareStage = E_PREPARESTAGE_GAMER_PICTURE_MANAGER;
            if (!mGamerPictureManager.Prepare(lpHeapMalloc)) return false;
            // fall through
        case E_PREPARESTAGE_NOTIFICATION_MANAGER:
            mePrepareStage = E_PREPARESTAGE_NOTIFICATION_MANAGER;
            if (!mNetworkNotificationManager.Prepare()) return false;
            // fall through
        case E_PREPARESTAGE_INVITE_MANAGER:
            mePrepareStage = E_PREPARESTAGE_INVITE_MANAGER;
            if (!mNetworkInviteManager.Prepare()) return false;
            // fall through
        case E_PREPARESTAGE_SELECTED_ROUTES_MANAGER:
            mePrepareStage = E_PREPARESTAGE_SELECTED_ROUTES_MANAGER;
            if (!mSelectedRoutesManager.Prepare()) return false;
            // fall through
        case E_PREPARESTAGE_MARKED_MAN_MANAGER:
            mePrepareStage = E_PREPARESTAGE_MARKED_MAN_MANAGER;
            if (!mMarkedManManager.Prepare()) return false;
            // fall through
        case E_PREPARESTAGE_STATE_MANAGER:
            mePrepareStage = E_PREPARESTAGE_STATE_MANAGER;
            if (!mStateManager.Prepare()) return false;
            // fall through
        case E_PREPARESTAGE_ROAD_RULES_MANAGER:
            mePrepareStage = E_PREPARESTAGE_ROAD_RULES_MANAGER;
            if (!mRoadRulesManager.Prepare(mpNetworkModule, &mServerInterface,
                                           GetPlayerManager(), GetTimeManager())) return false;
            // fall through
        case E_PREPARESTAGE_CHALLEGE_SUCCESS_MANAGER:
            mePrepareStage = E_PREPARESTAGE_CHALLEGE_SUCCESS_MANAGER;
            if (!mChallengeSuccessManager.Prepare()) return false;
            // fall through
        case E_PREPARESTAGE_GAMERCARD_MANAGER:
            mePrepareStage = E_PREPARESTAGE_GAMERCARD_MANAGER;
            if (!mGamerCardManager.Prepare()) return false;
            // fall through
        case E_PREPARESTAGE_EVENT_SCORES_MANAGER:
            mePrepareStage = E_PREPARESTAGE_EVENT_SCORES_MANAGER;
            if (!mEventScoresManager.Prepare()) return false;
            // fall through
        case E_PREPARESTAGE_AUTO_LOGIN_MANAGER:
            mePrepareStage = E_PREPARESTAGE_AUTO_LOGIN_MANAGER;
            if (!mAutoLoginManager.Prepare()) return false;
            // fall through
        case E_PREPARESTAGE_TEAM_SELECTION_MANAGER:
            mePrepareStage = E_PREPARESTAGE_TEAM_SELECTION_MANAGER;
            if (!mTeamSelectionManager.Prepare()) return false;
            // fall through

        case E_PREPARESTAGE_DONE:
            mePrepareStage = E_PREPARESTAGE_DONE;
            meReleaseStage = E_RELEASESTAGE_START;
            mTime.SetFloatVal(0.0f);
            mfTimeStep        = 0.0f;
            miFrameNum        = 0;
            mPlayerIDStatsGet = -1;
            mLastStatsSentToOnlinePlayStamp.SetFloatVal(0.0f);
            mbHasFever        = false;
            mbIsDeveloper     = false;
            mFreeBurnCarID    = 0;
            mFreeBurnWheelID  = 0;
            mfField86A90      = 0.0f;
            meCurrentDistrict = BrnWorld::E_DISTRICT_CRISTAL_SUMMIT;
            mu16CarColourIndex                     = 0;
            mu16PaintFinishIndex                   = 0;
            mbNotifyGameOfDisconnectAfterDiskError = false;
            mbDiskReadErrorThisFrame               = false;
            return true;

        default:
            CGS_ASSERT(false, CGS_ASSERT_MESSAGE_AT(
                "unknown prepare stage",
                __FILE__, 811 /* ARTIST 0x8259B318 */));
            return false;
        }
    }

    // ----------------------------------------------------------------------------------------
    // BrnNetworkManager::Release
    //
    // Staged tear-down, the mirror of Prepare: reset the session scalars, release the managers
    // newest first (the buddy manager disconnects before it releases), then the server
    // interface and the CgsNetwork base, and finally hand back the server interface's memory
    // buffer. Returns false while a stage is still releasing.
    // ----------------------------------------------------------------------------------------
    bool BrnNetworkManager::Release()
    {
        switch (meReleaseStage)
        {
        case E_RELEASESTAGE_START:
            meReleaseStage = E_RELEASESTAGE_START;
            mePrepareStage = E_PREPARESTAGE_START;
            mTime.SetFloatVal(0.0f);
            mfTimeStep              = 0.0f;
            miFrameNum              = 0;
            meLocalConsoleFrameRate = CgsSystem::E_FRAMERATE_UNKNOWN;
            mPlayerIDStatsGet       = -1;
            mLastStatsSentToOnlinePlayStamp.SetFloatVal(0.0f);
            mFreeBurnCarID          = 0;
            mFreeBurnWheelID        = 0;
            mfField86A90            = 0.0f;
            meCurrentDistrict       = BrnWorld::E_DISTRICT_CRISTAL_SUMMIT;
            mu16CarColourIndex      = 0;
            mu16PaintFinishIndex    = 0;
            mbNotifyGameOfDisconnectAfterDiskError = false;
            mbDiskReadErrorThisFrame               = false;
            // fall through

        case E_RELEASESTAGE_TEAM_SELECTION_MANAGER:
            meReleaseStage = E_RELEASESTAGE_TEAM_SELECTION_MANAGER;
            if (!mTeamSelectionManager.Release()) return false;
            // fall through
        case E_RELEASESTAGE_AUTO_LOGIN_MANAGER:
            meReleaseStage = E_RELEASESTAGE_AUTO_LOGIN_MANAGER;
            if (!mAutoLoginManager.Release()) return false;
            // fall through
        case E_RELEASESTAGE_EVENT_SCORES_MANAGER:
            meReleaseStage = E_RELEASESTAGE_EVENT_SCORES_MANAGER;
            if (!mEventScoresManager.Release()) return false;
            // fall through
        case E_RELEASESTAGE_GAMERCARD_MANAGER:
            meReleaseStage = E_RELEASESTAGE_GAMERCARD_MANAGER;
            if (!mGamerCardManager.Release()) return false;
            // fall through
        case E_RELEASESTAGE_CHALLEGE_SUCCESS_MANAGER:
            meReleaseStage = E_RELEASESTAGE_CHALLEGE_SUCCESS_MANAGER;
            if (!mChallengeSuccessManager.Release()) return false;
            // fall through
        case E_RELEASESTAGE_ROAD_RULES_MANAGER:
            meReleaseStage = E_RELEASESTAGE_ROAD_RULES_MANAGER;
            if (!mRoadRulesManager.Release()) return false;
            // fall through
        case E_RELEASESTAGE_STATE_MANAGER:
            meReleaseStage = E_RELEASESTAGE_STATE_MANAGER;
            if (!mStateManager.Release()) return false;
            // fall through
        case E_RELEASESTAGE_MARKED_MAN_MANAGER:
            meReleaseStage = E_RELEASESTAGE_MARKED_MAN_MANAGER;
            if (!mMarkedManManager.Release()) return false;
            // fall through
        case E_RELEASESTAGE_SELECTED_ROUTES_MANAGER:
            meReleaseStage = E_RELEASESTAGE_SELECTED_ROUTES_MANAGER;
            if (!mSelectedRoutesManager.Release()) return false;
            // fall through
        case E_RELEASESTAGE_INVITE_MANAGER:
            meReleaseStage = E_RELEASESTAGE_INVITE_MANAGER;
            if (!mNetworkInviteManager.Release()) return false;
            // fall through
        case E_RELEASESTAGE_NOTIFICATION_MANAGER:
            meReleaseStage = E_RELEASESTAGE_NOTIFICATION_MANAGER;
            if (!mNetworkNotificationManager.Release()) return false;
            // fall through
        case E_RELEASESTAGE_GAMER_PICTURE_MANAGER:
            meReleaseStage = E_RELEASESTAGE_GAMER_PICTURE_MANAGER;
            if (!mGamerPictureManager.Release()) return false;
            // fall through
        case E_RELEASESTAGE_IMAGE_MANAGER:
            meReleaseStage = E_RELEASESTAGE_IMAGE_MANAGER;
            if (!mImageManager.Release()) return false;
            // fall through
        case E_RELEASESTAGE_DIRTY_TRICK_MANAGER:
            meReleaseStage = E_RELEASESTAGE_DIRTY_TRICK_MANAGER;
            if (!mDirtyTrickManager.Release()) return false;
            // fall through
        case E_RELEASESTAGE_LIVE_REVENGE_MANAGER:
            meReleaseStage = E_RELEASESTAGE_LIVE_REVENGE_MANAGER;
            if (!mLiveRevengeManager.Release()) return false;
            // fall through
        case E_RELEASESTAGE_AGGRESSIVE_DRIVING_MANAGER:
            meReleaseStage = E_RELEASESTAGE_AGGRESSIVE_DRIVING_MANAGER;
            if (!mAggressiveDrivingManager.Release()) return false;
            // fall through
        case E_RELEASESTAGE_SCOREBOARD_MANAGER:
            meReleaseStage = E_RELEASESTAGE_SCOREBOARD_MANAGER;
            if (!mScoreboardManager.Release()) return false;
            // fall through
        case E_RELEASESTAGE_STATS_MANAGER:
            meReleaseStage = E_RELEASESTAGE_STATS_MANAGER;
            if (!mStatsManager.Release()) return false;
            // fall through
        case E_RELEASESTAGE_TRAFFIC_SYNC_MANAGER:
            meReleaseStage = E_RELEASESTAGE_TRAFFIC_SYNC_MANAGER;
            if (!mTrafficManager.Release()) return false;
            // fall through
        case E_RELEASESTAGE_STANDINGS_MANAGER:
            meReleaseStage = E_RELEASESTAGE_STANDINGS_MANAGER;
            if (!mStandingsManager.Release()) return false;
            // fall through
        case E_RELEASESTAGE_BUDDY_MANAGER:
            meReleaseStage = E_RELEASESTAGE_BUDDY_MANAGER;
            mBuddyManager.Disconnect();
            if (!mBuddyManager.Release()) return false;
            // fall through
        case E_RELEASESTAGE_TEXTURE_COMPRESSOR:
            meReleaseStage = E_RELEASESTAGE_TEXTURE_COMPRESSOR;
            if (!mTextureCompressor.Release()) return false;
            // fall through
        case E_RELEASESTAGE_CAMERA:
            meReleaseStage = E_RELEASESTAGE_CAMERA;
            if (!mCamera.Release()) return false;
            // fall through
        case E_RELEASESTAGE_POSTROUND_MANAGER:
            meReleaseStage = E_RELEASESTAGE_POSTROUND_MANAGER;
            if (!mPostRoundManager.Release()) return false;
            // fall through
        case E_RELEASESTAGE_SUSPENSION_MANAGER:
            meReleaseStage = E_RELEASESTAGE_SUSPENSION_MANAGER;
            if (!mSuspensionManager.Release()) return false;
            // fall through
        case E_RELEASESTAGE_MATCHMAKING_MANAGER:
            meReleaseStage = E_RELEASESTAGE_MATCHMAKING_MANAGER;
            if (!mMatchMakingManager.Release()) return false;
            // fall through
        case E_RELEASESTAGE_CONNECTION_MANAGER:
            meReleaseStage = E_RELEASESTAGE_CONNECTION_MANAGER;
            if (!mConnectionManager.Release()) return false;
            // fall through
        case E_RELEASESTAGE_LAUNCH_MANAGER:
            meReleaseStage = E_RELEASESTAGE_LAUNCH_MANAGER;
            if (!mLaunchManager.Release()) return false;
            // fall through
        case E_RELEASESTAGE_LOGIN_MANAGER:
            meReleaseStage = E_RELEASESTAGE_LOGIN_MANAGER;
            if (!mLoginManager.Release()) return false;
            // fall through
        case E_RELEASESTAGE_SERVER_INTERFACE:
            meReleaseStage = E_RELEASESTAGE_SERVER_INTERFACE;
            if (!mServerInterface.Release()) return false;
            // fall through
        case E_RELEASESTAGE_BASE_NETWORK_MANAGER:
            meReleaseStage = E_RELEASESTAGE_BASE_NETWORK_MANAGER;
            if (!CgsNetwork::NetworkManager::Release()) return false;
            // fall through

        case E_RELEASESTAGE_DONE:
            meReleaseStage = E_RELEASESTAGE_DONE;
            mServerInterface.ReleaseMemoryBuffer();
            return true;

        default:
            CGS_ASSERT(false, CGS_ASSERT_MESSAGE_AT(
                "unknown release stage",
                __FILE__, 1210 /* ARTIST 0x8258E230 */));
            return false;
        }
    }

    // ----------------------------------------------------------------------------------------
    // BrnNetworkManager::Destruct
    //
    // Reset the session scalars, then tear every sub-manager down in reverse construction
    // order (the scoreboard manager has no tear-down here), the server interface and the
    // CgsNetwork base last, and finally clear the two disk-error latches.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::Destruct()
    {
        mFreeBurnWheelID        = 0;
        mPlayerIDStatsGet       = -1;
        mFreeBurnCarID          = 0;
        mfField86A90            = 0.0f;
        meCurrentDistrict       = BrnWorld::E_DISTRICT_CRISTAL_SUMMIT;
        mu16CarColourIndex      = 0;
        mu16PaintFinishIndex    = 0;
        mTime.SetFloatVal(0.0f);
        mfTimeStep              = 0.0f;
        miFrameNum              = 0;
        meLocalConsoleFrameRate = CgsSystem::E_FRAMERATE_UNKNOWN;

        mTeamSelectionManager.Destruct();
        mAutoLoginManager.Destruct();
        mEventScoresManager.Destruct();
        mGamerCardManager.Destruct();
        mChallengeSuccessManager.Destruct();
        mRoadRulesManager.Destruct();
        mStateManager.Destruct();
        mMarkedManManager.Destruct();
        mSelectedRoutesManager.Destruct();
        mNetworkInviteManager.Destruct();
        mNetworkNotificationManager.Destruct();
        mGamerPictureManager.Destruct();
        mImageManager.Destruct();
        mDirtyTrickManager.Destruct();
        mLiveRevengeManager.Destruct();
        mAggressiveDrivingManager.Destruct();
        mStatsManager.Destruct();
        mBuddyManager.Destruct();
        mTextureCompressor.Destruct();
        mCamera.Destruct();
        mTrafficManager.Destruct();
        mStandingsManager.Destruct();
        mPostRoundManager.Destruct();
        mSuspensionManager.Destruct();
        mMatchMakingManager.Destruct();
        mConnectionManager.Destruct();
        mLaunchManager.Destruct();
        mLoginManager.Destruct();
        mServerInterface.Destruct();

        CgsNetwork::NetworkManager::Destruct();

        mbNotifyGameOfDisconnectAfterDiskError = false;
        mbDiskReadErrorThisFrame               = false;
    }
}

// FOLDED FROM BrnNetworkManager_wN1_02.cpp
#include "types.hpp"

#include "GameSource/Network/BrnNetworkManager.h"
#include "GameSource/Network/BrnNetworkModule.h"                                            // mpNetworkModule accessors
#include "GameSource/Network/BrnNetworkModuleIO.h"                                          // OutputBuffer::SetInvitesOpen
#include "GameSource/Network/BrnNetworkInEventTypeDefs.h"                                   // NetworkInDxtDecodeImageEvent
#include "GameSource/Network/BrnNetworkOutEventTypeDefs.h"                                  // NetworkOutLocalPlayerDisconnected, NetworkOutImageDxtDecodedEvent
#include "GameSource/Network/BrnServerInterface.h"                                          // component accessors, Update
#include "GameSource/Network/Components/BrnServerInterfaceTelemetry.h"                      // GetTelemetryComponent
#include "GameSource/Network/Parameters/BrnNetworkPlayerInfoData.h"                         // IsLocalPlayer
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h"              // NetworkToGameStateInterface::SetFramesSinceStart
#include "GameSource/GameState/BrnGameEvents.h"                                             // LocalPlayerDisconnectedEvent
#include "GameSource/Resource/BrnResourceAllocator.h"                                       // GetAvailableMemory, BRN_RESOURCE_MEMORY_CHECK
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"                                          // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                                  // the memory checks' log line
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"                   // CgsDev::PerfMonCpu::Start/StopMonitor
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"                                 // CgsGui::GuiEventNetworkDisconnected
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"                        // OnRoundFinish, SetDiskAccessible
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterfaceErrors.h"        // E_SERVER_INTERFACE_CONNECTION_ERROR_DISCONNECT
#include "GameShared/GameClasses/Network/Texture/CgsNetworkTexture.h"                       // ProcessNetworkTextureDecodeEvent
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"                   // KI_E_MESSAGE_TYPE_COUNT
#include "GameShared/GameClasses/Network/Time/CgsTimeManager.h"                             // SetStartFrame / GetU16FrameCount
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfo.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceTelemetry.h"
#include "GameShared/GameClasses/System/Timer/CgsTime.h"                                    // CgsSystem::Time
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"                    // CgsSystem::TimerStatus

// ============================================================================================
// BrnNetwork::BrnNetworkManager -- per-frame entry, session hooks and the small queries.
//
// Every sub-object is reached through its typed accessor (GetTrafficManager(), GetTimeManager(),
// ...); the base-class session hooks are called by their qualified names.
//
// Bodied here: ProcessBeforeSimulation, ProcessAfterSimulation, ProcessHLUpdateFlags,
// SyncTimeClientReadyCallback, GetLocalConsoleFrameRate, OnEnterGame, OnLeaveGame, OnLogIn,
// OnAutoLogin, OnAutoLoginProcessComplete, OnFinishedBoot, OnGameLaunching, OnRoundStart,
// OnRoundFinish, OnGameFinish, OnGameStart, JoinGameSession, CaptureTelemetryEvent,
// GetActiveRaceCarIndex, IsLocalPlayer, IsDoingFreeBurnLobby, GetMaxMessageSize,
// DxtDecodeCallback, ProcessNetworkTextureDecodeEvent.
// ============================================================================================

// Asserts pass __FILE__; ARTIST's reads "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Network/BrnNetworkManager.cpp".

namespace GsmIO = BrnGameState::GameStateModuleIO;

namespace BrnNetwork
{
    // ----------------------------------------------------------------------------------------
    // Latch this frame's game timer (its time, frame count and base time step), apply the
    // high-level update flags, then give every sub-manager its pre-simulation tick in the
    // console order and publish the player status / results and the invite state.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::ProcessBeforeSimulation(const BrnNetworkModuleIO::PreSimulationInputBuffer* lpInputBuffer,
                                                    BrnNetworkModuleIO::OutputBuffer*                  lpOutputBuffer,
                                                    CgsSystem::TimerStatus*                            lpTimerStatus,
                                                    BrnUpdateSet                                       luUpdateSet)
    {
        mpGameTimerStatus = lpTimerStatus;
        mTime             = lpTimerStatus->GetTime();
        miFrameNum        = mpGameTimerStatus->GetFrameCount();
        mfTimeStep        = mpGameTimerStatus->GetBaseTimeStep();

        ProcessHLUpdateFlags(luUpdateSet);

        GetAggressiveDrivingManager()->ProcessBeforeSimulation(lpOutputBuffer, mfTimeStep);
        GetStatsManager()->ProcessBeforeSimulation(lpOutputBuffer);
        GetLiveRevengeManager()->ProcessBeforeSimulation(lpOutputBuffer);
        GetScoreboardManager()->ProcessBeforeSimulation(lpOutputBuffer);
        GetStandingsManager()->Update(mbDiskReadErrorThisFrame);
        GetDirtyTrickManager()->ProcessBeforeSimulation(lpOutputBuffer);
        GetNetworkImageManager()->ProcessBeforeSimulation(lpOutputBuffer);
        GetChallengeSuccessManager()->ProcessBeforeSimulation(lpOutputBuffer);
        GetGamerCardManager()->ProcessBeforeSimulation();
        GetEventScoresManager()->ProcessBeforeSimulation(mfTimeStep);
        GetAutoLoginManager()->ProcessBeforeSimulation(mfTimeStep, luUpdateSet);
        GetTeamSelectionManager()->ProcessBeforeSimulation(lpOutputBuffer);
        GetRoadRulesManager()->ProcessBeforeSimulation(lpOutputBuffer, mfTimeStep, luUpdateSet);

        OutputPlayerStatusInfo(lpOutputBuffer);
        OutputPlayerResultsInfo(lpOutputBuffer);

        lpOutputBuffer->SetInvitesOpen(GetBuddyManager()->AreAnyInvitesOpen());
    }

    // ----------------------------------------------------------------------------------------
    // Post-simulation tick: apply the high-level update flags, then run every sub-manager in the
    // console order, most bracketed by a CPU monitor, and finally advance the network frame and
    // publish the frames-since-start count to the game state. The free memory read on entry is
    // re-checked after nearly every leg.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::ProcessAfterSimulation(const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInputBuffer,
                                                   BrnUpdateSet                                        luUpdateSet)
    {
        u32 luAvailableMemory = BrnResource::GetAvailableMemory();
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

        const bool lbAreWePlaying = GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame();

        ProcessHLUpdateFlags(luUpdateSet);

        CgsDev::PerfMonCpu::StartMonitor(miNetworkStateManagerPM);
        GetStateManager()->UpdateEvents(lpInputBuffer);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkStateManagerPM);

        CgsDev::PerfMonCpu::StartMonitor(miNetworkServerInterfacePM);
        GetServerInterface()->Update(lpInputBuffer);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkServerInterfacePM);

        CgsDev::PerfMonCpu::StartMonitor(miCGSNetworkAfterSimPM);
        CgsNetwork::NetworkManager::Update(mpGameTimerStatus, GetTimeManager()->GetU16FrameCount(), lbAreWePlaying);
        CgsDev::PerfMonCpu::StopMonitor(miCGSNetworkAfterSimPM);
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

        CgsDev::PerfMonCpu::StartMonitor(miNetworkBuddyPM);
        GetBuddyManager()->Update(lpInputBuffer, mfTimeStep,
                                  GetStateManager()->IsReadyToJoinGameSession(),
                                  GetStateManager()->IsReadyToJoinGameSession());
        CgsDev::PerfMonCpu::StopMonitor(miNetworkBuddyPM);
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

        CgsDev::PerfMonCpu::StartMonitor(miNetworkTrafficPM);
        GetTrafficManager()->Update(lbAreWePlaying);
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkTrafficPM);

        CgsDev::PerfMonCpu::StartMonitor(miNetworkAggressiveDrivingPM);
        GetAggressiveDrivingManager()->ProcessAfterSimulation(lpInputBuffer);
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkAggressiveDrivingPM);

        CgsDev::PerfMonCpu::StartMonitor(miNetworkDirtyTrickPM);
        GetDirtyTrickManager()->ProcessAfterSimulation(lpInputBuffer);
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkDirtyTrickPM);

        GetNetworkImageManager()->ProcessAfterSimulation(lpInputBuffer);
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

        CgsDev::PerfMonCpu::StartMonitor(miGamerPicUpdatPM);
        GetGamerPictureManager()->Update();
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        CgsDev::PerfMonCpu::StopMonitor(miGamerPicUpdatPM);

        CgsDev::PerfMonCpu::StartMonitor(miCameraUpdatePM);
        GetCamera()->Update();
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        CgsDev::PerfMonCpu::StopMonitor(miCameraUpdatePM);

        GetTextureCompressor()->Update();
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

        CgsDev::PerfMonCpu::StartMonitor(miNetworkNotificationPM);
        GetNetworkNotificationManager()->Update();
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkNotificationPM);

        CgsDev::PerfMonCpu::StartMonitor(miNetworkInvitePM);
        GetNetworkInviteManager()->Update();
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkInvitePM);

        CgsDev::PerfMonCpu::StartMonitor(miNetworkStatsPM);
        GetStatsManager()->ProcessAfterSimulation(lpInputBuffer);
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkStatsPM);

        CgsDev::PerfMonCpu::StartMonitor(miNetworkLiveRevengePM);
        GetLiveRevengeManager()->ProcessAfterSimulation(lpInputBuffer);
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkLiveRevengePM);

        CgsDev::PerfMonCpu::StartMonitor(miNetworkRoutesManagerPM);
        GetSelectedRoutesManager()->Update();
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkRoutesManagerPM);

        CgsDev::PerfMonCpu::StartMonitor(miNetworkStateManagerPM);
        GetStateManager()->Update(lpInputBuffer);
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkStateManagerPM);

        CgsDev::PerfMonCpu::StartMonitor(miNetworkRoadRulesManagerPM);
        GetRoadRulesManager()->ProcessAfterSimulation(lpInputBuffer, lbAreWePlaying);
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkRoadRulesManagerPM);

        CgsDev::PerfMonCpu::StartMonitor(miNetworkChallengeManagerPM);
        GetChallengeSuccessManager()->ProcessAfterSimulation(lpInputBuffer);
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkChallengeManagerPM);

        // A route change made while in a game republishes the game parameters.
        CgsDev::PerfMonCpu::StartMonitor(miNetworkManagerSelectedRoutesPM);
        if (GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame() &&
            GetSelectedRoutesManager()->HaveRoutesChanged())
        {
            GetSelectedRoutesManager()->SetRoutesChanged(false);
            OutputGameParameters();
        }
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkManagerSelectedRoutesPM);

        GetEventScoresManager()->ProcessAfterSimulation(lpInputBuffer);
        GetTeamSelectionManager()->ProcessAfterSimulation();

        CgsDev::PerfMonCpu::StartMonitor(miNetworkManagerOutputPlayerStatsPM);
        OutputPlayerStatsToGui(mPlayerIDStatsGet);
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkManagerOutputPlayerStatsPM);

        CgsDev::PerfMonCpu::StartMonitor(miNetworkManagerPostUpdatePM);
        CgsNetwork::NetworkManager::PostUpdate(mpGameTimerStatus, GetTimeManager()->GetU16FrameCount());
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkManagerPostUpdatePM);

        CgsDev::PerfMonCpu::StartMonitor(miNetworkManagerMiscPM);
        GetTimeManager()->NextFrame(mpGameTimerStatus);
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        mpGameTimerStatus = NULL;
        const s32 liFramesSinceStart = GetTimeManager()->GetFramesSinceStart();
        mpNetworkModule->GetNetworkToGameStateInterface()->SetFramesSinceStart(liFramesSinceStart);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkManagerMiscPM);
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
    }

    // ----------------------------------------------------------------------------------------
    // Bit 0x200 of the update set is the disk-read-error flag. While it is raised the manager
    // revokes every invite, marks the disk inaccessible and drops the server connection; once it
    // clears, the disk is accessible again and, if a disconnect was caused by the error, the GUI,
    // the game state and the network event queue are told the local player disconnected.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::ProcessHLUpdateFlags(BrnUpdateSet luUpdateSet)
    {
        CGS_ASSERT(GetPlayerManager(), CGS_ASSERT_MESSAGE_AT(
            "GetPlayerManager()",
            __FILE__, 3260 /* ARTIST 0x82596900 */));

        mbDiskReadErrorThisFrame = (luUpdateSet & 0x200) != 0;
        if (mbDiskReadErrorThisFrame)
        {
            CGS_ASSERT(GetServerInterface(), CGS_ASSERT_MESSAGE_AT(
                "GetServerInterface()",
                __FILE__, 3267 /* ARTIST 0x82596940 */));
            CGS_ASSERT(GetServerInterface()->GetConnectionComponent(), CGS_ASSERT_MESSAGE_AT(
                "GetServerInterface()->GetConnectionComponent()",
                __FILE__, 3268 /* ARTIST 0x82596968 */));

            mbNotifyGameOfDisconnectAfterDiskError = true;
            GetBuddyManager()->RevokeAllInvites();
            GetPlayerManager()->SetDiskAccessible(false);
            GetServerInterface()->GetConnectionComponent()->DisconnectFromServer();
        }
        else
        {
            GetPlayerManager()->SetDiskAccessible(true);
            if (mbNotifyGameOfDisconnectAfterDiskError)
            {
                CgsGui::GuiEventNetworkDisconnected lDisconnectedEvent;
                lDisconnectedEvent.meLastError  = CgsNetwork::E_SERVER_INTERFACE_CONNECTION_ERROR_DISCONNECT;
                lDisconnectedEvent.macReason[0] = '\0';
                mpNetworkModule->AddOutputGuiEvent(lDisconnectedEvent);

                // One empty record goes onto both queues.
                GsmIO::LocalPlayerDisconnectedEvent lGameDisconnectedEvent;
                mpNetworkModule->GetGameEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lGameDisconnectedEvent),
                    GsmIO::E_EVENT_LOCAL_PLAYER_DISCONNECTED, sizeof(lGameDisconnectedEvent));

                BrnNetworkModuleIO::NetworkOutLocalPlayerDisconnected lNetworkDisconnectedEvent;
                mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lNetworkDisconnectedEvent),
                    lNetworkDisconnectedEvent.GetEventType(), sizeof(lNetworkDisconnectedEvent));

                mbNotifyGameOfDisconnectAfterDiskError = false;
            }
        }
    }

    // ----------------------------------------------------------------------------------------
    // Start-time manager callback: a client finished syncing its clock. While the local player
    // sits in a game that has not started yet (the free-burn lobby), restart the networked
    // traffic for that client.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::SyncTimeClientReadyCallback(NetworkPlayerID lClientReadyID, void* lpUserData)
    {
        BrnNetworkManager* lpNetworkManager = static_cast<BrnNetworkManager*>(lpUserData);
        CGS_ASSERT(lpNetworkManager,
            CGS_ASSERT_MESSAGE_AT("lpNetworkManager", __FILE__, 2276 /* ARTIST 0x82581BD4 */));
        CGS_ASSERT(lpNetworkManager->GetServerInterface(),
            CGS_ASSERT_MESSAGE_AT(
                "lpNetworkManager->GetServerInterface()",
                __FILE__, 2281 /* ARTIST 0x82581BF8 */));
        CGS_ASSERT(lpNetworkManager->GetServerInterface()->GetGameComponent(),
            CGS_ASSERT_MESSAGE_AT(
                "lpNetworkManager->GetServerInterface()->GetGameComponent()",
                __FILE__, 2282 /* ARTIST 0x82581C20 */));

        if (lpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame() &&
            !lpNetworkManager->GetServerInterface()->GetGameComponent()->IsGameStarted())
        {
            CGS_ASSERT(lpNetworkManager->mpNetworkModule,
                CGS_ASSERT_MESSAGE_AT(
                    "lpNetworkManager->mpNetworkModule",
                    __FILE__, 2286 /* ARTIST 0x82581C78 */));
            lpNetworkManager->GetTrafficManager()->RestartNetworkTraffic(lClientReadyID);
        }
    }

    CgsSystem::EFrameRate BrnNetworkManager::GetLocalConsoleFrameRate()
    {
        return meLocalConsoleFrameRate;
    }

    // ----------------------------------------------------------------------------------------
    // Session edges. Each one runs the base-class hook, then the sub-managers that care, and
    // the enter/leave pair invalidates the start-frame bookmark.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::OnEnterGame()
    {
        CgsNetwork::NetworkManager::OnEnterGame();
        GetRoadRulesManager()->OnEnterGame();
        GetTimeManager()->SetStartFrame(CgsNetwork::TimeManager::E_START_FRAME_INVALID, NULL, 0.0f);
    }

    void BrnNetworkManager::OnLeaveGame()
    {
        CgsNetwork::NetworkManager::OnLeaveGame();
        GetLaunchManager()->OnLeaveGame();
        GetSelectedRoutesManager()->OnLeaveGame();
        GetRoadRulesManager()->OnLeaveGame();
        GetBuddyManager()->OnLeaveGame();
        mPlayerIDStatsGet = CgsNetwork::KI_INVALID_PLAYER_ID;
        GetLiveRevengeManager()->OnLeaveGame();
        GetTrafficManager()->OnLeaveGame();
        GetEventScoresManager()->OnLeaveOnline();
        GetTimeManager()->SetStartFrame(CgsNetwork::TimeManager::E_START_FRAME_INVALID, NULL, 0.0f);
    }

    void BrnNetworkManager::OnLogIn()
    {
        GetAutoLoginManager()->Connect();
    }

    void BrnNetworkManager::OnAutoLogin()
    {
        GetRoadRulesManager()->OnAutoLogin();
        GetEventScoresManager()->OnAutoLogin();
        GetLiveRevengeManager()->SendLiveRevengeRivalsToServer();
        GetStatsManager()->UploadOfflineProgression();
    }

    void BrnNetworkManager::OnAutoLoginProcessComplete(u32 luCompletedProcess)
    {
        GetAutoLoginManager()->AutoLoginProcessComplete(luCompletedProcess);
    }

    void BrnNetworkManager::OnFinishedBoot()
    {
        GetAutoLoginManager()->OnFinishedBoot();
    }

    void BrnNetworkManager::OnGameLaunching()
    {
        GetRoadRulesManager()->OnGameLaunching();
        GetTrafficManager()->OnGameLaunching();
    }

    // The round and game edges stamp the base hook with the current network time (rebuilt from
    // its float value) and the 16-bit network frame.
    void BrnNetworkManager::OnRoundStart()
    {
        CgsSystem::Time lStartTime(GetTime().GetFloatVal());
        CgsNetwork::NetworkManager::OnRoundStart(lStartTime, mfTimeStep, GetTimeManager()->GetU16FrameCount());

        GetStandingsManager()->OnRoundStart();
        GetTrafficManager()->OnRoundStart(IsDoingFreeBurnLobby());
        GetAggressiveDrivingManager()->OnRoundStart();
        GetLiveRevengeManager()->OnRoundStart();
        GetNetworkImageManager()->OnRoundStart();
        GetMarkedManManager()->OnRoundStart();
    }

    void BrnNetworkManager::OnRoundFinish()
    {
        CgsSystem::Time lFinishTime(GetTime().GetFloatVal());
        CgsNetwork::NetworkManager::OnRoundFinish(lFinishTime, GetTimeManager()->GetU16FrameCount());

        GetTrafficManager()->OnRoundFinish();
        GetPlayerManager()->OnRoundFinish();
        GetAggressiveDrivingManager()->OnRoundFinish();
        GetLiveRevengeManager()->OnRoundFinish();
        GetRoadRulesManager()->OnRoundFinish();
    }

    void BrnNetworkManager::OnGameFinish()
    {
        CgsSystem::Time lFinishTime(GetTime().GetFloatVal());
        CgsNetwork::NetworkManager::OnGameFinish(lFinishTime, GetTimeManager()->GetU16FrameCount());

        GetRoadRulesManager()->OnGameFinish();
        GetLiveRevengeManager()->OnGameFinish();
    }

    void BrnNetworkManager::OnGameStart()
    {
        CgsSystem::Time lStartTime(GetTime().GetFloatVal());
        CgsNetwork::NetworkManager::OnGameStart(lStartTime, GetTimeManager()->GetU16FrameCount());

        GetTrafficManager()->OnGameStart();
        GetStatsManager()->OnGameStart();
        GetCamera()->OnGameStart();
    }

    void BrnNetworkManager::JoinGameSession(char* lpcSessionID)
    {
        GetStateManager()->JoinGameSession(lpcSessionID);
    }

    void BrnNetworkManager::CaptureTelemetryEvent(ETelemetryHook leHook, const char* lpcData)
    {
        CGS_ASSERT(GetServerInterface(),
            CGS_ASSERT_MESSAGE_AT("GetServerInterface()", __FILE__, 3121 /* ARTIST 0x825821A8 */));
        CGS_ASSERT(GetServerInterface()->GetTelemetryComponent(),
            CGS_ASSERT_MESSAGE_AT(
                "GetServerInterface()->GetTelemetryComponent()", __FILE__, 3122 /* ARTIST 0x825821D0 */));
        GetServerInterface()->GetTelemetryComponent()->CaptureEvent(leHook, lpcData);
    }

    EActiveRaceCarIndex BrnNetworkManager::GetActiveRaceCarIndex(NetworkPlayerID lPlayerID)
    {
        CGS_ASSERT(mpNetworkModule, CGS_ASSERT_MESSAGE_AT(
            "mpNetworkModule",
            CGS_ARTIST_FILE, 3311 /* ARTIST 0x825822E4 */));
        return mpNetworkModule->GetActiveRaceCarIndex(lPlayerID);
    }

    // The local player's id comes from the server interface's player-info component.
    bool BrnNetworkManager::IsLocalPlayer(NetworkPlayerID lPlayerID)
    {
        PlayerInfoData lPlayerInfo;
        lPlayerInfo.Prepare();
        GetServerInterface()->GetPlayerInfoComponent()->GetLocalPlayerInfo(&lPlayerInfo);
        return lPlayerInfo.GetID() == lPlayerID;
    }

    // In a game whose session has not started: the free-burn lobby.
    bool BrnNetworkManager::IsDoingFreeBurnLobby()
    {
        CgsNetwork::ServerInterfaceGames* lpGameComponent = GetServerInterface()->GetGameComponent();
        return lpGameComponent->IsLocalPlayerInGame() && !lpGameComponent->IsGameStarted();
    }

    // ----------------------------------------------------------------------------------------
    // The largest message size over every message type, or over the reliable ones only.
    // Construct checks the two maxima against KI_MAX_MESSAGE_SIZE and
    // KI_MAX_RELIABLE_MESSAGE_SIZE.
    // ----------------------------------------------------------------------------------------
    s32 BrnNetworkManager::GetMaxMessageSize(bool lbReliableOnly)
    {
        struct MessageDef
        {
            s32  miMessageSize;
            bool mbReliable;
        };

        // Indexed by message type. The sizes are the console byte sizes of the message classes
        // (the same console widths KI_MAX_MESSAGE_SIZE / KI_MAX_RELIABLE_MESSAGE_SIZE are written
        // in), not host sizeof values.
        const MessageDef laMessageDefs[CgsNetwork::KI_E_MESSAGE_TYPE_COUNT] =
        {
            {    0, false }, {   40, true  }, {   56, false }, {   84, false },   //  0 ..  3
            {   40, false }, {   96, true  }, {   36, false }, {   36, false },   //  4 ..  7
            {   32, false }, {   68, false }, {   36, false }, {  176, false },   //  8 .. 11
            {   64, true  }, {   88, true  }, {   40, false }, {   72, true  },   // 12 .. 15
            { 1968, false }, {  448, false }, {   52, true  }, {  568, true  },   // 16 .. 19
            {   36, false }, {   44, true  }, {   44, true  }, {   88, true  },   // 20 .. 23
            {  160, true  }, {   48, true  }, {  288, true  }, {   56, true  },   // 24 .. 27
            {   92, true  }, {   56, true  }, {   44, true  }, {  100, true  },   // 28 .. 31
            {   36, false }, {   48, true  }, {   64, true  }, {  296, true  },   // 32 .. 35
            {   48, false }, {   56, true  }, {   88, true  }, {   44, true  },   // 36 .. 39
            {   48, false }, {   44, true  }, {   52, true  }, {  116, true  },   // 40 .. 43
        };

        s32 liMaxMsgSize = 0;
        for (s32 liMsgType = 0; liMsgType < CgsNetwork::KI_E_MESSAGE_TYPE_COUNT; ++liMsgType)
        {
            if (!lbReliableOnly || laMessageDefs[liMsgType].mbReliable)
            {
                const s32 liMessageSize = laMessageDefs[liMsgType].miMessageSize;
                liMaxMsgSize = (liMaxMsgSize > liMessageSize) ? liMaxMsgSize : liMessageSize;
            }
        }
        return liMaxMsgSize;
    }

    // ----------------------------------------------------------------------------------------
    // Texture compressor callback: a DXT decode finished. Queue the decoded pixels, tagged with
    // the player the image belongs to, onto the module's network event queue.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::DxtDecodeCallback(void* lpPixels, void* lpUserData)
    {
        CGS_ASSERT(lpPixels, CGS_ASSERT_MESSAGE_AT(
            "lpPixels",
            CGS_ARTIST_FILE, 3236 /* ARTIST 0x82593D10 */));
        CGS_ASSERT(lpUserData, CGS_ASSERT_MESSAGE_AT(
            "lpUserData",
            CGS_ARTIST_FILE, 3237 /* ARTIST 0x82593D34 */));
        BrnNetworkManager* lpNetworkManager = static_cast<BrnNetworkManager*>(lpUserData);
        CGS_ASSERT(lpNetworkManager, CGS_ASSERT_MESSAGE_AT(
            "lpNetworkManager",
            CGS_ARTIST_FILE, 3240 /* ARTIST 0x82593D50 */));

        BrnNetworkModuleIO::NetworkOutImageDxtDecodedEvent lNetworkOutImageDxtDecodedEvent;
        lNetworkOutImageDxtDecodedEvent.mpDecodedPixels = lpPixels;
        lNetworkOutImageDxtDecodedEvent.mPlayerName     = lpNetworkManager->mExportPlayerName;

        CGS_ASSERT(lpNetworkManager->mpNetworkModule, CGS_ASSERT_MESSAGE_AT(
            "lpNetworkManager->mpNetworkModule",
            CGS_ARTIST_FILE, 3245 /* ARTIST 0x82593D98 */));
        CGS_ASSERT(lpNetworkManager->mpNetworkModule->GetNetworkEventQueue(), CGS_ASSERT_MESSAGE_AT(
            "lpNetworkManager->mpNetworkModule->GetNetworkEventQueue()",
            CGS_ARTIST_FILE, 3246 /* ARTIST 0x82593DC4 */));
        lpNetworkManager->mpNetworkModule->GetNetworkEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lNetworkOutImageDxtDecodedEvent),
            lNetworkOutImageDxtDecodedEvent.GetEventType(), sizeof(lNetworkOutImageDxtDecodedEvent));
    }

    // ----------------------------------------------------------------------------------------
    // A received player image needs decoding: remember whose it is and hand the DXT1 pixels to
    // the texture compressor, which calls DxtDecodeCallback when the decode job completes.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::ProcessNetworkTextureDecodeEvent(
            const BrnNetworkModuleIO::NetworkInDxtDecodeImageEvent* lpDxtDecodeRequestEvent)
    {
        CGS_ASSERT(lpDxtDecodeRequestEvent, CGS_ASSERT_MESSAGE_AT(
            "lpDxtDecodeRequestEvent",
            __FILE__, 3208 /* ARTIST 0x82596828 */));
        CGS_ASSERT(renderengine::PIXELFORMAT_DXT1 == lpDxtDecodeRequestEvent->mpTextureToDecode->GetFormat(), CGS_ASSERT_MESSAGE_AT(
            "rw::graphics::core::PIXELFORMAT_LIN_DXT1 == lpDxtDecodeRequestEvent->mpTextureToDecode->GetFormat()",
            __FILE__, 3209 /* ARTIST 0x8259685C */));

        mExportPlayerName = lpDxtDecodeRequestEvent->mPlayerName;

        const CgsNetwork::NetworkTexture* lpTexture = lpDxtDecodeRequestEvent->mpTextureToDecode;
        GetTextureCompressor()->SetNewTextureToDecompress(lpTexture->GetTexture(),
                                                          lpTexture->GetTextureSize(),
                                                          lpTexture->GetWidth(),
                                                          lpTexture->GetHeight(),
                                                          DxtDecodeCallback,
                                                          this);
    }
}

// FOLDED FROM BrnNetworkManager_wN1_03.cpp
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT at ARTIST locations

#include "types.hpp"                                                                         // intptr_t
#include <cstring>                                                                         // strlen / strncpy

#include "GameSource/Network/BrnNetworkManager.h"
#include "GameSource/Network/BrnNetworkModule.h"                                           // AddOutputGuiEvent / Get*EventQueue
#include "GameSource/Network/BrnNetworkModuleIO.h"                                         // OutputBuffer status / lobby / results interfaces
#include "GameSource/Network/BrnNetworkOutEventTypeDefs.h"                                 // NetworkOut* records
#include "GameSource/Network/BrnNetworkPlayer.h"                                           // IsEliminated / IsFirstUpdateMessage
#include "GameSource/Network/BrnNetworkGameParams.h"                                       // GameParams
#include "GameSource/Network/Parameters/BrnNetworkPlayerParamsClass.h"                     // PlayerParams
#include "GameSource/Network/Parameters/BrnNetworkPlayerInfoData.h"                        // PlayerInfoData
#include "GameSource/GameState/BrnGameEvents.h"                                            // LocalPlayerDisconnectedEvent
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                                      // GuiEventNetworkPlayerImage / CustomMatchResults / CamStatus
#include "GameSource/Gui/Events/BrnGuiEventNetworkPlayerStats.h"                           // GuiEventNetworkPlayerStats
#include "GameShared/GameClasses/Core/CgsAssert.h"                                         // CGS_ASSERT
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"                  // PerfMonCpu::Start/StopMonitor
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"                                // GuiEventShowLoginQuestion / GuiEventNetworkDisconnected
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterfaceErrors.h"       // EServerInterfaceError
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceEvents.h" // EServerInterfaceEvent
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfo.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceTelemetry.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/X360/CgsServerInterfaceGamesX360.h" // SetSessionFlags
// ============================================================================================
// BrnNetwork::BrnNetworkManager -- event handling and GUI / game output (partfile).
//
// The two raised-event entry points (the login state machine's and the server interface's),
// the player-manager event callback, and the per-frame writers of the player status, lobby
// and results interfaces, the game-parameter event and the GUI stats / image events.
// ============================================================================================

namespace BrnNetwork
{
    // ----------------------------------------------------------------------------------------
    // BrnNetworkManager::TriggerEventFromLogin
    //
    // Every "show question" login event becomes a GuiEventShowLoginQuestion carrying the
    // matching question; PROCEED needs no GUI; anything else is a bad event.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::TriggerEventFromLogin(ELoginEvent leEvent, void* lpEventData)
    {
        (void)lpEventData;

        CgsGui::GuiEventShowLoginQuestion lShowQuestionEvent;

        switch (leEvent)
        {
        case E_LOGIN_EVENT_SHOW_TOS:
            lShowQuestionEvent.meLoginQuestion = CgsGui::E_LOGIN_QUESTION_TOS;
            break;
        case E_LOGIN_EVENT_SHOW_CREATE_ACCOUNT:
            lShowQuestionEvent.meLoginQuestion = CgsGui::E_LOGIN_QUESTION_CREATE_ACCOUNT;
            break;
        case E_LOGIN_EVENT_SHOW_SHARE:
            lShowQuestionEvent.meLoginQuestion = CgsGui::E_LOGIN_QUESTION_SHARE;
            break;
        case E_LOGIN_EVENT_SHOW_OPEN_US_ACCOUNT:
            lShowQuestionEvent.meLoginQuestion = CgsGui::E_LOGIN_QUESTION_OPEN_US_ACCOUNT;
            break;
        case E_LOGIN_EVENT_SHOW_NO_AGREEMENT:
            lShowQuestionEvent.meLoginQuestion = CgsGui::E_LOGIN_QUESTION_NO_AGREEMENT;
            break;
        case E_LOGIN_EVENT_SHOW_SIGN_IN:
            lShowQuestionEvent.meLoginQuestion = CgsGui::E_LOGIN_QUESTION_SHOW_SIGN_IN;
            break;
        case E_LOGIN_EVENT_SHOW_CHAT_RESTRICTED:
            lShowQuestionEvent.meLoginQuestion = CgsGui::E_LOGIN_QUESTION_CHAT_RESTRICTION;
            break;
        case E_LOGIN_EVENT_PROCEED:
            return;
        default:
            CGS_ASSERT(false, CGS_ASSERT_MESSAGE_AT(
                "Invalid login event",
                __FILE__, 1556 /* ARTIST 0x825959D8 */));
            return;
        }

        GetNetworkModule()->AddOutputGuiEvent(lShowQuestionEvent);
    }

    // ----------------------------------------------------------------------------------------
    // BrnNetworkManager::UpdateMenuDataFromPlayerParams
    //
    // A player's lobby parameters changed: refresh that player's menu data (marked man, free
    // burn car, paint) from them and, for a remote player, tell the game its car and paint.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::UpdateMenuDataFromPlayerParams(s32 liServerInterfacePlayerIndex)
    {
        PlayerParams lPlayerParams;
        lPlayerParams.Prepare();
        GetServerInterface()->GetGameComponent()->GetPlayerParametersByIndex(liServerInterfacePlayerIndex,
                                                                             &lPlayerParams);

        PlayerMenuData* lpPlayerMenuData =
            static_cast<PlayerMenuData*>(GetPlayerManager()->GetMenuDataByID(lPlayerParams.GetID()));
        CGS_ASSERT(lpPlayerMenuData != NULL, "Couldn't find menu data for playerID ");

        lpPlayerMenuData->mMarkedManID = lPlayerParams.GetMarkedPlayerID();
        lPlayerParams.GetFreeBurnCarID(&lpPlayerMenuData->mFreeBurnCarId);
        lpPlayerMenuData->mu16FreeburnCarColourIndex   = lPlayerParams.GetCarColourIndex();
        lpPlayerMenuData->mu16FreeburnPaintFinishIndex = lPlayerParams.GetPaintFinishIndex();
        lpPlayerMenuData->mfUnknown4C = lPlayerParams.IsCarDeformed() ? 0.85f : 0.0f;
        CGS_ASSERT(lpPlayerMenuData->mu16FreeburnPaintFinishIndex < 4, "Invalid Paint Finish: ");

        PlayerInfoData lPlayerInfo;
        lPlayerInfo.Prepare();
        GetServerInterface()->GetPlayerInfoComponent()->GetLocalPlayerInfo(&lPlayerInfo);

        if (lPlayerParams.GetID() != lPlayerInfo.GetID())
        {
            BrnNetworkModuleIO::NetworkOutPlayerChangedCarEvent lChangedCarEvent;
            lChangedCarEvent.SetNetworkPlayerID(lPlayerParams.GetID());
            lChangedCarEvent.SetModelID(lpPlayerMenuData->mFreeBurnCarId);
            lChangedCarEvent.SetWheelID(lpPlayerMenuData->mFreeBurnWheelId);
            lChangedCarEvent.mf10 = lpPlayerMenuData->mfUnknown4C;
            GetNetworkModule()->GetNetworkEventQueue()->AddEvent(lChangedCarEvent,
                                                                 lChangedCarEvent.GetEventType());

            BrnNetworkModuleIO::NetworkOutPlayerChangedCarColourEvent lCarColourEvent;
            lCarColourEvent.SetNetworkPlayerID(lPlayerParams.GetID());
            lCarColourEvent.SetActiveRaceCarIndex(
                GetNetworkModule()->GetActiveRaceCarIndex(lPlayerParams.GetID()));
            lCarColourEvent.SetCarColourIndex(lpPlayerMenuData->mu16FreeburnCarColourIndex);
            lCarColourEvent.SetPaintFinishIndex(lpPlayerMenuData->mu16FreeburnPaintFinishIndex);
            GetNetworkModule()->GetNetworkEventQueue()->AddEvent(lCarColourEvent,
                                                                 lCarColourEvent.GetEventType());
        }
    }

    // ----------------------------------------------------------------------------------------
    // BrnNetworkManager::OutputPlayerStatsToGui
    //
    // Send one player's online stats to the GUI. lPlayerID == invalid means "retry the
    // pending request", which is only resent when the cached stats are newer than the last
    // ones sent (or have just been recalculated). The name and rank come from the lobby
    // parameters for a player in the game, from the local player info otherwise. When the
    // stats are not downloaded yet the request is parked in mPlayerIDStatsGet.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::OutputPlayerStatsToGui(NetworkPlayerID lPlayerID)
    {
        if (!GetServerInterface()->GetConnectionComponent()->IsLoggedIn())
        {
            return;
        }

        if (lPlayerID == CgsNetwork::K_INVALID_PLAYER_ID)
        {
            lPlayerID = GetCamera()->GetRequestedPlayerID();
            if (lPlayerID == CgsNetwork::K_INVALID_PLAYER_ID)
            {
                return;
            }

            const NetworkPlayerStats* lpPlayerStats = GetStatsManager()->GetPlayerStatsByID(lPlayerID);
            if (lpPlayerStats == NULL)
            {
                return;
            }

            if (!(lpPlayerStats->GetTimeStamp() > mLastStatsSentToOnlinePlayStamp) &&
                !lpPlayerStats->IsCalculated())
            {
                return;
            }
            mPlayerIDStatsGet = lPlayerID;
        }

        const char* lpcPlayerName;
        s32         liRank;

        // The local player's name is read out of lPlayerInfo after the branch, so the record
        // lives at function scope (the console frame keeps it; a block scope would not).
        PlayerInfoData lPlayerInfo;

        CgsNetwork::ServerInterfaceGames* lpGamesComponent = GetServerInterface()->GetGameComponent();
        if (lpGamesComponent->IsPlayerInGame(lPlayerID))
        {
            PlayerParams lPlayerParams;
            lPlayerParams.Prepare();
            lpGamesComponent->GetPlayerParametersByPlayerID(lPlayerID, &lPlayerParams);
            lpcPlayerName = lPlayerParams.GetName();
            liRank        = lPlayerParams.GetRank();
        }
        else
        {
            lPlayerInfo.Prepare();
            GetServerInterface()->GetPlayerInfoComponent()->GetLocalPlayerInfo(&lPlayerInfo);
            CGS_ASSERT(lPlayerInfo.GetID() == lPlayerID, "Can only get local player stats when not in a game");
            lpcPlayerName = lPlayerInfo.GetName();
            liRank        = lPlayerInfo.GetRank();
        }

        const NetworkPlayerStats* lpPlayerStats = GetStatsManager()->GetPlayerStatsByName(lpcPlayerName);
        if (lpPlayerStats != NULL && lpPlayerStats->GetStatus() == NetworkPlayerStats::E_STATS_AGE_CURRENT)
        {
            BrnGui::GuiEventNetworkPlayerStats lPlayerStatsEvent;
            mPlayerIDStatsGet = CgsNetwork::K_INVALID_PLAYER_ID;

            CGS_ASSERT(std::strlen(lpcPlayerName) < sizeof(lPlayerStatsEvent.macPlayerName), "String too long: ");
            std::strncpy(lPlayerStatsEvent.macPlayerName, lpcPlayerName, sizeof(lPlayerStatsEvent.macPlayerName));
            lPlayerStatsEvent.mPlayerID    = lPlayerID;
            static_cast<BrnNetwork::NetworkPlayerStats&>(lPlayerStatsEvent) = *lpPlayerStats;
            lPlayerStatsEvent.miWorldRank  = liRank;
            GetNetworkModule()->AddOutputGuiEvent(lPlayerStatsEvent);

            mLastStatsSentToOnlinePlayStamp = lpPlayerStats->GetTimeStamp();
        }
        else
        {
            mPlayerIDStatsGet = lPlayerID;
        }
    }

    // ----------------------------------------------------------------------------------------
    // BrnNetworkManager::PackTextureAndSendDisplayEventToGui
    //
    // Hand a received player image to the GUI for display in the given slot.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::PackTextureAndSendDisplayEventToGui(const CgsNetwork::NetworkTexture* lpTexture,
                                                                s32 liTextureIndex)
    {
        CgsDev::PerfMonCpu::StartMonitor(miPackTextureToSendToGuiPM);

        BrnGui::GuiEventNetworkPlayerImage lEvent;
        lEvent.mpTexture      = lpTexture;
        lEvent.miTextureIndex = liTextureIndex;
        GetNetworkModule()->AddOutputGuiEvent(lEvent);

        CgsDev::PerfMonCpu::StopMonitor(miPackTextureToSendToGuiPM);
    }

    // ----------------------------------------------------------------------------------------
    // BrnNetworkManager::OutputPlayerStatusInfo
    //
    // Rebuild the in-game player status interface and the lobby player status interface of
    // the output buffer from every player in the game: identity, race-car slot, marked man,
    // voice and camera state, stats, the live-revenge relationship with the local player,
    // host / local / in-world flags, the lobby car selection and ready state. Then publish
    // the player counts and the local camera status.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::OutputPlayerStatusInfo(BrnNetworkModuleIO::OutputBuffer* lpOutput)
    {
        PlayerParams    lPlayerParams;
        GameParams      lGameParams;   // constructed and destroyed, never read
        PlayerMenuData* lpLocalPlayerMenuData = NULL;

        CGS_ASSERT(lpOutput != NULL, "lpOutput");

        CgsNetwork::PlayerManager*        lpPlayerManager  = GetPlayerManager();
        CgsNetwork::ServerInterfaceGames* lpGamesComponent = GetServerInterface()->GetGameComponent();

        const NetworkPlayerID lLocalPlayerID = lpPlayerManager->GetLocalPlayerID();

        if (lpGamesComponent->IsPlayerInGame(lLocalPlayerID))
        {
            lpOutput->GetInGamePlayerStatusInterface()->SetGameName(lpGamesComponent->GetGameName());
            lpOutput->GetInGamePlayerStatusInterface()->SetLocalPlayerIsHost(lpGamesComponent->IsLocalPlayerHost());

            lpLocalPlayerMenuData = static_cast<PlayerMenuData*>(lpPlayerManager->GetMenuDataByID(lLocalPlayerID));
            CGS_ASSERT(lpLocalPlayerMenuData != NULL, "lpLocalPlayerMenuData");
        }

        s32             liPlayerStatusDataIndex = 0;
        NetworkPlayerID lPlayerID               = CgsNetwork::K_INVALID_PLAYER_ID;

        while (lpPlayerManager->GetNextPlayerID(&lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_ALL_PLAYERS))
        {
            if (!lpGamesComponent->IsPlayerInGame(lPlayerID))
            {
                continue;
            }

            BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerStatusData =
                lpOutput->GetInGamePlayerStatusInterface()->GetPlayerStatusDataForWriting(liPlayerStatusDataIndex);
            PlayerMenuData* lpPlayerMenuData =
                static_cast<PlayerMenuData*>(lpPlayerManager->GetMenuDataByID(lPlayerID));

            lPlayerParams.PreparePattern();
            lpGamesComponent->GetPlayerParametersByPlayerID(lPlayerID, &lPlayerParams);

            lpPlayerStatusData->Clear();
            lpPlayerStatusData->mPlayerName.Construct(lpPlayerMenuData->macName);
            lpPlayerStatusData->mNetworkPlayerID      = lPlayerID;
            lpPlayerStatusData->meActiveRaceCarIndex = GetNetworkModule()->GetActiveRaceCarIndex(lPlayerID);

            if (lpPlayerMenuData->mMarkedManID != CgsNetwork::K_INVALID_PLAYER_ID &&
                lpPlayerManager->GetMenuDataByID(lpPlayerMenuData->mMarkedManID) != NULL)
            {
                lpPlayerStatusData->mMarkedManPlayerID = lpPlayerMenuData->mMarkedManID;
                lpPlayerStatusData->meMarkedManActiveRaceCarIndex =
                    GetNetworkModule()->GetActiveRaceCarIndex(lpPlayerMenuData->mMarkedManID);
            }

            CGS_ASSERT(lpPlayerMenuData->mu8HeadsetStatus < CgsNetwork::E_NETWORK_HEADSET_PLAYER_STATUS_COUNT,
                       "lpPlayerMenuData->mu8HeadsetStatus < CgsNetwork::E_NETWORK_HEADSET_PLAYER_STATUS_COUNT");
            lpPlayerStatusData->meVOIPStatus = lpPlayerMenuData->mu8HeadsetStatus;

            const NetworkPlayerStats* lpNetworkPlayerStats = GetStatsManager()->GetPlayerStatsByID(lPlayerID);
            if (lpNetworkPlayerStats != NULL)
            {
                lpPlayerStatusData->mPlayerStats = *lpNetworkPlayerStats;
            }

            if (lPlayerID != lLocalPlayerID)
            {
                const LiveRevengeRelationship* lpRelationship =
                    GetLiveRevengeManager()->GetNonConstRevengeRelation(lPlayerID);
                if (lpRelationship != NULL)
                {
                    lpPlayerStatusData->mLiveRevengeRelationship = *lpRelationship;
                }
            }

            lpPlayerStatusData->mbMarkedMan = (lpLocalPlayerMenuData->mMarkedManID == lPlayerParams.GetID());

            if (lLocalPlayerID == lPlayerID)
            {
                lpPlayerStatusData->mbIsEliminated       = false;
                lpPlayerStatusData->mbIsLocalPlayer      = true;
                lpPlayerStatusData->mbIsInLocalGameWorld = true;
            }
            else
            {
                lpPlayerStatusData->mbIsLocalPlayer = false;
                const BrnNetworkPlayer* lpNetworkPlayer =
                    static_cast<const BrnNetworkPlayer*>(lpPlayerManager->GetPlayerByID(lPlayerID));
                lpPlayerStatusData->mbIsInLocalGameWorld = !lpNetworkPlayer->IsFirstUpdateMessage();
                lpPlayerStatusData->mbIsEliminated       = lpNetworkPlayer->IsEliminated();
            }

            lpPlayerStatusData->mbIsHost = (lPlayerID == lpGamesComponent->GetHostPlayerID());

            CGS_ASSERT(lpPlayerMenuData->meCameraStatus >= BrnNetwork::E_CAMERA_STATUS_NONE,
                       "lpPlayerMenuData->meCameraStatus >= BrnNetwork::E_CAMERA_STATUS_NONE");
            CGS_ASSERT(lpPlayerMenuData->meCameraStatus < BrnNetwork::E_CAMERA_STATUS_COUNT,
                       "lpPlayerMenuData->meCameraStatus < BrnNetwork::E_CAMERA_STATUS_COUNT");
            lpPlayerStatusData->meCameraStatus = lpPlayerMenuData->meCameraStatus;

            BrnNetworkModuleIO::LobbyPlayerStatusData* lpPlayerLobbyData =
                lpOutput->GetOnlineLobbyPlayerStatusInterface()->GetPlayerLobbyData(liPlayerStatusDataIndex);
            lpPlayerLobbyData->Clear();
            lpPlayerLobbyData->mbLocalPlayer          = (lPlayerID == lLocalPlayerID);
            lpPlayerLobbyData->mbIsHost               = lpPlayerStatusData->mbIsHost;
            lpPlayerLobbyData->mPlayerID              = lPlayerParams.GetID();
            lpPlayerLobbyData->mbFinalSelection       = lpPlayerMenuData->mbFinalCarSelection;
            lpPlayerLobbyData->mSelectedCarID         = lpPlayerMenuData->mCarId;
            lpPlayerLobbyData->mSelectedWheelID       = lpPlayerMenuData->mWheelId;
            lpPlayerLobbyData->muCarColourIndex       = lPlayerParams.GetCarColourIndex();
            lpPlayerLobbyData->muCarPaintFinishIndex  = lPlayerParams.GetPaintFinishIndex();
            lpPlayerLobbyData->miPlayerColourIndex    = lPlayerParams.GetPlayerColourIndex();
            lpPlayerLobbyData->mbIsCriterion          = lPlayerParams.IsDeveloper();

            if (lpPlayerManager->IsLocalPlayer(lPlayerID))
            {
                lpPlayerLobbyData->meGameConnectionType = 0;
                lpPlayerLobbyData->meVoipConnectionType = 0;
            }
            else
            {
                const CgsNetwork::ConnectionData lConnectionData =
                    lpPlayerManager->GetPlayerByID(lPlayerID)->GetConnectionData();
                lpPlayerLobbyData->meGameConnectionType = lConnectionData.meGameConnectionType;
                lpPlayerLobbyData->meVoipConnectionType = lConnectionData.meVoipConnectionType;
            }

            if (lPlayerParams.IsPlaying())
            {
                lpPlayerLobbyData->meReadyStatus = BrnNetworkModuleIO::LobbyPlayerStatusData::E_READY_STATUS_PLAYING;
            }
            else if (lpPlayerLobbyData->mbIsHost)
            {
                lpPlayerLobbyData->meReadyStatus = BrnNetworkModuleIO::LobbyPlayerStatusData::E_READY_STATUS_HOST;
            }
            else
            {
                lpPlayerLobbyData->meReadyStatus = lPlayerParams.IsReady()
                    ? BrnNetworkModuleIO::LobbyPlayerStatusData::E_READY_STATUS_READY
                    : BrnNetworkModuleIO::LobbyPlayerStatusData::E_READY_STATUS_NOT_READY;
            }

            lpPlayerLobbyData->mePlayerTeam = lPlayerParams.GetPlayerTeam();
            ++liPlayerStatusDataIndex;
        }

        lpOutput->GetInGamePlayerStatusInterface()->SetNumPlayers(liPlayerStatusDataIndex);
        lpOutput->GetInGamePlayerStatusInterface()->SetTotalNumberPlayers(
            lpPlayerManager->GetTotalNumberPlayers(CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED));

        BrnGui::GuiEventCamStatus leCamStatusEvent;
        leCamStatusEvent.meCamStatus = static_cast<ECameraStatus>(GetCamera()->GetLocalCameraStatus());
        GetNetworkModule()->AddOutputGuiEvent(leCamStatusEvent);
    }

    // ----------------------------------------------------------------------------------------
    // BrnNetworkManager::OutputGameParameters
    //
    // Publish the current game's parameters (mode, security, boost, vehicle rules, rounds,
    // time limit, ranked / infinite boost / traffic flags and the selected routes) as a
    // NetworkOutGameParamsChanged event.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::OutputGameParameters()
    {
        BrnNetworkModuleIO::NetworkOutGameParamsChanged lEvent;
        GameParams lGameParams;

        lEvent.Construct();
        lGameParams.Prepare();
        GetServerInterface()->GetGameComponent()->GetGameParameters(&lGameParams);

        lEvent.mbInfiniteBoost     = lGameParams.InfiniteBoost();
        lEvent.miVehicleClass      = lGameParams.VehicleLevelLimit();
        lEvent.mbRanked            = lGameParams.IsRankedGame();
        lEvent.meSecurity          = lGameParams.Security();
        lEvent.meGameMode          = lGameParams.GameMode();
        lEvent.mePreviousGameMode  = lGameParams.PreviousGameMode();
        GetSelectedRoutesManager()->GetRouteData(lEvent.maEvents);
        lEvent.miNumRounds         = lGameParams.NumberRounds();
        lEvent.mbTrafficOn         = lGameParams.IsTrafficOn();
        lEvent.meBoostType         = lGameParams.BoostType();
        lEvent.mbTrafficCheckingOn = lGameParams.IsTrafficCheckingOn();
        lEvent.miNumRunnerCrashes  = lGameParams.NumRunnerCrashes();
        lEvent.meVehicleChoice     = lGameParams.VehicleChoice();
        lEvent.miTimeLimit         = lGameParams.TimeLimit();

        GetNetworkModule()->GetNetworkEventQueue()->AddEvent(lEvent, lEvent.GetEventType());
    }

    // ----------------------------------------------------------------------------------------
    // BrnNetworkManager::TriggerEventFromServerInterface
    //
    // The server interface's events, routed to the managers that care.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::TriggerEventFromServerInterface(CgsNetwork::EServerInterfaceEvent leEvent, void* lpData)
    {
        switch (leEvent)
        {
        case CgsNetwork::E_SERVER_INTERFACE_GENERAL_EVENT_LOBBY_API_CREATED:
            GetPlayerManager()->OnLobbyApiCreated();
            break;

        case CgsNetwork::E_SERVER_INTERFACE_CONNECTION_EVENT_CONNECTED:
            GetNetworkNotificationManager()->Connect();
            GetServerInterface()->GetTelemetryComponent()->CaptureEvent(E_TELEMETRY_NETWORK_CONNECT, NULL);
            break;

        case CgsNetwork::E_SERVER_INTERFACE_CONNECTION_EVENT_DISCONNECTED:
            if (!mbNotifyGameOfDisconnectAfterDiskError)
            {
                CgsGui::GuiEventNetworkDisconnected lDisconnectedEvent;
                lDisconnectedEvent.macReason[0] = 0;

                if (lpData != NULL)
                {
                    const char* lpcReason = static_cast<const char*>(lpData);
                    CGS_ASSERT(std::strlen(lpcReason) < CgsGui::GuiEventNetworkDisconnected::KI_MAX_REASON_LENGTH,
                               "String too long: ");
                    std::strncpy(lDisconnectedEvent.macReason, lpcReason,
                                 CgsGui::GuiEventNetworkDisconnected::KI_MAX_REASON_LENGTH);
                }

                if (GetLoginManager()->GetSignInType() == LoginManagerBase::E_SIGN_IN_TYPE_SILENT)
                {
                    lDisconnectedEvent.meLastError = CgsNetwork::E_SERVER_INTERFACE_ERROR_NONE;
                }
                else
                {
                    CGS_ASSERT(GetServerInterface() != NULL, "GetServerInterface()");
                    lDisconnectedEvent.meLastError =
                        GetServerInterface()->GetLastError(CgsNetwork::E_COMPONENTS_CONNECTION);

                    if (lDisconnectedEvent.meLastError == CgsNetwork::E_SERVER_INTERFACE_ERROR_NONE)
                    {
                        if (GetNetworkAdapter()->HadDuplicateLogin())
                        {
                            lDisconnectedEvent.meLastError =
                                CgsNetwork::E_SERVER_INTERFACE_CONNECTION_ERROR_DISCONNECT_DUPLICATE_LOGIN;
                        }
                        else if (GetServerInterface()->GetConnectionComponent()->IsConnectedToNetworkService())
                        {
                            lDisconnectedEvent.meLastError = CgsNetwork::E_SERVER_INTERFACE_CONNECTION_ERROR_DISCONNECT;
                        }
                        else
                        {
                            lDisconnectedEvent.meLastError = CgsNetwork::E_SERVER_INTERFACE_CONNECTION_NOT_SIGNED_IN;
                        }
                    }
                }

                GetNetworkModule()->AddOutputGuiEvent(lDisconnectedEvent);

                BrnGameState::GameStateModuleIO::LocalPlayerDisconnectedEvent lLocalPlayerDisconnectedEvent;
                GetNetworkModule()->GetGameEventQueue()->AddEvent(lLocalPlayerDisconnectedEvent,
                                                                  BrnGameState::GameStateModuleIO::E_EVENT_LOCAL_PLAYER_DISCONNECTED);

                BrnNetworkModuleIO::NetworkOutLocalPlayerDisconnected lNwDisconnectedEvent;
                GetNetworkModule()->GetNetworkEventQueue()->AddEvent(lNwDisconnectedEvent,
                                                                     lNwDisconnectedEvent.GetEventType());
            }

            GetStateManager()->Disconnected();
            GetHostMigrationManager()->Disconnected();
            GetStartTimeManager()->Disconnected();
            GetLoginManager()->Disconnected();
            GetLaunchManager()->Disconnected();
            GetMatchMakingManager()->Disconnected();
            GetSuspensionManager()->Disconnected();
            GetTrafficManager()->Disconnected();
            GetStandingsManager()->Disconnected();
            GetPostRoundManager()->Disconnected();
            GetCamera()->Disconnected();
            GetAggressiveDrivingManager()->Disconnected();
            GetLiveRevengeManager()->Disconnected();
            GetDirtyTrickManager()->Disconnected();
            GetNetworkImageManager()->Disconnected();
            GetGamerPictureManager()->Disconnected();
            GetSelectedRoutesManager()->Disconnected();
            GetNetworkNotificationManager()->Disconnect();
            GetRoadRulesManager()->Disconnected();
            GetChallengeSuccessManager()->Disconnected();
            GetStatsManager()->Disconnected();
            GetMarkedManManager()->Disconnected();
            GetScoreboardManager()->Disconnected();
            GetGamerCardManager()->Disconnected();
            GetAutoLoginManager()->Disconnected();
            GetTeamSelectionManager()->Disconnected();
            GetBuddyManager()->CancelInvites();
            GetPlayerManager()->Disconnected();
            GetVoIPManager()->Release();
            GetBuddyManager()->UpdateJoinableStatus();

            GetVersionDisplay()->SetGameServerGame(false);
            mPlayerIDStatsGet = CgsNetwork::K_INVALID_PLAYER_ID;
            break;

        case CgsNetwork::E_SERVER_INTERFACE_GAMES_EVENT_SEARCH_UPDATED:
        {
            BrnGui::GuiEventNetworkCustomMatchResults lSearchResults;
            GameParams lGameParams;

            CgsNetwork::ServerInterfaceGames* lpGamesComponent = GetServerInterface()->GetGameComponent();
            const s32 liNumGames = lpGamesComponent->GetNumberOfFoundGames();
            s32       liSearchGamesIndex = 0;

            lSearchResults.miNumGames = liNumGames;

            for (s32 liGamesIndex = 0; liGamesIndex < liNumGames; ++liGamesIndex)
            {
                if (liGamesIndex >= BrnGui::GuiEventNetworkCustomMatchResults::KI_MAX_NUM_GAMES)
                {
                    break;
                }

                lGameParams.Prepare();
                lpGamesComponent->GetFoundGame(liGamesIndex, &lGameParams);

                const char* lpcGameName = lGameParams.GetName();
                CGS_ASSERT(std::strlen(lpcGameName) < sizeof(lSearchResults.maacGameNames[liSearchGamesIndex]),
                           "String too long: ");
                std::strncpy(lSearchResults.maacGameNames[liSearchGamesIndex], lpcGameName,
                             sizeof(lSearchResults.maacGameNames[liSearchGamesIndex]));

                lSearchResults.maeGameMode[liSearchGamesIndex]         = lGameParams.GameMode();
                lSearchResults.maePreviousGameMode[liSearchGamesIndex] = lGameParams.PreviousGameMode();
                lSearchResults.maiMaxNumPlayers[liSearchGamesIndex]    = lGameParams.GetMaxPlayers();
                lSearchResults.maiNumPlayers[liSearchGamesIndex]       = lGameParams.GetNumberOfPlayers();

                // FLAG: a leading '@' in the game's host-name field takes one off both counts.
                if (lGameParams.GetHostName()[0] == '@')
                {
                    --lSearchResults.maiMaxNumPlayers[liSearchGamesIndex];
                    --lSearchResults.maiNumPlayers[liSearchGamesIndex];
                }

                lSearchResults.maiFoundGameIndex[liSearchGamesIndex] = liGamesIndex;
                lSearchResults.maiGameFlags[liSearchGamesIndex]      = 0;

                // The console tests the player count at the found-game index, not the search slot.
                if (lSearchResults.maiNumPlayers[liGamesIndex] != 0 && lGameParams.NetworkVersion() == 2)
                {
                    ++liSearchGamesIndex;
                }
                else
                {
                    --lSearchResults.miNumGames;
                }
            }

            GetNetworkModule()->AddOutputGuiEvent(lSearchResults);
            break;
        }

        case CgsNetwork::E_SERVER_INTERFACE_GAMES_EVENT_KICKED:
        {
            const CgsNetwork::EKickReason leKickReason = *static_cast<const CgsNetwork::EKickReason*>(lpData);
            GetStartTimeManager()->PrepareStartTime();
            GetStateManager()->SuspendToLeaveGame(E_LEFT_GAME_REASON_KICKED, leKickReason);
            GetBuddyManager()->UpdateJoinableStatus();
            break;
        }

        case CgsNetwork::E_SERVER_INTERFACE_GAMES_EVENT_PLAYER_ADDED:
        case CgsNetwork::E_SERVER_INTERFACE_GAMES_EVENT_PLAYER_REMOVED:
            GetBuddyManager()->UpdateJoinableStatus();
            break;

        case CgsNetwork::E_SERVER_INTERFACE_GAMES_EVENT_PLAYER_PARAMETERS_CHANGED:
        {
            const s32 liServerInterfacePlayerIndex = static_cast<s32>(reinterpret_cast<intptr_t>(lpData));
            UpdateMenuDataFromPlayerParams(liServerInterfacePlayerIndex);
            break;
        }

        case CgsNetwork::E_SERVER_INTERFACE_GAMES_EVENT_GAME_PARAMETERS_CHANGED:
        {
            GameParams lGameParams;
            lGameParams.Prepare();

            CgsNetwork::ServerInterfaceGames* lpGamesComponent = GetServerInterface()->GetGameComponent();
            CGS_ASSERT(lpGamesComponent != NULL, "lpGamesComponent");
            lpGamesComponent->GetGameParameters(&lGameParams);

            if (!lpGamesComponent->IsGameStarted() &&
                lpGamesComponent->GetHostPlayerID() != GetPlayerManager()->GetHostPlayerID())
            {
                GetPlayerManager()->SetHostPlayerID(lpGamesComponent->GetHostPlayerID());
            }

            OutputGameParameters();
            GetBuddyManager()->UpdateJoinableStatus();

            // FLAG: the security-mode and session-flag names are unrecovered.
            static_cast<CgsNetwork::ServerInterfaceGamesX360*>(GetServerInterface()->GetGameComponent())
                ->SetSessionFlags(lGameParams.Security() == 2 ? 0x600 : 0x400);
            break;
        }

        case CgsNetwork::E_SERVER_INTERFACE_GAMES_EVENT_GAME_DELETED:
            if (GetServerInterface()->GetConnectionComponent()->IsLoggedIn())
            {
                GetStateManager()->SuspendToLeaveGame(E_LEFT_GAME_REASON_GAME_DELETED, CgsNetwork::E_KICKREASON_COUNT);

                BrnGui::GuiEventNetworkPlayerImage lNetworkPlayerImageEvent;
                lNetworkPlayerImageEvent.mpTexture      = NULL;
                lNetworkPlayerImageEvent.miTextureIndex = -1;
                GetNetworkModule()->AddOutputGuiEvent(lNetworkPlayerImageEvent);

                PlayerInfoData lPlayerInfo;
                lPlayerInfo.Prepare();
                GetServerInterface()->GetPlayerInfoComponent()->GetLocalPlayerInfo(&lPlayerInfo);
                GetCamera()->RequestFeed(lPlayerInfo.GetID());
                GetStartTimeManager()->PrepareStartTime();
            }
            GetBuddyManager()->UpdateJoinableStatus();
            break;

        case CgsNetwork::E_SERVER_INTERFACE_GAMES_EVENT_GAME_ID_CHANGED:
            GetStateManager()->OnGameIDChanged();
            break;

        default:
            break;
        }
    }

    // ----------------------------------------------------------------------------------------
    // BrnNetworkManager::PlayerManagerEventCallback
    //
    // The player manager's lifecycle events. The event data word is the player id; the user
    // data is the network manager.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::PlayerManagerEventCallback(CgsNetwork::PlayerManager::EEvent leEvent,
                                                       void* lpEventData, void* lpUserData)
    {
        BrnNetworkManager* lpNetworkManager = static_cast<BrnNetworkManager*>(lpUserData);
        const NetworkPlayerID lPlayerID = static_cast<NetworkPlayerID>(reinterpret_cast<intptr_t>(lpEventData));

        switch (leEvent)
        {
        case CgsNetwork::PlayerManager::E_EVENT_PLAYER_ADDED:
        {
            PlayerParams lPlayerParams;
            const bool   lbIsLocalPlayer = lpNetworkManager->IsLocalPlayer(lPlayerID);

            lpNetworkManager->GetHostMigrationManager()->AddPlayer(lPlayerID);
            lpNetworkManager->GetStartTimeManager()->AddPlayer(lPlayerID);
            lpNetworkManager->GetStandingsManager()->AddPlayer(lPlayerID);
            lpNetworkManager->GetVoIPManager()->AddPlayer(lPlayerID);
            lpNetworkManager->GetTrafficManager()->AddPlayer(lPlayerID);
            lpNetworkManager->GetAggressiveDrivingManager()->AddPlayer(lPlayerID);
            lpNetworkManager->GetLiveRevengeManager()->AddPlayer(lPlayerID);
            lpNetworkManager->GetStatsManager()->AddPlayer(lPlayerID);
            lpNetworkManager->GetDirtyTrickManager()->AddPlayer(lPlayerID);
            lpNetworkManager->GetNetworkImageManager()->AddPlayer(lPlayerID);
            lpNetworkManager->GetSelectedRoutesManager()->AddPlayer(lPlayerID);
            lpNetworkManager->GetMarkedManManager()->AddPlayer(lPlayerID);
            lpNetworkManager->GetRoadRulesManager()->AddPlayer(lPlayerID);
            lpNetworkManager->GetChallengeSuccessManager()->AddPlayer(lPlayerID);
            lpNetworkManager->GetTeamSelectionManager()->AddPlayer(lPlayerID);

            PlayerMenuData* lpMenuData =
                static_cast<PlayerMenuData*>(lpNetworkManager->GetPlayerManager()->GetMenuDataByID(lPlayerID));
            CGS_ASSERT(lpMenuData != NULL, "lpMenuData");

            lPlayerParams.Prepare();
            lpNetworkManager->GetServerInterface()->GetGameComponent()->GetPlayerParametersByPlayerID(lPlayerID,
                                                                                                     &lPlayerParams);

            if (lbIsLocalPlayer)
            {
                lpNetworkManager->OutputGameParameters();
            }
            else
            {
                lpNetworkManager->GetCamera()->AddPlayer(lPlayerID);
                lpNetworkManager->GetGamerPictureManager()->AddPlayer(lPlayerID);
                lpNetworkManager->GetBuddyManager()->AddPlayerToHistory(&lPlayerParams);
            }

            lPlayerParams.GetFreeBurnCarID(&lpMenuData->mFreeBurnCarId);
            lpMenuData->meCameraStatus               = E_CAMERA_STATUS_NONE;
            lpMenuData->mCarId                       = 0;
            lpMenuData->mWheelId                     = 0;
            lpMenuData->mMarkedManID                 = CgsNetwork::K_INVALID_PLAYER_ID;
            lpMenuData->mu16FreeburnCarColourIndex   = lPlayerParams.GetCarColourIndex();
            lpMenuData->mu16FreeburnPaintFinishIndex = lPlayerParams.GetPaintFinishIndex();
            lpMenuData->mfUnknown4C                  = lPlayerParams.IsCarDeformed() ? 0.85f : 0.0f;
            lpMenuData->mu16CarColourIndex           = PlayerMenuData::KU_INVALID_COLOUR_INDEX;
            lpMenuData->mu16PaintFinishIndex         = PlayerMenuData::KU_INVALID_PAINT_FINISH_INDEX;
            CGS_ASSERT(lpMenuData->mu16FreeburnPaintFinishIndex < 4, "Invalid Paint Finish: ");

            BrnNetworkModuleIO::NetworkOutPlayerAddedEvent lPlayerAdded;
            lPlayerAdded.SetNetworkPlayerID(lPlayerID);
            lPlayerAdded.SetModelID(lpMenuData->mFreeBurnCarId);
            lPlayerAdded.SetWheelID(lpMenuData->mFreeBurnWheelId);
            lPlayerAdded.SetTeam(lPlayerParams.GetPlayerTeam());
            lPlayerAdded.mf18 = lpMenuData->mfUnknown4C;
            lPlayerAdded.SetLocalPlayer(lbIsLocalPlayer);
            lPlayerAdded.SetCarColourIndex(lpMenuData->mu16FreeburnCarColourIndex);
            lPlayerAdded.SetCarPaintFinishIndex(lpMenuData->mu16FreeburnPaintFinishIndex);
            lpNetworkManager->GetNetworkModule()->GetNetworkEventQueue()->AddEvent(lPlayerAdded,
                                                                                   lPlayerAdded.GetEventType());

            lpNetworkManager->GetStateManager()->PlayerAdded();
            break;
        }

        case CgsNetwork::PlayerManager::E_EVENT_START_PLAYER_REMOVAL:
            lpNetworkManager->GetHostMigrationManager()->RemovePlayer(lPlayerID);
            lpNetworkManager->GetStartTimeManager()->RemovePlayer(lPlayerID);
            lpNetworkManager->GetVoIPManager()->RemovePlayer(lpNetworkManager->GetServerInterface(), lPlayerID);
            lpNetworkManager->GetStandingsManager()->RemovePlayer(lPlayerID);
            lpNetworkManager->GetTrafficManager()->RemovePlayer(lPlayerID);
            lpNetworkManager->GetAggressiveDrivingManager()->RemovePlayer(lPlayerID);
            lpNetworkManager->GetLiveRevengeManager()->RemovePlayer(lPlayerID);
            lpNetworkManager->GetDirtyTrickManager()->RemovePlayer(lPlayerID);
            lpNetworkManager->GetNetworkImageManager()->RemovePlayer(lPlayerID);
            lpNetworkManager->GetSelectedRoutesManager()->RemovePlayer(lPlayerID);
            lpNetworkManager->GetMarkedManManager()->RemovePlayer(lPlayerID);
            lpNetworkManager->GetRoadRulesManager()->RemovePlayer(lPlayerID);
            lpNetworkManager->GetChallengeSuccessManager()->RemovePlayer(lPlayerID);
            lpNetworkManager->GetTeamSelectionManager()->RemovePlayer(lPlayerID);

            if (!lpNetworkManager->IsLocalPlayer(lPlayerID))
            {
                CGS_ASSERT(lpNetworkManager->GetPlayerManager() != NULL, "lpNetworkManager->GetPlayerManager()");
                const PlayerMenuData* lpPlayerMenuData = static_cast<const PlayerMenuData*>(
                    lpNetworkManager->GetPlayerManager()->GetMenuDataByID(lPlayerID));
                CGS_ASSERT(lpPlayerMenuData != NULL, "lpPlayerMenuData");

                CgsNetwork::PlayerName lPlayerName;
                lPlayerName.Construct(lpPlayerMenuData->macName);

                lpNetworkManager->GetCamera()->RemovePlayer(lPlayerID);
                lpNetworkManager->GetStatsManager()->RemovePlayer(lPlayerID);
                lpNetworkManager->GetGamerPictureManager()->RemovePlayer(lPlayerID);

                BrnNetworkModuleIO::NetworkOutPlayerRemovedEvent lPlayerRemoved;
                lPlayerRemoved.SetNetworkPlayerID(lPlayerID);
                CgsNetwork::ServerInterfaceGames* lpGamesComponent =
                    lpNetworkManager->GetServerInterface()->GetGameComponent();
                lPlayerRemoved.SetIsLocalPlayerInGame(lpGamesComponent->IsLocalPlayerInGame());
                lPlayerRemoved.SetIsLocalPlayerLeavingGame(lpGamesComponent->IsLocalPlayerLeavingGame());
                lPlayerRemoved.SetPlayerName(&lPlayerName);
                lpNetworkManager->GetNetworkModule()->GetNetworkEventQueue()->AddEvent(lPlayerRemoved,
                                                                                       lPlayerRemoved.GetEventType());
            }

            lpNetworkManager->GetStateManager()->PlayerRemoved();
            break;

        case CgsNetwork::PlayerManager::E_EVENT_PLAYER_FINALISED:
        {
            if (!lpNetworkManager->IsLocalPlayer(lPlayerID))
            {
                lpNetworkManager->GetCamera()->PlayerFinalised(lPlayerID);
            }
            lpNetworkManager->GetVoIPManager()->RemotePlayerFinalised(lpNetworkManager->GetServerInterface(), lPlayerID);
            lpNetworkManager->GetLiveRevengeManager()->RemotePlayerFinalised(lPlayerID);

            BrnNetworkModuleIO::NetworkOutPlayerFinalisedEvent lPlayerFinalised;
            lPlayerFinalised.SetNetworkPlayerID(lPlayerID);
            lpNetworkManager->GetNetworkModule()->GetNetworkEventQueue()->AddEvent(lPlayerFinalised,
                                                                                   lPlayerFinalised.GetEventType());
            break;
        }

        case CgsNetwork::PlayerManager::E_EVENT_PLAYER_LOST_CONTACT:
        {
            CGS_ASSERT(lpNetworkManager != NULL, "lpNetworkManager");
            CGS_ASSERT(CgsNetwork::K_INVALID_PLAYER_ID != lPlayerID, "CgsNetwork::K_INVALID_PLAYER_ID != lPlayerID");

            CgsNetwork::ServerInterfaceGames* lpGamesComponent =
                lpNetworkManager->GetServerInterface()->GetGameComponent();
            if (lpGamesComponent->IsLocalPlayerInGame() && lpGamesComponent->IsGameStarted())
            {
                const EActiveRaceCarIndex leActiveRaceCarIndex =
                    lpNetworkManager->GetNetworkModule()->GetActiveRaceCarIndex(lPlayerID);
                if (leActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID)
                {
                    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                               "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
                    CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                               "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

                    BrnNetworkModuleIO::NetworkPlayerDisconnectedEvent lPlayerDisconnectEvent;
                    lPlayerDisconnectEvent.Construct(
                        lPlayerID, leActiveRaceCarIndex,
                        BrnNetworkModuleIO::NetworkPlayerDisconnectedEvent::E_DISCONNECT_STATUS_LOST_CONTACT);
                    lpNetworkManager->GetNetworkModule()->GetNetworkEventQueue()->AddEvent(
                        lPlayerDisconnectEvent, lPlayerDisconnectEvent.GetEventType());
                }
            }
            break;
        }

        case CgsNetwork::PlayerManager::E_EVENT_PLAYER_REGAINED_CONTACT:
        {
            CGS_ASSERT(lpNetworkManager != NULL, "lpNetworkManager");
            CGS_ASSERT(CgsNetwork::K_INVALID_PLAYER_ID != lPlayerID, "CgsNetwork::K_INVALID_PLAYER_ID != lPlayerID");

            CgsNetwork::ServerInterfaceGames* lpGamesComponent =
                lpNetworkManager->GetServerInterface()->GetGameComponent();
            if (lpGamesComponent->IsLocalPlayerInGame() && lpGamesComponent->IsGameStarted())
            {
                const EActiveRaceCarIndex leActiveRaceCarIndex =
                    lpNetworkManager->GetNetworkModule()->GetActiveRaceCarIndex(lPlayerID);
                if (leActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID)
                {
                    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                               "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
                    CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                               "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

                    BrnNetworkModuleIO::NetworkPlayerDisconnectedEvent lPlayerDisconnectEvent;
                    lPlayerDisconnectEvent.Construct(
                        lPlayerID, leActiveRaceCarIndex,
                        BrnNetworkModuleIO::NetworkPlayerDisconnectedEvent::E_DISCONNECT_STATUS_CONNECTED);
                    lpNetworkManager->GetNetworkModule()->GetNetworkEventQueue()->AddEvent(
                        lPlayerDisconnectEvent, lPlayerDisconnectEvent.GetEventType());
                }
            }
            break;
        }

        case CgsNetwork::PlayerManager::E_EVENT_PLAYER_DISCONNECTED:
        {
            CGS_ASSERT(lpNetworkManager != NULL, "lpNetworkManager");
            CGS_ASSERT(CgsNetwork::K_INVALID_PLAYER_ID != lPlayerID, "CgsNetwork::K_INVALID_PLAYER_ID != lPlayerID");

            CgsNetwork::ServerInterfaceGames* lpGamesComponent =
                lpNetworkManager->GetServerInterface()->GetGameComponent();
            if (lpGamesComponent->IsLocalPlayerInGame() && lpGamesComponent->IsGameStarted())
            {
                BrnNetworkModuleIO::NetworkPlayerDisconnectedEvent lPlayerDisconnectEvent;
                lPlayerDisconnectEvent.Construct(
                    lPlayerID, lpNetworkManager->GetNetworkModule()->GetActiveRaceCarIndex(lPlayerID),
                    BrnNetworkModuleIO::NetworkPlayerDisconnectedEvent::E_DISCONNECT_STATUS_DISCONNECTED);
                lpNetworkManager->GetNetworkModule()->GetNetworkEventQueue()->AddEvent(
                    lPlayerDisconnectEvent, lPlayerDisconnectEvent.GetEventType());
            }
            break;
        }

        default:
            break;
        }
    }

    // ----------------------------------------------------------------------------------------
    // BrnNetworkManager::OutputPlayerResultsInfo
    //
    // Write the results record of every player in the game whose standings result has
    // arrived, packed from slot 0.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::OutputPlayerResultsInfo(BrnNetworkModuleIO::OutputBuffer* lpOutput)
    {
        CGS_ASSERT_AT(lpOutput != NULL, "lpOutput", CGS_ARTIST_FILE, 2844);

        NetworkPlayerID lPlayerID            = CgsNetwork::K_INVALID_PLAYER_ID;
        s32             liPlayerResultsIndex = 0;

        while (GetPlayerManager()->GetNextPlayerID(&lPlayerID,
                                                   CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
        {
            if (GetServerInterface()->GetGameComponent()->IsPlayerInGame(lPlayerID) &&
                GetStandingsManager()->ArePlayersResultsValid(lPlayerID))
            {
                BrnNetworkModuleIO::PlayerResultsData* lpPlayerResultsData =
                    lpOutput->GetPlayerResultsInterface()->GetPlayerResultsDataForWriting(liPlayerResultsIndex);
                GetStandingsManager()->FillOutResultsData(lpPlayerResultsData, lPlayerID);
                ++liPlayerResultsIndex;
            }
        }
    }
} // namespace BrnNetwork

#undef CGS_ARTIST_FILE
