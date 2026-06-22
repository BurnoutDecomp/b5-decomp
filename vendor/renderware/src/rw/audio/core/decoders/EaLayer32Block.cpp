// =====================================================================================
// rw::audio::core::EaLayer32Block -- EA-Layer-3 v2 block-header parse + usable-sample
// query.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   GetUsableSamples @0x82B8ABD0
//   Read             @0x82B94E88
// The PowerPC asm is authoritative; no DWARF layout and no prior source exist.
// Bodies are store-for-store equivalents of the asm. Read() assembles its header from
// individual byte loads (lbz) into big-endian 16- and 32-bit words on a big-endian
// target; that byte assembly is reproduced explicitly here so the parse is correct
// independent of host endianness (the X360 `lhz`/`lwz` after the byte stores read the
// bytes back most-significant-first). See EaLayer32Block.h for the layout description.
// =====================================================================================

#include "rw/audio/core/decoders/EaLayer32Block.h"

namespace rw
{
namespace audio
{
namespace core
{

// ---------------------------------------------------------------------------
// EaLayer32Block::GetUsableSamples @ 0x82B8ABD0
//
// r3/r11=this. Branch on muMode (0x1C). The default yield is 576 (one granule).
//   muMode == 0 : 576 - muFreeOffset, but if muState == 0 the block is empty -> muUsedCount
//   muMode == 1 : 576,                but if muState == 0                       -> muUsedCount
//   muMode == 2 : if muState == 0 -> 2 * muUsedCount, else 576
//   muMode >= 3 : 576
// ---------------------------------------------------------------------------
s32 EaLayer32Block::GetUsableSamples()
{
    u32 mode = muMode;
    s32 result = 576;

    if (mode == 0)
    {
        result = 576 - static_cast<s32>(muFreeOffset);
        if (muState == 0)
            return static_cast<s32>(muUsedCount);
        return result;
    }

    if (mode == 1)
    {
        if (muState == 0)
            return static_cast<s32>(muUsedCount);
        return result;
    }

    // mode == 2 (the asm's `cmplwi cr6, r10, 3; bgelr` returns 576 for mode >= 3).
    if (mode >= 3)
        return result;

    // mode == 2
    if (muState != 0)
        return result;
    return 2 * static_cast<s32>(muUsedCount);
}

// ---------------------------------------------------------------------------
// EaLayer32Block::Read @ 0x82B94E88
//
// r3/r11=this, r4=pData (byte-addressed). The first two bytes form a big-endian 16-bit
// header `hdr16`:
//   muChannels = ((hdr16 >> 14) & 1) + 1
//   result     = hdr16 & 0xFFF
// Bit 15 (the sign bit of the signed short) selects the form:
//   set   -> long form: bytes 2..5 form a big-endian 32-bit `hdr32`:
//              muMode      = (hdr32 >> 30) & 3
//              muFreeOffset= (hdr32 >> 20) & 0x3FF
//              muGranuleLen= hdr32        & 0x3FF
//              muUsedCount = (hdr32 >> 10) & 0x3FF
//              muState     = (muGranuleLen > 0) ? 6 : 0
//              muNextOffset= muGranuleLen + 6
//   clear -> short form:
//              muUsedCount = muFreeOffset = muMode = 0
//              muGranuleLen= result - 2
//              muState     = 2
//              muNextOffset= result - 2
// ---------------------------------------------------------------------------
s32 EaLayer32Block::Read(const s16* pData)
{
    const u8* p = reinterpret_cast<const u8*>(pData);

    u32 hdr16 = (static_cast<u32>(p[0]) << 8) | static_cast<u32>(p[1]);
    muChannels = ((hdr16 >> 14) & 1u) + 1u;
    s32 result = static_cast<s32>(hdr16 & 0xFFFu);

    u32 nextOffset = 0;

    // Bit 15 set == negative signed short == long form.
    if (hdr16 & 0x8000u)
    {
        u32 hdr32 = (static_cast<u32>(p[2]) << 24)
                  | (static_cast<u32>(p[3]) << 16)
                  | (static_cast<u32>(p[4]) << 8)
                  | (static_cast<u32>(p[5]));

        muState = 0;
        muMode = (hdr32 >> 30) & 3u;
        muFreeOffset = (hdr32 >> 20) & 0x3FFu;
        u32 granuleLen = hdr32 & 0x3FFu;
        muGranuleLen = granuleLen;
        muUsedCount = (hdr32 >> 10) & 0x3FFu;

        if (granuleLen > 0)
            muState = 6;
        nextOffset = granuleLen + 6u;
    }
    else
    {
        muUsedCount = 0;
        muFreeOffset = 0;
        muMode = 0;
        muGranuleLen = static_cast<u32>(result - 2);
        muState = 2;
    }

    muNextOffset = nextOffset;
    return result;
}

} // namespace core
} // namespace audio
} // namespace rw
