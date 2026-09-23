#include "GameShared/GameClasses/Network/VoIP/DirtySock/CgsVoIPManagerDirtySock.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterface.h"          // GetConnAPIRef
#include "GameShared/GameClasses/Network/Buddies/DirtySock/CgsBuddyManagerDirtySock.h"  // BuddyManagerBase::IsBuddyBlocked
#include "GameSource/GameState/BrnCgsPlayerName.h"                                    // PlayerName
#include "connapi.h"                                                                    // ConnApiGetClientList
#include "voip.h"                                                                       // VoipSpeaker / VoipMicrophone

// CgsNetwork::VoIPManager -- refresh each registered talker's voice connection id and
// blocked flag from the ConnApi member list, then re-route speakers and microphones.

namespace CgsNetwork
{

void VoIPManager::UpdateConnectionIDsAndSendMask(ServerInterface* lpServerInterface)
{
    CGS_ASSERT(lpServerInterface, "lpServerInterface");
    CGS_ASSERT(meStatus != E_VOIP_MANAGER_STATUS_UNPREPARED, "meStatus != E_VOIP_MANAGER_STATUS_UNPREPARED");

    DirtySock::ConnApiRefT* lpConnApi = lpServerInterface->GetConnAPIRef();
    CGS_ASSERT(lpConnApi, "lpConnApi");

    const DirtySock::ConnApiClientListT* lpClientList = ConnApiGetClientList(lpConnApi);
    CGS_ASSERT(lpClientList, "lpClientList");

    for (s32 liClient = 0; liClient < lpClientList->iNumClients; ++liClient)
    {
        const DirtySock::ConnApiClientT& lClient = lpClientList->Clients[liClient];

        const s32 liEntryIndex = GetIndexFromPlayerName(lClient.UserInfo.strName);
        if (liEntryIndex < 0)
        {
            continue;
        }
        CGS_ASSERT(liEntryIndex < static_cast<s32>(sizeof(maRegisteredTalkers) / sizeof(maRegisteredTalkers[0])),
                   "liEntryIndex < KI_MAX_NETWORK_PLAYERS");

        maRegisteredTalkers[liEntryIndex].miConnectionID = lClient.iVoipConnId;

        PlayerName lPlayerName;
        lPlayerName.Construct(lClient.UserInfo.strName);
        maRegisteredTalkers[liEntryIndex].mbIsBlocked = mpBuddyManager->IsBuddyBlocked(&lPlayerName);
    }

    const u32 luConnectionMask = GetConnectionMask();
    VoipSpeaker(mpVoiceRef, luConnectionMask);
    VoipMicrophone(mpVoiceRef, mbLocalPlayerHasHeadset ? luConnectionMask : 0u);
}

}
