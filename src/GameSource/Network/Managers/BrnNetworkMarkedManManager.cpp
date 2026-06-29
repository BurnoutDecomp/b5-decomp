// ===================================================================================
// BrnNetwork::MarkedManManager -- definition home
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkMarkedManManager.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (addresses noted per function). The manager
// keeps a fixed 7-slot table of remote players; each slot owns the send/recv MarkedManMessage
// pair registered with that player's CgsNetwork::NetworkPlayer, plus the running tally of how
// many peers have finalised their marked-man choice.
//
// Source-of-truth ladder: signatures / side-effects / branches come from the ASM; the
// declaration shape (member names/types, the 7-slot table, the nested DataEntry) from the
// DecFIGS DWARF. The original asserts streamed formatted text into the assert buffer via
// CgsDev::StrStream; per project convention they are reproduced with CGS_ASSERT + a plain
// message string (the macro supplies __FILE__/__LINE__).
//
// Six declared methods have NO standalone X360 symbol (fully inlined into their callers, or
// debug-log-only): GetDataEntry is reconstructed from the inlined linear searches in
// AddPlayer/RemovePlayer (and matches the sibling NetworkDirtyTrickManager::GetDirtyTrickDataEntry
// idiom); Update / ResetRemotePlayers / MarkingFinished have no recovered body and no inlined
// evidence beyond their DWARF local-variable hints, so they are left as faithful minimal bodies
// (documented at each site); _MarkedManMessageDeliveredCallback's DWARF body is purely a debug
// StrStream log with no state side-effects, so the reconstructed callback is a no-op.
// ===================================================================================

#include "GameSource/Network/Managers/BrnNetworkMarkedManManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"   // NetworkPlayer, PlayerMenuData
#include "GameSource/Network/BrnNetworkManager.h"
#include "GameSource/Network/BrnNetworkPlayerMenuData.h"               // BrnNetwork::PlayerMenuData (downcast target)

namespace BrnNetwork
{
    // The free-slot / no-player sentinel is the shared CgsNetwork player-id sentinel (-1).
    using CgsNetwork::KI_INVALID_PLAYER_ID;

    // BrnNetwork::MarkedManManager::Construct  @ 0x825539F0
    void MarkedManManager::Construct(BrnNetworkManager* lpNetworkManager)
    {
        CGS_ASSERT(lpNetworkManager != nullptr, "lpNetworkManager");

        mpNetworkManager = lpNetworkManager;

        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_MARKED_MAN_PLAYERS; ++liPlayerIndex)
        {
            maPlayers[liPlayerIndex].mPlayerID = KI_INVALID_PLAYER_ID;
            maPlayers[liPlayerIndex].mMarkedManMessageSend.Construct();
            maPlayers[liPlayerIndex].mMarkedManMessageRecv.Construct();
        }

        miNumberOfPlayersFinishedMarking = 0;
    }

    // BrnNetwork::MarkedManManager::Prepare  @ 0x82553A70
    bool MarkedManManager::Prepare()
    {
        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_MARKED_MAN_PLAYERS; ++liPlayerIndex)
        {
            maPlayers[liPlayerIndex].mPlayerID = KI_INVALID_PLAYER_ID;
            maPlayers[liPlayerIndex].mMarkedManMessageSend.Construct();
            maPlayers[liPlayerIndex].mMarkedManMessageRecv.Construct();
        }

        miNumberOfPlayersFinishedMarking = 0;
        return true;
    }

    // BrnNetwork::MarkedManManager::Release  @ 0x82553AC8
    bool MarkedManManager::Release()
    {
        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_MARKED_MAN_PLAYERS; ++liPlayerIndex)
        {
            maPlayers[liPlayerIndex].mPlayerID = KI_INVALID_PLAYER_ID;
            maPlayers[liPlayerIndex].mMarkedManMessageSend.Construct();
            maPlayers[liPlayerIndex].mMarkedManMessageRecv.Construct();
        }

        miNumberOfPlayersFinishedMarking = 0;
        return true;
    }

    // BrnNetwork::MarkedManManager::Destruct  @ 0x82553B20
    void MarkedManManager::Destruct()
    {
        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_MARKED_MAN_PLAYERS; ++liPlayerIndex)
        {
            maPlayers[liPlayerIndex].mPlayerID = KI_INVALID_PLAYER_ID;
            maPlayers[liPlayerIndex].mMarkedManMessageSend.Construct();
            maPlayers[liPlayerIndex].mMarkedManMessageRecv.Construct();
        }

        mpNetworkManager = nullptr;
        miNumberOfPlayersFinishedMarking = 0;
    }

    // BrnNetwork::MarkedManManager::Update  @ BrnNetworkMarkedManManager.cpp:150
    // No standalone X360 symbol survives (inlined to nothing); the DWARF hint lists no locals
    // and no side effects, so the per-frame tick is empty in this build.
    void MarkedManManager::Update()
    {
    }

    // BrnNetwork::MarkedManManager::OnRoundStart  @ 0x82548D18
    void MarkedManManager::OnRoundStart()
    {
        miNumberOfPlayersFinishedMarking = 0;
    }

    // BrnNetwork::MarkedManManager::Disconnected  @ 0x8255C3D0
    void MarkedManManager::Disconnected()
    {
        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_MARKED_MAN_PLAYERS; ++liPlayerIndex)
        {
            if (maPlayers[liPlayerIndex].mPlayerID != KI_INVALID_PLAYER_ID)
            {
                RemovePlayer(maPlayers[liPlayerIndex].mPlayerID);
            }
        }
    }

    // BrnNetwork::MarkedManManager::AddPlayer  @ 0x82553B78
    void MarkedManManager::AddPlayer(NetworkPlayerID lPlayerID)
    {
        CgsNetwork::NetworkPlayer* lpNetworkPlayer =
            mpNetworkManager->GetPlayerManager()->GetPlayerByID(lPlayerID);

        if (lpNetworkPlayer == nullptr)
        {
            return;
        }

        // The X360 inlines a linear scan for an existing slot with this id and asserts it is
        // NOT already present ("Trying to add player <id> twice.").
        DataEntry* lpExistingEntry = nullptr;
        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_MARKED_MAN_PLAYERS; ++liPlayerIndex)
        {
            if (maPlayers[liPlayerIndex].mPlayerID == lPlayerID)
            {
                lpExistingEntry = &maPlayers[liPlayerIndex];
                break;
            }
        }
        CGS_ASSERT(lpExistingEntry == nullptr, "Trying to add a player that is already marked.");

        // Find the first free slot (mPlayerID == -1) and assert there is one.
        DataEntry* lpDataEntry = nullptr;
        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_MARKED_MAN_PLAYERS; ++liPlayerIndex)
        {
            if (maPlayers[liPlayerIndex].mPlayerID == KI_INVALID_PLAYER_ID)
            {
                lpDataEntry = &maPlayers[liPlayerIndex];
                break;
            }
        }
        CGS_ASSERT(lpDataEntry != nullptr, "No free entry for this player");
        if (lpDataEntry == nullptr)
        {
            return;
        }

        lpDataEntry->mPlayerID = lPlayerID;
        lpNetworkPlayer->RegisterMessageType(KE_MESSAGE_TYPE_MARKED_MAN, 0x30,
                                             &lpDataEntry->mMarkedManMessageSend,
                                             &lpDataEntry->mMarkedManMessageRecv,
                                             &MarkedManManager::_MarkedManMessageArrivedCallback,
                                             &MarkedManManager::_MarkedManMessageDeliveredCallback,
                                             this);
    }

    // BrnNetwork::MarkedManManager::RemovePlayer  @ 0x82553CF8
    void MarkedManManager::RemovePlayer(NetworkPlayerID lPlayerID)
    {
        DataEntry* lpDataEntry = nullptr;
        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_MARKED_MAN_PLAYERS; ++liPlayerIndex)
        {
            if (maPlayers[liPlayerIndex].mPlayerID == lPlayerID)
            {
                lpDataEntry = &maPlayers[liPlayerIndex];
                break;
            }
        }

        if (lpDataEntry == nullptr)
        {
            return;
        }

        CgsNetwork::NetworkPlayer* lpNetworkPlayer =
            mpNetworkManager->GetPlayerManager()->GetPlayerByID(lPlayerID);
        if (lpNetworkPlayer != nullptr)
        {
            lpNetworkPlayer->UnRegisterMessageType(KE_MESSAGE_TYPE_MARKED_MAN);
        }

        lpDataEntry->mPlayerID = KI_INVALID_PLAYER_ID;
    }

    // BrnNetwork::MarkedManManager::SendMarkedManDataToAll  @ 0x82548D28
    void MarkedManManager::SendMarkedManDataToAll(bool lbIsFinalMarkDecision)
    {
        NetworkPlayerID lLocalNetworkPlayerID = KI_INVALID_PLAYER_ID;
        const bool lbHasLocalPlayer =
            mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lLocalNetworkPlayerID);
        CGS_ASSERT(lbHasLocalPlayer,
                   "mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID( &lLocalNetworkPlayerID )");

        BrnNetwork::PlayerMenuData* lpLocalMenuData = static_cast<BrnNetwork::PlayerMenuData*>(
            mpNetworkManager->GetPlayerManager()->GetMenuDataByID(lLocalNetworkPlayerID));

        const u16 lu16Frame = static_cast<u16>(mpNetworkManager->GetCurrentFrame() % 0xFFFFu);

        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_MARKED_MAN_PLAYERS; ++liPlayerIndex)
        {
            if (maPlayers[liPlayerIndex].mPlayerID != KI_INVALID_PLAYER_ID)
            {
                maPlayers[liPlayerIndex].mMarkedManMessageSend.PrepareForSend(
                    lu16Frame, lpLocalMenuData->mMarkedManID, lbIsFinalMarkDecision);
            }
        }
    }

    // BrnNetwork::MarkedManManager::GetNumberOfPlayersFinishedMarking
    //   @ BrnNetworkMarkedManManager.cpp:* (inlined accessor)
    s32 MarkedManManager::GetNumberOfPlayersFinishedMarking()
    {
        return miNumberOfPlayersFinishedMarking;
    }

    // BrnNetwork::MarkedManManager::MarkingFinished  @ BrnNetworkMarkedManManager.cpp:306
    // No standalone X360 symbol survives and the DWARF hint exposes no locals/side-effects;
    // reconstructed as an empty body (no recovered behaviour to fabricate).
    void MarkedManManager::MarkingFinished()
    {
    }

    // BrnNetwork::MarkedManManager::GetDataEntry  @ BrnNetworkMarkedManManager.cpp:251
    // Inlined into AddPlayer/RemovePlayer in the ARTIST build; reconstructed from those inline
    // searches (and the sibling NetworkDirtyTrickManager::GetDirtyTrickDataEntry idiom): linear
    // scan the 7 slots for the matching id, assert on no match, return the entry (or null).
    MarkedManManager::DataEntry* MarkedManManager::GetDataEntry(NetworkPlayerID lPlayerID)
    {
        DataEntry* lpDataEntry = nullptr;
        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_MARKED_MAN_PLAYERS; ++liPlayerIndex)
        {
            if (maPlayers[liPlayerIndex].mPlayerID == lPlayerID)
            {
                lpDataEntry = &maPlayers[liPlayerIndex];
                break;
            }
        }

        CGS_ASSERT(lpDataEntry != nullptr, "lpDataEntry");
        return lpDataEntry;
    }

    // BrnNetwork::MarkedManManager::ResetRemotePlayers  @ BrnNetworkMarkedManManager.cpp:227
    // No standalone X360 symbol survives; the DWARF hint lists only a loop index. Reconstructed
    // as the matching free-the-slots loop (mPlayerID back to the -1 sentinel) the name implies;
    // the message objects are left intact (re-Construct happens on the next Prepare/round).
    void MarkedManManager::ResetRemotePlayers()
    {
        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_MARKED_MAN_PLAYERS; ++liPlayerIndex)
        {
            maPlayers[liPlayerIndex].mPlayerID = KI_INVALID_PLAYER_ID;
        }
    }

    // BrnNetwork::MarkedManManager::_MarkedManMessageArrivedCallback  @ 0x82548E08
    void MarkedManManager::_MarkedManMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                            NetworkPlayerID lSendingPlayerID,
                                                            void* lpUserData)
    {
        MarkedManManager* lpMarkedManManager = static_cast<MarkedManManager*>(lpUserData);

        BrnNetwork::PlayerMenuData* lpMenuData = static_cast<BrnNetwork::PlayerMenuData*>(
            lpMarkedManManager->mpNetworkManager->GetPlayerManager()->GetMenuDataByID(
                lSendingPlayerID));
        CGS_ASSERT(lpMenuData != nullptr, "lpMenuData");

        MarkedManMessage* lpMarkedManMessage = static_cast<MarkedManMessage*>(lpMessage);
        CGS_ASSERT(lpMarkedManMessage != nullptr, "lpMarkedManMessage");

        NetworkPlayerID lMarkedManPlayerID = KI_INVALID_PLAYER_ID;
        bool            lbIsFinal          = false;
        lpMarkedManMessage->Retrieve(&lMarkedManPlayerID, &lbIsFinal);

        lpMenuData->mMarkedManID = lMarkedManPlayerID;
        if (lbIsFinal)
        {
            ++lpMarkedManManager->miNumberOfPlayersFinishedMarking;
        }
    }

    // BrnNetwork::MarkedManManager::_MarkedManMessageDeliveredCallback  @ BrnNetworkMarkedManManager.cpp:364
    // The DWARF body is purely a debug StrStream log (__FUNCTION__ + delivery status) with no
    // state side-effects, so the reconstructed delivery callback does nothing.
    void MarkedManManager::_MarkedManMessageDeliveredCallback(bool /*lbSuccess*/,
                                                              bool /*lbFakeNack*/,
                                                              CgsNetwork::SignalMessage* /*lpAck*/,
                                                              NetworkPlayerID /*lRecvingPlayerID*/,
                                                              void* /*lpUserData*/)
    {
    }
} // namespace BrnNetwork
