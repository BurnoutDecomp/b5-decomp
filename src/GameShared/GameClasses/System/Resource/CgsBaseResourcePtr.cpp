#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"

namespace CgsResource
{
    // BaseResourcePtr::BaseResourcePtr (ARTIST 0x82204E20) is header-inline: CgsResourcePtr.h.

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
        CGS_ASSERT_STREAM_AT(mpNext == this && mpPrev == this,
            "ERROR: Must remove from current list before adding to new list\n",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourcePtr.h", 384);
        mpNext = lpList;
        mpPrev = lpList->mpPrev;
        lpList->mpPrev = this;
        mpPrev->mpNext = this;
    }

}
