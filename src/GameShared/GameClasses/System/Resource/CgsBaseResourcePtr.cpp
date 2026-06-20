#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"

// CgsResource::BaseResourcePtr -- the non-templated base of every CgsResource::ResourcePtr<T>. The X360
// ARTIST build inlined these trivial accessors into their callers (no standalone symbol to decompile), so
// they are reconstructed from the member contract documented in CgsResourcePtr.h: mpResourceMemory (+0) is
// the main-memory resource pointer that operator->/GetMemoryResource read; ResourcePtrs that share one
// resource are linked into an intrusive doubly-linked alias list (mpNext/mpPrev) so a SetResource can be
// propagated to every alias.
//
// [SCOPED] Only the lifecycle the in-scope callers exercise is bodied here (ctor/dtor/GetResource/
// SetResource/GetResourceHandle/IsEqual/Reset/RemoveFromCurrentList). The alias-list *insertion* path
// (AddToNewList / CreateFromPointer / CreateFromHandle / Propogate) is part of the broader resource system
// and stays declaration-only until reconstructed from the X360 -- reconstructing it without a reference
// would be invention, and no current caller needs it. SetResource therefore sets this pointer directly;
// alias propagation across a shared list is the marked follow-on.

namespace CgsResource
{
    BaseResourcePtr::BaseResourcePtr()
        : mpResourceMemory(0)
        , mpNext(0)
        , mpPrev(0)
        , mpThis(this)
        , muThreadId(0)
    {
        mHandle.mpResourceMemory = 0;
        mHandle.mpSourceEntry = 0;
    }

    BaseResourcePtr::~BaseResourcePtr()
    {
        RemoveFromCurrentList();
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
        // pointer to every alias sharing mpNext/mpPrev -- needs the alias-list path reconstructed.]
        mpResourceMemory = (lppResource != 0) ? *lppResource : 0;
    }

    bool BaseResourcePtr::IsEqual(void* const* lppResource) const
    {
        return mpResourceMemory == ((lppResource != 0) ? *lppResource : 0);
    }

    void BaseResourcePtr::Reset()
    {
        mpResourceMemory = 0;
        mHandle.mpResourceMemory = 0;
        mHandle.mpSourceEntry = 0;
    }

    void BaseResourcePtr::RemoveFromCurrentList()
    {
        // Unlink from the intrusive alias list (null-safe: a standalone ptr has no neighbours).
        if (mpPrev != 0) mpPrev->mpNext = mpNext;
        if (mpNext != 0) mpNext->mpPrev = mpPrev;
        mpNext = 0;
        mpPrev = 0;
    }
}
