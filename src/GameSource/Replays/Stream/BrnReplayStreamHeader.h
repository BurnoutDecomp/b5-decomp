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
    // FLAG (X360 overrides DWARF on layout): the PS3 DWARF lays this out as
    // {miFileOffset s32, miFrunkSize s32, miFrameNumber s16, mxFlags u16, mfFrameTime
    // f32} == 16 bytes. The X360 InvalidateFrunksAhead @0x8264D190 indexes the
    // mpFrameOffsets array with a 24-byte stride and reads: a full 32-bit frame
    // number at +0x08, a 32-bit frame span at +0x0C (added to the frame number to
    // get the last frame the frunk covers), and the u16 flags at +0x14. So on X360
    // the entry is 24 bytes: miFrameNumber is widened to s32 and a 32-bit frame-span
    // word sits where the PS3 layout kept mfFrameTime. The members below match the
    // X360 stride/offsets; all access is BY NAME.
    struct StreamOffset
    {
        s32 miFileOffset;  // @0x00 byte offset of the frunk payload within the stream file
        s32 miFrunkSize;   // @0x04 frunk payload size in bytes
        s32 miFrameNumber; // @0x08 first game frame this frunk represents (X360: s32; PS3 DWARF: s16)
        s32 miFrameCount;  // @0x0C number of frames this frunk spans (X360 field; guard == "frunk is valid")
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
