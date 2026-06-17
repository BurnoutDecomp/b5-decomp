#pragma once

// MINIMAL SLICE for the RaceCarEntityModuleIO IO-buffer unlock; full layout
// reconstructed by BrnReplays::ReplayIO::StatusInterface's own TU (DWARF home
// GameSource/Replays/BrnReplayStatusInterface.h:38). Size 256 (NOMINAL -- not
// byte-verified, grown by own TU).
//
// Opaque sub-interface payload referenced by the RaceCarEntityModuleIO::
// InputBuffer_PreScene buffer (the buffer typedefs RaceCarEntityModuleIO::
// StatusInterface as ReplayStatusInterface and holds it BY POINTER via
// mpReplayStatusInterface, IO header :260; Get/SetReplayStatusInterface only pass
// the pointer through). A forward declaration would satisfy the by-pointer use, but
// a complete sized type is provided here so the canonical home is real.
//
// DWARF (BrnReplayStatusInterface.h:38) gives the full member list:
//   uint32_t mxStatusFlags;        // :101
//   Reel     maReels[5];           // :102
//   int32_t  miCurrentRecordReel;  // :103
//   int32_t  miCurrentPlaybackReel;// :104
// The Reel element type is a Replays-internal cascade, so collapsed to an opaque
// reserved blob here; the real members land with this type's own ledger TU.
//
// Natural alignment: no Vector*/Matrix*/SIMD or EventQueue member in the DWARF.

#include "types.hpp"   // reserved blob

namespace BrnReplays
{
    namespace ReplayIO
    {
        // DWARF: BrnReplayStatusInterface.h:38
        // (struct BrnReplays::ReplayIO::StatusInterface). Replay record/playback
        // status flags + per-reel state.
        struct StatusInterface
        {
            unsigned char maReserved[256]; // NOMINAL -- full layout grown by own TU
        };
    }
}
