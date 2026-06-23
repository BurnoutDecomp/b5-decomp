#include "SDKs/Realmc/RealmcGameInfo.h"

#include <cstring>  // std::memcpy -- the ctor head copy is a literal memcpy.

// ===========================================================================
// RealmcIface::GameInfo -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODY both come from the X360 asm. See
// RealmcGameInfo.h for the layout.
//
// The 0x42-byte head copy is pinned to (sizeof maName + sizeof muField40) so
// the host build stays self-consistent while reproducing the X360's 66-byte
// memcpy length exactly.
// ===========================================================================

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// GameInfo::GameInfo @ 0x82B51940
//
//   li   r30, 0
//   stw  r30, 0x44(r31)                         -> muField44 = 0
//   stw  r30, 0x48(r31)                         -> muField48 = 0
//   stw  r30, 0x4C(r31)                         -> muField4C = 0
//   li   r5, 0x42 ; bl memcpy                    -> memcpy(this, pSource, 66)
//   sth  r30, 0x40(r31)                          -> muField40 = 0
//
// The three trailing u32s are zeroed first, then 66 bytes (maName + muField40)
// are copied from the source, then muField40 is re-zeroed. Reproduced
// store-for-store.
// ---------------------------------------------------------------------------
GameInfo::GameInfo(const void* pSource)
{
    muField44 = 0;
    muField48 = 0;
    muField4C = 0;
    std::memcpy(maName, pSource, sizeof(maName) + sizeof(muField40)); // 66 bytes
    muField40 = 0;
}

} // namespace RealmcIface
