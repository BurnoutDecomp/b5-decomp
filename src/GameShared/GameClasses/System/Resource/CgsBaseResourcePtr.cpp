#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"

namespace CgsResource
{
    // ARTIST82204E20. The last member at1C is not initialized.
    BaseResourcePtr::BaseResourcePtr()
    {
        mResourceMemory.m_baseResources[0] = 0;
        mResourceMemory.m_baseResources[1] = 0;
        mResourceMemory.m_baseResources[2] = 0;
        mpNext = this;
        mpPrev = this;
        mHandle.mpResourceMemory = &mResourceMemory;
        mHandle.mpSourceEntry = 0;
    }

    // ARTIST821F1E18.
    BaseResourcePtr::~BaseResourcePtr()
    {
        BaseResourcePtr* lpNext = mpNext;
        if (lpNext != 0) lpNext->mpPrev = mpPrev;
        BaseResourcePtr* lpPrev = mpPrev;
        if (lpPrev != 0) lpPrev->mpNext = mpNext;
        mpNext = this;
        mpPrev = this;
    }

    void BaseResourcePtr::GetResource(SmallResource* lpResource) const
    {
        *lpResource = mResourceMemory;
    }
    void BaseResourcePtr::GetResourceHandle(ResourceHandle* lpHandle) const
    {
        *lpHandle = mHandle;
    }
    ResourceHandle BaseResourcePtr::GetResourceHandle() const
    {
        return mHandle;
    }

    // Three-word resource assignment followed by propagation is inlined in
    // ARTIST Pool::ReAllocateMemoryForEntry828F6880 at Entry+28.
    void BaseResourcePtr::SetResource(const SmallResource* lpResource)
    {
        mResourceMemory = *lpResource;
        Propogate();
    }

    // ARTIST8227D298 compares only the three cached resource words.
    bool BaseResourcePtr::IsEqual(const void* lpOther) const
    {
        void* const* lpResources = static_cast<void* const*>(lpOther);
        for (u32 luIndex = 0; luIndex < 3; ++luIndex)
        {
            if (mResourceMemory.m_baseResources[luIndex] != lpResources[luIndex])
                return false;
        }
        return true;
    }

    void BaseResourcePtr::Reset()
    {
        mResourceMemory.m_baseResources[0] = 0;
        mResourceMemory.m_baseResources[1] = 0;
        mResourceMemory.m_baseResources[2] = 0;
    }
    void BaseResourcePtr::RemoveFromCurrentList()
    {
        if (mpPrev != 0) mpPrev->mpNext = mpNext;
        if (mpNext != 0) mpNext->mpPrev = mpPrev;
        mpNext = this;
        mpPrev = this;
    }

    // ARTIST828D6230 inserts this detached node immediately before lpList.
    void BaseResourcePtr::AddToNewList(BaseResourcePtr* lpList)
    {
        CGS_ASSERT(mpNext == this && mpPrev == this,
            "ERROR: Must remove from current list before adding to new list\n");
        mpNext = lpList;
        mpPrev = lpList->mpPrev;
        lpList->mpPrev = this;
        mpPrev->mpNext = this;
    }

    // CRT initializer82C4AFD8 constructs this sentinel with the same zero-value,
    // self-ring and source-handle stores as the native default constructor.
    const BaseResourcePtr NULLResourcePtr;
}
