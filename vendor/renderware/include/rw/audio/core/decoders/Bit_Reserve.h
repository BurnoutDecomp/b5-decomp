#pragma once

// =====================================================================================
// rw::audio::core::Bit_Reserve -- the MSB-first bit reader behind the EARenderWare
// Layer-3 ("MP3"-family) Huffman decoder.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm of
//   Bit_Reserve::hget1bit    @0x82B90408
//   Bit_Reserve::rewindNbits @0x82B90460
// is authoritative. No DecFIGS DWARF layout and no prior source exist for this
// type, so the field layout below is grounded directly in the disassembly. Sibling to
// the committed rw::audio::core homes (PlugIn, Iir2, Unpack0, ...) and consumed by
// rw::audio::core::Layer3Dec::DecodeHuffman.
//
// LAYOUT (grounded in the asm offsets):
//   +0x00  muReserved   -- not touched by either entry point
//   +0x04  muBytePos    -- read cursor, a byte index masked with & 0x7FF (low 11 bits)
//   +0x08  muBitsLeft   -- bits still unconsumed in muAccum
//   +0x0C  muAccum      -- current word, MSB-first: the next bit is bit 31
//   +0x10  maBuffer     -- 2048-byte ring buffer (lbz at base + 0x10 + (idx & 0x7FF))
//
// muAccum is consumed from the top: hget1bit returns bit 31 then shifts left by one,
// refilling a byte (rotated so the byte sits in the high 8 bits) whenever muBitsLeft
// reaches 0. rewindNbits pushes the cursor back by N bits and, when the rewind does not
// land on a byte boundary, reloads muAccum from the preceding buffer byte.
// =====================================================================================

#include "types.hpp" // u8, u32, s32

namespace rw
{
namespace audio
{
namespace core
{

class Bit_Reserve
{
public:
    enum { KU_BUFFER_BYTES = 0x800 }; // ring buffer size; cursor masked with & 0x7FF

    u32 muReserved;                  // +0x00 -- untouched by these entry points
    u32 muBytePos;                   // +0x04 -- byte read cursor (masked & 0x7FF)
    u32 muBitsLeft;                  // +0x08 -- unconsumed bits in muAccum
    u32 muAccum;                     // +0x0C -- current word, next bit is bit 31
    u8  maBuffer[KU_BUFFER_BYTES];   // +0x10 -- 2048-byte ring buffer

    // @ 0x82B90408 -- read and return the next single bit (0 or 1), MSB-first,
    // refilling muAccum a byte at a time as it drains.
    s32 hget1bit();

    // @ 0x82B90460 -- rewind the read cursor by `nBits` bits. Returns `this`
    // (the asm returns r3 unchanged) so callers can chain.
    Bit_Reserve* rewindNbits(s32 nBits);
};

} // namespace core
} // namespace audio
} // namespace rw
