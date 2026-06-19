#ifndef GAMESOURCE_RESOURCE_BRNRESOURCEALLOCATOR_H
#define GAMESOURCE_RESOURCE_BRNRESOURCEALLOCATOR_H

#include "types.hpp"
#include "rw/rwcore_structs.h"   // rw::Resource, rw::ResourceDescriptor

// ============================================================================
// GameSource/Resource/BrnResourceAllocator.h
//
// MINIMAL SLICE -- models only the debug resource-allocator surface that
// ICE::ICEMemory::Construct touches: the named heap allocators are RW resource
// allocators that hand back an rw::Resource (its m_baseResources[0] is the
// memory pointer). Construct allocates its 1.75 MB "ICE Debug Memory" block from
// the global debug allocator and builds the ICE edit heap over it.
//
// Shape recovered from references/Feb-2007/.../GameSource/Resource/BrnResourceAllocator.h
// (HeapResourceAllocator : rw::IResourceAllocator with a public Allocate() that
// forwards to the virtual DoAllocate) and from the X360 asm of ICEMemory::Construct
// (a DoAllocate vtable call on Allocators::mGlobalDebugAllocator returning a
// Resource, whose first base pointer is read back). The full allocator hierarchy
// (rw::IResourceAllocator's C++ helper layer, the LinearResourceAllocator family,
// Allocators' construction) lands when those TUs are reconstructed -- GROW this
// header then; do NOT fork the allocator types elsewhere.
// ============================================================================

namespace BrnResource
{

// The Burnout heap-backed RenderWare resource allocator. Public Allocate() returns
// an rw::Resource carved from the underlying general allocator (the X360 build
// resolves this through the IResourceAllocator vtable's DoAllocate slot).
class HeapResourceAllocator
{
public:
    // Allocate a resource matching lDescriptor (lpcName is a debug tag). The
    // returned rw::Resource's m_baseResources[0] is the memory-resource pointer.
    rw::Resource Allocate(const rw::ResourceDescriptor& lDescriptor, const char* lpcName = 0);
};

// Accessor for the global debug allocator (asserts the backing allocator exists).
// X360: returns &Allocators::mGlobalDebugAllocator. DECLARATION-ONLY here.
HeapResourceAllocator* GetDebugAllocator();

// Owns the engine's named global allocators. Construct asserts the debug
// allocator's backing general allocator exists before carving the ICE debug
// block (X360: Allocators::mpInternalDebugAllocator). MINIMAL SLICE -- only the
// one static the ICE path null-checks is modelled; the concrete
// rw::core::GeneralResourceAllocator type is opaque (pointer/null-check only).
class Allocators
{
public:
    static void* mpInternalDebugAllocator;   // X360 dword: backing general allocator
};

} // namespace BrnResource

#endif // GAMESOURCE_RESOURCE_BRNRESOURCEALLOCATOR_H
