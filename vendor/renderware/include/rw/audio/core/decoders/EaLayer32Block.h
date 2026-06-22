#pragma once

// =====================================================================================
// rw::audio::core::EaLayer32Block -- one EA-Layer-3 v2 ("EaLayer32") compressed block
// header, as parsed by the EARenderWare Layer-3 streaming decoder.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm of
//   EaLayer32Block::GetUsableSamples @0x82B8ABD0
//   EaLayer32Block::Read             @0x82B94E88
// is authoritative. The DecFIGS DWARF for the EA-Layer-3 v1 decoder carries only its
// registration GUID (decealayer31.h:102, DecoderDesc::Guid = 1162621745) and the family
// constant LAYER3_BLOCKSAMPLES = 1152 (declayer3.h:32); no struct layout and no prior
// source exist for this block type, so the field layout below is grounded directly
// in the disassembly. Sibling to the committed rw::audio::core homes (Iir2, Unpack0,
// Bit_Reserve, ...). Both entry points are called by EaLayer3DecBase::DecodeGranule.
//
// A granule is 576 samples (0x240); a full Layer-3 block is two granules (1152).
//
// LAYOUT (grounded in the asm offsets; fields are 32-bit):
//   +0x00  muReserved   -- not touched by either entry point
//   +0x04  muState      -- granule decode state (0 / 2 / 6); 0 when no payload precedes
//   +0x08  muGranuleLen -- payload length field (bits 0..9 of the long header)
//   +0x0C  muNextOffset -- byte offset of the following block's data
//   +0x10  muUsedCount  -- usable-sample count field (bits 10..19 of the long header)
//   +0x14  muChannels   -- 1 + ((header >> 14) & 1)  (mono = 1, stereo = 2)
//   +0x18  muFreeOffset -- bits 20..29 of the long header
//   +0x1C  muMode       -- 2-bit block mode (bits 30..31 of the long header)
// =====================================================================================

#include "types.hpp" // u8, u16, u32, s16, s32

namespace rw
{
namespace audio
{
namespace core
{

class EaLayer32Block
{
public:
    enum { KU_GRANULE_SAMPLES = 576 };  // 0x240 -- one granule
    enum { KU_BLOCK_SAMPLES   = 1152 }; // LAYER3_BLOCKSAMPLES (declayer3.h:32)
    enum { KU_DECEALAYER31_GUID = 1162621745u };

    u32 muReserved;   // +0x00 -- untouched by these entry points
    u32 muState;      // +0x04 -- granule decode state (0 / 2 / 6)
    u32 muGranuleLen; // +0x08 -- payload length field
    u32 muNextOffset; // +0x0C -- byte offset to the next block's data
    u32 muUsedCount;  // +0x10 -- usable-sample count field
    u32 muChannels;   // +0x14 -- 1 or 2
    u32 muFreeOffset; // +0x18
    u32 muMode;       // +0x1C -- 2-bit block mode

    // @ 0x82B8ABD0 -- how many output samples this block currently yields, keyed off
    // muMode and whether muState has been advanced. Returns a sample count.
    s32 GetUsableSamples();

    // @ 0x82B94E88 -- parse the block header at `pData` into this block's fields. The
    // header's high bit selects a long (6-byte) form vs a short (2-byte) form. Returns
    // the low-12-bit length field from the first big-endian 16-bit word.
    s32 Read(const s16* pData);
};

} // namespace core
} // namespace audio
} // namespace rw
