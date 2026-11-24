#include "GameShared/GameClasses/Network/VoIP/DirtySock/CgsVoIPManagerDirtySock.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT at ARTIST locations
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
    CGS_ASSERT(lpServerInterface, CgsDev::Assert::MessageAt(
        "lpServerInterface",
        __FILE__, 584 /* ARTIST 0x828994E8 */));
    CGS_ASSERT(meStatus != E_VOIP_MANAGER_STATUS_UNPREPARED, CgsDev::Assert::MessageAt(
        "meStatus != E_VOIP_MANAGER_STATUS_UNPREPARED",
        __FILE__, 586 /* ARTIST 0x82899510 */));

    DirtySock::ConnApiRefT* lpConnApi = lpServerInterface->GetConnAPIRef();
    CGS_ASSERT(lpConnApi, CgsDev::Assert::MessageAt(
        "lpConnApi",
        __FILE__, 589 /* ARTIST 0x82899538 */));

    const DirtySock::ConnApiClientListT* lpClientList = ConnApiGetClientList(lpConnApi);
    CGS_ASSERT(lpClientList, CgsDev::Assert::MessageAt(
        "lpClientList",
        __FILE__, 592 /* ARTIST 0x82899568 */));

    for (s32 liClient = 0; liClient < lpClientList->iNumClients; ++liClient)
    {
        const DirtySock::ConnApiClientT& lClient = lpClientList->Clients[liClient];

        const s32 liEntryIndex = GetIndexFromPlayerName(lClient.UserInfo.strName);
        if (liEntryIndex < 0)
        {
            continue;
        }
        CGS_ASSERT(liEntryIndex < static_cast<s32>(sizeof(maRegisteredTalkers) / sizeof(maRegisteredTalkers[0])), CgsDev::Assert::MessageAt(
            "liEntryIndex < KI_MAX_NETWORK_PLAYERS",
            __FILE__, 607 /* ARTIST 0x828995BC */));

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
