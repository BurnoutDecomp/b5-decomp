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
// -- and returns true. The payload is modelled here as maResultWords[16] so every
// word the asm writes is addressable by name.
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
        ServerInterfaceEndGameDataX360();

        // X360 @ 0x82877130 -- zero the whole result payload, return true.
        virtual bool Prepare();

        // The result payload the X360 Prepare zeroes (this+0x04 .. this+0x40).
        u32 maResultWords[16];   // +0x04
    };

    typedef ServerInterfaceEndGameDataX360 ServerInterfaceEndGameData;
}

#endif // CGS_SERVER_INTERFACE_END_GAME_DATA_H
