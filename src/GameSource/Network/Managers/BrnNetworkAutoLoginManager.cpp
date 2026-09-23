// ===================================================================================
// BrnNetwork::AutoLoginManager
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkAutoLoginManager.cpp
//
// The online "auto login" state machine owned by BrnNetworkManager. Reconstructed from the
// X360 binary (Construct 0x8255E138, Destruct 0x8255E1F8, Prepare 0x8255E260, Release
// 0x8255E2A0, Disconnected 0x82556DB8, Connect 0x82556CB8, ProcessBeforeSimulation 0x82579910,
// AutoLoginProcessComplete 0x82556DD0, UpdateWaitAutoLogin 0x82576230, UpdateConnecting
// 0x8254B688, UpdateConnected 0x8255E2B8). All constants below (state values 0..3, the 4
// process count, the 60s prepare timeout, the 120s connected budget, the connection-status
// literal 23, the game-mode bound 10) come from the stored immediates / cmpwi operands in those
// functions.
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
#include "GameSource/Network/Managers/BrnNetworkStateManager.h"  // StateManager::HandleConnectEvent / IsIdle
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"            // BrnGui::GuiEventNetworkConnect
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h"  // GameStateToNetworkInterface::GetCurrentGameMode
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"

namespace BrnNetwork
{
    namespace
    {

        // The auto-login flow only disconnects when the current game mode is below this value
        // (X360 cmpwi 0xA against GetGameStateToNetworkInterface()'s current-game-mode field).
        const s32 KI_GAME_MODE_DISCONNECT_BOUND = 10;

        // Prepare arms the wait timer to this many seconds (X360 SetFloatVal 60.0).
        const f32 KF_PREPARE_TIMEOUT_SECONDS = 60.0f;

        // UpdateConnected asserts the connection does not outlive this budget (X360 fcmpu 120.0).
        const f32 KF_CONNECTED_BUDGET_SECONDS = 120.0f;

        // Bit 0 of the high-level update set keeps the auto-login connection up.
        const BrnUpdateSet KU_UPDATE_SET_STAY_CONNECTED = 1;
    }

    void
    AutoLoginManager::Construct(BrnNetworkModule* lpNetworkModule, BrnServerInterface* lpServerInterface)
    {
        mfTimeConnected = 0.0f;
        mpNetworkModule   = lpNetworkModule;
        mpServerInterface = lpServerInterface;

        CGS_ASSERT(mpNetworkModule != nullptr, "mpNetworkModule");
        CGS_ASSERT(mpServerInterface != nullptr, "mpServerInterface");

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
        mfTimeConnected = 0.0f;
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
                   "mpServerInterface->GetDownloadableConfigComponent()");

        mTimer.SetFloatVal(mpServerInterface->GetDownloadableConfigComponent()->GetAutoLoginTimeout());
        mProcessesComplete.UnSetAll();

        CGS_ASSERT(mpNetworkModule != nullptr, "mpNetworkModule");
        CGS_ASSERT(mpNetworkModule->GetNetworkManager() != nullptr,
                   "mpNetworkModule->GetNetworkManager()");

        mpNetworkModule->GetNetworkManager()->OnAutoLogin();

        mfTimeConnected = 0.0f;
        meState = E_AUTO_LOGIN_STATE_CONNECTED;
    }

    void
    AutoLoginManager::ProcessBeforeSimulation(f32 lfFrameDelta, BrnUpdateSet luUpdateSet)
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
            UpdateConnected(lfFrameDelta, luUpdateSet);
            break;
        default:
            break;
        }
    }

    void
    AutoLoginManager::AutoLoginProcessComplete(u32 luCompletedProcess)
    {
        CGS_ASSERT(luCompletedProcess < KU_AUTO_LOGIN_PROCESS_COUNT,
                   "leCompletedProcess < E_AUTO_LOGIN_PROCESS_COUNT");

        mProcessesComplete.SetBit(luCompletedProcess);
    }

    void
    AutoLoginManager::UpdateWaitAutoLogin(f32 lfFrameDelta)
    {
        // Count the frame time down off the wait timer.
        mTimer -= CgsSystem::Time(lfFrameDelta);

        CGS_ASSERT(mpNetworkModule != nullptr, "mpNetworkModule");
        CGS_ASSERT(mpNetworkModule->GetNetworkManager() != nullptr,
                   "mpNetworkModule->GetNetworkManager()");
        CGS_ASSERT(mpNetworkModule->GetNetworkManager()->GetStateManager() != nullptr,
                   "mpNetworkModule->GetNetworkManager()->GetStateManager()");

        BrnNetworkManager* lpNetworkManager = mpNetworkModule->GetNetworkManager();

        if (!(mTimer.GetFloatVal() > 0.0f)
            && lpNetworkManager->GetStateManager()->IsIdle())
        {
            CGS_ASSERT(mpServerInterface->GetDownloadableConfigComponent() != nullptr,
                       "mpServerInterface->GetDownloadableConfigComponent()");

            mTimer.SetFloatVal(mpServerInterface->GetDownloadableConfigComponent()->GetAutoLoginTimeout());
            meState = E_AUTO_LOGIN_STATE_CONNECTING;

            const bool lbLoggedIn = mpServerInterface->GetConnectionComponent()->IsLoggedIn();

            if (lbLoggedIn && !lpNetworkManager->GetLoginManager()->IsSigningIn())
            {
                // (dev log) "NetworkRoadRulesManager::ProcessBeforeSimulation() - Already logged in"
                Connect();
            }
            else if (!lpNetworkManager->GetLoginManager()->IsSigningIn())
            {
                // (dev log) "NetworkRoadRulesManager::ProcessBeforeSimulation() - NOT logged in -
                //            triggering signing in"
                const BrnGui::GuiEventNetworkConnect lConnectEvent(LoginManagerBase::E_SIGN_IN_TYPE_SILENT);
                lpNetworkManager->GetStateManager()->HandleConnectEvent(&lConnectEvent);
            }
        }
    }

    void
    AutoLoginManager::UpdateConnecting()
    {
        CGS_ASSERT(mpNetworkModule != nullptr, "mpNetworkModule");
        CGS_ASSERT(mpNetworkModule->GetNetworkManager() != nullptr,
                   "mpNetworkModule->GetNetworkManager()");
        CGS_ASSERT(mpNetworkModule->GetNetworkManager()->GetLoginManager() != nullptr,
                   "mpNetworkModule->GetNetworkManager()->GetLoginManager()");
        CGS_ASSERT(mpServerInterface != nullptr, "mpServerInterface");
        CGS_ASSERT(mpServerInterface->GetConnectionComponent() != nullptr,
                   "mpServerInterface->GetConnectionComponent()");

        BrnNetworkManager* lpNetworkManager = mpNetworkModule->GetNetworkManager();

        if (!lpNetworkManager->GetLoginManager()->IsSigningIn())
        {
            if (!mpServerInterface->GetConnectionComponent()->IsLoggedIn())
            {
                meState = E_AUTO_LOGIN_STATE_WAIT_AUTO_LOGIN;
            }
        }
    }

    void
    AutoLoginManager::UpdateConnected(f32 lfFrameDelta, BrnUpdateSet luUpdateSet)
    {
        // Accumulate the time spent connected.
        mfTimeConnected += lfFrameDelta;

        if (mfTimeConnected > KF_CONNECTED_BUDGET_SECONDS)
        {
            // Over budget. The console first walks the four processes and, for each one whose bit
            // is still clear, writes "Autologin process: <n> failed to complete within 120 secs"
            // to the network dev-log stream (no home in this tree; not reproduced), then asserts.
            CGS_ASSERT(false, "Autologin remained connected for longer than expected\n");
        }

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
        CGS_ASSERT(mpServerInterface != nullptr, "mpServerInterface");
        CGS_ASSERT(mpServerInterface->GetGameComponent() != nullptr,
                   "mpServerInterface->GetGameComponent()");
        CGS_ASSERT(mpNetworkModule->GetGameStateToNetworkInterface() != nullptr,
                   "mpNetworkModule->GetGameStateToNetworkInterface()");

        if (!mpServerInterface->GetGameComponent()->IsLocalPlayerInGame())
        {
            const s32 liGameMode =
                static_cast<s32>(mpNetworkModule->GetGameStateToNetworkInterface()->GetCurrentGameMode());

            if (liGameMode < KI_GAME_MODE_DISCONNECT_BOUND
                && (luUpdateSet & KU_UPDATE_SET_STAY_CONNECTED) != KU_UPDATE_SET_STAY_CONNECTED)
            {
                // (dev log) "Disconnecting from server having completed autologin processes"
                CGS_ASSERT(mpServerInterface->GetConnectionComponent() != nullptr,
                           "mpServerInterface->GetConnectionComponent()");
                mpServerInterface->GetConnectionComponent()->DisconnectFromServer();
            }
        }

        // All processes done -> fall back to the wait-auto-login state (X360 stw 1,8 at the
        // join point, reached whether or not the disconnect actually fired).
        meState = E_AUTO_LOGIN_STATE_WAIT_AUTO_LOGIN;
    }
}
