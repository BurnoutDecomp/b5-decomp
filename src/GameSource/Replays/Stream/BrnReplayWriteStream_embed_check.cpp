// Embed check: drive BrnReplays::WriteStream::InvalidateFrunksAhead over a small
// hand-built frunk ring and confirm the void/trim bookkeeping matches the X360 body.
#include "GameSource/Replays/Stream/BrnReplayWriteStream.h"
#include "GameSource/Replays/Stream/BrnReplayStreamHeader.h"
#include "GameSource/Replays/BrnReplayShared.h"

#include <cstring>
#include <cstddef>

using BrnReplays::WriteStream;
using BrnReplays::StreamHeader;
using BrnReplays::StreamOffset;

namespace
{
    // X360-attested sizes/offsets the bodied function relies on.
    static_assert(sizeof(StreamOffset) == 24, "X360 StreamOffset stride is 24 bytes");
    static_assert(offsetof(StreamOffset, miFrameNumber) == 0x08, "miFrameNumber @0x08");
    static_assert(offsetof(StreamOffset, miFrameCount)  == 0x0C, "miFrameCount @0x0C");
    static_assert(offsetof(StreamOffset, mxFlags)       == 0x14, "mxFlags @0x14");
    static_assert(offsetof(StreamHeader, miNumFrunks)   == 0x0C, "miNumFrunks @0x0C");
    static_assert(offsetof(StreamHeader, miFirstFrunk)  == 0x10, "miFirstFrunk @0x10");

    // Test shim: WriteStream's construction path lives in another TU, so a derived
    // class wires up the StreamHeader the bodied function reads (by name).
    struct TestWriteStream : public WriteStream
    {
        explicit TestWriteStream(StreamHeader* lpHeader)
        {
            std::memset(this, 0, sizeof(WriteStream));
            SetStreamHeaderForTest(lpHeader);
        }
    };
}

int BrnReplayWriteStream_embed_check()
{
    StreamOffset laOffsets[BrnReplays::KI_MAX_FRUNKS];
    std::memset(laOffsets, 0, sizeof(laOffsets));

    StreamHeader lHeader;
    std::memset(&lHeader, 0, sizeof(lHeader));
    lHeader.miNumFrunks    = 4;
    lHeader.miFirstFrunk   = 0;
    lHeader.mpFrameOffsets = laOffsets;

    // frunk 0: keyframe at frame 0 spanning 2 frames [0..1]
    laOffsets[0].miFrameNumber = 0;
    laOffsets[0].miFrameCount  = 2;
    laOffsets[0].mxFlags       = BrnReplays::KU_FLAG_KEYFRAME;
    // frunk 1: at frame 1 (inside frunk 0's covered range -> must be voided)
    laOffsets[1].miFrameNumber = 1;
    laOffsets[1].miFrameCount  = 1;
    laOffsets[1].mxFlags       = 0;
    // frunk 2: keyframe at frame 2 (just past the range -> survives)
    laOffsets[2].miFrameNumber = 2;
    laOffsets[2].miFrameCount  = 1;
    laOffsets[2].mxFlags       = BrnReplays::KU_FLAG_KEYFRAME;
    // frunk 3: at frame 3
    laOffsets[3].miFrameNumber = 3;
    laOffsets[3].miFrameCount  = 1;
    laOffsets[3].mxFlags       = 0;

    TestWriteStream lStream(&lHeader);
    lStream.InvalidateFrunksAhead(0);

    // frunk 1 must now be VOID; frunk 2 must be untouched.
    if ((laOffsets[1].mxFlags & BrnReplays::KU_FLAG_VOID) == 0)
        return 1;
    if ((laOffsets[2].mxFlags & BrnReplays::KU_FLAG_VOID) != 0)
        return 1;

    // The ring's first frunk (0) is a live keyframe, so the trim pass keeps it.
    if (lHeader.miFirstFrunk != 0)
        return 1;
    if (lHeader.miNumFrunks != 4)
        return 1;

    return 0;
}
