#pragma once

// ===========================================================================
// Realmc core -- vendor memory-card library (the "Realmc" / RealmcIface family
// in BURNOUT_X360_ARTIST.XEX). This header is the canonical OWNING home for the
// two Realmc core primitives that the X360 binary defines as standalone thunks:
//
//     RealmcCore::allocator::allocate    @ 0x82C44BC8
//     RealmcCore::allocator::deallocate  @ 0x82C44BF0
//     RealmcCore::Message::Apply         @ 0x82C44C08
//     RealmcCore::Message::Message       @ 0x82C456D8
//     RealmcCore::Message::`vector deleting destructor' @ 0x82C45718
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below
// is reconstructed purely from the X360 pseudocode + asm. `Realmc` is a vendor
// library boundary, so its identifiers (RealmcCore, allocator, allocate,
// deallocate, Message, Apply) are preserved verbatim per the naming convention.
//
// PLATFORM/VENDOR EXTERNS (flagged):
//   * g_pRealmcAllocator (X360 off_832BE204) -- a global pointer to the Realmc
//     allocator backend object. The object exposes an abstract interface whose
//     vtable holds, at slot +8, an Allocate(size, tag, extra) call and, at slot
//     +12, a Free(...) call. The concrete backend and its vtable are installed
//     by another (platform) Realmc TU; a forward declaration of the interface
//     and the global pointer is all this TU needs to compile and link.
//   * Message base/derived vtables (X360 off_821BA2CC / off_821BA2E8) -- the C++
//     vtables MSVC emits for RealmcCore::Message. They are produced by the
//     compiler from the class definition below, not hand-authored data.
// ===========================================================================

#include <cstddef>
#include <cstdint>

namespace RealmcCore
{

// ---------------------------------------------------------------------------
// IRealmcAllocatorBackend -- the abstract allocator object reached through the
// global g_pRealmcAllocator pointer (X360 off_832BE204).
//
// allocate() tail-calls vtable slot +8 with (size, tag, extra); deallocate()
// tail-calls vtable slot +12. On the X360 these are the 3rd and 4th entries of
// the backend vtable (offsets 8 and 12 == slots 2 and 3 for 4-byte X360
// pointers). Modelled here as virtual methods in that slot order.
// ---------------------------------------------------------------------------
class IRealmcAllocatorBackend
{
public:
    virtual ~IRealmcAllocatorBackend() {}            // vtable slot +0
    virtual void  Reserved1() = 0;                   // vtable slot +4
    virtual void* Allocate(std::size_t nSize,
                           const char* szTag,
                           int nExtra) = 0;           // vtable slot +8
    virtual void  Free(void* pBlock, std::size_t nSize) = 0; // vtable slot +12
};

// The global Realmc allocator backend (X360 off_832BE204). Installed by the
// platform Realmc heap layer (another TU); declared here for compile/link.
extern IRealmcAllocatorBackend* g_pRealmcAllocator;

// ---------------------------------------------------------------------------
// RealmcCore::allocator -- a thin stateless adaptor over g_pRealmcAllocator.
// Both methods forward to the global backend; allocate() stamps the allocation
// with the "RealmcCore::allocator" tag string.
// ---------------------------------------------------------------------------
class allocator
{
public:
    // @ 0x82C44BC8 -- forwards to g_pRealmcAllocator->Allocate(nSize,
    //                 "RealmcCore::allocator", nExtra).
    static void* allocate(std::size_t nSize, int nExtra);

    // @ 0x82C44BF0 -- forwards to g_pRealmcAllocator->Free(...).
    static void deallocate();
};

// ---------------------------------------------------------------------------
// RealmcCore::Message -- a Realmc message base object.
//
// LAYOUT (from asm):
//   +0  vtable pointer (set twice in the ctor: base off_821BA2CC then the final
//                       off_821BA2E8)
//   +4  muLock -- a 32-bit word zeroed under an interrupt-masking lwarx/stwcx.
//                 atomic in the ctor (the classic X360 reservation-init idiom).
//
// Apply() does NOT touch a Message object's own vtable -- it dispatches into a
// *target* object's vtable slot +84 (0x54), passing the Message as the argument.
// ---------------------------------------------------------------------------
class Message
{
public:
    // @ 0x82C456D8 -- construct: install vtable, atomically zero muLock.
    Message();

    // @ 0x82C44C08 -- dispatch this message onto pTarget via its vtable slot
    //                 +0x54 (84): pTarget->vtable[+0x54](pTarget, this).
    //
    // The target is any object exposing an "apply a Realmc message" virtual at
    // slot +0x54. Modelled as the IRealmcMessageTarget interface below.
    static int Apply(Message* pThis, class IRealmcMessageTarget* pTarget);

    virtual ~Message();                              // vtable slot +0

private:
    std::uint32_t muLock;                            // +4
};

// ---------------------------------------------------------------------------
// IRealmcMessageTarget -- the object Apply() dispatches into. Its vtable slot
// +0x54 (84) accepts a Message and applies it. The padding virtuals below pin
// the dispatched method to byte offset 0x54 (slot 21 for 4-byte X360 pointers:
// slot 0 == dtor at +0, so +0x54 == 0x54/4 == slot 21).
// ---------------------------------------------------------------------------
class IRealmcMessageTarget
{
public:
    virtual ~IRealmcMessageTarget() {}               // +0x00
    virtual void Reserved01() = 0;  virtual void Reserved02() = 0;  // +0x04 +0x08
    virtual void Reserved03() = 0;  virtual void Reserved04() = 0;  // +0x0C +0x10
    virtual void Reserved05() = 0;  virtual void Reserved06() = 0;  // +0x14 +0x18
    virtual void Reserved07() = 0;  virtual void Reserved08() = 0;  // +0x1C +0x20
    virtual void Reserved09() = 0;  virtual void Reserved10() = 0;  // +0x24 +0x28
    virtual void Reserved11() = 0;  virtual void Reserved12() = 0;  // +0x2C +0x30
    virtual void Reserved13() = 0;  virtual void Reserved14() = 0;  // +0x34 +0x38
    virtual void Reserved15() = 0;  virtual void Reserved16() = 0;  // +0x3C +0x40
    virtual void Reserved17() = 0;  virtual void Reserved18() = 0;  // +0x44 +0x48
    virtual void Reserved19() = 0;  virtual void Reserved20() = 0;  // +0x4C +0x50
    virtual int  ApplyMessage(Message* pMessage) = 0;               // +0x54
};

} // namespace RealmcCore
