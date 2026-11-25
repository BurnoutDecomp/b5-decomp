#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceBasePool.h"

namespace CgsResource
{
    // ARTIST828D9418 broadcasts the handle and complete three-word resource value.
    void BaseResourcePtr::Propogate()
    {
        CGS_ASSERT_STREAM_AT(mHandle.mpSourceEntry != 0,
            "ERROR: Resource pointer has not been initialized\n",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourcePtr.cpp", 72);
        for (BaseResourcePtr* lpNode = mpNext; lpNode != this; lpNode = lpNode->mpNext)
        {
            lpNode->mHandle = mHandle;
            const SmallResource* lpResource = static_cast<const SmallResource*>(mHandle.mpResourceMemory);
            lpNode->mResourceMemory = *lpResource;
        }
    }

    // ARTIST828D93A0 reads the source value unconditionally, then relinks aliases.
    void BaseResourcePtr::CreateFromHandle(const ResourceHandle* lpSrcHandle)
    {
        mHandle = *lpSrcHandle;
        const SmallResource* lpResource = static_cast<const SmallResource*>(mHandle.mpResourceMemory);
        mResourceMemory = *lpResource;
        if (mpNext != 0) mpNext->mpPrev = mpPrev;
        if (mpPrev != 0) mpPrev->mpNext = mpNext;
        mpNext = this;
        mpPrev = this;

        Entry* lpEntry = mHandle.mpSourceEntry;
        if (lpEntry != 0)
            AddToNewList(&lpEntry->mPtr);
    }
}
