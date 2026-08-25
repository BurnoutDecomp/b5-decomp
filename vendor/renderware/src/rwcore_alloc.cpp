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
    // FLAG PC-platform leaf: base-interface default of the [PC-LEAF, user-approved 2026-06-20]
    // allocator reconstruction (see the file header) -- the rwcore.lib chunk engine is elided.
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

    // The virtual DoAllocate slot (rwcore vtable [6]; the override the PDB vtable dump in
    // rwcore_structs.h proves): satisfy each per-type lane of the descriptor by bumping the
    // matching pool. A lane that cannot be satisfied yields a null base resource (the rw
    // convention the consumers' null-checks key on); zero-size lanes are null too.
    Resource LinearResourceAllocator::DoAllocate(const ResourceDescriptor& lrDescriptor, const char* /*lpcName*/)
    {
        Resource lResult;
        for (uint32_t luType = 0; luType < 4; ++luType)
        {
            const BaseResourceDescriptor& lrLane = lrDescriptor.m_baseResourceDescriptors[luType];
            lResult.m_baseResources[luType] =
                (lrLane.m_size != 0) ? Alloc(luType, lrLane.m_size, lrLane.m_alignment) : 0;
        }
        return lResult;
    }

    // Linear allocator: individual frees are no-ops; the pools are reclaimed en masse by a
    // subsequent Initialize() (matches the X360 LinearResourceAllocator, which only tracks usage).
    void LinearResourceAllocator::Free(void* /*lpBlock*/)
    {
    }

namespace core
{
    // EA::Allocator::GeneralAllocator's own in-heap bookkeeping overhead (X360 ctor/GetResourceDescriptor
    // both round up by this many bytes before carving the main heap's initial core out of the front of
    // the adopted region). Named from the literal 0xA38 in the ctor/GetResourceDescriptor asm.
    static const uint32_t KU_GENERAL_ALLOCATOR_OVERHEAD = 2616u;

    // Default-construct: the two EA GeneralAllocators + field_0x0 all default-construct themselves
    // (member init list); m_hasPhysical starts false until Initialize()/the 2-arg ctor adopts a region.
    GeneralResourceAllocator::GeneralResourceAllocator()
        : m_hasPhysical(false)
    {
    }

    // X360 ctor @0x82BC0AA8: adopt the resource regions immediately (same math as Initialize() -- the
    // X360 in fact tail-calls this ctor from both the ctor path and the out-of-line Initialize wrapper).
    GeneralResourceAllocator::GeneralResourceAllocator(const Resource& lrResource, const ResourceDescriptor& lrCapacity)
        : m_hasPhysical(false)
    {
        Initialize(lrResource, lrCapacity);
    }

    // X360 dtor @0x82BC0A50: destroys m_physicalAllocator then m_mainAllocator (reverse declaration
    // order) and restores the IResourceAllocator vtable -- both fall out of ordinary member teardown
    // now that field_0x0 / m_mainAllocator / m_physicalAllocator are real members; nothing to hand-code.
    GeneralResourceAllocator::~GeneralResourceAllocator()
    {
    }

    // GetResourceDescriptor @0x82BC0E28: copy the input 5-entry descriptor through, then accumulate a
    // synthetic requirement of { size = round_up(m_alignment[0], KU_GENERAL_ALLOCATOR_OVERHEAD),
    // alignment = m_alignment[0] } into entry 0 via the committed operator+= (entries 1..4 of the
    // synthetic operand are the additive identity, so they pass through unchanged).
    ::rw::BaseResourceDescriptors<5>* GeneralResourceAllocator::GetResourceDescriptor(::rw::BaseResourceDescriptors<5>* lpOut, const ::rw::BaseResourceDescriptors<5>* lpIn)
    {
        *lpOut = *lpIn;

        ::rw::BaseResourceDescriptors<5> lOverhead;
        for (uint32_t lu = 0; lu < 5; ++lu)
        {
            lOverhead.m_baseResourceDescriptors[lu].m_size      = 0;
            lOverhead.m_baseResourceDescriptors[lu].m_alignment = 1;
        }
        const uint32_t luAlignment0 = lpIn->m_baseResourceDescriptors[0].m_alignment;
        lOverhead.m_baseResourceDescriptors[0].m_size      = (luAlignment0 - 1 + KU_GENERAL_ALLOCATOR_OVERHEAD) & ~(luAlignment0 - 1);
        lOverhead.m_baseResourceDescriptors[0].m_alignment = luAlignment0;

        *lpOut += lOverhead;
        return lpOut;
    }

    // Adopt the resource regions into the main (and optional physical) EA GeneralAllocator(s). X360
    // ctor asm @0x82BC0AA8: SetOption(kOptionEnableSystemAlloc, 0) then Init(pInitialCore, nSize,
    // bShouldFreeInitialCore=false, bShouldTrace=false) on the main allocator, where pInitialCore is the
    // pool-0 base offset by the KU_GENERAL_ALLOCATOR_OVERHEAD padding (rounded up to the pool's
    // alignment) and nSize is the pool-0 capacity UNREDUCED by that padding (matches the asm literally:
    // r5 = a3[0] with no subtraction). The physical heap (pool index 2) is adopted the same way only if
    // its capacity is non-zero; otherwise it is Shutdown() and m_hasPhysical stays false.
    void GeneralResourceAllocator::Initialize(const Resource& lrResource, const ResourceDescriptor& lrCapacity)
    {
        m_mainAllocator.SetOption(EA::Allocator::GeneralAllocator::kOptionEnableSystemAlloc, 0);
        {
            const uint32_t luAlignment0 = lrCapacity.m_baseResourceDescriptors[0].m_alignment;
            const uint32_t luPadding0   = (luAlignment0 - 1 + KU_GENERAL_ALLOCATOR_OVERHEAD) & ~(luAlignment0 - 1);
            void* lpCore0 = static_cast<char*>(lrResource.m_baseResources[0]) + luPadding0;
            m_mainAllocator.Init(lpCore0, lrCapacity.m_baseResourceDescriptors[0].m_size,
                                  /*bShouldFreeInitialCore=*/false, /*bShouldTrace=*/false, 0, 0);
        }

        m_hasPhysical = (lrCapacity.m_baseResourceDescriptors[2].m_size != 0);
        if (m_hasPhysical)
        {
            m_physicalAllocator.SetOption(EA::Allocator::GeneralAllocator::kOptionEnableSystemAlloc, 0);
            m_physicalAllocator.Init(lrResource.m_baseResources[2], lrCapacity.m_baseResourceDescriptors[2].m_size,
                                      /*bShouldFreeInitialCore=*/false, /*bShouldTrace=*/false, 0, 0);
        }
        else
        {
            m_physicalAllocator.Shutdown();
        }
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

    // The virtual carve (X360 GeneralResourceAllocator::DoAllocate @0x82BC0C08, reached
    // through the +0 interface subobject): walk the descriptor's pools and carve each
    // non-empty one from the matching heap. The X360 walks the serialised FIVE-entry form
    // (index 2 through the physical heap); the PC's narrowed 4-pool walk routes through
    // Alloc(), which applies the same physical-heap routing. A pool the heap cannot serve
    // stays null in the result -- the same "empty base" the X360 hands back on exhaustion.
    ::rw::Resource GeneralResourceAllocator::ResourceAllocatorBridge::DoAllocate(
        const ::rw::ResourceDescriptor& lrDescriptor, const char* /*lpcName*/)
    {
        // The bridge sits at offset 0 of the owning GeneralResourceAllocator (X360: the class
        // IS the interface); recover the owner from `this`.
        GeneralResourceAllocator* lpOwner = reinterpret_cast<GeneralResourceAllocator*>(this);

        ::rw::Resource lResult;
        for (uint32_t luPool = 0; luPool < 4; ++luPool)
            lResult.m_baseResources[luPool] = 0;

        for (uint32_t luPool = 0; luPool < 4; ++luPool)
        {
            const uint32_t luSize = lrDescriptor.m_baseResourceDescriptors[luPool].m_size;
            if (luSize == 0)
                continue;
            uint32_t luAlignment = lrDescriptor.m_baseResourceDescriptors[luPool].m_alignment;
            if (luAlignment < 1)
                luAlignment = 1;
            lResult.m_baseResources[luPool] = lpOwner->Alloc(luPool, luSize, luAlignment);
        }
        return lResult;
    }
}
}
