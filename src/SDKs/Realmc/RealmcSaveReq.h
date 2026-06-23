#pragma once

// ===========================================================================
// RealmcIface::SaveReq -- a Realmc save/load "save request" record used by the
// X360 memory-card / save-load interface (the RealmcIface family in
// BURNOUT_X360_ARTIST.XEX; sibling to RealmcIface::CardData in RealmcCardData.h
// and the RealmcCore primitives in RealmcCore.h).
//
// This header is the canonical OWNING home for the SaveReq struct and its one
// reconstructed member function:
//
//     RealmcIface::SaveReq::SaveReq  @ 0x82B51D60   (ctor)
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below
// is reconstructed purely from the X360 pseudocode + asm. `Realmc` is a vendor
// library boundary, so its identifiers (RealmcIface, SaveReq) are preserved
// verbatim per the naming convention.
//
// LAYOUT (from the ctor asm):
//
//   +0x000  maHead     -- a 32-byte head block, memcpy'd wholesale from the
//                         source descriptor; the trailing byte (+0x1F) is then
//                         zero-stored (a flag/terminator). Opaque payload, so a
//                         raw byte block rather than fabricated named members.
//   +0x020  maParams   -- a 0x138-byte embedded "entry content" params
//                         sub-object copy-constructed from the source params via
//                         the Realmc params copy helper (X360 sub_82B51D10, which
//                         lives in the RealmcIface::EntryContentName TU). Modelled
//                         here as an opaque byte block; the copy is performed by
//                         the external helper declared below so this TU does not
//                         fabricate that sibling type's internals.
//   +0x158  muField158 -- u32, set from ctor arg a4.
//   +0x15C  muField15C -- u32, set from ctor arg a5.
//   +0x160  muField160 -- u32, set from *a6 (the first word of the a6 source).
//
// sizeof(SaveReq) == 0x164 (356) bytes.
// ===========================================================================

#include <cstddef>
#include <cstdint>

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// The Realmc "entry content params" copy helper (X360 sub_82B51D10). It
// copy-constructs the embedded params block at +0x20 from a source params
// object (copying its EntryContentName title/filename plus two trailing words).
// It is defined by the RealmcIface::EntryContentName TU; declared here so
// SaveReq's ctor can invoke it across the TU boundary without reconstructing
// that sibling type's body. (Un-homed dependency -- see home_notes.)
// ---------------------------------------------------------------------------
void RealmcCopyEntryContentParams(void* pDst, const void* pSrc);

class SaveReq
{
public:
    // @ 0x82B51D60 -- copy-construct the embedded params from pParamsSource,
    //                 store the three trailing words (a4, a5, *a6), copy the
    //                 32-byte head from pHeadSource, then zero maHead[0x1F].
    SaveReq(const void* pHeadSource,
            const void* pParamsSource,
            std::uint32_t uField158,
            std::uint32_t uField15C,
            const std::uint32_t* pField160Source);

    std::uint8_t  maHead[0x20];    // +0x000  32-byte head (last byte zeroed)
    std::uint8_t  maParams[0x138]; // +0x020  embedded entry-content params
    std::uint32_t muField158;      // +0x158
    std::uint32_t muField15C;      // +0x15C
    std::uint32_t muField160;      // +0x160
};
// sizeof(SaveReq) == 0x164 (356) bytes.

} // namespace RealmcIface
