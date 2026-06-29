#include "rw/rwcore_structs.h"
#include "rw/rwcore_general_alloc.h"

// rw::LinearResourceAllocator - the resource-memory linear (bump) allocator the game-data
// resource system carves module/bank memory through. [PC-LEAF, user-approved 2026-06-20]:
// faithful struct layout + bump semantics; the bit-exact rwcore.lib chunk engine is elided
// (behaviourally invisible - any correct allocator over the same fixed pools = identical
// gameplay). Non-virtual concrete methods (callers hold the concrete type via the AllocatorList).
namespace rw
{
    // Base default: a concrete allocator (the game's root allocator, or Linear/General) overrides this.
    Resource IResourceAllocator::DoAllocate(const ResourceDescriptor& /*lrDescriptor*/, const char* /*lpcName*/)
    {
        Resource lEmpty;
        for (uint32_t lu = 0; lu < 4; ++lu)
            lEmpty.m_baseResources[lu] = 0;
        return lEmpty;
    }

    // Base default (vtbl[3] +24, ?Free@IResourceAllocator@rw@@UEAAXPEAX_K@Z): the concrete
    // game allocator overrides this. The default is a no-op (matching the elided PC-LEAF
    // allocators, whose frees happen en masse via re-Initialize, not per-block).
    void IResourceAllocator::Free(void* /*lpBlock*/, uint64_t /*luSizeOrFlags*/)
    {
    }

    // A linear allocator keeps its bookkeeping in its own object (m_currentUsage), not inside the
    // managed heap, so the heap descriptor it requires == the requested usage. (X360
    // LinearResourceAllocator::GetResourceDescriptor.)
    ResourceDescriptor* LinearResourceAllocator::GetResourceDescriptor(ResourceDescriptor* lpOut, const ResourceDescriptor* lpIn)
    {
        *lpOut = *lpIn;
        return lpOut;
    }

    // Adopt the 4 resource pools [lrResource[t], +lrCapacity[t].m_size) and reset usage.
    void LinearResourceAllocator::Initialize(const Resource& lrResource, const ResourceDescriptor& lrCapacity)
    {
        m_heapResource = lrResource;
        m_heapCapacity = lrCapacity;
        for (uint32_t lu = 0; lu < 4; ++lu)
        {
            m_currentUsage.m_baseResourceDescriptors[lu].m_size      = 0;
            m_currentUsage.m_baseResourceDescriptors[lu].m_alignment = 1;
            m_paddingUsed.m_baseResourceDescriptors[lu].m_size       = 0;
            m_paddingUsed.m_baseResourceDescriptors[lu].m_alignment  = 1;
        }
        m_numAllocations = 0;
    }

    // Bump luSize bytes (luAlignment-aligned) out of pool luType; null if the pool is exhausted.
    void* LinearResourceAllocator::Alloc(uint32_t luType, uint32_t luSize, uint32_t luAlignment)
    {
        if (luType >= 4 || m_heapResource.m_baseResources[luType] == 0)
            return 0;
        if (luAlignment < 1)
            luAlignment = 1;

        BaseResourceDescriptor& lrUsage = m_currentUsage.m_baseResourceDescriptors[luType];
        const uint32_t luCapacity = m_heapCapacity.m_baseResourceDescriptors[luType].m_size;
        const uint32_t luOffset   = (lrUsage.m_size + luAlignment - 1) & ~(luAlignment - 1);

        m_paddingUsed.m_baseResourceDescriptors[luType].m_size += (luOffset - lrUsage.m_size);
        if (luOffset + luSize > luCapacity)
            return 0;   // pool exhausted

        void* lpBlock = static_cast<char*>(m_heapResource.m_baseResources[luType]) + luOffset;
        lrUsage.m_size = luOffset + luSize;
        ++m_numAllocations;
        return lpBlock;
    }

    // Linear allocator: individual frees are no-ops; the pools are reclaimed en masse by a
    // subsequent Initialize() (matches the X360 LinearResourceAllocator, which only tracks usage).
    void LinearResourceAllocator::Free(void* /*lpBlock*/)
    {
    }

namespace core
{
    // PC leaf: pass-through (the GeneralAllocator's bookkeeping lives in this object, not the heap).
    ResourceDescriptor* GeneralResourceAllocator::GetResourceDescriptor(ResourceDescriptor* lpOut, const ResourceDescriptor* lpIn)
    {
        *lpOut = *lpIn;
        return lpOut;
    }

    // Adopt the resource regions into the main (and optional physical) EA GeneralAllocator(s).
    void GeneralResourceAllocator::Initialize(const Resource& lrResource, const ResourceDescriptor& lrCapacity)
    {
        m_mainAllocator.Shutdown();
        m_mainAllocator.Init();
        if (lrResource.m_baseResources[0] && lrCapacity.m_baseResourceDescriptors[0].m_size)
            m_mainAllocator.AddCore(lrResource.m_baseResources[0], lrCapacity.m_baseResourceDescriptors[0].m_size);

        // optional physical/graphics heap over the third region (X360 uses resource[2]/capacity[4];
        // PC leaf uses capacity[2] of the 4-pool typedef -- reconcile in step 5).
        m_hasPhysical = (lrResource.m_baseResources[2] != 0 && lrCapacity.m_baseResourceDescriptors[2].m_size != 0);
        m_physicalAllocator.Shutdown();
        m_physicalAllocator.Init();
        if (m_hasPhysical)
            m_physicalAllocator.AddCore(lrResource.m_baseResources[2], lrCapacity.m_baseResourceDescriptors[2].m_size);
    }

    void* GeneralResourceAllocator::Alloc(uint32_t luType, uint32_t luSize, uint32_t luAlignment)
    {
        // graphics pools (>=2 in the 4-pool model) route to the physical heap when present.
        if (m_hasPhysical && luType >= 2)
            return m_physicalAllocator.MallocAligned(luSize, luAlignment);
        return m_mainAllocator.MallocAligned(luSize, luAlignment);
    }

    void GeneralResourceAllocator::Free(void* lpBlock)
    {
        if (!lpBlock)
            return;
        if (m_hasPhysical && m_physicalAllocator.Owns(lpBlock))
            m_physicalAllocator.Free(lpBlock);
        else
            m_mainAllocator.Free(lpBlock);
    }
}
}
