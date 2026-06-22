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
    bool BaseResourcePtr::IsEqual(const void* lpOther) const
    {
        const u32* lpThis = reinterpret_cast<const u32*>(this);
        const u32* lpRhs  = reinterpret_cast<const u32*>(lpOther);
        for (u32 luIndex = 0; luIndex < 3; ++luIndex)
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
}
