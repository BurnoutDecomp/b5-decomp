#pragma once

// =====================================================================================
// rw::audio::core::PackedColumnReader -- the run-length + delta unpacker that walks one
// "packed column" of an EARenderWare "rwaudio" packed table (the consumer Unpack0.h's
// banner names: PackedColumnReader). It pulls successive column values via the committed
// rw::audio::core::Unpack0::UnpackInt32 (zig-zag SIGNED varint) decoder.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm of
//   PackedColumnReader::ReadRunInfo   @0x82B6B928
//   PackedColumnReader::ReadSymbol    @0x82B6B9A0
//   PackedColumnReader::GetNextValue  @0x82B6B9F8
// is authoritative. No Feb-2007 leak source and no DecFIGS DWARF exist for this TU, so the
// field layout below is grounded directly in the disassembly. Sibling to the committed
// rw::audio::core homes (Unpack0, SeekTableParser, PlugIn, Iir2, ...) and consumed by
// rw::audio::core::PackedTableReader::UnpackNextRow.
//
// FLAG (rwaudio PDB reconcile): no PackedColumnReader entry was available in the rwaudio
// PDB references used to name the other homes, so the field NAMES below are reconstructed
// from the asm offsets (not PDB-authoritative). Offsets/sizes ARE attested by the asm.
//
// LAYOUT (grounded in the asm offsets):
//   +0x00  mppCursor   -- pointer to the byte cursor pointer; **mppCursor is the current
//                         input byte, advanced by the bytes UnpackInt32 consumes
//   +0x04  mValue      -- running column value: GetNextValue returns it; ReadSymbol adds
//                         the decoded delta into it
//   +0x08  mCount      -- remaining run length for the current symbol; decremented per
//                         GetNextValue, refilled by ReadRunInfo
//   +0x0C  mbLiteral   -- run/literal flag: when set, each value is its own symbol (read
//                         via ReadSymbol every step); when clear, the run repeats mValue
//
// DECODE. ReadRunInfo decodes a signed run header N: N>=0 means a literal run of N+1
// distinct symbols (mbLiteral=1, mCount=N+1); N<0 means a repeat run of 1-N copies of the
// next symbol (mbLiteral=0, mCount=1-N). GetNextValue refreshes the run when mCount runs
// out, reads a fresh symbol per step in literal runs (or once for a repeat run), then
// returns mValue and consumes one from mCount.
// =====================================================================================

#include "types.hpp" // u8, s32

namespace rw
{
namespace audio
{
namespace core
{

class PackedColumnReader
{
public:
    u8** mppCursor;  // +0x00 -- pointer to the input byte cursor pointer
    s32  mValue;     // +0x04 -- running column value (accumulated by ReadSymbol)
    s32  mCount;     // +0x08 -- remaining run length
    u8   mbLiteral;  // +0x0C -- literal-run flag (1 = per-step symbols, 0 = repeat)

    // @ 0x82B6B928 -- decode the next run header via Unpack0::UnpackInt32, advance the
    // cursor, and set mCount/mbLiteral. A negative header is a repeat run.
    void ReadRunInfo();

    // @ 0x82B6B9A0 -- decode a signed delta via Unpack0::UnpackInt32, advance the cursor,
    // and accumulate the delta into mValue.
    void ReadSymbol();

    // @ 0x82B6B9F8 -- return the next column value: refill the run when exhausted, pull a
    // symbol when one is due, then return mValue and consume one run step.
    s32 GetNextValue();
};

} // namespace core
} // namespace audio
} // namespace rw
