#include "GameShared/GameClasses/Network/CgsNetworkManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"   // CgsDev::PerfMonCpu

// CgsNetwork::NetworkManager -- the platform-independent core of the online hub. See the
// header for the layout.
//
// Every sub-object is reached by name. The version display, the voice manager, the
// start-time manager and the network adapter tear-down are inline on the console; here they
// are the sub-objects' own Construct / Destruct calls.

namespace CgsNetwork
{
    // Registered by Construct (the only writer), read by Update.
    s32 NetworkManager::_miPlayerManagerUpdatePerfMon;
    s32 NetworkManager::_miNetworkAdapterUpdatePerfMon;
    s32 NetworkManager::_miHostMigrationManagerUpdatePerfMon;
    s32 NetworkManager::_miStartTimeManagerUpdatePerfMon;
    s32 NetworkManager::_miVOIPManagerUpdatePerfMon;

    // Only the sub-objects' own constructors run here (vtables, embedded messages, the
    // network clocks); every manager field is seeded later by Construct.
    NetworkManager::NetworkManager()
    {
    }

    // Construct every sub-object, wire the start-time manager to its collaborators, reset the
    // staged state machines and register the per-sub-manager CPU monitors.
    void NetworkManager::Construct(NetworkManagerConstructParams* lpConstructParams)
    {
        CGS_ASSERT(lpConstructParams, "lpConstructParams");
        CGS_ASSERT(lpConstructParams->GetPlayerManagerConstructParams(),
                   "lpConstructParams->GetPlayerManagerConstructParams()");

        // FLAG: the adapter's declared parameter is not a console argument (the call passes
        // none); 0 is inert until that declaration drops it.
        mNetworkAdapter.Construct(0);
        mPlayerManager.Construct(lpConstructParams->GetPlayerManagerConstructParams());
        mHostMigrationManager.Construct();
        mStartTimeManager.Construct(&mHostMigrationManager, &mTimeManager, &mPlayerManager,
                                    StartTimeMessageArrivedLate,
                                    lpConstructParams->GetClientReadyCallback(),
                                    lpConstructParams->GetClientReadyCallbackData());
        mVersionDisplay.Construct();
        mVoIPManager.Construct(this);

        mbSysMenuOnScreen       = false;
        miActiveControllerPort  = 0;
        mePrepareStage          = E_PREPARESTAGE_START;
        meLocalConsoleFrameRate = CgsSystem::E_FRAMERATE_UNKNOWN;
        meReleaseStage          = E_RELEASESTAGE_DONE;

        _miPlayerManagerUpdatePerfMon        = CgsDev::PerfMonCpu::AddMonitor("CGSMan - Player Manager",     CgsDev::E_PMP_8, false, 1.0f, true);
        _miNetworkAdapterUpdatePerfMon       = CgsDev::PerfMonCpu::AddMonitor("CGSMan - Network Adaptor",    CgsDev::E_PMP_8, false, 1.0f, true);
        _miHostMigrationManagerUpdatePerfMon = CgsDev::PerfMonCpu::AddMonitor("CGSMan - Host Migration",     CgsDev::E_PMP_8, false, 1.0f, true);
        _miStartTimeManagerUpdatePerfMon     = CgsDev::PerfMonCpu::AddMonitor("CGSMan - Start Time Manager", CgsDev::E_PMP_8, false, 1.0f, true);
        _miVOIPManagerUpdatePerfMon          = CgsDev::PerfMonCpu::AddMonitor("CGSMan - VOIP Manager",       CgsDev::E_PMP_8, false, 1.0f, true);
    }

    // Reset the staged state, then tear the sub-objects down in reverse dependency order. The
    // clock (mTimeManager) is not torn down here.
    void NetworkManager::Destruct()
    {
        mePrepareStage          = E_PREPARESTAGE_START;
        meReleaseStage          = E_RELEASESTAGE_DONE;
        meLocalConsoleFrameRate = CgsSystem::E_FRAMERATE_UNKNOWN;
        mbSysMenuOnScreen       = false;
        miActiveControllerPort  = 0;

        mVoIPManager.Destruct();
        mVersionDisplay.Destruct();
        mStartTimeManager.Destruct();
        mHostMigrationManager.Destruct();
        mPlayerManager.Destruct();
        mNetworkAdapter.Destruct();
    }

    // Staged bring-up: version banner, network adapter, player manager, host migration and
    // the start-time manager, each resumed where the last call left off. The adapter reports
    // its own status; every other stage reports BUSY until its sub-object is ready.
    EManagerReturnCode NetworkManager::Prepare(NetworkManagerPrepareParams* lpPrepareParams)
    {
        switch (mePrepareStage)
        {
        case E_PREPARESTAGE_START:
            mePrepareStage          = E_PREPARESTAGE_START;
            meLocalConsoleFrameRate = lpPrepareParams->meLocalConsoleFrameRate;
            mVersionDisplay.Prepare(lpPrepareParams->mVersionDisplay.mpcVersion,
                                    lpPrepareParams->mVersionDisplay.muField_04,
                                    lpPrepareParams->mVersionDisplay.meServerType);
            mVersionDisplay.Register();
            // fall through

        case E_PREPARESTAGE_NETWORK_ADAPTER:
        {
            mePrepareStage = E_PREPARESTAGE_NETWORK_ADAPTER;
            const NetworkAdapterBase::ENetworkStatus leStatus =
                mNetworkAdapter.Prepare(&lpPrepareParams->mNetworkAdapter);
            if (leStatus != NetworkAdapterBase::E_NET_STATUS_READY)
            {
                return (leStatus == NetworkAdapterBase::E_NET_STATUS_ERROR) ? E_MANAGER_STATUS_ERROR
                                                                            : E_MANAGER_STATUS_BUSY;
            }
        }
            // fall through

        case E_PREPARESTAGE_PLAYER_MANAGER:
        {
            mePrepareStage = E_PREPARESTAGE_PLAYER_MANAGER;

            PlayerManagerPrepareParams lPrepareParams;
            lPrepareParams.mpNetworkAdapter                 = &mNetworkAdapter;
            lPrepareParams.mpServerInterface                = lpPrepareParams->mPlayerManager.mpServerInterface;
            lPrepareParams.mpTimeManager                    = &mTimeManager;
            lPrepareParams.meLocalConsoleFrameRate          = lpPrepareParams->meLocalConsoleFrameRate;
            lPrepareParams.mpfOnReceivedFromWrongIPCallback = lpPrepareParams->mPlayerManager.mpfOnReceivedFromWrongIPCallback;
            lPrepareParams.mpfConnectionFinalisedCallback   = lpPrepareParams->mPlayerManager.mpfConnectionFinalisedCallback;
            lPrepareParams.mpConnectionFinalisedUserData    = lpPrepareParams->mPlayerManager.mpConnectionFinalisedUserData;
            lPrepareParams.mpNetworkHeapAllocator           = lpPrepareParams->mPlayerManager.mpNetworkHeapAllocator;

            CGS_ASSERT(lPrepareParams.meLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_50HZ ||
                       lPrepareParams.meLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_60HZ,
                       "lPrepareParams.meLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_50HZ || lPrepareParams.meLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_60HZ");

            if (!mPlayerManager.Prepare(&lPrepareParams))
            {
                return E_MANAGER_STATUS_BUSY;
            }
        }
            // fall through

        case E_PREPARESTAGE_HOST_MIGRATION_MANGER:
            mePrepareStage = E_PREPARESTAGE_HOST_MIGRATION_MANGER;
            if (!mHostMigrationManager.Prepare(&mPlayerManager, meLocalConsoleFrameRate,
                                               mTimeManager.GetU16FrameCount()))
            {
                return E_MANAGER_STATUS_BUSY;
            }
            // fall through

        case E_PREPARESTAGE_START_TIME_MANAGER:
            mePrepareStage = E_PREPARESTAGE_START_TIME_MANAGER;
            if (!mStartTimeManager.Prepare())
            {
                return E_MANAGER_STATUS_BUSY;
            }
            // fall through

        case E_PREPARESTAGE_DONE:
            meReleaseStage = E_RELEASESTAGE_START;
            mePrepareStage = E_PREPARESTAGE_DONE;
            return E_MANAGER_STATUS_READY;

        default:
            CGS_ASSERT(false, "unknown prepare stage");
            return E_MANAGER_STATUS_BUSY;
        }
    }

    // Staged tear-down, the mirror of Prepare: voice, start time, host migration, players,
    // then the adapter. Returns false while a stage is still releasing.
    bool NetworkManager::Release()
    {
        switch (meReleaseStage)
        {
        case E_RELEASESTAGE_START:
            meReleaseStage          = E_RELEASESTAGE_START;
            meLocalConsoleFrameRate = CgsSystem::E_FRAMERATE_UNKNOWN;
            // fall through

        case E_RELEASESTAGE_VOIP_MANAGER:
            meReleaseStage = E_RELEASESTAGE_VOIP_MANAGER;
            if (!mVoIPManager.Release())
            {
                return false;
            }
            // fall through

        case E_RELEASESTAGE_START_TIME_MANAGER:
            meReleaseStage = E_RELEASESTAGE_START_TIME_MANAGER;
            if (!mStartTimeManager.Release())
            {
                return false;
            }
            // fall through

        case E_RELEASESTAGE_HOST_MIGRATION_MANGER:
            meReleaseStage = E_RELEASESTAGE_HOST_MIGRATION_MANGER;
            if (!mHostMigrationManager.Release())
            {
                return false;
            }
            // fall through

        case E_RELEASESTAGE_PLAYER_MANAGER:
            meReleaseStage = E_RELEASESTAGE_PLAYER_MANAGER;
            if (!mPlayerManager.Release())
            {
                return false;
            }
            // fall through

        case E_RELEASESTAGE_NETWORK_ADAPTER:
            meReleaseStage = E_RELEASESTAGE_NETWORK_ADAPTER;
            if (!mNetworkAdapter.Release())
            {
                return false;
            }
            // fall through

        case E_RELEASESTAGE_DONE:
            mePrepareStage = E_PREPARESTAGE_START;
            meReleaseStage = E_RELEASESTAGE_DONE;
            return true;

        default:
            CGS_ASSERT(false, "unknown release stage");
            return false;
        }
    }

    // Per-frame pump of every sub-object, each bracketed by its CPU monitor.
    void NetworkManager::Update(const CgsSystem::TimerStatus* lpTimerStatus, u16 lu16CurrentFrame, bool lbInGame)
    {
        CgsDev::PerfMonCpu::StartMonitor(_miPlayerManagerUpdatePerfMon);
        mPlayerManager.Update(lpTimerStatus, lu16CurrentFrame, lbInGame);
        CgsDev::PerfMonCpu::StopMonitor(_miPlayerManagerUpdatePerfMon);

        CgsDev::PerfMonCpu::StartMonitor(_miNetworkAdapterUpdatePerfMon);
        mNetworkAdapter.Update();
        CgsDev::PerfMonCpu::StopMonitor(_miNetworkAdapterUpdatePerfMon);

        CgsDev::PerfMonCpu::StartMonitor(_miHostMigrationManagerUpdatePerfMon);
        mHostMigrationManager.Update(lpTimerStatus, lu16CurrentFrame);
        CgsDev::PerfMonCpu::StopMonitor(_miHostMigrationManagerUpdatePerfMon);

        CgsDev::PerfMonCpu::StartMonitor(_miStartTimeManagerUpdatePerfMon);
        mStartTimeManager.Update(lpTimerStatus);
        CgsDev::PerfMonCpu::StopMonitor(_miStartTimeManagerUpdatePerfMon);

        CgsDev::PerfMonCpu::StartMonitor(_miVOIPManagerUpdatePerfMon);
        mVoIPManager.Update(lbInGame);
        CgsDev::PerfMonCpu::StopMonitor(_miVOIPManagerUpdatePerfMon);
    }

    // After the simulation: the player manager sends this frame's messages.
    void NetworkManager::PostUpdate(const CgsSystem::TimerStatus* lpTimerStatus, u16 lu16CurrentFrame)
    {
        mPlayerManager.PostUpdate(lu16CurrentFrame);
    }

    // Entering and leaving a game both re-arm the start-time handshake.
    void NetworkManager::OnEnterGame()
    {
        mStartTimeManager.PrepareStartTime();
    }

    void NetworkManager::OnLeaveGame()
    {
        mStartTimeManager.PrepareStartTime();
    }

    // Empty on the console: the base keeps no per-game state to start.
    void NetworkManager::OnGameStart(CgsSystem::Time lStartTime, u16 lu16CurrentFrame)
    {
    }

    // The game is over: forget the start frame so frame-since-start queries go invalid.
    // The time step is not read on the invalid path.
    void NetworkManager::OnGameFinish(CgsSystem::Time lFinishTime, u16 lu16CurrentFrame)
    {
        mTimeManager.SetStartFrame(TimeManager::E_START_FRAME_INVALID, nullptr, 0.0f);
    }

    // The round starts: bookmark the start frame against the agreed start time (none when the
    // handshake produced no valid time), then let the player manager start its round.
    void NetworkManager::OnRoundStart(CgsSystem::Time lStartTime, f32 lfTimeStep, u16 lu16CurrentFrame)
    {
        const StartTime* lpStartTime = mStartTimeManager.GetStartTime();
        mTimeManager.SetStartFrame(TimeManager::E_START_FRAME_PAST,
                                   lpStartTime ? &lpStartTime->mStartTime : nullptr,
                                   lfTimeStep);
        mPlayerManager.OnRoundStart();
    }

    // The round is over: forget the start frame, as at the end of a game.
    void NetworkManager::OnRoundFinish(CgsSystem::Time lFinishTime, u16 lu16CurrentFrame)
    {
        mTimeManager.SetStartFrame(TimeManager::E_START_FRAME_INVALID, nullptr, 0.0f);
    }

    // Registered with the start-time manager; the base class does nothing when the start
    // message arrives late.
    void NetworkManager::StartTimeMessageArrivedLate()
    {
    }
}
