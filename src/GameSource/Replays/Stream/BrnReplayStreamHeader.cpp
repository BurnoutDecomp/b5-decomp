#include "GameSource/Replays/Stream/BrnReplayStreamHeader.h"

#include "GameSource/Replays/BrnReplayShared.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX (BrnReplays::StreamHeader).
//
//   ChopOffTailFrames @ 0x8264B270
//
// While recording, the live ring holds miNumFrunks frunks starting at miFirstFrunk.
// Some trailing frunks may be voided or be non-keyframes that the stream cannot be
// independently replayed from. This scans backwards from the last frunk toward the
// first, dropping every tail frunk until it finds one that is a live keyframe (not
// VOID and KEYFRAME set), then sets miNumFrunks to keep frunks up to and including
// that keyframe. If no live keyframe is found the count collapses to zero.

namespace BrnReplays
{
    // @ 0x8264B270
    StreamHeader* StreamHeader::ChopOffTailFrames()
    {
        // X360 indexes the ring by absolute (un-wrapped) frunk position liLast and
        // wraps only when reading the offset entry: (miFirstFrunk + liLast) % ring.
        s32 liLast = miNumFrunks - 1;
        if (liLast >= 0)
        {
            s32 liRingPos = miFirstFrunk + liLast;
            do
            {
                const u16 luFlags =
                    mpFrameOffsets[liRingPos % KI_MAX_FRUNKS].mxFlags;

                // Stop at the first live keyframe: a frame the stream can resume from.
                if ((luFlags & KU_FLAG_VOID) == 0 && (luFlags & KU_FLAG_KEYFRAME) != 0)
                    break;

                --liLast;
                --liRingPos;
            }
            while (liLast >= 0);
        }

        // Keep frunks [0 .. liLast]; everything after the surviving keyframe is dropped
        // (liLast == -1 when nothing survives -> count becomes 0).
        miNumFrunks = liLast + 1;
        return this;
    }
}
