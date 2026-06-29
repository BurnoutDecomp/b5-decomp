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
    //
    // *** FLAG -- COMMITTED-HOME LAYOUT CORRECTION (X360-authoritative reorder) ***
    // The prior commit declared the members in DWARF source-LINE order
    // (mpResourceMemory, mpNext, mpPrev, mHandle, mpThis, muThreadId), which placed
    // the intrusive list pointers at +0x04/+0x08 and mHandle at +0x0C. The X360
    // ARTIST binary proves the real byte layout is different: every list/identity
    // method (ctor @0x82204E20, dtor @0x821F1E18, AddToNewList @0x828D6230,
    // IsEqual @0x8227D298) touches mpNext at +0x0C and mpPrev at +0x10, and treats
    // the FIRST THREE dwords (+0x00,+0x04,+0x08) as the comparable identity --
    // i.e. mResourceMemory followed immediately by the two ResourceHandle pointers.
    // So mHandle sits at +0x04..+0x0B and the list links at +0x0C/+0x10. The member
    // order below is reordered to match the asm. This is a layout REORDER (not an
    // additive grow); only mpResourceMemory (+0x00) is unchanged, so the templated
    // accessors in every per-type TU are unaffected (they only read offset 0). Any
    // code that read mHandle / mpNext / mpPrev by NAME stays correct. No prior TU
    // verified an absolute offset of these members across a pointer, so the reorder
    // is safe. Source-line refs kept in trailing comments for traceability.
    struct BaseResourcePtr
    {
    public:
        BaseResourcePtr();
        ~BaseResourcePtr();

        void           GetResource(void** lppResource) const;          // :76
        void           GetResourceHandle(ResourceHandle* lpHandle) const; // :80
        ResourceHandle GetResourceHandle() const;                      // :84
        void           SetResource(void* const* lppResource);          // :88
        // IsEqual @0x8227D298: the X360 compares the first three dwords of THIS and
        // of *lpOther (mResourceMemory + the two mHandle pointers); equal iff all
        // three match. Modeled with a raw const-pointer arg because the binary reads
        // three consecutive dwords from it (a BaseResourcePtr's / ResourceHandle's
        // identity region). DWARF declared it IsEqual(const Resource*).
        bool           IsEqual(const void* lpOther) const;             // :92

    protected:
        void Reset();                                                  // :111
        void RemoveFromCurrentList();                                  // :115
        void AddToNewList(BaseResourcePtr* lpList);                    // :119
        void CreateFromPointer(const BaseResourcePtr* lpOther);        // :123
        void CreateFromHandle(const ResourceHandle* lpHandle);         // :127
        void Propogate();                                              // :131  [sic] DWARF spelling

    protected:
        void*               mpResourceMemory;  // +0x00  :97  main-memory resource ptr (X360 *a1)
        ResourceHandle      mHandle;           // +0x04  :100 (two pointers == 8 bytes; +0x04,+0x08)
        BaseResourcePtr*    mpNext;            // +0x0C  :98  intrusive alias list (X360 +0xC)
        BaseResourcePtr*    mpPrev;            // +0x10  :99  (X360 +0x10)
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

        // CgsResourcePtr.h:631 -> baked assert line 637 (const operator*).
        // FLAG (verified X360 fact, additive correction): this const-deref accessor
        // fires the "Can not DEREFERENCE resource pointer ..." message at baked line
        // 637, NOT the "instance" message the other accessors use. Proven by the
        // out-of-line instantiation BrnResource::VehicleListResourc @ 0x82665E88
        // (ResourcePtr<VehicleListResource>::operator*() const), baked file
        // CgsResourcePtr.h line 637, msg "Can not dereference resource pointer - it
        // has no main memory resource". No prior instantiation forced this symbol, so
        // correcting the message here is safe (no other TU re-verifies against it).
        const Type& operator*() const
        {
            CGS_ASSERT(mpResourceMemory,
                "Can not dereference resource pointer - it has no main memory resource\n");
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

    // ADDITIVE GROW: the engine's shared "null" resource pointer sentinel. Game code
    // compares loaded resource pointers against it (`lResource != CgsResource::NULLResourcePtr`)
    // to assert a handle is populated; the X360 ARTIST build implements that compare as
    // BaseResourcePtr::IsEqual(&NULLResourcePtr, lResource) (e.g. the per-component
    // RaceCarStreamer::On*Loaded / Get*Resource asserts read &dword_82FAD94C, which IS
    // CgsResource::NULLResourcePtr). Declared extern here (its definition / rodata global
    // is not yet recovered in this port -- compile-only references are satisfied by the
    // declaration). GROW with the real definition when the sentinel's home TU lands.
    extern const BaseResourcePtr NULLResourcePtr;
}

#endif // CGS_RESOURCE_PTR_H
