#pragma once

// ===========================================================================
// XAUDIO::CObjectRefCount -- the intrusive reference-counted base class shared
// by the XAudio runtime objects in BURNOUT_X360_ARTIST.XEX. This header is the
// canonical OWNING home for:
//
//     XAUDIO::CObjectRefCount::AddRef                       @ 0x82C2F008
//     XAUDIO::CObjectRefCount::Release                      @ 0x8295F428
//     XAUDIO::CObjectRefCount::AbsoluteRelease              @ 0x8295F480
//     XAUDIO::CObjectRefCount::`scalar deleting destructor' @ 0x82961008
//
// There is no reference source and no DWARF for this TU, so the SHAPE below
// is reconstructed purely from the X360 asm of these four methods. `XAUDIO` is
// an external middleware boundary, so its identifiers are preserved verbatim per
// the naming convention.
//
// LAYOUT (grounded in the asm load/store displacements):
//   +0x00  mpVTable   -- vtable pointer (lwz 0(r3) in Release/AbsoluteRelease;
//                        stw off_82108FF4,0(r31) in the deleting destructor).
//   +0x04  mRefCount  -- 32-bit reference count (lwz/stw 4(r3) in AddRef/Release).
//
// VTABLE slots attested by this TU:
//   [0]  (@+0x00) -- the `scalar deleting destructor' (AbsoluteRelease tail-calls
//                    `(*vtable[0])(this, 1)`, i.e. destruct + free).
//   [3]  (@+0x0C) -- the same deleting destructor invoked by Release with the
//                    delete flag implied (Release calls `(*vtable[3])(this)`).
// Both attested slots dispatch to the deleting destructor; the intervening slots
// are not exercised by this TU and are modelled as opaque pointer slots.
// ===========================================================================

#include "types.hpp"

namespace XAUDIO
{

class CObjectRefCount
{
public:
    // The reference-counted-object vtable. Only slot[0] (and slot[3], which the
    // X360 build routes to the same deleting-destructor thunk) are attested by
    // this TU; the rest are opaque here.
    struct VTable
    {
        // [0] @+0x00 -- `scalar deleting destructor'(this, char aDeleteFlag).
        void* (*mpScalarDeletingDtor)(CObjectRefCount* apSelf, char aDeleteFlag);
        void* mpSlot04; // [1] @+0x04 opaque
        void* mpSlot08; // [2] @+0x08 opaque
        // [3] @+0x0C -- destructor slot Release dispatches through at refcount 0.
        void  (*mpDestruct)(CObjectRefCount* apSelf);
    };

    // @ 0x82C2F008 -- pre-increment mRefCount and return the new value.
    int AddRef();

    // @ 0x8295F428 -- decrement mRefCount; when it reaches 0 dispatch the
    // destructor through vtable slot [3]. Returns the post-decrement count
    // (0 once destroyed).
    int Release();

    // @ 0x8295F480 -- static helper: if `apObject` is non-null, destruct + free
    // it by tail-calling its deleting destructor (vtable[0]) with the delete
    // flag set. Returns the object pointer (null when `apObject` was null).
    static void* AbsoluteRelease(CObjectRefCount* apObject);

    // @ 0x82961008 -- `scalar deleting destructor': re-seat the vtable, and when
    // the low bit of `aDeleteFlag` is set, free the object through the XAudio
    // XMem memory manager. Returns `this`.
    void* ScalarDeletingDestructor(char aDeleteFlag);

    const VTable* mpVTable;   // +0x00
    int           mRefCount;  // +0x04
};

} // namespace XAUDIO
