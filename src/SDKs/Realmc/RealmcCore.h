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
//
// ---------------------------------------------------------------------------
// WAVE-EXTENSION (additive). The following Realmc-core primitives share this
// home (same vendor library boundary, same off_821BA2CC base vtable installed
// first in every ctor and restored in every deleting destructor):
//
//     RealmcCore::RefCount::Release                       @ 0x82C45108
//     RealmcCore::RefCount::Unreferenced                  @ 0x82C44D18
//     RealmcCore::RefCount::`vector deleting destructor'  @ 0x82C450C0
//     RealmcCore::Response::Response                      @ 0x82C458B0
//     RealmcCore::Response::Apply                         @ 0x82C44D38
//     RealmcCore::Response::`vector deleting destructor'  @ 0x82C458F0
//     RealmcCore::MessageString::MessageString            @ 0x82C46338
//     RealmcCore::MessageString::~MessageString           @ 0x82C46028
//     RealmcCore::MessageString::`scalar deleting destructor' @ 0x82C463C0
//
// off_821BA2CC is the base RefCount vtable (slot +0 = deleting dtor, slot +4 =
// the OnUnreferenced hook Release() fires when the count hits zero); Response
// and MessageString each install their own final vtable (off_821BA2FC /
// off_821BA370) after the base, exactly as MSVC emits for a derived ctor.
// ===========================================================================

#include <cstddef>
#include <cstdint>

namespace RealmcCore
{

// ---------------------------------------------------------------------------
// RefCount -- the shared, atomically-refcounted Realmc base object (X360 base
// vtable off_821BA2CC). It is the base of Response and MessageString below.
//
// LAYOUT (from asm):
//   +0  vtable pointer (off_821BA2CC for a bare RefCount)
//   +4  miRefCount -- the 32-bit reference count, decremented under the X360
//                     interrupt-masked lwarx/stwcx. idiom in Release().
//
// VTABLE (from the asm dispatch sites):
//   slot +0  the (scalar/vector deleting) destructor. The free function
//            Unreferenced(p) calls p->vtable[+0](p, 1) -- "delete this".
//   slot +4  OnUnreferenced() -- Release() fires this when the count hits 0.
//            The bare-RefCount default routes to the deleting destructor.
// ---------------------------------------------------------------------------
class RefCount
{
public:
    RefCount() : miRefCount(0) {}

    // @ 0x82C45108 -- atomically decrement miRefCount; when it reaches 0, fire
    //                 the virtual OnUnreferenced() hook (vtable slot +4). Returns
    //                 the post-decrement count.
    int Release(RefCount* pThis);

    // @ 0x82C44D18 -- if pThis != null, invoke its deleting destructor via
    //                 vtable slot +0 with the delete flag (this, 1). Modelled
    //                 as the static "delete through the vtable" forwarder the
    //                 asm shows (lwz r11,0(r3); li r4,1; lwz r11,0(r11); bctr).
    static RefCount* Unreferenced(RefCount* pThis);

    // Additive accessor (FLAG: not its own X360 function). Every Realmc smart-
    // pointer over a RefCount object bumps the count with the same interrupt-
    // masked lwarx/addi+1/stwcx. idiom inlined at its construction / assignment
    // site (e.g. MessagePtr::MessagePtr @ 0x82C45778 increments *(msg + 4)).
    // Exposed here by NAME so those sites raise the count through RefCount rather
    // than reaching the +4 field via a raw offset. Atomic increment, no layout
    // change.
    void AddRef();

    // slot +0 -- backs the X360 `vector deleting destructor' @ 0x82C450C0
    //            (restore base vtable, then operator delete if flag bit0 set).
    virtual ~RefCount();

    // slot +4 -- the "count reached zero" hook fired by Release().
    virtual void OnUnreferenced() = 0;

protected:
    std::int32_t miRefCount;  // +4
};

// ---------------------------------------------------------------------------
// RealmcString -- the small Realmc string the X360 keeps inside MessageString.
//
// LAYOUT (from MessageString's ctor/dtor + the assign/reserve helpers):
//   +0  mpBegin  -- start of the character buffer (or the shared empty
//                   singleton when the string is empty)
//   +4  mpEnd    -- one past the last character (points at the NUL terminator)
//   +8  mpCapEnd -- one past the end of the allocated buffer
//
// The buffer is heap-owned only when (mpCapEnd - mpBegin) > 1 and mpBegin is
// non-null; otherwise mpBegin aliases a shared 1-byte empty singleton and must
// NOT be freed -- exactly the guard the X360 destructor evaluates.
// ---------------------------------------------------------------------------
class RealmcString
{
public:
    RealmcString() : mpBegin(nullptr), mpEnd(nullptr), mpCapEnd(nullptr) {}

    // Assign from a [pBegin, pEnd) character range (X360 sub_82B562A0): reserve
    // (pEnd - pBegin + 1) bytes, copy the range, NUL-terminate. Declared here;
    // the reserve/copy machinery lives in the shared Realmc string TU.
    void Assign(const char* pBegin, const char* pEnd);

    // Release the heap buffer if it is owned (the MessageString-dtor guard).
    void Free();

private:
    char* mpBegin;   // +0
    char* mpEnd;     // +4
    char* mpCapEnd;  // +8
};

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

// ---------------------------------------------------------------------------
// Response -- a Realmc response object (derives RefCount; final vtable
// off_821BA2FC). It carries one target/payload pointer and dispatches itself
// onto a target object via that target's vtable slot +0x50.
//
// LAYOUT (from the ctor asm):
//   +0  vtable pointer (base off_821BA2CC then final off_821BA2FC)
//   +4  miRefCount (inherited from RefCount, atomically zeroed in the ctor)
//   +8  mpPayload  -- the pointer/value passed to the ctor (stw r4, 8(r3))
//
// sizeof == 0xC (12) -- the deleting destructor frees 12 bytes.
// ---------------------------------------------------------------------------
class Response : public RefCount
{
public:
    // @ 0x82C458B0 -- install the final vtable, atomically zero the refcount,
    //                 store the payload pointer at +8.
    explicit Response(void* pPayload);

    // @ 0x82C44D38 -- dispatch this response onto pTarget via pTarget's vtable
    //                 slot +0x50 (80): pTarget->vtable[+0x50](pTarget, this).
    //                 (IDA lists (a1=this, a2=target); the asm swaps r3/r4 so
    //                 the target is `this` for the dispatch.)
    static int Apply(Response* pThis, class IRealmcResponseTarget* pTarget);

    void OnUnreferenced() override {}  // slot +4 (final vtable)

    // slot +0 -- backs the X360 `vector deleting destructor' @ 0x82C458F0.
    ~Response() override;

private:
    void* mpPayload;  // +8
};

// ---------------------------------------------------------------------------
// IRealmcResponseTarget -- the object Response::Apply() dispatches into. Its
// vtable slot +0x50 (80) accepts a Response and applies it. The padding
// virtuals pin the dispatched method to byte offset 0x50 (slot 20 for 4-byte
// X360 pointers: slot 0 == dtor at +0, so +0x50 == 0x50/4 == slot 20).
// ---------------------------------------------------------------------------
class IRealmcResponseTarget
{
public:
    virtual ~IRealmcResponseTarget() {}              // +0x00
    virtual void Reserved01() = 0;  virtual void Reserved02() = 0;  // +0x04 +0x08
    virtual void Reserved03() = 0;  virtual void Reserved04() = 0;  // +0x0C +0x10
    virtual void Reserved05() = 0;  virtual void Reserved06() = 0;  // +0x14 +0x18
    virtual void Reserved07() = 0;  virtual void Reserved08() = 0;  // +0x1C +0x20
    virtual void Reserved09() = 0;  virtual void Reserved10() = 0;  // +0x24 +0x28
    virtual void Reserved11() = 0;  virtual void Reserved12() = 0;  // +0x2C +0x30
    virtual void Reserved13() = 0;  virtual void Reserved14() = 0;  // +0x34 +0x38
    virtual void Reserved15() = 0;  virtual void Reserved16() = 0;  // +0x3C +0x40
    virtual void Reserved17() = 0;  virtual void Reserved18() = 0;  // +0x44 +0x48
    virtual void Reserved19() = 0;                                  // +0x4C
    virtual int  ApplyResponse(Response* pResponse) = 0;            // +0x50
};

// ---------------------------------------------------------------------------
// MessageString -- a Realmc message that carries one id word and one owned
// string (derives RefCount; final vtable off_821BA370).
//
// LAYOUT (from the ctor/dtor asm):
//   +0    vtable pointer (base off_821BA2CC then final off_821BA370)
//   +4    miRefCount (inherited from RefCount, atomically zeroed in the ctor)
//   +8    muId      -- the id word passed to the ctor (stw r4, 8(r3))
//   +0xC  maText    -- a RealmcString (begin/end/capEnd), assigned in the ctor
//                      from the source range and freed (if owned) in the dtor.
//
// sizeof == 0x1C (28) -- the scalar deleting destructor frees 28 bytes.
// ---------------------------------------------------------------------------
class MessageString : public RefCount
{
public:
    // @ 0x82C46338 -- install the final vtable, atomically zero the refcount,
    //                 store muId at +8, assign maText from the source's
    //                 [begin,end) character range (X360 sub_82B562A0).
    MessageString(std::uint32_t uId, const char* const* ppSourceRange);

    void OnUnreferenced() override {}  // slot +4 (final vtable)

    // @ 0x82C46028 -- restore the base vtable, free the owned string buffer,
    //                 restore the RefCount base vtable. Backs the X360 `scalar
    //                 deleting destructor' @ 0x82C463C0 (which frees 28 bytes).
    ~MessageString() override;

private:
    std::uint32_t muId;    // +8
    RealmcString  maText;  // +0xC
};

// ---------------------------------------------------------------------------
// WAVE-EXTENSION (additive). RealmcCore::MessagePtr -- a small intrusive smart
// pointer / refcount wrapper over a RefCount-derived "message" object (the X360
// MessagePtr family). Reconstructed from BURNOUT_X360_ARTIST.XEX (no leak /
// DWARF). Per-function X360 addresses:
//
//     RealmcCore::MessagePtr::MessagePtr   @ 0x82C45778  (ctor; AddRef the msg)
//     RealmcCore::MessagePtr::~MessagePtr  @ 0x82C457B0  (Release the msg, null it)
//     RealmcCore::MessagePtr::operator=    @ 0x82C45838  (rebind: Release old, AddRef new)
//     RealmcCore::MessagePtr::Apply        @ 0x82C44C38  (dispatch into msg vtable +8)
//     RealmcCore::MessagePtr::EMPTY_MESSAGE@ 0x82C44C28  (shared empty-message singleton)
//     RealmcCore::MessagePtr::`scalar deleting destructor' @ 0x82C45FC0
//
// LAYOUT (from the ctor / dtor / operator= asm):
//   +0x00  vtable pointer  (off_821BA2F4 -- MessagePtr's own vtable; the virtual
//                           dtor below models the install. MessagePtr's only
//                           virtual is the destructor.)
//   +0x04  mpMessage       (the held IRealmcMessage*; AddRef'd on bind, Release'd
//                           on rebind/teardown)
//
// sizeof(MessagePtr) == 8 (vtable ptr + mpMessage) -- the scalar deleting
// destructor frees 8 bytes.
// ---------------------------------------------------------------------------

// IRealmcMessage -- the object a MessagePtr holds. It IS a RefCount (MessagePtr
// AddRef's / Release's it through the RefCount count at +4), and it exposes one
// extra virtual at vtable slot +8 (byte offset 0x08) that Apply() dispatches
// into. RefCount pins slots +0 (deleting dtor) and +4 (OnUnreferenced); Process
// is the next slot (+8), exactly the entry Apply()'s `lwz r11, 8(vtable)` reads.
class IRealmcMessage : public RefCount
{
public:
    // vtable slot +0x08 -- "apply / process this message". Apply() tail-calls it
    // with the held message as the implicit `this` (the X360 reads vtable+8 of
    // mpMessage and branches to it with r3 = mpMessage).
    virtual int Process() = 0;
};

class MessagePtr
{
public:
    // @ 0x82C45778 -- install MessagePtr's vtable, AddRef the message (the X360
    //                 inlines the interrupt-masked lwarx/addi+1/stwcx. increment
    //                 of *(msg + 4) == the RefCount count), store mpMessage.
    explicit MessagePtr(IRealmcMessage* pMessage);

    // @ 0x82C45838 -- rebind to rhs.mpMessage: when it differs from the current
    //                 message, Release the old one, store the new one, then
    //                 AddRef it (the same inlined atomic increment). Returns
    //                 this. (No-op when both already point at the same message.)
    MessagePtr& operator=(const MessagePtr& rOther);

    // @ 0x82C44C38 -- dispatch into the held message's vtable slot +8:
    //                 return mpMessage->Process(). (Static thunk: the X360 takes
    //                 the MessagePtr in r3, loads mpMessage, then mpMessage's
    //                 vtable+8.)
    static int Apply(MessagePtr* pThis);

    // @ 0x82C44C28 -- return the shared empty-message singleton (X360
    //                 off_832BE1F0), the placeholder a MessagePtr points at when
    //                 it carries no real message.
    static IRealmcMessage* EMPTY_MESSAGE();

    // slot +0 -- backs the X360 `scalar deleting destructor' @ 0x82C45FC0
    //            (restore vtable, Release mpMessage, null it; then operator delete
    //            8 bytes when the delete flag bit0 is set).
    virtual ~MessagePtr();

private:
    IRealmcMessage* mpMessage;  // +0x04 (held, AddRef'd message)
};

// The shared empty-message singleton (X360 off_832BE1F0). Installed by another
// Realmc TU at boot; declared here for compile/link. EMPTY_MESSAGE() returns it.
extern IRealmcMessage* g_pRealmcEmptyMessage;

} // namespace RealmcCore
