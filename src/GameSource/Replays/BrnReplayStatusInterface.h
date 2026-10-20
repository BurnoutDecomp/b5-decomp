#pragma once

// Canonical home for BrnReplays::ReplayIO::StatusInterface.
// DWARF home: GameSource/Replays/BrnReplayStatusInterface.h:38.
//
// Replay record/playback status interface: a status-flags word, a fixed array of
// named reel slots, the current record/playback reel indices, and a trailing
// playback-seconds-remaining float. Carried BY POINTER through the RaceCarEntity/World/Sound/
// Effects/Gui IO buffers (Get/SetReplayStatusInterface), and assigned by value via
// operator= when an IO buffer snapshots it.
//
// LAYOUT (X360 asm authoritative; full member list grown by this type's own TU):
//   @0x000  uint32_t mxStatusFlags         // DWARF :101
//   @0x004  Reel     maReels[6]            // 6 * sizeof(Reel)(=257) = 0x606 bytes
//   @0x60C  int32_t  miCurrentRecordReel   // DWARF :103
//   @0x610  int32_t  miCurrentPlaybackReel // DWARF :104
//   @0x614  f32      mfPlaybackTimeRemaining // target-only seconds, see below
//   sizeof  == 0x618
//
// X360-vs-DWARF discrepancies (X360 asm wins per project rule):
//   * Reel count: the PS3 DWARF declares Reel maReels[5], but the X360 build uses
//     SIX reels. GetReel/SetReel assert `index < 6` (out-of-range fire at
//     BrnReplayStatusInterface.h:321 / :314), and operator= runs its per-reel copy
//     loop exactly 6 times (asm: `li r10, 6` ... `bne` until predecrement hits 1,
//     stride 0x101). The sibling BrnReplays::ReplayModule likewise carries a reel
//     block; this interface mirrors the X360 module's 6-slot count.
//   * Trailing float: operator= copies the additional float at+0x614.
//     ARTIST8264E56C..8264E5AC produces remaining playback frames, accounting
//     for the1800-frame wrap, multiplied by1/60;8264E624 stores0 in the
//     other original branch. mfPlaybackTimeRemaining describes this attested
//     target-only value; the old debug-HUD-alpha analogy was incorrect.

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
            f32  mfPlaybackTimeRemaining; // @0x614  (X360-only trailing seconds)

            // Original DWARF h:82; native cache consumers read this full word.
            u32 GetStatusFlags() const { return mxStatusFlags; }

            // ARTIST-only target value, produced8264E5AC and copied823A64E8.
            f32 GetPlaybackTimeRemaining() const { return mfPlaybackTimeRemaining; }

            // ---- ledger methods owned by this TU ----

            // GetReel @ 0x824EB0B8 -- returns &maReels[index]; asserts index in
            // [0, 6). (const Reel* in DWARF; const-qualified accessor.)
            const Reel* GetReel(s32 liIndex) const;

            // SetReel @ 0x8264B010 -- memcpy(&maReels[index], pSrc, sizeof(Reel));
            // asserts index in [0, 6).
            void SetReel(s32 liIndex, const Reel* pSrc);

            // operator= @ 0x823A6488 -- member-wise copy: status flags, all 6 reels
            // (bounded name copy + used flag), the two current-reel indices, and the
            // trailing playback time remaining.
            StatusInterface& operator=(const StatusInterface& rOther);
        };
    }
}
