#pragma once

// Takedown classification. Recovered from the DecFIGS DWARF; minimal (the values the
// X360 spine exposes for the race-car crash event).
#include "types.hpp"

namespace BrnGameState
{
    enum ETakedownType : s32
    {
        E_TYPE_B3CLASSIC = 0,
        E_NUM_TYPES      = 1
    };
}
