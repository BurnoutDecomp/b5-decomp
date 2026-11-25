#pragma once

// Game-wide constants/enums. Recovered from the DecFIGS DWARF (BurnoutConstants.h).
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (range-guarded operator++)

// ARTIST's postfix index guards return the old index after a failed assert.
// Matching builds preserve the original filename and lines at 0x821F1DF4
// and 0x8232855C, including copies inlined into player and mode handling.
#define KAC_BURNOUT_CONSTANTS_ARTIST_FILE "..\\..\\..\\GameSource\\BurnoutConstants.h"

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
inline EActiveRaceCarIndex operator++(EActiveRaceCarIndex& leEnumIndex, int)
{
    const EActiveRaceCarIndex leOldEnumIndex = leEnumIndex;
    leEnumIndex = static_cast<EActiveRaceCarIndex>(static_cast<s32>(leEnumIndex) + 1);
    CGS_ASSERT_AT(leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT,
                  "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT",
                  KAC_BURNOUT_CONSTANTS_ARTIST_FILE, 39);
    return leOldEnumIndex;
}

// Global slot index of a race car across all modes (the player + up to 34 rivals/traffic
// racers tracked by the global output interface), or INVALID. DWARF: 0..34, COUNT = 35.
enum EGlobalRaceCarIndex : s32
{
    E_GLOBAL_RACE_CAR_INDEX_INVALID = -1,
    E_GLOBAL_RACE_CAR_INDEX_0       = 0,
    E_GLOBAL_RACE_CAR_INDEX_1       = 1,
    E_GLOBAL_RACE_CAR_INDEX_2       = 2,
    E_GLOBAL_RACE_CAR_INDEX_3       = 3,
    E_GLOBAL_RACE_CAR_INDEX_4       = 4,
    E_GLOBAL_RACE_CAR_INDEX_5       = 5,
    E_GLOBAL_RACE_CAR_INDEX_6       = 6,
    E_GLOBAL_RACE_CAR_INDEX_7       = 7,
    E_GLOBAL_RACE_CAR_INDEX_8       = 8,
    E_GLOBAL_RACE_CAR_INDEX_9       = 9,
    E_GLOBAL_RACE_CAR_INDEX_10      = 10,
    E_GLOBAL_RACE_CAR_INDEX_11      = 11,
    E_GLOBAL_RACE_CAR_INDEX_12      = 12,
    E_GLOBAL_RACE_CAR_INDEX_13      = 13,
    E_GLOBAL_RACE_CAR_INDEX_14      = 14,
    E_GLOBAL_RACE_CAR_INDEX_15      = 15,
    E_GLOBAL_RACE_CAR_INDEX_16      = 16,
    E_GLOBAL_RACE_CAR_INDEX_17      = 17,
    E_GLOBAL_RACE_CAR_INDEX_18      = 18,
    E_GLOBAL_RACE_CAR_INDEX_19      = 19,
    E_GLOBAL_RACE_CAR_INDEX_20      = 20,
    E_GLOBAL_RACE_CAR_INDEX_21      = 21,
    E_GLOBAL_RACE_CAR_INDEX_22      = 22,
    E_GLOBAL_RACE_CAR_INDEX_23      = 23,
    E_GLOBAL_RACE_CAR_INDEX_24      = 24,
    E_GLOBAL_RACE_CAR_INDEX_25      = 25,
    E_GLOBAL_RACE_CAR_INDEX_26      = 26,
    E_GLOBAL_RACE_CAR_INDEX_27      = 27,
    E_GLOBAL_RACE_CAR_INDEX_28      = 28,
    E_GLOBAL_RACE_CAR_INDEX_29      = 29,
    E_GLOBAL_RACE_CAR_INDEX_30      = 30,
    E_GLOBAL_RACE_CAR_INDEX_31      = 31,
    E_GLOBAL_RACE_CAR_INDEX_32      = 32,
    E_GLOBAL_RACE_CAR_INDEX_33      = 33,
    E_GLOBAL_RACE_CAR_INDEX_34      = 34,
    E_GLOBAL_RACE_CAR_INDEX_COUNT   = 35
};

inline EGlobalRaceCarIndex operator++(EGlobalRaceCarIndex& leEnumIndex, int)
{
    const EGlobalRaceCarIndex leOldEnumIndex = leEnumIndex;
    leEnumIndex = static_cast<EGlobalRaceCarIndex>(static_cast<s32>(leEnumIndex) + 1);
    CGS_ASSERT_AT(leEnumIndex <= E_GLOBAL_RACE_CAR_INDEX_COUNT,
                  "leEnumIndex <= E_GLOBAL_RACE_CAR_INDEX_COUNT",
                  KAC_BURNOUT_CONSTANTS_ARTIST_FILE, 84);
    return leOldEnumIndex;
}

#undef KAC_BURNOUT_CONSTANTS_ARTIST_FILE

namespace BrnGameState
{
    // DWARF BurnoutConstants.h:91 -- lifecycle of a freeburn challenge. COUNT (7)
    // doubles as the "no status / uninitialised" sentinel that network messages
    // construct the field with (e.g. BrnNetwork::FreeburnChallengeMessage).
    enum EChallengeStatus
    {
        E_CHALLENGE_STATUS_ONGOING                      = 0,
        E_CHALLENGE_STATUS_SUCCESS                      = 1,
        E_CHALLENGE_STATUS_ABORTED                      = 2,
        E_CHALLENGE_STATUS_ABORTED_DUE_TO_PLAYER_LEAVE  = 3,
        E_CHALLENGE_STATUS_ABORTED_BEFORE_STARTING      = 4,
        E_CHALLENGE_STATUS_RESET_IF_NEEDED              = 5,
        E_CHALLENGE_STATUS_FAILURE                      = 6,
        E_CHALLENGE_STATUS_COUNT                        = 7,
    };
} // namespace BrnGameState
