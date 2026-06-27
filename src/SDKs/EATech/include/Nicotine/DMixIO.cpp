#include "SDKs/EATech/include/Nicotine/DMixIO.hpp"

// ===========================================================================
//  Nicotine::DMixIO -- dynamic-mixer I/O accessor.
//
//  Store-for-store reconstruction from BURNOUT_X360_ARTIST.XEX. See DMixIO.hpp
//  for the per-function source addresses and the member-layout rationale.
//  X360 asm is authoritative over the partial PC DecFIGS DWARF.
// ===========================================================================

// ---------------------------------------------------------------------------
//  EXTERNAL DEPENDENCY (un-homed in this batch -- FLAGGED).
//  NFSMixShape::GetIntPitchMultFromCents converts a signed "cents" pitch value
//  into the engine's integer pitch multiplier. It is defined in another TU
//  (the NFSMixShape family, which has no DecFIGS DWARF home). GetDMixOutput's
//  DMX_PITCH branch tail-calls it (X360 @0x82B4496C: b NFSMixShape__Get...).
//  Declared here so this TU compiles; NOT defined here.
// ---------------------------------------------------------------------------
namespace NFSMixShape
{
    int GetIntPitchMultFromCents(int iCents);
}

namespace Nicotine
{

// ctor @ 0x82B448B0
//   result[2] = 0; *result = &vtable; result[3] = 0;
DMixIO::DMixIO()
    : m_ID(0)        // (X360 does not write +0x04 here; left at construction default)
    , m_pDMixInputBlock(0)   // result[2] @ +0x08
    , m_pDMixOutputBlock(0)  // result[3] @ +0x0C
{
    // vptr (+0x00) is installed by the compiler-generated prologue (the X360
    // ctor's `*result = &off_8214780C`), reproduced here by the virtual dtor.
    (void)m_ID;
}

// SetDMixInput @ 0x82B448D0
//   v3 = *(result+8); if (v3) *(4*a2 + v3) = a3; return result;
DMixIO* DMixIO::SetDMixInput(int iSlot, int iValue)
{
    int* lpiInput = m_pDMixInputBlock;
    if (lpiInput)
        lpiInput[iSlot] = iValue;
    return this;
}

// SetDMixInputUnsafe (DWARF DMixIO.hpp:45): unchecked store (no null guard).
void DMixIO::SetDMixInputUnsafe(int iSlot, int iValue)
{
    m_pDMixInputBlock[iSlot] = iValue;
}

// GetDMixInput @ 0x82B448E8
//   v2 = *(a1+8); if (v2) return *(4*a2 + v2); else return 0;
int DMixIO::GetDMixInput(int iSlot)
{
    int* lpiInput = m_pDMixInputBlock;
    if (lpiInput)
        return lpiInput[iSlot];
    return 0;
}

// GetDMixInputPtr (DWARF DMixIO.hpp:59)
int* DMixIO::GetDMixInputPtr()
{
    return m_pDMixInputBlock;
}

// GetDMixOutput @ 0x82B44908
//   v3 = *(a1+12); if (!v3) return 0;
//   v4 = (16*a2) & 0x10;            // 0 for even slot, 16 for odd slot
//   v5 = *(4*(a2>>1) + v3);         // packed int: two 16-bit fields
//   switch (a3) { ... }
int DMixIO::GetDMixOutput(int iSlot, int ePreset)
{
    int* lpiOutput = m_pDMixOutputBlock;
    if (!lpiOutput)
        return 0;

    // Odd slots live in the high 16 bits of the packed int, even in the low.
    int iShift  = (16 * iSlot) & 0x10;
    int iPacked = lpiOutput[iSlot >> 1];

    switch (ePreset)
    {
    case DMX_VOL:    // 0
    case DMX_FREQ:   // 2
    case DMX_DEPTH:  // 4
        // sraw then clrlwi #17 -> arithmetic shift, keep low 15 bits.
        return (iPacked >> iShift) & 0x7FFF;

    case DMX_PITCH:  // 1
        // sraw then clrlwi #16 -> keep low 16 bits, then cents->mult.
        return NFSMixShape::GetIntPitchMultFromCents((iPacked >> iShift) & 0xFFFF);

    case DMX_AZIM:   // 3
        // sraw then clrlwi #16 -> keep low 16 bits.
        return (iPacked >> iShift) & 0xFFFF;

    default:
        return 0;
    }
}

// GetDMixOutputPtr (DWARF DMixIO.hpp:60)
int* DMixIO::GetDMixOutputPtr()
{
    return m_pDMixOutputBlock;
}

// TurnOffMixOutput @ 0x82B44988
//   v1 = *(result+12); if (v1) *(v1+60) = 0; return result;
DMixIO* DMixIO::TurnOffMixOutput()
{
    int* lpiOutput = m_pDMixOutputBlock;
    if (lpiOutput)
        lpiOutput[60 / sizeof(int)] = 0;  // enable word at byte +0x3C
    return this;
}

// TurnOnMixOutput @ 0x82B449A0
//   v1 = *(result+12); if (v1) *(v1+60) = 1; return result;
DMixIO* DMixIO::TurnOnMixOutput()
{
    int* lpiOutput = m_pDMixOutputBlock;
    if (lpiOutput)
        lpiOutput[60 / sizeof(int)] = 1;  // enable word at byte +0x3C
    return this;
}

// GetStateID @ 0x82B449B8
//   lhz r11,4(r3); clrlwi r3,r11,24  -> low 8 bits of the state word.
int DMixIO::GetStateID()
{
    return m_ID & 0xFF;
}

// GetSFX_ID @ 0x82B449C8
//   (*(a1+4) >> 4) & 0x7F   (extrwi r3,r11,7,21 -> 7 bits starting at bit 4).
int DMixIO::GetSFX_ID()
{
    return (m_ID >> 4) & 0x7F;
}

// GetInstanceNum @ 0x82B449D8
//   *(a1+4) ; extrwi r3,r11,5,16 -> 5 bits at PPC bit-offset 16 == (m_ID >> 11) & 0x1F
//   (PPC extrwi extracts 5 bits whose low bit lands at 31-(16+5-1)=11; bits [15:11]).
int DMixIO::GetInstanceNum()
{
    return (m_ID >> 11) & 0x1F;
}

// IsSFXObj @ 0x82B449E8
//   (*(a1+4) & 0xE0000000) == 0x40000000  -> top-3-bit class tag == 0b010.
int DMixIO::IsSFXObj()
{
    return (m_ID & 0xE0000000u) == 0x40000000u;
}

} // namespace Nicotine
