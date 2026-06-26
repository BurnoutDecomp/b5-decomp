#ifndef CGS_SERVER_INTERFACE_GAMES_H
#define CGS_SERVER_INTERFACE_GAMES_H

#include "types.hpp"

// CgsNetwork::ServerInterfaceGames - the DirtySock game/lobby server-interface component.
//
// MINIMAL SLICE: only the EGameServerConnectionType enum is materialised here, recovered from the
// DecFIGS DWARF (CgsServerInterfaceGames.h). It is the type of the network-server-interface debug
// component's "Connection Type" menu variable. FLAG: the full ServerInterfaceGames class (lobby /
// play / search state) is not yet reconstructed and is OMITTED; extend this header (don't re-fork
// the enum) when a consumer needs the rest of the class.

namespace CgsNetwork
{
    struct ServerInterfaceGames
    {
        enum EGameServerConnectionType
        {
            E_GAME_SERVER_CONNECTION_TYPE_NONE              = 0,
            E_GAME_SERVER_CONNECTION_TYPE_FALLBACK_BOTH     = 1,
            E_GAME_SERVER_CONNECTION_TYPE_FALLBACK_VOIP_ONLY = 2,
            E_GAME_SERVER_CONNECTION_TYPE_FALLBACK_GAME_ONLY = 3,
            E_GAME_SERVER_CONNECTION_TYPE_NO_FALLBACK       = 4,
            E_GAME_SERVER_CONNECTION_TYPE_COUNT             = 5,
        };
    };
}

#endif // CGS_SERVER_INTERFACE_GAMES_H
