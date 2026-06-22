#include "SDKs/Realmc/RealmcLoadEntryInfo.h"

#include <cstring>  // std::memcpy -- the X360 operator= body is literally a memcpy of
                    // the 32-byte head plus four individual word copies.

// ===========================================================================
// RealmcIface::LoadEntryInfo -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODIES both come from the X360 asm. See
// RealmcLoadEntryInfo.h for the layout and the store-order notes.
// ===========================================================================

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// LoadEntryInfo::LoadEntryInfo @ 0x82B519E8
//
//   li  r11, 0
//   stw r11, 0x20(r3)   -> maTrailing[0] = 0
//   stw r11, 0x24(r3)   -> maTrailing[1] = 0
//   stw r11, 0x28(r3)   -> maTrailing[2] = 0
//   stw r11, 0x2C(r3)   -> maTrailing[3] = 0
//   stb r11, 0(r3)      -> maHead[0] = 0
//   blr
//
// Zero the four trailing words (in order), then zero the leading head byte. The
// rest of the head is left as-is (the ctor only touches byte +0x00).
// ---------------------------------------------------------------------------
LoadEntryInfo::LoadEntryInfo()
{
    maTrailing[0] = 0;
    maTrailing[1] = 0;
    maTrailing[2] = 0;
    maTrailing[3] = 0;
    maHead[0] = 0;
}

// ---------------------------------------------------------------------------
// LoadEntryInfo::operator= @ 0x82B51A78
//
//   lwz r11, 0x20(r4) ; stw r11, 0x20(r31)   -> maTrailing[0] = rOther.maTrailing[0]
//   lwz r11, 0x24(r4) ; stw r11, 0x24(r31)   -> maTrailing[1] = rOther.maTrailing[1]
//   lwz r11, 0x28(r4) ; stw r11, 0x28(r31)   -> maTrailing[2] = rOther.maTrailing[2]
//   lwz r11, 0x2C(r4) ; stw r11, 0x2C(r31)   -> maTrailing[3] = rOther.maTrailing[3]
//   li  r5, 0x20 ; bl memcpy                 -> memcpy(this, rOther, 32)  (the head)
//   li  r11, 0 ; stb r11, 0x1F(r31)          -> maHead[0x1F] = 0  (flag byte, last)
//   return this
//
// Store order is exact: the four trailing words first (individually), THEN the
// 32-byte head memcpy (Dst=this, Src=rOther, Size=0x20), THEN the +0x1F byte is
// cleared last.
// ---------------------------------------------------------------------------
LoadEntryInfo& LoadEntryInfo::operator=(const LoadEntryInfo& rOther)
{
    maTrailing[0] = rOther.maTrailing[0];
    maTrailing[1] = rOther.maTrailing[1];
    maTrailing[2] = rOther.maTrailing[2];
    maTrailing[3] = rOther.maTrailing[3];

    std::memcpy(maHead, rOther.maHead, 0x20);  // memcpy(this, rOther, 32)

    maHead[0x1F] = 0;                          // clear the flag byte (last)
    return *this;
}

} // namespace RealmcIface
