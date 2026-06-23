// ============================================================================
// b5-decomp/src/GameSource/Network/SharedIO/BrnNetworkOutGameParamsChanged.h
// ============================================================================
// BrnNetwork::BrnNetworkModuleIO::NetworkOutGameParamsChanged -- the network event the
// host broadcasts when the game parameters change (built and queued by
// BrnNetwork::BrnNetworkManager::OutputGameParameters, X360 0x825966A0, which stack-
// allocates one as a 480-byte object and AddEvent()s it as event-type 15, size 480).
//
// No source or DWARF is available for this TU. The SHAPE is recovered from the X360 asm:
//   * Construct @ 0x82581010 -- the reset/init reproduced here.
//   * the caller (OutputGameParameters) frames the object as a 440-byte head region
//     (`_BYTE v9[440]`) followed by named scalar fields, fixing sizeof == 480.
//
// LAYOUT (offsets from `this`):
//   +0x000  maOpaqueHead[0x20]  -- 32-byte head Construct does NOT touch (event
//                                  base/header set up elsewhere by the queue/event base).
//   +0x020  maParams[9]         -- 9 per-route game-parameter entries, 44-byte (0x2C)
//                                  stride (ends at +0x1AC). Construct inits each entry's
//                                  first three words:
//                                    +0x00 mfDefault = 0x39000000 (a float bit-pattern)
//                                    +0x04 mi4       = 0
//                                    +0x08 mi8       = 0
//                                  the entry's remaining 0x20 bytes are left untouched.
//   --- the game-parameter scalar block the event carries (set explicitly below) ---
//   +0x1AC  mfDefaultA          -- 0x39000000 (the 10th Construct loop store)
//   +0x1B0  mi1B0 = 0
//   +0x1B4  mi1B4 = 0
//   +0x1B8  miMaxPlayers        = 0x0A (10)
//   +0x1BC  miGameMode          = 0x12 (18)
//   +0x1C0  mi1C0 = 0
//   +0x1C4  mi1C4 = 0
//   +0x1C8  mi1C8 = 0
//   +0x1CC  mi1CC = 3
//   +0x1D0  mi1D0 = 9
//   +0x1D4  mi1D4 = 3
//   +0x1D8  mi1D8 = 0
//   +0x1DC  mbIsRankedGame = true   (0x1DC..0x1DF set to 1)
//   +0x1DD  mbFlag1D       = true
//   +0x1DE  mbFlag1E       = true
//   +0x1DF  mbFlag1F       = true
//
// The 0x39000000 default written into every entry's first word and into mfDefaultA is
// preserved as the exact 32-bit pattern the X360 stores (`lis r8, 0x3900`), reproduced
// via a typed bit-copy rather than asserting a decimal float value as X360 fact. Field
// names beyond the asm-evident ones (max players / game mode / ranked) are offset-keyed
// (miNNN) where the X360 gives no further hint.
#pragma once

#include "types.hpp"

namespace BrnNetwork
{
namespace BrnNetworkModuleIO
{
    // ------------------------------------------------------------------------
    // One 44-byte (0x2C) game-parameter entry. Construct inits the first three words;
    // the remaining 0x20 bytes are offset-pinned opaque storage (finer field types not
    // recoverable from the reset).
    // ------------------------------------------------------------------------
    struct OutGameParamEntry
    {
        f32 mfDefault;             // +0x00  init to the 0x39000000 bit-pattern
        s32 mi4;                   // +0x04  init 0
        s32 mi8;                   // +0x08  init 0
        u8  maOpaque[0x2C - 0x0C]; // +0x0C  untouched-by-Construct tail (0x20 bytes)
    };
    static_assert(sizeof(OutGameParamEntry) == 0x2C, "OutGameParamEntry stride (0x2C)");

    struct NetworkOutGameParamsChanged
    {
        u8                maOpaqueHead[0x20];  // +0x000  event head (untouched by Construct)
        OutGameParamEntry maParams[9];         // +0x020  9 x 0x2C -> ends at +0x1AC

        f32  mfDefaultA;        // +0x1AC  (10th Construct loop store; 0x39000000)
        s32  mi1B0;             // +0x1B0  = 0
        s32  mi1B4;             // +0x1B4  = 0
        s32  miMaxPlayers;      // +0x1B8  = 10
        s32  miGameMode;        // +0x1BC  = 18
        s32  mi1C0;             // +0x1C0  = 0
        s32  mi1C4;             // +0x1C4  = 0
        s32  mi1C8;             // +0x1C8  = 0
        s32  mi1CC;             // +0x1CC  = 3
        s32  mi1D0;             // +0x1D0  = 9
        s32  mi1D4;             // +0x1D4  = 3
        s32  mi1D8;             // +0x1D8  = 0
        bool mbIsRankedGame;    // +0x1DC  = true
        bool mbFlag1D;          // +0x1DD  = true
        bool mbFlag1E;          // +0x1DE  = true
        bool mbFlag1F;          // +0x1DF  = true

        // @ 0x82581010 -- reset/init (the event's default state).
        void Construct();
    };
    static_assert(sizeof(NetworkOutGameParamsChanged) == 480, "NetworkOutGameParamsChanged size (event-type 15, 480 bytes)");
}
}
