// =====================================================================================
// CTwoPassInfoList -- list bookkeeping for the WMV two-pass encoder's per-frame info.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for this TU.
//
//   CTwoPassInfoList::CTwoPassInfoList  @0x82AA0610
//
// The constructor zeroes four 32-bit words at +0,+4,+0xC,+0x10 (stw r11=0). The word at
// +0x08 is deliberately left untouched by the asm, so it is modelled as a separate field
// the ctor does not initialise (not part of the zero set). Only the asm-attested stores
// are emitted; sizeof covers the highest written slot.
// =====================================================================================

#include "types.hpp"

class CTwoPassInfoList
{
public:
    CTwoPassInfoList();

private:
    u32 muField00;     // +0x00  zeroed
    u32 muField04;     // +0x04  zeroed
    u32 muField08;     // +0x08  NOT initialised by the ctor (no store at +8)
    u32 muField0C;     // +0x0C  zeroed
    u32 muField10;     // +0x10  zeroed
};

// CTwoPassInfoList::CTwoPassInfoList @0x82AA0610 -- zero +0,+4,+0xC,+0x10; +8 untouched.
CTwoPassInfoList::CTwoPassInfoList()
{
    muField00 = 0;
    muField04 = 0;
    muField0C = 0;
    muField10 = 0;
}
