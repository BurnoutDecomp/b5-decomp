#pragma once

// =====================================================================================
// rw::audio::core::PackedTableReader -- reads one row at a time out of an EARenderWare
// "rwaudio" packed table by pulling the next value from each of its four packed columns.
// Each column is a rw::audio::core::PackedColumnReader (run-length + delta unpacker); the
// table reader just fans a row read across the four of them.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm of
//   PackedTableReader::UnpackNextRow @0x82B6DC68
// is authoritative. No Feb-2007 leak source and no DecFIGS DWARF exist for this TU, so the
// field NAMES below are reconstructed from the asm offsets (not PDB-authoritative), while
// the offsets/strides ARE attested by the disassembly. Sibling to the committed
// rw::audio::core homes (PackedColumnReader, Unpack0, SeekTableParser, ...) and consumed by
// rw::audio::core::SeekTableParser::ParseChunkSection1.
//
// LAYOUT (grounded in the asm offsets):
//   +0x00  mpCursor     -- the shared input byte cursor: each column's PackedColumnReader
//                          holds mppCursor = &mpCursor, so all four columns read from (and
//                          advance) this one pointer as they consume the interleaved packed
//                          stream. Attested by the inlined constructor in
//                          SeekTableParser::ParseChunkSection1 (the four `stw &mpCursor`
//                          stores at column offsets +0x04/+0x14/+0x24/+0x34).
//   +0x04  maColumns[0] -- one PackedColumnReader per column; the asm reaches column N at
//   +0x14  maColumns[1]    this+0x04+N*0x10 (stride 0x10 on the 32-bit X360 image; widens
//   +0x24  maColumns[2]    to the host PackedColumnReader size on a 64-bit host -- semantic
//   +0x34  maColumns[3]    parity, not byte-matching).
// =====================================================================================

#include "types.hpp"                            // u8, s32
#include "rw/audio/core/PackedColumnReader.h"   // PackedColumnReader

namespace rw
{
namespace audio
{
namespace core
{

class PackedTableReader
{
public:
    u8*                mpCursor;       // +0x00 -- shared byte cursor; columns advance it
    PackedColumnReader maColumns[4];   // +0x04/+0x14/+0x24/+0x34 -- one reader per column

    // Construct a reader over the packed-table byte stream `apTableData`: seat the shared
    // cursor at the stream start and point every column's mppCursor at it (zeroing each
    // column's value/count/literal state). Reconstructed from the constructor the X360
    // compiler inlined into SeekTableParser::ParseChunkSection1 @0x82B6E908. (The columns
    // hold &mpCursor -- do not copy a live PackedTableReader.)
    explicit PackedTableReader(u8* apTableData);

    // @ 0x82B6DC68 -- pull the next value from each of the four columns into apRow[0..3]
    // and return the fourth (last) column value.
    s32 UnpackNextRow(s32* apRow);
};

} // namespace core
} // namespace audio
} // namespace rw
