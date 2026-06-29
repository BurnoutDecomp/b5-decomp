#include "GameSource/Network/Managers/BrnNetworkConnectionManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                  // CGS_ASSERT
#include "GameSource/Network/BrnNetworkManager.h"                                   // BrnNetworkManager, GetTime/GetServerInterface/GetPlayerManager
#include "GameSource/Network/BrnServerInterface.h"                                  // BrnServerInterface (GetGameComponent/GetUsersetsComponent/GetStatus)
#include "GameSource/Network/BrnNetworkPlayer.h"                                    // BrnNetwork::BrnNetworkPlayer (IsNatable/GetPlayerName)
#include "GameSource/Network/Parameters/BrnNetworkPlayerParamsClass.h"              // BrnNetwork::PlayerParams
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"                // CgsNetwork::PlayerManager
#include "GameShared/GameClasses/Network/Players/CgsPlayersConnectionManager.h"     // CgsNetwork::PlayersConnectionManager
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h" // EComponents / EKickReason
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"    // ServerInterfaceGames
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceUsersets.h" // ServerInterfaceUsersets

// ===========================================================================
// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::ConnectionManager::Construct           @ 0x82544510
//   BrnNetwork::ConnectionManager::KickUnNATablePlayer @ 0x82566860
//   BrnNetwork::ConnectionManager::UpdateNATData       @ 0x8256CFF0
// (Prepare/Release/Destruct/Update carry no recovered body in the available
//  exports -- the DWARF lists them as empty shells -- and are trivial no-ops.)
//
// The NAT-kick policy: on a host that has not yet started the game, once at least
// KF_UN_NAT_KICK_TIMEOUT seconds have elapsed since the last kick, walk the player
// registry and evict the player that cannot NAT-traverse to its peers. The kick is
// issued through whichever server-interface component (game or userset) the local
// player is in.
//
// The X360 streams a couple of diagnostic lines through an un-homed global debug
// stream (off_82F335C8) on the "component busy, not kicking" branches; that logging
// is a pure side-channel with no observable contract, so it is dropped here (the
// same treatment CgsMessage.cpp gives off_82F335C8). The observable behaviour --
// simply NOT issuing the kick that frame -- is preserved.
// ===========================================================================

namespace BrnNetwork
{
    // X360 UpdateNATData builds a Time(2.0f) from this (flt_8207C624, resolved by
    // Hex-Rays to 2.0) and proceeds only once currentTime - mLastKickTime >= it.
    const f32 ConnectionManager::KF_UN_NAT_KICK_TIMEOUT = 2.0f;

    // ---- Construct @ 0x82544510 -----------------------------------------------
    // `stw r4, 8(r3)` -- stash the owning manager; nothing else.
    void ConnectionManager::Construct(BrnNetworkManager* lpNetworkManager)
    {
        mpNetworkManager = lpNetworkManager;
    }

    // ---- lifecycle no-ops (no recovered body) ---------------------------------
    bool ConnectionManager::Prepare()
    {
        return true;
    }

    bool ConnectionManager::Release()
    {
        return true;
    }

    void ConnectionManager::Destruct()
    {
    }

    void ConnectionManager::Update()
    {
    }

    // ---- KickUnNATablePlayer @ 0x82566860 -------------------------------------
    void ConnectionManager::KickUnNATablePlayer(NetworkPlayerID liPlayerID)
    {
        // Stamp the time of this kick attempt (asm: read the manager time, store into
        // mLastKickTime's seconds@+0 / fraction@+4).
        mLastKickTime = mpNetworkManager->GetTime();

        BrnServerInterface* lpServerInterface = mpNetworkManager->GetServerInterface();

        CgsNetwork::ServerInterfaceGames* lpGames =
            reinterpret_cast<CgsNetwork::ServerInterfaceGames*>(lpServerInterface->GetGameComponent());
        CgsNetwork::ServerInterfaceUsersets* lpUsersets =
            reinterpret_cast<CgsNetwork::ServerInterfaceUsersets*>(lpServerInterface->GetUsersetsComponent());

        CGS_ASSERT(lpGames, "lpGames");
        CGS_ASSERT(lpUsersets, "lpUsersets");

        // Fetch the target player's lobby parameters (fills the name we kick by).
        PlayerParams lPlayerParams;
        lPlayerParams.Prepare();
        lpGames->GetPlayerParametersByPlayerID(liPlayerID, &lPlayerParams);

        CGS_ASSERT(lpGames->IsLocalPlayerInGame(),
                   "Natting when not in a game or a userset!");

        if (lpGames->IsLocalPlayerInGame())
        {
            const char* lpcPlayerName = lPlayerParams.GetName();

            if (lpUsersets->IsPlayerInOurUserset(lpcPlayerName))
            {
                // The player is in our userset -- kick through the userset component.
                if (lpServerInterface->GetStatus(CgsNetwork::E_COMPONENTS_USERSETS)
                        != BrnServerInterface::E_STATUS_BUSY)
                {
                    lpUsersets->KickPlayer(lpcPlayerName, CgsNetwork::E_KICKREASON_UNNATABLE);
                }
                // else: userset component busy -- skip the kick this frame (X360 logs a
                // diagnostic to the un-homed debug stream; dropped).
            }
            else
            {
                // The player is in the game (not our userset) -- kick through the game component.
                if (lpServerInterface->GetStatus(CgsNetwork::E_COMPONENTS_GAMES)
                        != BrnServerInterface::E_STATUS_BUSY)
                {
                    lpGames->KickPlayer(lpcPlayerName, CgsNetwork::E_KICKREASON_UNNATABLE, 0);
                }
                // else: game component busy -- skip the kick this frame (debug log dropped).
            }
        }
    }

    // ---- UpdateNATData @ 0x8256CFF0 -------------------------------------------
    void ConnectionManager::UpdateNATData()
    {
        BrnServerInterface* lpServerInterface = mpNetworkManager->GetServerInterface();
        CgsNetwork::ServerInterfaceGames* lpGames =
            reinterpret_cast<CgsNetwork::ServerInterfaceGames*>(lpServerInterface->GetGameComponent());

        // Rate-limit: do nothing until KF_UN_NAT_KICK_TIMEOUT has elapsed since the last kick.
        const CgsSystem::Time lTimeout(KF_UN_NAT_KICK_TIMEOUT);
        const CgsSystem::Time lElapsed = mpNetworkManager->GetTime() - mLastKickTime;
        if (lElapsed < lTimeout)
        {
            return;
        }

        // Host-only NAT policy, and only before the game has started.
        if (!lpGames->IsLocalPlayerInGame())
        {
            return;
        }
        if (!lpGames->IsLocalPlayerHost())
        {
            return;
        }
        if (lpGames->IsGameStarted())
        {
            return;
        }

        CgsNetwork::PlayersConnectionManager* lpConnectionManager =
            mpNetworkManager->GetPlayerManager()->GetPlayersConnectionManager();
        CgsNetwork::PlayerManager* lpPlayerManager = mpNetworkManager->GetPlayerManager();

        if (lpConnectionManager->AreAllConnectionsSuccessful())
        {
            // Everyone else is linked: kick any player that has finished a peer connection but
            // whose own game-component state is idle yet still flagged un-NATable.
            NetworkPlayerID lPlayerID = -1;
            while (lpPlayerManager->GetNextPlayerID(&lPlayerID,
                                                    CgsNetwork::PlayerManager::E_CONSIDER_ALL_PLAYERS))
            {
                BrnNetworkPlayer* lpPlayer =
                    static_cast<BrnNetworkPlayer*>(lpPlayerManager->GetPlayerByID(lPlayerID));
                if (lpPlayer
                    && lpPlayer->IsNatable()
                    && lpServerInterface->GetStatus(CgsNetwork::E_COMPONENTS_GAMES)
                           == BrnServerInterface::E_STATUS_IDLE)
                {
                    lpGames->KickPlayer(lpPlayer->GetPlayerName(),
                                        CgsNetwork::E_KICKREASON_LOST_CONNECTION, 0);
                    mLastKickTime = mpNetworkManager->GetTime();
                }
            }
        }
        else
        {
            // Some peers are still connecting: find the first pair that has failed to connect
            // to each other and kick the player the connection table nominates.
            NetworkPlayerID lPlayerID1 = -1;
            if (!lpPlayerManager->GetNextPlayerID(&lPlayerID1,
                                                  CgsNetwork::PlayerManager::E_CONSIDER_ALL_PLAYERS))
            {
                return;
            }

            do
            {
                NetworkPlayerID lPlayerID2 = -1;
                if (!lpPlayerManager->GetNextPlayerID(&lPlayerID2,
                                                      CgsNetwork::PlayerManager::E_CONSIDER_ALL_PLAYERS))
                {
                    // No second player to pair with -- advance the outer player and retry.
                    continue;
                }

                // Scan the remaining players for one that lPlayerID1 has failed to connect to.
                bool lbFoundFailedPair = false;
                do
                {
                    if (lPlayerID1 != lPlayerID2
                        && lpConnectionManager->HavePlayersFailedToConnect(lPlayerID1, lPlayerID2))
                    {
                        lbFoundFailedPair = true;
                        break;
                    }
                }
                while (lpPlayerManager->GetNextPlayerID(
                           &lPlayerID2, CgsNetwork::PlayerManager::E_CONSIDER_ALL_PLAYERS));

                if (lbFoundFailedPair)
                {
                    const NetworkPlayerID lPlayerIDToKick =
                        lpConnectionManager->GetIDOfPlayerToKick(lPlayerID1, lPlayerID2);
                    KickUnNATablePlayer(lPlayerIDToKick);
                    return;
                }
            }
            while (lpPlayerManager->GetNextPlayerID(
                       &lPlayerID1, CgsNetwork::PlayerManager::E_CONSIDER_ALL_PLAYERS));
        }
    }
}
