#pragma once

// ===========================================================================
// RealmcIface::MessageSetAutosaveDone -- a Realmc memory-card interface
// message/task that notifies its target that the "set autosave" operation has
// finished. Part of the RealmcIface message family in BURNOUT_X360_ARTIST.XEX;
// structurally it is a RealmcCore::IRunnableTask subclass (the same refcounted
// runnable-task base MessageSetActiveCardDone / MessageCheckLoadedData derive)
// whose Apply dispatches the notification into a target object.
//
// This header is the canonical OWNING home for the reconstructed members:
//
//     RealmcIface::MessageSetAutosaveDone::Apply                        @ 0x82B56B90
//     RealmcIface::MessageSetAutosaveDone::`vector deleting destructor' @ 0x82B56BB0
//        (compiler-generated from the virtual dtor + the class operator delete)
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below
// is reconstructed purely from the X360 pseudocode + asm. `Realmc` is a vendor
// library boundary, so its identifiers (RealmcIface, MessageSetAutosaveDone,
// Apply) are preserved verbatim per the naming convention.
//
// LAYOUT (from the `vector deleting destructor' free size @ 0x82B56BB0):
//   The deleting dtor frees 20 (0x14) bytes and chains straight to the
//   RealmcCore::IRunnableTask base dtor. The IRunnableTask base is 16 (0x10)
//   bytes (RefCount vtable@+0x00 + refcount@+0x04 + mpContext@+0x08 +
//   mpMemcardState@+0x0C), so this class carries exactly ONE own 4-byte member
//   at +0x10 (set by its ctor, which is NOT part of this 2-function TU):
//     +0x00 vtable (own final off_82148904) | +0x04 miRefCount (RefCount) |
//     +0x08 mpContext | +0x0C mpMemcardState | +0x10 muField10 (own).
//   (On the PC target the pointer members widen; size is not byte-matched, per
//   the semantic-parity rule -- muField10's EXISTENCE is grounded from the free
//   size, but its role/type is unrecovered because the ctor is not in this TU.)
// ===========================================================================

#include "types.hpp"
#include "SDKs/Realmc/RealmcCore.h"          // RealmcCore::IRunnableTask base + FreeMemSize

namespace RealmcIface
{

class MessageSetAutosaveDone;

// ---------------------------------------------------------------------------
// IRealmcSetAutosaveDoneTarget -- the object MessageSetAutosaveDone::Apply()
// dispatches into. Its vtable slot +0x18 (slot 6) accepts the message and
// handles the set-autosave-finished notification. The reserved virtuals below
// pin the dispatched method to byte offset 0x18 (slot 0 == dtor at +0x00, so
// slots 1..5 fill +0x04 .. +0x14 and OnSetAutosaveDone lands at +0x18).
//
// Apply asm @ 0x82B56B90:
//     mr    r11, r4          ; r11 = lpTarget (a2)
//     mr    r4,  r3          ; r4  = lpThis   (a1, the message)
//     mr    r3,  r11         ; r3  = lpTarget (now the dispatch `this`)
//     lwz   r10, 0(r11)      ; r10 = lpTarget->vtable
//     lwz   r11, 0x18(r10)   ; r11 = vtable[+0x18]  (slot 6)
//     mtctr r11 ; bctr       ; tail-call lpTarget->[+0x18](lpTarget, lpThis)
// i.e. lpTarget->OnSetAutosaveDone(this). The pseudocode `(*(*a2 + 24))(a2, a1)`
// is the same construct (24 == 0x18).
// ---------------------------------------------------------------------------
class IRealmcSetAutosaveDoneTarget
{
public:
    virtual ~IRealmcSetAutosaveDoneTarget() {}                       // +0x00
    virtual void Reserved01() = 0;  virtual void Reserved02() = 0;   // +0x04 +0x08
    virtual void Reserved03() = 0;  virtual void Reserved04() = 0;   // +0x0C +0x10
    virtual void Reserved05() = 0;                                   // +0x14
    // +0x18 (slot 6) -- apply the set-autosave-finished notification.
    virtual int OnSetAutosaveDone(MessageSetAutosaveDone* lpMessage) = 0; // FLAG: slot name inferred
};

// ---------------------------------------------------------------------------
// RealmcIface::MessageSetAutosaveDone -- derives RealmcCore::IRunnableTask (own
// final vtable off_82148904). Carries one own member word at +0x10 (see LAYOUT
// above). Mirrors RealmcIface::MessageSetActiveCardDone / MessageCheckLoadedData.
// ---------------------------------------------------------------------------
class MessageSetAutosaveDone : public RealmcCore::IRunnableTask
{
public:
    // @ 0x82B56B90 (body in RealmcIfaceMessageSetAutosaveDone.cpp) -- dispatch
    // this notification into the target's vtable slot +0x18 (slot 6): the X360
    // keeps the message in `this` (r3) and passes the target in r4, then swaps so
    // the target becomes the dispatch `this` and the message its argument.
    int Apply(IRealmcSetAutosaveDoneTarget* lpTarget);

    // Backs the X360 `vector deleting destructor' @ 0x82B56BB0: it restores this
    // class's vtable (off_82148904), chains the RealmcCore::IRunnableTask base
    // dtor, then frees 20 bytes when the delete flag bit0 is set. The virtual
    // dtor + the class operator delete below reproduce it compiler-generated.
    virtual ~MessageSetAutosaveDone() {}
    static void operator delete(void* lpBlock, size_t luSize)
    {
        RealmcCore::FreeMemSize(lpBlock, static_cast<u32>(luSize));
    }

private:
    // +0x10 -- one own member word. Its EXISTENCE is grounded from the deleting
    // dtor's 20-byte free size (IRunnableTask base is 16 bytes); its role/type is
    // unrecovered because the ctor that sets it is not part of this 2-function TU.
    u32 muField10;  // +0x10  (FLAG: role/type unrecovered -- ctor not in this TU)
};

} // namespace RealmcIface
