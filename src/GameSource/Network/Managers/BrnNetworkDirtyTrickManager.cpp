// ===================================================================================
// BrnNetwork::NetworkDirtyTrickManager::GetDirtyTrickDataEntry
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkDirtyTrickManager.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   BrnNetwork::NetworkDirtyTrickManager::Get @ 0x825486A8
//     (the DWARF name is GetDirtyTrickDataEntry, h:126; called by AddPlayer/RemovePlayer)
//
// Linear-search the fixed 7-slot table for the entry whose mPlayerID matches the search
// key. The X360 body walks the records at a 0x6C (108) byte stride, comparing each
// record's first word (mPlayerID) against lNetworkPlayerID; on a match it forms the
// entry pointer (index * 0x6C + base) and -- when non-null -- returns it. On no match
// (or a null pointer) it fires the "lpEntry" assert and returns the (null) result.
//
// Store/branch-for-branch match to the asm:
//   lpEntry = 0; for (i=0; i<7; ++i) { if (maDirtyTrickData[i].mPlayerID == key) {
//       lpEntry = &maDirtyTrickData[i]; break; } }
//   if (lpEntry == NULL) ASSERT("lpEntry"); return lpEntry;
// The asm's "if (lpEntry) return" / fall-through-to-assert is exactly the lpEntry==NULL
// guarded assert; the matched-entry address is never null in practice, but the guard is
// reproduced faithfully (the assert fires only on the no-match fall-through).
//
// The rest of the manager: the lifecycle, the per-peer message registration, the per-frame
// relay of the game's dirty-trick events to every peer and the delivery of a peer's dirty
// trick back onto the network-to-game-state queue.

#include "GameSource/Network/Managers/BrnNetworkDirtyTrickManager.h"

#include "GameShared/GameClasses/Development/Log/CgsLog.h"                   // CgsDev::Log::gpDebugPrint (nack warning)
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"         // RegisterMessageType / GetRegisteredSendMessage
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"         // GetPlayerByID / GetNextPlayerID
#include "GameShared/GameClasses/Network/Time/CgsTimeManager.h"              // GetU16FrameCount
#include "GameSource/Network/BrnNetworkModule.h"                              // mapping lookups, IO interfaces
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h" // DirtyTrickQueue

namespace BrnNetwork
{
    // The reliable-message type id the dirty-trick send/recv messages register under.
    static const s32 KI_DIRTY_TRICK_MESSAGE_TYPE = 18;

    NetworkDirtyTrickManager::DirtyTrickData*
    NetworkDirtyTrickManager::GetDirtyTrickDataEntry(NetworkPlayerID lNetworkPlayerID)
    {
        DirtyTrickData* lpEntry = nullptr;

        for (s32 liIndex = 0; liIndex < KI_MAX_DIRTY_TRICK_PLAYERS; ++liIndex)
        {
            if (maDirtyTrickData[liIndex].mPlayerID == lNetworkPlayerID)
            {
                lpEntry = &maDirtyTrickData[liIndex];
                break;
            }
        }

        CGS_ASSERT(lpEntry != nullptr, "lpEntry");
        return lpEntry;
    }

    // Store the three collaborators and free every slot, constructing both of its messages.
    void NetworkDirtyTrickManager::Construct(BrnNetworkModule* lpNetworkModule,
                                             CgsNetwork::PlayerManager* lpPlayerManager,
                                             CgsNetwork::TimeManager* lpTimeManager)
    {
        mpNetworkModule = lpNetworkModule;
        mpPlayerManager = lpPlayerManager;
        mpTimeManager   = lpTimeManager;

        for (s32 liIndex = 0; liIndex < KI_MAX_DIRTY_TRICK_PLAYERS; ++liIndex)
        {
            maDirtyTrickData[liIndex].mPlayerID = -1;
            maDirtyTrickData[liIndex].mDirtyTrickMessageSend.Construct();
            maDirtyTrickData[liIndex].mDirtyTrickMessageRecv.Construct();
        }
    }

    // Nothing to acquire or free (both fold onto the shared `return true` body).
    bool NetworkDirtyTrickManager::Prepare()
    {
        return true;
    }

    bool NetworkDirtyTrickManager::Release()
    {
        return true;
    }

    // Free every slot again, re-constructing both of its messages.
    void NetworkDirtyTrickManager::Destruct()
    {
        for (s32 liIndex = 0; liIndex < KI_MAX_DIRTY_TRICK_PLAYERS; ++liIndex)
        {
            maDirtyTrickData[liIndex].mPlayerID = -1;
            maDirtyTrickData[liIndex].mDirtyTrickMessageSend.Construct();
            maDirtyTrickData[liIndex].mDirtyTrickMessageRecv.Construct();
        }
    }

    // The pre-simulation tick has nothing to publish (the console body is the shared empty one).
    void NetworkDirtyTrickManager::ProcessBeforeSimulation(BrnNetworkModuleIO::OutputBuffer* /*lpOutputBuffer*/)
    {
    }

    // Relay this frame's dirty-trick events from the game to every peer.
    void NetworkDirtyTrickManager::ProcessAfterSimulation(const BrnNetworkModuleIO::PostSimulationInputBuffer* /*lpInputBuffer*/)
    {
        CGS_ASSERT(mpNetworkModule, "mpNetworkModule");
        ProcessDirtyTrickEvents();
    }

    // Claim a free slot for the joining peer and register its send/recv messages with it.
    void NetworkDirtyTrickManager::AddPlayer(NetworkPlayerID lNetworkPlayerID)
    {
        CGS_ASSERT(mpPlayerManager, "mpPlayerManager");

        CgsNetwork::NetworkPlayer* lpNetworkPlayer = mpPlayerManager->GetPlayerByID(lNetworkPlayerID);
        if (lpNetworkPlayer != nullptr)
        {
            DirtyTrickData* lpDataEntry = GetDirtyTrickDataEntry(-1);
            CGS_ASSERT(lpDataEntry, "lpDataEntry");

            lpNetworkPlayer->RegisterMessageType(KI_DIRTY_TRICK_MESSAGE_TYPE,
                                                 sizeof(DirtyTrickMessage),
                                                 &lpDataEntry->mDirtyTrickMessageSend,
                                                 &lpDataEntry->mDirtyTrickMessageRecv,
                                                 &NetworkDirtyTrickManager::_DirtyTrickMessageArrivedCallback,
                                                 &NetworkDirtyTrickManager::_DirtyTrickDeliveredCallback,
                                                 this);
            lpDataEntry->mPlayerID = lNetworkPlayerID;
        }
    }

    // Unregister the leaving peer's messages and free its slot.
    void NetworkDirtyTrickManager::RemovePlayer(NetworkPlayerID lNetworkPlayerID)
    {
        CGS_ASSERT(mpPlayerManager, "mpPlayerManager");

        CgsNetwork::NetworkPlayer* lpNetworkPlayer = mpPlayerManager->GetPlayerByID(lNetworkPlayerID);
        if (lpNetworkPlayer != nullptr)
        {
            lpNetworkPlayer->UnRegisterMessageType(KI_DIRTY_TRICK_MESSAGE_TYPE);
            GetDirtyTrickDataEntry(lNetworkPlayerID)->mPlayerID = -1;
        }
    }

    // Tear down every still-registered slot.
    void NetworkDirtyTrickManager::Disconnected()
    {
        for (s32 liIndex = 0; liIndex < KI_MAX_DIRTY_TRICK_PLAYERS; ++liIndex)
        {
            if (maDirtyTrickData[liIndex].mPlayerID != -1)
            {
                RemovePlayer(maDirtyTrickData[liIndex].mPlayerID);
                CGS_ASSERT(maDirtyTrickData[liIndex].mPlayerID == -1,
                           "maDirtyTrickData[liIndex].mPlayerID == CgsNetwork::K_INVALID_PLAYER_ID");
            }
        }
    }

    // Send every game-side dirty-trick event that carries a status to the peers.
    void NetworkDirtyTrickManager::ProcessDirtyTrickEvents()
    {
        const BrnNetworkModuleIO::GameStateToNetworkInterface::DirtyTrickQueue* lpQueue =
            mpNetworkModule->GetGameStateToNetworkInterface()->GetDirtyTrickQueue();

        for (s32 liIndex = 0; liIndex < lpQueue->GetLength(); ++liIndex)
        {
            const BrnNetworkModuleIO::DirtyTrickEvent lEvent = lpQueue->GetEvent(liIndex);
            if (lEvent.meDirtyTrickStatus != E_DIRTY_TRICK_NONE)
            {
                SendDirtyTrickMessage(lEvent.meAggressorActiveRaceCarIndex, lEvent.meVictimActiveRaceCarIndex,
                                      static_cast<u8>(lEvent.meDirtyTrickType),
                                      static_cast<u8>(lEvent.meDirtyTrickStatus));
            }
        }
    }

    // Arm the dirty-trick send message of every finalised peer with this trick.
    void NetworkDirtyTrickManager::SendDirtyTrickMessage(EActiveRaceCarIndex leAggressor, EActiveRaceCarIndex leVictim,
                                                         u8 lu8DirtyTrickType, u8 lu8DirtyTrickStatus)
    {
        CGS_ASSERT(mpPlayerManager, "mpPlayerManager");

        NetworkPlayerID lPlayerID = -1;
        while (mpPlayerManager->GetNextPlayerID(&lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
        {
            CgsNetwork::NetworkPlayer* lpNetworkPlayer = mpPlayerManager->GetPlayerByID(lPlayerID);
            if (lpNetworkPlayer != nullptr)
            {
                DirtyTrickMessage* lpDirtyTrickMessage = static_cast<DirtyTrickMessage*>(
                    lpNetworkPlayer->GetRegisteredSendMessage(KI_DIRTY_TRICK_MESSAGE_TYPE));
                CGS_ASSERT(lpDirtyTrickMessage, "lpDirtyTrickMessage");

                const NetworkPlayerID lAggressorPlayerID = mpNetworkModule->GetNetworkPlayerID(leAggressor);
                const NetworkPlayerID lVictimPlayerID    = mpNetworkModule->GetNetworkPlayerID(leVictim);

                CGS_ASSERT(!lpDirtyTrickMessage->IsMessageValid(), "!lpDirtyTrickMessage->IsMessageValid()");

                lpDirtyTrickMessage->PrepareForSend(mpTimeManager->GetU16FrameCount(), lAggressorPlayerID,
                                                    lVictimPlayerID, lu8DirtyTrickType, lu8DirtyTrickStatus);
            }
        }
    }

    // A peer's dirty trick arrived: hand it to the game as active-race-car indices.
    void NetworkDirtyTrickManager::ReceiveDirtyTrickMessage(NetworkPlayerID /*lNetworkPlayerID*/,
                                                            DirtyTrickMessage* lpDirtyTrickMessageRecv)
    {
        CGS_ASSERT(mpNetworkModule, "mpNetworkModule");
        CGS_ASSERT(lpDirtyTrickMessageRecv, "lpDirtyTrickMessageRecv");

        NetworkPlayerID lAggressorPlayerID;
        NetworkPlayerID lVictimPlayerID;
        u8              lu8DirtyTrickType;
        u8              lu8DirtyTrickStatus;
        lpDirtyTrickMessageRecv->Retrieve(&lAggressorPlayerID, &lVictimPlayerID, &lu8DirtyTrickType,
                                          &lu8DirtyTrickStatus);

        BrnNetworkModuleIO::DirtyTrickEvent lEvent;
        lEvent.meAggressorActiveRaceCarIndex = mpNetworkModule->GetActiveRaceCarIndex(lAggressorPlayerID);
        lEvent.meVictimActiveRaceCarIndex    = mpNetworkModule->GetActiveRaceCarIndex(lVictimPlayerID);
        lEvent.meDirtyTrickType              = static_cast<EPaybackType>(lu8DirtyTrickType);
        lEvent.meDirtyTrickStatus            = static_cast<EDirtyTrickStatus>(lu8DirtyTrickStatus);
        mpNetworkModule->GetNetworkToGameStateInterface()->GetDirtyTrickQueue()->AddEvent(lEvent);
    }

    void NetworkDirtyTrickManager::_DirtyTrickMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                                     NetworkPlayerID lNetworkPlayerID, void* lpContext)
    {
        NetworkDirtyTrickManager* lpDirtyTrickManager = static_cast<NetworkDirtyTrickManager*>(lpContext);
        CGS_ASSERT(lpDirtyTrickManager, "lpDirtyTrickManager");

        DirtyTrickMessage* lpDirtyTrickMessage = static_cast<DirtyTrickMessage*>(lpMessage);
        CGS_ASSERT(lpDirtyTrickMessage, "lpDirtyTrickMessage");

        lpDirtyTrickManager->ReceiveDirtyTrickMessage(lNetworkPlayerID, lpDirtyTrickMessage);
    }

    void NetworkDirtyTrickManager::_DirtyTrickDeliveredCallback(bool /*lbDelivered*/, bool lbFakeNack,
                                                                CgsNetwork::SignalMessage* /*lpMessage*/,
                                                                NetworkPlayerID /*lNetworkPlayerID*/, void* /*lpContext*/)
    {
        if (lbFakeNack)
        {
            *CgsDev::Log::gpDebugPrint << "WARNING: Fack Nack found in ";
            *CgsDev::Log::gpDebugPrint << "BrnNetwork::NetworkDirtyTrickManager::_DirtyTrickDeliveredCallback";
            *CgsDev::Log::gpDebugPrint << "\n";
        }
    }
} // namespace BrnNetwork
