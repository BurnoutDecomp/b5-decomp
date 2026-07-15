#pragma once

// ===========================================================================
// RealmcCore::MessageClear -- a Realmc memory-card runnable-task/message that
// tells its target to "clear". Part of the RealmcCore task family in
// BURNOUT_X360_ARTIST.XEX; structurally it is a RealmcCore::IRunnableTask
// subclass (base vtable off_821BA2CC RefCount + the IRunnableTask final vtable),
// whose Apply dispatches the notification into a target object's vtable.
//
// This header is the canonical OWNING home for the reconstructed members:
//
//     RealmcCore::MessageClear::Apply                       @ 0x82B55370
//     RealmcCore::MessageClear::`scalar deleting destructor' @ 0x82B554B8
//        (compiler-generated from the virtual dtor + the class operator delete)
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below
// is reconstructed purely from the X360 pseudocode + asm. `Realmc` is a vendor
// library boundary, so its identifiers (RealmcCore, MessageClear, Apply) are
// preserved verbatim per the naming convention.
//
// MIRRORS the committed sibling RealmcIface::MessageBootupDone
// (SDKs/Realmc/RealmcIfaceMessages.h): the same IRunnableTask-derived message
// shape (Apply that forwards into a target vtable slot + a compiler-generated
// deleting destructor that chains the IRunnableTask base dtor and, when the
// delete flag is set, does the sized RealmcCore::FreeMemSize).
//
// LAYOUT: the `scalar deleting destructor' @0x82B554B8 frees 8 bytes, i.e. just
// the RefCount base (vtable ptr @+0 + miRefCount @+4). Unlike MessageBootupDone
// (which frees 12 and installs its own final vtable off_8214880C), MessageClear
// carries NO own data members and the deleting-dtor asm installs no distinct
// final vtable -- it goes straight to the IRunnableTask base dtor. (On the PC
// target pointer/count widths differ; size is not byte-matched, per the
// project's semantic-parity rule.)
//
// FLAG (abstract in C++, exactly like the committed MessageBootupDone sibling):
// the IRunnableTask base declares pure virtuals (RefCount::OnUnreferenced +
// IRunnableTask::OnTaskComplete/OnTaskRun/GetTaskType). Only Apply + the
// deleting destructor are attested for this TU; the task-body virtuals' bodies
// live in functions outside this TU and are NOT fabricated here, so the class is
// left abstract rather than inventing those overrides.
// ===========================================================================

#include "types.hpp"
#include "SDKs/Realmc/RealmcCore.h"   // RealmcCore::IRunnableTask base + FreeMemSize

namespace RealmcCore
{

class MessageClear;

// ---------------------------------------------------------------------------
// IRealmcClearTarget -- the object MessageClear::Apply() dispatches into. Its
// vtable slot +0x44 (68 == slot 17) accepts the message and handles the "clear"
// notification. The padding virtuals below pin the dispatched method to byte
// offset 0x44 (slot 0 == dtor at +0x00, so the 16 reserved slots fill
// +0x04 .. +0x40 and OnClear lands at +0x44 for 4-byte X360 pointers).
//
// Apply asm @ 0x82B55370:
//     mr   r11, r4            ; r11 = pTarget (a2)
//     mr   r4,  r3            ; r4  = pThis   (a1, the message)
//     mr   r3,  r11           ; r3  = pTarget (now `this`)
//     lwz  r10, 0(r11)        ; r10 = pTarget->vtable
//     lwz  r11, 0x44(r10)     ; r11 = vtable[+0x44]  (slot 17)
//     mtctr r11 ; bctr        ; tail-call pTarget->[+0x44](pTarget, pThis)
// i.e. pTarget->OnClear(pThis). The pseudocode `(*(*a2 + 68))(a2, a1)` is the
// same construct (68 == 0x44).
//
// FLAG: slot name (OnClear) inferred from dispatch role; only its slot offset is
// grounded by the asm. Follows the committed IRealmcIfaceHandler /
// IRealmcLoadDoneTarget reserved-slot precedent.
// ---------------------------------------------------------------------------
class IRealmcClearTarget
{
public:
    virtual ~IRealmcClearTarget() {}                               // +0x00
    virtual void Reserved01() = 0;  virtual void Reserved02() = 0; // +0x04 +0x08
    virtual void Reserved03() = 0;  virtual void Reserved04() = 0; // +0x0C +0x10
    virtual void Reserved05() = 0;  virtual void Reserved06() = 0; // +0x14 +0x18
    virtual void Reserved07() = 0;  virtual void Reserved08() = 0; // +0x1C +0x20
    virtual void Reserved09() = 0;  virtual void Reserved10() = 0; // +0x24 +0x28
    virtual void Reserved11() = 0;  virtual void Reserved12() = 0; // +0x2C +0x30
    virtual void Reserved13() = 0;  virtual void Reserved14() = 0; // +0x34 +0x38
    virtual void Reserved15() = 0;  virtual void Reserved16() = 0; // +0x3C +0x40
    // +0x44 (slot 17) -- apply the "clear" notification.
    virtual int OnClear(MessageClear* pMessage) = 0;   // FLAG: slot name inferred
};

// ---------------------------------------------------------------------------
// RealmcCore::MessageClear -- derives RealmcCore::IRunnableTask (which derives
// RefCount). No own data members (deleting dtor frees only the 8-byte base).
// ---------------------------------------------------------------------------
class MessageClear : public IRunnableTask
{
public:
    // @ 0x82B55370 (body in RealmcCoreMessageClear.cpp) -- dispatch this "clear"
    // notification into the target's vtable slot +0x44 (17): the X360 keeps the
    // message in `this` (r3) and passes the target in r4, then swaps so the
    // target becomes the dispatch `this` and the message its argument.
    int Apply(IRealmcClearTarget* pTarget);

    // Backs the X360 `scalar deleting destructor' @0x82B554B8: it chains the
    // RealmcCore::IRunnableTask base dtor, then frees 8 bytes through the Realmc
    // backend when the delete flag bit0 is set. The virtual dtor + the class
    // operator delete below reproduce it compiler-generated.
    virtual ~MessageClear() {}
    static void operator delete(void* lpBlock, size_t luSize)
    {
        RealmcCore::FreeMemSize(lpBlock, static_cast<u32>(luSize));
    }
};

} // namespace RealmcCore
