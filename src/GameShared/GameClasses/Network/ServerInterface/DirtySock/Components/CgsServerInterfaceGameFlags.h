#ifndef CGS_SERVER_INTERFACE_GAME_FLAGS_H
#define CGS_SERVER_INTERFACE_GAME_FLAGS_H

#include "types.hpp"

// ===========================================================================
// CgsNetwork game-flag bits
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfaceGameFlags.h
//
// The bits of ServerInterfaceGameParamsBase::muGameFlags (+0xEC). The create-game
// path sets KU_GAME_FLAGS_PERSISTENT (SetFixedGame) and SetRankedGame toggles
// KU_GAME_FLAGS_RANKED.
// ===========================================================================

namespace CgsNetwork
{
    const u32 KU_GAME_FLAGS_AUTO_START       = 1;
    const u32 KU_GAME_FLAGS_CHALLENGE        = 2;
    const u32 KU_GAME_FLAGS_PERSISTENT       = 4;
    const u32 KU_GAME_FLAGS_JOIN_HISTORY     = 8;
    const u32 KU_GAME_FLAGS_INDIVIDUAL_RANKS = 16;
    const u32 KU_GAME_FLAGS_LOCKED           = 32;
    const u32 KU_GAME_FLAGS_HOST_MIGRATION   = 64;
    const u32 KU_GAME_FLAGS_AUTO_LOCK        = 128;
    const u32 KU_GAME_FLAGS_PRIVATE          = 256;
    const u32 KU_GAME_FLAGS_QUICK            = 512;
    const u32 KU_GAME_FLAGS_RANKED           = 1024;
    const u32 KU_GAME_FLAGS_STARTED          = 2048;
    const u32 KU_GAME_FLAGS_UNAVAILABLE      = 4096;
    const u32 KU_GAME_FLAGS_USERSETS         = 8192;
}

#endif // CGS_SERVER_INTERFACE_GAME_FLAGS_H
