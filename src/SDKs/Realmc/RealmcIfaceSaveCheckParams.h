#pragma once

// ===========================================================================
// RealmcIface::SaveCheckParams -- the parameter record the X360 save/load
// interface hands to Realmc's "check save" / "save" tasks. It owns a
// heap-allocated array of copy-constructed SaveReq records (see RealmcSaveReq.h,
// the sibling home for the 356-byte SaveReq). Part of the RealmcIface family in
// BURNOUT_X360_ARTIST.XEX (siblings: RealmcIface::SaveReq, RealmcIface::CardData
// in RealmcCardData.h, the RealmcCore primitives in RealmcCore.h).
//
// This header is the canonical OWNING home for the struct and its two
// reconstructed member functions:
//
//     RealmcIface::SaveCheckParams::SaveCheckParams  @ 0x82B51E38   (ctor)
//     RealmcIface::SaveCheckParams::~SaveCheckParams @ 0x82B51F88   (dtor)
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below
// is reconstructed purely from the X360 pseudocode + asm. `Realmc` is a vendor
// library boundary, so its identifiers (RealmcIface, SaveCheckParams, SaveReq)
// are preserved verbatim per the naming convention.
//
// LAYOUT (from the ctor / dtor asm):
//
//   +0x000  mCount   -- s32, the number of SaveReq slots (ctor arg a2). Compared
//                       with signed cmpwi/cmpw + ble/blt, so signed.
//   +0x004  mppReqs  -- SaveReq**, a heap array of mCount SaveReq* pointers
//                       (allocated "SaveReq array", 4*mCount bytes). Each slot
//                       points at a heap-allocated 356-byte SaveReq copy.
//
// sizeof(SaveCheckParams) == 0x08 (8) bytes. No vtable (plain params record).
// ===========================================================================

#include "SDKs/Realmc/RealmcSaveReq.h"  // RealmcIface::SaveReq (0x164-byte record)
#include "types.hpp"                    // s32 primitive

namespace RealmcIface
{

class SaveCheckParams
{
public:
    // @ 0x82B51E38 -- store nCount, allocate the SaveReq* array (when nCount),
    //                 then for each slot allocate a 356-byte SaveReq and
    //                 copy-construct it from paSources[i]; a failed per-slot
    //                 allocation stores a null pointer.
    SaveCheckParams(s32 nCount, SaveReq* const* paSources);

    // @ 0x82B51F88 -- free each non-null SaveReq (356 bytes) then free the
    //                 SaveReq* array (4*mCount bytes) when present.
    ~SaveCheckParams();

    s32       mCount;   // +0x000
    SaveReq** mppReqs;  // +0x004
};
// sizeof(SaveCheckParams) == 0x08 (8) bytes.

} // namespace RealmcIface
