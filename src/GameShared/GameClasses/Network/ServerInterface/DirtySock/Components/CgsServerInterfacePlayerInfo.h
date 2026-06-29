#ifndef CGS_SERVER_INTERFACE_PLAYER_INFO_H
#define CGS_SERVER_INTERFACE_PLAYER_INFO_H

#include "types.hpp"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceComponent.h"

// ===========================================================================
// CgsNetwork::ServerInterfacePlayerInfo
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfacePlayerInfo.{h,cpp}
//
// The player-info server-interface component (the
// E_PREPARESTAGE_PLAYER_INFO_COMPONENT / E_RELEASESTAGE_PLAYER_INFO_COMPONENT
// stage owner; embedded by value as BrnServerInterfaceBase::mPlayerInfo). Like
// every other DirtySock component it is a polymorphic leaf over the shared
// CgsNetwork::ServerInterfaceComponent base. It owns and serves the per-player
// CgsNetwork::ServerInterfacePlayerInfoDataBase records the lobby reports (homed
// separately in CgsServerInterfacePlayerInfoData.{h,cpp}).
//
// FLAGGED: this component has no dedicated dossier in the available exports.
// Mirroring the sibling component homes that are reached only through the
// aggregate's by-value embed + the polymorphic-teardown vtable walk
// (CgsServerInterfaceRankings.h / CgsServerInterfaceBroadcastMessages.h), only the
// shared ServerInterfaceComponent base layout is modelled here -- the leaf carries
// no recovered data members of its own, and its behavioural methods (Construct /
// Prepare / Update / GetPlayerInfo / ...) belong to their own TUs. Any player-info
// cache members would GROW this home additively when their stores are recovered.
// ===========================================================================

namespace CgsNetwork
{
    class ServerInterfacePlayerInfo : public ServerInterfaceComponent
    {
    public:
        ServerInterfacePlayerInfo();

        // Polymorphic teardown slot (scalar deleting destructor: restores the shared
        // component vtable off_820CDBF8 at this+0 and conditionally frees -- the leaf
        // owns no heap members beyond the base layout).
        virtual ~ServerInterfacePlayerInfo();
    };
}

#endif // CGS_SERVER_INTERFACE_PLAYER_INFO_H
