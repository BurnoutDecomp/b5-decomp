#pragma once

#include "types.hpp"
#include "SDKs/Realmc/RealmcCore.h"   // RealmcCore::IRunnableTask base + FreeMemSize

// ===========================================================================
// RealmcIface::MessageCardRemoved -- the Realmc interface task/message raised
// when a memory card is removed. Part of the RealmcIface message family in
// BURNOUT_X360_ARTIST.XEX; structurally it is a RealmcCore::IRunnableTask
// subclass (its own final vtable off_821485D0), exactly like its committed
// sibling RealmcIface::MessageBootupDone -- see RealmcIfaceMessages.{h,cpp}.
//
// This header is the canonical OWNING home for the reconstructed members:
//
//     RealmcIface::MessageCardRemoved::Apply                       @ 0x82B51FF0
//     RealmcIface::MessageCardRemoved::`vector deleting destructor' @ 0x82B52010
//        (compiler-generated from the virtual dtor + the class operator delete)
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below
// is reconstructed purely from the X360 pseudocode + asm. `Realmc` is a vendor
// library boundary, so its identifiers (RealmcIface, MessageCardRemoved, Apply)
// are preserved verbatim per the naming convention.
//
// FLAG (minimal slice, mirrors the committed MessageBootupDone precedent): the
// target interface below is modelled only as far as the two X360-emitted bodies
// need. Its dispatch slot is pinned by Apply @0x82B51FF0 (`lwz r11,0xC(vtbl)` ==
// slot 3), following the committed IRealmcMessageTarget reserved-slot convention
// (RealmcCore.h). The class does NOT override the IRunnableTask task-body pure
// virtuals (OnTaskComplete/OnTaskRun/GetTaskType/OnUnreferenced) -- those bodies
// are not among this TU's ledger functions, so, exactly like the reviewed
// MessageBootupDone, this concrete-in-the-binary class is left abstract here
// rather than fabricating the un-homed slot bodies.
// ===========================================================================

namespace RealmcIface
{

class MessageCardRemoved;

// ---------------------------------------------------------------------------
// IRealmcCardRemovedTarget -- the object MessageCardRemoved::Apply() dispatches
// into. Its vtable slot +0x0C (3) accepts the message and handles the
// card-removed notification. The padding virtuals below pin the dispatched
// method to byte offset 0x0C (slot 3 for 4-byte X360 pointers: slot 0 == dtor
// at +0x00, so the 2 reserved slots fill +0x04 .. +0x08 and OnCardRemoved lands
// at +0x0C).
//
// Apply asm @ 0x82B51FF0:
//     mr   r11, r4            ; r11 = pTarget (a2)
//     mr   r4,  r3            ; r4  = pThis   (a1, the message)
//     mr   r3,  r11           ; r3  = pTarget (now `this`)
//     lwz  r10, 0(r11)        ; r10 = pTarget->vtable
//     lwz  r11, 0xC(r10)      ; r11 = vtable[+0x0C]  (slot 3)
//     mtctr r11 ; bctr        ; tail-call pTarget->[+0x0C](pTarget, pThis)
// i.e. pTarget->OnCardRemoved(pThis). The pseudocode `(*(*a2 + 12))(a2, a1)` is
// the same construct (12 == 0x0C).
// ---------------------------------------------------------------------------
class IRealmcCardRemovedTarget
{
public:
    virtual ~IRealmcCardRemovedTarget() {}                          // +0x00
    virtual void Reserved01() = 0;  virtual void Reserved02() = 0;  // +0x04 +0x08
    // +0x0C (slot 3) -- apply the card-removed notification.
    virtual int OnCardRemoved(MessageCardRemoved* lpMessage) = 0;   // FLAG: slot name inferred
};

// ---------------------------------------------------------------------------
// RealmcIface::MessageCardRemoved -- derives RealmcCore::IRunnableTask (own
// final vtable off_821485D0). It adds no members of its own that the two homed
// bodies read: the `vector deleting destructor' @0x82B52010 restores the class
// vtable (off_821485D0), chains RealmcCore::IRunnableTask::~IRunnableTask, then
// frees the block when the delete flag bit0 is set (the X360 frees 8 bytes; on
// the PC target the layout is not byte-matched, per the project's semantic-parity
// rule -- see the committed MessageBootupDone, which frees 12).
// ---------------------------------------------------------------------------
class MessageCardRemoved : public RealmcCore::IRunnableTask
{
public:
    // @ 0x82B51FF0 (body in RealmcIfaceMessageCardRemoved.cpp) -- dispatch this
    // notification into the target's vtable slot +0x0C (3): the X360 keeps the
    // message in `this` (r3) and passes the target in r4, then swaps so the
    // target becomes the dispatch `this` and the message its argument.
    int Apply(IRealmcCardRemovedTarget* lpTarget);

    // Backs the X360 `vector deleting destructor' @0x82B52010: it restores this
    // class's vtable (off_821485D0), chains the RealmcCore::IRunnableTask base
    // dtor, then frees the block when the delete flag bit0 is set. The virtual
    // dtor + the class operator delete below reproduce it compiler-generated.
    virtual ~MessageCardRemoved() {}
    static void operator delete(void* lpBlock, size_t luSize)
    {
        RealmcCore::FreeMemSize(lpBlock, static_cast<u32>(luSize));
    }
};

} // namespace RealmcIface
