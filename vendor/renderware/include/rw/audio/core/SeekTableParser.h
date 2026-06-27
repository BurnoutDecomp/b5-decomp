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
// FLAG (rwaudio PDB reconcile): DIVERGE from the NFS ProStreet 08 X360 PDB. The PDB's
//   rw::audio::core::SeekTableParser is a 32-byte parser-STATE object with 8 instance
//   members (void* mpSeekData @0x00; int mStreamSkip @0x04; int mDecoderSkip @0x08;
//   int mPlayerSkip @0x0c; unsigned int mChunkOffset @0x10; int mSeekDataVersion @0x14;
//   int mLatency @0x18; bool mIsNewFeedChunk @0x1c; 3 bytes pad), sizeof=32. This
//   reconstructed header instead models only the recovered static 16-byte copy helper
//   (ParseChunkSection0) plus a local ChunkSection0 POD -- it has NO instance data
//   members, so the PDB names cannot be mapped onto it (no field order / sizeof to align).
//   Keeping the ARTIST-attested layout unchanged; the PDB state members belong to the
//   full SeekTableParser TU that is not yet homed here. Do not retrofit until that TU is
//   decompiled (the static copy step is the only attested piece).
// =====================================================================================

#include "types.hpp" // u8

namespace rw
{
namespace audio
{
namespace core
{

// A 16-byte chunk-section header block the seek-table parser copies verbatim.
struct ChunkSection0
{
    u8 mau8Bytes[16];
};

class SeekTableParser
{
public:
    // @ 0x82B6E880 -- copy the 16-byte chunk-section block from `apSource` into
    // `apDest` byte-for-byte (lbz/stb x16), then return `apDest`. Store-for-store with
    // the X360 (no widening, no reordering -- a plain 16-byte copy).
    static ChunkSection0* ParseChunkSection0(ChunkSection0* apDest, const ChunkSection0* apSource);
};

} // namespace core
} // namespace audio
} // namespace rw
