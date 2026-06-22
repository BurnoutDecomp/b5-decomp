// =====================================================================================
// rw::audio::core::BitGetter -- MSB-first bit cursor over a byte buffer.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   GetBits @0x82680460
// The PowerPC asm is authoritative; no prior source and no DWARF exist.
// The body is a store-for-store equivalent of the asm: `lbzx` is a single-byte load,
// `clrlwi r9,r10,29` is `bitPos & 7`, `srwi r7,r10,3` is `bitPos >> 3`,
// `subfic r11,r9,8` is `8 - (bitPos & 7)`, and the accumulate is
//   result = (result << take) | ((byte >> (8 - take - (bitPos&7))) & ((1 << take) - 1)).
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
u32 BitGetter::GetBits(BitGetter* self, u32 count)
{
    u32 result = 0;
    if (count)
    {
        const u8* base = self->mpBase;
        do
        {
            u32 bitPos = self->muBitPos;
            u32 inByteBit = bitPos & 7;          // bit offset within the current byte
            u32 take = 8u - inByteBit;           // bits available in the current byte
            if (take > count)
                take = count;                    // clamp to what's still requested
            u32 byteVal = base[bitPos >> 3];     // single-byte fetch (lbzx)
            count -= take;
            self->muBitPos = bitPos + take;
            result = ((byteVal >> (8u - take - inByteBit)) & ((1u << take) - 1u))
                     | (result << take);
        } while (count);
    }
    return result;
}

} // namespace core
} // namespace audio
} // namespace rw
