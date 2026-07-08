#pragma once

// BrnReplays::RaceCarSerialiserFrame -- the per-frame snapshot the race-car-entity
// replay serialiser records/plays back. Unlike the traffic/prop frames, the X360
// RaceCarEntitySerialiser does NOT walk this frame field-by-field: Read/Write hand the
// whole 0xD7E0-byte frame region to the raw BaseSerialiser::Read/Write byte primitives
// (see BrnReplayRaceCarEntitySerialiser.cpp). No DWARF or Feb-2007 source was recovered
// for this type, so it is modelled as an opaque fixed-size serialised blob -- the only
// attested surface is its SIZE and the Clear() method CheckStaticLayoutCleared drives.
//
// X360 attestation for the frame SIZE:
//   * The static layout buffer is 0x1B000 (110592) bytes (RaceCarEntitySerialiser::
//     Construct @0x8264C4C0, `lis 3,1`|`lis 4,1` -> 0x1B000 static + dynamic).
//   * It holds TWO RaceCarSerialiserFrame copies -- the "previous" frame at static-base
//     +0 and the "live" frame at static-base +0xD7E0 (55264). Read @0x8264C518 / Write
//     @0x8264C618 serialise the live frame with size r5 = 0xD7E0; CheckStaticLayoutCleared
//     @0x82657A48 Clear()s the +0xD7E0 frame then the +0 frame. So each frame is exactly
//     0xD7E0 bytes and 2*0xD7E0 + 0x40-byte tail == 0x1B000.

#include "types.hpp"

namespace BrnReplays
{
    // One race-car-serialiser frame. Size 0xD7E0 (X360-attested). Its interior layout is
    // not recovered; Read/Write treat it as a raw serialised byte blob (external serialised
    // data), so it is held as opaque storage. GROW this home into named fields (do not
    // redefine it) when the RaceCarSerialiserFrame TU family is reconstructed.
    static const u32 KU_RACECAR_FRAME_SIZE = 0xD7E0; // 55264

    struct RaceCarSerialiserFrame
    {
        u8 maFrameData[KU_RACECAR_FRAME_SIZE]; // @0x0000 undecoded serialised frame state

        // Clear @0x826C????  (BrnReplays::RaceCarSerialiserFrame::Clear) -- reset the frame
        // to its empty state. Declared here for the compile-only gate; the body lives in the
        // frame's own [todo] TU. Return type modelled as RaceCarSerialiserFrame* to match the
        // sibling TrafficSerialiserFrame::Clear (which returns the frame) and the X360 call
        // site, where Clear's return is threaded into r3 as CheckStaticLayoutCleared's result.
        RaceCarSerialiserFrame* Clear();
    };

    // The static-layout buffer GetStaticLayout returns: a "previous" frame followed by the
    // "live" frame, plus a 64-byte tail. Sized to the 0x1B000 Construct argument.
    struct RaceCarEntitySerialiserStaticLayout
    {
        RaceCarSerialiserFrame mPreviousFrame; // @0x0000  Clear()ed second by CheckStaticLayoutCleared
        RaceCarSerialiserFrame mLiveFrame;     // @0xD7E0  serialised by Read/Write; Clear()ed first
        u8 maTail[0x1B000 - 2 * KU_RACECAR_FRAME_SIZE]; // @0x1AFC0  64-byte undecoded tail
    };

    // Pin the X360-attested sizes so the +0xD7E0 live-frame offset and the 0x1B000 buffer
    // size are exact.
    static_assert(sizeof(RaceCarSerialiserFrame) == 0xD7E0,
                  "RaceCarSerialiserFrame must be 0xD7E0 bytes (X360 Read/Write size arg)");
    static_assert(sizeof(RaceCarEntitySerialiserStaticLayout) == 0x1B000,
                  "RaceCarEntitySerialiserStaticLayout must be 0x1B000 bytes (Construct arg)");
}
