#pragma once

// CgsResource::ResourceHandle — a handle onto a loaded resource (the resource memory
// plus its source pool entry). Reconstructed from the DecFIGS DWARF; minimal recon of
// the two stored pointers (accessors are inlined / live in their own TUs).
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"   // CgsResource::ID (returned by value)

namespace CgsResource
{
    class Entry;

    struct ResourceHandle
    {
        void* mpResourceMemory;
        Entry* mpSourceEntry;

        // CgsResourceHandle.h:93 (DWARF). The resource's identity hash, read from the
        // source pool entry. Out-of-line in the X360 ARTIST build (@0x828D8438); declared
        // here, defined in CgsResourceHandle.cpp.
        ID GetResourceId() const;

        // ADDITIVE GROW (DWARF-attested via the DecFIGS DispatchThreadInputBuffer::Construct
        // call list; the X360 inlines it as the two null stores @0x823C5BB8 +0x9994/+0x9998):
        // drop the handle back to the null state.
        void Clear()
        {
            mpResourceMemory = 0;
            mpSourceEntry = 0;
        }

        // ADDITIVE GROW (ColourCalibrationScreen::Update @0x8246AA28, whose inlined
        // "mColourCalibrationTextureHandle != CgsResource::NULLResourceHandle" assert is
        // this pairwise compare): handles are equal when both stored pointers match.
        bool operator==(const ResourceHandle& lrOther) const
        {
            return mpResourceMemory == lrOther.mpResourceMemory &&
                   mpSourceEntry == lrOther.mpSourceEntry;
        }
        bool operator!=(const ResourceHandle& lrOther) const { return !(*this == lrOther); }
    };

    // The null handle sentinel (X360 .data qword @0x82FB3688, initial value {NULL, NULL} --
    // read from the decrypted XEX). Defined in CgsResourceHandle.cpp.
    extern const ResourceHandle NULLResourceHandle;

    // ========================================================================
    // CgsResource::SafeResourceHandle<ResourceType> — a typed wrapper over a
    // ResourceHandle. DWARF home: CgsResourceHandle.h:138 (DecFIGS dwarfdump of
    // CgsResourceHandle.h shows it deriving publicly from ResourceHandle and
    // declaring operator->()/operator->() const plus the two pointer-conversion
    // operators). It adds NO data members; the only stored state is the inherited
    // ResourceHandle (mpResourceMemory @+0x00, mpSourceEntry @+0x04).
    //
    // LAYOUT NOTE (X360-authoritative double indirection): the X360 ARTIST build
    // emits every accessor as a DOUBLE dereference of the handle —
    //     lwz r11, 0(this)   ; r11 = mpResourceMemory  (a CgsResource::SmallResource*)
    //     lwz r11, 0(r11)    ; r11 = *(void**)mpResourceMemory  (the main-memory ptr)
    // i.e. mpResourceMemory points at the SmallResource and the accessor returns the
    // SmallResource's first dword (its main-memory resource, SmallResource::
    // GetMemoryResource()) reinterpreted as ResourceType*. The null guard tests that
    // inner pointer ("it has no main memory resource"). GetMemoryResource() is
    // out-of-line (its own TU), so the build inlined it to the raw load reproduced
    // below; modelling it as the direct deref keeps both the semantics and the
    // inline shape. mpResourceMemory is kept as void* (the committed ResourceHandle
    // field type) and reinterpret_cast through.
    //
    // *** FLAG — COMMITTED-TYPE DIVERGENCE (do NOT silently swap) ***
    // GameShared/GameClasses/Fonts/CgsFont.h ALSO defines a
    // CgsResource::SafeResourceHandle<T>, but with a DIFFERENT shape/body:
    //     template<class T> struct SafeResourceHandle { T* mpResource; u32 muSafetyId;
    //         T* operator->() const { return mpResource; } ... };
    // That model is a SINGLE-deref simplification ({resource-ptr, safety-id}) and does
    // NOT match this DWARF/X360 layout (`: public ResourceHandle`, double-deref). The
    // two definitions are an ODR conflict if both headers are pulled into one TU. This
    // BrnParticle facade TU includes ONLY this header (never CgsFont.h), so it compiles
    // cleanly, but the divergence is a real committed-type mismatch that the Fonts
    // group's home should be reconciled against this DWARF-accurate one. Reported as a
    // dep_flag rather than edited here (CgsFont.h is another group's file).
    // ========================================================================
    template <class ResourceType>
    struct SafeResourceHandle : public ResourceHandle
    {
        // CgsResourceHandle.h:401 — non-const operator-> (baked assert line 403,
        // "Can not instance resource pointer ...").
        ResourceType* operator->()
        {
            CGS_ASSERT(*reinterpret_cast<void* const*>(mpResourceMemory),
                "Can not instance resource pointer - it has no main memory resource\n");
            return *reinterpret_cast<ResourceType* const*>(mpResourceMemory);
        }

        // CgsResourceHandle.h:417 — const operator-> (baked assert line 419,
        // "Can not instance resource pointer ...").
        const ResourceType* operator->() const
        {
            CGS_ASSERT(*reinterpret_cast<void* const*>(mpResourceMemory),
                "Can not instance resource pointer - it has no main memory resource\n");
            return *reinterpret_cast<ResourceType* const*>(mpResourceMemory);
        }

        // CgsResourceHandle.h:488 — non-const conversion to ResourceType* (baked
        // assert line 505, "Can not get resource pointer ...").
        operator ResourceType*()
        {
            CGS_ASSERT(*reinterpret_cast<void* const*>(mpResourceMemory),
                "Can not get resource pointer - it has no main memory resource\n");
            return *reinterpret_cast<ResourceType* const*>(mpResourceMemory);
        }

        // CgsResourceHandle.h:503 — const conversion to const ResourceType* (baked
        // assert line 505, "Can not get resource pointer ...").
        operator const ResourceType*() const
        {
            CGS_ASSERT(*reinterpret_cast<void* const*>(mpResourceMemory),
                "Can not get resource pointer - it has no main memory resource\n");
            return *reinterpret_cast<ResourceType* const*>(mpResourceMemory);
        }

        // Convenience accessors (the debug-font path uses these; grown here per the
        // CgsResourcePtr_CgsResource_Font.cpp DEP_FLAG so CgsFont.h can drop its private handle and reuse
        // this one). A null handle has no resource memory; Get() null-safely follows the double-deref.
        bool IsNull() const { return mpResourceMemory == 0; }
        ResourceType* Get() const
        {
            return mpResourceMemory ? *reinterpret_cast<ResourceType* const*>(mpResourceMemory) : 0;
        }
    };
}
