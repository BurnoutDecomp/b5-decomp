#ifndef CGS_SERVER_INTERFACE_USERSETS_H
#define CGS_SERVER_INTERFACE_USERSETS_H

#include "types.hpp"

#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceComponent.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"  // CgsNetwork::EKickReason

// ===========================================================================
// CgsNetwork::ServerInterfaceUsersets
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfaceUsersets.{h,cpp}
//
// The usersets (lobby userset create/join/leave/delete/update) server-interface
// component. Derives from CgsNetwork::ServerInterfaceComponent
// (CgsServerInterfaceUsersets.h DWARF:
// `ServerInterfaceUsersets : public CgsNetwork::ServerInterfaceComponent`).
//
// This TU owns only the X360 vector-deleting destructor @ 0x827DE280 (it restores the
// shared component vtable slot off_820CDBF8 at this+0 and conditionally frees). The
// behavioural members and methods (Construct / Prepare / CreateUserset / JoinUserset /
// KickPlayer / ... and the display-list / action members modelled in the
// CgsServerInterfaceUsersets.h DWARF) are owned by their own dossiers; only the
// destructor is reconstructed here so the leaf's vtable slot is emitted exactly once.
// The base layout (vptr + mpcCurrentAction / meStatus / miLastError) is inherited by
// name from the committed ServerInterfaceComponent -- not re-forked here.
// ===========================================================================

namespace CgsNetwork
{
    class ServerInterfaceUsersets : public ServerInterfaceComponent
    {
    public:
        // CgsServerInterfaceUsersets.h:72
        enum EAction
        {
            E_ACTION_CREATE = 0,
            E_ACTION_DELETE = 1,
            E_ACTION_JOIN   = 2,
            E_ACTION_LEAVE  = 3,
            E_ACTION_UPDATE = 4,
            E_ACTION_COUNT  = 5,
        };

        ServerInterfaceUsersets();

        // CgsServerInterfaceUsersets.h:69 -- vector deleting destructor @ 0x827DE280.
        virtual ~ServerInterfaceUsersets();

        // ADDITIVE GROW (flagged by the BrnNetworkConnectionManager group). The NAT-kick walk
        // (X360 KickUnNATablePlayer @ 0x82566860) routes the kick through the userset component
        // when the target is in the local player's userset:
        //   IsPlayerInOurUserset() -- is the named player in our userset? (DWARF h:238)
        //   KickPlayer()           -- evict the named player with a kick reason (DWARF h:241)
        // Signatures from DecFIGS DWARF (CgsServerInterfaceUsersets.h). Declared-only here;
        // bodies are homed in the usersets component's own behavioural TU.
        bool IsPlayerInOurUserset(const char* lpcPlayerName) const;
        void KickPlayer(const char* lpcPlayerName, EKickReason leReason);
    };
}

#endif // CGS_SERVER_INTERFACE_USERSETS_H
