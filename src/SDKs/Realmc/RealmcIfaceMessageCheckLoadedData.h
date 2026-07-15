#pragma once

#include "types.hpp"
#include "SDKs/Realmc/RealmcCore.h"          // RealmcCore::IRunnableTask base + FreeMemSize

// ===========================================================================
// RealmcIface::MessageCheckLoadedData -- a game-facing Realmc interface message
// that asks its target to check the just-loaded save data. Reconstructed from
// BURNOUT_X360_ARTIST.XEX. Structurally identical to RealmcIface::MessageBootupDone
// (RealmcIfaceMessages.h): a concrete RealmcCore::IRunnableTask subclass whose
// Apply() forwards the notification into a handler/target vtable slot, and whose
// compiler-generated `scalar deleting destructor' chains the IRunnableTask base
// dtor + the sized Realmc free.
//
// This header is the canonical OWNING home for the two reconstructed functions:
//
//     RealmcIface::MessageCheckLoadedData::Apply                       @ 0x82B56E78
//     RealmcIface::MessageCheckLoadedData::`scalar deleting destructor' @ 0x82B56E98
//        (compiler-generated from the virtual dtor + the class operator delete)
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below is
// reconstructed purely from the X360 pseudocode + asm. `Realmc` is a vendor library
// boundary, so its identifiers (RealmcIface, MessageCheckLoadedData, Apply) are
// preserved verbatim per the naming convention.
//
// LAYOUT (from the deleting-dtor free size @0x82B56E98):
//   The `scalar deleting destructor' frees exactly 16 (0x10) bytes -- the size of
//   the RealmcCore::IRunnableTask base alone (RefCount vtable@+0x00 + refcount@+0x04
//   + mpContext@+0x08 + mpMemcardState@+0x0C). MessageCheckLoadedData therefore adds
//   NO own data members; it only installs its own final vtable off_8214894C (stored
//   into *this by the deleting dtor). One vtable == one vptr. (On the PC target the
//   pointer members widen; size is not byte-matched, per the semantic-parity rule.)
// ===========================================================================

namespace RealmcIface
{

class MessageCheckLoadedData;

// ---------------------------------------------------------------------------
// IRealmcCheckLoadedDataTarget -- the object MessageCheckLoadedData::Apply()
// dispatches into. Its vtable slot +0x10 (slot 4) accepts the message and handles
// the "check loaded data" notification. The reserved virtuals below pin the
// dispatched method to byte offset 0x10 (slot 0 == dtor at +0x00, so slots 1..3
// fill +0x04 .. +0x0C and OnCheckLoadedData lands at +0x10).
//
// Apply asm @ 0x82B56E78:
//     mr    r11, r4          ; r11 = lpTarget (a2)
//     mr    r4,  r3          ; r4  = lpThis   (a1, the message)
//     mr    r3,  r11         ; r3  = lpTarget (now the dispatch `this`)
//     lwz   r10, 0(r11)      ; r10 = lpTarget->vtable
//     lwz   r11, 0x10(r10)   ; r11 = vtable[+0x10]  (slot 4)
//     mtctr r11 ; bctr       ; tail-call lpTarget->[+0x10](lpTarget, lpThis)
// i.e. lpTarget->OnCheckLoadedData(this). The pseudocode `(*(*a2 + 16))(a2, a1)`
// is the same construct (16 == 0x10).
// ---------------------------------------------------------------------------
class IRealmcCheckLoadedDataTarget
{
public:
    virtual ~IRealmcCheckLoadedDataTarget() {}                       // +0x00
    virtual void Reserved01() = 0;  virtual void Reserved02() = 0;   // +0x04 +0x08
    virtual void Reserved03() = 0;                                   // +0x0C
    // +0x10 (slot 4) -- apply the "check loaded data" notification.
    virtual int OnCheckLoadedData(MessageCheckLoadedData* lpMessage) = 0; // FLAG: slot name inferred
};

// ---------------------------------------------------------------------------
// RealmcIface::MessageCheckLoadedData -- concrete RealmcCore::IRunnableTask, with
// its own final vtable off_8214894C. Mirrors RealmcIface::MessageBootupDone.
// ---------------------------------------------------------------------------
class MessageCheckLoadedData : public RealmcCore::IRunnableTask
{
public:
    // @0x82B56E78 (body in RealmcIfaceMessageCheckLoadedData.cpp) -- dispatch this
    // notification into the target's vtable slot +0x10 (slot 4): the X360 keeps the
    // message in `this` (r3) and passes the target in r4, then swaps so the target
    // becomes the dispatch `this` and the message its argument.
    int Apply(IRealmcCheckLoadedDataTarget* lpTarget);

    // Backs the X360 `scalar deleting destructor' @0x82B56E98: it restores this
    // class's vtable (off_8214894C), chains the RealmcCore::IRunnableTask base
    // dtor, then frees 16 bytes when the delete flag bit0 is set. The virtual dtor
    // + the class operator delete below reproduce it compiler-generated.
    virtual ~MessageCheckLoadedData() {}
    static void operator delete(void* lpBlock, size_t luSize)
    {
        RealmcCore::FreeMemSize(lpBlock, static_cast<u32>(luSize));
    }
};

} // namespace RealmcIface
