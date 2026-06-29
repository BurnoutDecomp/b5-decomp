#include "GameSource/Network/Managers/BrnNetworkLaunchManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Network/BrnNetworkManager.h"
#include "GameSource/Network/Managers/BrnNetworkStateManager.h"
#include "GameSource/Network/Managers/BrnNetworkSuspensionManager.h"
#include "GameSource/Network/Parameters/BrnNetworkPlayerParamsClass.h"
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"
#include "GameShared/GameClasses/Network/Players/CgsPlayersConnectionManager.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"  // E_COMPONENTS_GAMES
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"

// ===========================================================================
// BrnNetwork::LaunchManager  (GameSource/Network/Managers/BrnNetworkLaunchManager.cpp)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct                  @ 0x82571080
//   Update                     @ 0x82543EA8
//   Start                      @ 0x82544208
//   ServerInterfaceIdle        @ 0x82544370
//   SuspensionFinishedCallback @ 0x82544450
//   Disconnected               @ 0x825444F8
//   OnLeaveGame                @ 0x82544508
//   Prepare                    @ 0x8254FFB0   Release @ 0x8254FFD8
//   UpdateRunning              @ 0x82543F20
//   UpdateLocking              @ 0x8256CE30
//   UpdateSetPlayersToPlaying  @ 0x82566528
//   UpdateUnlockFromError      @ 0x82566708
//   UpdateDownloadingStats     @ 0x82550000
//   UpdateLaunching            @ 0x82550070
//   UpdateSuspending           @ 0x82558B68
//   UpdateWaiting              @ 0x82544098
//   UpdateError                @ 0x82571xxx (the maUpdateFunctions[9] slot)
//   UpdateNone  @ 0x825DCAB0   Stop @ 0x825F8DE4   Reset @ 0x825F8EB8   Destruct @ 0x825F8EC0
//
// The launch sequence: once Start() arms it, Update() dispatches the current
// ESubState's handler each frame. The host walks RUNNING -> LOCKING -> set every
// player "playing" -> download stats -> LAUNCHING -> SUSPENDING -> WAITING -> idle;
// any failure routes through UNLOCK_FROM_ERROR / ERROR and unlocks the game so the
// lobby can recover. The handler return value is the EStatus reported up to the
// StateManager.
// ===========================================================================

namespace BrnNetwork
{
    namespace
    {
        // X360 KF_LAUNCH_WAIT_TIME (DWARF BrnNetworkLaunchManager.cpp:28; the Time(3.0)
        // literal loaded from flt_8207C620 in UpdateSuspending). The post-suspend settle
        // window, in seconds, before UpdateWaiting reports the launch complete.
        const f32 KF_LAUNCH_WAIT_TIME = 3.0f;

        // The DirtySock component the launch sequence drives (E_COMPONENTS_GAMES == 1; the
        // literal `1` argument at every GetStatus/GetLastError/ClearLastError call site).
        const s32 KI_GAMES_COMPONENT = CgsNetwork::E_COMPONENTS_GAMES;

        // The minimum number of finalised players required to launch (the asm compares the
        // finalised-player count against 2 with `cmpwi 2; blt`).
        const s32 KI_MIN_PLAYERS_TO_LAUNCH = 2;

        // The X360 routes the launch-manager progress lines through the shared network debug
        // StrStream (off_82F335C8). Modelled as a free no-op helper, matching the established
        // CgsServerInterfaceGames.cpp DirtySockDebugLog convention; the real sink lives in the
        // development log subsystem (not reconstructed here).
        void DebugLog(const char* /*lpcText*/)
        {
        }
    }

    // -- lifecycle --------------------------------------------------------------

    void LaunchManager::Construct(BrnNetworkManager* lpNetworkManager)
    {
        // Zero the dispatch table first (X360: the 10-slot clear loop), then wire each
        // ESubState to its handler.
        for (s32 liIndex = 0; liIndex < E_SUBSTATE_COUNT; ++liIndex)
            maUpdateFunctions[liIndex] = 0;

        maUpdateFunctions[E_SUBSTATE_NONE]                   = &LaunchManager::UpdateNone;
        maUpdateFunctions[E_SUBSTATE_RUNNING]                = &LaunchManager::UpdateRunning;
        maUpdateFunctions[E_SUBSTATE_LOCKING]                = &LaunchManager::UpdateLocking;
        maUpdateFunctions[E_SUBSTATE_SET_PLAYERS_TO_PLAYING] = &LaunchManager::UpdateSetPlayersToPlaying;
        maUpdateFunctions[E_SUBSTATE_UNLOCK_FROM_ERROR]      = &LaunchManager::UpdateUnlockFromError;
        maUpdateFunctions[E_SUBSTATE_DOWNLOADING_STATS]      = &LaunchManager::UpdateDownloadingStats;
        maUpdateFunctions[E_SUBSTATE_LAUNCHING]              = &LaunchManager::UpdateLaunching;
        maUpdateFunctions[E_SUBSTATE_SUSPENDING]             = &LaunchManager::UpdateSuspending;
        maUpdateFunctions[E_SUBSTATE_WAITING]                = &LaunchManager::UpdateWaiting;
        maUpdateFunctions[E_SUBSTATE_ERROR]                  = &LaunchManager::UpdateError;

        // Every sub-state must have a handler (X360 fires this assert once per slot).
        for (s32 liIndex = 0; liIndex < E_SUBSTATE_COUNT; ++liIndex)
            CGS_ASSERT(maUpdateFunctions[liIndex] != 0,
                       "No update function supplied for this substate");

        mpNetworkManager       = lpNetworkManager;
        meSubState             = E_SUBSTATE_NONE;
        mTimeToStopWaiting     = CgsSystem::Time(0.0f);
        miNumPlayersSetPlaying = 0;

        // X360 inlines Stop()'s log here.
        DebugLog("[LAUNCH MGR]: ");
        DebugLog("Stopping launch\n");
        DebugLog("\n");
    }

    bool LaunchManager::Prepare()
    {
        Stop();
        return true;
    }

    bool LaunchManager::Release()
    {
        Stop();
        return true;
    }

    void LaunchManager::Destruct()
    {
        // FLAGGED: the X360 Destruct (@ 0x825F8EC0) is an 8-byte leaf with no operations
        // recovered into the decompiler export (DecFIGS hints are empty; the manager is
        // embedded by value, so there is no owned storage to release). Modelled as the
        // trivial body the recovery shows; no member writes are fabricated.
    }

    // -- start / stop / reset ---------------------------------------------------

    void LaunchManager::Start()
    {
        CGS_ASSERT(meSubState == E_SUBSTATE_NONE, "Already managing pre-launch!");

        meSubState             = E_SUBSTATE_RUNNING;
        miNumPlayersSetPlaying = 0;

        DebugLog("[LAUNCH MGR]: ");
        DebugLog("Starting launch\n");
        DebugLog("\n");
    }

    void LaunchManager::Stop()
    {
        meSubState         = E_SUBSTATE_NONE;
        mTimeToStopWaiting = CgsSystem::Time(0.0f);

        DebugLog("[LAUNCH MGR]: ");
        DebugLog("Stopping launch\n");
        DebugLog("\n");
    }

    void LaunchManager::Reset()
    {
        // FLAGGED: the X360 Reset (@ 0x825F8EB8) is an 8-byte leaf with no operations
        // recovered into the decompiler export (DecFIGS hints are empty, no calls). Modelled
        // as the trivial body the recovery shows; no member writes are fabricated.
    }

    void LaunchManager::Disconnected()
    {
        // X360: *(this+40) = 0 -- drop straight back to the idle sub-state.
        meSubState = E_SUBSTATE_NONE;
    }

    void LaunchManager::OnLeaveGame()
    {
        Stop();
    }

    // -- dispatch ---------------------------------------------------------------

    LaunchManager::EStatus LaunchManager::Update()
    {
        CGS_ASSERT(maUpdateFunctions[meSubState] != 0,
                   "No update function supplied for the current substate");

        return (this->*maUpdateFunctions[meSubState])();
    }

    // -- shared idle/error gate -------------------------------------------------

    bool LaunchManager::ServerInterfaceIdle()
    {
        const BrnServerInterface::EStatus leStatus =
            mpNetworkManager->GetServerInterface()->GetStatus(KI_GAMES_COMPONENT);

        if (leStatus == BrnServerInterface::E_STATUS_BUSY)
            return false;

        if (leStatus == BrnServerInterface::E_STATUS_ERROR)
        {
            // A "kick" error (33) just means a player left; clear it and treat as idle.
            if (mpNetworkManager->GetServerInterface()->GetLastError(KI_GAMES_COMPONENT) == 33)
            {
                mpNetworkManager->GetServerInterface()->ClearLastError(KI_GAMES_COMPONENT);
                return true;
            }

            meSubState = E_SUBSTATE_ERROR;
            DebugLog("[LAUNCH MGR]: ");
            DebugLog("Error detected\n");
            DebugLog("\n");
            return false;
        }

        if (leStatus < BrnServerInterface::E_STATUS_COUNT)   // E_STATUS_IDLE
            return true;

        CGS_ASSERT(false,
                   "Invalid state returned by mpNetworkManager->GetServerInterface()->GetStatus() in LaunchManager::ServerInterfaceIdle");
        return false;
    }

    // -- per-substate handlers --------------------------------------------------

    LaunchManager::EStatus LaunchManager::UpdateNone()
    {
        // X360: folded with BrnWorld::PVSDebugComponent::IsSimple (both compile to `return 0`).
        return E_STATUS_IDLE;
    }

    LaunchManager::EStatus LaunchManager::UpdateRunning()
    {
        if (mpNetworkManager->GetServerInterface()->GetStatus(KI_GAMES_COMPONENT)
                != BrnServerInterface::E_STATUS_IDLE)
            return E_STATUS_WAITING_FOR_LAUNCH;

        CgsNetwork::ServerInterfaceGames* lpGames =
            mpNetworkManager->GetServerInterface()->GetGameComponent();

        if (!lpGames->IsLocalPlayerInGame())
        {
            DebugLog("[LAUNCH MGR]: ");
            DebugLog("LAUNCH ERROR Local player not in the game\n");
            DebugLog("\n");
            return E_STATUS_ERROR;
        }

        if (mpNetworkManager->GetPlayerManager()->GetTotalNumberPlayers(
                CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED)
            < KI_MIN_PLAYERS_TO_LAUNCH)
        {
            DebugLog("[LAUNCH MGR]: ");
            DebugLog("LAUNCH ERROR Not enough players\n");
            DebugLog("\n");
            return E_STATUS_ERROR;
        }

        if (!mpNetworkManager->GetPlayersConnectionManager()->AreAllConnectionsSuccessful())
        {
            DebugLog("[LAUNCH MGR]: ");
            DebugLog("LAUNCH ERROR A connection has failed\n");
            DebugLog("\n");
            return E_STATUS_ERROR;
        }

        if (lpGames->IsLocalPlayerHost())
            lpGames->LockGame();

        meSubState             = E_SUBSTATE_LOCKING;
        miNumPlayersSetPlaying = 0;
        return E_STATUS_LAUNCHING;
    }

    LaunchManager::EStatus LaunchManager::UpdateLocking()
    {
        const bool lbNATFinalised =
            mpNetworkManager->GetPlayersConnectionManager()->AreAllConnectionsSuccessful();
        CgsNetwork::ServerInterfaceGames* lpGames =
            mpNetworkManager->GetServerInterface()->GetGameComponent();

        if (!ServerInterfaceIdle())
            return E_STATUS_BUSY;

        // Host-only: if we are host with fewer than the minimum players the game is no longer
        // viable -- bail back to the error-unlock path.
        if (lpGames->IsLocalPlayerHost()
            && mpNetworkManager->GetPlayerManager()->GetTotalNumberPlayers(
                   CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED)
                   < KI_MIN_PLAYERS_TO_LAUNCH)
        {
            lpGames->UnlockGame();
            DebugLog("[LAUNCH MGR]: ");
            DebugLog("Update locking failed as players left\n");
            DebugLog("\n");
            meSubState = E_SUBSTATE_UNLOCK_FROM_ERROR;
            return E_STATUS_BUSY;
        }

        if (!lbNATFinalised && lpGames->IsLocalPlayerHost())
        {
            mpNetworkManager->GetServerInterface()->ClearLastError(KI_GAMES_COMPONENT);
            lpGames->UnlockGame();
            DebugLog("[LAUNCH MGR]: ");
            DebugLog("Update locking failed as not everyone is finalised\n");
            DebugLog("\n");
            meSubState = E_SUBSTATE_UNLOCK_FROM_ERROR;
            return E_STATUS_BUSY;
        }

        if (!mpNetworkManager->GetStateManager()->GameModeHasEnoughTeams())
        {
            lpGames->UnlockGame();
            DebugLog("[LAUNCH MGR]: ");
            DebugLog("Update locking failed as not enough teams\n");
            DebugLog("\n");
            meSubState = E_SUBSTATE_UNLOCK_FROM_ERROR;
            return E_STATUS_BUSY;
        }

        meSubState             = E_SUBSTATE_SET_PLAYERS_TO_PLAYING;
        miNumPlayersSetPlaying = 0;
        return E_STATUS_BUSY;
    }

    LaunchManager::EStatus LaunchManager::UpdateSetPlayersToPlaying()
    {
        const bool lbNATFinalised =
            mpNetworkManager->GetPlayersConnectionManager()->AreAllConnectionsSuccessful();
        CgsNetwork::ServerInterfaceGames* lpGames =
            mpNetworkManager->GetServerInterface()->GetGameComponent();

        if (!ServerInterfaceIdle())
            return E_STATUS_BUSY;

        if (lpGames->IsLocalPlayerHost()
            && mpNetworkManager->GetPlayerManager()->GetTotalNumberPlayers(
                   CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED)
                   < KI_MIN_PLAYERS_TO_LAUNCH)
        {
            lpGames->UnlockGame();
            DebugLog("[LAUNCH MGR]: ");
            DebugLog("Update set players to playing failed as not enough players\n");
            DebugLog("\n");
            meSubState = E_SUBSTATE_UNLOCK_FROM_ERROR;
            return E_STATUS_BUSY;
        }

        if (!lbNATFinalised && lpGames->IsLocalPlayerHost())
        {
            mpNetworkManager->GetServerInterface()->ClearLastError(KI_GAMES_COMPONENT);
            lpGames->UnlockGame();
            DebugLog("[LAUNCH MGR]: ");
            DebugLog("Update set players to playing failed as not everyone has finalised\n");
            DebugLog("\n");
            meSubState = E_SUBSTATE_UNLOCK_FROM_ERROR;
            return E_STATUS_BUSY;
        }

        // Push each player's "playing" flag one player per tick (host only).
        if (lpGames->IsLocalPlayerHost()
            && miNumPlayersSetPlaying
                   < mpNetworkManager->GetPlayerManager()->GetTotalNumberPlayers(
                         CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
        {
            PlayerParams lPlayerParams;
            lPlayerParams.Prepare();
            lpGames->GetPlayerParametersByIndex(miNumPlayersSetPlaying, &lPlayerParams);
            lPlayerParams.SetPlaying(true);
            lpGames->UpdatePlayerParameters(lPlayerParams.GetID(), &lPlayerParams);
            ++miNumPlayersSetPlaying;
            return E_STATUS_BUSY;
        }

        meSubState = E_SUBSTATE_DOWNLOADING_STATS;
        return E_STATUS_BUSY;
    }

    LaunchManager::EStatus LaunchManager::UpdateUnlockFromError()
    {
        CgsNetwork::ServerInterfaceGames* lpGames =
            mpNetworkManager->GetServerInterface()->GetGameComponent();
        CGS_ASSERT(lpGames != 0, "lpGames");

        DebugLog("[LAUNCH MGR]: ");
        DebugLog("Updating unlock from error\n");
        DebugLog("\n");

        if (!ServerInterfaceIdle())
            return E_STATUS_BUSY;

        // Walk the players back DOWN, clearing each one's "playing" flag, until we run out.
        if (!lpGames->IsLocalPlayerHost() || miNumPlayersSetPlaying < 0)
        {
            meSubState = E_SUBSTATE_ERROR;
            return E_STATUS_BUSY;
        }

        if (miNumPlayersSetPlaying < lpGames->GetNumberPlayersInGame())
        {
            PlayerParams lPlayerParams;
            lPlayerParams.Prepare();
            lpGames->GetPlayerParametersByIndex(miNumPlayersSetPlaying, &lPlayerParams);
            lPlayerParams.SetPlaying(false);
            lpGames->UpdatePlayerParameters(lPlayerParams.GetID(), &lPlayerParams);
        }

        --miNumPlayersSetPlaying;
        return E_STATUS_BUSY;
    }

    LaunchManager::EStatus LaunchManager::UpdateDownloadingStats()
    {
        if (ServerInterfaceIdle())
        {
            CgsNetwork::ServerInterfaceGames* lpGames =
                mpNetworkManager->GetServerInterface()->GetGameComponent();

            if (lpGames->IsLocalPlayerHost())
                lpGames->StartGame();

            meSubState = E_SUBSTATE_LAUNCHING;
        }
        return E_STATUS_BUSY;
    }

    LaunchManager::EStatus LaunchManager::UpdateLaunching()
    {
        const bool lbNATFinalised =
            mpNetworkManager->GetPlayersConnectionManager()->AreAllConnectionsSuccessful();
        CgsNetwork::ServerInterfaceGames* lpGames =
            mpNetworkManager->GetServerInterface()->GetGameComponent();

        if (ServerInterfaceIdle() && lbNATFinalised && lpGames->IsGameStarted())
        {
            // Suspend the server interface for the in-game switch; SuspensionFinishedCallback
            // advances us to WAITING (success) or UNLOCK_FROM_ERROR (failure).
            meSubState = E_SUBSTATE_SUSPENDING;
            mpNetworkManager->GetSuspensionManager()->Suspend(
                SuspensionManager::E_SUSPENSION_TYPE_IN_GAME,
                &LaunchManager::SuspensionFinishedCallback,
                this);
            return E_STATUS_LAUNCHING;
        }

        // Lost finalisation while host -> roll back to the error-unlock path.
        if (!lbNATFinalised && lpGames->IsLocalPlayerHost())
        {
            DebugLog("[LAUNCH MGR]: ");
            DebugLog("Update launching failed to launch as players are not finalised\n");
            DebugLog("\n");
            mpNetworkManager->GetServerInterface()->ClearLastError(KI_GAMES_COMPONENT);
            lpGames->UnlockGame();
            meSubState = E_SUBSTATE_UNLOCK_FROM_ERROR;
        }

        return E_STATUS_LAUNCHING;
    }

    LaunchManager::EStatus LaunchManager::UpdateSuspending()
    {
        mpNetworkManager->GetSuspensionManager()->Update();
        CgsNetwork::ServerInterfaceGames* lpGames =
            mpNetworkManager->GetServerInterface()->GetGameComponent();

        DebugLog("[LAUNCH MGR]: ");
        DebugLog("Update suspending\n");
        DebugLog("\n");

        // SuspensionFinishedCallback sets meSubState to WAITING (success) or
        // UNLOCK_FROM_ERROR (failure); until then it stays SUSPENDING (the asm `if (v6)` test
        // -- a non-zero/non-NONE sub-state means we are still mid-suspend).
        if (meSubState == E_SUBSTATE_NONE)
            return E_STATUS_LAUNCHED;

        if (meSubState == E_SUBSTATE_UNLOCK_FROM_ERROR)
        {
            if (lpGames->IsLocalPlayerHost())
            {
                mpNetworkManager->GetServerInterface()->ClearLastError(KI_GAMES_COMPONENT);
                lpGames->UnlockGame();
                return E_STATUS_LAUNCHING;
            }

            CGS_ASSERT(mpNetworkManager != 0, "mpNetworkManager");
            CGS_ASSERT(mpNetworkManager->GetTimerStatus() != 0,
                       "mpNetworkManager->GetTimerStatus()");

            // Start the post-suspend settle window.
            const CgsSystem::Time lWaitDuration(KF_LAUNCH_WAIT_TIME);
            mTimeToStopWaiting =
                mpNetworkManager->GetTimerStatus()->GetGameTimerStatus()->GetTime()
                + lWaitDuration;
            meSubState = E_SUBSTATE_WAITING;
        }

        return E_STATUS_LAUNCHING;
    }

    LaunchManager::EStatus LaunchManager::UpdateWaiting()
    {
        CGS_ASSERT(mpNetworkManager != 0, "mpNetworkManager");
        CGS_ASSERT(mpNetworkManager->GetTimerStatus() != 0,
                   "mpNetworkManager->GetTimerStatus()");

        const CgsSystem::Time lNow =
            mpNetworkManager->GetTimerStatus()->GetGameTimerStatus()->GetTime();

        if (lNow >= mTimeToStopWaiting)
        {
            meSubState = E_SUBSTATE_NONE;
            return E_STATUS_LAUNCHED;
        }

        return E_STATUS_LAUNCHING;
    }

    LaunchManager::EStatus LaunchManager::UpdateError()
    {
        // FLAGGED: the maUpdateFunctions[E_SUBSTATE_ERROR] slot was not emitted with disassembly
        // into the recovery export (only a DecFIGS hint suggesting a DebugLog triplet, which is a
        // no-op stub anyway). Only the return value is grounded -- E_STATUS_ERROR is the sole
        // EStatus consistent with the error sub-state; the speculative logging is not reconstructed.
        return E_STATUS_ERROR;
    }

    // -- suspension callback ----------------------------------------------------

    void LaunchManager::SuspensionFinishedCallback(bool lbSuccess, void* lpUserData)
    {
        LaunchManager* lpLaunchManager = static_cast<LaunchManager*>(lpUserData);

        if (lbSuccess)
        {
            lpLaunchManager->meSubState = E_SUBSTATE_WAITING;
        }
        else
        {
            DebugLog("[LAUNCH MGR]: ");
            DebugLog("Error suspending server interface for launch\n");
            DebugLog("\n");
            lpLaunchManager->meSubState = E_SUBSTATE_UNLOCK_FROM_ERROR;
        }
    }
}
