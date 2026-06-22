#pragma once

// MINIMAL SLICE of the BrnReplays shared replay enums/constants.
// DWARF home: GameSource/Replays/BrnReplayShared.h:96.
//
// Carries just the ESerialiserId enum needed by BaseSerialiser::GetId() and the
// RequestInterface TU. The full set of replay shared constants (stream buffer
// sizes, frunk counts, ESerialiserContext, etc.) is reconstructed by the replay
// shared TU -- GROW this canonical home additively when that lands; do NOT fork it.

#include "types.hpp"

namespace BrnReplays
{
    // DWARF: BrnReplayShared.h:97
    //
    // X360-vs-DWARF discrepancy (X360 asm wins): the Feb-2007 leak declares
    // E_ID_COUNT == 5 (five named ids below). The X360 build's RequestInterface
    // range-asserts serialiser ids against ELEVEN (RegisterSerialiser @0x821F34A0:
    // `cmpwi r11, 0xB`) and its Append loop runs 11 times -- so E_ID_COUNT == 11 on
    // X360. The six ids between E_ID_GAME_MODULE and E_ID_COUNT are X360 additions
    // that are NOT named in any recovered source; they are left as HONEST unnamed
    // gaps (E_ID_X360_RESERVED_5..10) rather than fabricated as X360 fact.
    enum ESerialiserId
    {
        E_ID_RACECAR_ENTITY  = 0,
        E_ID_RACECAR_DEBUG   = 1,
        E_ID_DEBUG_DRAW      = 2,
        E_ID_DIRECTOR_BRIDGE = 3,
        E_ID_GAME_MODULE     = 4,

        // --- unnamed X360-only ids (names unrecovered; presence is asm fact) ---
        E_ID_X360_RESERVED_5  = 5,
        E_ID_X360_RESERVED_6  = 6,
        E_ID_X360_RESERVED_7  = 7,
        E_ID_X360_RESERVED_8  = 8,
        E_ID_X360_RESERVED_9  = 9,
        E_ID_X360_RESERVED_10 = 10,

        E_ID_COUNT = 11, // X360 (leak: 5)
    };

    // DWARF: BrnReplayShared.h:57. The replay-time vs debug-time context a
    // serialiser runs in. Recovered fully from the Feb-2007 leak.
    enum ESerialiserContext
    {
        E_CONTEXT_NORMAL = 0,
        E_CONTEXT_DEBUG  = 1,
        E_CONTEXT_COUNT  = 2,
    };

    // DWARF: BrnReplayShared.h:135-150. Replay stream sizing constants (recovered
    // from the Feb-2007 leak). GROW this set as more replay TUs land. KI_MAX_FRUNKS
    // is the size of the WriteStream/ReadStream frunk-index ring -- the X360
    // InvalidateFrunksAhead @0x8264D190 wraps its ring cursor with `i % 1800`,
    // matching this value.
    static const s32 KI_MAX_FRUNKS = 1800; // BrnReplayShared.h:138
}
