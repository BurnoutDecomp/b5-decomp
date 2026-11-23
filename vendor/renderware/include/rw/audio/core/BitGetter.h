#pragma once

// =====================================================================================
// rw::audio::core::BitGetter -- a big-endian (MSB-first) bit cursor over a byte buffer,
// used by the EARenderWare "rwaudio" packed-stream header readers (it feeds
// SndPlayer1::UnpackHeader / SndPlayer1::GetFileInfo and the CgsStreamMod header path).
//
// ARTIST GetBits at 0x82680460 establishes cursor behavior. The Xbox 360
// audio PDB and MAP identify the class layout and its three member methods.
//
// LAYOUT (grounded in the asm: r8=this, `lwz r6,0(r8)` / `lwz r10,4(r8)`):
//   +0x00  mpBitBuffer   -- base byte pointer of the bit stream (loaded as a byte via lbzx)
//   +0x04  mBitPosition  -- absolute bit cursor; byte index = mBitPosition >> 3,
//                       in-byte bit offset (from the MSB) = mBitPosition & 7
//
// GetBits(n) returns the next `n` bits as an unsigned integer, MSB-first, advancing the
// cursor by `n`. It coalesces across byte boundaries one byte-fragment at a time:
// each step takes min(n_remaining, 8 - (bitPos&7)) bits from the current byte, shifts
// them into the low end of the accumulator, and repeats. n==0 returns 0 and consumes
// nothing.
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
    u32 GetBits(u32 count);

    void SetBitBuffer(void *apBuffer)
    {
        mpBitBuffer = static_cast<u8 *>(apBuffer);
        mBitPosition = 0;
    }

    u32 GetBitPosition()
    {
        return mBitPosition;
    }

private:

    u8 *mpBitBuffer; // +0x00 -- base of the bit stream
    u32 mBitPosition;      // +0x04 -- absolute bit cursor (bit index from mpBitBuffer)
};

} // namespace core
} // namespace audio
} // namespace rw
