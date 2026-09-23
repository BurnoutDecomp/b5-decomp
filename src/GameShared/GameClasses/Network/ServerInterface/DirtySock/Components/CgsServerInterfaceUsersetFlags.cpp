#include "types.hpp"

// CgsNetwork::ConvertUsersetFlags -- translate a userset flag word between the game's
// userset flag bits and the DirtySock lobby flag bits through the six-row table
// KAU_DIRTYSOCK_FLAGS_TO_USERSET_FLAGS ({ userset flag, DirtySock flag } per row).
// luDirection 0 converts DirtySock flags to userset flags (test column 1, emit column 0);
// 1 converts userset flags to DirtySock flags (test column 0, emit column 1).

namespace CgsNetwork
{
    namespace
    {
        const u32 KU_NUM_USERSET_FLAGS = 6;

        const u32 KAU_DIRTYSOCK_FLAGS_TO_USERSET_FLAGS[KU_NUM_USERSET_FLAGS][2] =
        {
            { 0x01, 0x001000 },
            { 0x02, 0x000100 },
            { 0x04, 0x000800 },
            { 0x08, 0x008000 },
            { 0x10, 0x080000 },
            { 0x20, 0x400000 },
        };
    }

    int ConvertUsersetFlags(int lxFlags, unsigned int luDirection)
    {
        // The column tested is the one not emitted.
        const u32 luFromColumn = (luDirection == 0) ? 1u : 0u;

        u32 luResult = 0;
        for (u32 luFlag = 0; luFlag < KU_NUM_USERSET_FLAGS; ++luFlag)
        {
            const u32 luFrom = KAU_DIRTYSOCK_FLAGS_TO_USERSET_FLAGS[luFlag][luFromColumn];
            if ((luFrom & static_cast<u32>(lxFlags)) == luFrom)
            {
                luResult |= KAU_DIRTYSOCK_FLAGS_TO_USERSET_FLAGS[luFlag][luDirection];
            }
        }
        return static_cast<int>(luResult);
    }
}
