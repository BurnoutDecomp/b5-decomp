// ===================================================================================
// BrnNetwork::NetworkPlayerStatsManager  -- recovered function bodies
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkPlayerStatsManager.cpp
//
// This TU homes the recovered functions of the online player-stats manager:
//   NetworkPlayerStatsManager()              @ 0x827E1110  (the C++ constructor)
//   GetPlayerStatsByName(const char*)        @ 0x825526C8
//   HandleOfflineProgressionEvent(...)       @ 0x82546BB0  (private Process*Simulation helper)
//   the lifecycle (Construct / Prepare / Release / Destruct / Disconnected), the request queue
//   (AddEvent, CopyEvents, CopyNextValidEventOutOfQueue, AddEventFromNetworkPlayerID,
//   RequestPlayerStats, ValidateQueueAndPostGettingStatsEvents), the per-player update exchange
//   (AddPlayer, RemovePlayer, ClearAllMessaage, SendAndRecieveUpdateMessages, UpdatePlayersStats,
//   UpdateLocalPlayersStat, CheckForLocalPlayerTakedowns), the stats download (ProcessResults,
//   CopyResultsToOutputBuffer, CheckForAndHandleServerErrors, GetPlayerNameFromID,
//   IsThisLocalPlayer), the lookups, HandleGameResults, both Process*Simulation passes,
//   OnGameStart (an empty body sharing the console's common empty function), and the inlined
//   helpers GetNonConstLocalPlayerStats / GetStatsUpdateEntry / MarkNeedsUpdatingFlagInAllPlayers /
//   ServerBusy / HandleNewNumberOfRivals.
//
// Every store / branch / constant below is grounded in the X360 assembly listed in the
// dossier postmortem (scratchpad/wave5). Sub-object layout/offsets are by-name only; the
// absolute X360 byte offsets are quoted as provenance comments.
// ===================================================================================

#include "GameSource/Network/Managers/BrnNetworkPlayerStatsManager.h"

#include <cstring>  // std::memcpy

#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT
#include "GameSource/Network/BrnServerInterface.h"                              // BrnServerInterface::GetStatus / EStatus
#include "GameSource/Network/BrnServerInterfaceBase.h"                          // GetCustomCommandsComponent()
#include "GameSource/Network/Components/BrnServerInterfaceCustomCommands.h"     // ServerInterfaceCustomCommands::UploadOfflineProgress
#include "GameSource/Network/BrnNetworkModule.h"                                // BrnNetworkModule::GetNetworkEventQueue
#include "GameSource/Network/BrnNetworkModuleIO.h"                              // NetworkEventQueue
#include "GameSource/Network/BrnNetworkOutEventTypeDefs.h"                      // NetworkOutGetOfflineProgression
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                // VariableEventQueue::AddEvent
#include "GameShared/GameClasses/Development/CgsStrStream.h"                     // CgsDev::StrStream (streamed asserts)
#include "GameShared/GameClasses/Network/CgsNetworkConstants.h"                  // CgsNetwork::K_INVALID_PLAYER_ID
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"             // PlayerManager::GetPlayerByID / GetNext*PlayerID
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"             // NetworkPlayer::(Un)RegisterMessageType
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h" // IsLoggedIn
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfo.h" // stat-view download
#include "GameSource/Network/BrnNetworkManager.h"                                // GetPlayerManager / GetStateManager / GetLiveRevengeManager
#include "GameSource/Network/Managers/BrnNetworkStateManager.h"                  // StateManager::IsIdle
#include "GameSource/Network/Managers/BrnNetworkLiveRevengeManager.h"            // LiveRevengeManager::GetNumberOfRivals
#include "GameSource/Network/Managers/BrnStatsRequestEvent.h"                    // StatsRequestEvent::Construct
#include "GameSource/Network/Parameters/BrnNetworkPlayerInfoData.h"              // PlayerInfoData (local player's lobby record)
#include "GameSource/Network/Components/BrnServerInterfaceDownloadableConfig.h" // TimeTillStatsExpire
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h" // IsLocalPlayerInGame
#include "GameShared/GameClasses/Network/Time/CgsTimeManager.h"                  // TimeManager::GetU16FrameCount
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                         // ::LobbyNameCmp
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                      // CgsDev::Log::gpDebugPrint

namespace BrnNetwork
{
    // The stats view the stats download selects on the player-info component.
    const char* KPC_VIEW_STATS = "lobby";

    namespace
    {
        // The per-player stats update message's wire id (AddPlayer / RemovePlayer register it).
        const s32 KI_STATS_UPDATE_MESSAGE_TYPE = 40;

        // The display-type tags the player-info component reports for each stat row.
        const s32 KI_STAT_ROW_TYPE_RANK    = 0x7E726E6B;   // "~rnk"
        const s32 KI_STAT_ROW_TYPE_NUMBER  = 0x7E6E756D;   // "~num"
        const s32 KI_STAT_ROW_TYPE_PERCENT = 0x7E706374;   // "~pct"
        const s32 KI_STAT_ROW_TYPE_TIME    = 0x7E74696D;   // "~tim"
    }

    // -----------------------------------------------------------------------------
    // NetworkPlayerStatsManager() @ 0x827E1110
    //
    // The compiler-synthesised C++ constructor. The X360 body is purely the inlined
    // construction of this object's embedded sub-objects:
    //   * each of the 7 StatsUpdateEntry's two StatsUpdateMessage members has its message
    //     vtable installed (the 14 `stw off_820CF30C` stores at +0x28C..+0x2A0);
    //   * the NetworkPlayerStatsResults cache (mPlayerStatsCache) has each of its 32
    //     NetworkPlayerStats records cleared (the 33-iteration loop zeroing each record's
    //     timestamp word + count, starting at +0x5D8);
    //   * the OnlineGameResults buffer (mBufferedOnlineStats) has its GameAction vtable
    //     installed (the `stw off_820CE3E4` at +0x1088 off the cache base) and its leading
    //     result words zeroed.
    //
    // The manager's OWN scalar/pointer/flag members (meCurrentStatus, the mpNetwork* pointers,
    // the mbWaiting* flags, miCachedNumberOfChallegesCompleted) are NOT touched by this ctor in
    // the asm -- they are initialised later by Construct(). So the human constructor simply lets
    // each embedded sub-object construct; the vtable installs + cache clear are owned by those
    // sub-objects' own constructors (StatsUpdateMessage / NetworkPlayerStatsResults /
    // OnlineGameResults), exactly as the binary inlines them here.
    // -----------------------------------------------------------------------------
    NetworkPlayerStatsManager::NetworkPlayerStatsManager()
    {
        // (no explicit body: the embedded sub-objects construct themselves, matching the
        //  binary's inlined sub-object construction; the manager's own scalars are set by
        //  Construct(), not by this ctor.)
    }

    // -----------------------------------------------------------------------------
    // GetPlayerStatsByName(const char* lpcName) @ 0x825526C8
    //
    // Assert the name is non-null, then forward to the cache's by-name lookup. The X360
    // tail-calls NetworkPlayerStatsResults::GetPlayerStats on this object's mPlayerStatsCache
    // (asm `addi r3, r30, 0x578`). The cache returns a mutable NetworkPlayerStats*; this
    // accessor hands it back const.
    // -----------------------------------------------------------------------------
    const NetworkPlayerStats* NetworkPlayerStatsManager::GetPlayerStatsByName(const char* lpcName)
    {
        CGS_ASSERT(lpcName != nullptr, "lpcName");

        return mPlayerStatsCache.GetPlayerStats(lpcName);
    }

    // -----------------------------------------------------------------------------
    // HandleOfflineProgressionEvent(const NetworkInOfflineProgression* lpEvent) @ 0x82546BB0
    //
    // Cache the incoming event's freeburn-challenge success count, then decide what to do with
    // the offline-progression record:
    //   * If we are armed for an offline-progression upload from the gamestate
    //     (mbWaitingForOfflineProgressFromGamestate), AND the manager is idle
    //     (meCurrentStatus == E_STATS_STATUS_IDLE), AND the server-interface's custom-commands
    //     component reports idle (GetStatus(E_COMPONENTS_CUSTOM_COMMANDS) == E_STATUS_IDLE):
    //       go to E_STATS_STATUS_OFFLINE_PROGRESSION and upload the record now, clearing both
    //       the armed flag and the pending-upload flag.
    //   * Otherwise: buffer the whole event for a later upload and raise the pending-upload flag.
    //
    // X360 offsets (provenance): mpServerInterface @+0x1774, meCurrentStatus @+0x177C,
    // miCachedNumberOfChallegesCompleted @+0x1780, mbWaitingToUploadOfflineProgress @+0x1785,
    // mbWaitingForOfflineProgressFromGamestate @+0x1786, mBufferedOfflineProgression @+0x1728.
    // The component status code 9 == CgsNetwork::E_COMPONENTS_CUSTOM_COMMANDS; the idle status
    // value 2 == BrnServerInterface::E_STATUS_IDLE; the new status 7 ==
    // E_STATS_STATUS_OFFLINE_PROGRESSION (all read from the asm immediates).
    // -----------------------------------------------------------------------------
    void NetworkPlayerStatsManager::HandleOfflineProgressionEvent(
        const BrnNetworkModuleIO::NetworkInOfflineProgression* lpEvent)
    {
        // Always cache the event's freeburn-challenge success count (asm: store of *(a2+0x40)
        // into +0x1780, unconditional).
        miCachedNumberOfChallegesCompleted = lpEvent->miFreeburnChallengeSuccessCount;

        if (!mbWaitingForOfflineProgressFromGamestate)
            return;

        if (meCurrentStatus == E_STATS_STATUS_IDLE
            && mpServerInterface->GetStatus(CgsNetwork::E_COMPONENTS_CUSTOM_COMMANDS)
                   == BrnServerInterface::E_STATUS_IDLE)
        {
            meCurrentStatus = E_STATS_STATUS_OFFLINE_PROGRESSION;

            CGS_ASSERT(mpServerInterface != nullptr, "mpServerInterface");
            CGS_ASSERT(mpServerInterface->GetCustomCommandsComponent() != nullptr,
                       "mpServerInterface->GetCustomCommandsComponent()");

            // The event's OfflineProgressionT record sits at offset 0, so the event pointer
            // doubles as the OFFPROG record pointer the custom-commands component expects.
            mpServerInterface->GetCustomCommandsComponent()->UploadOfflineProgress(
                &lpEvent->mOfflineProgression, miCachedNumberOfChallegesCompleted);

            mbWaitingForOfflineProgressFromGamestate = false;
            mbWaitingToUploadOfflineProgress         = false;
        }
        else
        {
            // Buffer the whole 68-byte event for a later upload attempt (asm: memcpy of 0x44
            // bytes from a2 into +0x1728), and raise the pending-upload flag.
            std::memcpy(&mBufferedOfflineProgression, lpEvent, sizeof(mBufferedOfflineProgression));
            mbWaitingToUploadOfflineProgress = true;
        }
    }

    // -----------------------------------------------------------------------------
    // UploadOfflineProgression()
    //
    // Ask the game state for the offline progression record (the one-byte request event, tag
    // 55) and arm the wait for it; HandleOfflineProgressionEvent uploads it when it arrives.
    // -----------------------------------------------------------------------------
    void NetworkPlayerStatsManager::UploadOfflineProgression()
    {
        BrnNetworkModuleIO::NetworkOutGetOfflineProgression lRequestEvent;
        mpNetworkModule->GetNetworkEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequestEvent),
            lRequestEvent.GetEventType(), sizeof(lRequestEvent));
        mbWaitingForOfflineProgressFromGamestate = true;
    }

    // -----------------------------------------------------------------------------
    // HandleFreeburnChallengeEvent(const NetworkInFreeburnChallengeEvent*)
    //
    // A freeburn challenge was completed: remember the new completed-challenge count and
    // publish it as the local player's challenges-completed stat. (Inlined into the state
    // manager's network-event pump on the console.)
    // -----------------------------------------------------------------------------
    void NetworkPlayerStatsManager::HandleFreeburnChallengeEvent(
        const BrnNetworkModuleIO::NetworkInFreeburnChallengeEvent* lpEvent)
    {
        if (lpEvent->meChallengeStatus == BrnGameState::E_CHALLENGE_STATUS_SUCCESS)
        {
            miCachedNumberOfChallegesCompleted = lpEvent->miNumberOfCompletedChallenges;
            UpdateLocalPlayersStat(NetworkPlayerStats::E_STATS_VALUE_CHALLENGES_COMPLETED,
                                   lpEvent->miNumberOfCompletedChallenges);
        }
    }

    // -----------------------------------------------------------------------------
    // HandleNewNumberOfAchievements(s32)
    //
    // Publish the new achievement count as the local player's achievements stat. (Inlined into
    // the state manager's game-state action pump on the console.)
    // -----------------------------------------------------------------------------
    void NetworkPlayerStatsManager::HandleNewNumberOfAchievements(s32 liNumberOfAchievements)
    {
        UpdateLocalPlayersStat(NetworkPlayerStats::E_STATS_VALUE_ACHIEVEMENTS_EARNT, liNumberOfAchievements);
    }
    // -----------------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------------

    // Latch the module and its network manager, build the record cache (and its debug component),
    // empty the request queue, reset the scalar state and every per-player update entry.
    void NetworkPlayerStatsManager::Construct(BrnNetworkModule* lpNetworkModule)
    {
        CGS_ASSERT(lpNetworkModule != nullptr, "lpNetworkModule");
        mpNetworkModule  = lpNetworkModule;
        mpNetworkManager = lpNetworkModule->GetNetworkManager();
        CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");

        mPlayerStatsCache.Construct(mpNetworkManager);

        mEventQueue.miReadIndex  = 0;
        mEventQueue.miWriteIndex = 0;
        mEventQueue.miLength     = 0;

        mpServerInterface                        = nullptr;
        mpServerInterfacePlayerInfo              = nullptr;
        meCurrentStatus                          = E_STATS_STATUS_UNPREPARED;
        mbWaitingToUploadStats                   = false;
        mbWaitingToUploadOfflineProgress         = false;
        mbWaitingForOfflineProgressFromGamestate = false;
        miCachedNumberOfChallegesCompleted       = 0;

        ClearAllMessaage();
    }

    // Prepare the record cache, cache the server interface and its player-info component, and go
    // idle.
    bool NetworkPlayerStatsManager::Prepare()
    {
        if (!mPlayerStatsCache.Prepare())
        {
            return false;
        }

        CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager != NULL");
        mpServerInterface = mpNetworkManager->GetServerInterface();
        CGS_ASSERT(mpServerInterface != nullptr, "mpServerInterface != NULL");
        mpServerInterfacePlayerInfo = mpServerInterface->GetPlayerInfoComponent();
        CGS_ASSERT(mpServerInterfacePlayerInfo != nullptr, "mpServerInterfacePlayerInfo != NULL");

        mEventQueue.miReadIndex  = 0;
        meCurrentStatus          = E_STATS_STATUS_IDLE;
        mEventQueue.miWriteIndex = 0;
        mEventQueue.miLength     = 0;

        mbWaitingToUploadStats                   = false;
        mbWaitingToUploadOfflineProgress         = false;
        mbWaitingForOfflineProgressFromGamestate = false;
        miCachedNumberOfChallegesCompleted       = 0;

        ClearAllMessaage();
        return true;
    }

    bool NetworkPlayerStatsManager::Release()
    {
        mEventQueue.miReadIndex  = 0;
        mEventQueue.miWriteIndex = 0;
        mEventQueue.miLength     = 0;

        mpServerInterfacePlayerInfo = nullptr;
        mpServerInterface           = nullptr;
        meCurrentStatus             = E_STATS_STATUS_UNPREPARED;

        mbWaitingToUploadStats                   = false;
        mbWaitingToUploadOfflineProgress         = false;
        mbWaitingForOfflineProgressFromGamestate = false;
        miCachedNumberOfChallegesCompleted       = 0;

        ClearAllMessaage();
        mPlayerStatsCache.Release();
        return true;
    }

    void NetworkPlayerStatsManager::Destruct()
    {
        mPlayerStatsCache.Destruct();

        mbWaitingToUploadStats                   = false;
        mbWaitingToUploadOfflineProgress         = false;
        mbWaitingForOfflineProgressFromGamestate = false;
        miCachedNumberOfChallegesCompleted       = 0;

        ClearAllMessaage();
    }

    // Lost the connection: drop every queued request and go back to idle.
    void NetworkPlayerStatsManager::Disconnected()
    {
        mEventQueue.miReadIndex  = 0;
        meCurrentStatus          = E_STATS_STATUS_IDLE;
        mEventQueue.miWriteIndex = 0;
        mEventQueue.miLength     = 0;

        ClearAllMessaage();
    }

    // Nothing to do when a game starts.
    void NetworkPlayerStatsManager::OnGameStart()
    {
    }

    // Free every per-player update entry and rebuild its message pair.
    void NetworkPlayerStatsManager::ClearAllMessaage()
    {
        for (s32 liEntry = 0; liEntry < KI_NUMBER_OF_STATS_UPDATE_ENTRIES; ++liEntry)
        {
            maStatsUpdateEntry[liEntry].mPlayerID   = CgsNetwork::K_INVALID_PLAYER_ID;
            maStatsUpdateEntry[liEntry].mbOutOfDate = false;
            maStatsUpdateEntry[liEntry].mSendMessage.Construct();
            maStatsUpdateEntry[liEntry].mRecieveMessage.Construct();
        }
    }

    // -----------------------------------------------------------------------------
    // Request queue
    // -----------------------------------------------------------------------------

    void NetworkPlayerStatsManager::AddEvent(const StatsRequestEvent* lpStatsRequestEvent)
    {
        CGS_ASSERT(lpStatsRequestEvent != nullptr, "lpStatsRequestEvent");
        CGS_ASSERT(mEventQueue.GetLength() + 1 < mEventQueue.GetMaxLength(),
                   "mEventQueue.GetLength() + 1 < mEventQueue.GetMaxLength()");
        mEventQueue.Push(lpStatsRequestEvent);
    }

    // Pop the next queued request into mCurrentEventBeingProcessed; false when the queue is empty.
    bool NetworkPlayerStatsManager::CopyNextValidEventOutOfQueue()
    {
        if (mEventQueue.GetLength() <= 0)
        {
            return false;
        }
        mEventQueue.Pop(&mCurrentEventBeingProcessed);
        return true;
    }

    // Queue a stats request for a connected player, by name.
    void NetworkPlayerStatsManager::AddEventFromNetworkPlayerID(NetworkPlayerID lNetworkPlayerID)
    {
        CGS_ASSERT(lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
                   "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

        StatsRequestEvent lEvent;
        lEvent.Construct(GetPlayerNameFromID(lNetworkPlayerID), lNetworkPlayerID);
        AddEvent(&lEvent);
    }

    // Queue a stats request for a player known only by name.
    void NetworkPlayerStatsManager::RequestPlayerStats(const char* lpcPlayerName)
    {
        CGS_ASSERT(lpcPlayerName != nullptr, "lpcPlayerName");

        StatsRequestEvent lEvent;
        lEvent.Construct(lpcPlayerName, CgsNetwork::K_INVALID_PLAYER_ID);
        AddEvent(&lEvent);
    }

    // -----------------------------------------------------------------------------
    // Players
    // -----------------------------------------------------------------------------

    // The update entry tracking lPlayerID (K_INVALID_PLAYER_ID finds a free entry), or NULL.
    NetworkPlayerStatsManager::StatsUpdateEntry* NetworkPlayerStatsManager::GetStatsUpdateEntry(
        NetworkPlayerID lPlayerID)
    {
        for (s32 liEntryIndex = 0; liEntryIndex < KI_NUMBER_OF_STATS_UPDATE_ENTRIES; ++liEntryIndex)
        {
            if (maStatsUpdateEntry[liEntryIndex].mPlayerID == lPlayerID)
            {
                return &maStatsUpdateEntry[liEntryIndex];
            }
        }
        return nullptr;
    }

    // A player joined: ask for their stats, claim a free update entry for them and register its
    // message pair with their network player.
    void NetworkPlayerStatsManager::AddPlayer(NetworkPlayerID lNetworkPlayerId)
    {
        CGS_ASSERT(lNetworkPlayerId != CgsNetwork::K_INVALID_PLAYER_ID,
                   "lNetworkPlayerId != CgsNetwork::K_INVALID_PLAYER_ID");

        AddEventFromNetworkPlayerID(lNetworkPlayerId);

        CgsNetwork::NetworkPlayer* lpNetworkPlayer =
            mpNetworkManager->GetPlayerManager()->GetPlayerByID(lNetworkPlayerId);
        if (lpNetworkPlayer != nullptr)
        {
            StatsUpdateEntry* lpStatsEntry = GetStatsUpdateEntry(CgsNetwork::K_INVALID_PLAYER_ID);
            if (lpStatsEntry == nullptr)
            {
                char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "No room to add player " << lNetworkPlayerId << " in " << __FUNCTION__;
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
                CgsDev::Assert::EndAssert();
            }

            lpStatsEntry->mPlayerID = lNetworkPlayerId;
            lpStatsEntry->mSendMessage.Construct();
            lpStatsEntry->mRecieveMessage.Construct();
            lpNetworkPlayer->RegisterMessageType(KI_STATS_UPDATE_MESSAGE_TYPE, sizeof(StatsUpdateMessage),
                                                 &lpStatsEntry->mSendMessage, &lpStatsEntry->mRecieveMessage,
                                                 nullptr, nullptr, nullptr);
        }
    }

    // A player left: free their update entry and unregister its message pair.
    void NetworkPlayerStatsManager::RemovePlayer(NetworkPlayerID lNetworkPlayerId)
    {
        CGS_ASSERT(lNetworkPlayerId != CgsNetwork::K_INVALID_PLAYER_ID,
                   "lNetworkPlayerId != CgsNetwork::K_INVALID_PLAYER_ID");

        CgsNetwork::NetworkPlayer* lpNetworkPlayer =
            mpNetworkManager->GetPlayerManager()->GetPlayerByID(lNetworkPlayerId);
        if (lpNetworkPlayer != nullptr)
        {
            StatsUpdateEntry* lpStatsEntry = GetStatsUpdateEntry(lNetworkPlayerId);
            if (lpStatsEntry == nullptr)
            {
                char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "Player " << lNetworkPlayerId << " was not registered in " << __FUNCTION__;
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
                CgsDev::Assert::EndAssert();
            }

            lpStatsEntry->mPlayerID = CgsNetwork::K_INVALID_PLAYER_ID;
            lpStatsEntry->mSendMessage.Destruct();
            lpStatsEntry->mRecieveMessage.Destruct();
            lpNetworkPlayer->UnRegisterMessageType(KI_STATS_UPDATE_MESSAGE_TYPE);
        }
    }

    // -----------------------------------------------------------------------------
    // Stats records
    // -----------------------------------------------------------------------------

    const NetworkPlayerStats* NetworkPlayerStatsManager::GetLocalPlayerStats()
    {
        const NetworkPlayerStats* lpLocalPlayerStats = mPlayerStatsCache.GetLocalPlayerStats();
        return lpLocalPlayerStats;
    }

    NetworkPlayerStats* NetworkPlayerStatsManager::GetNonConstLocalPlayerStats()
    {
        NetworkPlayerStats* lpLocalPlayerStats = mPlayerStatsCache.GetLocalPlayerStats();
        return lpLocalPlayerStats;
    }

    const NetworkPlayerStats* NetworkPlayerStatsManager::GetPlayerStatsByID(NetworkPlayerID lPlayerID)
    {
        CGS_ASSERT(lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID, "lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");
        return GetPlayerStatsByName(GetPlayerNameFromID(lPlayerID));
    }

    // Every connected player's update entry has to be resent.
    void NetworkPlayerStatsManager::MarkNeedsUpdatingFlagInAllPlayers()
    {
        for (s32 liEntryIndex = 0; liEntryIndex < KI_NUMBER_OF_STATS_UPDATE_ENTRIES; ++liEntryIndex)
        {
            if (maStatsUpdateEntry[liEntryIndex].mPlayerID != CgsNetwork::K_INVALID_PLAYER_ID)
            {
                maStatsUpdateEntry[liEntryIndex].mbOutOfDate = true;
            }
        }
    }

    // Overwrite one of the local player's cached stat values and flag every player's update
    // entry for a resend.
    void NetworkPlayerStatsManager::UpdateLocalPlayersStat(NetworkPlayerStats::EStatsValue leStatType, s32 liValue)
    {
        NetworkPlayerID lLocalPlayerID;
        if (mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lLocalPlayerID))
        {
            NetworkPlayerStats* lpPlayerStats = mPlayerStatsCache.GetPlayerStats(lLocalPlayerID);
            if (lpPlayerStats != nullptr)
            {
                CGS_ASSERT(leStatType >= NetworkPlayerStats::E_STATS_VALUE_START,
                           "leStatType >= NetworkPlayerStats::E_STATS_VALUE_START");
                CGS_ASSERT(leStatType < NetworkPlayerStats::E_STATS_VALUE_COUNT,
                           "leStatType < NetworkPlayerStats::E_STATS_VALUE_COUNT");
                lpPlayerStats->SetStatAsInt(leStatType, liValue, NetworkPlayerStats::E_STAT_TYPE_NUMBER);
                MarkNeedsUpdatingFlagInAllPlayers();
            }
        }
    }

    // The local player's rival count changed.
    void NetworkPlayerStatsManager::HandleNewNumberOfRivals()
    {
        UpdateLocalPlayersStat(NetworkPlayerStats::E_STATS_VALUE_NUMBER_OF_RIVALS,
                               mpNetworkManager->GetLiveRevengeManager()->GetNumberOfRivals());
    }

    // Upload the finished free-burn lobby game's stats now when the manager and the custom-commands
    // component are both idle; otherwise keep a copy and upload it from ProcessBeforeSimulation.
    void NetworkPlayerStatsManager::UploadFreeBurnLobbyStats(
        const BrnGameState::GameStateModuleIO::OnlineGameResults* lpResults)
    {
        CGS_ASSERT(lpResults != nullptr, "lpResults");

        if (meCurrentStatus == E_STATS_STATUS_IDLE
            && mpServerInterface->GetStatus(CgsNetwork::E_COMPONENTS_CUSTOM_COMMANDS)
                   == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE)
        {
            CGS_ASSERT(mpServerInterface != nullptr, "mpServerInterface");
            CGS_ASSERT(mpServerInterface->GetCustomCommandsComponent() != nullptr,
                       "mpServerInterface->GetCustomCommandsComponent()");
            CGS_ASSERT(mpNetworkManager->GetLiveRevengeManager() != nullptr,
                       "mpNetworkManager->GetLiveRevengeManager()");

            meCurrentStatus = E_STATS_STATUS_UPLOADING_FREE_BURN_STATS;
            mpServerInterface->GetCustomCommandsComponent()->UploadFreeBurnLobbyStats(
                lpResults, mpNetworkManager->GetLiveRevengeManager()->GetNumberOfRivals(),
                miCachedNumberOfChallegesCompleted);
            mbWaitingToUploadStats = false;
        }
        else
        {
            mBufferedOnlineStats   = *lpResults;
            mbWaitingToUploadStats = true;
        }
    }

    // A game finished: re-request the stats of every player in it (when logged in).
    void NetworkPlayerStatsManager::HandleGameResults(
        const BrnGameState::GameStateModuleIO::OnlineGameResults* /*lpResults*/)
    {
        CGS_ASSERT(mpServerInterface != nullptr, "mpServerInterface");
        CGS_ASSERT(mpServerInterface->GetConnectionComponent() != nullptr,
                   "mpServerInterface->GetConnectionComponent()");

        if (mpServerInterface->GetConnectionComponent()->IsLoggedIn())
        {
            CgsNetwork::PlayerManager* lpPlayerManager = mpNetworkManager->GetPlayerManager();
            CGS_ASSERT(lpPlayerManager != nullptr, "lpPlayerManager");

            NetworkPlayerID lPlayerID = CgsNetwork::K_INVALID_PLAYER_ID;
            CGS_ASSERT(lpPlayerManager->GetNextLocalPlayerID(&lPlayerID),
                       "lpPlayerManager->GetNextLocalPlayerID( &lPlayerID )");

            lPlayerID = CgsNetwork::K_INVALID_PLAYER_ID;
            while (lpPlayerManager->GetNextPlayerID(&lPlayerID,
                                                    CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
            {
                AddEventFromNetworkPlayerID(lPlayerID);
            }
        }
    }

    // -----------------------------------------------------------------------------
    // Per-frame
    // -----------------------------------------------------------------------------

    // True while the given server-interface component is not idle.
    bool NetworkPlayerStatsManager::ServerBusy(CgsNetwork::EComponents leComponent)
    {
        return mpServerInterface->GetStatus(leComponent) != CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE;
    }

    // Pull the game's stats requests and offline-progression events, then run the takedown check
    // and the per-player update message exchange.
    void NetworkPlayerStatsManager::ProcessAfterSimulation(
        const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInput)
    {
        CGS_ASSERT(lpInput != nullptr, "lpInput");
        CGS_ASSERT(lpInput->GetStatsInputInterface() != nullptr, "lpInput->GetStatsInputInterface()");
        CopyEvents(lpInput->GetStatsInputInterface()->GetStatsInputQueue());

        CGS_ASSERT(lpInput->GetNetworkEventQueue() != nullptr, "lpInput->GetNetworkEventQueue()");

        const CgsModule::Event* lpEvent = nullptr;
        s32 liEventSize = 0;
        s32 liEventType = lpInput->GetNetworkEventQueue()->GetFirstEvent(&lpEvent, &liEventSize);
        while (lpEvent != nullptr)
        {
            if (liEventType == BrnNetworkModuleIO::NetworkInOfflineProgression::KI_EVENT_TYPE)
            {
                HandleOfflineProgressionEvent(
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInOfflineProgression*>(lpEvent));
            }
            liEventType = lpInput->GetNetworkEventQueue()->GetNextEvent(lpEvent, &lpEvent, &liEventSize);
        }

        CheckForLocalPlayerTakedowns();
        SendAndRecieveUpdateMessages();
    }

    // Validate the request queue, then (while the network state manager is idle) step the stats
    // state machine: buffered uploads first, otherwise the next queued request walks
    // view-download -> view-select -> stats-download -> results.
    void NetworkPlayerStatsManager::ProcessBeforeSimulation(BrnNetworkModuleIO::OutputBuffer* lpOutputBuffer)
    {
        CGS_ASSERT(lpOutputBuffer != nullptr, "lpOutputBuffer");
        CGS_ASSERT(meCurrentStatus != E_STATS_STATUS_UNPREPARED,
                   "Trying to use the stats manager before it's prepared");

        ValidateQueueAndPostGettingStatsEvents(lpOutputBuffer);

        CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");
        CGS_ASSERT(mpNetworkManager->GetStateManager() != nullptr, "mpNetworkManager->GetStateManager()");
        if (!mpNetworkManager->GetStateManager()->IsIdle())
        {
            return;
        }

        CGS_ASSERT(mpServerInterfacePlayerInfo != nullptr, "mpServerInterfacePlayerInfo");

        switch (meCurrentStatus)
        {
        case E_STATS_STATUS_IDLE:
            if (mbWaitingToUploadStats && !ServerBusy(CgsNetwork::E_COMPONENTS_CUSTOM_COMMANDS))
            {
                UploadFreeBurnLobbyStats(&mBufferedOnlineStats);
            }
            else if (mbWaitingToUploadOfflineProgress && !ServerBusy(CgsNetwork::E_COMPONENTS_CUSTOM_COMMANDS))
            {
                HandleOfflineProgressionEvent(&mBufferedOfflineProgression);
            }
            else if (!ServerBusy(CgsNetwork::E_COMPONENTS_PLAYER_INFO) && CopyNextValidEventOutOfQueue())
            {
                if (mpServerInterfacePlayerInfo->HasStatViewInfoBeenDownloaded())
                {
                    mpServerInterfacePlayerInfo->SelectStatView(KPC_VIEW_STATS);
                    meCurrentStatus = E_STATS_STATUS_SELECTING_VIEW;
                }
                else
                {
                    mpServerInterfacePlayerInfo->DownloadStatViews();
                    meCurrentStatus = E_STATS_STATUS_DOWNLOADING_VIEWS;
                }
            }
            break;

        case E_STATS_STATUS_DOWNLOADING_VIEWS:
            if (CheckForAndHandleServerErrors(CgsNetwork::E_COMPONENTS_PLAYER_INFO)
                || ServerBusy(CgsNetwork::E_COMPONENTS_PLAYER_INFO))
            {
                break;
            }
            mpServerInterfacePlayerInfo->SelectStatView(KPC_VIEW_STATS);
            meCurrentStatus = E_STATS_STATUS_SELECTING_VIEW;
            // fall through: the view is selected, start the download this frame

        case E_STATS_STATUS_SELECTING_VIEW:
            if (CheckForAndHandleServerErrors(CgsNetwork::E_COMPONENTS_PLAYER_INFO)
                || ServerBusy(CgsNetwork::E_COMPONENTS_PLAYER_INFO))
            {
                break;
            }
            mpServerInterfacePlayerInfo->DownloadPlayersStats(mCurrentEventBeingProcessed.GetName());
            meCurrentStatus = E_STATS_STATUS_DOWNLOADING_STATS;
            // fall through: check for the results this frame

        case E_STATS_STATUS_DOWNLOADING_STATS:
            if (CheckForAndHandleServerErrors(CgsNetwork::E_COMPONENTS_PLAYER_INFO)
                || ServerBusy(CgsNetwork::E_COMPONENTS_PLAYER_INFO))
            {
                break;
            }
            ProcessResults();
            CopyResultsToOutputBuffer(lpOutputBuffer);
            meCurrentStatus = E_STATS_STATUS_IDLE;
            break;

        case E_STATS_STATUS_UPLOADING_FREE_BURN_STATS:
        case E_STATS_STATUS_OFFLINE_PROGRESSION:
        {
            bool lbFinished = false;

            CGS_ASSERT(mpServerInterface != nullptr, "mpServerInterface");
            CGS_ASSERT(mpServerInterface->GetCustomCommandsComponent() != nullptr,
                       "mpServerInterface->GetCustomCommandsComponent()");

            ServerInterfaceCustomCommands* lpCustomCommands = mpServerInterface->GetCustomCommandsComponent();
            const s32 liStatus = lpCustomCommands->GetStatus();
            if (liStatus == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE)
            {
                lbFinished = true;
            }
            else if (liStatus == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_ERROR)
            {
                lpCustomCommands->ClearLastError();
                lbFinished = true;
            }

            if (lbFinished)
            {
                if (meCurrentStatus == E_STATS_STATUS_OFFLINE_PROGRESSION)
                {
                    mpNetworkManager->OnAutoLoginProcessComplete(3);
                }
                meCurrentStatus = E_STATS_STATUS_IDLE;
            }
            break;
        }

        default:
            CGS_ASSERT(false, "Unknown StatsManager Status");
            break;
        }
    }
    // -----------------------------------------------------------------------------
    // Request / result plumbing
    // -----------------------------------------------------------------------------

    // Queue every stats request the game posted this frame.
    void NetworkPlayerStatsManager::CopyEvents(
        const BrnNetworkModuleIO::StatsInputInterface::StatsInputQueue* lpStatsEventQueue)
    {
        CGS_ASSERT(lpStatsEventQueue != nullptr, "lpStatsEventQueue");

        for (s32 liEventIndex = 0; liEventIndex < lpStatsEventQueue->GetLength(); ++liEventIndex)
        {
            AddEvent(&lpStatsEventQueue->GetEvent(liEventIndex));
        }
    }

    // Hand the record just downloaded for the current request to the game.
    void NetworkPlayerStatsManager::CopyResultsToOutputBuffer(BrnNetworkModuleIO::OutputBuffer* lpOutputBuffer)
    {
        CGS_ASSERT(lpOutputBuffer != nullptr, "lpOutputBuffer");
        CGS_ASSERT(lpOutputBuffer->GetStatsOutputInterface() != nullptr,
                   "lpOutputBuffer->GetStatsOutputInterface()");

        NetworkPlayerStats* lpStats = mPlayerStatsCache.GetPlayerStats(mCurrentEventBeingProcessed.GetName());
        lpOutputBuffer->GetStatsOutputInterface()->AppendStatsEvent(lpStats);
    }

    // Answer every queued request from the cache where possible: a player with no record yet gets
    // an empty placeholder record posted and the request put back for download; a cached record is
    // posted, and re-requested when it has expired or is still being calculated.
    void NetworkPlayerStatsManager::ValidateQueueAndPostGettingStatsEvents(
        BrnNetworkModuleIO::OutputBuffer* lpOutputBuffer)
    {
        const s32 liNumberOfEvents = mEventQueue.GetLength();

        CGS_ASSERT(lpOutputBuffer != nullptr, "lpOutputBuffer");
        CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");
        CGS_ASSERT(mpNetworkManager->GetServerInterface() != nullptr, "mpNetworkManager->GetServerInterface()");
        CGS_ASSERT(mpNetworkManager->GetServerInterface()->GetDownloadableConfigComponent() != nullptr,
                   "mpNetworkManager->GetServerInterface()->GetDownloadableConfigComponent()");

        const f32 lfTimeTillStatsExpire =
            mpNetworkManager->GetServerInterface()->GetDownloadableConfigComponent()->TimeTillStatsExpire();

        for (s32 liEventIndex = 0; liEventIndex < liNumberOfEvents; ++liEventIndex)
        {
            StatsRequestEvent lEvent;
            mEventQueue.Pop(&lEvent);

            NetworkPlayerStats* lpStats = mPlayerStatsCache.GetPlayerStats(lEvent.GetName());
            if (lpStats == nullptr)
            {
                NetworkPlayerStats lStats;
                lStats.Construct();

                const bool lbIsLocalPlayer = mpNetworkManager->IsLocalPlayer(lEvent.GetPlayerID())
                                             || IsThisLocalPlayer(lEvent.GetName());
                CGS_ASSERT(lStats.Prepare(lEvent.GetName(), Time(0.0f), lbIsLocalPlayer, lEvent.GetPlayerID()),
                           "lStats.Prepare( lEvent.GetName(), 0, mpNetworkManager->IsLocalPlayer( lEvent.GetPlayerID() ) || IsThisLocalPlayer( lEvent.GetName() ), lEvent.GetPlayerID() )");

                lpOutputBuffer->GetStatsOutputInterface()->AppendStatsEvent(&lStats);
                mEventQueue.Push(&lEvent);
            }
            else
            {
                lpStats->SetPlayerID(lEvent.GetPlayerID());

                CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");
                const Time lExpiryTime(lfTimeTillStatsExpire);
                if ((mpNetworkManager->GetTime() - lpStats->GetTimeStamp()) > lExpiryTime
                    || lpStats->IsCalculated())
                {
                    mEventQueue.Push(&lEvent);
                }

                lpOutputBuffer->GetStatsOutputInterface()->AppendStatsEvent(lpStats);
            }
        }
    }

    // Build the downloaded record for the current request out of the player-info component's
    // stat rows and store it in the cache.
    void NetworkPlayerStatsManager::ProcessResults()
    {
        NetworkPlayerStats lStats;

        const bool lbIsLocalPlayer = mpNetworkManager->IsLocalPlayer(mCurrentEventBeingProcessed.GetPlayerID())
                                     || IsThisLocalPlayer(mCurrentEventBeingProcessed.GetName());
        lStats.Prepare(mCurrentEventBeingProcessed.GetName(), mpNetworkManager->GetTime(), lbIsLocalPlayer,
                       mCurrentEventBeingProcessed.GetPlayerID());
        lStats.SetStatus(NetworkPlayerStats::E_STATS_AGE_CURRENT);

        for (NetworkPlayerStats::EStatsValue leLoopCounter = NetworkPlayerStats::E_STATS_VALUE_START;
             leLoopCounter < NetworkPlayerStats::E_STATS_VALUE_COUNT;
             leLoopCounter++)
        {
            char lacBuffer[NetworkPlayerStats::KI_MININUM_CHAR_BUFFER_SIZE];
            CGS_ASSERT(mpServerInterfacePlayerInfo->GetPlayerStats(leLoopCounter, lacBuffer,
                                                                   NetworkPlayerStats::KI_MININUM_CHAR_BUFFER_SIZE),
                       "mpServerInterfacePlayerInfo->GetPlayerStats(leLoopCounter, lacBuffer, NetworkPlayerStats::KI_MININUM_CHAR_BUFFER_SIZE)");

            s32 liStatType;
            CGS_ASSERT(mpServerInterfacePlayerInfo->GetRowType(leLoopCounter, &liStatType),
                       "mpServerInterfacePlayerInfo->GetRowType(leLoopCounter, &liStatType)");

            NetworkPlayerStats::EStatType leStatType;
            switch (liStatType)
            {
            case KI_STAT_ROW_TYPE_RANK:
            case KI_STAT_ROW_TYPE_NUMBER:
                leStatType = NetworkPlayerStats::E_STAT_TYPE_NUMBER;
                break;

            case KI_STAT_ROW_TYPE_PERCENT:
                leStatType = NetworkPlayerStats::E_STAT_TYPE_PERCENT;
                break;

            case KI_STAT_ROW_TYPE_TIME:
                leStatType = NetworkPlayerStats::E_STAT_TYPE_TIME;
                break;

            default:
            {
                char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "Unknown stat type" << liStatType;
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
                CgsDev::Assert::EndAssert();
                leStatType = NetworkPlayerStats::E_STAT_TYPE_NUMBER;
                break;
            }
            }

            lStats.SetStat(leLoopCounter, lacBuffer, leStatType);
        }

        lStats.SetTimeStamp(mpNetworkManager->GetTime());
        lStats.SetCalculated(false);
        mPlayerStatsCache.InsertPlayerStats(lStats);
    }

    // A server-interface component failed: log its error, clear it and drop back to idle.
    bool NetworkPlayerStatsManager::CheckForAndHandleServerErrors(CgsNetwork::EComponents leComponent)
    {
        CGS_ASSERT(mpServerInterface != nullptr, "mpServerInterface");

        if (mpServerInterface->GetStatus(leComponent) != CgsNetwork::ServerInterfaceDirtySock::E_STATUS_ERROR)
        {
            return false;
        }

        *CgsDev::Log::gpDebugPrint << "An Error occurred in the server interface: "
                                   << mpServerInterface->GetLastError(leComponent) << "\n";
        mpServerInterface->ClearLastError(leComponent);
        meCurrentStatus = E_STATS_STATUS_IDLE;
        return true;
    }

    // -----------------------------------------------------------------------------
    // Players
    // -----------------------------------------------------------------------------

    // The lobby name of a player: from the session's menu data while we are in a game with them,
    // otherwise the local player's own lobby record (the only other name the manager knows).
    const char* NetworkPlayerStatsManager::GetPlayerNameFromID(NetworkPlayerID lNetworkPlayerID)
    {
        PlayerInfoData lPlayerInfo;

        CGS_ASSERT(mpServerInterface != nullptr, "mpServerInterface");
        CGS_ASSERT(mpServerInterface->GetGameComponent() != nullptr, "mpServerInterface->GetGameComponent()");
        CGS_ASSERT(mpNetworkManager->GetPlayerManager() != nullptr, "mpNetworkManager->GetPlayerManager()");
        CGS_ASSERT(lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
                   "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

        if (mpServerInterface->GetGameComponent()->IsLocalPlayerInGame())
        {
            CgsNetwork::PlayerMenuData* lpMenuData =
                mpNetworkManager->GetPlayerManager()->GetMenuDataByID(lNetworkPlayerID);
            if (lpMenuData != nullptr)
            {
                CGS_ASSERT(lpMenuData->macName[0] != '\0', "lpMenuData->macName[0] != '\\0'");
                return lpMenuData->macName;
            }
        }

        CGS_ASSERT(lPlayerInfo.Prepare(), "lPlayerInfo.Prepare()");
        CGS_ASSERT(mpServerInterfacePlayerInfo != nullptr, "mpServerInterfacePlayerInfo");
        mpServerInterfacePlayerInfo->GetLocalPlayerInfo(&lPlayerInfo);
        CGS_ASSERT(lPlayerInfo.GetID() == lNetworkPlayerID, "lPlayerInfo.GetID() == lNetworkPlayerID");

        // The console hands back the name inside this stack-local record.
        return lPlayerInfo.GetName();
    }

    // True when lpcName is the local player's lobby name.
    bool NetworkPlayerStatsManager::IsThisLocalPlayer(const char* lpcName)
    {
        PlayerInfoData lPlayerInfo;
        CGS_ASSERT(lPlayerInfo.Prepare(), "lPlayerInfo.Prepare()");
        CGS_ASSERT(mpServerInterfacePlayerInfo != nullptr, "mpServerInterfacePlayerInfo");
        mpServerInterfacePlayerInfo->GetLocalPlayerInfo(&lPlayerInfo);
        return ::LobbyNameCmp(lpcName, lPlayerInfo.GetName()) == 0;
    }

    // A remote player's update message carried new counters: store them in their cached record.
    void NetworkPlayerStatsManager::UpdatePlayersStats(NetworkPlayerID lPlayerID, s32 liNumberOfChallenges,
                                                       s32 liNumberOfRivals, s32 liNumberOfAchievements,
                                                       s32 liNumberOfTakedowns)
    {
        CGS_ASSERT(lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID, "lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

        NetworkPlayerStats* lpPlayerStats = mPlayerStatsCache.GetPlayerStats(lPlayerID);
        if (lpPlayerStats != nullptr)
        {
            lpPlayerStats->SetStatAsInt(NetworkPlayerStats::E_STATS_VALUE_CHALLENGES_COMPLETED, liNumberOfChallenges,
                                        NetworkPlayerStats::E_STAT_TYPE_NUMBER);
            lpPlayerStats->SetStatAsInt(NetworkPlayerStats::E_STATS_VALUE_NUMBER_OF_RIVALS, liNumberOfRivals,
                                        NetworkPlayerStats::E_STAT_TYPE_NUMBER);
            lpPlayerStats->SetStatAsInt(NetworkPlayerStats::E_STATS_VALUE_ACHIEVEMENTS_EARNT, liNumberOfAchievements,
                                        NetworkPlayerStats::E_STAT_TYPE_NUMBER);
            lpPlayerStats->SetStatAsInt(NetworkPlayerStats::E_STATS_VALUE_TAKEDOWNS, liNumberOfTakedowns,
                                        NetworkPlayerStats::E_STAT_TYPE_NUMBER);
        }
        else
        {
            *CgsDev::Log::gpDebugPrint << "RECIEVED STATS UPDATE FOR PLAYER WE HAVE NO STATS FOR" << lPlayerID;
        }
    }

    // Count the local player's takedowns this frame into their cached record.
    void NetworkPlayerStatsManager::CheckForLocalPlayerTakedowns()
    {
        const CgsModule::EventQueue<BrnGameState::TakedownEvent, 8>* lpTakedownQueue =
            mpNetworkModule->GetTakedownEventInputQueue();

        NetworkPlayerID lLocalPlayerID = CgsNetwork::K_INVALID_PLAYER_ID;
        mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lLocalPlayerID);

        for (s32 liTakedownIndex = 0; liTakedownIndex < lpTakedownQueue->GetLength(); ++liTakedownIndex)
        {
            const BrnGameState::TakedownEvent lTakedownEvent = lpTakedownQueue->GetEvent(liTakedownIndex);
            const NetworkPlayerID lAggressorID = mpNetworkModule->GetNetworkPlayerID(lTakedownEvent.meAggressorIndex);
            if (lAggressorID != CgsNetwork::K_INVALID_PLAYER_ID && lAggressorID == lLocalPlayerID)
            {
                NetworkPlayerStats* lpLocalPlayersStats = mPlayerStatsCache.GetPlayerStats(lLocalPlayerID);
                CGS_ASSERT(lpLocalPlayersStats != nullptr, "lpLocalPlayersStats");
                if (lpLocalPlayersStats != nullptr)
                {
                    lpLocalPlayersStats->SetStatAsInt(
                        NetworkPlayerStats::E_STATS_VALUE_TAKEDOWNS,
                        *lpLocalPlayersStats->GetStatValuePtr(NetworkPlayerStats::E_STATS_VALUE_TAKEDOWNS) + 1,
                        NetworkPlayerStats::E_STAT_TYPE_NUMBER);
                    MarkNeedsUpdatingFlagInAllPlayers();
                }
            }
        }
    }

    // Per connected player: send our counters when they are out of date and it is our turn in the
    // round robin, and apply whatever update the player sent us.
    void NetworkPlayerStatsManager::SendAndRecieveUpdateMessages()
    {
        for (s32 liEntryIndex = 0; liEntryIndex < KI_NUMBER_OF_STATS_UPDATE_ENTRIES; ++liEntryIndex)
        {
            StatsUpdateEntry& lEntry = maStatsUpdateEntry[liEntryIndex];
            if (lEntry.mPlayerID == CgsNetwork::K_INVALID_PLAYER_ID)
            {
                continue;
            }

            if (lEntry.mbOutOfDate
                && mpNetworkManager->GetPlayerManager()->IsPlayerTurnToSendRoundRobinMessage(lEntry.mPlayerID, true, 0))
            {
                const NetworkPlayerStats* lpLocalPlayerStats = GetLocalPlayerStats();
                CGS_ASSERT(lpLocalPlayerStats != nullptr, "lpLocalPlayerStats");

                lEntry.mSendMessage.PrepareForSend(
                    mpNetworkManager->GetTimeManager()->GetU16FrameCount(),
                    *lpLocalPlayerStats->GetStatValuePtr(NetworkPlayerStats::E_STATS_VALUE_CHALLENGES_COMPLETED),
                    *lpLocalPlayerStats->GetStatValuePtr(NetworkPlayerStats::E_STATS_VALUE_NUMBER_OF_RIVALS),
                    *lpLocalPlayerStats->GetStatValuePtr(NetworkPlayerStats::E_STATS_VALUE_ACHIEVEMENTS_EARNT),
                    *lpLocalPlayerStats->GetStatValuePtr(NetworkPlayerStats::E_STATS_VALUE_TAKEDOWNS));
                lEntry.mbOutOfDate = false;
            }

            s32 liRemoteNumberOfChallenges;
            s32 liRemoteNumberOfRivals;
            s32 liRemoteNumberOfAchievements;
            s32 liRemoteNumberOfTakedowns;
            if (lEntry.mRecieveMessage.Retrieve(&liRemoteNumberOfChallenges, &liRemoteNumberOfRivals,
                                                &liRemoteNumberOfAchievements, &liRemoteNumberOfTakedowns))
            {
                UpdatePlayersStats(lEntry.mPlayerID, liRemoteNumberOfChallenges, liRemoteNumberOfRivals,
                                   liRemoteNumberOfAchievements, liRemoteNumberOfTakedowns);
            }
        }
    }
}
