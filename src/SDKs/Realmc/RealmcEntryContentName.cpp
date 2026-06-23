#include "SDKs/Realmc/RealmcEntryContentName.h"

#include <cstring>  // std::memcpy / std::memset / std::strncpy

// ===========================================================================
// RealmcIface::EntryContentName -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODIES both come from the X360 asm. See
// RealmcEntryContentName.h for the per-offset layout map.
// ===========================================================================

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// EntryContentName::SetTitle @ 0x82B51AD8
//
//   cmplwi cr6, r4, 0 ; li r5, 0x100
//   beq    cr6, memset-branch
//     bl memcpy(r3=this, r4=src, r5=0x100)   -> copy 256 bytes
//     li r11,0 ; sth r11, 0xFE(r31)          -> zero the 16-bit word at +0xFE
//   else:
//     li r4,0 ; bl memset(r3=this, 0, 0x100) -> zero 256 bytes
//   return r3 (this)
//
// macTitle is a fixed 256-byte block; the trailing 16-bit word at +0xFE is the
// final two-byte title slot, NUL-terminated after a copy.
// ---------------------------------------------------------------------------
EntryContentName* EntryContentName::SetTitle(const void* pTitleSource)
{
    if (pTitleSource)
    {
        std::memcpy(macTitle, pTitleSource, 0x100);
        // sth 0, 0xFE(this) -- zero the trailing 16-bit slot of the title.
        macTitle[0xFE] = 0;
        macTitle[0xFF] = 0;
    }
    else
    {
        std::memset(macTitle, 0, 0x100);
    }
    return this;
}

// ---------------------------------------------------------------------------
// EntryContentName::SetFileName @ 0x82B51B28
//
//   cmplwi cr6, r4, 0
//   beq    cr6, null-branch
//     li r5, 0x2A ; addi r3, r31, 0x100 ; bl strncpy(dst=+0x100, src, 0x2A)
//     li r11,0 ; stb r11, 0x129(r31)         -> NUL-terminate at +0x129
//   else:
//     li r11,0 ; stb r11, 0x100(r31)         -> macFileName[0] = 0
//   return r3 (this)
// ---------------------------------------------------------------------------
EntryContentName* EntryContentName::SetFileName(const char* pFileNameSource)
{
    if (pFileNameSource)
    {
        std::strncpy(macFileName, pFileNameSource, 0x2A);
        // stb 0, 0x129(this) -- the last byte of the 42-byte field is the NUL.
        macFileName[0x29] = '\0';
    }
    else
    {
        macFileName[0] = '\0';
    }
    return this;
}

// ---------------------------------------------------------------------------
// EntryContentName::EntryContentName @ 0x82B51C10
//
//   bl SetTitle(this, a2)                    -> r4 = a2 (title source)
//   mr r4, a3 ; bl SetFileName(this, a3)     -> r4 = a3 (file-name source)
//   extsw  r11, a4 ; std r11, scratch        -> sign-extend a4 to 64-bit
//   lfd f0, scratch ; fcfid f0, f0           -> (double)(__int64)a4
//   frsp   f13, f0                            -> narrow to float
//   lfs    f0, flt_8200C6B8                   -> rodata constant (IDA: 2.8)
//   fmuls  f0, f13, f0                        -> (float)a4 * 2.8f
//   fctiwz f0, f0 ; stfiwx f0, 0, r10(+0x12C) -> (int)trunc -> mnSize
//
// The store at +0x12C is an integer word (fctiwz produces the truncated integer
// and stfiwx stores that integer), so mnSize is an int, not a float. flt_8200C6B8
// is the shared rodata single-precision constant IDA renders as 2.8.
// ---------------------------------------------------------------------------
EntryContentName::EntryContentName(const void* pTitleSource,
                                   const char* pFileNameSource,
                                   int nSizeSeed)
{
    SetTitle(pTitleSource);
    SetFileName(pFileNameSource);

    // (double)(__int64)nSizeSeed -> float -> * 2.8f -> truncate toward zero.
    const float lfScaled = static_cast<float>(static_cast<long long>(nSizeSeed)) * 2.8f;
    mnSize = static_cast<std::int32_t>(lfScaled);
}

} // namespace RealmcIface
