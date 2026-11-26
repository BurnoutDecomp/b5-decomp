#ifndef CGS_RESOURCE_PTR_H
#define CGS_RESOURCE_PTR_H

#include "types.hpp"
#include <cstddef>
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"
#include "GameShared/GameClasses/System/Resource/CgsSmallResource.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CGS_ASSERT_STREAM
#include "GameShared/GameClasses/Development/CgsStrStream.h"         // the streamed assert messages

// CgsResource::BaseResourcePtr / ResourcePtr<T>: a typed, relinkable view of a pool resource.
//
// Declarator lines are the DecFIGS DWARF's (CgsResourcePtr.h: BaseResourcePtr h:65, NULLResourcePtr
// h:149, ResourcePtr<T> h:160, its members h:417 .. h:711). ResourcePtr<T>'s members are defined
// out of the class, inline, as the LegacySrc copy of this header does; the accessors that check
// the main-memory resource stream their message through the assert buffer on the line ARTIST
// bakes (file string @0x82007258 / 0x8201AE48): operator-> 544, const operator-> 563,
// GetMemoryResource 581, const GetMemoryResource 599, operator* 618. ARTIST keeps an out-of-line
// copy of operator-> (e.g. ResourcePtr<BrnAI::AISectionsData>::operator-> @0x82324FA0: the
// streamed assert, then the cached word) and calls it.

namespace CgsResource
{
    // Native value/link/handle layout. ARTIST copies three resource words at 0/4/8,
    // links at C/10 and the source handle at 14/18. DecFIGS names the trailing
    // mpThis member; native ResourcePtr arrays and owners require a 32-byte stride.
    template <class Type> struct ResourcePtr;





































    struct BaseResourcePtr
    {
        friend class Pool;
        friend struct Entry;
        template <class Type> friend struct ResourcePtr;
    public:
        BaseResourcePtr();
        ~BaseResourcePtr();

        void GetResource(SmallResource* lpResource) const;
        void GetResourceHandle(ResourceHandle* lpHandle) const;
        ResourceHandle GetResourceHandle() const;
        void SetResource(const SmallResource* lpResource);
        bool IsEqual(const SmallResource* lpResource) const;

    protected:
        // DWARF h:111; the body (h:255-262 in DecFIGS) hands the handle to
        // ResourceHandle::Construct (see CgsResourceHandle.h).
        void Reset()
        {
            mpNext = this;
            mpPrev = this;
            mHandle.Construct(&mResourceMemory, 0);
        }
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

    // DWARF CgsResourcePtr.h:69; header-inline as LegacySrc CgsResourcePtr.h:217 spells it. ARTIST
    // inlines it at its call sites (e.g. FactoryNew<GenericRwacWaveContent> 0x826DD768..0x826DD784)
    // and keeps one out-of-line copy (0x82204E20, called by the dynamic initializer 0x82C71080).
    // SmallResource initializes every lane; ARTIST clears main memory again. mpThis (+1C) is not
    // initialized.
    inline BaseResourcePtr::BaseResourcePtr()
    {
        mResourceMemory.m_baseResources[0] = 0;
        mpNext = this;
        mpPrev = this;
        mHandle.mpResourceMemory = &mResourceMemory;
        mHandle.mpSourceEntry = 0;
    }

    // DWARF CgsResourcePtr.h:92 (IsEqual(const ResourceHandle::Resource*) const); header-inline
    // as LegacySrc CgsResourcePtr.h:331 defines it. ARTIST keeps an out-of-line copy (0x8227D298:
    // rw::operator== inlined, then `!= 0`) and inlines it inside loops (ICEList::GetICETakeData
    // 0x8267BE10, PFXNodeFader::LookupColourCube 0x82504314).
    inline bool BaseResourcePtr::IsEqual(const SmallResource* lpResource) const
    {
        return ((mResourceMemory == *lpResource) != 0);
    }




















    // DecFIGS CgsResourcePtr.h:149 defines this namespace-scope const in the
    // header. Its internal linkage gives each translation unit its own sentinel;
    // ARTIST TrafficEntity and TrafficCarStreamer use different instances.
    const BaseResourcePtr NULLResourcePtr;









    template <class Type>
    struct ResourcePtr : public BaseResourcePtr
    {
    public:
        inline ResourcePtr();
        inline ResourcePtr(const ResourcePtr<Type>& lrOther);
        inline ResourcePtr(const BaseResourcePtr& lrOther);
        inline ResourcePtr(const ResourceHandle& lrHandle);

        inline ResourcePtr<Type>& operator=(const ResourcePtr<Type>& lrOther);
        inline ResourcePtr<Type>& operator=(const BaseResourcePtr& lrOther);
        inline ResourcePtr<Type>& operator=(const ResourceHandle& lrHandle);

        bool HasMemoryResource() const { return mResourceMemory.m_baseResources[0] != 0; }

        inline Type* operator->();
        inline const Type* operator->() const;
        inline Type* GetMemoryResource();
        inline const Type* GetMemoryResource() const;
        inline Type& operator*();
        inline const Type& operator*() const;

        inline bool operator==(const ResourcePtr<Type>& lrOther) const;
        inline bool operator==(const BaseResourcePtr& lrOther) const;
        inline bool operator==(const ResourceHandle& lrHandle) const;
        inline bool operator!=(const ResourcePtr<Type>& lrOther) const;
        inline bool operator!=(const BaseResourcePtr& lrOther) const;
        inline bool operator!=(const ResourceHandle& lrHandle) const;
    };



































































































































































































































    template <class Type>
    inline
    ResourcePtr<Type>::ResourcePtr()
    {
    }



























    template <class Type>
    inline
    ResourcePtr<Type>::ResourcePtr(const BaseResourcePtr& lrOther)
    {
        CreateFromHandle(&lrOther.mHandle);
    }











    template <class Type>
    inline
    ResourcePtr<Type>::ResourcePtr(const ResourceHandle& lrHandle)
    {
        CreateFromHandle(&lrHandle);
    }











    template <class Type>
    inline ResourcePtr<Type>&
    ResourcePtr<Type>::operator=(const ResourcePtr<Type>& lrOther)
    {
        CreateFromHandle(&lrOther.mHandle);
        return *this;
    }











    template <class Type>
    inline ResourcePtr<Type>&
    ResourcePtr<Type>::operator=(const BaseResourcePtr& lrOther)
    {
        CreateFromHandle(&lrOther.mHandle);
        return *this;
    }











    template <class Type>
    inline ResourcePtr<Type>&
    ResourcePtr<Type>::operator=(const ResourceHandle& lrHandle)
    {
        CreateFromHandle(&lrHandle);
        return *this;
    }


    // ARTIST used two source spellings for this header. A translation unit
    // selects a historical spelling when its original bodies establish it.
#if defined(CGS_RESOURCE_PTR_MATCH_RELATIVE_ASSERTS)
#define CGS_RESOURCE_PTR_ASSERT_FILE "..\\..\\..\\GameShared\\GameClasses\\System/Resource/CgsResourcePtr.h"
#elif defined(CGS_RESOURCE_PTR_MATCH_ABSOLUTE_ASSERTS)
#define CGS_RESOURCE_PTR_ASSERT_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourcePtr.h"
#else
#define CGS_RESOURCE_PTR_ASSERT_FILE __FILE__
#endif

    template <class Type>
    inline Type*
    ResourcePtr<Type>::operator->()
    {




        CGS_ASSERT_STREAM_AT(mResourceMemory.GetMemoryResource(),
            "Can not instance resource pointer - it has no main memory resource\n",
            CGS_RESOURCE_PTR_ASSERT_FILE, 544);
        return static_cast<Type*>(mResourceMemory.GetMemoryResource());
    }






    template <class Type>
    inline const Type*
    ResourcePtr<Type>::operator->() const
    {




        CGS_ASSERT_STREAM_AT(mResourceMemory.GetMemoryResource(),
            "Can not instance resource pointer - it has no main memory resource\n",
            CGS_RESOURCE_PTR_ASSERT_FILE, 563);
        return static_cast<const Type*>(mResourceMemory.GetMemoryResource());
    }





    template <class Type>
    inline Type*
    ResourcePtr<Type>::GetMemoryResource()
    {




        CGS_ASSERT_STREAM_AT(mResourceMemory.GetMemoryResource(),
            "Can not instance resource pointer - it has no main memory resource\n",
            CGS_RESOURCE_PTR_ASSERT_FILE, 581);
        return static_cast<Type*>(mResourceMemory.GetMemoryResource());
    }





    template <class Type>
    inline const Type*
    ResourcePtr<Type>::GetMemoryResource() const
    {




        CGS_ASSERT_STREAM_AT(mResourceMemory.GetMemoryResource(),
            "Can not instance resource pointer - it has no main memory resource\n",
            CGS_RESOURCE_PTR_ASSERT_FILE, 599);
        return static_cast<const Type*>(mResourceMemory.GetMemoryResource());
    }




#undef CGS_RESOURCE_PTR_ASSERT_FILE

    template <class Type>
    inline Type&
    ResourcePtr<Type>::operator*()
    {




        CGS_ASSERT_STREAM(mResourceMemory.GetMemoryResource(), "Can not dereference resource pointer - it has no main memory resource\n");
        return *static_cast<Type*>(mResourceMemory.GetMemoryResource());
    }
























    template <class Type>
    inline bool
    ResourcePtr<Type>::operator==(const BaseResourcePtr& lrOther) const
    {
        return lrOther.IsEqual(&mResourceMemory);
    }










































    template <class Type>
    inline bool
    ResourcePtr<Type>::operator!=(const BaseResourcePtr& lrOther) const
    {
        return !lrOther.IsEqual(&mResourceMemory);
    }



























    template <class Type>
    inline const Type&
    ResourcePtr<Type>::operator*() const
    {
        CGS_ASSERT_STREAM(mResourceMemory.GetMemoryResource(), "Can not dereference resource pointer - it has no main memory resource\n");
        return *static_cast<const Type*>(mResourceMemory.GetMemoryResource());
    }
}
#endif
