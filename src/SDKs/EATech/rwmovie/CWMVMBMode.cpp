// =====================================================================================
// CWMVMBMode -- per-macroblock mode record for the WMV video encoder.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for this TU.
//
//   CWMVMBMode::CWMVMBMode  @0x82A7EEB0
//
// The constructor:
//   * runs a 6-iteration ctr loop (li r9,6; mtctr; bdnz) writing 0 to the six 32-bit
//     words at +0x04,+0x08,+0x0C,+0x10,+0x14,+0x18.
//   * reads the word at +0x00, clears its top three bits (clrlwi r11,r11,3 == & 0x1FFFFFFF)
//     and stores it back -- a read-modify-write of an in-place bitfield word, NOT a plain
//     zero. The +0x00 word's prior value is whatever the allocation left there.
//   * stores 3 (li r10,3) to the word at +0x54 (result[21]).
// Loop is reconstructed as a real for-loop; the strength-reduced mask is a plain &.
// =====================================================================================

#include "types.hpp"

class CWMVMBMode
{
public:
    CWMVMBMode();

private:
    u32 muFlags00;        // +0x00  read-modify-write: &= 0x1FFFFFFF (clear top 3 bits)
    u32 muWords[6];       // +0x04..+0x18  six words zeroed by the ctor loop
    u8  mPad1C[0x54 - 0x1C]; // +0x1C..+0x53  not touched by the ctor
    u32 muField54;        // +0x54  set to 3
};

// Mask applied to the +0x00 flags word: clear the top three bits.
static const u32 KU_MBMODE_FLAG_MASK = 0x1FFFFFFFu;

// CWMVMBMode::CWMVMBMode @0x82A7EEB0.
CWMVMBMode::CWMVMBMode()
{
    for (int liWord = 0; liWord < 6; ++liWord)
    {
        muWords[liWord] = 0;
    }
    muFlags00 &= KU_MBMODE_FLAG_MASK;
    muField54 = 3;
}
