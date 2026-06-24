// =====================================================================================
// rw::audio::core::SeekTableParser::ParseChunkSection0 -- the inner 16-byte chunk-section
// copy step. Reconstructed from BURNOUT_X360_ARTIST.XEX @0x82B6E880; the PowerPC asm is
// authoritative. No reference source and no DecFIGS DWARF exist for this TU.
//
// The asm is 16 paired lbz r11,N(r4) / stb r11,N(r3) for N = 0..0xF, then blr returning
// r3. It is a flat 16-byte byte-for-byte copy of the source chunk-section block into the
// destination, returning the destination pointer.
// =====================================================================================

#include "rw/audio/core/SeekTableParser.h"

namespace rw
{
namespace audio
{
namespace core
{

ChunkSection0* SeekTableParser::ParseChunkSection0(ChunkSection0* apDest,
                                                   const ChunkSection0* apSource)
{
    // 16 byte-for-byte stores (lbz/stb at displacements 0..0xF), return r3 (apDest).
    for (int li = 0; li < 16; ++li)
        apDest->mau8Bytes[li] = apSource->mau8Bytes[li];
    return apDest;
}

} // namespace core
} // namespace audio
} // namespace rw
