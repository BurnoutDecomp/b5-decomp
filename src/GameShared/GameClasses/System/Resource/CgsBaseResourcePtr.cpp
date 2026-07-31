#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsResource::BaseResourcePtr -- the non-templated base of every CgsResource::ResourcePtr<T>.
//
// Four functions in this TU are reconstructed STORE-FOR-STORE from the X360 ARTIST binary
// (the build emitted them out-of-line); the rest are trivial accessors inlined in the X360 and
// reconstructed from the member contract in CgsResourcePtr.h:
//
//   BaseResourcePtr::BaseResourcePtr   @ 0x82204E20  (ctor -- self-circular list init)
//   BaseResourcePtr::~BaseResourcePtr  @ 0x821F1E18  (dtor -- inline unlink + re-self-link)
//   BaseResourcePtr::IsEqual           @ 0x8227D298  (3-dword identity compare)
//   BaseResourcePtr::AddToNewList      @ 0x828D6230  (insert-before into an alias ring)
//
// LAYOUT (X360-authoritative -- see the FLAG in CgsResourcePtr.h):
//   +0x00 mpResourceMemory   +0x04/+0x08 mHandle (2 ptrs)
//   +0x0C mpNext             +0x10 mpPrev        +0x14 mpThis   +0x18 muThreadId
// A "detached" BaseResourcePtr is its own one-element ring: mpNext == mpPrev == this.

namespace CgsResource
{
    // @0x82204E20  ctor. asm: stw 0 ->+0,+4,+8 (then a redundant 0 ->+0), stw this ->+0xC,+0x10,+0x14,
    // stw 0 ->+0x18. So mpResourceMemory + the two mHandle pointers are zeroed, the list is self-circular
    // (mpNext = mpPrev = this), mpThis = this, muThreadId = 0.
    BaseResourcePtr::BaseResourcePtr()
        : mpResourceMemory(0)
        , mpNext(this)
        , mpPrev(this)
        , mpThis(this)
        , muThreadId(0)
    {
        mHandle.mpResourceMemory = 0;
        mHandle.mpSourceEntry = 0;
    }

    // @0x821F1E18  dtor. asm inlines the unlink (it does NOT call RemoveFromCurrentList):
    //   v1 = mpNext;  if (v1) v1->mpPrev = mpPrev;
    //   v2 = mpPrev;  if (v2) v2->mpNext = mpNext;
    //   mpNext = this; mpPrev = this;
    // i.e. splice this out of its ring, then re-make this a one-element ring. The null guards mirror the
    // asm's beq-on-zero even though a live ring never holds null neighbours.
    BaseResourcePtr::~BaseResourcePtr()
    {
        BaseResourcePtr* lpNext = mpNext;   // +0xC
        if (lpNext != 0)
            lpNext->mpPrev = mpPrev;        // [next+0x10] = prev
        BaseResourcePtr* lpPrev = mpPrev;   // +0x10
        if (lpPrev != 0)
            lpPrev->mpNext = mpNext;        // [prev+0xC]  = next
        mpNext = this;                      // +0xC  = this
        mpPrev = this;                      // +0x10 = this
    }

    void BaseResourcePtr::GetResource(void** lppResource) const
    {
        *lppResource = mpResourceMemory;   // X360: returns *a1 (offset 0) directly
    }

    void BaseResourcePtr::GetResourceHandle(ResourceHandle* lpHandle) const
    {
        *lpHandle = mHandle;
    }

    ResourceHandle BaseResourcePtr::GetResourceHandle() const
    {
        return mHandle;
    }

    void BaseResourcePtr::SetResource(void* const* lppResource)
    {
        // Point this ptr at the new main-memory resource. [follow-on: the X360 also propagates the new
        // pointer to every alias sharing mpNext/mpPrev via Propogate() -- that path is its own TU.]
        mpResourceMemory = (lppResource != 0) ? *lppResource : 0;
    }

    // @0x8227D298  IsEqual. asm walks both objects from offset 0 comparing dwords; returns 1 iff the
    // first three dwords are pairwise equal (the loop bound is v3 >= 3), else 0. The compared region is
    // mResourceMemory (+0) followed by the two mHandle pointers (+4,+8) -- a BaseResourcePtr's identity.
    // ⚠️ x64 WIDTH FIX (race-car streamer wave 2026-07-31). The console's "3 dwords" are
    // three POINTERS -- mpResourceMemory, mHandle.mpResourceMemory, mHandle.mpSourceEntry --
    // which are 4 bytes each only because the Xenon is 32-bit. Walking literally 3 x u32 on
    // the x64 gate compares 12 of the 24 identity bytes (mpResourceMemory plus the LOW HALF
    // of mHandle.mpResourceMemory), so two pointers differing only in their high halves
    // compared EQUAL. Size the walk from the identity region itself, per the project's
    // "never transcribe a console byte-size literal" rule; on X360 this is bit-identical
    // to the original loop.
    bool BaseResourcePtr::IsEqual(const void* lpOther) const
    {
        const u32 KU_IDENTITY_WORDS = static_cast<u32>(
            (sizeof(void*) + sizeof(ResourceHandle)) / sizeof(void*));   // 3 on both targets

        void* const* lpThis = reinterpret_cast<void* const*>(this);
        void* const* lpRhs  = reinterpret_cast<void* const*>(lpOther);
        for (u32 luIndex = 0; luIndex < KU_IDENTITY_WORDS; ++luIndex)
        {
            if (lpThis[luIndex] != lpRhs[luIndex])
                return false;
        }
        return true;
    }

    void BaseResourcePtr::Reset()
    {
        mpResourceMemory = 0;
        mHandle.mpResourceMemory = 0;
        mHandle.mpSourceEntry = 0;
    }

    void BaseResourcePtr::RemoveFromCurrentList()
    {
        // Splice this ptr out of its alias ring (null-safe). The X360 dtor inlines an equivalent unlink;
        // this named entry point exists for the explicit detach path.
        if (mpPrev != 0) mpPrev->mpNext = mpNext;
        if (mpNext != 0) mpNext->mpPrev = mpPrev;
        mpNext = this;
        mpPrev = this;
    }

    // @0x828D6230  AddToNewList. asm:
    //   if (mpNext != this || mpPrev != this) { ASSERT-fire "Must remove from current list before
    //                                           adding to new list" (CgsResourcePtr.h:384) }
    //   mpNext        = lpList;                 // +0xC  = list
    //   mpPrev        = lpList->mpPrev;         // +0x10 = *(list+0x10)
    //   lpList->mpPrev= this;                   // *(list+0x10) = this
    //   mpPrev->mpNext= this;                   // *(this->mpPrev + 0xC) = this
    // i.e. insert THIS immediately before lpList in lpList's ring. The precondition is that THIS is
    // currently a detached one-element ring (mpNext == mpPrev == this).
    void BaseResourcePtr::AddToNewList(BaseResourcePtr* lpList)
    {
        CGS_ASSERT(mpNext == this && mpPrev == this,
            "ERROR: Must remove from current list before adding to new list\n");

        mpNext = lpList;                 // +0xC
        mpPrev = lpList->mpPrev;         // +0x10
        lpList->mpPrev = this;           // *(list+0x10) = this
        mpPrev->mpNext = this;           // *(this->mpPrev + 0xC) = this
    }

    // ------------------------------------------------------------------------
    // The shared "null resource pointer" sentinel -- X360 &dword_82FAD94C.
    //
    // ATTESTED USE (race-car streamer wave 2026-07-31): every `slot != NULLResourcePtr`
    // in game source compiles to `BaseResourcePtr::IsEqual(&dword_82FAD94C, slot)` --
    // RaceCarStreamer::AddVehicleData @0x822EBE18 does it three times, and the three
    // RaceCarStreamer::Get*Resource asserts do it once each. IsEqual reads only the
    // IDENTITY region (mpResourceMemory + the two mHandle pointers), so the only
    // load-bearing property of this object is that all three are NULL.
    //
    // ⚠️ DIVERGENCE, deliberate and behaviourally inert: the console's is a static
    // all-zero blob, so ITS mpThis/mpNext/mpPrev are 0 too. BaseResourcePtr has a
    // user-provided default ctor that self-links the alias ring (mpNext = mpPrev =
    // mpThis = this), and there is no legal way to produce a zero-byte const object of
    // this type without bypassing that ctor. Nothing reads those three fields of the
    // SENTINEL: IsEqual ignores them, and the assign-from-sentinel sites are spelled
    // against CgsResource::NULLResourceHandle (the console's own dword_82FAD960 ==
    // &NULLResourcePtr + 0x14, i.e. exactly the {mpThis, muThreadId} pair -- which for
    // the console blob IS {0, 0}). Its dtor unlinks a one-element ring: a no-op.
    const BaseResourcePtr NULLResourcePtr;
}
