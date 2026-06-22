// =====================================================================================
// rw::audio::core::Unpack0 -- "format 0" variable-length integer decoders.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   UnpackInt32  @0x82B6B818
//   UnpackUInt32 @0x82B8F460
// The PowerPC asm is authoritative; no Feb-2007 leak source and no DWARF exist.
// Bodies are store-for-store equivalents of the IDA pseudocode, which mirrors the
// asm exactly (the asm's `srawi` arithmetic shifts map to signed `>> 1`; `rlwinm`
// rotate-and-mask fields map to the shown shift/mask expressions; `clrlslwi`,
// `clrrwi`, `rotlwi` likewise). See Unpack0.h for the encoding description.
// =====================================================================================

#include "rw/audio/core/Unpack0.h"

namespace rw
{
namespace audio
{
namespace core
{

namespace
{
// PPC rotate-left-word-by-N (the asm's `rotlwi r,r,8` / IDA __ROL4__).
inline unsigned int Rol4(unsigned int v, unsigned int n)
{
    return (v << n) | (v >> (32u - n));
}
} // namespace

// ---------------------------------------------------------------------------
// Unpack0::UnpackInt32 @ 0x82B6B818  (SIGNED, zig-zag low-bit sign)
//
// Faithful to IDA pseudocode: r3=src, r4=pOut. v3 holds the sign bit, v5 the
// (still-positive) magnitude; at LABEL_11 the sign bit flips the value via
// -1 - v5. The arithmetic shifts (srawi) are signed `>> 1`.
// ---------------------------------------------------------------------------
int Unpack0::UnpackInt32(const unsigned char* a1, unsigned int* a2)
{
    int v3;            // r9  -- sign bit
    unsigned int v4;   // r10 -- first byte
    unsigned int v5;   // r11 -- decoded magnitude
    int result;        // r3  -- bytes consumed
    int v7;            // r11
    int v8;            // r9
    int v9;            // r11

    v3 = 0;
    v4 = *a1;
    if (v4 < 0xC0)
    {
        v5 = v4 >> 1;
        v3 = v4 & 1;
        result = 1;
        goto LABEL_11;
    }
    if (*a1 < 0xF0u)
    {
        v7 = a1[1];
        result = 2;
        v3 = v7 & 1;
        v5 = ((((v4 << 8) | v7) >> 1) & 0xFFFF9FFF) + 96;
        goto LABEL_11;
    }
    if (*a1 < 0xFCu)
    {
        v8 = a1[2];
        result = 3;
        v9 = (((a1[1] | (v4 << 8)) << 8) & 0xFF0FFFFF | v8) >> 1;
        goto LABEL_9;
    }
    if (*a1 != 255)
    {
        v8 = a1[3];
        result = 4;
        v9 = (((((Rol4(a1[1], 8) | a1[2]) & 0x3FFFF | (v4 << 16) & 0x30000) << 8)
               | (unsigned int)(v8 & 0xFFFFFFFE)) >> 1) + 393216;
        goto LABEL_9;
    }
    result = 5;
    v5 = ((((Rol4(a1[1], 8) | a1[2]) << 8) | a1[3]) << 8) | a1[4];
    goto LABEL_11;

LABEL_9:
    v3 = v8 & 1;
    v5 = (unsigned int)(v9 + 6240);

LABEL_11:
    if (v3)
        v5 = -1 - v5;
    *a2 = v5;
    return result;
}

// ---------------------------------------------------------------------------
// Unpack0::UnpackUInt32 @ 0x82B8F460  (UNSIGNED)
//
// Faithful to IDA pseudocode: r3=src, r4=pOut. No sign handling; the decoded
// value (v3) is written straight to *pOut.
// ---------------------------------------------------------------------------
int Unpack0::UnpackUInt32(const unsigned char* a1, unsigned int* a2)
{
    unsigned int v3;   // r10
    int result;        // r3
    int v5;            // r9
    int v6;            // r8

    v3 = *a1;
    if (v3 >= 0xC0)
    {
        if (*a1 >= 0xF0u)
        {
            if (*a1 >= 0xFCu)
            {
                if (*a1 == 255)
                {
                    result = 5;
                    v3 = ((((Rol4(a1[1], 8) | a1[2]) << 8) | a1[3]) << 8) | a1[4];
                }
                else
                {
                    v6 = a1[2];
                    result = 4;
                    v3 = ((((Rol4(a1[1], 8) | v6) << 8) | a1[3]) & 0x3FFFFFF
                          | (v3 << 24) & 0x3000000) + 798912;
                }
            }
            else
            {
                v5 = a1[1];
                result = 3;
                v3 = ((((v5 | (v3 << 8)) << 8) | a1[2]) & 0xFF0FFFFF) + 12480;
            }
        }
        else
        {
            result = 2;
            v3 = ((a1[1] | (v3 << 8)) & 0xFFFF3FFF) + 192;
        }
    }
    else
    {
        result = 1;
    }
    *a2 = v3;
    return result;
}

} // namespace core
} // namespace audio
} // namespace rw
