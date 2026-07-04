#include "GameSource/Replays/Stream/BrnReplayWriteStream.h"

#include "GameSource/Replays/Stream/BrnReplayStreamHeader.h"
#include "GameSource/Replays/BrnReplayShared.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX (BrnReplays::WriteStream).
//
//   ResetStream            @ 0x8264D100
//   InvalidateFrunksAhead  @ 0x8264D190
//
// When a frunk is (re)recorded at index liStartFrunk, any frunks that were already
// in the stream for the frames it now covers are stale. This walks forward marking
// those overlapped frunks VOID, then trims the leading edge of the live ring up to
// the first surviving keyframe so playback resumes from a self-contained frame.

namespace BrnReplays
{
    // @ 0x8264D100
    // Reset the write stream to an empty, freshly-allocated header. Frees every
    // allocation the header allocator has made, carves a new StreamHeader out of it,
    // stamps the "REPLAY " magic (through the terminating NUL) and zeroes the header's
    // frunk index, then clears the per-recording bookkeeping (write cursor, stall/alloc
    // counters, ended/paused flags, file + intermediate-buffer positions). The
    // DiskWriteStream link (mpStream @0x50) is intentionally left untouched --
    // StartNewStream owns it. Returns the freshly allocated StreamHeader (X360 r3).
    StreamHeader* WriteStream::ResetStream()
    {
        mHeaderMalloc.FreeAll();

        StreamHeader* lpHeader =
            static_cast<StreamHeader*>(mHeaderMalloc.Malloc(sizeof(StreamHeader)));
        mpStreamHeader = lpHeader;

        // Stamp the 8-byte magic ("REPLAY " + NUL). The X360 body is a byte copy that
        // runs through and including the terminating NUL of the rodata literal.
        static const char KacReplayMagic[8] = { 'R', 'E', 'P', 'L', 'A', 'Y', ' ', '\0' };
        for (s32 liByte = 0; ; ++liByte)
        {
            lpHeader->macMagicNumber[liByte] = KacReplayMagic[liByte];
            if (KacReplayMagic[liByte] == '\0')
                break;
        }

        lpHeader->miVersion      = 0;
        lpHeader->miNumFrunks    = 0;
        lpHeader->miFirstFrunk   = 0;
        lpHeader->mpFrameOffsets = nullptr;

        miCurrentWriteIndex  = 0; // @0x40
        miStallCount         = 0; // @0x44
        mbEnded              = false; // @0x48
        mbPaused             = false; // @0x49
        miNumFrunksAllocated = 0; // @0x4C
        miFilePosition       = 0; // @0x54  (mpStream @0x50 deliberately untouched)
        miBufferPosition     = 0; // @0x58

        return lpHeader;
    }

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
