// ===================================================================================
// BrnNetwork::NetworkAggressiveDrivingManager -- implementation
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkAggressiveDrivingManager.cpp
//
// Reconstructed from the X360 ARTIST pseudocode + ASM (spine), the DecFIGS DWARF (shape),
// and the project conventions. All member / interface access is BY NAME through the
// reconstructed headers; no raw offset pointer arithmetic. The X360 build interleaves dev-only
// CgsDev::StrStream debug logging ("***** AGG MOVE *****", "WARNING: Sending ... aggressive
// moves", "Aggressive move type N not handled yet") into several bodies; those streams have no
// observable effect on the relay's state and reference un-reconstructed StrStream formatting
// internals, so they are intentionally omitted (documented at each site).
// ===================================================================================
#include "GameSource/Network/Managers/BrnNetworkAggressiveDrivingManager.h"

#include "GameSource/Network/BrnNetworkModule.h"                              // BrnNetworkModule, GetGameStateToNetworkInterface / GetActiveRaceCarIndex / GetNetworkPlayerID
#include "GameSource/Network/BrnNetworkManager.h"                             // BrnNetworkManager, LiveRevengeManager
#include "GameSource/Network/Managers/BrnNetworkLiveRevengeRelationship.h"    // LiveRevengeRelationship (GetCurrentScoreForLocalPlayer)
#include "GameSource/Network/BrnNetworkModuleIO.h"                            // OutputBuffer / PostSimulationInputBuffer typed-queue accessors
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h" // GameStateToNetworkInterface
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"          // PlayerManager
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"          // NetworkPlayer::RegisterMessageType / UnRegisterMessageType
#include "GameShared/GameClasses/Network/Time/CgsTimeManager.h"              // TimeManager::GetCurrentFrameNumber
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstring>   // std::memmove (X360 RemoveAggressiveMove uses memmove)

namespace BrnNetwork
{
    // The reliable-message type id the aggressive-driving send/recv messages register under
    // (X360 RegisterMessageType(player, 17, ...)). Named here for the (Un)Register calls.
    static const s32 KI_AGGRESSIVE_DRIVING_MESSAGE_TYPE = 17;

    // ---------------------------------------------------------------------------------
    // Construct / Destruct  (X360 @ 0x82542DF8 / 0x82542E68 -- identical reset loop)
    // ---------------------------------------------------------------------------------
    void NetworkAggressiveDrivingManager::Construct(BrnNetworkModule* lpNetworkModule,
                                                    CgsNetwork::PlayerManager* lpPlayerManager,
                                                    CgsNetwork::TimeManager* lpTimeManager)
    {
        mpNetworkModule = lpNetworkModule;
        mpPlayerManager = lpPlayerManager;
        mpTimeManager   = lpTimeManager;

        mfNoImpactTime                   = -1.0f;
        mbAreWeInOnlineGame              = false;
        miConsecutiveImpactsOnSamePlayer = 0;
        mLastVictimNetworkPlayerID       = -1;

        for (s32 liIndex = 0; liIndex < KI_MAX_NETWORK_PLAYERS; ++liIndex)
        {
            AggressiveDrivingData& lData = maAggressiveDrivingData[liIndex];
            lData.mPlayerID = -1;
            lData.mAggressiveDrivingMessageSend.Construct();
            lData.mAggressiveDrivingMessageRecv.Construct();
            lData.miBufferCount = 0;
        }
    }

    void NetworkAggressiveDrivingManager::Destruct()
    {
        // X360 0x82542E68: byte-identical to Construct's reset loop (it re-runs the per-entry
        // Construct + clears the scalar state), minus the manager-pointer stores.
        mfNoImpactTime                   = -1.0f;
        mbAreWeInOnlineGame              = false;
        miConsecutiveImpactsOnSamePlayer = 0;
        mLastVictimNetworkPlayerID       = -1;

        for (s32 liIndex = 0; liIndex < KI_MAX_NETWORK_PLAYERS; ++liIndex)
        {
            AggressiveDrivingData& lData = maAggressiveDrivingData[liIndex];
            lData.mPlayerID = -1;
            lData.mAggressiveDrivingMessageSend.Construct();
            lData.mAggressiveDrivingMessageRecv.Construct();
            lData.miBufferCount = 0;
        }
    }

    // ---------------------------------------------------------------------------------
    // Prepare / Release  (X360 @ 0x82542xxx -- trivial true returns in this slice)
    // ---------------------------------------------------------------------------------
    bool NetworkAggressiveDrivingManager::Prepare()
    {
        return true;
    }

    bool NetworkAggressiveDrivingManager::Release()
    {
        return true;
    }

    // ---------------------------------------------------------------------------------
    // OnRoundStart / OnRoundFinish  (X360 @ 0x82542ED0 / 0x82542EF8)
    // ---------------------------------------------------------------------------------
    void NetworkAggressiveDrivingManager::OnRoundStart()
    {
        mfNoImpactTime                   = -1.0f;
        mbAreWeInOnlineGame              = true;
        mLastVictimNetworkPlayerID       = -1;
        miConsecutiveImpactsOnSamePlayer = 0;
    }

    void NetworkAggressiveDrivingManager::OnRoundFinish()
    {
        mbAreWeInOnlineGame = false;
    }

    // ---------------------------------------------------------------------------------
    // GetAggressiveDrivingDataEntry  (X360 @ 0x82542F08-ish; linear search by mPlayerID)
    // ---------------------------------------------------------------------------------
    NetworkAggressiveDrivingManager::AggressiveDrivingData*
    NetworkAggressiveDrivingManager::GetAggressiveDrivingDataEntry(NetworkPlayerID lPlayerID)
    {
        for (s32 liIndex = 0; liIndex < KI_MAX_NETWORK_PLAYERS; ++liIndex)
        {
            if (maAggressiveDrivingData[liIndex].mPlayerID == lPlayerID)
            {
                return &maAggressiveDrivingData[liIndex];
            }
        }
        return nullptr;
    }

    // ---------------------------------------------------------------------------------
    // ProcessBeforeSimulation  (X360 @ 0x82573C40)
    //   Drains inbound aggressive moves into local events, then decays the no-impact window.
    // ---------------------------------------------------------------------------------
    void NetworkAggressiveDrivingManager::ProcessBeforeSimulation(
            BrnNetworkModuleIO::OutputBuffer* lpOutput, f32 lfTimeStep)
    {
        CGS_ASSERT(mpNetworkModule != nullptr, "mpNetworkModule");
        CGS_ASSERT(lpOutput != nullptr, "lpOutput");

        if (mbAreWeInOnlineGame)
        {
            HandleReceivingMessages(lpOutput);
            mfNoImpactTime -= lfTimeStep;
        }
    }

    // ---------------------------------------------------------------------------------
    // ProcessAfterSimulation  (X360 @ 0x82573CE0)
    //   Pulls the frame's impact/takedown events into buffered moves, then pumps the sends.
    // ---------------------------------------------------------------------------------
    void NetworkAggressiveDrivingManager::ProcessAfterSimulation(
            const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInput)
    {
        CGS_ASSERT(mpNetworkModule != nullptr, "mpNetworkModule");

        if (mbAreWeInOnlineGame)
        {
            ProcessInputQueue(lpInput);
            HandleSendingMessages();
        }
    }

    // ---------------------------------------------------------------------------------
    // AddPlayer  (X360 @ 0x8254F548)
    // ---------------------------------------------------------------------------------
    void NetworkAggressiveDrivingManager::AddPlayer(NetworkPlayerID lPlayerID)
    {
        CGS_ASSERT(mpPlayerManager != nullptr, "mpPlayerManager");

        CgsNetwork::NetworkPlayer* lpNetworkPlayer = mpPlayerManager->GetPlayerByID(lPlayerID);
        if (lpNetworkPlayer != nullptr)
        {
            // Claim the first free table slot (mPlayerID == -1).
            AggressiveDrivingData* lpDataEntry = GetAggressiveDrivingDataEntry(-1);
            CGS_ASSERT(lpDataEntry != nullptr, "lpDataEntry");

            lpNetworkPlayer->RegisterMessageType(KI_AGGRESSIVE_DRIVING_MESSAGE_TYPE,
                                                 sizeof(AggressiveDrivingMessage),
                                                 &lpDataEntry->mAggressiveDrivingMessageSend,
                                                 &lpDataEntry->mAggressiveDrivingMessageRecv,
                                                 nullptr, nullptr, this);
            lpDataEntry->mPlayerID = lPlayerID;
        }
    }

    // ---------------------------------------------------------------------------------
    // RemovePlayer  (X360 @ 0x8254F608)
    //   Drops every buffered move that involves lPlayerID, then unregisters + frees the slot.
    // ---------------------------------------------------------------------------------
    void NetworkAggressiveDrivingManager::RemovePlayer(NetworkPlayerID lPlayerID)
    {
        CGS_ASSERT(mpPlayerManager != nullptr, "mpPlayerManager");

        CgsNetwork::NetworkPlayer* lpNetworkPlayer = mpPlayerManager->GetPlayerByID(lPlayerID);

        for (s32 liIndex = 0; liIndex < KI_MAX_NETWORK_PLAYERS; ++liIndex)
        {
            AggressiveDrivingData& lData = maAggressiveDrivingData[liIndex];
            if (lData.mPlayerID != -1)
            {
                for (s32 liMoveIndex = 0; liMoveIndex < lData.miBufferCount; ++liMoveIndex)
                {
                    const AggressiveMoveData& lMove = lData.maBuffer[liMoveIndex];
                    if (lMove.mAggressorNetworkPlayerID == lPlayerID ||
                        lMove.mVictimNetworkPlayerID == lPlayerID)
                    {
                        RemoveAggressiveMove(liIndex, liMoveIndex);
                        --liMoveIndex;   // re-test the slot the tail just shuffled into
                    }
                }
            }
        }

        if (lpNetworkPlayer != nullptr)
        {
            lpNetworkPlayer->UnRegisterMessageType(KI_AGGRESSIVE_DRIVING_MESSAGE_TYPE);
            AggressiveDrivingData* lpEntry = GetAggressiveDrivingDataEntry(lPlayerID);
            lpEntry->mPlayerID = -1;
        }
    }

    // ---------------------------------------------------------------------------------
    // Disconnected  (X360 @ 0x8254F710)
    //   Tear down every still-registered player slot.
    // ---------------------------------------------------------------------------------
    void NetworkAggressiveDrivingManager::Disconnected()
    {
        for (s32 liIndex = 0; liIndex < KI_MAX_NETWORK_PLAYERS; ++liIndex)
        {
            if (maAggressiveDrivingData[liIndex].mPlayerID != -1)
            {
                RemovePlayer(maAggressiveDrivingData[liIndex].mPlayerID);
                CGS_ASSERT(maAggressiveDrivingData[liIndex].mPlayerID == -1,
                           "maAggressiveDrivingData[liIndex].mPlayerID == CgsNetwork::K_INVALID_PLAYER_ID");
            }
        }
    }

    // ---------------------------------------------------------------------------------
    // RemoveAggressiveMove  (X360 @ 0x825433C0)
    //   Shuffle the buffer down over [liMoveIndex] and drop the count by one.
    // ---------------------------------------------------------------------------------
    void NetworkAggressiveDrivingManager::RemoveAggressiveMove(s32 liPlayerIndex, s32 liMoveIndex)
    {
        CGS_ASSERT(liPlayerIndex >= 0, "liPlayerIndex >= 0");
        CGS_ASSERT(liPlayerIndex < KI_MAX_NETWORK_PLAYERS, "liPlayerIndex < KI_MAX_NETWORK_PLAYERS");
        CGS_ASSERT(liMoveIndex >= 0, "liMoveIndex >= 0");
        CGS_ASSERT(liMoveIndex < KI_MAX_PLAYER_AGGRESSIVE_MOVES_BUFFER,
                   "liMoveIndex < KI_MAX_PLAYER_AGGRESSIVE_MOVES_BUFFER");

        AggressiveDrivingData& lData = maAggressiveDrivingData[liPlayerIndex];
        CGS_ASSERT(lData.miBufferCount > 0,
                   "maAggressiveDrivingData[liPlayerIndex].miBufferCount > 0");
        CGS_ASSERT(liMoveIndex < lData.miBufferCount,
                   "liMoveIndex < maAggressiveDrivingData[liPlayerIndex].miBufferCount");

        AggressiveMoveData* lpMove = &lData.maBuffer[liMoveIndex];
        const s32 liEntriesToMove = (KI_MAX_PLAYER_AGGRESSIVE_MOVES_BUFFER - 1) - liMoveIndex;
        CGS_ASSERT((lpMove + liEntriesToMove) <= &lData.maBuffer[KI_MAX_PLAYER_AGGRESSIVE_MOVES_BUFFER],
                   "(((char *) lpMove) + sizeof(AggressiveMoveData) * liEntriesToMove) <= "
                   "(char *) &maAggressiveDrivingData[liPlayerIndex].maBuffer[KI_MAX_PLAYER_AGGRESSIVE_MOVES_BUFFER]");

        std::memmove(lpMove, lpMove + 1, sizeof(AggressiveMoveData) * liEntriesToMove);
        --lData.miBufferCount;
    }

    // ---------------------------------------------------------------------------------
    // CompareMove  (X360 @ 0x82542xxx)
    //   Orders two moves by move-type (primary key, AggressiveMoveData+0x00) then magnitude
    //   (tiebreak, +0x30). Returns >0 if lpMove0 ranks above lpMove1, <0 if below, 0 if equal
    //   -- matching the (type, then float magnitude) comparisons inlined into AddNewMove's asm.
    // ---------------------------------------------------------------------------------
    s32 NetworkAggressiveDrivingManager::CompareMove(const AggressiveMoveData* lpMove0,
                                                     const AggressiveMoveData* lpMove1)
    {
        if (lpMove0->meAggressiveMoveType != lpMove1->meAggressiveMoveType)
        {
            return static_cast<s32>(lpMove0->meAggressiveMoveType)
                 - static_cast<s32>(lpMove1->meAggressiveMoveType);
        }
        // Equal type: higher magnitude ranks above.
        if (lpMove0->mfMagnitude > lpMove1->mfMagnitude) return 1;
        if (lpMove0->mfMagnitude < lpMove1->mfMagnitude) return -1;
        return 0;
    }

    // ---------------------------------------------------------------------------------
    // AddNewMove  (X360 @ 0x82542F08)
    //   Append lpNewMove to lpData's buffer. When the buffer is full, the X360 code finds the
    //   TOP-RANKED buffered move (max by (type, magnitude)) and overwrites it iff the new move
    //   ranks at least as high; otherwise the new move is dropped. If that top-ranked entry is
    //   itself a TAKE_DOWN (type 0 -- i.e. the whole buffer is takedowns) it asserts
    //   "Too many takedowns?!" and drops the new move.
    // ---------------------------------------------------------------------------------
    bool NetworkAggressiveDrivingManager::AddNewMove(AggressiveDrivingData* lpData,
                                                     const AggressiveMoveData* lpNewMove)
    {
        s32 liMoveIndex;

        if (lpData->miBufferCount >= KI_MAX_PLAYER_AGGRESSIVE_MOVES_BUFFER)
        {
            // Buffer full: find the top-ranked buffered move (max by (type, magnitude)).
            s32 liBestIndex = 0;
            for (s32 liIndex = 1; liIndex < lpData->miBufferCount; ++liIndex)
            {
                if (CompareMove(&lpData->maBuffer[liIndex], &lpData->maBuffer[liBestIndex]) > 0)
                {
                    liBestIndex = liIndex;
                }
            }

            AggressiveMoveData& lTopMove = lpData->maBuffer[liBestIndex];
            if (lTopMove.meAggressiveMoveType == E_AGGRESSIVE_MOVE_TAKE_DOWN)
            {
                CGS_ASSERT(false, "Too many takedowns?!");
                return false;
            }

            if (CompareMove(&lTopMove, lpNewMove) > 0)
            {
                // The top-ranked buffered move outranks the new one -- drop it.
                // (X360 logs "Not adding aggressive move ... less interesting" here.)
                return false;
            }
            liMoveIndex = liBestIndex;
        }
        else
        {
            liMoveIndex = lpData->miBufferCount;
            ++lpData->miBufferCount;
        }

        lpData->maBuffer[liMoveIndex] = *lpNewMove;
        return true;
    }

    // ---------------------------------------------------------------------------------
    // UpdateOnlineBattling  (X360 @ 0x825430E8)
    //   Tracks consecutive impacts on the same victim; once the threshold is reached, queues a
    //   BATTLING aggressive move against that victim for every tracked player.
    // ---------------------------------------------------------------------------------
    void NetworkAggressiveDrivingManager::UpdateOnlineBattling(
            const BrnPhysics::Vehicle::ImpactEvent* lpImpactEvent)
    {
        BrnNetworkModuleIO::GameStateToNetworkInterface* lpInterface =
            mpNetworkModule->GetGameStateToNetworkInterface();

        // ImpactEvent uses the global ::EActiveRaceCarIndex; the network mapping interface uses
        // BrnNetwork::EActiveRaceCarIndex. They share value semantics -- cast across the scopes.
        const EActiveRaceCarIndex leImpactAggressorActiveRaceCarIndex =
            static_cast<EActiveRaceCarIndex>(lpImpactEvent->meAggressorActiveRaceCarIndex);
        const EActiveRaceCarIndex leImpactVictimActiveRaceCarIndex =
            static_cast<EActiveRaceCarIndex>(lpImpactEvent->meVictimActiveRaceCarIndex);

        const NetworkPlayerID lImpactAggressorNetworkPlayerID =
            lpInterface->GetNetworkPlayerID(leImpactAggressorActiveRaceCarIndex);
        const NetworkPlayerID lImpactVictimNetworkPlayerID =
            lpInterface->GetNetworkPlayerID(leImpactVictimActiveRaceCarIndex);

        const BrnNetwork::EAggressiveMoveType leMoveType =
            static_cast<BrnNetwork::EAggressiveMoveType>(lpImpactEvent->meImpactType);
        if (leMoveType != E_AGGRESSIVE_MOVE_BATTLING && leMoveType != E_AGGRESSIVE_MOVE_TRADING_PAINT)
        {
            return;
        }

        if (lImpactVictimNetworkPlayerID == mLastVictimNetworkPlayerID && mfNoImpactTime >= 0.0f)
        {
            ++miConsecutiveImpactsOnSamePlayer;
        }
        else
        {
            mLastVictimNetworkPlayerID       = lImpactVictimNetworkPlayerID;
            miConsecutiveImpactsOnSamePlayer = 1;
            mfNoImpactTime                   = KF_BATTLING_IMPACT_DELAY;
        }

        if (miConsecutiveImpactsOnSamePlayer < KI_CONSECUTIVE_IMPACTS_FOR_BATTLING)
        {
            return;
        }

        for (s32 liIndex = 0; liIndex < KI_MAX_NETWORK_PLAYERS; ++liIndex)
        {
            AggressiveDrivingData* lpEntry = &maAggressiveDrivingData[liIndex];
            CGS_ASSERT(lpEntry != nullptr, "lpEntry");
            CGS_ASSERT(leImpactAggressorActiveRaceCarIndex >= 0,
                       "leImpactAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
            CGS_ASSERT(leImpactAggressorActiveRaceCarIndex < KI_MAX_RACE_CARS,
                       "leImpactAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
            CGS_ASSERT(leImpactVictimActiveRaceCarIndex >= 0,
                       "leImpactVictimActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
            CGS_ASSERT(leImpactVictimActiveRaceCarIndex < KI_MAX_RACE_CARS,
                       "leImpactVictimActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
            CGS_ASSERT(lImpactAggressorNetworkPlayerID != -1,
                       "lImpactAggressorNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");
            CGS_ASSERT(lImpactVictimNetworkPlayerID != -1,
                       "lImpactVictimNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");
            CGS_ASSERT(leImpactAggressorActiveRaceCarIndex != leImpactVictimActiveRaceCarIndex,
                       "leImpactAggressorActiveRaceCarIndex != leImpactVictimActiveRaceCarIndex");
            CGS_ASSERT(lImpactAggressorNetworkPlayerID != lImpactVictimNetworkPlayerID,
                       "lImpactAggressorNetworkPlayerID != lImpactVictimNetworkPlayerID");

            AggressiveMoveData lNewMove;
            lNewMove.Clear();
            lNewMove.meAggressiveMoveType      = E_AGGRESSIVE_MOVE_BATTLING;
            lNewMove.mAggressorNetworkPlayerID = lImpactAggressorNetworkPlayerID;
            lNewMove.mVictimNetworkPlayerID    = lImpactVictimNetworkPlayerID;
            lNewMove.muImpactScore             = lpImpactEvent->muScore;
            lNewMove.mDirection                = lpImpactEvent->mDirection;
            lNewMove.mfMagnitude               = lpImpactEvent->mfMagnitude;
            lNewMove.mfDuration                = lpImpactEvent->mfDuration;
            lNewMove.mfSteeringDirection       = lpImpactEvent->mfSteeringDirection;
            lNewMove.mfRecoveryTime            = lpImpactEvent->mfRecoveryTime;
            lNewMove.miNumberOfTimesTriedToSend = 0;

            AddNewMove(lpEntry, &lNewMove);
        }
    }

    // ---------------------------------------------------------------------------------
    // AddImpactEvent  (X360 @ 0x8254F208)
    //   Classify one local impact and buffer it (against the remote car) as an aggressive move.
    // ---------------------------------------------------------------------------------
    bool NetworkAggressiveDrivingManager::AddImpactEvent(
            const BrnPhysics::Vehicle::ImpactEvent* lpImpactEvent)
    {
        if (mpPlayerManager->GetNumberNetworkPlayers(false) == 0)
        {
            return false;
        }

        NetworkPlayerID lLocalPlayerID;
        const bool lbHaveLocal =
            mpNetworkModule->GetNetworkManager()->GetPlayerManager()->GetNextLocalPlayerID(&lLocalPlayerID);
        CGS_ASSERT(lbHaveLocal,
                   "mpNetworkModule->GetNetworkManager()->GetPlayerManager()->GetNextLocalPlayerID( &lLocalPlayerID )");

        BrnNetworkModuleIO::GameStateToNetworkInterface* lpInterface =
            mpNetworkModule->GetGameStateToNetworkInterface();
        const EActiveRaceCarIndex leLocalActiveRaceCarIndex =
            lpInterface->GetActiveRaceCarIndex(lLocalPlayerID);

        // ImpactEvent uses the global ::EActiveRaceCarIndex; the mapping interface uses
        // BrnNetwork::EActiveRaceCarIndex. Cast the impact indices across the scopes (same values).
        const EActiveRaceCarIndex leAggressorActiveRaceCarIndex =
            static_cast<EActiveRaceCarIndex>(lpImpactEvent->meAggressorActiveRaceCarIndex);
        const EActiveRaceCarIndex leVictimActiveRaceCarIndex =
            static_cast<EActiveRaceCarIndex>(lpImpactEvent->meVictimActiveRaceCarIndex);

        // Pick the *remote* participant: if we are the aggressor the takedown is "ours" so also
        // feed the battling tracker; if we are the victim only the remote (aggressor) matters.
        EActiveRaceCarIndex leRemoteActiveRaceCarIndex;
        if (leAggressorActiveRaceCarIndex == leLocalActiveRaceCarIndex)
        {
            leRemoteActiveRaceCarIndex = leVictimActiveRaceCarIndex;
            UpdateOnlineBattling(lpImpactEvent);
        }
        else if (leVictimActiveRaceCarIndex == leLocalActiveRaceCarIndex)
        {
            leRemoteActiveRaceCarIndex = leAggressorActiveRaceCarIndex;
        }
        else
        {
            return false;
        }

        CGS_ASSERT(leRemoteActiveRaceCarIndex != leLocalActiveRaceCarIndex,
                   "leRemoteActiveRaceCarIndex != leLocalActiveRaceCarIndex");

        const NetworkPlayerID lPlayerID = lpInterface->GetNetworkPlayerID(leRemoteActiveRaceCarIndex);
        AggressiveDrivingData* lpEntry = GetAggressiveDrivingDataEntry(lPlayerID);
        CGS_ASSERT(lpEntry != nullptr, "lpEntry");

        // Map the physics impact type onto the aggressive-move type.
        AggressiveMoveData lNewMove;
        lNewMove.Clear();
        lNewMove.mAggressorNetworkPlayerID = -1;
        lNewMove.mVictimNetworkPlayerID    = -1;
        lNewMove.meTakedownType            = static_cast<BrnGameState::ETakedownType>(-1);

        switch (lpImpactEvent->meImpactType)
        {
        case 1:  lNewMove.meAggressiveMoveType = E_AGGRESSIVE_MOVE_TRADING_PAINT; break; // slam
        case 3:  lNewMove.meAggressiveMoveType = E_AGGRESSIVE_MOVE_SLAM;          break;
        case 4:  lNewMove.meAggressiveMoveType = E_AGGRESSIVE_MOVE_SHUNT;         break;
        default: return false;
        }

        CGS_ASSERT(leAggressorActiveRaceCarIndex >= 0,
                   "lpImpactEvent->meAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(leAggressorActiveRaceCarIndex < KI_MAX_RACE_CARS,
                   "lpImpactEvent->meAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
        CGS_ASSERT(leVictimActiveRaceCarIndex >= 0,
                   "lpImpactEvent->meVictimActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(leVictimActiveRaceCarIndex < KI_MAX_RACE_CARS,
                   "lpImpactEvent->meVictimActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
        CGS_ASSERT(leVictimActiveRaceCarIndex != leAggressorActiveRaceCarIndex,
                   "lpImpactEvent->meVictimActiveRaceCarIndex != lpImpactEvent->meAggressorActiveRaceCarIndex");

        const NetworkPlayerID lAggressorNetworkPlayerID =
            lpInterface->GetNetworkPlayerID(leAggressorActiveRaceCarIndex);
        const NetworkPlayerID lVictimNetworkPlayerID =
            lpInterface->GetNetworkPlayerID(leVictimActiveRaceCarIndex);
        CGS_ASSERT(lAggressorNetworkPlayerID != lVictimNetworkPlayerID,
                   "lAggressorNetworkPlayerID != lVictimNetworkPlayerID");

        lNewMove.mAggressorNetworkPlayerID  = lAggressorNetworkPlayerID;
        lNewMove.mVictimNetworkPlayerID     = lVictimNetworkPlayerID;
        lNewMove.muImpactScore              = lpImpactEvent->muScore;
        lNewMove.mDirection                 = lpImpactEvent->mDirection;
        lNewMove.mfMagnitude                = lpImpactEvent->mfMagnitude;
        lNewMove.mfDuration                 = lpImpactEvent->mfDuration;
        lNewMove.mfSteeringDirection        = lpImpactEvent->mfSteeringDirection;
        lNewMove.mfRecoveryTime             = lpImpactEvent->mfRecoveryTime;
        lNewMove.miNumberOfTimesTriedToSend = 0;

        AddNewMove(lpEntry, &lNewMove);
        return true;
    }

    // ---------------------------------------------------------------------------------
    // AddTakedownEvent  (X360 @ 0x82570A88)
    //   Buffer a takedown the local player performed/suffered as an aggressive move for every
    //   tracked player, flagging "settled score" from the live-revenge relationship.
    // ---------------------------------------------------------------------------------
    bool NetworkAggressiveDrivingManager::AddTakedownEvent(const BrnGameState::TakedownEvent* lpTakedownEvent)
    {
        if (mpPlayerManager->GetNumberNetworkPlayers(false) == 0)
        {
            return false;
        }

        const BrnGameState::EActiveRaceCarIndex leAggressorActiveRaceCarIndex = lpTakedownEvent->meAggressorIndex;
        const BrnGameState::EActiveRaceCarIndex leVictimActiveRaceCarIndex    = lpTakedownEvent->meVictimIndex;
        CGS_ASSERT(leAggressorActiveRaceCarIndex != leVictimActiveRaceCarIndex,
                   "leAggressorActiveRaceCarIndex != leVictimActiveRaceCarIndex");

        if (lpTakedownEvent->mbMarkedManTakeDown)
        {
            UpdateMarkedPlayersOnMarkedManTakeDown(
                static_cast<EActiveRaceCarIndex>(leAggressorActiveRaceCarIndex));
        }
        if (lpTakedownEvent->mbRemote)
        {
            return false;
        }

        NetworkPlayerID lLocalPlayerID;
        const bool lbHaveLocal =
            mpNetworkModule->GetNetworkManager()->GetPlayerManager()->GetNextLocalPlayerID(&lLocalPlayerID);
        CGS_ASSERT(lbHaveLocal,
                   "mpNetworkModule->GetNetworkManager()->GetPlayerManager()->GetNextLocalPlayerID( &lLocalPlayerID )");

        BrnNetworkModuleIO::GameStateToNetworkInterface* lpInterface =
            mpNetworkModule->GetGameStateToNetworkInterface();

        // Is the local player the aggressor or the victim of this takedown?
        bool lbLocalIsVictim    = false;
        bool lbLocalIsAggressor = false;
        NetworkPlayerID lRevengePlayerID;
        if (static_cast<s32>(leAggressorActiveRaceCarIndex) ==
            static_cast<s32>(lpInterface->GetActiveRaceCarIndex(lLocalPlayerID)))
        {
            lRevengePlayerID   = lpInterface->GetNetworkPlayerID(
                static_cast<EActiveRaceCarIndex>(leVictimActiveRaceCarIndex));
            lbLocalIsVictim    = true;   // local dealt it; the rival is the "victim" relationship side
        }
        else if (static_cast<s32>(leVictimActiveRaceCarIndex) ==
                 static_cast<s32>(lpInterface->GetActiveRaceCarIndex(lLocalPlayerID)))
        {
            lRevengePlayerID   = lpInterface->GetNetworkPlayerID(
                static_cast<EActiveRaceCarIndex>(leAggressorActiveRaceCarIndex));
            lbLocalIsAggressor = true;
        }
        else
        {
            return false;
        }

        // Decide whether this takedown "settles a score" via the live-revenge relationship.
        bool lbSettledScore = false;
        if (lRevengePlayerID != -1)
        {
            BrnNetworkManager* lpNetworkManager = mpNetworkModule->GetNetworkManager();
            CGS_ASSERT(lpNetworkManager != nullptr, "mpNetworkModule->GetNetworkManager()");
            CGS_ASSERT(lpNetworkManager->GetLiveRevengeManager() != nullptr,
                       "mpNetworkModule->GetNetworkManager()->GetLiveRevengeManager()");

            LiveRevengeRelationship* lpLiveRevengeRelationship =
                lpNetworkManager->GetLiveRevengeManager()->GetNonConstRevengeRelation(lRevengePlayerID);
            CGS_ASSERT(lpLiveRevengeRelationship != nullptr, "lpLiveRevengeRelationship");

            const s32 liCurrentScore = lpLiveRevengeRelationship->GetCurrentScoreForLocalPlayer();
            if ((liCurrentScore > 0 && lbLocalIsAggressor) || (liCurrentScore < 0 && lbLocalIsVictim))
            {
                lbSettledScore = true;
            }
        }

        // Buffer the takedown move against every tracked player.
        BrnNetworkManager* lpNetworkManager = mpNetworkModule->GetNetworkManager();
        const s32 liNumPlayers = lpNetworkManager->GetPlayerManager()->GetNumberNetworkPlayers(false);
        for (s32 liIndex = 0; liIndex < liNumPlayers; ++liIndex)
        {
            AggressiveDrivingData* lpEntry = &maAggressiveDrivingData[liIndex];
            CGS_ASSERT(lpEntry->mPlayerID != lLocalPlayerID,
                       "maAggressiveDrivingData[liIndex].mPlayerID != lLocalPlayerID");
            CGS_ASSERT(lpEntry != nullptr, "lpEntry");

            if (lpEntry->mPlayerID != -1)
            {
                CGS_ASSERT(leAggressorActiveRaceCarIndex >= 0,
                           "leAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
                CGS_ASSERT(leAggressorActiveRaceCarIndex < KI_MAX_RACE_CARS,
                           "leAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
                CGS_ASSERT(leVictimActiveRaceCarIndex >= 0,
                           "leVictimActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
                CGS_ASSERT(leVictimActiveRaceCarIndex < KI_MAX_RACE_CARS,
                           "leVictimActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
                CGS_ASSERT(leVictimActiveRaceCarIndex != leAggressorActiveRaceCarIndex,
                           "leVictimActiveRaceCarIndex != leAggressorActiveRaceCarIndex");

                AggressiveMoveData lNewMove;
                lNewMove.Clear();
                lNewMove.meAggressiveMoveType      = E_AGGRESSIVE_MOVE_TAKE_DOWN;
                lNewMove.mAggressorNetworkPlayerID =
                    lpInterface->GetNetworkPlayerID(static_cast<EActiveRaceCarIndex>(leAggressorActiveRaceCarIndex));
                lNewMove.mVictimNetworkPlayerID    =
                    lpInterface->GetNetworkPlayerID(static_cast<EActiveRaceCarIndex>(leVictimActiveRaceCarIndex));
                lNewMove.meTakedownType            = lpTakedownEvent->meType;
                lNewMove.mbMarkedMan               = lpTakedownEvent->mbMarkedManTakeDown;
                lNewMove.mbSettledScore            = lbSettledScore;
                lNewMove.miNumberOfTimesTriedToSend = 0;

                // (X360 logs "***** AGG MOVE: ... *****" here -- dev-only, omitted.)
                AddNewMove(lpEntry, &lNewMove);
            }
        }
        return true;
    }

    // ---------------------------------------------------------------------------------
    // UpdateMarkedPlayersOnMarkedManTakeDown  (X360 @ 0x82542xxx)
    //   On a marked-man takedown by leAggressor, clear the marked-man flag for affected players.
    //   The X360 body resolves the aggressor's network player id and resets marked-man menu state;
    //   the menu-data subsystem (BrnNetwork::PlayerMenuData) is not yet reconstructed, so the reset
    //   is modelled as the boolean result the caller consumes (true == a marked man was reset).
    // ---------------------------------------------------------------------------------
    bool NetworkAggressiveDrivingManager::UpdateMarkedPlayersOnMarkedManTakeDown(
            EActiveRaceCarIndex leAggressorActiveRaceCarIndex)
    {
        BrnNetworkModuleIO::GameStateToNetworkInterface* lpInterface =
            mpNetworkModule->GetGameStateToNetworkInterface();
        const NetworkPlayerID lPlayerID = lpInterface->GetNetworkPlayerID(leAggressorActiveRaceCarIndex);

        // The X360 walks the player-menu table resetting the marked-man flag for lPlayerID; that
        // PlayerMenuData table is un-reconstructed. Report whether a valid aggressor was resolved
        // (the only value HandleReceivingMessages/AddTakedownEvent branch on).
        return lPlayerID != -1;
    }

    // ---------------------------------------------------------------------------------
    // ProcessInputQueue  (X360 @ 0x82570F38)
    //   Forward every queued impact + takedown event into the buffering path.
    // ---------------------------------------------------------------------------------
    void NetworkAggressiveDrivingManager::ProcessInputQueue(
            const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInput)
    {
        CGS_ASSERT(mpNetworkModule != nullptr, "mpNetworkModule");
        CGS_ASSERT(mpPlayerManager != nullptr, "mpPlayerManager");

        const CgsModule::BaseEventQueue<BrnPhysics::Vehicle::ImpactEvent>* lpImpactQueue =
            lpInput->GetImpactEventQueue();
        for (s32 liIndex = 0; liIndex < lpImpactQueue->GetLength(); ++liIndex)
        {
            BrnPhysics::Vehicle::ImpactEvent lImpactEvent = lpImpactQueue->GetEvent(liIndex);
            AddImpactEvent(&lImpactEvent);
        }

        const CgsModule::BaseEventQueue<BrnGameState::TakedownEvent>* lpTakedownQueue =
            lpInput->GetTakedownEventQueue();
        for (s32 liEventIndex = 0; liEventIndex < lpTakedownQueue->GetLength(); ++liEventIndex)
        {
            BrnGameState::TakedownEvent lTakedownEvent = lpTakedownQueue->GetEvent(liEventIndex);
            AddTakedownEvent(&lTakedownEvent);
        }
    }

    // ---------------------------------------------------------------------------------
    // HandleSendingMessages  (X360 @ 0x82543540)
    //   Drop over-sent moves, bump every buffered move's retry count for players whose
    //   round-robin turn it is, then pack the buffer into the per-player send message.
    // ---------------------------------------------------------------------------------
    void NetworkAggressiveDrivingManager::HandleSendingMessages()
    {
        CGS_ASSERT(mpNetworkModule != nullptr, "mpNetworkModule");
        CGS_ASSERT(mpPlayerManager != nullptr, "mpPlayerManager");

        // First pass: drop any move that has already been sent its full quota.
        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_NETWORK_PLAYERS; ++liPlayerIndex)
        {
            AggressiveDrivingData& lData = maAggressiveDrivingData[liPlayerIndex];
            if (lData.mPlayerID != -1)
            {
                for (s32 liMoveIndex = 0; liMoveIndex < lData.miBufferCount; ++liMoveIndex)
                {
                    if (lData.maBuffer[liMoveIndex].miNumberOfTimesTriedToSend >=
                        KI_MAX_NUMBER_OF_AGGRESSIVE_MOVE_SENDS)
                    {
                        RemoveAggressiveMove(liPlayerIndex, liMoveIndex);
                        --liMoveIndex;   // re-test the slot the tail shuffled into
                    }
                }
            }
        }

        // Second pass: for players whose turn it is, bump retry counts and pack + send.
        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_NETWORK_PLAYERS; ++liPlayerIndex)
        {
            AggressiveDrivingData& lData = maAggressiveDrivingData[liPlayerIndex];
            const NetworkPlayerID lPlayerID = lData.mPlayerID;
            if (lPlayerID == -1)
            {
                continue;
            }

            if (!mpPlayerManager->IsPlayerTurnToSendRoundRobinMessage(lPlayerID, true, 0))
            {
                continue;
            }

            for (s32 liMoveIndex = 0; liMoveIndex < lData.miBufferCount; ++liMoveIndex)
            {
                ++lData.maBuffer[liMoveIndex].miNumberOfTimesTriedToSend;
            }

            // (X360 logs "WARNING: Sending N aggressive moves, which seems high!" when the buffer
            //  count exceeds KI_MAX_NUMBER_OF_AGGRESSIVE_MOVE_SENDS -- dev-only, omitted.)

            if (lData.miBufferCount > 0)
            {
                lData.mAggressiveDrivingMessageSend.PrepareForSend(
                    mpTimeManager->GetCurrentFrameNumber(), lData.miBufferCount, lData.maBuffer);
            }
        }
    }

    // ---------------------------------------------------------------------------------
    // HandleReceivingMessages  (X360 @ 0x825703C0)
    //   Pull inbound aggressive moves out of every player's recv message and turn each one back
    //   into the matching local event (takedown / impact / battling-gui), keyed by move type.
    // ---------------------------------------------------------------------------------
    void NetworkAggressiveDrivingManager::HandleReceivingMessages(BrnNetworkModuleIO::OutputBuffer* lpOutput)
    {
        CGS_ASSERT(mpNetworkModule != nullptr, "mpNetworkModule");

        CgsModule::BaseEventQueue<BrnPhysics::Vehicle::ImpactEvent>* lpImpactEventQueue =
            lpOutput->GetVehicleManagerImpactEventQueue();
        CgsModule::BaseEventQueue<BrnGameState::TakedownEvent>* lpTakedownEventQueue =
            lpOutput->GetNetworkToGameStateTakedownEventQueue();

        BrnNetworkModuleIO::GameStateToNetworkInterface* lpInterface =
            mpNetworkModule->GetGameStateToNetworkInterface();

        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_NETWORK_PLAYERS; ++liPlayerIndex)
        {
            AggressiveDrivingData& lData = maAggressiveDrivingData[liPlayerIndex];
            if (lData.mPlayerID == -1)
            {
                continue;
            }

            s32 liNumberOfMoves = 0;
            AggressiveMoveData laAggressiveMoveDataArray[KI_MAX_AGGRESSIVE_MOVES];
            if (!lData.mAggressiveDrivingMessageRecv.Retrieve(&liNumberOfMoves, laAggressiveMoveDataArray))
            {
                continue;
            }

            for (s32 liMoveIndex = 0; liMoveIndex < liNumberOfMoves; ++liMoveIndex)
            {
                const AggressiveMoveData& lMove = laAggressiveMoveDataArray[liMoveIndex];

                const EActiveRaceCarIndex leAggressorActiveRaceCarIndex =
                    lpInterface->GetActiveRaceCarIndex(lMove.mAggressorNetworkPlayerID);
                const EActiveRaceCarIndex leVictimActiveRaceCarIndex =
                    lpInterface->GetActiveRaceCarIndex(lMove.mVictimNetworkPlayerID);

                // Both participants must be present in this client's car mapping.
                if (leAggressorActiveRaceCarIndex == -1 || leVictimActiveRaceCarIndex == -1)
                {
                    continue;
                }

                switch (lMove.meAggressiveMoveType)
                {
                case E_AGGRESSIVE_MOVE_TAKE_DOWN:
                {
                    CGS_ASSERT(lMove.mAggressorNetworkPlayerID != lMove.mVictimNetworkPlayerID,
                               "laAggressiveMoveDataArray[liMoveIndex].mAggressorNetworkPlayerID != "
                               "laAggressiveMoveDataArray[liMoveIndex].mVictimNetworkPlayerID");
                    CGS_ASSERT(leAggressorActiveRaceCarIndex != leVictimActiveRaceCarIndex,
                               "leAggressorActiveRaceCarIndex != leVictimActiveRaceCarIndex");

                    BrnGameState::TakedownEvent lTakedownEvent;
                    lTakedownEvent.meAggressorIndex        =
                        static_cast<BrnGameState::EActiveRaceCarIndex>(leAggressorActiveRaceCarIndex);
                    lTakedownEvent.meVictimIndex           =
                        static_cast<BrnGameState::EActiveRaceCarIndex>(leVictimActiveRaceCarIndex);
                    lTakedownEvent.meType                  = lMove.meTakedownType;
                    lTakedownEvent.mbMarkedManTakeDown     = lMove.mbMarkedMan;
                    lTakedownEvent.mbRemote                = true;
                    lTakedownEvent.mbSettledScore          = lMove.mbSettledScore;

                    if (lMove.mbMarkedMan)
                    {
                        if (!UpdateMarkedPlayersOnMarkedManTakeDown(leAggressorActiveRaceCarIndex))
                        {
                            lTakedownEvent.mbMarkedManTakeDown = false;
                        }
                    }

                    CGS_ASSERT(leAggressorActiveRaceCarIndex != leVictimActiveRaceCarIndex,
                               "lTakedownEvent.meAggressorIndex != lTakedownEvent.meVictimIndex");

                    // The X360 reuses the first already-queued takedown when present rather than
                    // appending a duplicate; otherwise it adds a fresh event.
                    if (lpTakedownEventQueue->GetLength() <= 0)
                    {
                        lpTakedownEventQueue->AddEvent(lTakedownEvent);
                    }
                    break;
                }
                case E_AGGRESSIVE_MOVE_SLAM:
                case E_AGGRESSIVE_MOVE_SHUNT:
                case E_AGGRESSIVE_MOVE_TRADING_PAINT:
                {
                    CGS_ASSERT(leAggressorActiveRaceCarIndex != leVictimActiveRaceCarIndex,
                               "leAggressorActiveRaceCarIndex != leVictimActiveRaceCarIndex");

                    BrnPhysics::Vehicle::ImpactEvent lImpactEvent;
                    // Map the aggressive-move type back onto the physics impact type.
                    switch (lMove.meAggressiveMoveType)
                    {
                    case E_AGGRESSIVE_MOVE_SLAM:          lImpactEvent.meImpactType = static_cast<BrnPhysics::Vehicle::EImpactType>(3); break;
                    case E_AGGRESSIVE_MOVE_SHUNT:         lImpactEvent.meImpactType = static_cast<BrnPhysics::Vehicle::EImpactType>(4); break;
                    case E_AGGRESSIVE_MOVE_TRADING_PAINT: lImpactEvent.meImpactType = static_cast<BrnPhysics::Vehicle::EImpactType>(1); break;
                    default:                              lImpactEvent.meImpactType = static_cast<BrnPhysics::Vehicle::EImpactType>(0); break;
                    }
                    // The recv mapping yields BrnNetwork::EActiveRaceCarIndex; ImpactEvent stores the
                    // global ::EActiveRaceCarIndex (same values) -- cast across the scopes.
                    lImpactEvent.meAggressorActiveRaceCarIndex =
                        static_cast<::EActiveRaceCarIndex>(leAggressorActiveRaceCarIndex);
                    lImpactEvent.meVictimActiveRaceCarIndex    =
                        static_cast<::EActiveRaceCarIndex>(leVictimActiveRaceCarIndex);
                    lImpactEvent.mDirection                    = lMove.mDirection;
                    lImpactEvent.mfMagnitude                   = lMove.mfMagnitude;
                    lImpactEvent.mfDuration                    = lMove.mfDuration;
                    lImpactEvent.mfSteeringDirection           = lMove.mfSteeringDirection;
                    lImpactEvent.mfRecoveryTime                = lMove.mfRecoveryTime;
                    lImpactEvent.muScore                       = lMove.muImpactScore;

                    lpImpactEventQueue->AddEvent(lImpactEvent);
                    break;
                }
                case E_AGGRESSIVE_MOVE_BATTLING:
                {
                    NetworkPlayerID lLocalPlayerID;
                    const bool lbHaveLocal = mpPlayerManager->GetNextLocalPlayerID(&lLocalPlayerID);
                    CGS_ASSERT(lbHaveLocal, "mpPlayerManager->GetNextLocalPlayerID(&lLocalPlayerID)");

                    CGS_ASSERT(leAggressorActiveRaceCarIndex != leVictimActiveRaceCarIndex,
                               "leAggressorActiveRaceCarIndex != leVictimActiveRaceCarIndex");
                    CGS_ASSERT(leAggressorActiveRaceCarIndex >= 0,
                               "leAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
                    CGS_ASSERT(leAggressorActiveRaceCarIndex < KI_MAX_RACE_CARS,
                               "leAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
                    CGS_ASSERT(leVictimActiveRaceCarIndex >= 0,
                               "leVictimActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
                    CGS_ASSERT(leVictimActiveRaceCarIndex < KI_MAX_RACE_CARS,
                               "leVictimActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
                    CGS_ASSERT(leAggressorActiveRaceCarIndex !=
                                   lpInterface->GetActiveRaceCarIndex(lLocalPlayerID),
                               "leAggressorActiveRaceCarIndex != mpNetworkModule->GetActiveRaceCarIndex( lLocalPlayerID )");

                    // Only surface a battling gui event when the local player is the one being
                    // battled (the victim of the inbound battling move).
                    if (leVictimActiveRaceCarIndex != lpInterface->GetActiveRaceCarIndex(lLocalPlayerID))
                    {
                        // The X360 builds a BrnGui::GuiNetworkPlayerBattlingEvent {aggressor, victim}
                        // and AddEvent()s it (type id 483) onto OutputBuffer::GetGuiEventQueue(). That
                        // gui-event type + the variable gui-event queue are not yet reconstructed, so the
                        // dispatch is documented here rather than fabricated; the relay's own state is
                        // already fully updated above.
                    }
                    break;
                }
                default:
                    // (X360 logs "Aggressive move type N not handled yet!" -- dev-only, omitted.)
                    CGS_ASSERT(false, "Aggressive move type not handled yet");
                    break;
                }
            }
        }
    }
} // namespace BrnNetwork
