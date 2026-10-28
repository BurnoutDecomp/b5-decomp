#pragma once

#include "types.hpp"

// renderengine::PixelFormat - the render-engine texture pixel format enum (EATech renderengine,
// SDKs/EATech/include/.../gcm/renderengine/pixelformat.h). The DecFIGS DWARF gives the logical
// PIXELFORMAT_* names; the ENUMERATOR VALUES here are the X360 ARTIST build's, read directly off
// the texture-format comparisons in the X360 asm (CgsNetworkImageConverter / CgsNetworkTexture),
// which differ from the PS3 DWARF values (each platform encodes its own GPU texture-format code).
//
// The 32-bit value is the X360 GPU texture-format word: the low 6 bits are the GPUTEXTUREFORMAT
// (0x06 = 8_8_8_8, 0x03 = 1_5_5_5, 0x0B = 8_8, 0x12 = DXT1), the upper bytes carry the
// endian/swizzle/sign flags. Only the formats this build's image path actually names are pinned
// here; the rest of the EATech enum is out of scope until a TU needs them.
//
// FLAG (reviewer): values are X360-asm-attested; the X8 vs A8 distinction (0x28.. vs 0x18.. high
// byte) and the DXT1/G8B8 names are inferred from the GPUTEXTUREFORMAT low bits + the asserting
// call context (32bpp source, 16bpp A1R5G5B5 dest, block-compressed via GetHeightInBlocks).

namespace renderengine
{
    enum PixelFormat
    {
        PIXELFORMAT_NA        = -1,

        PIXELFORMAT_A8R8G8B8  = 0x18280086,   // 32bpp ARGB  (low6 = 0x06, alpha)
        PIXELFORMAT_X8R8G8B8  = 0x28280086,   // 32bpp XRGB  (low6 = 0x06, no alpha)
        PIXELFORMAT_A1R5G5B5  = 0x18280043,   // 16bpp ARGB1555 (low6 = 0x03)
        PIXELFORMAT_G8B8      = 0x1A20000B,    // 16bpp two-channel (low6 = 0x0B)
        PIXELFORMAT_DXT1      = 0x1A200052,    // block-compressed BC1 (low6 = 0x12)
    };
}
