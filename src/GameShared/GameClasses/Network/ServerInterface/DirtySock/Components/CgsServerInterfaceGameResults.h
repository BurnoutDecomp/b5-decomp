#ifndef CGS_SERVER_INTERFACE_GAME_RESULTS_H
#define CGS_SERVER_INTERFACE_GAME_RESULTS_H

#include "types.hpp"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceStructureInterface.h"

// ===========================================================================
// CgsNetwork::ServerInterfaceGameResultsBase
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfaceGameResults.h
//
// The game-result record ServerInterfaceGames::SendGameResult uploads: a server-interface
// structure that knows how to serialise itself into the lobby message record. The only
// data is the inherited vptr at +0x00; the game leaf (BrnNetwork::GameResults) carries
// the payload.
//
// Vtable (after the six ServerInterfaceStructureInterface slots):
//   +0x18  Prepare
//   +0x1C  SerialiseToString   (SendGameResult calls it through this slot)
//
// No base Prepare / SerialiseToString body survives in the console image: the only
// result vtable is the game leaf's, which overrides both, and the leaf's destructor
// reinstalls the ServerInterfaceStructureInterface vptr directly (this destructor is
// empty and inlined). Both are therefore pure here.
// ===========================================================================

namespace CgsNetwork
{
    struct ServerInterfaceGameResultsBase : public ServerInterfaceStructureInterface
    {
    public:
        ServerInterfaceGameResultsBase() {}
        virtual ~ServerInterfaceGameResultsBase() {}

        virtual bool Prepare() = 0;

        // Append this result record's tagged fields to the lobby message record.
        virtual void SerialiseToString(char* lpcRecord, s32 liRecLen) const = 0;
    };
}

#endif // CGS_SERVER_INTERFACE_GAME_RESULTS_H
