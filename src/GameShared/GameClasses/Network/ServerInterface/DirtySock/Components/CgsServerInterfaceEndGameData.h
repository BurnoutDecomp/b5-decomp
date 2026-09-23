#ifndef CGS_SERVER_INTERFACE_END_GAME_DATA_H
#define CGS_SERVER_INTERFACE_END_GAME_DATA_H

#include "types.hpp"

// ===========================================================================
// CgsNetwork::ServerInterfaceEndGameDataBase  (+ X360 leaf)
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfaceEndGameData.{h,cpp}
//
// The end-game-data structure the post-round flow fills in and submits to the
// server. BrnNetwork::PostRoundManager::ActionEndGame constructs one on the stack
// (as the platform-aliased `ServerInterfaceEndGameData`) and calls Prepare().
//
// Base layout (CgsServerInterfaceEndGameData.h DWARF):
//     +0x00  vptr
// virtuals (DWARF order):
//     virtual ~ServerInterfaceEndGameDataBase();   (h:52)
//     virtual bool Prepare();                       (cpp:39)
//
// On X360 the concrete type is ServerInterfaceEndGameDataX360, the platform leaf
// `ServerInterfaceEndGameData` resolves to. Its Prepare (X360 @ 0x82877130) clears
// the structure's own 16-word (64-byte) result payload -- the asm zeroes
// this+0x04 .. this+0x40 inclusive in an 8-iteration two-stores-per-iteration loop
// -- and returns true.
//
// The payload is eight 8-byte player records (console layout): the name pointer the
// post-round flow copies out of each player's lobby params, then that player's network
// id. PostRoundManager::ActionEndGame fills them; ServerInterfaceGamesX360::EndGame
// matches each name against the ConnApi client list and posts the id with the 'skil'
// control. The name is a real pointer, so a record is 16 bytes on the x64 host.
// ===========================================================================

namespace CgsNetwork
{
    struct ServerInterfaceEndGameDataBase
    {
    public:
        ServerInterfaceEndGameDataBase();

        // CgsServerInterfaceEndGameData.h:52
        virtual ~ServerInterfaceEndGameDataBase();

        // Base virtual; the X360 leaf override ServerInterfaceEndGameDataX360::Prepare @0x82877130
        // is the one bodied in this group.
        virtual bool Prepare();
    };

    // X360 platform leaf. `ServerInterfaceEndGameData` aliases to this on the X360
    // build (matching the PostRoundManager call site that constructs the alias).
    struct ServerInterfaceEndGameDataX360 : public ServerInterfaceEndGameDataBase
    {
    public:
        // One player's end-of-game record.
        struct PlayerRecord
        {
            const char* mpcName;     // +0x00
            s32         miPlayerID;  // +0x04
        };

        // The number of player records the payload holds.
        static const s32 KI_MAX_PLAYER_RECORDS = 8;

        ServerInterfaceEndGameDataX360();

        // X360 @ 0x82877130 -- zero the whole result payload, return true.
        virtual bool Prepare();

        PlayerRecord maPlayerRecords[KI_MAX_PLAYER_RECORDS];   // +0x04 (8 bytes per record on the console)
    };

    typedef ServerInterfaceEndGameDataX360 ServerInterfaceEndGameData;
}

#endif // CGS_SERVER_INTERFACE_END_GAME_DATA_H
