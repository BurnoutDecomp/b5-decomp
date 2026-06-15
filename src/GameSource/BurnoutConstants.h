#pragma once

// Game-wide constants/enums. Recovered from the DecFIGS DWARF (BurnoutConstants.h).
#include "types.hpp"

// Slot index of an active race car (the local player + rivals), or INVALID.
enum EActiveRaceCarIndex : s32
{
    E_ACTIVE_RACE_CAR_INDEX_INVALID = -1,
    E_ACTIVE_RACE_CAR_INDEX_0       = 0,
    E_ACTIVE_RACE_CAR_INDEX_1       = 1,
    E_ACTIVE_RACE_CAR_INDEX_2       = 2,
    E_ACTIVE_RACE_CAR_INDEX_3       = 3,
    E_ACTIVE_RACE_CAR_INDEX_4       = 4,
    E_ACTIVE_RACE_CAR_INDEX_5       = 5,
    E_ACTIVE_RACE_CAR_INDEX_6       = 6,
    E_ACTIVE_RACE_CAR_INDEX_7       = 7,
    E_ACTIVE_RACE_CAR_INDEX_COUNT   = 8
};
