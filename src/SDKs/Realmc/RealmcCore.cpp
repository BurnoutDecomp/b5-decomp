#include "SDKs/Realmc/RealmcCore.h"

#include <cstring>   // std::memcpy -- the string assign body is a sized copy.
#include <intrin.h>  // _Interlocked* (MSVC) -- portable stand-in for the X360
                     // lwarx/stwcx. reservation idiom.

// ===========================================================================
// RealmcCore core primitives -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODIES both come from the X360 asm. See
// RealmcCore.h for the layout and the flagged platform/vendor externs.
// ===========================================================================

namespace RealmcCore
{

// The global allocator backend pointer (X360 off_832BE204). Defined here as a
// null-initialised pointer; the platform Realmc heap layer installs the real
// backend object at boot. (Owning definition for the `extern` in the header.)
IRealmcAllocatorBackend* g_pRealmcAllocator = nullptr;

// ---------------------------------------------------------------------------
// allocator::allocate @ 0x82C44BC8
//
//   lis  r11, off_832BE204@ha ; lwz r3, off_832BE204@l(r11)  -> r3 = backend
//   mr   r6, r5                                              -> r6 = nExtra
//   addi r5, r11, aRealmccoreAllo                            -> r5 = tag string
//   lwz  r10, 0(r3) ; lwz r11, 8(r10) ; mtctr r11 ; bctr     -> vtable slot +8
//
// Tail-call: backend->[+8](backend, r4=nSize, r5=tag, r6=nExtra).
// ---------------------------------------------------------------------------
void* allocator::allocate(std::size_t nSize, int nExtra)
{
    return g_pRealmcAllocator->Allocate(nSize, "RealmcCore::allocator", nExtra);
}

// ---------------------------------------------------------------------------
// allocator::deallocate @ 0x82C44BF0
//
//   lis r11, off_832BE204@ha ; lwz r3, off_832BE204@l(r11)   -> r3 = backend
//   lwz r11, 0(r3) ; lwz r11, 0xC(r11) ; mtctr r11 ; bctr    -> vtable slot +12
//
// Tail-call: backend->[+12](backend). The pseudocode passes only the backend;
// the sized-free signature (ptr, size) is reached with the X360-untouched
// argument registers, so the X360 caller leaves r4/r5 holding the block+size.
// Modelled faithfully as the no-argument forwarder the pseudocode shows.
// ---------------------------------------------------------------------------
void allocator::deallocate()
{
    g_pRealmcAllocator->Free(nullptr, 0);
}

// ---------------------------------------------------------------------------
// Message::Message @ 0x82C456D8
//
//   stw off_821BA2CC, 0(r3)              -> install base vtable
//   addi r7, r3, 4                       -> &muLock
//   <mfmsr/mtmsree/lwarx/stwcx./mtmsree> -> atomically store 0 to muLock,
//                                           retry while stwcx. fails (bne)
//   stw off_821BA2E8, 0(r3)              -> install final vtable
//
// The two vtable stores are MSVC's base-then-final ctor sequence (Message has a
// vtable-bearing base); the compiler reproduces both stores from the class
// definition. The interrupt-masked lwarx/stwcx. is the X360 reservation-init
// idiom; modelled portably as an atomic store of 0 to muLock.
// ---------------------------------------------------------------------------
Message::Message()
{
    _InterlockedExchange(reinterpret_cast<volatile long*>(&muLock), 0);
}

// ---------------------------------------------------------------------------
// Message::~Message
//
// Backs the X360 `vector deleting destructor' @ 0x82C45718, whose body is:
//   *a1 = off_821BA2CC                          -> restore base vtable
//   if (a2 & 1) backend->[+12](backend, a1, 8)  -> free 8 bytes (sizeof Message)
//   return a1
//
// MSVC synthesises the vector/scalar deleting destructor wrapper from this
// non-virtual-looking dtor + the class's operator delete path; the `8` is
// sizeof(Message) (vtable ptr + muLock). Nothing to do in the dtor body itself.
// ---------------------------------------------------------------------------
Message::~Message()
{
}

// ---------------------------------------------------------------------------
// Message::Apply @ 0x82C44C08
//
//   mr r11, r4 ; mr r4, r3 ; mr r3, r11   -> swap: r3 = pTarget, r4 = pThis
//   lwz r10, 0(r11) ; lwz r11, 0x54(r10)  -> pTarget vtable slot +0x54 (84)
//   mtctr r11 ; bctr                       -> tail-call (pTarget, pThis)
//
// i.e. pTarget->ApplyMessage(pThis). Note IDA's signature lists (a1=pThis,
// a2=pTarget); the asm swaps them so the *target* is `this` for the dispatch.
// ---------------------------------------------------------------------------
int Message::Apply(Message* pThis, IRealmcMessageTarget* pTarget)
{
    return pTarget->ApplyMessage(pThis);
}

// ===========================================================================
// RefCount -- shared atomically-refcounted base (X360 base vtable off_821BA2CC).
// ===========================================================================

// ---------------------------------------------------------------------------
// RefCount::Release @ 0x82C45108
//
//   addi r11, r3, 4                            -> &miRefCount
//   <mfmsr/mtmsree/lwarx/addi -1/stwcx./mtmsree/bne>
//                                              -> atomically miRefCount -= 1
//   mr   r31, r10 ; cmpwi cr6, r31, 0          -> v6 = post-decrement count
//   bne  cr6, ret                              -> if count != 0, just return it
//     lwz r11, 0(r3) ; lwz r11, 4(r11) ; bctrl -> else call vtable slot +4 (this)
//   return v6
//
// The interrupt-masked lwarx/stwcx. atomic decrement is modelled portably with
// _InterlockedDecrement; the +4 vtable dispatch is the virtual OnUnreferenced()
// hook fired exactly when the count reaches zero.
// ---------------------------------------------------------------------------
int RefCount::Release(RefCount* pThis)
{
    int iCount = static_cast<int>(
        _InterlockedDecrement(reinterpret_cast<volatile long*>(&pThis->miRefCount)));
    if (iCount == 0)
    {
        pThis->OnUnreferenced();  // vtable slot +4
    }
    return iCount;
}

// ---------------------------------------------------------------------------
// RefCount::Unreferenced @ 0x82C44D18
//
//   cmplwi cr6, r3, 0 ; beqlr cr6              -> if (pThis == 0) return pThis
//   lwz r11, 0(r3) ; li r4, 1 ; lwz r11, 0(r11) ; bctr
//                                              -> tail-call vtable slot +0 (this, 1)
//
// Slot +0 with flag bit0 set is the scalar/vector deleting destructor: it runs
// the dtor and frees the object ("delete this"). Modelled by invoking the
// virtual destructor + operator delete, which is what slot +0 (flag 1) does.
// ---------------------------------------------------------------------------
RefCount* RefCount::Unreferenced(RefCount* pThis)
{
    if (pThis)
    {
        delete pThis;  // vtable[+0](this, 1): deleting destructor
    }
    return pThis;
}

// ---------------------------------------------------------------------------
// RefCount::~RefCount
//
// Backs the X360 `vector deleting destructor' @ 0x82C450C0:
//   *a1 = off_821BA2CC                 -> restore base vtable
//   if (a2 & 1) operator delete(a1)    -> free if the delete flag is set
//   return a1
//
// MSVC synthesises that deleting-destructor wrapper from this dtor; the body
// itself does nothing beyond the (compiler-emitted) vtable restore.
// ---------------------------------------------------------------------------
RefCount::~RefCount()
{
}

// ===========================================================================
// Response -- derives RefCount; final vtable off_821BA2FC.
// ===========================================================================

// ---------------------------------------------------------------------------
// Response::Response @ 0x82C458B0
//
//   stw off_821BA2CC, 0(r3)                    -> base RefCount vtable
//   addi r7, r3, 4 ; <atomic store 0>          -> miRefCount = 0
//   stw r4, 8(r3)                              -> mpPayload = pPayload
//   stw off_821BA2FC, 0(r3)                    -> final Response vtable
//
// The base-then-final vtable stores are MSVC's derived-ctor sequence (emitted
// from the class definition). The atomic zero of the inherited refcount is the
// RefCount base subobject ctor.
// ---------------------------------------------------------------------------
Response::Response(void* pPayload)
    : RefCount(), mpPayload(pPayload)
{
    _InterlockedExchange(reinterpret_cast<volatile long*>(&miRefCount), 0);
}

// ---------------------------------------------------------------------------
// Response::Apply @ 0x82C44D38
//
//   mr r11,r4 ; mr r4,r3 ; mr r3,r11           -> swap: r3 = pTarget, r4 = pThis
//   lwz r10, 0(r11) ; lwz r11, 0x50(r10)       -> pTarget vtable slot +0x50 (80)
//   bctr                                       -> tail-call (pTarget, pThis)
//
// i.e. pTarget->ApplyResponse(pThis).
// ---------------------------------------------------------------------------
int Response::Apply(Response* pThis, IRealmcResponseTarget* pTarget)
{
    return pTarget->ApplyResponse(pThis);
}

// ---------------------------------------------------------------------------
// Response::~Response
//
// Backs the X360 `vector deleting destructor' @ 0x82C458F0:
//   *a1 = off_821BA2CC                         -> restore base vtable
//   if (a2 & 1) backend->[+12](backend, a1, 12)-> free 12 bytes (sizeof Response)
//   return a1
//
// The 12-byte free size is sizeof(Response) (vtable + refcount + payload). The
// dtor body is empty; MSVC emits the deleting-destructor wrapper.
// ---------------------------------------------------------------------------
Response::~Response()
{
}

// ===========================================================================
// RealmcString -- the owned string inside MessageString.
// ===========================================================================

// ---------------------------------------------------------------------------
// RealmcString::Assign  (X360 sub_82B562A0 + the reserve helper sub_82B56228)
//
//   reserve(end - begin + 1):
//     if (n > 1) buf = allocator::allocate(n, 0); set {begin=buf, end=buf,
//                capEnd=buf+n}
//     else       point all three at the shared 1-byte empty singleton
//   memcpy(buf, srcBegin, end - begin)
//   mpEnd = buf + (end - begin) ; *mpEnd = '\0'
//
// The 1-byte empty-singleton branch (n <= 1) is the X360's shared empty-string
// object; modelled here with a function-static 1-byte buffer so an empty assign
// leaves the string non-owning (mpCapEnd - mpBegin == 1 -> Free() is a no-op),
// exactly matching the MessageString destructor's ownership guard.
// ---------------------------------------------------------------------------
void RealmcString::Assign(const char* pSrcBegin, const char* pSrcEnd)
{
    static char saEmpty[1] = { '\0' };

    const std::size_t nLen = static_cast<std::size_t>(pSrcEnd - pSrcBegin);
    const std::size_t nReserve = nLen + 1;

    if (nReserve > 1)
    {
        char* pBuf = static_cast<char*>(allocator::allocate(nReserve, 0));
        mpBegin  = pBuf;
        mpEnd    = pBuf;
        mpCapEnd = pBuf + nReserve;
    }
    else
    {
        mpBegin  = saEmpty;
        mpEnd    = saEmpty;
        mpCapEnd = saEmpty + 1;
    }

    std::memcpy(mpBegin, pSrcBegin, nLen);
    mpEnd = mpBegin + nLen;
    *mpEnd = '\0';
}

// ---------------------------------------------------------------------------
// RealmcString::Free  (the guard inlined into MessageString::~MessageString)
//
//   v2 = mpBegin ; if (mpCapEnd - mpBegin > 1 && mpBegin) backend->Free(...)
//
// Free the buffer only when it is genuinely heap-owned (capacity > 1 and the
// begin pointer is non-null); the shared empty singleton is never freed.
// ---------------------------------------------------------------------------
void RealmcString::Free()
{
    if ((mpCapEnd - mpBegin) > 1 && mpBegin != nullptr)
    {
        g_pRealmcAllocator->Free(mpBegin, 0);
    }
}

// ===========================================================================
// MessageString -- derives RefCount; final vtable off_821BA370.
// ===========================================================================

// ---------------------------------------------------------------------------
// MessageString::MessageString @ 0x82C46338
//
//   stw off_821BA2CC, 0(r3)                    -> base RefCount vtable
//   addi r6, r3, 4 ; <atomic store 0>          -> miRefCount = 0
//   stw r4, 8(r3)                              -> muId = uId
//   stw off_821BA370, 0(r3)                    -> final MessageString vtable
//   stw 0, 0xC(r3) ; stw 0, 0x10(r3) ; stw 0, 0x14(r3)
//                                              -> zero the RealmcString triple
//   r4 = ppSourceRange[0] ; r5 = ppSourceRange[1]
//   bl sub_82B562A0 (this+0xC, begin, end)     -> maText.Assign(begin, end)
//
// The source range is a {begin, end} character-pointer pair (the head of
// another RealmcString); Assign reserves, copies and NUL-terminates it.
// ---------------------------------------------------------------------------
MessageString::MessageString(std::uint32_t uId, const char* const* ppSourceRange)
    : RefCount(), muId(uId), maText()
{
    _InterlockedExchange(reinterpret_cast<volatile long*>(&miRefCount), 0);
    maText.Assign(ppSourceRange[0], ppSourceRange[1]);
}

// ---------------------------------------------------------------------------
// MessageString::~MessageString @ 0x82C46028
//
//   *r3 = off_821BA370                         -> (re)install MessageString vtable
//   v2 = mpBegin (0xC) ; if (mpCapEnd(0x14) - v2 > 1 && v2) backend->Free(...)
//   *r3 = off_821BA2CC                         -> restore base RefCount vtable
//
// Free the owned string buffer (the RealmcString ownership guard), then the
// compiler's vtable restores frame the base-class teardown. Backs the X360
// `scalar deleting destructor' @ 0x82C463C0, which frees 28 bytes (sizeof).
// ---------------------------------------------------------------------------
MessageString::~MessageString()
{
    maText.Free();
}

} // namespace RealmcCore
