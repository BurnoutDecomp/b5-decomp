#pragma once

// Race-car category. Recovered from the DecFIGS DWARF (BrnRaceCarType.h).
#include "types.hpp"

namespace BrnWorld
{
    enum ERaceCarType : s32
    {
        E_RACE_CAR_TYPE_PLAYER   = 0,
        E_RACE_CAR_TYPE_AI       = 1,
        E_RACE_CAR_TYPE_NETWORK  = 2,
        E_RACE_CAR_TYPE_INACTIVE = 3,
        E_RACE_CAR_TYPE_COUNT    = 4
    };
}
