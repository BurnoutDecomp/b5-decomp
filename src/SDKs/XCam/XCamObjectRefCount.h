#pragma once

// ===========================================================================
// XCAM::CObjectRefCount -- the reference-counted polymorphic base used by the
// XCAM (X360 capture/camera) SDK objects in BURNOUT_X360_ARTIST.XEX. This
// header is the canonical OWNING home for:
//
//     XCAM::CObjectRefCount::Release                       @ 0x82980A30
//     XCAM::CObjectRefCount::`vector deleting destructor'   @ 0x82980970
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below
// is reconstructed from the X360 asm of the two methods. `XCAM` is an X360 SDK
// boundary, so its identifiers (CObjectRefCount, Release) are preserved
// verbatim per the naming convention.
//
// LAYOUT (store-for-store with the asm):
//   +0  mpVtable    -- the C++ vtable pointer; the vector deleting destructor
//                      restamps it with off_8210BC28 (this class's own vtable).
//   +4  muRefCount  -- 32-bit reference count.
//
// Release @ 0x82980A30 (asm):
//   r11 = lwz 4(r3) ; addic. r11,r11,-1 ; stw r11,4(r3)   -> --muRefCount
//   bne -> return muRefCount
//   else: r11 = lwz 0(r3) (vtable) ; li r4,1 ; r11 = lwz 0(r11) (slot 0) ;
//         mtctr ; bctrl                                    -> vtable[0](this, 1)
//         return 0
//   i.e. when the count reaches zero, invoke vtable slot 0 with deleteFlag=1 --
//   the C++ deleting destructor (`delete this`) -- and return 0.
//
// `vector deleting destructor' @ 0x82980970 (asm):
//   *this = off_8210BC28 (restamp own vtable) ; if (a2 & 1) operator delete(this)
//   ; return this.  Modelled below as the virtual destructor body + the standard
//   deleteFlag delete semantics MSVC compiles into vtable slot 0.
// ===========================================================================

#include "types.hpp"

namespace XCAM
{

class CObjectRefCount
{
public:
    // @ 0x82980970 -- the (vector) deleting destructor lives in vtable slot 0.
    // The X360 body restamps the vtable to this class's own table and, when the
    // deleteFlag bit is set, frees the storage. Modelling it as a virtual dtor
    // reproduces exactly that MSVC-emitted slot-0 deleting-destructor behaviour.
    virtual ~CObjectRefCount();

    // @ 0x82980A30 -- decrement the reference count; when it reaches zero,
    // dispatch the deleting destructor (vtable slot 0, deleteFlag=1) and return
    // 0. Otherwise return the new count.
    int Release();

    u32 muRefCount; // +4  reference count (the +0 vtable slot is implicit)
};

} // namespace XCAM
