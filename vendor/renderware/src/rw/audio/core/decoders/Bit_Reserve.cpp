// =====================================================================================
// rw::audio::core::Bit_Reserve -- MSB-first bit reader for the Layer-3 Huffman decoder.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   hget1bit    @0x82B90408
//   rewindNbits @0x82B90460
// The PowerPC asm is authoritative; no DWARF layout and no prior source exist.
// Bodies are store-for-store equivalents of the asm (the `clrlwi r,r,21` masks map to
// & 0x7FF, `rotrwi r,r,8` of a byte to `<< 24`, `clrlwi. r,r,29` to & 7, `srwi r,r,3`
// to >> 3, `subfic r,r,0x20` to 32 - n). See Bit_Reserve.h for the layout description.
// =====================================================================================

#include "rw/audio/core/decoders/Bit_Reserve.h"

namespace rw
{
namespace audio
{
namespace core
{

// ---------------------------------------------------------------------------
// Bit_Reserve::hget1bit @ 0x82B90408
//
// r3/r11=this. When muBitsLeft is 0, refill: load the byte at maBuffer[muBytePos &
// 0x7FF], post-increment muBytePos, set muBitsLeft = 8, and seat the byte in the high
// 8 bits of muAccum (rotrwi r,r,8 on a 0..255 value == << 24). Then return the top bit
// (muAccum >> 31), shift muAccum left by one, and decrement muBitsLeft.
// ---------------------------------------------------------------------------
s32 Bit_Reserve::hget1bit()
{
    if (muBitsLeft == 0)
    {
        u32 idx = muBytePos & 0x7FFu;
        ++muBytePos;
        u32 byteVal = maBuffer[idx];
        muBitsLeft = 8;
        muAccum = byteVal << 24; // rotrwi by 8 of a byte: seat it in the high 8 bits
    }

    s32 result = static_cast<s32>(muAccum >> 31);
    muBitsLeft -= 1;
    muAccum <<= 1;
    return result;
}

// ---------------------------------------------------------------------------
// Bit_Reserve::rewindNbits @ 0x82B90460
//
// r3=this, r4=nBits. total = nBits + muBitsLeft; new muBitsLeft = total & 7;
// muBytePos -= total >> 3. If the rewind did not land on a byte boundary (total & 7 !=
// 0), reload muAccum from the byte just before the (masked) cursor, left-shifted so the
// remaining unconsumed bits sit at the top: maBuffer[(muBytePos - 1) & 0x7FF] <<
// (32 - (total & 7)). Returns `this`.
// ---------------------------------------------------------------------------
Bit_Reserve* Bit_Reserve::rewindNbits(s32 nBits)
{
    u32 total = static_cast<u32>(nBits) + muBitsLeft;
    u32 fracBits = total & 7u;
    muBitsLeft = fracBits;
    muBytePos = muBytePos - (total >> 3);

    if (fracBits != 0)
    {
        u32 idx = (muBytePos - 1u) & 0x7FFu;
        muAccum = static_cast<u32>(maBuffer[idx]) << (32u - fracBits);
    }
    return this;
}

} // namespace core
} // namespace audio
} // namespace rw
