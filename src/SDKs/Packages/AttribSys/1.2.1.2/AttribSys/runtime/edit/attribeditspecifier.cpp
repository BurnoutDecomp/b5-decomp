#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/edit/attribeditspecifier.h"

// ===========================================================================
// Attrib::EditSpecifierLess -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODY both come from the X360 asm.
// operator() @ 0x82803DC0. See attribeditspecifier.h for the layout.
// ===========================================================================

namespace Attrib
{

// ---------------------------------------------------------------------------
// EditSpecifierLess::operator() @ 0x82803DC0
//
//   ld    r11, 0(r4)   ; ld r10, 0(r5)   ; cmpld cr6,r11,r10 ; bne tail   (key0, u64)
//   ld    r11, 8(r4)   ; ld r10, 8(r5)   ; cmpld cr6,r11,r10 ; bne tail   (key1, u64)
//   ld    r11, 0x10(r4); ld r10, 0x10(r5); cmpld cr6,r11,r10 ; bne tail   (key2, u64)
//   ; all three equal -> compare the u32 tie-breaker and return lhs<rhs:
//   lwz   r11, 0x18(r4); lwz r10, 0x18(r5)
//   subfc r11,r10,r11 ; subfe r11,r11,r11 ; clrlwi r3,r11,31   ; return (key3_l < key3_r)
//   blr
// tail (loc_82803E08):
//   li r11,1 ; blt cr6, +8 ; li r11,0 ; clrlwi r3,r11,24       ; return (first-diff was <)
//   blr
//
// All comparisons are UNSIGNED: the three 64-bit keys use `cmpld` (compare
// logical doubleword) and the 32-bit tie-breaker uses the subfc/subfe unsigned
// less-than idiom. The Hex-Rays `__int128`/`DWORD2` reads and the `*(a2+4)`
// first-field offset are misreads -- the asm loads `0(r4)` first (offset 0) and
// each `ld` is a plain 64-bit scalar load, not a 128-bit vector op. The result
// is a lexicographic strict-weak ordering over (key0, key1, key2, key3).
// ---------------------------------------------------------------------------
bool EditSpecifierLess::operator()(const EditSpecifier& lfLhs,
                                   const EditSpecifier& lfRhs) const
{
    if (lfLhs.muKey0 != lfRhs.muKey0)
    {
        return lfLhs.muKey0 < lfRhs.muKey0;
    }
    if (lfLhs.muKey1 != lfRhs.muKey1)
    {
        return lfLhs.muKey1 < lfRhs.muKey1;
    }
    if (lfLhs.muKey2 != lfRhs.muKey2)
    {
        return lfLhs.muKey2 < lfRhs.muKey2;
    }
    return lfLhs.muKey3 < lfRhs.muKey3;
}

} // namespace Attrib
