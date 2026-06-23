#ifndef CGS_SERVER_INTERFACE_BROADCAST_MESSAGES_H
#define CGS_SERVER_INTERFACE_BROADCAST_MESSAGES_H

#include "types.hpp"

#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceComponent.h"

// ===========================================================================
// CgsNetwork::ServerInterfaceBroadcastMessages
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfaceBroadcastMessages.{h,cpp}
//
// The broadcast-messages server-interface component (the in-game message /
// chat broadcast stage owner). Derives from CgsNetwork::ServerInterfaceComponent
// (CgsServerInterfaceBroadcastMessages.h DWARF:
// `ServerInterfaceBroadcastMessages : public CgsNetwork::ServerInterfaceComponent`).
//
// This TU owns only the X360 scalar-deleting destructor @ 0x827DE1A8 (it restores the
// shared component vtable slot off_820CDBF8 at this+0 and conditionally frees). The
// rich behavioural members and methods (Construct / Prepare / Update / SendGameMessage
// / RegisterMessageCallback / ... and the callback / message-packet arrays modelled in
// the CgsServerInterfaceBroadcastMessages.h DWARF) are owned by their own dossiers; only
// the destructor is reconstructed here, so the leaf's vtable slot is emitted exactly
// once. The base layout (vptr + mpcCurrentAction / meStatus / miLastError) is inherited
// by name from the committed ServerInterfaceComponent -- not re-forked here.
// ===========================================================================

namespace CgsNetwork
{
    class ServerInterfaceBroadcastMessages : public ServerInterfaceComponent
    {
    public:
        // CgsServerInterfaceBroadcastMessages.h:109
        enum EAction
        {
            E_ACTION_BROADCAST_MESSAGE = 0,
            E_ACTION_COUNT             = 1,
        };

        ServerInterfaceBroadcastMessages();

        // CgsServerInterfaceBroadcastMessages.h:107 -- scalar deleting destructor @ 0x827DE1A8.
        virtual ~ServerInterfaceBroadcastMessages();
    };
}

#endif // CGS_SERVER_INTERFACE_BROADCAST_MESSAGES_H
