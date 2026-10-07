#pragma once

// BrnReplays::StreamOffset / StreamHeader -- the per-frunk index entry and the
// stream file header that index the frames ("frunks") of a recorded replay stream.
// DWARF home: GameSource/Replays/Stream/BrnReplayStreamHeader.h:45 / :63.
//
// MINIMAL SLICE created for the WriteStream TU's InvalidateFrunksAhead. Homes the
// two index structs plus the void/keyframe flag bits. The header member functions
// (Clear/FixUp/FixDown/IsFrunkInStream) are declared by the full StreamHeader TU --
// GROW this home additively when that lands; do NOT fork it.

#include "types.hpp"

namespace BrnReplays
{
    // DWARF: BrnReplayStreamHeader.h:33-34. Per-frunk index flag bits.
    static const u16 KU_FLAG_VOID     = 1; // frunk superseded -> skip on playback
    static const u16 KU_FLAG_KEYFRAME = 2; // frunk is a full-state keyframe

    // DWARF: BrnReplayStreamHeader.h:45 -- one index entry per recorded frunk.
    //
    // ARTIST overrides the older PS3 layout. AddFrunk's writer 8265EE6C stores
    // the full 64-bit frame at +0; 8265EE88/8265EFDC store the file byte offset
    // and payload size at +8/+C. StartNewStream returns that same frame with
    // ldx at 8265C29C and uses +8/+C for DiskReadStream's byte range. The old
    // frame/count labels at +8/+C confused disk overwrite with frame overlap.
    struct StreamOffset
    {
        s64 miFrameNumber; // @0x00 original 64-bit game frame
        s32 miFileOffset;  // @0x08 byte offset of the frunk payload within the stream file
        s32 miFrunkSize;   // @0x0C frunk payload size in bytes
        f32 mfFrameTime;   // @0x10 game time of the frunk's first frame
        u16 mxFlags;       // @0x14 KU_FLAG_VOID / KU_FLAG_KEYFRAME bitfield
        u16 muPad;         // @0x16 (entry padded to a 24-byte stride on X360)
    };

    // DWARF: BrnReplayStreamHeader.h:63 -- the stream file header (a ring index over
    // the recorded frunks). X360 member offsets match the PS3 DWARF here:
    //   +0x00 macMagicNumber[8] ; +0x08 miVersion ; +0x0C miNumFrunks
    //   +0x10 miFirstFrunk      ; +0x14 mpFrameOffsets
    // (InvalidateFrunksAhead reads miNumFrunks @+0x0C, miFirstFrunk @+0x10 and the
    // mpFrameOffsets array base @+0x14, exactly as laid out below.)
    struct StreamHeader
    {
        char         macMagicNumber[8]; // @0x00
        s32          miVersion;         // @0x08
        s32          miNumFrunks;       // @0x0C count of live frunks in the ring
        s32          miFirstFrunk;      // @0x10 ring index of the first live frunk
        StreamOffset* mpFrameOffsets;   // @0x14 base of the frunk index array (ring of KI_MAX_FRUNKS)

        // @0x8264B270 -- trim trailing frunks that cannot be replayed from.
        // Walks back from the last recorded frunk and drops every tail frunk that is
        // not a live keyframe, shrinking miNumFrunks. Defined in
        // BrnReplayStreamHeader.cpp. Called by ReplayModule::UpdateRecording_PreSim.
        StreamHeader* ChopOffTailFrames();
    };
}
