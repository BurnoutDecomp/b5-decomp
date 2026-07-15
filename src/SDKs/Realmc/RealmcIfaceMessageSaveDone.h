#pragma once

// ===========================================================================
// RealmcIface::MessageSaveDone -- a Realmc save/load interface message that
// notifies its target that a memory-card SAVE has finished. The save-side
// sibling of RealmcIface::MessageLoadDone: same RealmcCore::MessageString base
// (it carries an id + an owned string, plus one trailing own word), whose Apply
// dispatches the notification into a target object -- but at a different target
// vtable slot and with a different own-word count / final vtable.
//
// This header is the canonical OWNING home for the reconstructed members:
//
//     RealmcIface::MessageSaveDone::Apply                       @ 0x82B56088
//     RealmcIface::MessageSaveDone::`vector deleting destructor' @ 0x82B560A8
//        (compiler-generated from the virtual dtor + the class operator delete)
//
// There is no ctor in this TU's ledger (unlike MessageLoadDone, whose ctor was
// present-but-BLOCKED): the only two X360 functions attributed to the class are
// Apply and the deleting destructor, so no constructor is reconstructed here.
// The class LAYOUT below is grounded from the deleting destructor's free size
// (32 bytes) against the 28-byte MessageString base.
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below
// is reconstructed purely from the X360 pseudocode + asm. `Realmc` is a vendor
// library boundary, so its identifiers (RealmcIface, MessageSaveDone, Apply) are
// preserved verbatim per the naming convention.
// ===========================================================================

#include "types.hpp"
#include "SDKs/Realmc/RealmcCore.h"   // RealmcCore::MessageString base + FreeMemSize

namespace RealmcIface
{

class MessageSaveDone;

// ---------------------------------------------------------------------------
// IRealmcSaveDoneTarget -- the object MessageSaveDone::Apply() dispatches into.
// Its vtable slot +0x34 (13) accepts the message and handles the save-finished
// notification. The padding virtuals below pin the dispatched method to byte
// offset 0x34 (slot 13 for 4-byte X360 pointers: slot 0 == dtor at +0x00, so
// the 12 reserved slots fill +0x04 .. +0x30 and OnSaveDone lands at +0x34).
//
// Apply asm @ 0x82B56088:
//     mr   r11, r4            ; r11 = pTarget (a2)
//     mr   r4,  r3            ; r4  = pThis   (a1, the message)
//     mr   r3,  r11           ; r3  = pTarget (now `this`)
//     lwz  r10, 0(r11)        ; r10 = pTarget->vtable
//     lwz  r11, 0x34(r10)     ; r11 = vtable[+0x34]  (slot 13)
//     mtctr r11 ; bctr        ; tail-call pTarget->[+0x34](pTarget, pThis)
// i.e. pTarget->OnSaveDone(pThis). The pseudocode `(*(*a2 + 52))(a2, a1)` is the
// same construct (52 == 0x34).
// ---------------------------------------------------------------------------
class IRealmcSaveDoneTarget
{
public:
    virtual ~IRealmcSaveDoneTarget() {}              // +0x00
    virtual void Reserved01() = 0;  virtual void Reserved02() = 0;  // +0x04 +0x08
    virtual void Reserved03() = 0;  virtual void Reserved04() = 0;  // +0x0C +0x10
    virtual void Reserved05() = 0;  virtual void Reserved06() = 0;  // +0x14 +0x18
    virtual void Reserved07() = 0;  virtual void Reserved08() = 0;  // +0x1C +0x20
    virtual void Reserved09() = 0;  virtual void Reserved10() = 0;  // +0x24 +0x28
    virtual void Reserved11() = 0;  virtual void Reserved12() = 0;  // +0x2C +0x30
    // +0x34 (slot 13) -- apply the save-finished notification.
    virtual int OnSaveDone(MessageSaveDone* pMessage) = 0;   // FLAG: slot name inferred
};

// ---------------------------------------------------------------------------
// RealmcIface::MessageSaveDone -- derives RealmcCore::MessageString (own final
// vtable off_821488D0).
//
// LAYOUT (from the `vector deleting destructor' @0x82B560A8 free size):
//   +0x00 .. +0x1B  RealmcCore::MessageString base (RefCount vtable/refcount +
//                   muId@+8 + maText RealmcString@+0xC) -- 28 bytes (0x1C) on X360,
//                   confirmed by MessageString::~MessageString @0x82C46028 freeing
//                   28 bytes. One own word follows the base directly (no pad).
//   +0x1C           muField1C -- the single own trailing word. FLAG: this TU has
//                   NO ctor in its ledger, so the word's ROLE/TYPE is not attested
//                   (nothing here reads or writes it); its EXISTENCE is grounded
//                   purely from the deleting destructor's 32-byte free against the
//                   28-byte base. Modelled as one opaque u32 by size.
//
// sizeof == 0x20 (32) on X360 -- the `vector deleting destructor' @0x82B560A8
// frees 32 bytes, i.e. the 28-byte base + one own 4-byte word. (On the PC target
// the pointer members widen; size is not byte-matched, per the project's
// semantic-parity rule.)
// ---------------------------------------------------------------------------
class MessageSaveDone : public RealmcCore::MessageString
{
public:
    // @ 0x82B56088 (body in RealmcIfaceMessageSaveDone.cpp) -- dispatch this
    // notification into the target's vtable slot +0x34 (13): the X360 keeps the
    // message in `this` (r3) and passes the target in r4, then swaps so the
    // target becomes the dispatch `this` and the message its argument.
    int Apply(IRealmcSaveDoneTarget* pTarget);

    // Backs the X360 `vector deleting destructor' @0x82B560A8: it restores this
    // class's vtable (off_821488D0), chains the RealmcCore::MessageString base
    // dtor, then frees 32 bytes when the delete flag bit0 is set. The virtual
    // dtor + the class operator delete below reproduce it compiler-generated.
    virtual ~MessageSaveDone() {}
    static void operator delete(void* lpBlock, size_t luSize)
    {
        RealmcCore::FreeMemSize(lpBlock, static_cast<u32>(luSize));
    }

private:
    // The single own word follows the 28-byte (0x1C) MessageString base directly;
    // there is NO +0x18 pad (that offset is base territory -- maText's tail).
    u32 muField1C;  // +0x1C  (own trailing word; role/type unattested -- see LAYOUT)
};

} // namespace RealmcIface
