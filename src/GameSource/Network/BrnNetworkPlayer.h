#ifndef BRN_NETWORK_PLAYER_H
#define BRN_NETWORK_PLAYER_H

#include "types.hpp"
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"  // CgsNetwork::NetworkPlayer base

// ===========================================================================
// BrnNetwork::BrnNetworkPlayer -- MINIMAL SLICE
//   Home: GameSource/Network/BrnNetworkPlayer.{h,cpp}
//
// The Burnout per-player session object (DWARF
// `BrnNetwork::BrnNetworkPlayer : public CgsNetwork::NetworkPlayer`). The full
// ~3KB layout (buffered-message ring, per-mode send helpers, the reliable-message
// callbacks) is reconstructed in its own large TU; this header models only the two
// members the NAT-kick policy (BrnNetworkConnectionManager) reaches, exposed BY
// NAME as declared-only accessors so no layout is committed here:
//
//   * IsNatable()      -- the per-player "can NAT-traverse to its peers" flag the
//                         host reads before kicking (X360 UpdateNATData: `lbz this+0xBA9`).
//   * GetPlayerName()  -- the player's display name string handed to the games
//                         component's KickPlayer (X360: `addi r4, this+0xB98`).
//
// Both bodies belong to the full BrnNetworkPlayer TU; declared-only here (the same
// declared-only-accessor idiom used by BrnNetworkManager's GetServerInterface etc.).
// ===========================================================================

namespace BrnNetwork
{
    class BrnNetworkPlayer : public CgsNetwork::NetworkPlayer
    {
    public:
        // X360 UpdateNATData reads the natable flag at this+0xBA9 (a bool); a player is
        // only kicked-as-unNATable when this is set. Declared-only.
        bool IsNatable() const;

        // X360 hands the games-component KickPlayer the player name at this+0xB98.
        // Declared-only.
        const char* GetPlayerName() const;
    };
}

#endif // BRN_NETWORK_PLAYER_H
