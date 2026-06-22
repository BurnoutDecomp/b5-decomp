#pragma once

// Reconstructed canonical home for BrnReplays::Reel.
// DWARF home: GameSource/Replays/BrnReplayReels.h:42 (struct BrnReplays::Reel).
//
//   char macName[256];   // BrnReplayReels.h:44
//   bool mbUsed;         // BrnReplayReels.h:45
//
// A "reel" is a named replay-recording slot: a fixed 256-byte name buffer plus a
// used/unused flag. sizeof(Reel) == 257 on the X360 build (no trailing padding;
// macName is char[256] at offset 0, mbUsed is a 1-byte bool at offset 256). This
// 257-byte stride is load-bearing: BrnReplays::ReplayIO::StatusInterface stores an
// array of Reel and its GetReel/SetReel/operator= bodies stride the array by
// sizeof(Reel) (verified against the X360 asm: 257 == 0x101).

namespace BrnReplays
{
    // DWARF: BrnReplayReels.h:42
    struct Reel
    {
        char macName[256]; // BrnReplayReels.h:44 -- NUL-terminated reel name
        bool mbUsed;       // BrnReplayReels.h:45 -- slot in use
    };
}
