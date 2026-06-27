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
// r3/r11=this. When mCached is 0, refill: load the byte at mBuffer[mOutPtr &
// 0x7FF], post-increment mOutPtr, set mCached = 8, and seat the byte in the high
// 8 bits of mCacheData (rotrwi r,r,8 on a 0..255 value == << 24). Then return the top bit
// (mCacheData >> 31), shift mCacheData left by one, and decrement mCached.
// FLAG (rwaudio PDB reconcile): member names per NFS ProStreet 08 X360 PDB.
// ---------------------------------------------------------------------------
s32 Bit_Reserve::hget1bit()
{
    if (mCached == 0)
    {
        u32 idx = mOutPtr & 0x7FFu;
        ++mOutPtr;
        u32 byteVal = mBuffer[idx];
        mCached = 8;
        mCacheData = byteVal << 24; // rotrwi by 8 of a byte: seat it in the high 8 bits
    }

    s32 result = static_cast<s32>(mCacheData >> 31);
    mCached -= 1;
    mCacheData <<= 1;
    return result;
}

// ---------------------------------------------------------------------------
// Bit_Reserve::rewindNbits @ 0x82B90460
//
// r3=this, r4=nBits. total = nBits + mCached; new mCached = total & 7;
// mOutPtr -= total >> 3. If the rewind did not land on a byte boundary (total & 7 !=
// 0), reload mCacheData from the byte just before the (masked) cursor, left-shifted so the
// remaining unconsumed bits sit at the top: mBuffer[(mOutPtr - 1) & 0x7FF] <<
// (32 - (total & 7)). Returns `this`.
// ---------------------------------------------------------------------------
Bit_Reserve* Bit_Reserve::rewindNbits(s32 nBits)
{
    u32 total = static_cast<u32>(nBits) + mCached;
    u32 fracBits = total & 7u;
    mCached = fracBits;
    mOutPtr = mOutPtr - (total >> 3);

    if (fracBits != 0)
    {
        u32 idx = (mOutPtr - 1u) & 0x7FFu;
        mCacheData = static_cast<u32>(mBuffer[idx]) << (32u - fracBits);
    }
    return this;
}

} // namespace core
} // namespace audio
} // namespace rw
