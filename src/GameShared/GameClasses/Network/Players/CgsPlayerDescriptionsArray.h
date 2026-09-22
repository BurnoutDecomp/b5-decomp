#pragma once

// ===================================================================================
// CgsNetwork::PlayerData -- owning header
//   b5-decomp/src/GameShared/GameClasses/Network/Players/CgsPlayerDescriptionsArray.h
//
// One slot of the player registry's active / inactive tables (PlayerManager::
// maActivePlayers / maInactivePlayers). 12 console bytes; PlayerManager::Construct fills
// every slot in this member order (menu data, network player, then a -1 id), and the
// registry lookups walk the tables by the id word at +0x08 and read the network player
// at +0x04 (a null network player marks a local player).
//
//   +0x00  mpMenuData       (PlayerMenuData*)
//   +0x04  mpNetworkPlayer  (NetworkPlayer*)
//   +0x08  mPlayerID        (NetworkPlayerID)
//
// The accessors are inlined everywhere on the console (no standalone bodies).
// ===================================================================================

#include "types.hpp"

namespace CgsNetwork
{
    struct NetworkPlayer;
    struct PlayerMenuData;

    typedef s32 NetworkPlayerID;

    struct PlayerData
    {
        NetworkPlayer* GetNetworkPlayer() const                { return mpNetworkPlayer; }
        void           SetNetworkPlayer(NetworkPlayer* lpPlayer) { mpNetworkPlayer = lpPlayer; }
        PlayerMenuData* GetMenuData() const                     { return mpMenuData; }
        void           SetMenuData(PlayerMenuData* lpMenuData)  { mpMenuData = lpMenuData; }
        void           Set(NetworkPlayer* lpPlayer, PlayerMenuData* lpMenuData)
        {
            mpNetworkPlayer = lpPlayer;
            mpMenuData      = lpMenuData;
        }
        NetworkPlayerID GetPlayerID() const                     { return mPlayerID; }
        void           SetPlayerID(NetworkPlayerID lPlayerID)   { mPlayerID = lPlayerID; }

    private:
        PlayerMenuData* mpMenuData;        // +0x00
        NetworkPlayer*  mpNetworkPlayer;   // +0x04
        NetworkPlayerID mPlayerID;         // +0x08
    };
}
