// =====================================================================================
// rw::audio::core::BitGetter -- MSB-first bit cursor over a byte buffer.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   GetBits @0x82680460
// DecFIGS BrnSoundUnity records the four unsigned locals; ARTIST establishes
// the order of the byte reads and cursor updates.
// See BitGetter.h for the layout and the bit-stream semantics.
// =====================================================================================

#include "rw/audio/core/BitGetter.h"

namespace rw
{
namespace audio
{
namespace core
{

// ---------------------------------------------------------------------------
// BitGetter::GetBits @ 0x82680460
// ---------------------------------------------------------------------------
u32 BitGetter::GetBits(u32 numBits)
{
    u32 val = 0;
    while (numBits)
    {
        const u32 index = mBitPosition >> 3;
        const u32 bitpos = mBitPosition & 7;
        u32 nbits = 8u - bitpos;
        if (nbits > numBits)
            nbits = numBits;
        // ARTIST narrows this right shift to a byte at 0x826804AC.
        val = (val << nbits) |
              ((mpBitBuffer[index] >> static_cast<u8>(8u - nbits - bitpos)) & ((1u << nbits) - 1u));
        numBits -= nbits;
        mBitPosition += nbits;
    }
    return val;
}

} // namespace core
} // namespace audio
} // namespace rw
