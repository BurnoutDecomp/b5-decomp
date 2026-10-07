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
    static_assert(offsetof(StreamOffset, miFrameNumber) == 0x00, "64-bit frame @0x00");
    static_assert(offsetof(StreamOffset, miFileOffset)  == 0x08, "file byte offset @0x08");
    static_assert(offsetof(StreamOffset, miFrunkSize)   == 0x0C, "frunk byte size @0x0C");
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

    // Frunk 0 overwrites [0x20000,0x3FFFF]; frame ids are independent of disk ranges.
    laOffsets[0].miFrameNumber = 0x100000001LL;
    laOffsets[0].miFileOffset  = 0x20000;
    laOffsets[0].miFrunkSize   = 0x20000;
    laOffsets[0].mxFlags       = BrnReplays::KU_FLAG_KEYFRAME;
    // Frunk 1's bytes are overwritten even though its frame number is different.
    laOffsets[1].miFrameNumber = 0x200000001LL;
    laOffsets[1].miFileOffset  = 0x30000;
    laOffsets[1].miFrunkSize   = 0x10000;
    laOffsets[1].mxFlags       = 0;
    // Frunk 2 begins just beyond the written disk range and survives.
    laOffsets[2].miFrameNumber = 7;
    laOffsets[2].miFileOffset  = 0x40000;
    laOffsets[2].miFrunkSize   = 0x10000;
    laOffsets[2].mxFlags       = BrnReplays::KU_FLAG_KEYFRAME;
    laOffsets[3].miFrameNumber = 8;
    laOffsets[3].miFileOffset  = 0x50000;
    laOffsets[3].miFrunkSize   = 0x10000;
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
