#pragma once

// =====================================================================================
// rw::audio::core::BitGetter -- a big-endian (MSB-first) bit cursor over a byte buffer,
// used by the EARenderWare "rwaudio" packed-stream header readers (it feeds
// SndPlayer1::UnpackHeader / SndPlayer1::GetFileInfo and the CgsStreamMod header path).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm of
//   BitGetter::GetBits @0x82680460
// is authoritative. No prior source and no DecFIGS DWARF exist for this TU.
// Sibling to the committed rw::audio::core homes (Unpack0, PlugIn, Iir2, ...).
//
// LAYOUT (grounded in the asm: r8=this, `lwz r6,0(r8)` / `lwz r10,4(r8)`):
//   +0x00  mpBase    -- base byte pointer of the bit stream (loaded as a byte via lbzx)
//   +0x04  muBitPos  -- absolute bit cursor; byte index = muBitPos >> 3,
//                       in-byte bit offset (from the MSB) = muBitPos & 7
//
// GetBits(n) returns the next `n` bits as an unsigned integer, MSB-first, advancing the
// cursor by `n`. It coalesces across byte boundaries one byte-fragment at a time:
// each step takes min(n_remaining, 8 - (bitPos&7)) bits from the current byte, shifts
// them into the low end of the accumulator, and repeats. n==0 returns 0 and consumes
// nothing. The original signature is the C ABI shape (int* this, unsigned count); the
// loaded value is a single byte (`lbzx`), so the base is modelled as `const u8*`.
// =====================================================================================

#include "types.hpp" // u8, u32

namespace rw
{
namespace audio
{
namespace core
{

class BitGetter
{
public:
    // @ 0x82680460 -- read `count` bits (MSB-first) starting at the current cursor,
    // advance the cursor by `count`, and return them right-justified. count==0 -> 0.
    static u32 GetBits(BitGetter* self, u32 count);

    const u8* mpBase; // +0x00 -- base of the bit stream
    u32 muBitPos;     // +0x04 -- absolute bit cursor (bit index from mpBase)
};

} // namespace core
} // namespace audio
} // namespace rw
