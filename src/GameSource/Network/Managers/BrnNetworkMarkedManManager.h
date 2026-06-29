#pragma once

// ===================================================================================
// BrnNetwork::MarkedManManager -- owning header
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkMarkedManManager.h
//
// The network-side manager for the "Marked Man" road-rules mode: tracks, per remote player,
// the marked-man choice each peer has sent / received, and totals how many peers have
// finalised their decision. SHAPE is taken from the DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/GameSource/Network/Managers/BrnNetworkMarkedManManager.h),
// gated against BURNOUT_X360_ARTIST.XEX. Every member is accessed BY NAME; the X360 byte
// offsets the ARTIST asm dereferences are documented inline for traceability:
//
//   maPlayers[0]               at +0x000  (7 entries, 100-byte stride -- `addi rN,rN,0x64`)
//     DataEntry::mPlayerID            at +0x00 of the entry  (first word; -1 == free)
//     DataEntry::mMarkedManMessageSend at +0x04 (entry+4   == `addi r6,r31,4`)
//     DataEntry::mMarkedManMessageRecv at +0x34 (entry+0x34 == `addi r7,r31,0x34`)
//   mpNetworkManager           at +0x2BC (700)  -- `lwz r11,0x2BC(rThis)`
//   miNumberOfPlayersFinishedMarking at +0x2C0 (704) -- `*(this+704)++` / `=0`
//
// MarkedManMessage is reused BY NAME from its committed home (BrnMarkedManMessage.h); each
// is 0x30 bytes on X360 (entry stride 100 = 4 + 0x30 + 0x30 + 4 alignment slack). The C++
// reconstruction lets the compiler lay the entry out naturally (native widths), so the
// offsets above are documentation, not asserted byte layout.
// ===================================================================================

#include "types.hpp"
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"        // BrnNetwork::NetworkPlayerID
#include "GameSource/Network/Messages/BrnMarkedManMessage.h"       // BrnNetwork::MarkedManMessage

namespace CgsNetwork
{
    struct ReliableMessage;
    struct SignalMessage;
}

namespace BrnNetwork
{
    class BrnNetworkManager;

    class MarkedManManager
    {
    public:
        void Construct(BrnNetworkManager* lpNetworkManager);
        bool Prepare();
        bool Release();
        void Destruct();
        void Update();
        void OnRoundStart();
        void Disconnected();
        void AddPlayer(NetworkPlayerID lPlayerID);
        void RemovePlayer(NetworkPlayerID lPlayerID);
        void SendMarkedManDataToAll(bool lbIsFinalMarkDecision);
        s32  GetNumberOfPlayersFinishedMarking();
        void MarkingFinished();

    private:
        // The reliable-message-type tag the per-player send/recv MarkedManMessage pair is
        // registered under (X360: `li r4,0x19` into NetworkPlayer::RegisterMessageType).
        static const s32 KE_MESSAGE_TYPE_MARKED_MAN = 25;

        // Fixed-capacity table of remote players (DWARF maPlayers[7]).
        static const s32 KI_MAX_MARKED_MAN_PLAYERS = 7;

        // DWARF BrnNetworkMarkedManManager.h:103. One tracked remote player: its id plus the
        // outgoing / incoming MarkedManMessage objects registered with NetworkPlayer.
        struct DataEntry
        {
            NetworkPlayerID  mPlayerID;             // :105  (+0x00; -1 == free slot)
            MarkedManMessage mMarkedManMessageSend; // :106  (+0x04)
            MarkedManMessage mMarkedManMessageRecv; // :107  (+0x34)
        };

        DataEntry* GetDataEntry(NetworkPlayerID lPlayerID);
        void       ResetRemotePlayers();

        // Reliable-message arrival / delivery callbacks registered per player. The X360 passes
        // `this` as the trailing void* user-data so the static callback can recover the manager.
        static void _MarkedManMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                      NetworkPlayerID lSendingPlayerID,
                                                      void* lpUserData);
        static void _MarkedManMessageDeliveredCallback(bool lbSuccess, bool lbFakeNack,
                                                        CgsNetwork::SignalMessage* lpAck,
                                                        NetworkPlayerID lRecvingPlayerID,
                                                        void* lpUserData);

        DataEntry          maPlayers[KI_MAX_MARKED_MAN_PLAYERS];  // :110  (+0x000)
        BrnNetworkManager* mpNetworkManager;                      // :112  (+0x2BC)
        s32                miNumberOfPlayersFinishedMarking;      // :113  (+0x2C0)
    };
} // namespace BrnNetwork
