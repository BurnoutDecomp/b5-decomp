#include "GameShared/GameClasses/Network/Players/CgsPlayersConnectionManager.h"
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"                  // registry lookups / AddPlayer / RemovePlayer
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"                  // SetConnectionData / GetName
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterface.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfo.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceDefaultPlayerInfoData.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceDefaultPlayerParameters.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "connapi.h"     // ConnApiGetClientList, ConnApiClientListT
#include "lobbyapi.h"    // LobbyApiStatus, LobbyApiUserT
#include "lobbyname.h"   // LobbyNameCmp

// CgsNetwork::PlayersConnectionManager -- reconciling the registry with the ConnApi member
// list, and the local-player name lookup.

namespace CgsNetwork
{

// The lobby status selector for the local user's record.
static const s32 KI_LOBBY_STATUS_SELF = 0x73656C66;   // 'self'

// ---- GetPlayerName -------------------------------------------------------------------
// A registered player's name; the local player's comes from the player-info component.
const char* PlayersConnectionManager::GetPlayerName(NetworkPlayerID lPlayerID) const
{
    const NetworkPlayer* lpPlayer = mpPlayerManager->GetPlayerByID(lPlayerID);
    if (lpPlayer != nullptr)
    {
        return lpPlayer->GetName();
    }

    // The console returned the address of its stack copy of the record; the PC keeps the copy
    // in static storage so the name outlives the call.
    static DefaultPlayerInfoData sLocalPlayerInfo;

    CGS_ASSERT(mpServerInterface, "mpServerInterface");
    static_cast<ServerInterfacePlayerInfo*>(mpServerInterface->GetComponent(E_COMPONENTS_PLAYER_INFO))
        ->GetLocalPlayerInfo(&sLocalPlayerInfo);
    if (sLocalPlayerInfo.GetID() == lPlayerID)
    {
        return sLocalPlayerInfo.GetName();
    }

    CGS_ASSERT(false, "Could not find name of player");
    return nullptr;
}

// ---- UpdatePlayerList ----------------------------------------------------------------
// Bring the registry in line with the ConnApi member list. In a game: add every member not yet
// known (the local player through the lobby's 'self' record, a remote one with the id its
// lobby parameters carry), then follow each remote member's game connection -- demangling
// marks the handshake started, an active link fills the player's connection block and starts
// the test-connection exchange, a lost one finalises the connection as failed. Out of a game
// only presence is tracked. Every registered player no longer in the list is removed.
void PlayersConnectionManager::UpdatePlayerList(const DirtySock::ConnApiClientListT* lpClientList,
                                                const CgsSystem::TimerStatus* lpTimerStatus,
                                                u16 lu16CurrentFrame)
{
    bool lbLocalPlayerInList = false;
    bool labEntryInList[KI_MAX_CONNECTION_ENTRIES];
    for (s32 liIndex = 0; liIndex < KI_MAX_CONNECTION_ENTRIES; ++liIndex)
    {
        labEntryInList[liIndex] = false;
    }

    DefaultPlayerInfoData lLocalPlayerInfo;
    LobbyApiUserT         lSelfUser;
    LobbyApiStatus(mpServerInterface->GetLobbyAPIRef(), KI_LOBBY_STATUS_SELF, &lSelfUser,
                   static_cast<s32>(sizeof(lSelfUser)));
    lLocalPlayerInfo.SerialiseFromUser(&lSelfUser);

    ServerInterfaceGames* lpGames = static_cast<ServerInterfaceGames*>(mpServerInterface->GetGameComponent());

    if (lpGames->IsLocalPlayerInGame())
    {
        for (s32 liClient = 0; liClient < lpClientList->iNumClients; ++liClient)
        {
            const DirtySock::ConnApiClientT& lClient = lpClientList->Clients[liClient];
            const bool lbLocal = (LobbyNameCmp(lLocalPlayerInfo.GetName(), lClient.UserInfo.strName) == 0);

            if (lbLocal)
            {
                lbLocalPlayerInList = true;
                const NetworkPlayerID lSelfID = lLocalPlayerInfo.GetID();
                const NetworkPlayerID lPMLocalPlayerID = mpPlayerManager->GetLocalPlayerID();
                if (lPMLocalPlayerID != lSelfID)
                {
                    CGS_ASSERT(lPMLocalPlayerID == K_INVALID_PLAYER_ID, "lPMLocalPlayerID == K_INVALID_PLAYER_ID");
                    AddPlayer(lpTimerStatus, lClient.UserInfo.strName, lSelfID, liClient, lbLocal);
                    muLocalPlayerIPAddress = lClient.UserInfo.uAddr;
                }
                continue;
            }

            ConnectionDataEntry* lpEntry = nullptr;
            NetworkPlayer* lpPlayer = mpPlayerManager->GetPlayerByName(lClient.UserInfo.strName);
            if (lpPlayer != nullptr)
            {
                lpEntry = GetEntry(lpPlayer->GetPlayerID());
                CGS_ASSERT(lpEntry, "lpEntry");
            }
            else
            {
                if (lClient.GameInfo.eStatus == DirtySock::CONNAPI_STATUS_DISC ||
                    lClient.GameInfo.eStatus == DirtySock::CONNAPI_STATUS_DEST)
                {
                    continue;
                }

                const NetworkPlayerID lLocalPlayerID = mpPlayerManager->GetLocalPlayerID();
                DefaultPlayerParameters lPlayerParams;
                lPlayerParams.Prepare();
                lpGames->GetPlayerParametersByPlayerName(lClient.UserInfo.strName, &lPlayerParams);
                const NetworkPlayerID lNewPlayerID = lPlayerParams.GetID();
                CGS_ASSERT(lNewPlayerID != lLocalPlayerID, "lNewPlayerID != lLocalPlayerID");
                CGS_ASSERT(lNewPlayerID != K_INVALID_PLAYER_ID, "lNewPlayerID != K_INVALID_PLAYER_ID");

                lpEntry = AddPlayer(lpTimerStatus, lClient.UserInfo.strName, lNewPlayerID, liClient, lbLocal);
                CGS_ASSERT(lpEntry, "lpEntry");
            }

            const s32 liEntryOffset = static_cast<s32>(lpEntry - maConnectionDataEntry);
            CGS_ASSERT(liEntryOffset >= 0, "liEntryOffset >= 0");
            CGS_ASSERT(liEntryOffset < KI_MAX_CONNECTION_ENTRIES, "liEntryOffset < KI_MAX_NETWORK_PLAYERS");
            labEntryInList[liEntryOffset] = true;

            switch (lClient.GameInfo.eStatus)
            {
                case DirtySock::CONNAPI_STATUS_INIT:
                case DirtySock::CONNAPI_STATUS_CONN:
                case DirtySock::CONNAPI_STATUS_CLSE:
                    break;

                case DirtySock::CONNAPI_STATUS_MNGL:
                    lpEntry->mPlayerConnectionData.meConnectionStatus = E_CONNAPI_PROTOMANGLING;
                    break;

                case DirtySock::CONNAPI_STATUS_ACTV:
                    if (lpEntry->mPlayerConnectionData.meConnectionStatus == E_NOT_STARTED ||
                        lpEntry->mPlayerConnectionData.meConnectionStatus == E_CONNAPI_PROTOMANGLING)
                    {
                        lpEntry->mPlayerConnectionData.meConnectionStatus = E_CHECKING_CONNECTION;

                        ConnectionData lConnectionData;
                        lConnectionData.Clear();
                        lConnectionData.mpNetGameLink         = lClient.pGameLinkRef;
                        lConnectionData.miIPAddress           = static_cast<s32>(lClient.UserInfo.uAddr);
                        lConnectionData.miLocalIPAddress      = static_cast<s32>(lClient.UserInfo.uLocalAddr);
                        lConnectionData.muGamePort            = lClient.UserInfo.uLocalGamePort;
                        lConnectionData.muVoipPort            = lClient.UserInfo.uLocalVoipPort;
                        lConnectionData.muLocalGamePort       = lClient.GameInfo.uLocalPort;
                        lConnectionData.muMnglGamePort        = lClient.GameInfo.uMnglPort;
                        lConnectionData.muLocalVoipPort       = lClient.VoipInfo.uLocalPort;
                        lConnectionData.muMnglVoipPort        = lClient.VoipInfo.uMnglPort;
                        lConnectionData.muConnApiClientId     = lClient.UserInfo.uClientId;
                        lConnectionData.meGameConnectionType  = (lClient.GameInfo.uConnFlags >> 1) & 1;
                        lConnectionData.meVoipConnectionType  = (lClient.VoipInfo.uConnFlags >> 1) & 1;
                        lpEntry->mConnectionData = lConnectionData;

                        NetworkPlayer* lpConnectedPlayer =
                            mpPlayerManager->GetPlayerByID(lpEntry->mPlayerConnectionData.mPlayerID);
                        lpConnectedPlayer->SetConnectionData(lConnectionData);
                        lpConnectedPlayer->SendDirtySockConnectionTelemetry(lClient.GameInfo.uConnFlags,
                                                                            lClient.VoipInfo.uConnFlags);
                        SendTestConnectionMessage(lpEntry, lpTimerStatus, lu16CurrentFrame);
                    }
                    break;

                case DirtySock::CONNAPI_STATUS_DISC:
                case DirtySock::CONNAPI_STATUS_DEST:
                    if (lpEntry->mPlayerConnectionData.meConnectionStatus == E_NOT_STARTED ||
                        lpEntry->mPlayerConnectionData.meConnectionStatus == E_CONNAPI_PROTOMANGLING)
                    {
                        lpEntry->mPlayerConnectionData.meConnectionStatus = E_CONNECTION_FAILURE;
                        if (mpfConnectionFinalisedCallback != nullptr)
                        {
                            mpfConnectionFinalisedCallback(false, lpEntry->mPlayerConnectionData.mPlayerID,
                                                           lpEntry->mConnectionData,
                                                           mpConnectionFinalisedUserData);
                        }
                    }
                    break;

                default:
                    CGS_ASSERT(false, "Unknown connapi status");
                    break;
            }
        }
    }
    else
    {
        for (s32 liClient = 0; liClient < lpClientList->iNumClients; ++liClient)
        {
            const DirtySock::ConnApiClientT& lClient = lpClientList->Clients[liClient];
            if (LobbyNameCmp(lLocalPlayerInfo.GetName(), lClient.UserInfo.strName) == 0)
            {
                lbLocalPlayerInList = true;
                continue;
            }

            NetworkPlayer* lpPlayer = mpPlayerManager->GetPlayerByName(lClient.UserInfo.strName);
            if (lpPlayer == nullptr)
            {
                continue;
            }

            ConnectionDataEntry* lpEntry = GetEntry(lpPlayer->GetPlayerID());
            CGS_ASSERT(lpEntry, "lpEntry");
            const s32 liEntryOffset = static_cast<s32>(lpEntry - maConnectionDataEntry);
            CGS_ASSERT(liEntryOffset >= 0, "liEntryOffset >= 0");
            CGS_ASSERT(liEntryOffset < KI_MAX_CONNECTION_ENTRIES, "liEntryOffset < KI_MAX_NETWORK_PLAYERS");
            labEntryInList[liEntryOffset] = true;
        }
    }

    const NetworkPlayerID lLocalPlayerID = mpPlayerManager->GetLocalPlayerID();
    if (!lbLocalPlayerInList && lLocalPlayerID != K_INVALID_PLAYER_ID)
    {
        RemovePlayer(lLocalPlayerID);
    }

    for (s32 liIndex = 0; liIndex < KI_MAX_CONNECTION_ENTRIES; ++liIndex)
    {
        const NetworkPlayerID lEntryPlayerID = maConnectionDataEntry[liIndex].mPlayerConnectionData.mPlayerID;
        if (labEntryInList[liIndex] || lEntryPlayerID == K_INVALID_PLAYER_ID)
        {
            continue;
        }

        if (lEntryPlayerID == mpPlayerManager->GetLocalPlayerID())
        {
            mpPlayerManager->RemovePlayer(lEntryPlayerID);
            muLocalPlayerIPAddress = 0;
        }
        else
        {
            ConnectionDataEntry* lpEntry = GetEntry(lEntryPlayerID);
            CGS_ASSERT(lpEntry, "lpEntry");
            UnRegisterMessageTypes(lpEntry);
            mpPlayerManager->RemovePlayer(lEntryPlayerID);
            lpEntry->mPlayerConnectionData.mPlayerID = K_INVALID_PLAYER_ID;
        }
    }
}

}
