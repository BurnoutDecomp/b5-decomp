#pragma once

#include "types.hpp"
#include <cstdint>

// =============================================================================
// CgsGraphics::Ptr32<T> -- a SERIALISED 32-bit pointer slot.
//
// The world-graphics resources (InstanceList / Instance, Model, and the flip-only
// material family) are streamed in as the console's own memory image: the world
// data porter emits them as an endian flip of the X360 blob, and each resource
// type's FixUp relocates them IN PLACE. Every pointer inside those blobs is
// therefore FOUR BYTES, not eight.
//
// Declaring such a struct with host-width pointers silently doubles every offset
// past the first pointer and splices adjacent fields together on the first read
// (a dereferenced address whose HIGH 32 BITS hold the NEXT field is the
// signature). Ptr32<T> pins the slot to the console's 4 bytes while staying
// source-compatible at every `x->member->...` / `T* p = x->member;` call site.
//
// Resolving a slot to a host pointer is the project's low-4 GB PointerFromU32
// convention: the engine's entire root allocation is reserved below 4 GB
// (CgsMemory::LowMemory, see BrnResourceAllocator.cpp), so a 32-bit slot holds a
// complete host address.
// =============================================================================

namespace CgsGraphics
{
    template <class T>
    struct Ptr32
    {
        u32 muSlot;

        T*   Get()            const { return reinterpret_cast<T*>(static_cast<uintptr_t>(muSlot)); }
        operator T*()         const { return Get(); }
        T*   operator->()     const { return Get(); }
        T&   operator*()      const { return *Get(); }
        T&   operator[](u32 luIndex) const { return Get()[luIndex]; }
    };
}
