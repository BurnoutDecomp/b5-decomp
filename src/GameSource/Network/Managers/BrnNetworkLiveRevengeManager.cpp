// GameSource/Network/Managers/BrnNetworkLiveRevengeManager.cpp
//
// BrnNetwork::LiveRevengeManager -- live-revenge rival tracking. The manager owns the
// 250-entry relationship table (the saved profile), maps each remote player in the session to
// a table row, turns takedowns / paybacks / marks / round results into relationship stats,
// keeps the top-ten rival list and uploads its changed rows, and syncs each rival's copy of the
// relationship over a reliable message.

#include "GameSource/Network/Managers/BrnNetworkLiveRevengeManager.h"

#include <cstdlib>   // std::qsort

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"                 // CgsDev::StrStream (streamed asserts)
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"
#include "GameShared/GameClasses/Network/CgsNetworkUtils.h"                  // CgsNetwork::UsernameCompare
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h" // EStatus
#include "GameShared/GameClasses/Network/Time/CgsTimeManager.h"              // TimeManager::GetCurrentFrameNumber
#include "GameSource/GameState/BrnGameActions.h"                             // OnlineRoundResults::GetPosition
#include "GameSource/Network/BrnNetworkModule.h"
#include "GameSource/Network/BrnNetworkModuleIO.h"                           // OutputBuffer, PostSimulationInputBuffer, NetworkEventQueue
#include "GameSource/Network/BrnNetworkManager.h"
#include "GameSource/Network/BrnNetworkInEventTypeDefs.h"                    // NetworkInPaybackIntialised / NetworkInPaybackSucceeded
#include "GameSource/Network/BrnNetworkOutEventTypeDefs.h"                   // NetworkOutRivalCount / LiveRevengeProfileData / AutosaveProfile
#include "GameSource/Network/Components/BrnServerInterfaceCustomCommands.h"  // ServerInterfaceCustomCommands
#include "GameSource/Network/BrnNetworkPlayerMenuData.h"                     // BrnNetwork::PlayerMenuData::mMarkedManID
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h"
#include "GameSource/Network/SharedIO/BrnNetworkToGuiIOInterfaces.h"         // NetworkToGuiInterface::AddLiveRevengeUpdate

namespace BrnNetwork
{
    // The "no table row" value of a mapping slot's player id.
    const s32 KI_INVALID_RACE_CAR_INDEX_TO_REVENGE_TABLE = -1;

    // ============================================================================
    // Lifecycle
    // ============================================================================

    void LiveRevengeManager::Construct(BrnNetworkModule* lpNetworkModule)
    {
        mbAreWeInOnlineGame  = false;
        mpLiveRevengeProfile = nullptr;
        miNumberOfTopRivals  = 0;

        mTakedownEventQueue.Construct();

        CGS_ASSERT(lpNetworkModule, "lpNetworkModule");
        mpNetworkModule  = lpNetworkModule;
        mpNetworkManager = lpNetworkModule->GetNetworkManager();
        CGS_ASSERT(mpNetworkManager, "mpNetworkManager");

        miNumberOfTopRivals = 0;
        ClearTopRivals();
        mbProfileIsDirty                   = false;
        mbNeedToUpdateMarksForCurrentRound = false;
        meLiveRevengeUploadStatus          = E_LIVE_REVENGE_UPLOAD_STATUS_IDLE;

        mDebugComponent.Construct(this);
    }

    bool LiveRevengeManager::Prepare(CgsMemory::HeapMalloc* lpHeapMalloc)
    {
        CGS_ASSERT(lpHeapMalloc, "lpHeapMalloc");
        mpAllocator = lpHeapMalloc;

        CGS_ASSERT(lpHeapMalloc->GetAllocator()->ValidateHeap(
                       EA::Allocator::GeneralAllocator::kHeapValidationLevelFull),
                   "lpHeapMalloc->GetAllocator()->ValidateHeap(rw::core::GeneralAllocator::kHeapValidationLevelFull)");
        mpLiveRevengeProfile = static_cast<LiveRevengeProfile*>(lpHeapMalloc->Malloc(sizeof(LiveRevengeProfile), 4));
        CGS_ASSERT(lpHeapMalloc->GetAllocator()->ValidateHeap(
                       EA::Allocator::GeneralAllocator::kHeapValidationLevelFull),
                   "lpHeapMalloc->GetAllocator()->ValidateHeap(rw::core::GeneralAllocator::kHeapValidationLevelFull)");
        CGS_ASSERT(mpLiveRevengeProfile, "mpLiveRevengeProfile");

        mpLiveRevengeProfile->Clear();
        ResetRevengeTableMappings();

        miNumberOfTopRivals = 0;
        mbProfileIsDirty                   = false;
        mbNeedToUpdateMarksForCurrentRound = false;
        meLiveRevengeUploadStatus          = E_LIVE_REVENGE_UPLOAD_STATUS_IDLE;
        ClearTopRivals();

        mDebugComponent.Prepare();
        return true;
    }

    bool LiveRevengeManager::Release()
    {
        CGS_ASSERT(mpLiveRevengeProfile, "mpLiveRevengeProfile");
        CGS_ASSERT(mpAllocator, "mpAllocator");

        mpLiveRevengeProfile->Clear();
        mpAllocator->Free(mpLiveRevengeProfile);

        mpLiveRevengeProfile = nullptr;
        mpAllocator          = nullptr;
        mpNetworkManager     = nullptr;
        ResetRevengeTableMappings();

        miNumberOfTopRivals = 0;
        mbProfileIsDirty                   = false;
        mbNeedToUpdateMarksForCurrentRound = false;
        meLiveRevengeUploadStatus          = E_LIVE_REVENGE_UPLOAD_STATUS_IDLE;
        ClearTopRivals();

        mDebugComponent.Release();
        return true;
    }

    void LiveRevengeManager::Destruct()
    {
        mbAreWeInOnlineGame  = false;
        mpLiveRevengeProfile = nullptr;
        mpAllocator          = nullptr;

        miNumberOfTopRivals = 0;
        meLiveRevengeUploadStatus          = E_LIVE_REVENGE_UPLOAD_STATUS_IDLE;
        mbProfileIsDirty                   = false;
        mbNeedToUpdateMarksForCurrentRound = false;
        ClearTopRivals();

        mDebugComponent.Destruct();
    }

    // ============================================================================
    // Per-frame
    // ============================================================================

    void LiveRevengeManager::ProcessBeforeSimulation(BrnNetworkModuleIO::OutputBuffer* lpOutputBuffer)
    {
        ProcessTakedownQueue(lpOutputBuffer);

        if (meLiveRevengeUploadStatus == E_LIVE_REVENGE_UPLOAD_STATUS_PENDING)
        {
            SendLiveRevengeRivalsToServer();
        }

        if (meLiveRevengeUploadStatus == E_LIVE_REVENGE_UPLOAD_STATUS_IN_PROGRESS)
        {
            CGS_ASSERT(mpNetworkManager, "mpNetworkManager");
            CGS_ASSERT(mpNetworkManager->GetServerInterface(), "mpNetworkManager->GetServerInterface()");
            CGS_ASSERT(mpNetworkManager->GetServerInterface()->GetCustomCommandsComponent(),
                       "mpNetworkManager->GetServerInterface()->GetCustomCommandsComponent()");

            ServerInterfaceCustomCommands* lpCustomCommands =
                mpNetworkManager->GetServerInterface()->GetCustomCommandsComponent();
            switch (lpCustomCommands->GetStatus())
            {
            case CgsNetwork::ServerInterfaceDirtySock::E_STATUS_ERROR:
                // FLAG: the error code is also written to the network log stream ("Could not
                // upload live revenge data, error code: <n>"), which has no home in this tree.
                lpCustomCommands->GetAndClearLastError();
                lpCustomCommands->ClearLastError();
                meLiveRevengeUploadStatus = E_LIVE_REVENGE_UPLOAD_STATUS_IDLE;
                mpNetworkManager->OnAutoLoginProcessComplete(1);
                break;

            case CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE:
                meLiveRevengeUploadStatus = E_LIVE_REVENGE_UPLOAD_STATUS_IDLE;
                mpNetworkManager->OnAutoLoginProcessComplete(1);
                break;

            default:
                break;
            }
        }

        {
            BrnNetworkModuleIO::NetworkOutLiveRevengeProfileData lLiveRevengeProfileEvent;
            lLiveRevengeProfileEvent.mpLiveRevengeProfile = mpLiveRevengeProfile;
            mpNetworkModule->GetNetworkEventQueue()->AddEvent(&lLiveRevengeProfileEvent,
                                                              lLiveRevengeProfileEvent.GetEventType());
        }

        if (mbNeedToUpdateMarksForCurrentRound)
        {
            const s32 liNumberOfMarksRecieved =
                mpNetworkManager->GetMarkedManManager()->GetNumberOfPlayersFinishedMarking();
            const s32 liNumberOfPlayersInGame =
                mpNetworkManager->GetPlayerManager()->GetTotalNumberPlayers(
                    CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED);
            if (liNumberOfMarksRecieved == liNumberOfPlayersInGame)
            {
                UpdateMarkedManInfo();
            }
        }

        if (mbProfileIsDirty)
        {
            CGS_ASSERT(IsTableValid(),
                       "If this assert fires then it the live revenge manager is corrupting the profile. Deleting your save game will fix this for now (tell Alex Veal).\n");

            BrnNetworkModuleIO::NetworkOutAutosaveProfile lAutosaveEvent;
            mpNetworkModule->GetNetworkEventQueue()->AddEvent(&lAutosaveEvent, lAutosaveEvent.GetEventType());
            mbProfileIsDirty = false;
        }
    }

    void LiveRevengeManager::ProcessAfterSimulation(const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInputBuffer)
    {
        CGS_ASSERT(lpInputBuffer, "lpInputBuffer");
        mTakedownEventQueue.Append(*lpInputBuffer->GetTakedownEventInputQueue());
        ProcessGameDirtyTrickInterface();
    }

    // Feed this frame's takedowns into the relationships (only in the modes that keep live
    // revenge, and never for a player inside a freeburn challenge), then report a grown rival
    // count to the game.
    void LiveRevengeManager::ProcessTakedownQueue(BrnNetworkModuleIO::OutputBuffer* lpOutputBuffer)
    {
        CGS_ASSERT(lpOutputBuffer, "lpOutputBuffer");
        CGS_ASSERT(mpNetworkManager, "mpNetworkManager");
        CGS_ASSERT(mpNetworkModule, "mpNetworkModule");

        const BrnNetworkModuleIO::GameStateToNetworkInterface* lpGameStateToNetworkInterface =
            mpNetworkModule->GetGameStateToNetworkInterface();
        CGS_ASSERT(lpGameStateToNetworkInterface, "lpGameStateToNetworkInterface");

        const s32 liNumberOfTakedowns = mTakedownEventQueue.GetLength();
        if (liNumberOfTakedowns == 0)
        {
            return;
        }

        const BrnGameState::GameStateModuleIO::EGameModeType leCurrentGameModeType =
            lpGameStateToNetworkInterface->GetCurrentGameMode();
        const s32 liOldNumberOfRivals = GetNumberOfRivals();

        if ((leCurrentGameModeType == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY ||
             leCurrentGameModeType == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME) ||
            leCurrentGameModeType == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_RACE)
        {
            for (s32 liEventCounter = 0; liEventCounter < mTakedownEventQueue.GetLength(); ++liEventCounter)
            {
                const BrnGameState::TakedownEvent lEvent = mTakedownEventQueue.GetEvent(liEventCounter);
                const EActiveRaceCarIndex leVictimActiveRaceCarIndex = lEvent.meVictimIndex;
                const EActiveRaceCarIndex leAggressorActiveCarIndex  = lEvent.meAggressorIndex;

                CGS_ASSERT(lEvent.meAggressorIndex != lEvent.meVictimIndex,
                           "lEvent.meAggressorIndex != lEvent.meVictimIndex");

                if (!lpGameStateToNetworkInterface->GetPlayerInFreeburnChallenge(leAggressorActiveCarIndex) &&
                    !lpGameStateToNetworkInterface->GetPlayerInFreeburnChallenge(leVictimActiveRaceCarIndex))
                {
                    UpdateLiveRevengeRelationShip(leAggressorActiveCarIndex, leVictimActiveRaceCarIndex,
                                                  lEvent.mbMarkedManTakeDown, lpOutputBuffer);
                }
            }
        }

        mTakedownEventQueue.Clear();

        const s32 liNewNumberOfRivals = GetNumberOfRivals();
        if (liNewNumberOfRivals > liOldNumberOfRivals)
        {
            BrnNetworkModuleIO::NetworkOutRivalCount lRivalCountMessage;
            lRivalCountMessage.miNumberOfRivals = liNewNumberOfRivals;
            mpNetworkModule->GetNetworkEventQueue()->AddEvent(&lRivalCountMessage,
                                                              lRivalCountMessage.GetEventType());
        }
    }

    // Turn this frame's payback (dirty-trick) events into relationship stats.
    void LiveRevengeManager::ProcessGameDirtyTrickInterface()
    {
        const BrnNetworkModuleIO::GameStateToNetworkInterface::DirtyTrickQueue* lpDirtyTrickEventQueue =
            mpNetworkModule->GetGameStateToNetworkInterface()->GetDirtyTrickQueue();
        CGS_ASSERT(lpDirtyTrickEventQueue, "lpDirtyTrickEventQueue");

        for (s32 liIndex = 0; liIndex < lpDirtyTrickEventQueue->GetLength(); ++liIndex)
        {
            const BrnNetworkModuleIO::DirtyTrickEvent lDirtyTrickEvent = lpDirtyTrickEventQueue->GetEvent(liIndex);
            UpdatePaybacksData(lDirtyTrickEvent.meAggressorActiveRaceCarIndex,
                               lDirtyTrickEvent.meVictimActiveRaceCarIndex,
                               lDirtyTrickEvent.meDirtyTrickStatus);
        }
    }

    // ============================================================================
    // Session and round edges
    // ============================================================================

    void LiveRevengeManager::Disconnected()
    {
        ResetRevengeTableMappings();
    }

    void LiveRevengeManager::OnRoundStart()
    {
        mbAreWeInOnlineGame = true;
        if (!mpNetworkManager->IsDoingFreeBurnLobby())
        {
            mbNeedToUpdateMarksForCurrentRound = true;
        }
    }

    // Both are empty bodies on the console.
    void LiveRevengeManager::OnLeaveGame()
    {
    }

    // Every mapped rival gets one more shared event; the profile is then due a save.
    void LiveRevengeManager::OnRoundFinish()
    {
        mbAreWeInOnlineGame = false;

        for (s32 liMappingIndex = 0; liMappingIndex < KI_MAX_NETWORK_PLAYERS; ++liMappingIndex)
        {
            if (maPlayerToTableIndexData[liMappingIndex].mPlayerID == KI_INVALID_RACE_CAR_INDEX_TO_REVENGE_TABLE)
            {
                continue;
            }

            LiveRevengeMappingEntry* lpEntry = FindMappingEntry(maPlayerToTableIndexData[liMappingIndex].mPlayerID);
            if (lpEntry)
            {
                LiveRevengeProfile* lpProfile = GetProfile();
                CGS_ASSERT(lpProfile, "lpProfile");
                lpProfile->maRelationshipTable[static_cast<u32>(lpEntry->miRevengeTableIndex)].OnRoundFinish();
            }
        }

        AutoSaveLiveRevengeProfile();
    }

    void LiveRevengeManager::OnGameFinish()
    {
    }

    // Credit the round's finishing order: a rival who finished behind the local player (or
    // did not finish) is a local win, one who finished ahead is a rival win.
    void LiveRevengeManager::HandleRoundResults(const BrnGameState::GameStateModuleIO::OnlineRoundResults* lpResults)
    {
        CGS_ASSERT(lpResults, "lpResults");
        CGS_ASSERT(mpNetworkManager, "mpNetworkManager");
        CGS_ASSERT(mpNetworkManager->GetPlayerManager(), "mpNetworkManager->GetPlayerManager()");

        NetworkPlayerID lLocalPlayerID = CgsNetwork::K_INVALID_PLAYER_ID;
        if (!mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lLocalPlayerID))
        {
            return;
        }

        const s32 liLocalPlayersPosition = lpResults->GetPosition(lLocalPlayerID);
        for (s32 liMappingIndex = 0; liMappingIndex < KI_MAX_NETWORK_PLAYERS; ++liMappingIndex)
        {
            const NetworkPlayerID lRivalPlayerID = maPlayerToTableIndexData[liMappingIndex].mPlayerID;
            if (lRivalPlayerID == KI_INVALID_RACE_CAR_INDEX_TO_REVENGE_TABLE)
            {
                continue;
            }

            const s32 liRivalPosition = lpResults->GetPosition(lRivalPlayerID);
            LiveRevengeRelationship* lpLiveRevengeRelationship = GetNonConstRevengeRelationship(lRivalPlayerID);
            CGS_ASSERT(lpLiveRevengeRelationship, "lpLiveRevengeRelationship");

            if (liLocalPlayersPosition == BrnGameState::GameStateModuleIO::OnlineRoundResults::KI_POSITION_DISCONNECTED)
            {
                continue;
            }

            if (liRivalPosition == BrnGameState::GameStateModuleIO::OnlineRoundResults::KI_POSITION_DISCONNECTED ||
                liRivalPosition > liLocalPlayersPosition)
            {
                lpLiveRevengeRelationship->AddWinByLocalPlayer();
            }
            else if (liRivalPosition < liLocalPlayersPosition)
            {
                lpLiveRevengeRelationship->AddWinByRival();
            }
        }
    }

    // Who marked whom this round: a local mark on a mapped rival, or a rival's mark on the
    // local player, counts as a mark in that relationship.
    void LiveRevengeManager::UpdateMarkedManInfo()
    {
        NetworkPlayerID lLocalPlayerID = CgsNetwork::K_INVALID_PLAYER_ID;
        mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lLocalPlayerID);

        NetworkPlayerID lPlayerID = CgsNetwork::K_INVALID_PLAYER_ID;
        while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(
                   &lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
        {
            PlayerMenuData* lpPlayerMenuData =
                static_cast<PlayerMenuData*>(mpNetworkManager->GetPlayerManager()->GetMenuDataByID(lPlayerID));
            CGS_ASSERT(lpPlayerMenuData, "lpPlayerMenuData");
            if (!lpPlayerMenuData || lpPlayerMenuData->mMarkedManID == CgsNetwork::K_INVALID_PLAYER_ID)
            {
                continue;
            }

            CGS_ASSERT(lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID, "lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");
            CGS_ASSERT(lLocalPlayerID != CgsNetwork::K_INVALID_PLAYER_ID, "lLocalPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

            if (lPlayerID == lLocalPlayerID)
            {
                LiveRevengeRelationship* lpRelationship = GetNonConstRevengeRelationship(lpPlayerMenuData->mMarkedManID);
                if (lpRelationship)
                {
                    lpRelationship->AddMarkByPlayer();
                    AutoSaveLiveRevengeProfile();
                }
                else
                {
                    CGS_ASSERT(mpNetworkManager->GetPlayerManager()->GetPlayerByID(lpPlayerMenuData->mMarkedManID) == nullptr,
                               "mpNetworkManager->GetPlayerManager()->GetPlayerByID( lpPlayerMenuData->mMarkedManID ) == NULL");
                }
            }
            else if (lpPlayerMenuData->mMarkedManID == lLocalPlayerID)
            {
                LiveRevengeRelationship* lpRelationship = GetNonConstRevengeRelationship(lPlayerID);
                CGS_ASSERT(lpRelationship, "lpRelationship");
                if (lpRelationship)
                {
                    lpRelationship->AddMarkByRival();
                    AutoSaveLiveRevengeProfile();
                }
            }
        }

        mbNeedToUpdateMarksForCurrentRound = false;
    }

    // ============================================================================
    // Players
    // ============================================================================

    // Find (or create) the new player's table row by name, map the player to it, publish it to
    // the debug menu and register the per-player sync message pair.
    void LiveRevengeManager::AddPlayer(NetworkPlayerID lNetworkPlayerID)
    {
        CGS_ASSERT(mpNetworkManager, "mpNetworkManager");
        CGS_ASSERT(mpNetworkManager->GetPlayerManager(), "mpNetworkManager->GetPlayerManager()");
        CGS_ASSERT(IsTableValid(), "IsTableValid()");

        CgsNetwork::NetworkPlayer* lpNetworkPlayer =
            mpNetworkManager->GetPlayerManager()->GetPlayerByID(lNetworkPlayerID);
        if (lpNetworkPlayer)
        {
            // The reference constant is the global KI_MAX_PLAYERS (8 network players), which
            // has no home in this tree yet; the active race-car count has the same value.
            CGS_ASSERT(NetworkPlayerIDToActiveRaceCarIndex(lNetworkPlayerID) < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                       "NetworkPlayerIDToActiveRaceCarIndex(lNetworkPlayerID) < KI_MAX_PLAYERS");

            CgsNetwork::PlayerName lPlayerName;
            lPlayerName.Construct(lpNetworkPlayer->GetName());
            // PlayerName::IsEmpty (inline): the first name character is the terminator.
            CGS_ASSERT(lPlayerName.GetPlayerName()[0] != '\0', "!lPlayerName.IsEmpty()");

            s32 liPlayerIndexInTable = FindPlayerInTableByName(lPlayerName.GetPlayerName());
            if (liPlayerIndexInTable == -1)
            {
                LiveRevengeRelationship::UniquePlayerID lUniquePlayerID;
                GetUniqueIDByName(&lPlayerName, &lUniquePlayerID);
                // UniquePlayerID::IsValid (inline): a non-empty name and a non-zero XUID.
                CGS_ASSERT(lUniquePlayerID.GetPlayerName()[0] != '\0' && lUniquePlayerID.mqXuid != 0,
                           "lUniquePlayerID.IsValid()");

                liPlayerIndexInTable = AddNewTableEntry(&lUniquePlayerID);
                CGS_ASSERT(liPlayerIndexInTable != -1, "liPlayerIndexInTable != -1");
            }

            AddMappingEntry(lNetworkPlayerID, liPlayerIndexInTable);
            AddRelationshipToDebugMenu(liPlayerIndexInTable);

            LiveRevengeMappingEntry* lpEntry = FindMappingEntry(lNetworkPlayerID);
            lpNetworkPlayer->RegisterMessageType(KI_LIVE_REVENGE_SYNC_MESSAGE_TYPE,
                                                 sizeof(LiveRevengeSyncMessage),
                                                 &lpEntry->mSendMessage, &lpEntry->mRecvMessage,
                                                 &LiveRevengeManager::_SyncMessageArrivedCallback,
                                                 &LiveRevengeManager::_SyncMessageDeliveredCallback,
                                                 this);
        }

        CGS_ASSERT(IsTableValid(), "IsTableValid()");
    }

    void LiveRevengeManager::RemovePlayer(NetworkPlayerID lPlayerID)
    {
        CgsNetwork::NetworkPlayer* lpNetworkPlayer = mpNetworkManager->GetPlayerManager()->GetPlayerByID(lPlayerID);
        if (lpNetworkPlayer)
        {
            // The mapping is looked up unchecked (a missing entry is not guarded on the console).
            RemoveRelationshipFromDebugMenu(FindMappingEntry(lPlayerID)->miRevengeTableIndex);
            RemoveMappingEntry(lPlayerID);
            lpNetworkPlayer->UnRegisterMessageType(KI_LIVE_REVENGE_SYNC_MESSAGE_TYPE);
        }
    }

    // A remote player finished joining: send them our side of the relationship.
    void LiveRevengeManager::RemotePlayerFinalised(NetworkPlayerID lNetworkPlayerID)
    {
        CgsNetwork::NetworkPlayer* lpNetworkPlayer =
            mpNetworkManager->GetPlayerManager()->GetPlayerByID(lNetworkPlayerID);
        CGS_ASSERT(lpNetworkPlayer, "lpNetworkPlayer");

        LiveRevengeSyncMessage* lpLiveRevengeSyncMessage = static_cast<LiveRevengeSyncMessage*>(
            lpNetworkPlayer->GetRegisteredSendMessage(KI_LIVE_REVENGE_SYNC_MESSAGE_TYPE));
        CGS_ASSERT(lpLiveRevengeSyncMessage, "lpLiveRevengeSyncMessage");

        const LiveRevengeRelationship* lpRevengeRelationship = GetRevengeRelationship(lNetworkPlayerID);
        CGS_ASSERT(lpRevengeRelationship, "lpRevengeRelationship");

        lpLiveRevengeSyncMessage->PrepareForSend(lpRevengeRelationship,
                                                 mpNetworkManager->GetTimeManager()->GetCurrentFrameNumber());
    }

    // ============================================================================
    // Relationship table
    // ============================================================================

    s32 LiveRevengeManager::FindPlayerInTableByName(const char* lpcName)
    {
        LiveRevengeProfile* lpProfile = GetProfile();
        CGS_ASSERT(lpProfile, "lpProfile");

        const s32 liNumberOfRivals = static_cast<s32>(lpProfile->maRelationshipTable.GetLength());
        for (s32 liRivalIndex = 0; liRivalIndex < liNumberOfRivals; ++liRivalIndex)
        {
            if (CgsNetwork::UsernameCompare(
                    lpProfile->maRelationshipTable[static_cast<u32>(liRivalIndex)].GetRivalName()->GetPlayerName(),
                    lpcName) == 0)
            {
                return liRivalIndex;
            }
        }
        return -1;
    }

    // Append a fresh relationship for the new rival, or, with the table full, overwrite the
    // row that changed longest ago.
    s32 LiveRevengeManager::AddNewTableEntry(const LiveRevengeRelationship::UniquePlayerID* lpUniqueID)
    {
        LiveRevengeRelationship lRelationship;
        LiveRevengeProfile* lpProfile = GetProfile();
        CGS_ASSERT(lpProfile, "lpProfile");
        CGS_ASSERT(lpUniqueID, "lpUniqueID");
        // UniquePlayerID::IsValid (inline): a non-empty name and a non-zero XUID.
        CGS_ASSERT(lpUniqueID->GetPlayerName()[0] != '\0' && lpUniqueID->mqXuid != 0, "lpUniqueID->IsValid()");

        lRelationship.Construct();
        lRelationship.Prepare(lpUniqueID);

        if (lpProfile->maRelationshipTable.GetLength() < static_cast<u32>(LiveRevengeProfile::KI_MAX_REVENGE_HISTORY))
        {
            lpProfile->maRelationshipTable.Append(lRelationship);
            return static_cast<s32>(lpProfile->maRelationshipTable.GetLength()) - 1;
        }

        CgsSystem::DateAndTime lOldestDateAndTime = lpProfile->maRelationshipTable[0].GetLastChangedTime();
        s32 liOldestIndex = 0;
        const s32 liNumberOfRivals = static_cast<s32>(lpProfile->maRelationshipTable.GetLength());
        for (s32 liRivalIndex = 1; liRivalIndex < liNumberOfRivals; ++liRivalIndex)
        {
            if (lpProfile->maRelationshipTable[static_cast<u32>(liRivalIndex)].GetLastChangedTime() < lOldestDateAndTime)
            {
                lOldestDateAndTime = lpProfile->maRelationshipTable[static_cast<u32>(liRivalIndex)].GetLastChangedTime();
                liOldestIndex      = liRivalIndex;
            }
        }

        lpProfile->maRelationshipTable[static_cast<u32>(liOldestIndex)] = lRelationship;
        return liOldestIndex;
    }

    s32 LiveRevengeManager::GetNumberOfRelationships()
    {
        LiveRevengeProfile* lpProfile = GetProfile();
        if (!lpProfile)
        {
            return 0;
        }
        return static_cast<s32>(lpProfile->maRelationshipTable.GetLength());
    }

    // A rival is a relationship with at least one takedown either way.
    s32 LiveRevengeManager::GetNumberOfRivals()
    {
        LiveRevengeProfile* lpProfile = GetProfile();
        CGS_ASSERT(lpProfile, "lpProfile");

        s32 liRivalCount = 0;
        const s32 liNumberOfRivals = static_cast<s32>(lpProfile->maRelationshipTable.GetLength());
        for (s32 liRivalIndex = 0; liRivalIndex < liNumberOfRivals; ++liRivalIndex)
        {
            if (lpProfile->maRelationshipTable[static_cast<u32>(liRivalIndex)].GetTotalTakedowns() > 0)
            {
                ++liRivalCount;
            }
        }
        return liRivalCount;
    }

    bool LiveRevengeManager::IsTableValid()
    {
        for (u32 luTableIndex = 0; luTableIndex < mpLiveRevengeProfile->maRelationshipTable.GetLength(); ++luTableIndex)
        {
            if (!mpLiveRevengeProfile->maRelationshipTable[luTableIndex].Validate())
            {
                return false;
            }
        }
        return true;
    }

    void LiveRevengeManager::AutoSaveLiveRevengeProfile()
    {
        mbProfileIsDirty = true;
    }

    // The game state loaded the profile into the buffer this manager owns: validate it, drop every
    // per-player mapping and the top-rival list, then rebuild the top rivals from the loaded table.
    void LiveRevengeManager::HandleLiveRevengeProfileLoadedEvent(const LiveRevengeProfile* lpLiveRevengeProfile)
    {
        CGS_ASSERT(lpLiveRevengeProfile, "lpLiveRevengeProfile");
        CGS_ASSERT(mpLiveRevengeProfile->ValidateProfile(), "mpLiveRevengeProfile->ValidateProfile()");
        CGS_ASSERT(IsTableValid(),
                   "If this fires then save/load has probably corrupted the profile. Deleting your save game will fix this for now (tell Alex Veal)\n");

        ResetRevengeTableMappings();
        ClearTopRivals();
        UpdateTopRivals();
        mbProfileIsDirty = false;
    }

    // ============================================================================
    // Per-player mapping
    // ============================================================================

    void LiveRevengeManager::ResetRevengeTableMappings()
    {
        for (s32 liIndex = 0; liIndex < KI_MAX_NETWORK_PLAYERS; ++liIndex)
        {
            maPlayerToTableIndexData[liIndex].mPlayerID = KI_INVALID_RACE_CAR_INDEX_TO_REVENGE_TABLE;
        }
    }

    LiveRevengeMappingEntry* LiveRevengeManager::FindMappingEntry(NetworkPlayerID lNetworkPlayerID)
    {
        for (s32 liIndex = 0; liIndex < KI_MAX_NETWORK_PLAYERS; ++liIndex)
        {
            if (maPlayerToTableIndexData[liIndex].mPlayerID == lNetworkPlayerID)
            {
                return &maPlayerToTableIndexData[liIndex];
            }
        }
        return nullptr;
    }

    // Claim a free slot for the player, construct its sync messages, and flag the row for
    // re-upload if it is one of the top rivals.
    void LiveRevengeManager::AddMappingEntry(NetworkPlayerID lNetworkPlayerID, s32 liRevengeTableIndex)
    {
        CGS_ASSERT(lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
                   "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

        LiveRevengeMappingEntry* lpEntry = FindMappingEntry(KI_INVALID_RACE_CAR_INDEX_TO_REVENGE_TABLE);
        if (!lpEntry)
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Could not find PlayerID " << lNetworkPlayerID << " in " << __FUNCTION__;
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
            return;
        }

        lpEntry->mPlayerID           = lNetworkPlayerID;
        lpEntry->miRevengeTableIndex = liRevengeTableIndex;
        lpEntry->mSendMessage.Construct();
        lpEntry->mRecvMessage.Construct();

        const s32 liTopRivalIndex = GetRivalTopIndex(liRevengeTableIndex);
        if (liTopRivalIndex != -1)
        {
            maDirtyTopRivals.SetBit(static_cast<u32>(liTopRivalIndex));
        }
    }

    void LiveRevengeManager::RemoveMappingEntry(NetworkPlayerID lNetworkPlayerID)
    {
        CGS_ASSERT(lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
                   "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

        LiveRevengeMappingEntry* lpEntry = FindMappingEntry(lNetworkPlayerID);
        if (!lpEntry)
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Could not find PlayerID " << lNetworkPlayerID << " in " << __FUNCTION__;
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
            return;
        }

        lpEntry->mPlayerID           = KI_INVALID_RACE_CAR_INDEX_TO_REVENGE_TABLE;
        lpEntry->miRevengeTableIndex = -1;
        lpEntry->mSendMessage.Release();
        lpEntry->mSendMessage.Destruct();
        lpEntry->mRecvMessage.Release();
        lpEntry->mRecvMessage.Destruct();
    }

    LiveRevengeRelationship* LiveRevengeManager::GetNonConstRevengeRelationship(NetworkPlayerID lNetworkPlayerID)
    {
        CGS_ASSERT(lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
                   "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

        const LiveRevengeMappingEntry* lpEntry = FindMappingEntry(lNetworkPlayerID);
        if (!lpEntry)
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Could not find PlayerID " << lNetworkPlayerID << " in " << __FUNCTION__;
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
            return nullptr;
        }

        CGS_ASSERT(lpEntry->miRevengeTableIndex >= 0, "lpEntry->miRevengeTableIndex >= 0");
        CGS_ASSERT(lpEntry->miRevengeTableIndex < LiveRevengeProfile::KI_MAX_REVENGE_HISTORY,
                   "lpEntry->miRevengeTableIndex < LiveRevengeProfile::KI_MAX_REVENGE_HISTORY");

        LiveRevengeProfile* lpProfile = GetProfile();
        CGS_ASSERT(lpProfile, "lpProfile");
        return &lpProfile->maRelationshipTable[static_cast<u32>(lpEntry->miRevengeTableIndex)];
    }

    EActiveRaceCarIndex LiveRevengeManager::NetworkPlayerIDToActiveRaceCarIndex(NetworkPlayerID lNetworkPlayerID)
    {
        CGS_ASSERT(mpNetworkManager, "mpNetworkManager");
        return mpNetworkModule->GetActiveRaceCarIndex(lNetworkPlayerID);
    }

    void LiveRevengeManager::AddRelationshipToDebugMenu(s32 liLiveRevengeTableIndex)
    {
        mDebugComponent.AddPlayer(&mpLiveRevengeProfile->maRelationshipTable[static_cast<u32>(liLiveRevengeTableIndex)]);
    }

    void LiveRevengeManager::RemoveRelationshipFromDebugMenu(s32 liLiveRevengeTableIndex)
    {
        mDebugComponent.RemovePlayer(&mpLiveRevengeProfile->maRelationshipTable[static_cast<u32>(liLiveRevengeTableIndex)]);
    }

    // ============================================================================
    // Takedowns, paybacks and the GUI
    // ============================================================================

    // A takedown between the local player and a mapped rival: update the relationship, show
    // the live-revenge message (not for marked-man takedowns) and report a changed rival count.
    void LiveRevengeManager::UpdateLiveRevengeRelationShip(EActiveRaceCarIndex leAggressorActiveRaceCarIndex,
                                                           EActiveRaceCarIndex leVictimActiveRaceCarIndex,
                                                           bool lbMarkedManTakedown,
                                                           BrnNetworkModuleIO::OutputBuffer* lpOutputBuffer)
    {
        const s32 liNumberOfRivals = GetNumberOfRivals();
        CGS_ASSERT(mpNetworkManager, "mpNetworkManager");
        CGS_ASSERT(lpOutputBuffer, "lpOutputBuffer");

        if (mpNetworkManager->GetPlayerManager()->IsLocalPlayer(mpNetworkModule->GetNetworkPlayerID(leAggressorActiveRaceCarIndex)))
        {
            LiveRevengeRelationship* lpRelationship =
                GetNonConstRevengeRelationship(mpNetworkModule->GetNetworkPlayerID(leVictimActiveRaceCarIndex));
            if (lpRelationship)
            {
                lpRelationship->AddTakedownByLocalPlayer(lbMarkedManTakedown);
            }
            if (!lbMarkedManTakedown)
            {
                DisplayPlayerTakedownMessage(lpOutputBuffer, leAggressorActiveRaceCarIndex, leVictimActiveRaceCarIndex);
            }
            AutoSaveLiveRevengeProfile();
        }
        else if (mpNetworkManager->GetPlayerManager()->IsLocalPlayer(mpNetworkModule->GetNetworkPlayerID(leVictimActiveRaceCarIndex)))
        {
            LiveRevengeRelationship* lpRelationship =
                GetNonConstRevengeRelationship(mpNetworkModule->GetNetworkPlayerID(leAggressorActiveRaceCarIndex));
            if (lpRelationship)
            {
                lpRelationship->AddTakedownByRival(lbMarkedManTakedown);
            }
            if (!lbMarkedManTakedown)
            {
                DisplayRivalTakedownMessage(lpOutputBuffer, leAggressorActiveRaceCarIndex, leVictimActiveRaceCarIndex);
            }
            AutoSaveLiveRevengeProfile();
        }

        if (GetNumberOfRivals() != liNumberOfRivals)
        {
            mpNetworkManager->GetStatsManager()->HandleNewNumberOfRivals();
        }
    }

    // A payback between the local player and a rival: an initialised payback counts as dealt,
    // a crashed (succeeded) one as scored, credited to whichever side dealt it.
    void LiveRevengeManager::UpdatePaybacksData(EActiveRaceCarIndex leAggressorActiveRaceCarIndex,
                                                EActiveRaceCarIndex leVictimActiveRaceCarIndex,
                                                EDirtyTrickStatus leDirtyTrickStatus)
    {
        CGS_ASSERT(leVictimActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                   "leVictimActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(leVictimActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leVictimActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
        CGS_ASSERT(leAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                   "leAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(leAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

        NetworkPlayerID lLocalPlayerNetworkID = CgsNetwork::K_INVALID_PLAYER_ID;
        CGS_ASSERT(mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lLocalPlayerNetworkID),
                   "mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID( &lLocalPlayerNetworkID )");

        const bool lbLocalPlayerWasPaybackVictim =
            mpNetworkModule->GetActiveRaceCarIndex(lLocalPlayerNetworkID) == leVictimActiveRaceCarIndex;
        const NetworkPlayerID lRivalPlayerID = lbLocalPlayerWasPaybackVictim
            ? mpNetworkModule->GetNetworkPlayerID(leAggressorActiveRaceCarIndex)
            : mpNetworkModule->GetNetworkPlayerID(leVictimActiveRaceCarIndex);
        CGS_ASSERT(lRivalPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
                   "lRivalPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

        LiveRevengeRelationship* lpRelationship = GetNonConstRevengeRelationship(lRivalPlayerID);
        CGS_ASSERT(lpRelationship, "lpRelationship");

        // The status enumerators are E_DIRTY_TRICK_STATUS_INITIALISED (2) and
        // E_DIRTY_TRICK_STATUS_CRASHED (4); the enum's home names only its zero value so far.
        switch (static_cast<s32>(leDirtyTrickStatus))
        {
        case 2:
            if (lbLocalPlayerWasPaybackVictim)
            {
                lpRelationship->AddPaybackDealtByRival();
            }
            else
            {
                lpRelationship->AddPaybackDealtByLocalPlayer();
            }
            AutoSaveLiveRevengeProfile();
            break;

        case 4:
            if (lbLocalPlayerWasPaybackVictim)
            {
                lpRelationship->AddPaybackScoredByRival();
            }
            else
            {
                lpRelationship->AddPaybackScoredByLocalPlayer();
            }
            AutoSaveLiveRevengeProfile();
            break;

        default:
            break;
        }
    }

    // Header inline on the console (see the class declaration).
    void LiveRevengeManager::HandlePaybackInitialisedEvent(const BrnNetworkModuleIO::NetworkInPaybackIntialised* lpPaybackEvent)
    {
        CGS_ASSERT(lpPaybackEvent, "lpPaybackEvent");
        UpdatePaybacksData(lpPaybackEvent->mePaybackAggressorIndex, lpPaybackEvent->mePaybackVictimIndex,
                           static_cast<EDirtyTrickStatus>(2));   // E_DIRTY_TRICK_STATUS_INITIALISED
    }

    // Header inline on the console (see the class declaration).
    void LiveRevengeManager::HandlePaybackSucceededEvent(const BrnNetworkModuleIO::NetworkInPaybackSucceeded* lpPaybackEvent)
    {
        CGS_ASSERT(lpPaybackEvent, "lpPaybackEvent");
        UpdatePaybacksData(lpPaybackEvent->mePaybackAggressorIndex, lpPaybackEvent->mePaybackVictimIndex,
                           static_cast<EDirtyTrickStatus>(4));   // E_DIRTY_TRICK_STATUS_CRASHED
    }

    // The local player took the rival down: classify the relationship change for the HUD
    // (a new rivalry, a reignited one, extending a lead, or settling the score).
    void LiveRevengeManager::DisplayPlayerTakedownMessage(BrnNetworkModuleIO::OutputBuffer* lpOutputBuffer,
                                                          EActiveRaceCarIndex leAggressorActiveRaceCarIndex,
                                                          EActiveRaceCarIndex leVictimActiveRaceCarIndex)
    {
        CGS_ASSERT(leVictimActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                   "leVictimActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(leVictimActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leVictimActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

        NetworkPlayerID lLocalPlayerID = CgsNetwork::K_INVALID_PLAYER_ID;
        mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lLocalPlayerID);
        const EActiveRaceCarIndex leLocalActiveRaceCarIndex = mpNetworkModule->GetActiveRaceCarIndex(lLocalPlayerID);
        CGS_ASSERT(leAggressorActiveRaceCarIndex == leLocalActiveRaceCarIndex,
                   "leAggressorActiveRaceCarIndex == leLocalActiveRaceCarIndex");

        const LiveRevengeRelationship* lpRelationship =
            GetRevengeRelationship(mpNetworkModule->GetNetworkPlayerID(leVictimActiveRaceCarIndex));
        if (!lpRelationship)
        {
            return;
        }

        NetworkToGuiLiveRevengeUpdate::LiveRevengeStatus leLiveRevengeStatus;
        if (lpRelationship->GetTotalTakedowns() == 1)
        {
            leLiveRevengeStatus = NetworkToGuiLiveRevengeUpdate::E_LIVEREVENGESTATUS_NEWRIVALRY;
            mpNetworkManager->CaptureTelemetryEvent(E_TELEMETRY_RIVAL_ADDED, GetNumberOfRivals());
        }
        else if (lpRelationship->GetCurrentScoreForLocalPlayer() == 1)
        {
            leLiveRevengeStatus = NetworkToGuiLiveRevengeUpdate::E_LIVEREVENGESTATUS_REIGNITED;
            mpNetworkManager->CaptureTelemetryEvent(E_TELEMETRY_RIVAL_ADDED, GetNumberOfRivals());
        }
        else if (lpRelationship->IsPlayerAheadInCurrentRelationship())
        {
            leLiveRevengeStatus = NetworkToGuiLiveRevengeUpdate::E_LIVEREVENGESTATUS_AHEAD;
        }
        else
        {
            CGS_ASSERT(lpRelationship->IsCurrentRelationshipEqual(), "lpRelationship->IsCurrentRelationshipEqual()");
            leLiveRevengeStatus = NetworkToGuiLiveRevengeUpdate::E_LIVEREVENGESTATUS_SETTLED;
            mpNetworkManager->CaptureTelemetryEvent(E_TELEMETRY_RIVAL_REMOVED, GetNumberOfRivals());
        }

        lpOutputBuffer->GetNetworkToGuiInterface()->AddLiveRevengeUpdate(
            leAggressorActiveRaceCarIndex, leVictimActiveRaceCarIndex, leLiveRevengeStatus,
            lpRelationship->GetCurrentScoreForLocalPlayer());
    }

    // The rival took the local player down: the mirror of DisplayPlayerTakedownMessage, scored
    // from the rival's side (falling behind instead of extending a lead).
    void LiveRevengeManager::DisplayRivalTakedownMessage(BrnNetworkModuleIO::OutputBuffer* lpOutputBuffer,
                                                         EActiveRaceCarIndex leAggressorActiveRaceCarIndex,
                                                         EActiveRaceCarIndex leVictimActiveRaceCarIndex)
    {
        CGS_ASSERT(lpOutputBuffer, "lpOutputBuffer");

        NetworkPlayerID lLocalPlayerID = CgsNetwork::K_INVALID_PLAYER_ID;
        mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lLocalPlayerID);
        const EActiveRaceCarIndex leLocalActiveRaceCarIndex = mpNetworkModule->GetActiveRaceCarIndex(lLocalPlayerID);
        CGS_ASSERT(leVictimActiveRaceCarIndex == leLocalActiveRaceCarIndex,
                   "leVictimActiveRaceCarIndex == leLocalActiveRaceCarIndex");
        CGS_ASSERT(leAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                   "leAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(leAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

        const LiveRevengeRelationship* lpRelationship =
            GetRevengeRelationship(mpNetworkModule->GetNetworkPlayerID(leAggressorActiveRaceCarIndex));
        if (!lpRelationship)
        {
            return;
        }

        NetworkToGuiLiveRevengeUpdate::LiveRevengeStatus leLiveRevengeStatus;
        if (lpRelationship->GetTotalTakedowns() == 1)
        {
            leLiveRevengeStatus = NetworkToGuiLiveRevengeUpdate::E_LIVEREVENGESTATUS_NEWRIVALRY;
            mpNetworkManager->CaptureTelemetryEvent(E_TELEMETRY_RIVAL_ADDED, GetNumberOfRivals());
        }
        else if (lpRelationship->GetCurrentScoreForRival() == 1)
        {
            leLiveRevengeStatus = NetworkToGuiLiveRevengeUpdate::E_LIVEREVENGESTATUS_REIGNITED;
            mpNetworkManager->CaptureTelemetryEvent(E_TELEMETRY_RIVAL_ADDED, GetNumberOfRivals());
        }
        else if (lpRelationship->IsRivalAheadInCurrentRelationship())
        {
            leLiveRevengeStatus = NetworkToGuiLiveRevengeUpdate::E_LIVEREVENGESTATUS_BEHIND;
        }
        else
        {
            CGS_ASSERT(lpRelationship->IsCurrentRelationshipEqual(), "lpRelationship->IsCurrentRelationshipEqual()");
            leLiveRevengeStatus = NetworkToGuiLiveRevengeUpdate::E_LIVEREVENGESTATUS_SETTLED;
            mpNetworkManager->CaptureTelemetryEvent(E_TELEMETRY_RIVAL_REMOVED, GetNumberOfRivals());
        }

        lpOutputBuffer->GetNetworkToGuiInterface()->AddLiveRevengeUpdate(
            leAggressorActiveRaceCarIndex, leVictimActiveRaceCarIndex, leLiveRevengeStatus,
            lpRelationship->GetCurrentScoreForRival());
    }

    // ============================================================================
    // Top rivals and the server upload
    // ============================================================================

    // qsort comparator: descending rival score.
    int LiveRevengeManager::_SortTopRivals(const void* lpRival1, const void* lpRival2)
    {
        const LiveRevengeQSortData* lpRivalDetails1 = static_cast<const LiveRevengeQSortData*>(lpRival1);
        const LiveRevengeQSortData* lpRivalDetails2 = static_cast<const LiveRevengeQSortData*>(lpRival2);

        if (lpRivalDetails2->miRivalScore > lpRivalDetails1->miRivalScore)
        {
            return 1;
        }
        if (lpRivalDetails2->miRivalScore < lpRivalDetails1->miRivalScore)
        {
            return -1;
        }
        return 0;
    }

    // Rank every relationship by total takedowns and keep the best ten; a slot whose row
    // changed is flagged for re-upload.
    void LiveRevengeManager::UpdateTopRivals()
    {
        LiveRevengeQSortData laRivals[LiveRevengeProfile::KI_MAX_REVENGE_HISTORY];

        LiveRevengeProfile* lpProfile = GetProfile();
        CGS_ASSERT(lpProfile, "lpProfile");

        const s32 liTotalRivals = static_cast<s32>(lpProfile->maRelationshipTable.GetLength());
        // rw::core::stdc::Min, spelled in place.
        miNumberOfTopRivals = (liTotalRivals < KI_NUMBER_OF_RIVALS_TO_STORE_ON_SERVER)
                                  ? liTotalRivals : KI_NUMBER_OF_RIVALS_TO_STORE_ON_SERVER;

        for (s32 liRivalIndex = 0; liRivalIndex < liTotalRivals; ++liRivalIndex)
        {
            laRivals[liRivalIndex].miTableIndex = liRivalIndex;
            laRivals[liRivalIndex].miRivalScore =
                lpProfile->maRelationshipTable[static_cast<u32>(liRivalIndex)].GetTotalTakedowns();
        }

        std::qsort(laRivals, static_cast<size_t>(liTotalRivals), sizeof(LiveRevengeQSortData),
                   &LiveRevengeManager::_SortTopRivals);

        for (s32 liRivalIndex = 0; liRivalIndex < miNumberOfTopRivals; ++liRivalIndex)
        {
            if (maTopIndexes[liRivalIndex] != laRivals[liRivalIndex].miTableIndex)
            {
                maTopIndexes[liRivalIndex] = laRivals[liRivalIndex].miTableIndex;
                maDirtyTopRivals.SetBit(static_cast<u32>(liRivalIndex));
            }
        }
    }

    // The top-ten slot holding table row liTableIndex, or -1.
    s32 LiveRevengeManager::GetRivalTopIndex(s32 liTableIndex) const
    {
        for (s32 liRivalIndex = 0; liRivalIndex < miNumberOfTopRivals; ++liRivalIndex)
        {
            if (maTopIndexes[liRivalIndex] == liTableIndex)
            {
                return liRivalIndex;
            }
        }
        return -1;
    }

    void LiveRevengeManager::ClearTopRivals()
    {
        for (s32 liRivalIndex = 0; liRivalIndex < KI_NUMBER_OF_RIVALS_TO_STORE_ON_SERVER; ++liRivalIndex)
        {
            maTopIndexes[liRivalIndex] = -1;
        }
        maDirtyTopRivals.UnSetAll();
    }

    // SendLiveRevengeRivalsToServer: declared only. Its upload record type has no usable home
    // yet: ServerInterfaceCustomCommands::UploadLiveRevengeData takes a forward-declared
    // BrnNetwork::RivalDataT that is never defined, while the record itself is
    // BrnNetwork::ServerGeneratedTypes::RivalDataT; the name copy also needs CgsCore::StrnCpy.

    // GetUniqueIDByName: declared only. It fills the identity through the unique-player-id
    // construct-from-player-params inline, which the identity's home header does not declare yet.

    // ============================================================================
    // Relationship sync messages
    // ============================================================================

    // A rival's copy of our relationship arrived (already flipped to our side): merge it.
    void LiveRevengeManager::SyncMessageArrivedCallback(LiveRevengeSyncMessage* lpMessage, NetworkPlayerID lRemotePlayerID)
    {
        LiveRevengeRelationship lRemoteRelationship;

        CGS_ASSERT(lpMessage, "lpMessage");
        CGS_ASSERT(lpMessage->Retrieve(&lRemoteRelationship), "lpMessage->Retrieve(&lRemoteRelationship)");

        LiveRevengeRelationship* lpLocalRelationship = GetNonConstRevengeRelationship(lRemotePlayerID);
        CGS_ASSERT(lpLocalRelationship, "lpLocalRelationship");

        lpLocalRelationship->Merge(&lRemoteRelationship);
        AutoSaveLiveRevengeProfile();
    }

    void LiveRevengeManager::_SyncMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                         NetworkPlayerID lSendingPlayerID, void* lpData)
    {
        LiveRevengeManager*     lpManager            = static_cast<LiveRevengeManager*>(lpData);
        LiveRevengeSyncMessage* lpLiveRevengeMessage = static_cast<LiveRevengeSyncMessage*>(lpMessage);
        CGS_ASSERT(lpManager, "lpManager");
        CGS_ASSERT(lpMessage, "lpMessage");

        lpManager->SyncMessageArrivedCallback(lpLiveRevengeMessage, lSendingPlayerID);
    }

    void LiveRevengeManager::_SyncMessageDeliveredCallback(bool /*lbSuccess*/, bool lbFakeNack,
                                                           CgsNetwork::SignalMessage* /*lpAck*/,
                                                           NetworkPlayerID /*lRecvingPlayerID*/, void* /*lpData*/)
    {
        if (lbFakeNack)
        {
            // FLAG: "WARNING: Fack Nack found in <function>\n" goes to the network log stream,
            // which has no home in this tree; the line is dropped.
        }
    }
} // namespace BrnNetwork
