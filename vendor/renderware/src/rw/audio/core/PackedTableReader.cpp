// =====================================================================================
// rw::audio::core::PackedTableReader::UnpackNextRow -- pull the next value from each of
// the four packed columns and write the row to apRow[0..3]. Reconstructed from
// BURNOUT_X360_ARTIST.XEX @0x82B6DC68; the PowerPC asm is authoritative. No Feb-2007 leak
// source and no DecFIGS DWARF exist for this TU. Sole dependency is the committed
// rw::audio::core::PackedColumnReader::GetNextValue (@0x82B6B9F8). See PackedTableReader.h
// for layout. Consumed by rw::audio::core::SeekTableParser::ParseChunkSection1.
// =====================================================================================

#include "rw/audio/core/PackedTableReader.h"
#include "rw/audio/core/PackedColumnReader.h"

namespace rw
{
namespace audio
{
namespace core
{

// ---------------------------------------------------------------------------
// PackedTableReader::UnpackNextRow @ 0x82B6DC68
//
// Pull one value from each of the four columns (this+0x04/0x14/0x24/0x34, one
// PackedColumnReader each, stride 0x10) into apRow[0..3] and return the fourth
// (last) value. The asm stores the first three results via r11 to 0/4/8(apRow)
// and the fourth (which stays in r3) to 0xC(apRow), returning it. Store-for-store
// with the X360.
// ---------------------------------------------------------------------------
s32 PackedTableReader::UnpackNextRow(s32* apRow)
{
    apRow[0] = maColumns[0].GetNextValue(); // this+0x04 -> 0(apRow)
    apRow[1] = maColumns[1].GetNextValue(); // this+0x14 -> 4(apRow)
    apRow[2] = maColumns[2].GetNextValue(); // this+0x24 -> 8(apRow)

    s32 result = maColumns[3].GetNextValue(); // this+0x34 -> 0xC(apRow)
    apRow[3] = result;
    return result; // r3 -- fourth column value
}

} // namespace core
} // namespace audio
} // namespace rw
