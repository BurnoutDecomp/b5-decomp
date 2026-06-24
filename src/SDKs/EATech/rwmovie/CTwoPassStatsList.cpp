// =====================================================================================
// CTwoPassStatsList -- accumulated statistics for the WMV two-pass encoder.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for this TU.
//
//   CTwoPassStatsList::CTwoPassStatsList  @0x82AA0628
//
// The constructor:
//   * loads the double constant at dbl_82001CA8 (0.0) once and stfd's it to +0x18 and
//     +0x20 -- these are 8-byte double (f64) accumulator fields, NOT the two adjacent
//     int stores the Hex-Rays output (`*(result+24)=0`, `*(result+32)=0`) suggests.
//   * stw r11=0 to four 32-bit words at +0,+4,+0xC,+0x10.
// Words at +8 and +0x14 are deliberately left untouched. Only the asm-attested stores
// (and their exact widths) are emitted.
// =====================================================================================

#include "types.hpp"

class CTwoPassStatsList
{
public:
    CTwoPassStatsList();

private:
    u32 muField00;     // +0x00  stw 0
    u32 muField04;     // +0x04  stw 0
    u32 muField08;     // +0x08  NOT initialised (no store)
    u32 muField0C;     // +0x0C  stw 0
    u32 muField10;     // +0x10  stw 0
    u32 muField14;     // +0x14  NOT initialised (no store)
    f64 mfStat18;      // +0x18  stfd 0.0 (8-byte double)
    f64 mfStat20;      // +0x20  stfd 0.0 (8-byte double)
};

// CTwoPassStatsList::CTwoPassStatsList @0x82AA0628.
CTwoPassStatsList::CTwoPassStatsList()
{
    mfStat18 = 0.0;
    mfStat20 = 0.0;
    muField00 = 0;
    muField04 = 0;
    muField0C = 0;
    muField10 = 0;
}
