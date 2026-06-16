#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugCollections.h"

#include <new>   // ::operator new / ::operator delete

// Custom array-new the debug static pools allocate their backing blocks through (DWARF
// CgsDebugCollections.h:45). The X360 routes this through the rw resource allocator
// (AllocateMemoryResource). The PC RenderWare SDK exposes rw::IResourceAllocator only as an opaque
// vtbl (rwcore_structs.h: Alloc @ vtbl+8) with no clean wrapper and no existing reconstructed
// precedent, so - to keep the debug-pool bring-up buildable rather than block on a fragile,
// untestable manual vtbl call - the backing blocks are taken from the global heap here. This is real
// storage; threading the rw allocator through to a faithful Alloc call is the remaining refinement
// (the allocator pointer is already plumbed in to every Construct).

void* operator new[](size_t size, rw::IResourceAllocator* /*lpAllocator*/, CgsDev::Internal::AllocationType /*leAllocation*/)
{
    return ::operator new(size);
}

// Matching placement-delete (invoked only if the corresponding new[] expression throws; the pools
// free their blocks via the (deferred) Destruct path).
void operator delete[](void* lpMemory, rw::IResourceAllocator* /*lpAllocator*/, CgsDev::Internal::AllocationType /*leAllocation*/)
{
    ::operator delete(lpMemory);
}
