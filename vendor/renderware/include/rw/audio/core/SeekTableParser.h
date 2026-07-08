#pragma once

// =====================================================================================
// rw::audio::core::SeekTableParser -- the parser that walks an audio stream's seek-table
// chunk sections. This header is the canonical OWNING home for the one recovered helper:
//
//     rw::audio::core::SeekTableParser::ParseChunkSection0  @ 0x82B6E880
//
// EARenderWare "rwaudio" middleware. Reconstructed from BURNOUT_X360_ARTIST.XEX
// (PowerPC) -- the X360 asm is authoritative. There is NO matching translation unit
// available for this type and no DecFIGS DWARF.
//
// The recovered function @0x82B6E880 is the inner copy step the (larger, not-yet-homed)
// SeekTableParser::ParseChunkSection0 calls: a flat 16-byte byte-for-byte copy of a
// chunk-section record (a 16-byte chunk header/id block) from source into the parser's
// destination slot. The asm is 16 paired lbz/stb at displacements 0..0xF and returns r3
// (the destination). Only this copy step is attested here; the surrounding parser state
// machine is reconstructed when its own TU is decompiled (FLAG: minimal home).
//
// Lowercase rw::audio:: namespaces match the third-party middleware API.
//
// STATE LAYOUT (32 bytes) -- adopted from the NFS ProStreet 08 X360 rwaudio PDB and now
//   OFFSET-CONFIRMED against the ARTIST asm of the four parser methods below (Parse
//   @0x82B6FBD8, ParseHeader0 @0x82B6F8E0, ParseChunkSection0 @0x82B6F328, ParseChunkSection1
//   @0x82B6E908). The earlier revision of this header modelled only the static 16-byte copy
//   helper because no offsets were attested; the state members are now grounded (see the
//   per-member stores called out below), so the PDB names are mapped on.
//
// Members and where each store/read is attested:
//   +0x00 mpSeekData       ParseHeader0 sets = apSeekData + word[+4] (or 0); the section loop
//                          rewrites it to mpSeekData_base + running seek-offset delta.
//   +0x04 mStreamSkip      section loop `stw pos, 4`  -- accumulated chunk length (position).
//   +0x08 mDecoderSkip     section loop `stw target-player-pos, 8`.
//   +0x0C mPlayerSkip      section loop `stw min(target-pos, mLatency), 0xC`.
//   +0x10 mChunkOffset     section loop `stw accChunkOffset, 0x10`.
//   +0x14 mSeekDataVersion ParseHeader0 `stw versionByte, 0x14` (version>>4 selects section).
//   +0x18 mLatency         ParseHeader0 `stw latencyHalfword, 0x18`; section reads it (a1[6]).
//   +0x1C mIsNewFeedChunk  section loop `stb (flag==1), 0x1C`.
// PDB types mpSeekData as void*; modelled here as u8* for the byte-pointer arithmetic the
// asm performs. (The static copy helper ParseChunkSection0 below is unchanged.)
// =====================================================================================

#include "types.hpp" // u8, s32

namespace rw
{
namespace audio
{
namespace core
{

// A 16-byte serialised chunk-section record. The seek-table parser copies it verbatim
// (ParseChunkSection0 copy helper) and reads it as four 32-bit columns: a per-row
// chunk-offset delta, a per-row seek-data byte delta, the chunk length, and a new-feed
// flag. These are the same four columns ParseChunkSection1 pulls from a PackedTableReader.
struct ChunkSection0
{
    union
    {
        u8 mau8Bytes[16];
        struct
        {
            s32 miChunkOffsetDelta; // +0x00 -- accumulates into mChunkOffset
            s32 miSeekOffsetDelta;  // +0x04 -- accumulates into mpSeekData
            s32 miLength;           // +0x08 -- chunk length (negative terminates the table)
            s32 miFlag;             // +0x0C -- 1 == new-feed chunk
        };
    };
};

class SeekTableParser
{
public:
    u8* mpSeekData;        // +0x00 -- PDB void*; base + running seek-offset delta
    s32 mStreamSkip;       // +0x04
    s32 mDecoderSkip;      // +0x08
    s32 mPlayerSkip;       // +0x0C
    s32 mChunkOffset;      // +0x10
    s32 mSeekDataVersion;  // +0x14
    s32 mLatency;          // +0x18
    u8  mIsNewFeedChunk;   // +0x1C (3 bytes pad follow to 32)

    // @ 0x82B6FBD8 -- entry point. If the blob's first byte is non-zero, treat it as
    // "no usable table" (result true); otherwise parse the header. When the result is
    // true (target not resolved within the table) reset the resolved output members.
    bool Parse(u8* apSeekData, s32 aTargetPos);

    // @ 0x82B6F8E0 -- read the 8-byte header (version byte, latency halfword, seek-data
    // offset word), then dispatch on (version >> 4): 0 -> ParseChunkSection0,
    // 1 -> ParseChunkSection1, anything else -> false. The section table starts at
    // apSeekData + 8.
    bool ParseHeader0(u8* apSeekData, s32 aTargetPos);

    // @ 0x82B6F328 -- walk raw 16-byte chunk-section records (copied one at a time via the
    // copy helper below), resolving the chunk that contains aTargetPos into the state
    // members. Returns false once aTargetPos is covered (found), true if the table's
    // terminator is reached first (not found).
    bool ParseChunkSection0(u8* apRecords, s32 aTargetPos);

    // @ 0x82B6E908 -- same walk as ParseChunkSection0 but the rows come from a
    // PackedTableReader over the packed column stream at apTableData.
    bool ParseChunkSection1(u8* apTableData, s32 aTargetPos);

    // @ 0x82B6E880 -- copy the 16-byte chunk-section record from `apSource` into
    // `apDest` byte-for-byte (lbz/stb x16), then return `apDest`. Store-for-store with
    // the X360 (no widening, no reordering -- a plain 16-byte copy). This is the private
    // per-row read step ParseChunkSection0 calls.
    static ChunkSection0* ParseChunkSection0(ChunkSection0* apDest, const ChunkSection0* apSource);
};

} // namespace core
} // namespace audio
} // namespace rw
