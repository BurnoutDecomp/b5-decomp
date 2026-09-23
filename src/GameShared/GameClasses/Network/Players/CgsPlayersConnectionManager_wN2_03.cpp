#include "GameShared/GameClasses/Network/Players/CgsPlayersConnectionManager.h"
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"                  // registry lookups / AddPlayer / RemovePlayer
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"                  // SetConnectionData / GetName
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "connapi.h"     // ConnApiGetClientList, ConnApiClientListT
#include "lobbyname.h"   // LobbyNameCmp

// CgsNetwork::PlayersConnectionManager -- the kick choice and the ConnApi status callback.

namespace CgsNetwork
{

// ---- GetIDOfPlayerToKick -------------------------------------------------------------
// Of two players who cannot reach each other, the one to drop: never the host; otherwise
// whichever of the two appears later in the ConnApi member list, falling back to the higher
// id when the list does not settle it.
NetworkPlayerID PlayersConnectionManager::GetIDOfPlayerToKick(NetworkPlayerID lPlayerID1,
                                                              NetworkPlayerID lPlayerID2) const
{
    CGS_ASSERT(lPlayerID1 != lPlayerID2, "Don't call with the same Player IDs");

    const NetworkPlayerID lHostPlayerID = mpPlayerManager->GetHostPlayerID();
    if (lPlayerID1 == lHostPlayerID)
    {
        return lPlayerID2;
    }
    if (lPlayerID2 == lHostPlayerID)
    {
        return lPlayerID1;
    }

    const char* lpcName1 = GetPlayerName(lPlayerID1);
    const char* lpcName2 = GetPlayerName(lPlayerID2);

    DirtySock::ConnApiRefT* lpConnApi = mpServerInterface->GetConnAPIRef();
    if (lpConnApi != nullptr)
    {
        const DirtySock::ConnApiClientListT* lpClientList = ConnApiGetClientList(lpConnApi);
        if (lpClientList != nullptr && lpcName1 != nullptr && lpcName2 != nullptr)
        {
            for (s32 liIndex = lpClientList->iNumClients - 1; liIndex >= 0; --liIndex)
            {
                const char* lpcClientName = lpClientList->Clients[liIndex].UserInfo.strName;
                if (LobbyNameCmp(lpcName1, lpcClientName) == 0)
                {
                    return lPlayerID1;
                }
                if (LobbyNameCmp(lpcName2, lpcClientName) == 0)
                {
                    return lPlayerID2;
                }
            }
        }
    }

    return (lPlayerID1 > lPlayerID2) ? lPlayerID1 : lPlayerID2;
}

// ---- ConnApiStatusChangeCallback -----------------------------------------------------
// A member's game connection went away: clear its player's connection block and report the
// player disconnected. (User data = the manager.)
void PlayersConnectionManager::ConnApiStatusChangeCallback(DirtySock::ConnApiRefT* lpConnApi,
                                                           DirtySock::ConnApiCbInfoT* lpCbInfo,
                                                           void* lpUserData)
{
    PlayersConnectionManager* lpSelf = static_cast<PlayersConnectionManager*>(lpUserData);

    const DirtySock::ConnApiClientListT* lpClientList = ConnApiGetClientList(lpConnApi);
    CGS_ASSERT(lpClientList, "lpClientList");

    const bool lbDisconnected = (lpCbInfo->eNewStatus == DirtySock::CONNAPI_STATUS_DISC) ||
                                (lpCbInfo->eNewStatus == DirtySock::CONNAPI_STATUS_DEST);
    if (!lbDisconnected || lpCbInfo->eType != DirtySock::CONNAPI_CBTYPE_GAMEEVENT)
    {
        return;
    }

    NetworkPlayer* lpPlayer = lpSelf->mpPlayerManager->GetPlayerByName(
        lpClientList->Clients[lpCbInfo->iClientIndex].UserInfo.strName);
    if (lpPlayer == nullptr)
    {
        return;
    }

    ConnectionData lConnectionData;
    lConnectionData.Clear();
    lpPlayer->SetConnectionData(lConnectionData);

    if (lpSelf->mpfPlayerDisconnectedCallback != nullptr)
    {
        lpSelf->mpfPlayerDisconnectedCallback(lpPlayer->GetPlayerID(), lpSelf->mpPlayerDisconnectedCallbackData);
    }
}

}
