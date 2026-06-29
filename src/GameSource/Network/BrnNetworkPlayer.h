#ifndef BRN_NETWORK_PLAYER_H
#define BRN_NETWORK_PLAYER_H

#include "types.hpp"
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"  // CgsNetwork::NetworkPlayer base
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h" // CgsNetwork::ReliableMessage (callback arg)
#include "GameSource/Network/Messages/BrnStuntMultiplierMessage.h"    // mStuntMultiplierMessageSend (by value)
#include "GameSource/Network/Messages/BrnStuntScoreUpdatedMessage.h"  // mStuntScoreUpdatedMessageSend (by value)

// ===========================================================================
// BrnNetwork::BrnNetworkPlayer -- PARTIAL SLICE
//   Home: GameSource/Network/BrnNetworkPlayer.{h,cpp}
//
// The Burnout per-player session object (DWARF
// `BrnNetwork::BrnNetworkPlayer : public CgsNetwork::NetworkPlayer`). The full ~0x2230-byte
// layout (the 16-deep buffered UpdateMessage ring, the ~12 paired send/recv reliable-message
// subobjects, the latency window, the per-mode send helpers and arrived/delivered callbacks)
// is reconstructed across this class's own large TU. This header still commits NO full layout:
// it models only the handful of members the reconstructed functions reach, exposed BY NAME so
// no intervening storage is pinned. (Same declared-only-accessor idiom the manager slices use.)
//
// X360-vs-DWARF NOTE: the stunt-multiplier / stunt-score-updated message family
// (BrnStuntMultiplierMessage, the SendStunt*/ProcessReceivedStuntMultiplier methods and their
// arrived-callbacks) is X360-ONLY -- present in BURNOUT_X360_ARTIST.XEX but absent from the
// DecFIGS (PS3) DWARF for this class. They are reconstructed here against the ARTIST asm; the
// DWARF member layout (which lacks them) is therefore NOT imported.
// ===========================================================================

namespace CgsNetwork { class TimeManager; }   // mpTimeManager (pointer-only member)

namespace BrnNetwork
{
    class BrnNetworkModule;     // mpNetworkModule

    class BrnNetworkPlayer : public CgsNetwork::NetworkPlayer
    {
    public:
        // X360 UpdateNATData reads the natable flag at this+0xBA9 (a bool); a player is
        // only kicked-as-unNATable when this is set. Declared-only.
        bool IsNatable() const;

        // X360 hands the games-component KickPlayer the player name at this+0xB98.
        // Declared-only.
        const char* GetPlayerName() const;

        // -------------------------------------------------------------------------------
        // Stunt-score / stunt-multiplier reliable-message API (X360-only family).
        // -------------------------------------------------------------------------------

        // X360 0x825826E8 -- stamp the stunt-score send-slot for the current network frame.
        void SendStuntScoreUpdatedMessage(s32 liStuntScore);

        // X360 0x82582720 -- stamp the stunt-multiplier send-slot for the current network
        // frame, recording the frames-since-start the multiplier was sampled on.
        void SendStuntMultiplierMessage(s32 liStuntMultiplier);

        // X360 0x82593DF0 -- a remote multiplier arrived: convert it from the sender's console
        // frame rate to ours and queue a network event (type 34, 16-byte payload) for the
        // game side. lNetworkPlayerID identifies the sending player; lu64MultiplierData is the
        // opaque multiplier payload retrieved from the message.
        void ProcessReceivedStuntMultiplier(CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                            s32 liStuntMultiplier, s64 li64MultiplierData);

        // -------------------------------------------------------------------------------
        // Reliable-message arrived callbacks (registered with the base message tables; the
        // X360 stores them as raw function pointers, so they are static and take the message,
        // the sending player id, and the user-data BrnNetworkPlayer*).
        // -------------------------------------------------------------------------------

        // X360 0x82594668 -- StuntMultiplierMessage arrived.
        static void _StuntMultiplierMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                           CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                           void* lpUserData);

        // X360 0x825945C0 -- StuntScoreUpdatedMessage arrived (the X360 unpacks it through the
        // CheckpointTriggeredMessage retrieval path, then queues a type-47 8-byte event).
        static void _StuntScoreUpdatedMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                            CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                            void* lpUserData);

        // X360-attested: BrnNetwork::StandingsManager::_RoundFinishedMessageArrivedCallback
        // (0x82545530) stores 1 into the bool at this+0x2232 when an inbound
        // PlayerFinishedRoundMessage for the current round reports the sender won it
        // (`stb r30(==1), 0x2232(r3)`). Surfaced as a named setter so that TU reaches the flag by
        // name rather than by raw offset. ADDITIVE GROW (BrnNetworkStandingsManager TU);
        // declared-only (body lands with the full BrnNetworkPlayer TU). FLAGGED: the source
        // member name is not independently attested -- mbHasWonRound is provisional.
        void SetHasWonRound(bool lbHasWonRound);

    private:
        // Only the members the reconstructed functions reach are modelled (by name). Their
        // X360 byte offsets are recorded for documentation; the reconstruction accesses them
        // by name (native x64 widths, semantic parity -- see CgsNetworkPlayer.h), so the
        // intervening storage is NOT pinned here.
        StuntScoreUpdatedMessage mStuntScoreUpdatedMessageSend;   // X360 +0x2100
        StuntMultiplierMessage   mStuntMultiplierMessageSend;     // X360 +0x2158
        CgsNetwork::TimeManager* mpTimeManager;                   // X360 +0x2224
        BrnNetworkModule*        mpNetworkModule;                 // X360 +0x2228
        bool                     mbHasWonRound;                   // X360 +0x2232 (StandingsManager sets it; FLAGGED name)
    };
}

#endif // BRN_NETWORK_PLAYER_H
