// ===================================================================================
// BrnNetwork::AutoLoginManager
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkAutoLoginManager.cpp
//
// The online "auto login" state machine owned by BrnNetworkManager. Reconstructed from the
// X360 binary (Construct 0x8255E138, Destruct 0x8255E1F8, Prepare 0x8255E260, Release
// 0x8255E2A0, Disconnected 0x82556DB8, Connect 0x82556CB8, ProcessBeforeSimulation 0x82579910,
// AutoLoginProcessComplete 0x82556DD0, UpdateWaitAutoLogin 0x82576230, UpdateConnecting
// 0x8254B688, UpdateConnected 0x8255E2B8). All constants below (state values 0..3, the 4
// process count, the 60s prepare timeout, the 120s connected budget, the grounded login-state
// literals 14/15, the connection-status literal 23, the game-mode bound 10) come from the
// stored immediates / cmpwi operands in those functions.
//
// The X360 bodies also stream a handful of dev-only diagnostic lines into the engine's debug
// text stream (a global that is compiled out of the retail build and not reconstructed). Those
// no-op-in-retail prints are noted in comments where they occur rather than fabricating the
// un-homed dev-stream global; the assertions are preserved via CGS_ASSERT.
// ===================================================================================

#include "GameSource/Network/Managers/BrnNetworkAutoLoginManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Network/BrnNetworkModule.h"      // BrnNetworkModule::GetNetworkManager / GetGameStateToNetworkInterface
#include "GameSource/Network/BrnNetworkManager.h"     // BrnNetworkManager
#include "GameSource/Network/BrnServerInterface.h"    // BrnServerInterface (GetConnectionComponent / GetGameComponent / GetDownloadableConfigComponent)
#include "GameSource/Network/Components/BrnServerInterfaceDownloadableConfig.h"
#include "GameSource/Network/Managers/BrnNetworkStateManager.h"  // StateManager::HandleConnectEvent / GetConnectionStatus
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h"  // GameStateToNetworkInterface::GetCurrentGameMode
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"

namespace BrnNetwork
{
    namespace
    {
        // The network manager's login state machine reports these two state values while a
        // sign-in is mid-flight; the auto-login flow treats them as "do not start another
        // sign-in" (X360 cmpwi 0xF / 0xE in UpdateWaitAutoLogin / UpdateConnecting).
        const s32 KI_NETWORK_LOGIN_STATE_SIGNING_IN = 14;   // 0xE
        const s32 KI_NETWORK_LOGIN_STATE_SIGNED_IN  = 15;   // 0xF

        // The state manager's connection status the auto-login flow waits for before it will
        // connect (X360 cmpwi 23 in UpdateWaitAutoLogin).
        const s32 KI_STATE_MANAGER_STATUS_READY = 23;

        // The auto-login flow only disconnects when the current game mode is below this value
        // (X360 cmpwi 0xA against GetGameStateToNetworkInterface()'s current-game-mode field).
        const s32 KI_GAME_MODE_DISCONNECT_BOUND = 10;

        // Prepare arms the wait timer to this many seconds (X360 SetFloatVal 60.0).
        const f32 KF_PREPARE_TIMEOUT_SECONDS = 60.0f;

        // UpdateConnected asserts the connection does not outlive this budget (X360 fcmpu 120.0).
        const f32 KF_CONNECTED_BUDGET_SECONDS = 120.0f;

        // Whether the network login state machine is mid sign-in (logged in or signing in).
        bool IsNetworkSignInInProgress(const BrnNetworkManager* lpNetworkManager)
        {
            const s32 liState = lpNetworkManager->GetNetworkLoginState();
            return (liState == KI_NETWORK_LOGIN_STATE_SIGNED_IN)
                || (liState == KI_NETWORK_LOGIN_STATE_SIGNING_IN);
        }
    }

    void
    AutoLoginManager::Construct(BrnNetworkModule* lpNetworkModule, BrnServerInterface* lpServerInterface)
    {
        mfReserved = 0.0f;
        mpNetworkModule   = lpNetworkModule;
        mpServerInterface = lpServerInterface;

        CGS_ASSERT(mpNetworkModule != nullptr, "mpNetworkModule\n");
        CGS_ASSERT(mpServerInterface != nullptr, "mpServerInterface\n");

        mTimer.SetFloatVal(0.0f);
        mProcessesComplete.UnSetAll();
        meState = E_AUTO_LOGIN_STATE_IDLE;
    }

    void
    AutoLoginManager::Destruct()
    {
        meState = E_AUTO_LOGIN_STATE_IDLE;
        mProcessesComplete.UnSetAll();
        mTimer.SetFloatVal(0.0f);
        mfReserved = 0.0f;
        mpServerInterface = nullptr;
        mpNetworkModule   = nullptr;
    }

    bool
    AutoLoginManager::Prepare()
    {
        mTimer.SetFloatVal(KF_PREPARE_TIMEOUT_SECONDS);
        mProcessesComplete.UnSetAll();
        return true;
    }

    bool
    AutoLoginManager::Release()
    {
        mProcessesComplete.UnSetAll();
        return true;
    }

    void
    AutoLoginManager::Disconnected()
    {
        mProcessesComplete.UnSetAll();
        meState = E_AUTO_LOGIN_STATE_WAIT_AUTO_LOGIN;
    }

    void
    AutoLoginManager::Connect()
    {
        if (meState != E_AUTO_LOGIN_STATE_CONNECTING)
        {
            return;
        }

        CGS_ASSERT(mpServerInterface->GetDownloadableConfigComponent() != nullptr,
                   "mpServerInterface->GetDownloadableConfigComponent()\n");

        mTimer.SetFloatVal(mpServerInterface->GetDownloadableConfigComponent()->GetAutoLoginTimeout());
        mProcessesComplete.UnSetAll();

        CGS_ASSERT(mpNetworkModule != nullptr, "mpNetworkModule\n");
        CGS_ASSERT(mpNetworkModule->GetNetworkManager() != nullptr,
                   "mpNetworkModule->GetNetworkManager()\n");

        mpNetworkModule->GetNetworkManager()->OnAutoLogin();

        mfReserved = 0.0f;
        meState = E_AUTO_LOGIN_STATE_CONNECTED;
    }

    void
    AutoLoginManager::ProcessBeforeSimulation(f32 lfFrameDelta, bool lbForceStayConnected)
    {
        switch (meState)
        {
        case E_AUTO_LOGIN_STATE_WAIT_AUTO_LOGIN:
            UpdateWaitAutoLogin(lfFrameDelta);
            break;
        case E_AUTO_LOGIN_STATE_CONNECTING:
            UpdateConnecting();
            break;
        case E_AUTO_LOGIN_STATE_CONNECTED:
            UpdateConnected(lfFrameDelta, lbForceStayConnected);
            break;
        default:
            break;
        }
    }

    void
    AutoLoginManager::AutoLoginProcessComplete(u32 luCompletedProcess)
    {
        CGS_ASSERT(luCompletedProcess < KU_AUTO_LOGIN_PROCESS_COUNT,
                   "leCompletedProcess < E_AUTO_LOGIN_PROCESS_COUNT\n");

        mProcessesComplete.SetBit(luCompletedProcess);
    }

    void
    AutoLoginManager::UpdateWaitAutoLogin(f32 lfFrameDelta)
    {
        // Count the frame time down off the wait timer.
        mTimer -= CgsSystem::Time(lfFrameDelta);

        CGS_ASSERT(mpNetworkModule != nullptr, "mpNetworkModule\n");
        CGS_ASSERT(mpNetworkModule->GetNetworkManager() != nullptr,
                   "mpNetworkModule->GetNetworkManager()\n");
        CGS_ASSERT(mpNetworkModule->GetNetworkManager()->GetStateManager() != nullptr,
                   "mpNetworkModule->GetNetworkManager()->GetStateManager()\n");

        BrnNetworkManager* lpNetworkManager = mpNetworkModule->GetNetworkManager();

        if (mTimer.GetFloatVal() <= 0.0f
            && lpNetworkManager->GetStateManager()->GetConnectionStatus() == KI_STATE_MANAGER_STATUS_READY)
        {
            CGS_ASSERT(mpServerInterface->GetDownloadableConfigComponent() != nullptr,
                       "mpServerInterface->GetDownloadableConfigComponent()\n");

            mTimer.SetFloatVal(mpServerInterface->GetDownloadableConfigComponent()->GetAutoLoginTimeout());
            meState = E_AUTO_LOGIN_STATE_CONNECTING;

            const bool lbLoggedIn = mpServerInterface->GetConnectionComponent()->IsLoggedIn();

            if (lbLoggedIn && IsNetworkSignInInProgress(lpNetworkManager))
            {
                // (dev log) "NetworkRoadRulesManager::ProcessBeforeSimulation() - Already logged in"
                Connect();
            }
            else if (IsNetworkSignInInProgress(lpNetworkManager))   // asm: body runs when login-state in {14,15}
            {
                // (dev log) "NetworkRoadRulesManager::ProcessBeforeSimulation() - NOT logged in -
                //            triggering signing in"
                StateManager::ConnectEvent lConnectEvent;
                lConnectEvent.mbTriggerSignIn = true;
                lpNetworkManager->GetStateManager()->HandleConnectEvent(&lConnectEvent);
            }
        }
    }

    void
    AutoLoginManager::UpdateConnecting()
    {
        CGS_ASSERT(mpNetworkModule != nullptr, "mpNetworkModule\n");
        CGS_ASSERT(mpNetworkModule->GetNetworkManager() != nullptr,
                   "mpNetworkModule->GetNetworkManager()\n");
        CGS_ASSERT(mpNetworkModule->GetNetworkManager()->HasLoginManager(),
                   "mpNetworkModule->GetNetworkManager()->GetLoginManager()\n");
        CGS_ASSERT(mpServerInterface != nullptr, "mpServerInterface\n");
        CGS_ASSERT(mpServerInterface->GetConnectionComponent() != nullptr,
                   "mpServerInterface->GetConnectionComponent()\n");

        BrnNetworkManager* lpNetworkManager = mpNetworkModule->GetNetworkManager();

        if (IsNetworkSignInInProgress(lpNetworkManager))   // asm: body runs when login-state in {14,15}
        {
            if (!mpServerInterface->GetConnectionComponent()->IsLoggedIn())
            {
                meState = E_AUTO_LOGIN_STATE_WAIT_AUTO_LOGIN;
            }
        }
    }

    void
    AutoLoginManager::UpdateConnected(f32 lfFrameDelta, bool lbForceStayConnected)
    {
        // Accumulate the frame time onto the connected timer.
        const f32 lfElapsed = mTimer.GetFloatVal() + lfFrameDelta;
        mTimer.SetFloatVal(lfElapsed);

        CGS_ASSERT(lfElapsed <= KF_CONNECTED_BUDGET_SECONDS,
                   "Autologin remained connected for longer than expected\n");

        // Wait until every auto-login process has reported complete: while any process bit is
        // still clear (a clear bit remains in [0, KU_AUTO_LOGIN_PROCESS_COUNT)), leave the state
        // machine in E_AUTO_LOGIN_STATE_CONNECTED and do nothing further this tick.
        if (mProcessesComplete.GetFirstClearBit()
            != CgsContainers::BitArray<KU_AUTO_LOGIN_PROCESS_COUNT>::KI_INVALID_BITINDEX)
        {
            return;
        }

        // Every process is complete. If the local player is not in a game and the current game
        // mode is below the disconnect bound (and the caller has not forced the connection to
        // stay), drop the server connection.
        CGS_ASSERT(mpServerInterface != nullptr, "mpServerInterface\n");
        CGS_ASSERT(mpServerInterface->GetGameComponent() != nullptr,
                   "mpServerInterface->GetGameComponent()\n");
        CGS_ASSERT(mpNetworkModule->GetGameStateToNetworkInterface() != nullptr,
                   "mpNetworkModule->GetGameStateToNetworkInterface()\n");

        if (!mpServerInterface->GetGameComponent()->IsLocalPlayerInGame())
        {
            const s32 liGameMode =
                static_cast<s32>(mpNetworkModule->GetGameStateToNetworkInterface()->GetCurrentGameMode());

            if (liGameMode < KI_GAME_MODE_DISCONNECT_BOUND && !lbForceStayConnected)
            {
                // (dev log) "Disconnecting from server having completed autologin processes"
                CGS_ASSERT(mpServerInterface->GetConnectionComponent() != nullptr,
                           "mpServerInterface->GetConnectionComponent()\n");
                mpServerInterface->GetConnectionComponent()->DisconnectFromServer();
            }
        }

        // All processes done -> fall back to the wait-auto-login state (X360 stw 1,8 at the
        // join point, reached whether or not the disconnect actually fired).
        meState = E_AUTO_LOGIN_STATE_WAIT_AUTO_LOGIN;
    }
}
