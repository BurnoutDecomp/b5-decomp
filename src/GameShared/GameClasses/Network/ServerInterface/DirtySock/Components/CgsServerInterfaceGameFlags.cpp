#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGameFlags.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfoData.h" // EConversionFlags

// Translate a game-flag word between the game's own bit layout (column 0) and the lobby's
// game-record system flags (column 1) through a fixed 14-row pair table: every row whose
// source-side bits are all set in the input contributes its destination-side bits.

namespace CgsNetwork
{
    namespace
    {
        const u32 KU_NUM_FLAGS = 14;

        // { game flag, DirtySock game-record flag } (the console .rdata table).
        const u32 KAU_DIRTYSOCK_FLAGS_TO_GAME_FLAGS[KU_NUM_FLAGS][2] =
        {
            { KU_GAME_FLAGS_AUTO_START,       0x00000002u },
            { KU_GAME_FLAGS_CHALLENGE,        0x00000008u },
            { KU_GAME_FLAGS_PERSISTENT,       0x00000040u },
            { KU_GAME_FLAGS_JOIN_HISTORY,     0x00000100u },
            { KU_GAME_FLAGS_INDIVIDUAL_RANKS, 0x00000200u },
            { KU_GAME_FLAGS_LOCKED,           0x00001000u },
            { KU_GAME_FLAGS_HOST_MIGRATION,   0x00002000u },
            { KU_GAME_FLAGS_AUTO_LOCK,        0x00008000u },
            { KU_GAME_FLAGS_PRIVATE,          0x00010000u },
            { KU_GAME_FLAGS_QUICK,            0x00020000u },
            { KU_GAME_FLAGS_RANKED,           0x00040000u },
            { KU_GAME_FLAGS_STARTED,          0x00080000u },
            { KU_GAME_FLAGS_UNAVAILABLE,      0x00200000u },
            { KU_GAME_FLAGS_USERSETS,         0x04000000u },
        };
    }

    // leConvertTo selects the destination column; the other column is the source.
    u32 ConvertFlags(u32 luFlags, EConversionFlags leConvertTo)
    {
        const EConversionFlags leConvertFrom =
            (leConvertTo == E_CONVERSION_FROM_WIRE) ? E_CONVERSION_TO_WIRE : E_CONVERSION_FROM_WIRE;

        u32 luNewFlags = 0;
        for (u32 luIndex = 0; luIndex < KU_NUM_FLAGS; ++luIndex)
        {
            const u32 luFromBits = KAU_DIRTYSOCK_FLAGS_TO_GAME_FLAGS[luIndex][leConvertFrom];
            if ((luFromBits & luFlags) == luFromBits)
            {
                luNewFlags |= KAU_DIRTYSOCK_FLAGS_TO_GAME_FLAGS[luIndex][leConvertTo];
            }
        }
        return luNewFlags;
    }
}
