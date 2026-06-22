#pragma once

// Canonical home for BrnReplays::ReplayIO::StatusInterface.
// DWARF home: GameSource/Replays/BrnReplayStatusInterface.h:38.
//
// Replay record/playback status interface: a status-flags word, a fixed array of
// named reel slots, the current record/playback reel indices, and a trailing
// debug-HUD alpha float. Carried BY POINTER through the RaceCarEntity/World/Sound/
// Effects/Gui IO buffers (Get/SetReplayStatusInterface), and assigned by value via
// operator= when an IO buffer snapshots it.
//
// LAYOUT (X360 asm authoritative; full member list grown by this type's own TU):
//   @0x000  uint32_t mxStatusFlags         // DWARF :101
//   @0x004  Reel     maReels[6]            // 6 * sizeof(Reel)(=257) = 0x606 bytes
//   @0x60C  int32_t  miCurrentRecordReel   // DWARF :103
//   @0x610  int32_t  miCurrentPlaybackReel // DWARF :104
//   @0x614  f32      mfDebugHudAlpha       // see X360-vs-DWARF note below
//   sizeof  == 0x618
//
// X360-vs-DWARF discrepancies (X360 asm wins per project rule):
//   * Reel count: the PS3 DWARF declares Reel maReels[5], but the X360 build uses
//     SIX reels. GetReel/SetReel assert `index < 6` (out-of-range fire at
//     BrnReplayStatusInterface.h:321 / :314), and operator= runs its per-reel copy
//     loop exactly 6 times (asm: `li r10, 6` ... `bne` until predecrement hits 1,
//     stride 0x101). The sibling BrnReplays::ReplayModule likewise carries a reel
//     block; this interface mirrors the X360 module's 6-slot count.
//   * Trailing float: operator= copies an additional 4-byte float at +0x614 via
//     lfs/stfs (X360 0x823A64E0/0x823A64E8) that the PS3 DWARF member list does not
//     enumerate. It is named mfDebugHudAlpha here to mirror the analogous trailing
//     float (BrnReplayModule.h:186 float32_t mfDebugHudAlpha) that follows the reel
//     block in the sibling ReplayModule layout. HONEST best-effort name -- the
//     field's presence/offset are asm fact; the semantic name is inferred.

#include "types.hpp"
#include "GameSource/Replays/BrnReplayReels.h"

namespace BrnReplays
{
    namespace ReplayIO
    {
        // DWARF: BrnReplayStatusInterface.h:38
        struct StatusInterface
        {
            // ---- layout ----
            u32  mxStatusFlags;          // @0x000
            Reel maReels[6];             // @0x004  (X360: 6 slots, asm authoritative)
            s32  miCurrentRecordReel;    // @0x60C
            s32  miCurrentPlaybackReel;  // @0x610
            f32  mfDebugHudAlpha;        // @0x614  (X360-only trailing float)

            // ---- ledger methods owned by this TU ----

            // GetReel @ 0x824EB0B8 -- returns &maReels[index]; asserts index in
            // [0, 6). (const Reel* in DWARF; const-qualified accessor.)
            const Reel* GetReel(s32 liIndex) const;

            // SetReel @ 0x8264B010 -- memcpy(&maReels[index], pSrc, sizeof(Reel));
            // asserts index in [0, 6).
            void SetReel(s32 liIndex, const Reel* pSrc);

            // operator= @ 0x823A6488 -- member-wise copy: status flags, all 6 reels
            // (bounded name copy + used flag), the two current-reel indices, and the
            // trailing debug alpha.
            StatusInterface& operator=(const StatusInterface& rOther);
        };
    }
}
