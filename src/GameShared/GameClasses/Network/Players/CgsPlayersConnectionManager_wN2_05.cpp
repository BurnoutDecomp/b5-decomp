#include "GameShared/GameClasses/Network/Players/CgsPlayersConnectionManager.h"
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterface.h"   // AddConnectionStatusChangeCallback

// CgsNetwork::PlayersConnectionManager -- binding the manager to its registry and server
// interface, and hooking the ConnApi status changes.

namespace CgsNetwork
{

// ---- Prepare -------------------------------------------------------------------------
// Latch the registry, the server interface and both callbacks, free every connection entry
// and take the server interface's connection-status callback.
bool PlayersConnectionManager::Prepare(PlayerManager* lpPlayerManager, ServerInterface* lpServerInterface,
                                       ConnMgrConnectionFinalisedCallback lpfConnectionFinalisedCallback,
                                       void* lpConnectionFinalisedUserData,
                                       ConnMgrPlayerDisconnectedCallback lpfPlayerDisconnectedCallback,
                                       void* lpPlayerDisconnectedCallbackData)
{
    mpPlayerManager                  = lpPlayerManager;
    mpfPlayerDisconnectedCallback    = lpfPlayerDisconnectedCallback;
    mpPlayerDisconnectedCallbackData = lpPlayerDisconnectedCallbackData;
    mpfConnectionFinalisedCallback   = lpfConnectionFinalisedCallback;
    mpConnectionFinalisedUserData    = lpConnectionFinalisedUserData;
    mpServerInterface                = lpServerInterface;
    muLocalPlayerIPAddress           = 0;

    for (s32 liIndex = 0; liIndex < KI_MAX_CONNECTION_ENTRIES; ++liIndex)
    {
        maConnectionDataEntry[liIndex].mPlayerConnectionData.mPlayerID = K_INVALID_PLAYER_ID;
    }

    mpServerInterface->AddConnectionStatusChangeCallback(&ConnApiStatusChangeCallback, this);
    return true;
}

// ---- OnLobbyApiCreated ---------------------------------------------------------------
// A new lobby (and ConnApi) came up: take the connection-status callback again.
void PlayersConnectionManager::OnLobbyApiCreated()
{
    mpServerInterface->AddConnectionStatusChangeCallback(&ConnApiStatusChangeCallback, this);
}

}
