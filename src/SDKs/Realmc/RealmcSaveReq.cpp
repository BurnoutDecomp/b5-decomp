#include "SDKs/Realmc/RealmcSaveReq.h"

#include <cstring>  // std::memcpy -- the 32-byte head copy is a literal memcpy.

// ===========================================================================
// RealmcIface::SaveReq -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODY both come from the X360 asm. See
// RealmcSaveReq.h for the layout and the params-copy helper.
//
// The 32-byte head copy length is pinned to sizeof(maHead) so the host build
// stays self-consistent while reproducing the X360's 0x20-byte memcpy exactly.
// ===========================================================================

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// SaveReq::SaveReq @ 0x82B51D60
//
//   addi r3, r31, 0x20 ; mr r4, r5 ; bl sub_82B51D10
//                                     -> copy-construct maParams from pParamsSource
//   stw r29, 0x158(r31)               -> muField158 = uField158 (a4)
//   stw r28, 0x15C(r31)               -> muField15C = uField15C (a5)
//   lwz r11, 0(r27) ; stw r11, 0x160(r31)
//                                     -> muField160 = *pField160Source (*a6)
//   li r5, 0x20 ; mr r4, r30 ; mr r3, r31 ; bl memcpy
//                                     -> memcpy(maHead, pHeadSource, 32)
//   li r11, 0 ; stb r11, 0x1F(r31)    -> maHead[0x1F] = 0
//
// Reproduced store-for-store. The embedded params copy is delegated to the
// external Realmc params copy helper (the X360 sub_82B51D10 path through
// RealmcIface::EntryContentName), preserving the TU boundary.
// ---------------------------------------------------------------------------
SaveReq::SaveReq(const void* pHeadSource,
                 const void* pParamsSource,
                 std::uint32_t uField158,
                 std::uint32_t uField15C,
                 const std::uint32_t* pField160Source)
{
    RealmcCopyEntryContentParams(maParams, pParamsSource);  // sub_82B51D10
    muField158 = uField158;                                 // +0x158
    muField15C = uField15C;                                 // +0x15C
    muField160 = *pField160Source;                          // +0x160
    std::memcpy(maHead, pHeadSource, sizeof(maHead));       // memcpy(this, src, 32)
    maHead[0x1F] = 0;                                       // +0x1F
}

// ---------------------------------------------------------------------------
// SaveReq::SaveReq (copy)
//
// Same store order as the six-argument ctor, every value taken from rOther:
// the params block first, then muField158 / muField15C / muField160, then the
// 32-byte head memcpy, then maHead[0x1F] = 0.
// ---------------------------------------------------------------------------
SaveReq::SaveReq(const SaveReq& rOther)
{
    RealmcCopyEntryContentParams(maParams, rOther.maParams);
    muField158 = rOther.muField158;
    muField15C = rOther.muField15C;
    muField160 = rOther.muField160;
    std::memcpy(maHead, rOther.maHead, sizeof(maHead));
    maHead[0x1F] = 0;
}

} // namespace RealmcIface
