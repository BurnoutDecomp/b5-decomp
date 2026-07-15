#pragma once

// ===========================================================================
// RealmcIface::MessageSetActiveCardDone -- a Realmc memory-card interface
// message/task that notifies its target that the "set active card" operation
// has finished. Part of the RealmcIface message family in
// BURNOUT_X360_ARTIST.XEX; structurally it is a RealmcCore::IRunnableTask
// subclass (the same refcounted runnable-task base MessageBootupDone derives)
// whose Apply dispatches the notification into a target object.
//
// This header is the canonical OWNING home for the reconstructed members:
//
//     RealmcIface::MessageSetActiveCardDone::Apply                        @ 0x82B54F38
//     RealmcIface::MessageSetActiveCardDone::`vector deleting destructor' @ 0x82B54F58
//        (compiler-generated from the virtual dtor + the class operator delete)
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below
// is reconstructed purely from the X360 pseudocode + asm. `Realmc` is a vendor
// library boundary, so its identifiers (RealmcIface, MessageSetActiveCardDone,
// Apply) are preserved verbatim per the naming convention.
//
// LAYOUT (from the `vector deleting destructor' free size @ 0x82B54F58):
//   The deleting dtor frees 16 (0x10) bytes and chains straight to the
//   RealmcCore::IRunnableTask base dtor with NO own-member stores, so this class
//   adds NO members of its own -- sizeof == the 16-byte IRunnableTask base:
//     +0x00 vtable (own final off_8214879C) | +0x04 miRefCount (RefCount) |
//     +0x08 mpContext | +0x0C mpMemcardState. (On the PC target the pointer
//     members widen; size is not byte-matched, per the semantic-parity rule.)
// ===========================================================================

#include "types.hpp"
#include "SDKs/Realmc/RealmcCore.h"          // RealmcCore::IRunnableTask base + FreeMemSize

namespace RealmcIface
{

class MessageSetActiveCardDone;

// ---------------------------------------------------------------------------
// IRealmcSetActiveCardDoneTarget -- the object MessageSetActiveCardDone::Apply()
// dispatches into. Its vtable slot +0x04 (slot 1, immediately after the dtor)
// accepts the message and handles the set-active-card-finished notification.
//
// Apply asm @ 0x82B54F38:
//     mr   r11, r4            ; r11 = pTarget (a2)
//     mr   r4,  r3            ; r4  = pThis   (a1, the message)
//     mr   r3,  r11           ; r3  = pTarget (now `this`)
//     lwz  r10, 0(r11)        ; r10 = pTarget->vtable
//     lwz  r11, 4(r10)        ; r11 = vtable[+0x04]  (slot 1)
//     mtctr r11 ; bctr        ; tail-call pTarget->[+0x04](pTarget, pThis)
// i.e. pTarget->OnSetActiveCardDone(pThis). The pseudocode `(*(*a2 + 4))(a2, a1)`
// is the same construct.
// ---------------------------------------------------------------------------
class IRealmcSetActiveCardDoneTarget
{
public:
    virtual ~IRealmcSetActiveCardDoneTarget() {}                          // +0x00
    // +0x04 (slot 1) -- apply the set-active-card-finished notification.
    virtual int OnSetActiveCardDone(MessageSetActiveCardDone* lpMessage) = 0; // FLAG: slot name inferred
};

// ---------------------------------------------------------------------------
// RealmcIface::MessageSetActiveCardDone -- derives RealmcCore::IRunnableTask
// (own final vtable off_8214879C). Carries no own members (see LAYOUT above).
// ---------------------------------------------------------------------------
class MessageSetActiveCardDone : public RealmcCore::IRunnableTask
{
public:
    // @ 0x82B54F38 (body in RealmcIfaceMessageSetActiveCardDone.cpp) -- dispatch
    // this notification into the target's vtable slot +0x04 (slot 1): the X360
    // keeps the message in `this` (r3) and passes the target in r4, then swaps so
    // the target becomes the dispatch `this` and the message its argument.
    int Apply(IRealmcSetActiveCardDoneTarget* lpTarget);

    // Backs the X360 `vector deleting destructor' @ 0x82B54F58: it restores this
    // class's vtable (off_8214879C), chains the RealmcCore::IRunnableTask base
    // dtor, then frees 16 bytes when the delete flag bit0 is set. The virtual
    // dtor + the class operator delete below reproduce it compiler-generated.
    virtual ~MessageSetActiveCardDone() {}
    static void operator delete(void* lpBlock, size_t luSize)
    {
        RealmcCore::FreeMemSize(lpBlock, static_cast<u32>(luSize));
    }
};

} // namespace RealmcIface
