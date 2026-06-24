#pragma once

// ===========================================================================
// XAUDIO::CBatchAllocatedObject -- a ref-counted, XMem-batch-allocated XAudio
// base object in BURNOUT_X360_ARTIST.XEX. This header is the canonical OWNING
// home for:
//
//     XAUDIO::CBatchAllocatedObject::AbsoluteRelease           @ 0x82C2D828
//     XAUDIO::CBatchAllocatedObject::`scalar deleting destructor' @ 0x82C2DBB8
//
// There is no reference source and no DWARF for this TU, so the SHAPE is
// reconstructed from the X360 asm. `XAUDIO` is an external middleware boundary,
// so its identifiers are preserved verbatim per the naming convention.
//
// Layout / behaviour from the asm:
//   - Polymorphic, single inheritance: the deleting destructor @ 0x82C2DBB8
//     stores the derived vtable (off_821B5514) into *this, releases the held
//     object at +8 via its vtable slot 1, nulls +8, then stores the base vtable
//     (off_82108FF4) into *this (the base-subobject dtor) and, when the deleting
//     flag's low bit is set, frees `this` through the XAudio XMem manager
//     (XMemFree(&unk_83222C40, this, 0x61820000)). The two staged vtable stores
//     are the derived-then-base destruction sequence; modelled here as one
//     virtual class (semantic parity -- the staged vptr writes are a compiler
//     artifact, not reproduced byte-for-byte).
//   - The held object at +8 is reached only through two vtable slots: slot 0
//     (offset +0) and slot 1 (offset +4). Its concrete type is not homed, so it
//     is modelled below as a FLAGGED two-slot interface (IBatchHeld) capturing
//     exactly the two virtual calls the asm makes.
//   - AbsoluteRelease @ 0x82C2D828: capture the held object (+8); if present
//     call its slot 0; invoke this->vtable[0](this, 1) -- i.e. the virtual
//     deleting destructor with the delete flag (`delete this`); then, if the
//     held object was present, call its slot 1 and return that; otherwise return
//     the deleting destructor's result. This is the lock-style "acquire / delete
//     self / release" shape.
// ===========================================================================

#include "types.hpp"

namespace XAUDIO
{

// The object held at CBatchAllocatedObject +8, reached only through vtable slots
// 0 and 1 by this TU. FLAGGED: its concrete type is not homed; this two-slot
// interface captures exactly the virtual calls the X360 code makes (slot 0 at
// +0, slot 1 at +4) so the dispatch shape is faithful.
class IBatchHeld
{
public:
    virtual void Vf0() = 0;  // vtable[0] -- called by AbsoluteRelease before self-delete
    virtual s32  Vf1() = 0;  // vtable[1] -- called on release (dtor and AbsoluteRelease); asm returns its r3
};

class CBatchAllocatedObject
{
public:
    // Out-of-line virtual destructor: releases the held object (+8) via its
    // slot 1 and nulls it. The X360 scalar deleting destructor @ 0x82C2DBB8 is
    // the compiler-synthesised thunk over this dtor + the class operator delete.
    virtual ~CBatchAllocatedObject();

    // @ 0x82C2D828 -- acquire-via-held-slot-0, delete self, release-via-held-
    // slot-1. Returns the value the asm leaves in r3 (the held slot-1 result
    // when an object was held, otherwise the deleting destructor's result).
    s32 AbsoluteRelease();

    // Class-specific deallocation routing through the XAudio XMem manager.
    void operator delete(void* pMemory);

private:
    // +4: a reserved word ahead of mpHeld in the X360 layout. Not read by either
    // function in this TU; FLAGGED -- modelled only to land mpHeld at +8.
    u32         muReserved4; // +4

    // +8: the held object released through its vtable during teardown.
    IBatchHeld* mpHeld;      // +8
};

} // namespace XAUDIO
