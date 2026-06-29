#pragma once

// ===================================================================================
// CgsNetwork::PlayerManager -- minimal owning header
//   b5-decomp/src/GameShared/GameClasses/Network/Players/CgsPlayerManager.h
//
// The session-wide player registry. CgsNetworkPlayer.cpp drives a handful of its
// queries during the per-frame send pump (round-robin turn, connection status, ack/nack
// scheduling, the local-player/game ids, and the per-message-type ack/nack SignalMessage
// objects), plus reaches the embedded ReliableMessageManager.
//
// This header is INTENTIONALLY MINIMAL: it declares only the X360-attested members/methods
// CgsNetworkPlayer.cpp uses, with signatures from the DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/.../CgsPlayerManager.h), gated against the ARTIST binary.
// The full registry layout (player records, NAT data, host-migration state, ~9.5 KB) is
// reconstructed in CgsPlayerManager.cpp's own TU; each method body below lives there.
//
// The ack/nack ring sizes are the bounds the player pump asserts against
// (KI_MAX_ACKS_TO_BUFFER == KI_MAX_NACKS_TO_BUFFER == 10).
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Players/CgsReliableMessageManager.h"
#include "GameShared/GameClasses/Network/Players/CgsConnectionStatusMessage.h"  // EConnectionStatus

namespace CgsNetwork
{
    struct SignalMessage;
    struct NetworkPlayer;
    struct PlayerMenuData;

    struct PlayerManager
    {
        static const s32 KI_MAX_ACKS_TO_BUFFER  = 10;
        static const s32 KI_MAX_NACKS_TO_BUFFER = 10;

        // Which players a registry walk should visit. DWARF CgsPlayerManager.h:18.
        // GetNextPlayerID's second arg in the host-migration walks is 0 (finalised players).
        enum EPlayersToConsider
        {
            E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED = 0,
            E_CONSIDER_ALL_PLAYERS                = 1,
            E_CONSIDER_PLAYERS_COUNT              = 2,
        };

        // --- registry iteration / lookup (host-migration view) ---
        // Iterate registered player ids: seed *lpPlayerID with -1, then call repeatedly;
        // returns false when the walk is exhausted. DWARF :212 / :217 / :226 / :273.
        bool GetNextPlayerID(NetworkPlayerID* lpPlayerID, EPlayersToConsider leConsider) const;
        bool GetNextLocalPlayerID(NetworkPlayerID* lpPlayerID) const;
        NetworkPlayer* GetPlayerByID(NetworkPlayerID liPlayerID) const;
        void SetHostPlayerID(NetworkPlayerID liPlayerID);

        // The lobby/menu view of the player registered under liPlayerID (the registry stores
        // game-specific PlayerMenuData-derived objects; the Burnout build downcasts the result
        // to BrnNetwork::PlayerMenuData at the call sites). DWARF CgsPlayerManager.h:256.
        PlayerMenuData* GetMenuDataByID(NetworkPlayerID liPlayerID) const;

        // Count of registered network players. The X360 call sites pass a single bool flag
        // (0 == count all players, not just the in-game subset). DWARF CgsPlayerManager.h.
        // ADDITIVE GROW (BrnNetworkAggressiveDrivingManager TU): declared-only; body is the
        // CgsPlayerManager.cpp registry TU.
        s32 GetNumberNetworkPlayers(bool lbInGameOnly) const;

        // --- round-robin / connection scheduling (player-pump view) ---
        // Is it liPlayerID's turn to piggy-back a round-robin message this frame? DWARF :364.
        bool IsPlayerTurnToSendRoundRobinMessage(NetworkPlayerID liPlayerID, bool lbInGame,
                                                 s32 liArg);
        // Connection lifecycle state for liPlayerID (==E_CONNECTION_SUCCESS means linked).
        // X360-attested on the PlayersConnectionManager base; surfaced here for the pump.
        EConnectionStatus GetConnectionStatus(NetworkPlayerID liPlayerID) const;

        // Does liPlayerID have a pending ack/nack queued at slot liIndex this frame? DWARF :253/258.
        bool AckNeedsSending(NetworkPlayerID liPlayerID, s32 liIndex) const;
        bool NackNeedsSending(NetworkPlayerID liPlayerID, s32 liIndex) const;

        // The buffered ack/nack control message for the given slot. DWARF :262/266.
        SignalMessage* GetAck(s32 liIndex);
        SignalMessage* GetNack(s32 liIndex);

        // --- identity ---
        NetworkPlayerID GetLocalPlayerID() const;   // reads mLocalPlayerID (DWARF h:162)
        u8              GetGameID() const;           // DWARF :297

        // --- embedded reliable-message queue (DWARF h:110 mReliableMessageManager by value) ---
        ReliableMessageManager& GetReliableMessageManager();
    };
}
