#include "GameSource/Replays/Stream/BrnReplayWriteStream.h"

#include "GameSource/Replays/Stream/BrnReplayStreamHeader.h"
#include "GameSource/Replays/BrnReplayShared.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX (BrnReplays::WriteStream).
//
//   InvalidateFrunksAhead @ 0x8264D190
//
// When a frunk is (re)recorded at index liStartFrunk, any frunks that were already
// in the stream for the frames it now covers are stale. This walks forward marking
// those overlapped frunks VOID, then trims the leading edge of the live ring up to
// the first surviving keyframe so playback resumes from a self-contained frame.

namespace BrnReplays
{
    // @ 0x8264D190
    void WriteStream::InvalidateFrunksAhead(s32 liStartFrunk)
    {
        StreamHeader* lpHeader = mpStreamHeader;
        StreamOffset* lpOffsets = lpHeader->mpFrameOffsets;

        // --- pass 1: void every later frunk whose frames the new frunk now owns ---
        // The new frunk is only meaningful if it actually spans frames (frame count
        // non-zero); the X360 body guards on exactly that before scanning forward.
        if (lpOffsets[liStartFrunk].miFrameCount != 0)
        {
            const s32 liDataMin = lpOffsets[liStartFrunk].miFrameNumber;
            const s32 liDataMax =
                lpOffsets[liStartFrunk].miFrameCount + liDataMin - 1;

            for (s32 liIndex = liStartFrunk + 1;
                 liIndex < lpHeader->miNumFrunks;
                 ++liIndex)
            {
                // Frunks are ordered by frame; once one starts past the range the
                // new frunk covers, nothing further overlaps.
                if (lpOffsets[liIndex].miFrameNumber > liDataMax)
                    break;

                lpOffsets[liIndex].mxFlags |= KU_FLAG_VOID;

                // (re-read of mpStreamHeader each iteration in the X360 body is
                // elided: lpHeader/lpOffsets are loop-invariant here.)
            }
        }

        // --- pass 2: advance the ring start past leading voided non-keyframes ---
        // Walk the ring from the current first frunk, counting frunks that are either
        // voided or not a keyframe, and stop at the first live keyframe (a frame the
        // stream can be replayed from). Everything skipped is dropped from the count.
        s32 liDropped = 0;
        const s32 liNumFrunks = lpHeader->miNumFrunks;
        if (liNumFrunks > 0)
        {
            s32 liFrunkIndex = lpHeader->miFirstFrunk;
            for (;; ++liFrunkIndex)
            {
                const u16 luFlags =
                    lpOffsets[liFrunkIndex % KI_MAX_FRUNKS].mxFlags;

                if ((luFlags & KU_FLAG_VOID) == 0 && (luFlags & KU_FLAG_KEYFRAME) != 0)
                    break;

                if (++liDropped >= liNumFrunks)
                    return;
            }

            lpHeader->miFirstFrunk = liFrunkIndex % KI_MAX_FRUNKS;
            lpHeader->miNumFrunks -= liDropped;
        }
    }
}
