#ifndef CGS_SERVER_INTERFACE_GAME_SEARCH_PARAMS_H
#define CGS_SERVER_INTERFACE_GAME_SEARCH_PARAMS_H

#include "types.hpp"
#include "../CgsServerInterfaceStructureInterface.h"

// ===========================================================================
// CgsNetwork::ServerInterfaceGameSearchParamsBase
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfaceGameSearchParams.{h,cpp}
//
// Parameter block describing a "search for games" request. Derives from
// ServerInterfaceStructureInterface (vptr-only polymorphic base) and serialises
// itself into a tagfield record (START/COUNT/ROOM/ASYNC/SYS*/PLAYERS/CUSTOM*).
//
// LAYOUT (dwarfdump + X360 asm @ 0x82878898 SerialiseToString):
//   +0x00  vptr
//   +0x04  miNumGames        (s32)
//   +0x08  miRoomID          (s32)   (-1 == none)
//   +0x0C  muGameFlagsMask   (u32)
//   +0x10  muGameFlagsValue  (u32)
//   +0x14  mbReturnPlayers   (bool)
//
// VTABLE order after the StructureInterface six entries:
//   [+0x18] Prepare, [+0x1C] GetCustomFlagsMask, [+0x20] GetCustomFlagsValue.
// The asm calls them at exactly those vtable offsets, so they follow the base's
// GetData()-const slot (+0x14).
// ===========================================================================

namespace CgsNetwork
{
    struct ServerInterfaceGameSearchParamsBase : public ServerInterfaceStructureInterface
    {
    public:
        ServerInterfaceGameSearchParamsBase();
        virtual ~ServerInterfaceGameSearchParamsBase();

        // CgsServerInterfaceGameSearchParams.cpp:48
        virtual bool Prepare();

        // CgsServerInterfaceGameSearchParams.cpp:70
        void SerialiseToString(char* lpcRecord, s32 liRecLen) const;

        // CgsServerInterfaceGameSearchParams.h:120
        void SetReturnPlayers(bool lbReturnPlayers) { mbReturnPlayers = lbReturnPlayers; }

        // CgsServerInterfaceGameSearchParams.h:126
        bool ReturnPlayers() const { return mbReturnPlayers; }

    protected:
        // CgsServerInterfaceGameSearchParams.h:83
        virtual u32 GetCustomFlagsMask() const = 0;
        // CgsServerInterfaceGameSearchParams.h:88
        virtual u32 GetCustomFlagsValue() const = 0;

        // CgsServerInterfaceGameSearchParams.h:73
        s32 miNumGames;
        // CgsServerInterfaceGameSearchParams.h:74
        s32 miRoomID;
        // CgsServerInterfaceGameSearchParams.h:75
        u32 muGameFlagsMask;
        // CgsServerInterfaceGameSearchParams.h:76
        u32 muGameFlagsValue;
        // CgsServerInterfaceGameSearchParams.h:78
        bool mbReturnPlayers;
    };
}

#endif // CGS_SERVER_INTERFACE_GAME_SEARCH_PARAMS_H
