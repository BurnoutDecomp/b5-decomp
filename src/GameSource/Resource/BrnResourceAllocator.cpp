#include "GameSource/Resource/BrnResourceAllocator.h"
#include "ppmalloc/EAGeneralAllocator.h"   // the backing EA general allocator
#include <cstdlib>                          // malloc

// BrnResource::Allocators / HeapResourceAllocator - the engine's root debug resource allocator,
// the heap the whole GameData resource system (ConstructResourceModule -> ResourceModule::Construct)
// carves its module/bank memory from, and the one ICE::ICEMemory::Construct allocates its debug pool
// from.
//
// [PC-LEAF, consistent with the allocator-layer decision]: on X360 this is
// Allocators::mGlobalDebugAllocator - an rw allocator over a memory region the boot path reserves;
// here it is a big malloc-backed region with an EA::Allocator::GeneralAllocator over it. The backing
// block is allocated lazily on first use; mpInternalDebugAllocator is published non-null at static
// init (it points at the allocator object) so the ICE null-check passes regardless of init order.
namespace BrnResource
{
    namespace
    {
        // 768 MB root debug heap. [PC-LEAF] the X360 reserves a fixed ~smaller budget split across banks;
        // the PC bring-up routes the 27 pools' memory (~266 MB) through the MemoryModule's root banks (a
        // ~384 MB type0+type1 backing carved from here), with a debug-heap FALLBACK if a bank carve fails
        // (another ~266 MB) -- so this is sized to hold the bank backing + a full fallback at once.
        const size_t KN_DEBUG_HEAP_SIZE = 768u * 1024u * 1024u;

        EA::Allocator::GeneralAllocator s_DebugGeneralAllocator;   // ctor -> Init (empty) at static init
        HeapResourceAllocator           s_DebugResourceAllocator;
        bool                            s_bDebugHeapBacked = false;

        // Lazily malloc + adopt the backing block (avoids a 64 MB allocation at static init).
        void EnsureDebugHeapBacked()
        {
            if (s_bDebugHeapBacked)
                return;
            void* lpBlock = malloc(KN_DEBUG_HEAP_SIZE);
            if (lpBlock)
                s_DebugGeneralAllocator.AddCore(lpBlock, KN_DEBUG_HEAP_SIZE);
            s_bDebugHeapBacked = true;
        }
    }

    // Published non-null at static init; points at the root HeapResourceAllocator (which IS-A
    // rw::IResourceAllocator) so the resource-module Construct chain can carve memory through its
    // virtual DoAllocate. The backing 64 MB block is adopted lazily on first Allocate. (ICE null-checks
    // this; the resource modules cast it to rw::IResourceAllocator* and call DoAllocate.)
    void* Allocators::mpInternalDebugAllocator = static_cast<rw::IResourceAllocator*>(&s_DebugResourceAllocator);

    HeapResourceAllocator* GetDebugAllocator()
    {
        EnsureDebugHeapBacked();
        return &s_DebugResourceAllocator;
    }

    // Out-of-line defaulted destructor: anchors the DefaultLinearAllocator vtable in this TU so
    // the compiler emits its polymorphic-delete thunk (the X360 `scalar deleting destructor'
    // @0x82666948). The underlying object teardown is trivial (only the rw::IResourceAllocator
    // base) -- the X360 thunk just rewrites the vptr and conditionally operator-deletes.
    DefaultLinearAllocator::~DefaultLinearAllocator() = default;

    // rw::IResourceAllocator virtual entry point -> forward to Allocate.
    rw::Resource HeapResourceAllocator::DoAllocate(const rw::ResourceDescriptor& lDescriptor, const char* lpcName)
    {
        return Allocate(lDescriptor, lpcName);
    }

    // Carve a resource matching lDescriptor from the root debug heap. The returned rw::Resource's
    // m_baseResources[0] is the memory block (the X360 DoAllocate fills the per-pool base pointers;
    // PC leaf serves pool 0 from the general heap).
    rw::Resource HeapResourceAllocator::Allocate(const rw::ResourceDescriptor& lDescriptor, const char* /*lpcName*/)
    {
        EnsureDebugHeapBacked();
        rw::Resource lResource;
        for (uint32_t lu = 0; lu < 4; ++lu)
            lResource.m_baseResources[lu] = 0;

        const uint32_t luSize  = lDescriptor.m_baseResourceDescriptors[0].m_size;
        uint32_t       luAlign = lDescriptor.m_baseResourceDescriptors[0].m_alignment;
        if (luAlign < 16)
            luAlign = 16;
        if (luSize)
            lResource.m_baseResources[0] = s_DebugGeneralAllocator.MallocAligned(luSize, luAlign);
        return lResource;
    }
}
