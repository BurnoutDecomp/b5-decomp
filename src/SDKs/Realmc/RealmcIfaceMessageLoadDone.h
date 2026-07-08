#pragma once

// ===========================================================================
// RealmcIface::MessageLoadDone -- a Realmc save/load interface message that
// notifies its target that a memory-card load has finished. Part of the
// RealmcIface message family in BURNOUT_X360_ARTIST.XEX; structurally it is a
// RealmcCore::MessageString subclass (it carries an id + an owned string, plus
// three of its own trailing words) whose Apply dispatches the notification into
// a target object.
//
// This header is the canonical OWNING home for the reconstructed members:
//
//     RealmcIface::MessageLoadDone::Apply                      @ 0x82B570B8
//     RealmcIface::MessageLoadDone::`vector deleting destructor' @ 0x82B570D8
//        (compiler-generated from the virtual dtor + the class operator delete)
//
// BLOCKED (honest gap, NOT fabricated):
//     RealmcIface::MessageLoadDone::MessageLoadDone @ 0x82B57018 -- the ctor
//     builds a TEMPORARY string range on the stack and hands it to the
//     RealmcCore::MessageString base ctor. That temp is produced by two callees
//     the dossier could not identify/home:
//       * an un-recovered EASTL `basic_string` construction helper (the dossier
//         renders it only as `STUB(&temp, "EASTL basic_string", pText)`), and
//       * `RealmcCore::allocator<char>::RangeInitialize(&range, pBegin, pEnd)`
//         -- the 8-bit sibling of the char16_t String16::RangeInitialize homed in
//         RealmcContainers (@0x82B55580); the char instantiation used here is not
//         yet homed and the temp basic_string layout it reads cannot be grounded.
//     Reproducing the ctor faithfully would require fabricating both callees'
//     signatures and the temp-string plumbing, so it is left BLOCKED. The class
//     LAYOUT it establishes IS grounded from its stores and is modelled below.
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below
// is reconstructed purely from the X360 pseudocode + asm. `Realmc` is a vendor
// library boundary, so its identifiers (RealmcIface, MessageLoadDone, Apply) are
// preserved verbatim per the naming convention.
// ===========================================================================

#include "types.hpp"
#include "SDKs/Realmc/RealmcCore.h"   // RealmcCore::MessageString base + FreeMemSize

namespace RealmcIface
{

class MessageLoadDone;

// ---------------------------------------------------------------------------
// IRealmcLoadDoneTarget -- the object MessageLoadDone::Apply() dispatches into.
// Its vtable slot +0x30 (12) accepts the message and handles the load-finished
// notification. The padding virtuals below pin the dispatched method to byte
// offset 0x30 (slot 12 for 4-byte X360 pointers: slot 0 == dtor at +0x00, so
// the 11 reserved slots fill +0x04 .. +0x2C and OnLoadDone lands at +0x30).
//
// Apply asm @ 0x82B570B8:
//     mr   r11, r4            ; r11 = pTarget (a2)
//     mr   r4,  r3            ; r4  = pThis   (a1, the message)
//     mr   r3,  r11           ; r3  = pTarget (now `this`)
//     lwz  r10, 0(r11)        ; r10 = pTarget->vtable
//     lwz  r11, 0x30(r10)     ; r11 = vtable[+0x30]  (slot 12)
//     mtctr r11 ; bctr        ; tail-call pTarget->[+0x30](pTarget, pThis)
// i.e. pTarget->OnLoadDone(pThis). The pseudocode `(*(*a2 + 48))(a2, a1)` is the
// same construct (48 == 0x30).
// ---------------------------------------------------------------------------
class IRealmcLoadDoneTarget
{
public:
    virtual ~IRealmcLoadDoneTarget() {}              // +0x00
    virtual void Reserved01() = 0;  virtual void Reserved02() = 0;  // +0x04 +0x08
    virtual void Reserved03() = 0;  virtual void Reserved04() = 0;  // +0x0C +0x10
    virtual void Reserved05() = 0;  virtual void Reserved06() = 0;  // +0x14 +0x18
    virtual void Reserved07() = 0;  virtual void Reserved08() = 0;  // +0x1C +0x20
    virtual void Reserved09() = 0;  virtual void Reserved10() = 0;  // +0x24 +0x28
    virtual void Reserved11() = 0;                                  // +0x2C
    // +0x30 (slot 12) -- apply the load-finished notification.
    virtual int OnLoadDone(MessageLoadDone* pMessage) = 0;   // FLAG: slot name inferred
};

// ---------------------------------------------------------------------------
// RealmcIface::MessageLoadDone -- derives RealmcCore::MessageString (own final
// vtable off_8214896C).
//
// LAYOUT (from the ctor stores @0x82B57018 + the deleting-dtor free size):
//   +0x00 .. +0x1B  RealmcCore::MessageString base (RefCount vtable/refcount +
//                   muId@+8 + maText RealmcString@+0xC) -- 28 bytes (0x1C) on X360,
//                   confirmed by MessageString::~MessageString @0x82C46028 freeing
//                   28 bytes. The three own words follow the base directly (no pad).
//   +0x1C           muField1C -- the ctor's 5th argument (stw r27,0x1C(this)).
//   +0x20           muField20 -- the ctor's 6th argument (stw r26,0x20(this)).
//   +0x24           mpText    -- the ctor's `const char*` text argument, stored
//                   verbatim (stw r28,0x24(this)); the same pointer the temp
//                   MessageString text is built from.
//
// sizeof == 0x28 (40) on X360 -- the `vector deleting destructor' @0x82B570D8
// frees 40 bytes, i.e. the 28-byte base + the three own 4-byte words. (On the PC
// target the pointer members widen; size is not byte-matched, per the project's
// semantic-parity rule.)
// ---------------------------------------------------------------------------
class MessageLoadDone : public RealmcCore::MessageString
{
public:
    // @ 0x82B570B8 (body in RealmcIfaceMessageLoadDone.cpp) -- dispatch this
    // notification into the target's vtable slot +0x30 (12): the X360 keeps the
    // message in `this` (r3) and passes the target in r4, then swaps so the
    // target becomes the dispatch `this` and the message its argument.
    int Apply(IRealmcLoadDoneTarget* pTarget);

    // Backs the X360 `vector deleting destructor' @0x82B570D8: it restores this
    // class's vtable (off_8214896C), chains the RealmcCore::MessageString base
    // dtor, then frees 40 bytes when the delete flag bit0 is set. The virtual
    // dtor + the class operator delete below reproduce it compiler-generated.
    virtual ~MessageLoadDone() {}
    static void operator delete(void* lpBlock, size_t luSize)
    {
        RealmcCore::FreeMemSize(lpBlock, static_cast<u32>(luSize));
    }

private:
    // The three own words follow the 28-byte (0x1C) MessageString base directly;
    // there is NO +0x18 pad (that offset is base territory -- maText's tail).
    u32         muField1C;  // +0x1C  (ctor arg 5)
    u32         muField20;  // +0x20  (ctor arg 6)
    const char* mpText;     // +0x24  (ctor `const char*` text argument)
};

} // namespace RealmcIface
