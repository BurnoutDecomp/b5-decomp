#include "types.hpp"

#include "GameSource/Network/BrnNetworkManager.h"
#include "GameSource/Network/BrnNetworkPlayer.h"                             // BrnNetworkPlayerConstructParams
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"   // CgsDev::PerfMonCpu::AddMonitor

// ============================================================================================
// BrnNetwork::BrnNetworkManager -- lifecycle (partfile of BrnNetworkManager.cpp).
//
// Bodied here: the constructor, Construct and Destruct. Prepare and Release wait on
// declarations in headers this file does not own (the login manager's and three console-build
// managers' bool Prepare/Release, the server-interface prepare block's string members, the
// buddy manager's launch-data parameter); their bodies are staged out of tree.
// ============================================================================================

namespace BrnNetwork
{
    namespace
    {
        // The largest message and the largest reliable message the network heap is sized for;
        // Construct cross-checks the registered message types against them.
        const s32 KI_MAX_MESSAGE_SIZE          = 1968;
        const s32 KI_MAX_RELIABLE_MESSAGE_SIZE = 568;
    }

    // ----------------------------------------------------------------------------------------
    // BrnNetworkManager::BrnNetworkManager
    //
    // Only the sub-objects' own constructors run (base, server interface, every manager, the
    // seven network players, the menu data and the clocks); Construct seeds every field.
    // FLAG: NetworkServers has only a constructor taking the manager, so it is handed `this`
    // here; the console constructor stores nothing into it (Construct brings it up).
    // ----------------------------------------------------------------------------------------
    BrnNetworkManager::BrnNetworkManager()
        : mNetworkServers(this)
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
        // OWED: the console also stores the manager and the current server type into the
        // server interface's debug component here (+0x14 / +0x0C of it); that component has
        // no accessor yet.
        mServerInterface.SetNetworkManager(this);

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

        // OWED: the server selection is brought up here (manager back-pointer, server type
        // E_SERVER_TYPE_DEMO_1, address and port cleared, then the address resolved); the
        // NetworkServers header declares no public entry point that does only that.

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
