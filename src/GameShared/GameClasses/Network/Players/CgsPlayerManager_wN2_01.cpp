#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"                  // NetworkPlayer virtuals / Destruct
#include "GameShared/GameClasses/Network/Packeting/CgsNetworkAdapterBase.h"          // NetworkAdapter::mpMessageSentCallbackFunction
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"            // CgsDev::PerfMonCpu
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstdint>   // intptr_t

// CgsNetwork::PlayerManager -- the registry lifecycle (Construct / Prepare / Release /
// Destruct), the per-frame Update / PostUpdate / SendMessages pumps, the round edges, the
// lobby / disconnect edges and the game-id stamp.

namespace CgsNetwork
{

// The GetNextPlayerID perfmon handle starts out invalid; Prepare registers it once.
s32  PlayerManager::miNextPlayerIDPerfmon = -1;
bool PlayerManager::_mbRegisteredPerfmon  = false;

// The message-sent hook Prepare installs on the network adapter. Empty in this build.
static void MessageSentCallbackFunction(void* lpUserData)
{
    (void)lpUserData;
}

// ---- Construct -----------------------------------------------------------------------
// Point every network player's construct params at the registry and construct the players,
// seed the inactive table from the caller's tables (the last slot has menu data only), then
// reset every member, the ack/nack messages, the two embedded managers, the debug component
// and the bandwidth counters.
void PlayerManager::Construct(PlayerManagerConstructParams* lpConstructParams)
{
    CGS_ASSERT(lpConstructParams->mapPlayerList, "lpConstructParams->mapPlayerList");

    // Every slot but the last carries a network player.
    for (s32 liIndex = 0; liIndex < KI_MAX_PLAYERS - 1; ++liIndex)
    {
        CGS_ASSERT(lpConstructParams->mapConstructParams[liIndex],
                   "lpConstructParams->mapConstructParams[liIndex]");
        lpConstructParams->mapConstructParams[liIndex]->mpPlayerManager = this;
        lpConstructParams->mapPlayerList[liIndex]->Construct(lpConstructParams->mapConstructParams[liIndex]);
    }

    s32 liIndex = 0;
    for (; liIndex < KI_MAX_PLAYERS - 1; ++liIndex)
    {
        maInactivePlayers[liIndex].Set(lpConstructParams->mapPlayerList[liIndex],
                                       lpConstructParams->mapMenuData[liIndex]);
        maInactivePlayers[liIndex].SetPlayerID(K_INVALID_PLAYER_ID);
        maActivePlayers[liIndex].Set(nullptr, nullptr);
        maActivePlayers[liIndex].SetPlayerID(K_INVALID_PLAYER_ID);
    }
    for (; liIndex < KI_MAX_PLAYERS; ++liIndex)
    {
        maInactivePlayers[liIndex].Set(nullptr, lpConstructParams->mapMenuData[liIndex]);
        maInactivePlayers[liIndex].SetPlayerID(K_INVALID_PLAYER_ID);
        maActivePlayers[liIndex].Set(nullptr, nullptr);
        maActivePlayers[liIndex].SetPlayerID(K_INVALID_PLAYER_ID);
    }

    miNumActivePlayers               = 0;
    miNumInactivePlayers             = KI_MAX_PLAYERS;
    mpNetworkAdapter                 = nullptr;
    mu16CurrentFrame                 = 0;
    mpfOnReceivedFromWrongIPCallback = nullptr;
    mpfConnectionFinalisedCallback   = nullptr;
    mpConnectionFinalisedUserData    = nullptr;
    mHostPlayerID                    = K_INVALID_PLAYER_ID;
    meLocalConsoleFrameRate          = CgsSystem::E_FRAMERATE_UNKNOWN;
    mLocalPlayerID                   = K_INVALID_PLAYER_ID;
    mu8GameID                        = KU8_INVALID_GAME_ID;

    for (s32 liAck = 0; liAck < KI_MAX_ACKS_TO_BUFFER; ++liAck)
    {
        maAckMessage[liAck].Construct();
    }
    for (s32 liNack = 0; liNack < KI_MAX_NACKS_TO_BUFFER; ++liNack)
    {
        maNackMessage[liNack].Construct();
    }

    mReliableMessageManager.Construct();
    mConnectionManager.Construct();

    mpServerInterface = nullptr;
    mePrepareState    = E_CONSTRUCTED;
    ResetEventCallbacks();
    mbDiskAccessible  = true;

    mDebugComponent.Construct(this);
    ResetAllBandwidthCounters();
}

// ---- Destruct ------------------------------------------------------------------------
// Destruct every player object still referenced by either table, empty both tables and
// reset the registry to its released state.
void PlayerManager::Destruct()
{
    ResetEventCallbacks();

    for (s32 liIndex = 0; liIndex < miNumActivePlayers; ++liIndex)
    {
        NetworkPlayer* lpNetworkPlayer = maActivePlayers[liIndex].GetNetworkPlayer();
        if (lpNetworkPlayer != nullptr)
        {
            lpNetworkPlayer->Destruct();
        }
    }
    for (s32 liIndex = 0; liIndex < miNumInactivePlayers; ++liIndex)
    {
        NetworkPlayer* lpNetworkPlayer = maInactivePlayers[liIndex].GetNetworkPlayer();
        if (lpNetworkPlayer != nullptr)
        {
            lpNetworkPlayer->Destruct();
        }
    }

    for (s32 liIndex = 0; liIndex < KI_MAX_PLAYERS; ++liIndex)
    {
        maInactivePlayers[liIndex].Set(nullptr, nullptr);
        maInactivePlayers[liIndex].SetPlayerID(K_INVALID_PLAYER_ID);
        maActivePlayers[liIndex].Set(nullptr, nullptr);
        maActivePlayers[liIndex].SetPlayerID(K_INVALID_PLAYER_ID);
    }

    miNumActivePlayers      = 0;
    miNumInactivePlayers    = 0;
    mpNetworkAdapter        = nullptr;
    mpServerInterface       = nullptr;
    meLocalConsoleFrameRate = CgsSystem::E_FRAMERATE_UNKNOWN;
    mpTimeManager           = nullptr;
    mLocalPlayerID          = K_INVALID_PLAYER_ID;
    mbDiskAccessible        = true;

    mReliableMessageManager.Destruct();
    mePrepareState = E_RELEASED;

    mDebugComponent.Destruct();
    ResetAllBandwidthCounters();
}

// ---- Prepare -------------------------------------------------------------------------
// A resumable state machine: bind the session objects, then prepare the connection
// manager, then the reliable-message queue. A step that fails returns false and leaves the
// state on that step so the next call retries it. The two registry perfmons are registered
// on the first call.
bool PlayerManager::Prepare(PlayerManagerPrepareParams* lpPrepareParams)
{
    CGS_ASSERT(miNumActivePlayers == 0, "miNumActivePlayers == 0");

    switch (mePrepareState)
    {
    case E_CONSTRUCTED:
    case E_RELEASED:
        CGS_ASSERT(miNumActivePlayers == 0, "miNumActivePlayers == 0");
        CGS_ASSERT(miNumInactivePlayers == KI_MAX_PLAYERS, "miNumInactivePlayers == KI_MAX_PLAYERS");

        mpNetworkAdapter                 = lpPrepareParams->mpNetworkAdapter;
        mu8GameID                        = KU8_INVALID_GAME_ID;
        mpfOnReceivedFromWrongIPCallback = lpPrepareParams->mpfOnReceivedFromWrongIPCallback;
        mpServerInterface                = lpPrepareParams->mpServerInterface;
        meLocalConsoleFrameRate          = lpPrepareParams->meLocalConsoleFrameRate;
        mLocalPlayerID                   = K_INVALID_PLAYER_ID;
        mpTimeManager                    = lpPrepareParams->mpTimeManager;

        CGS_ASSERT(meLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_50HZ ||
                       meLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_60HZ,
                   "meLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_50HZ || meLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_60HZ");

        mpNetworkAdapter->mpMessageSentCallbackFunction =
            reinterpret_cast<void*>(&MessageSentCallbackFunction);

        mePrepareState = E_PREPARING_CONNECTION_MANAGER;
        mDebugComponent.Prepare();
        ResetAllBandwidthCounters();
        // fall through

    case E_PREPARING_CONNECTION_MANAGER:
        mePrepareState                 = E_PREPARING_CONNECTION_MANAGER;
        mpfConnectionFinalisedCallback = lpPrepareParams->mpfConnectionFinalisedCallback;
        mpConnectionFinalisedUserData  = lpPrepareParams->mpConnectionFinalisedUserData;
        if (!mConnectionManager.Prepare(this, lpPrepareParams->mpServerInterface,
                                        ConnectionFinalisedCallback, this,
                                        PlayerDisconnectedCallback, this))
        {
            return false;
        }
        mePrepareState = E_PREPARING_RELIABLE_MESSAGE_MANAGER;
        // fall through

    case E_PREPARING_RELIABLE_MESSAGE_MANAGER:
        mePrepareState = E_PREPARING_RELIABLE_MESSAGE_MANAGER;
        if (!mReliableMessageManager.Prepare(this, lpPrepareParams->mpNetworkHeapAllocator))
        {
            return false;
        }
        mePrepareState = E_FULLY_PREPARED;
        // fall through

    case E_FULLY_PREPARED:
        mePrepareState = E_FULLY_PREPARED;
        ResetEventCallbacks();
        break;

    default:
        // The console streams mePrepareState between these two literals.
        CGS_ASSERT(false, "PlayerManager: unknown prepare state \n");
        break;
    }

    if (!_mbRegisteredPerfmon)
    {
        miNextPlayerIDPerfmon = CgsDev::PerfMonCpu::AddMonitor("PLAYERManager - GetNextPlayerID",
                                                               CgsDev::E_PMP_9, false, 5.0f, true);
        miPLAYERManagerSendMessagesPM = CgsDev::PerfMonCpu::AddMonitor("PLAYERManager - SendMessages",
                                                                       CgsDev::E_PMP_9, false, 5.0f, true);
        _mbRegisteredPerfmon = true;
    }

    return true;
}

// ---- Release -------------------------------------------------------------------------
// Release every active player, forget the session objects, release both embedded managers
// and drop back to E_RELEASED. Fails (state unchanged) when the reliable-message queue does.
bool PlayerManager::Release()
{
    while (miNumActivePlayers > 0)
    {
        ReleasePlayer(0);
    }

    CGS_ASSERT(miNumActivePlayers == 0, "miNumActivePlayers == 0");
    CGS_ASSERT(miNumInactivePlayers == KI_MAX_PLAYERS, "miNumInactivePlayers == KI_MAX_PLAYERS");

    meLocalConsoleFrameRate = CgsSystem::E_FRAMERATE_UNKNOWN;
    mLocalPlayerID          = K_INVALID_PLAYER_ID;
    mpServerInterface       = nullptr;
    mpNetworkAdapter        = nullptr;
    mpTimeManager           = nullptr;
    ResetEventCallbacks();

    mDebugComponent.Release();
    ResetAllBandwidthCounters();

    mConnectionManager.Release();
    if (!mReliableMessageManager.Release())
    {
        return false;
    }

    mePrepareState = E_RELEASED;
    return true;
}

// ---- Update --------------------------------------------------------------------------
// Latch the frame; once fully prepared, drain the receive side, advance the connection
// tests and the reliable queue, then update every network player and broadcast the
// lost / regained contact edge when a player's contact flag flips during its update.
void PlayerManager::Update(const CgsSystem::TimerStatus* lpTimerStatus, u16 lu16CurrentFrame,
                           bool lbInGame)
{
    mu16CurrentFrame = lu16CurrentFrame;
    if (mePrepareState != E_FULLY_PREPARED)
    {
        return;
    }

    ReceiveMessages();
    mConnectionManager.Update(lpTimerStatus, mu16CurrentFrame);
    mReliableMessageManager.Update();

    NetworkPlayerID lPlayerID = K_INVALID_PLAYER_ID;
    while (GetNextPlayerID(&lPlayerID, E_CONSIDER_ALL_PLAYERS))
    {
        NetworkPlayer* lpNetworkPlayer = GetPlayerByID(lPlayerID);
        if (lpNetworkPlayer == nullptr)
        {
            continue;
        }

        const bool lbTalking = !lpNetworkPlayer->HasConnectionFailed();
        lpNetworkPlayer->Update(lpTimerStatus, mu16CurrentFrame, lbInGame);
        if (lbTalking != !lpNetworkPlayer->HasConnectionFailed())
        {
            BroadcastEvent(lpNetworkPlayer->HasConnectionFailed() ? E_EVENT_PLAYER_LOST_CONTACT
                                                                  : E_EVENT_PLAYER_REGAINED_CONTACT,
                           reinterpret_cast<void*>(static_cast<intptr_t>(lPlayerID)));
        }
    }
}

// ---- PostUpdate ----------------------------------------------------------------------
// Once fully prepared: the frame must match the one Update latched; then run the send pump.
void PlayerManager::PostUpdate(u16 lu16CurrentFrame)
{
    if (mePrepareState != E_FULLY_PREPARED)
    {
        return;
    }

    CGS_ASSERT(mu16CurrentFrame == lu16CurrentFrame, "mu16CurrentFrame == lu16CurrentFrame");
    mu16CurrentFrame = lu16CurrentFrame;
    SendMessages();
}

// ---- SendMessages --------------------------------------------------------------------
// Pump every active network player's sends; while the disk is unreadable the players'
// queued messages are dropped instead.
void PlayerManager::SendMessages()
{
    CgsDev::PerfMonCpu::StartMonitor(miPLAYERManagerSendMessagesPM);

    for (s32 liIndex = 0; liIndex < miNumActivePlayers; ++liIndex)
    {
        NetworkPlayer* lpNetworkPlayer = maActivePlayers[liIndex].GetNetworkPlayer();
        if (lpNetworkPlayer == nullptr)
        {
            continue;
        }

        if (mbDiskAccessible)
        {
            lpNetworkPlayer->SendMessages();
        }
        else
        {
            lpNetworkPlayer->ResetAllMessages();
        }
    }

    CgsDev::PerfMonCpu::StopMonitor(miPLAYERManagerSendMessagesPM);
}

// ---- OnRoundStart --------------------------------------------------------------------
// Forget the received-message window and tell every finalised network player.
void PlayerManager::OnRoundStart()
{
    mReliableMessageManager.ClearRcvdReliableMessages();

    NetworkPlayerID lPlayerID = K_INVALID_PLAYER_ID;
    while (GetNextPlayerID(&lPlayerID, E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
    {
        NetworkPlayer* lpNetworkPlayer = GetPlayerByID(lPlayerID);
        if (lpNetworkPlayer != nullptr)
        {
            lpNetworkPlayer->OnRoundStart();
        }
    }
}

// ---- OnRoundFinish -------------------------------------------------------------------
// Drop every buffered outgoing reliable message addressed to a finalised player.
void PlayerManager::OnRoundFinish()
{
    NetworkPlayerID lPlayerID = K_INVALID_PLAYER_ID;
    while (GetNextPlayerID(&lPlayerID, E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
    {
        mReliableMessageManager.ClearPlayersSendReliableMessages(lPlayerID);
    }
}

// ---- OnLobbyApiCreated ---------------------------------------------------------------
void PlayerManager::OnLobbyApiCreated()
{
    mConnectionManager.OnLobbyApiCreated();
}

// ---- Disconnected --------------------------------------------------------------------
// The session is gone: move every active player back to the inactive table (releasing its
// network player), forget the host, the local player and the game id, and reset the
// connection-test entries.
void PlayerManager::Disconnected()
{
    mHostPlayerID = K_INVALID_PLAYER_ID;

    for (s32 liPlayerIndex = 0; liPlayerIndex < miNumActivePlayers; ++liPlayerIndex)
    {
        maInactivePlayers[miNumInactivePlayers] = maActivePlayers[liPlayerIndex];
        ++miNumInactivePlayers;

        NetworkPlayer* lpNetworkPlayer = maActivePlayers[liPlayerIndex].GetNetworkPlayer();
        if (lpNetworkPlayer != nullptr)
        {
            lpNetworkPlayer->Release();
        }
        maActivePlayers[liPlayerIndex].SetNetworkPlayer(nullptr);
        maActivePlayers[liPlayerIndex].SetPlayerID(K_INVALID_PLAYER_ID);
    }

    miNumActivePlayers = 0;
    mLocalPlayerID     = K_INVALID_PLAYER_ID;
    mu8GameID          = KU8_INVALID_GAME_ID;
    mConnectionManager.Disconnected();
}

// ---- SetGameID -----------------------------------------------------------------------
// Adopt a new session game id (the invalid id is stored as 0) and reset every ack / nack
// signal message. The messages are stamped with game id 0, not the new id. In the nack
// pass the console tests each nack slot's valid flag but clears the matching ack slot's.
void PlayerManager::SetGameID(s32 liGameID)
{
    const u8 lu8GameID = static_cast<u8>(liGameID);
    if (mu8GameID == lu8GameID)
    {
        return;
    }

    mu8GameID = lu8GameID;
    if (lu8GameID == KU8_INVALID_GAME_ID)
    {
        mu8GameID = 0;
    }

    for (s32 liIndex = 0; liIndex < KI_MAX_ACKS_TO_BUFFER; ++liIndex)
    {
        if (maAckMessage[liIndex].IsMessageValid())
        {
            maAckMessage[liIndex].SetMessageInvalid();
        }
        maAckMessage[liIndex].Construct();
        maAckMessage[liIndex].SetGameID(0);
    }

    for (s32 liIndex = 0; liIndex < KI_MAX_NACKS_TO_BUFFER; ++liIndex)
    {
        if (maNackMessage[liIndex].IsMessageValid())
        {
            maAckMessage[liIndex].SetMessageInvalid();
        }
        maNackMessage[liIndex].Construct();
        maNackMessage[liIndex].SetGameID(0);
    }
}

}
