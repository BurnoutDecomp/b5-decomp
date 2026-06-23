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
// Shape recovered from the DWARF (HeapResourceAllocator : rw::IResourceAllocator
// with a public Allocate() that forwards to the virtual DoAllocate) and from the
// X360 asm of ICEMemory::Construct
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
class HeapResourceAllocator : public rw::IResourceAllocator
{
public:
    // Allocate a resource matching lDescriptor (lpcName is a debug tag). The
    // returned rw::Resource's m_baseResources[0] is the memory-resource pointer.
    rw::Resource Allocate(const rw::ResourceDescriptor& lDescriptor, const char* lpcName = 0);

    // rw::IResourceAllocator interface: the resource modules allocate their backing memory through
    // this (virtual) entry point; it just forwards to Allocate.
    rw::Resource DoAllocate(const rw::ResourceDescriptor& lDescriptor, const char* lpcName) override;
};

// The Burnout linear (bump) RenderWare resource allocator. Sub-allocates from a fixed
// region by advancing a cursor; frees happen en masse (re-Construct). Like
// HeapResourceAllocator it IS-A rw::IResourceAllocator, so the resource modules can carve
// memory through the virtual DoAllocate. MINIMAL SLICE -- only the polymorphic surface and
// the rw allocator interface are modelled here; Construct (@0x82661A60), DoAllocate
// (@0x82664800), DoFree (@0x82661CF8) and DoFreeDisposable (@0x82661D88) land when those
// TUs are reconstructed (GROW this class then, do NOT fork it elsewhere). The X360 emits a
// scalar deleting destructor for it (@0x82666948) because it is deleted polymorphically.
// It inherits the rw::IResourceAllocator virtual surface (DoAllocate / virtual dtor); the
// concrete overrides land with the allocator-leaf TUs. The out-of-line (defaulted) virtual
// destructor anchors the vtable in BrnResourceAllocator.cpp so the X360's polymorphic-delete
// thunk (`scalar deleting destructor' @0x82666948) is emitted there.
class DefaultLinearAllocator : public rw::IResourceAllocator
{
public:
    ~DefaultLinearAllocator() override;
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
