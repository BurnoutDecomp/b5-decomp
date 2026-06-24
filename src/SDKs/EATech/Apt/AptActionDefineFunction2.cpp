#include "SDKs/EATech/Apt/AptActionDefineFunction2.h"

// ===========================================================================
//  AptAction_DefineFunction2::getDF2Flag -- @ 0x82AD5230.
//
//  Store-for-store reconstruction from BURNOUT_X360_ARTIST.XEX. X360 asm is
//  authoritative.
//
//      li      r11, 1
//      lha     r10, 0xA(r3)      ; SIGNED 16-bit flags word at +0x0A
//      slw     r11, r11, r4      ; 1 << nFlagBit
//      and     r3, r10, r11
//      blr
// ===========================================================================

namespace AptAction_DefineFunction2
{

int getDF2Flag(const void* pRecord, char nFlagBit)
{
    const s16 lsFlags =
        *reinterpret_cast<const s16*>(static_cast<const u8*>(pRecord) + 0xA);
    return static_cast<int>(lsFlags) & (1 << nFlagBit);
}

} // namespace AptAction_DefineFunction2
