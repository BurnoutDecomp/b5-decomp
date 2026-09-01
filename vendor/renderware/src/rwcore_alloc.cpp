#include "rw/rwcore_structs.h"
#include "rw/rwcore_general_alloc.h"
#include <cstring>
#include <malloc.h>
#if defined(D_PLATFORM_X360)
#include <xtl.h>
#endif

// rw::LinearResourceAllocator - the resource-memory linear (bump) allocator the game-data
// resource system carves module/bank memory through. [PC-LEAF, user-approved 2026-06-20]:
// faithful Paradise structure and bump semantics over the same fixed pools.
namespace rw
{
#if defined(D_PLATFORM_X360)
    namespace
    {
        uint32_t CountLeadingZeros(uint32_t luValue)
        {
            uint32_t luCount = 0;
            for (uint32_t luMask = 0x80000000u; luMask != 0 && (luValue & luMask) == 0;
                 luMask >>= 1)
                ++luCount;
            return luCount;
        }

        const uint8_t KAU_XMEM_ALIGNMENT_ATTRIBUTES[32] = {
            15, 15, 15, 15, 15, 15, 15, 15,
            15, 15, 15, 15, 15, 15, 15, 15,
            15, 14, 13, 12, 11, 10, 9, 8,
            7, 6, 5, 4, 3, 2, 2, 2
        };
    }
#else
    namespace
    {
        size_t GetHostAlignment(uint32_t luAlignment)
        {
            size_t luHostAlignment = luAlignment;
            if (luHostAlignment < sizeof(void*))
                luHostAlignment = sizeof(void*);
            return luHostAlignment;
        }
    }
#endif

    // ARTIST @0x8266AC50. The extended helper builds a lane-zero descriptor from
    // size/alignment, dispatches the allocator's DoAllocate slot, and returns lane zero.
    // The flags and resource-type arguments are present in the ABI but unused by this body.
    void* IResourceAllocator::Alloc(size_t luSize, const char* lpcName,
                                    uint32_t /*luFlags*/, uint32_t luAlignment,
                                    uint32_t /*luAlignmentOffset*/)
    {
        ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size = static_cast<uint32_t>(luSize);
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = luAlignment;
        return DoAllocate(lDescriptor, lpcName).m_baseResources[0];
    }

    // ARTIST @0x8266ABD8. The short helper is the same lane-zero carve with the
    // descriptor's identity alignment (one); its flags argument is unused.
    void* IResourceAllocator::Alloc(size_t luSize, const char* lpcName,
                                    uint32_t /*luFlags*/)
    {
        ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size = static_cast<uint32_t>(luSize);
        return DoAllocate(lDescriptor, lpcName).m_baseResources[0];
    }

    // Base default: a concrete allocator (the game's root allocator, or Linear/General) overrides this.
    Resource IResourceAllocator::DoAllocate(const ResourceDescriptor& /*lrDescriptor*/, const char* /*lpcName*/)
    {
        Resource lEmpty;
        for (uint32_t lu = 0; lu < KU_RESOURCE_LANE_COUNT; ++lu)
            lEmpty.m_baseResources[lu] = 0;
        return lEmpty;
    }

    // ARTIST @0x82666128. The convenience
    // Free(pointer) entry constructs a resource with the pointer in lane zero,
    // clears every other lane, and dispatches virtual DoFree. This dispatch is
    // essential: TestBed allocators inherit this function and override DoFree.
    void IResourceAllocator::Free(void* lpBlock, size_t /*luSizeOrFlags*/)
    {
        Resource lResource;
        lResource.m_baseResources[0] = lpBlock;
        for (uint32_t luLane = 1; luLane < KU_RESOURCE_LANE_COUNT; ++luLane)
            lResource.m_baseResources[luLane] = 0;
        DoFree(lResource);
    }

    // ARTIST @0x826645F8. Disposable lanes one and four are transferred into a
    // temporary Resource, cleared in the caller's Resource, then released through DoFree.
    void IResourceAllocator::DoFreeDisposable(Resource& lrResource)
    {
        Resource lDisposable;
        lDisposable.m_baseResources[1] = lrResource.m_baseResources[1];
        lDisposable.m_baseResources[4] = lrResource.m_baseResources[4];
        lrResource.m_baseResources[1] = nullptr;
        lrResource.m_baseResources[4] = nullptr;
        DoFree(lDisposable);
    }

    // ARTIST @0x82BBCA38 / 0x82BBCAE8. Every populated resource lane is
    // independently allocated and freed. Lane two uses XMem physical memory.
#if defined(D_PLATFORM_X360)
    Resource Xbox2NativeSystemAllocator::DoAllocate(
        const ResourceDescriptor& lrDescriptor, const char* /*lpcName*/)
    {
        Resource lResult;
        for (uint32_t luLane = 0; luLane < KU_RESOURCE_LANE_COUNT; ++luLane)
        {
            const BaseResourceDescriptor& lrLane =
                lrDescriptor.m_baseResourceDescriptors[luLane];
            if (lrLane.m_size != 0)
            {
                const uint32_t luAlignment = lrLane.m_alignment != 0 ? lrLane.m_alignment : 1u;
                if (luLane == 2)
                {
                    const uint32_t luAlignmentAttribute =
                        KAU_XMEM_ALIGNMENT_ATTRIBUTES[CountLeadingZeros(luAlignment)];
                    const uint32_t luAttributes =
                        0x80C00000u | ((luAlignmentAttribute << 24) & 0x7F000000u);
                    lResult.m_baseResources[luLane] = XMemAlloc(lrLane.m_size, luAttributes);
                }
                else
                lResult.m_baseResources[luLane] = _aligned_malloc(lrLane.m_size, luAlignment);
            }
        }
        return lResult;
    }

    void Xbox2NativeSystemAllocator::DoFree(const Resource& lrResource)
    {
        for (uint32_t luLane = 0; luLane < KU_RESOURCE_LANE_COUNT; ++luLane)
        {
            if (luLane == 2)
            {
                if (lrResource.m_baseResources[luLane])
                    XMemFree(lrResource.m_baseResources[luLane], 0x80C00002u);
            }
            else
                _aligned_free(lrResource.m_baseResources[luLane]);
        }
    }
#else
    // FLAG PC-platform leaf: the PC native allocator preserves Paradise's five-lane
    // interface and uses the host aligned-allocation service for each populated lane.
    Resource SystemAllocatorGeneric::DoAllocate(
        const ResourceDescriptor& lrDescriptor, const char* /*lpcName*/)
    {
        Resource lResult;
        for (uint32_t luLane = 0; luLane < KU_RESOURCE_LANE_COUNT; ++luLane)
        {
            const BaseResourceDescriptor& lrLane =
                lrDescriptor.m_baseResourceDescriptors[luLane];
            if (lrLane.m_size != 0)
            {
                lResult.m_baseResources[luLane] =
                    _aligned_malloc(lrLane.m_size, GetHostAlignment(lrLane.m_alignment));
            }
        }
        return lResult;
    }

    void SystemAllocatorGeneric::DoFree(const Resource& lrResource)
    {
        for (uint32_t luLane = 0; luLane < KU_RESOURCE_LANE_COUNT; ++luLane)
            _aligned_free(lrResource.m_baseResources[luLane]);
    }

    Resource ZeroingSystemAllocatorGeneric::DoAllocate(
        const ResourceDescriptor& lrDescriptor, const char* lpcName)
    {
        Resource lResult = SystemAllocatorGeneric::DoAllocate(lrDescriptor, lpcName);
        for (uint32_t luLane = 0; luLane < KU_RESOURCE_LANE_COUNT; ++luLane)
        {
            if (lResult.m_baseResources[luLane])
            {
                std::memset(lResult.m_baseResources[luLane], 0,
                            lrDescriptor.m_baseResourceDescriptors[luLane].m_size);
            }
        }
        return lResult;
    }
#endif

    // A linear allocator keeps its bookkeeping in its own object (m_currentUsage), not inside the
    // managed heap, so the heap descriptor it requires == the requested usage. (X360
    // LinearResourceAllocator::GetResourceDescriptor.)
    ResourceDescriptor* LinearResourceAllocator::GetResourceDescriptor(ResourceDescriptor* lpOut, const ResourceDescriptor* lpIn)
    {
        *lpOut = *lpIn;
        return lpOut;
    }

    // Adopt the five resource pools [lrResource[t], +lrCapacity[t].m_size) and reset usage.
    void LinearResourceAllocator::Initialize(const Resource& lrResource, const ResourceDescriptor& lrCapacity)
    {
        m_heapResource = lrResource;
        m_heapCapacity = lrCapacity;
        for (uint32_t lu = 0; lu < KU_RESOURCE_LANE_COUNT; ++lu)
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
        if (luType >= KU_RESOURCE_LANE_COUNT || m_heapResource.m_baseResources[luType] == 0)
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

    // The virtual DoAllocate slot satisfies each resource lane by bumping the
    // matching pool. A lane that cannot be satisfied yields a null base resource (the rw
    // convention the consumers' null-checks key on); zero-size lanes are null too.
    Resource LinearResourceAllocator::DoAllocate(const ResourceDescriptor& lrDescriptor, const char* /*lpcName*/)
    {
        Resource lResult;
        for (uint32_t luType = 0; luType < KU_RESOURCE_LANE_COUNT; ++luType)
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

    void LinearResourceAllocator::DoFree(const Resource& /*lrResource*/)
    {
        // ARTIST's linear allocator reclaims its pools only when Initialize resets
        // the bump cursors; individual Resources have no release operation.
    }

namespace core
{
    // EA::Allocator::GeneralAllocator's own in-heap bookkeeping overhead (X360 ctor/GetResourceDescriptor
    // both round up by this many bytes before carving the main heap's initial core out of the front of
    // the adopted region). Named from the literal 0xA38 in the ctor/GetResourceDescriptor asm.
    static const uint32_t KU_GENERAL_ALLOCATOR_OVERHEAD = 2616u;

    // Default-construct: the two EA GeneralAllocators construct normally and m_hasPhysical
    // starts false until Initialize()/the two-argument ctor adopts a region.
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
    // order) and restores the IResourceAllocator vtable -- both fall out of ordinary C++ teardown.
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
        // ARTIST routes lane two and subsequent graphics lanes to the physical heap when present.
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

    // The virtual carve (X360 GeneralResourceAllocator::DoAllocate @0x82BC0C08):
    // walk the descriptor's lanes and carve each
    // non-empty one from the matching heap. Lane two and later lanes route through
    // Alloc(), which applies the physical-heap routing. A lane the heap cannot serve
    // stays null in the result -- the same "empty base" the X360 hands back on exhaustion.
    ::rw::Resource GeneralResourceAllocator::DoAllocate(
        const ::rw::ResourceDescriptor& lrDescriptor, const char* /*lpcName*/)
    {
        ::rw::Resource lResult;
        for (uint32_t luPool = 0; luPool < KU_RESOURCE_LANE_COUNT; ++luPool)
        {
            const uint32_t luSize = lrDescriptor.m_baseResourceDescriptors[luPool].m_size;
            if (luSize == 0)
                continue;
            uint32_t luAlignment = lrDescriptor.m_baseResourceDescriptors[luPool].m_alignment;
            if (luAlignment < 1)
                luAlignment = 1;
            lResult.m_baseResources[luPool] = Alloc(luPool, luSize, luAlignment);
        }
        return lResult;
    }

    // ARTIST @ 0x82BC0DA8. Walk the serialised resource lanes in reverse;
    // lane two belongs to the physical allocator and every other lane belongs
    // to the main allocator.
    void GeneralResourceAllocator::DoFree(
        const ::rw::Resource& lrResource)
    {
        for (int liLane = static_cast<int>(KU_RESOURCE_LANE_COUNT) - 1; liLane >= 0; --liLane)
        {
            void* lpBlock = lrResource.m_baseResources[liLane];
            if (!lpBlock)
                continue;

            if (liLane == 2)
                m_physicalAllocator.Free(lpBlock);
            else
                m_mainAllocator.Free(lpBlock);
        }
    }
}
}
