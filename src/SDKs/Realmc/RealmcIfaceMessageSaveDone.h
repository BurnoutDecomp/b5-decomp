#pragma once

// ===========================================================================
// RealmcIface::MessageSaveDone -- a Realmc save/load interface message that
// notifies the message processor that a memory-card SAVE has finished. The save-side
// sibling of RealmcIface::MessageLoadDone: same RealmcCore::MessageString base
// (it carries an id + an owned string, plus one trailing own word), whose Apply
// hands the notification to the shared RealmcCore::IMessageProcessor -- but at a
// different processor slot and with a different own-word count / final vtable.
//
// This header is the canonical OWNING home for the reconstructed members:
//
//     RealmcIface::MessageSaveDone::Apply                       @ 0x82B56088
//     RealmcIface::MessageSaveDone::`vector deleting destructor' @ 0x82B560A8
//        (compiler-generated from the virtual dtor + the class operator delete)
//
// There is no ctor in this TU's ledger (unlike MessageLoadDone, whose ctor is
// in RealmcIfaceMessageLoadDone.cpp): the only two console functions attributed to the class are
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
#include "SDKs/Realmc/RealmcCore.h"   // RealmcCore::MessageString base, IMessageProcessor, FreeMemSize

namespace RealmcIface
{

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
    // Message vtable +8 -- pProcessor->ProcessMessage(this), the processor's
    //                       +0x34 slot (the thunk swaps its two arguments so
    //                       the processor becomes `this`).
    void Apply(RealmcCore::IMessageProcessor* pProcessor) override;

    // Backs the X360 `vector deleting destructor' @0x82B560A8: it restores this
    // class's vtable (off_821488D0), chains the RealmcCore::MessageString base
    // dtor, then frees 32 bytes when the delete flag bit0 is set. The virtual
    // dtor + the class operator delete below reproduce it compiler-generated.
    ~MessageSaveDone() override {}
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
