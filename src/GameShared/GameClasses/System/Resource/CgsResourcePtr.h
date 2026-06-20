#ifndef CGS_RESOURCE_PTR_H
#define CGS_RESOURCE_PTR_H

#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"   // CgsResource::ResourceHandle
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT

// ============================================================================
// GameShared/GameClasses/System/Resource/CgsResourcePtr.h
//
// CgsResource::BaseResourcePtr + the generic CgsResource::ResourcePtr<Type>
// smart-pointer template. Reconstructed from the DecFIGS DWARF
// (GameShared/GameClasses/System/Resource/CgsResourcePtr.h) and the X360
// pseudocode for the ResourcePtr<T> accessors that the ARTIST build emitted
// out-of-line (operator->, operator*, GetMemoryResource for many T).
//
// A ResourcePtr<Type> is an intrusively-linked smart pointer onto a loaded
// resource. BaseResourcePtr carries the main-memory resource pointer
// (mpResourceMemory, offset 0), a doubly-linked alias list (mpNext/mpPrev), a
// copy of the source ResourceHandle, a self pointer (mpThis) and the owning-
// thread id used by the propagation logic. ResourcePtr<Type> adds NO data
// members -- it just type-casts the base's resource pointer to Type* in
// operator->/operator*/GetMemoryResource.
//
// LAYOUT NOTE (X360 vs DWARF): the DWARF declares the first field as a *value*
// `ResourceHandle::Resource mResourceMemory` (typedef Resource == SmallResource).
// The X360 binary for every accessor reads offset 0 as a single 4-byte pointer
// (`int* a1; if (!*a1) ...; return *a1`) and returns it directly as Type*. X360
// is authoritative for shape, so the field is modeled as the pointer it is
// actually used as -- `void* mpResourceMemory` -- which the accessors
// reinterpret/static_cast to Type*. The committed CgsResourceHandle.h does not
// expose the nested `Resource` typedef, so void* is the minimal non-forking
// slice; bit-identical to the X360 4-byte field. The "main memory resource"
// wording in the assert ("it has no main memory resource") matches the
// !mpResourceMemory test 1:1.
//
// This is the ONE shared home for the generic. Every per-type instantiation TU
// (CgsResourcePtr_BrnTriggerData.cpp, CgsResourcePtr_BrnStreetData_StreetData.cpp,
// ResourcePtr_BrnProgressionProgressionData.cpp, BrnTrafficDataResourcePtr.cpp,
// ResourcePtr_BrnAI_AISectionsData.cpp, ...) only emits `template ...;`
// explicit-instantiation lines against this header.
// ============================================================================

namespace EAThread
{
    // EAThreadDynamicData::ThreadId is an EA SDK platform type (not project-owned)
    // and is NOT committed under b5-decomp/src. Modeled as a minimal 4-byte opaque
    // id, matching the X360 4-byte field width. No ResourcePtr accessor touches it
    // (declaration-only padding); replace with the real EA header alias if/when the
    // EAThread shim lands.
    typedef u32 ThreadId;
}

namespace CgsResource
{
    // CgsResourcePtr.h:65 (DWARF). Non-templated base shared by every ResourcePtr<T>.
    // Members declared at their DWARF offsets; only mpResourceMemory (offset 0) is
    // read by the templated accessors. The list/lifecycle methods have their own TUs
    // and are declaration-only here.
    struct BaseResourcePtr
    {
    public:
        BaseResourcePtr();
        ~BaseResourcePtr();

        void           GetResource(void** lppResource) const;          // :76
        void           GetResourceHandle(ResourceHandle* lpHandle) const; // :80
        ResourceHandle GetResourceHandle() const;                      // :84
        void           SetResource(void* const* lppResource);          // :88
        bool           IsEqual(void* const* lppResource) const;        // :92

    protected:
        void Reset();                                                  // :111
        void RemoveFromCurrentList();                                  // :115
        void AddToNewList(BaseResourcePtr* lpList);                    // :119
        void CreateFromPointer(const BaseResourcePtr* lpOther);        // :123
        void CreateFromHandle(const ResourceHandle* lpHandle);         // :127
        void Propogate();                                              // :131  [sic] DWARF spelling

    protected:
        void*               mpResourceMemory;  // +0x00  :97  main-memory resource ptr (X360 *a1)
        BaseResourcePtr*    mpNext;            // +0x04  :98  intrusive alias list
        BaseResourcePtr*    mpPrev;            // +0x08  :99
        ResourceHandle      mHandle;           // +0x0C  :100 (two pointers == 8 bytes)
        BaseResourcePtr*    mpThis;            // +0x14  :104
        EAThread::ThreadId  muThreadId;        // +0x18  :107 owning-thread id (DWARF _mUThreadId)
    };

    // CgsResourcePtr.h:160 (DWARF). Type-safe wrapper over BaseResourcePtr; adds no
    // data members. The accessors below guard the null resource via CGS_ASSERT (the
    // X360-baked file/line are discarded per project convention). The ctors /
    // assignment / comparison operators are declared-only (their own TUs / not
    // attested in this batch).
    template <class Type>
    struct ResourcePtr : public BaseResourcePtr
    {
    public:
        // Trivial default ctor (X360-inlined): BaseResourcePtr() zeroes the fields. Defined inline so a
        // ResourcePtr<T> can be a data member; the copy/handle ctors + operator= remain declared-only
        // (their own TUs / not attested in this batch).
        ResourcePtr() {}
        ResourcePtr(const ResourcePtr<Type>& lrOther);
        ResourcePtr(const BaseResourcePtr& lrOther);
        ResourcePtr(const ResourceHandle& lrHandle);

        ResourcePtr<Type>& operator=(const ResourcePtr<Type>& lrOther);
        ResourcePtr<Type>& operator=(const BaseResourcePtr& lrOther);
        ResourcePtr<Type>& operator=(const ResourceHandle& lrHandle);

        // CgsResourcePtr.h:538 -> baked assert line 544 (non-const).
        Type* operator->()
        {
            CGS_ASSERT(mpResourceMemory,
                "Can not instance resource pointer - it has no main memory resource\n");
            return static_cast<Type*>(mpResourceMemory);
        }

        // CgsResourcePtr.h:557 -> baked assert line 563 (const).
        const Type* operator->() const
        {
            CGS_ASSERT(mpResourceMemory,
                "Can not instance resource pointer - it has no main memory resource\n");
            return static_cast<const Type*>(mpResourceMemory);
        }

        // CgsResourcePtr.h:612 -> baked assert line 563 (non-const operator*).
        Type& operator*()
        {
            CGS_ASSERT(mpResourceMemory,
                "Can not instance resource pointer - it has no main memory resource\n");
            return *static_cast<Type*>(mpResourceMemory);
        }

        const Type& operator*() const
        {
            CGS_ASSERT(mpResourceMemory,
                "Can not instance resource pointer - it has no main memory resource\n");
            return *static_cast<const Type*>(mpResourceMemory);
        }

        // CgsResourcePtr.h:575 -> baked assert line 581 (non-const).
        Type* GetMemoryResource()
        {
            CGS_ASSERT(mpResourceMemory,
                "Can not instance resource pointer - it has no main memory resource\n");
            return static_cast<Type*>(mpResourceMemory);
        }

        // CgsResourcePtr.h:593 -> baked assert line 599 (const).
        const Type* GetMemoryResource() const
        {
            CGS_ASSERT(mpResourceMemory,
                "Can not instance resource pointer - it has no main memory resource\n");
            return static_cast<const Type*>(mpResourceMemory);
        }

        bool operator==(const ResourcePtr<Type>& lrOther) const;
        bool operator==(const BaseResourcePtr& lrOther) const;
        bool operator==(const ResourceHandle& lrHandle) const;
        bool operator!=(const ResourcePtr<Type>& lrOther) const;
        bool operator!=(const BaseResourcePtr& lrOther) const;
        bool operator!=(const ResourceHandle& lrHandle) const;
    };
}

#endif // CGS_RESOURCE_PTR_H
