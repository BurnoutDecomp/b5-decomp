#include "SDKs/Realmc/RealmcDataBuffer.h"

// ===========================================================================
// RealmcIface::DataBuffer -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODY both come from the X360 asm. See
// RealmcDataBuffer.h for the {pointer, size} layout.
// ===========================================================================

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// DataBuffer::operator= @ 0x82B51928
//
//   lwz r11, 0(r4) ; stw r11, 0(r3)   -> mpData = rOther.mpData
//   lwz r11, 4(r4) ; stw r11, 4(r3)   -> muSize = rOther.muSize
//   blr                                -> return this
//
// A flat two-word memberwise copy of the {pointer, size} descriptor.
// ---------------------------------------------------------------------------
DataBuffer& DataBuffer::operator=(const DataBuffer& rOther)
{
    mpData = rOther.mpData;
    muSize = rOther.muSize;
    return *this;
}

} // namespace RealmcIface
