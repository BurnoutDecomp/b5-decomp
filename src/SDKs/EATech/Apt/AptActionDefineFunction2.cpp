#include "SDKs/EATech/Apt/AptActionDefineFunction2.h"

// ===========================================================================
//  AptAction_DefineFunction2::getDF2Flag -- @ 0x82AD5230 (X360) / x64 twin
//  @0x14084E310.
//
//  x64 (rung 1 for Apt): `movsx eax, word ptr [rcx+0Eh]` -- the SIGNED 16-bit
//  flags word sits at +0x0E of the RESOLVED in-memory record. The console read
//  +0x0A; the +4 delta is the record's leading `const char* szName` widening
//  4->8 (B4 record: szName@0, nParams@+8, nRegisterCount@+0xC, nFlags@+0xE on
//  x64). Reading the console +0x0A on x64 lands inside nRegisterCount.
// ===========================================================================

namespace AptAction_DefineFunction2
{

int getDF2Flag(const void* pRecord, char nFlagBit)
{
    const s16 lsFlags =
        *reinterpret_cast<const s16*>(static_cast<const u8*>(pRecord) + 0xE);
    return static_cast<int>(lsFlags) & (1 << nFlagBit);
}

} // namespace AptAction_DefineFunction2
