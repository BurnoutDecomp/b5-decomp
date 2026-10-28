#ifndef CGS_RESOURCE_PTR_H
#define CGS_RESOURCE_PTR_H

#include "types.hpp"
#include <cstddef>
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"
#include "GameShared/GameClasses/System/Resource/CgsSmallResource.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsResource
{
    // Native value/link/handle layout. ARTIST copies three resource words at0/4/8,
    // links atC/10 and the source handle at14/18. DecFIGS names the trailing
    // mpThis member; native ResourcePtr arrays and owners require a32-byte stride.
    struct BaseResourcePtr
    {
        friend class Pool;
    public:
        BaseResourcePtr();
        ~BaseResourcePtr();

        void GetResource(SmallResource* lpResource) const;
        void GetResourceHandle(ResourceHandle* lpHandle) const;
        ResourceHandle GetResourceHandle() const;
        void SetResource(const SmallResource* lpResource);
        bool IsEqual(const void* lpOther) const;

    protected:
        void Reset();
        void RemoveFromCurrentList();
        void AddToNewList(BaseResourcePtr* lpList);
        void CreateFromPointer(const BaseResourcePtr* lpOther);
        void CreateFromHandle(const ResourceHandle* lpHandle);
        void Propogate();

        SmallResource mResourceMemory; // +00 cached main/system/local resources
        BaseResourcePtr* mpNext;       // +0C intrusive alias ring
        BaseResourcePtr* mpPrev;       // +10
        ResourceHandle mHandle;        // +14 source SmallResource and Entry
        BaseResourcePtr* mpThis;       // +1C; untouched by the ARTIST constructor

        static void AssertNativeLayout()
        {
            CGS_STATIC_ASSERT((sizeof(SmallResource) == 12), "X360 three resource words");
            CGS_STATIC_ASSERT((sizeof(BaseResourcePtr) == 32), "X360 resource pointer stride");
            CGS_STATIC_ASSERT((offsetof(BaseResourcePtr, mResourceMemory) == 0), "ARTIST cached resource");
            CGS_STATIC_ASSERT((offsetof(BaseResourcePtr, mpNext) == 0x0C), "ARTIST next alias");
            CGS_STATIC_ASSERT((offsetof(BaseResourcePtr, mpPrev) == 0x10), "ARTIST previous alias");
            CGS_STATIC_ASSERT((offsetof(BaseResourcePtr, mHandle) == 0x14), "ARTIST source handle");
            CGS_STATIC_ASSERT((offsetof(BaseResourcePtr, mpThis) == 0x1C), "native trailing member");
        }
    };

    template <class Type>
    struct ResourcePtr : public BaseResourcePtr
    {
    public:
        ResourcePtr() {}
        ResourcePtr(const ResourcePtr<Type>& lrOther);
        ResourcePtr(const BaseResourcePtr& lrOther);
        ResourcePtr(const ResourceHandle& lrHandle)
        {
            CreateFromHandle(&lrHandle);
        }

        ResourcePtr<Type>& operator=(const ResourcePtr<Type>& lrOther)
        {
            CreateFromHandle(&lrOther.mHandle);
            return *this;
        }
        ResourcePtr<Type>& operator=(const BaseResourcePtr& lrOther);
        ResourcePtr<Type>& operator=(const ResourceHandle& lrHandle)
        {
            CreateFromHandle(&lrHandle);
            return *this;
        }

        bool HasMemoryResource() const { return mResourceMemory.m_baseResources[0] != 0; }
        Type* operator->()
        {
            CGS_ASSERT(mResourceMemory.m_baseResources[0],
                "Can not instance resource pointer - it has no main memory resource\n");
            return static_cast<Type*>(mResourceMemory.m_baseResources[0]);
        }
        const Type* operator->() const
        {
            CGS_ASSERT(mResourceMemory.m_baseResources[0],
                "Can not instance resource pointer - it has no main memory resource\n");
            return static_cast<const Type*>(mResourceMemory.m_baseResources[0]);
        }
        Type& operator*()
        {
            CGS_ASSERT(mResourceMemory.m_baseResources[0],
                "Can not instance resource pointer - it has no main memory resource\n");
            return *static_cast<Type*>(mResourceMemory.m_baseResources[0]);
        }
        const Type& operator*() const
        {
            CGS_ASSERT(mResourceMemory.m_baseResources[0],
                "Can not dereference resource pointer - it has no main memory resource\n");
            return *static_cast<const Type*>(mResourceMemory.m_baseResources[0]);
        }
        Type* GetMemoryResource()
        {
            CGS_ASSERT(mResourceMemory.m_baseResources[0],
                "Can not instance resource pointer - it has no main memory resource\n");
            return static_cast<Type*>(mResourceMemory.m_baseResources[0]);
        }
        const Type* GetMemoryResource() const
        {
            CGS_ASSERT(mResourceMemory.m_baseResources[0],
                "Can not instance resource pointer - it has no main memory resource\n");
            return static_cast<const Type*>(mResourceMemory.m_baseResources[0]);
        }

        bool operator==(const ResourcePtr<Type>& lrOther) const;
        bool operator==(const ResourceHandle& lrHandle) const;
        bool operator!=(const ResourcePtr<Type>& lrOther) const;
        bool operator!=(const ResourceHandle& lrHandle) const;
        bool operator==(const BaseResourcePtr& lrOther) const { return IsEqual(&lrOther); }
        bool operator!=(const BaseResourcePtr& lrOther) const { return !IsEqual(&lrOther); }
    };

    extern const BaseResourcePtr NULLResourcePtr;
}
#endif
