#pragma once

#include "rw/rwcore_structs.h"               // rw::Resource / ResourceDescriptor
#include "ppmalloc/EAGeneralAllocator.h"     // the two embedded EA GeneralAllocators

// rw::core::GeneralResourceAllocator - the general-purpose resource allocator (supports per-block
// Free, unlike the bump LinearResourceAllocator). The X360 (rw::core::GeneralResourceAllocator
// 0x82BC0AA8 / Initialize 0x82BC0E00 / GetResourceDescriptor 0x82BC0E28 / DoAllocate 0x82BC0C08)
// wraps TWO EA::Allocator::GeneralAllocators: a main heap over the resource's first region and an
// optional physical/graphics heap over a second region. [PC-LEAF, user-approved 2026-06-20]:
// faithful structure (two EA GeneralAllocators + a has-physical flag), bodies backed by the working
// PC GeneralAllocator engine; the bit-exact rwcore.lib chunk layout is elided (behaviourally
// invisible). Non-virtual concrete methods (the resource code holds the concrete type via
// BrnGameDataAllocatorList). Uses the rw::Resource/ResourceDescriptor (4-pool) typedefs for
// consistency with the AllocatorList + LinearResourceAllocator. [NOTE: the X360 ARTIST descriptor is
// 5-type with the physical capacity at index 4; reconcile the pool->heap routing when the consuming
// CreateAllocators bodies are filled in step 5.]
namespace rw
{
namespace core
{
    struct GeneralResourceAllocator
    {
        // Compute the heap descriptor required to back a usage descriptor (PC leaf: the
        // GeneralAllocator's own state lives in this object, not the managed heap, so the heap
        // requirement == the usage; the X360 added its ~2616-byte in-heap object overhead).
        static ResourceDescriptor* GetResourceDescriptor(ResourceDescriptor* lpOut, const ResourceDescriptor* lpIn);

        // Adopt the resource regions: main heap over [resource[0], +capacity[0]); physical heap over
        // [resource[2], +capacity[2]) if that capacity is non-zero.
        void  Initialize(const Resource& lrResource, const ResourceDescriptor& lrCapacity);
        // Allocate luSize bytes (luAlignment-aligned) for pool luType (graphics pools route to the
        // physical heap when present; everything else to the main heap).
        void* Alloc(uint32_t luType, uint32_t luSize, uint32_t luAlignment);
        void  Free(void* lpBlock);

        EA::Allocator::GeneralAllocator& GetPhysicalGeneralAllocator() { return m_physicalAllocator; }

        EA::Allocator::GeneralAllocator m_mainAllocator;      // X360 +4    main resource heap
        EA::Allocator::GeneralAllocator m_physicalAllocator;  // X360 +1308 physical/graphics heap
        bool                            m_hasPhysical;        // X360 +2612
    };
}
}
