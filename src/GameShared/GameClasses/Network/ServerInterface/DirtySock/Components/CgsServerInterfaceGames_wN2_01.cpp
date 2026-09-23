#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::gpDebugPrint
#include "connapi.h"                                          // DirtySock::ConnApiCbInfoT

// CgsNetwork::ServerInterfaceGames::ConnApiCallback -- the games component's ConnApi status
// hook (registered in its server-interface slot): it only logs the change.

namespace CgsNetwork
{

void ServerInterfaceGames::ConnApiCallback(DirtySock::ConnApiCbInfoT* lpCbInfo,
                                           ServerInterfaceComponent* /*lpComponent*/)
{
    *CgsDev::Log::gpDebugPrint << "CgsNetwork::ServerInterfaceGames::ConnApiCallback" << ": eType "
                               << lpCbInfo->eType << " eOldStatus " << lpCbInfo->eOldStatus
                               << " eNewStatus " << lpCbInfo->eNewStatus << "\n";
}

}
