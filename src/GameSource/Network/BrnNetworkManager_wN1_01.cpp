#include "types.hpp"

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
            lPlayerManagerParams.Construct(&mServerInterface, nullptr, nullptr, nullptr, lpHeapMalloc);

            CgsNetwork::NetworkManagerPrepareParams lPrepareParams;
            lPrepareParams.Construct(&lVersionDisplayParams, &lNetworkAdapterParams,
                                     &lPlayerManagerParams, leLocalConsoleFrameRate);

            const CgsNetwork::EManagerReturnCode lReturnCode = CgsNetwork::NetworkManager::Prepare(&lPrepareParams);
            if (lReturnCode == CgsNetwork::E_MANAGER_STATUS_BUSY)
            {
                return false;
            }
            CGS_ASSERT(lReturnCode == CgsNetwork::E_MANAGER_STATUS_READY,
                       "lReturnCode == CgsNetwork::E_MANAGER_STATUS_READY");

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
            CGS_ASSERT(false, "unknown prepare stage");
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
            CGS_ASSERT(false, "unknown release stage");
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
