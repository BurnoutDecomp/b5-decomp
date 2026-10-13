#include "rw/rwcore_structs.h"
#include "rw/rwcore_general_alloc.h"
#include <cstring>
#include <malloc.h>
#include <new>
#if defined(D_PLATFORM_X360)
#include <xtl.h>
#endif

// rw::LinearResourceAllocator - the resource-memory linear (bump) allocator the game-data
// resource system carves module/bank memory through. [PC-LEAF, user-approved 2026-06-20]:
// faithful Paradise structure and bump semantics over the same fixed pools.
namespace rw
{
    namespace
    {
        template <class Allocator>
        uint32_t GetAllocatorStorageAlignment(uint32_t luAlignment)
        {
            return luAlignment < alignof(Allocator)
                ? static_cast<uint32_t>(alignof(Allocator)) : luAlignment;
        }

        template <class Allocator>
        uint32_t GetAllocatorStorageSize(uint32_t luAlignment)
        {
            luAlignment = GetAllocatorStorageAlignment<Allocator>(luAlignment);
            return (static_cast<uint32_t>(sizeof(Allocator)) + luAlignment - 1u)
                & ~(luAlignment - 1u);
        }
    }
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

    // ARTIST 0x82BBCD90. The backing includes the placement object before lane-zero
    // payload. Only its native sizeof/alignment differs from the console's 148 bytes.
    ResourceDescriptor* LinearResourceAllocator::GetResourceDescriptor(ResourceDescriptor* lpOut, const ResourceDescriptor* lpIn)
    {
        *lpOut = *lpIn;
        ResourceDescriptor lStorage;
        const uint32_t luAlignment = lpIn->m_baseResourceDescriptors[0].m_alignment;
        lStorage.m_baseResourceDescriptors[0].m_size =
            GetAllocatorStorageSize<LinearResourceAllocator>(luAlignment);
        lStorage.m_baseResourceDescriptors[0].m_alignment =
            GetAllocatorStorageAlignment<LinearResourceAllocator>(luAlignment);
        *lpOut += lStorage;
        return lpOut;
    }

    // ARTIST 0x82BBCE30 adjusts only lane zero; r5 retains the original capacity.
    LinearResourceAllocator* LinearResourceAllocator::Initialize(
        const Resource& lrResource, const ResourceDescriptor& lrCapacity)
    {
        void* const lpStorage = lrResource.m_baseResources[0];
        if (lpStorage == nullptr)
            return nullptr;
        Resource lHeapResource = lrResource;
        lHeapResource.m_baseResources[0] = static_cast<unsigned char*>(lpStorage)
            + GetAllocatorStorageSize<LinearResourceAllocator>(
                lrCapacity.m_baseResourceDescriptors[0].m_alignment);
        return new (lpStorage) LinearResourceAllocator(lHeapResource, lrCapacity);
    }

    // ARTIST 0x82BBCC70 receives already-adjusted heap bases.
    LinearResourceAllocator::LinearResourceAllocator(
        const Resource& lrResource, const ResourceDescriptor& lrCapacity)
    {
        AdoptMemory(lrResource, lrCapacity);
    }

    // FLAG PC-platform leaf: native out-of-region objects adopt the exact pool
    // extents. Reserving a second allocator object here would reduce those extents.
    void LinearResourceAllocator::AdoptMemory(const Resource& lrResource, const ResourceDescriptor& lrCapacity)
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

    // FLAG PC-platform leaf: native lane convenience API, absent from the ARTIST
    // symbol family. Delegate to the same resource transaction as the game wrapper.
    void* LinearResourceAllocator::Alloc(uint32_t luType, uint32_t luSize, uint32_t luAlignment)
    {
        if (luType >= KU_RESOURCE_LANE_COUNT)
            return nullptr;
        ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[luType].m_size = luSize;
        lDescriptor.m_baseResourceDescriptors[luType].m_alignment = luAlignment;
        return DoAllocate(lDescriptor, nullptr).m_baseResources[luType];
    }

    // ARTIST 0x82BBCF50, with five-lane padding differences from 0x82B63FF8 and
    // descriptor accumulation from 0x8268D9F8. Align absolute cursors, validate the
    // complete proposed usage, then commit once. Failure changes no allocator state.
    Resource LinearResourceAllocator::DoAllocate(const ResourceDescriptor& lrDescriptor, const char* /*lpcName*/)
    {
        Resource lResult;
        ResourceDescriptor lPadding;
        for (uint32_t luLane = 0; luLane < KU_RESOURCE_LANE_COUNT; ++luLane)
        {
            const uintptr_t luCursor = reinterpret_cast<uintptr_t>(m_heapResource.m_baseResources[luLane])
                + m_currentUsage.m_baseResourceDescriptors[luLane].m_size;
            const uint32_t luAlignment = lrDescriptor.m_baseResourceDescriptors[luLane].m_alignment;
            uintptr_t luAlignedCursor = luCursor;
            if (luAlignment > 1)
            {
                const uintptr_t luMask = luAlignment - 1u;
                luAlignedCursor = (luCursor + luMask) & ~luMask;
            }
            lResult.m_baseResources[luLane] = reinterpret_cast<void*>(luAlignedCursor);
            lPadding.m_baseResourceDescriptors[luLane].m_size =
                static_cast<uint32_t>(luAlignedCursor - luCursor);
        }

        ResourceDescriptor lProposedUsage = m_currentUsage;
        lProposedUsage += lPadding;
        lProposedUsage += lrDescriptor;
        for (uint32_t luLane = 0; luLane < KU_RESOURCE_LANE_COUNT; ++luLane)
            if (lProposedUsage.m_baseResourceDescriptors[luLane].m_size
                > m_heapCapacity.m_baseResourceDescriptors[luLane].m_size)
                return Resource();

        for (uint32_t luLane = 0; luLane < KU_RESOURCE_LANE_COUNT; ++luLane)
            if (lrDescriptor.m_baseResourceDescriptors[luLane].m_size == 0)
                lResult.m_baseResources[luLane] = nullptr;
        m_currentUsage += lPadding;
        m_currentUsage += lrDescriptor;
        for (uint32_t luLane = 0; luLane < KU_RESOURCE_LANE_COUNT; ++luLane)
            m_currentUsage.m_baseResourceDescriptors[luLane].m_alignment = 1;
        m_paddingUsed += lPadding;
        ++m_numAllocations;
        return lResult;
    }

    // Linear allocator: individual frees are no-ops; the pools are reclaimed en masse by a
    // subsequent pool adoption (the X360 allocator also has no individual free).
    void LinearResourceAllocator::Free(void* /*lpBlock*/)
    {
    }

    void LinearResourceAllocator::DoFree(const Resource& /*lrResource*/)
    {
        // Individual Resources have no release operation on this linear allocator.
    }

    // The adopted heap: the base of each of the five pools.
    const Resource& LinearResourceAllocator::GetLinearHeapBase() const
    {
        return m_heapResource;
    }

    // The per-pool sizes the allocator was initialised over (returned by value).
    ResourceDescriptor LinearResourceAllocator::GetCapacity() const
    {
        return m_heapCapacity;
    }

    // The per-pool bump cursors: bytes carved so far, padding included.
    ResourceDescriptor LinearResourceAllocator::GetCurrentUsage() const
    {
        return m_currentUsage;
    }

namespace core
{
    // Default-construct: the two EA GeneralAllocators construct normally and m_hasPhysical
    // starts false until adoption/the two-argument ctor receives a region.
    GeneralResourceAllocator::GeneralResourceAllocator()
        : m_hasPhysical(false)
    {
    }

    // ARTIST 0x82BC0AA8. The backing's lane zero contains both object storage and
    // the full advertised main capacity. The host EA members widen this reserve.
    GeneralResourceAllocator::GeneralResourceAllocator(const Resource& lrResource, const ResourceDescriptor& lrCapacity)
        : m_hasPhysical(false)
    {
        Resource lHeapResource = lrResource;
        if (lHeapResource.m_baseResources[0] != nullptr)
            lHeapResource.m_baseResources[0] =
                static_cast<unsigned char*>(lHeapResource.m_baseResources[0])
                + GetAllocatorStorageSize<GeneralResourceAllocator>(
                    lrCapacity.m_baseResourceDescriptors[0].m_alignment);
        AdoptMemory(lHeapResource, lrCapacity);
    }

    // X360 dtor @0x82BC0A50: destroys m_physicalAllocator then m_mainAllocator (reverse declaration
    // order) and restores the IResourceAllocator vtable -- both fall out of ordinary C++ teardown.
    GeneralResourceAllocator::~GeneralResourceAllocator()
    {
    }

    // ARTIST 0x82BC0E28. Add the aligned allocator-object reserve to lane zero.
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
        lOverhead.m_baseResourceDescriptors[0].m_size =
            GetAllocatorStorageSize<GeneralResourceAllocator>(luAlignment0);
        lOverhead.m_baseResourceDescriptors[0].m_alignment =
            GetAllocatorStorageAlignment<GeneralResourceAllocator>(luAlignment0);

        *lpOut += lOverhead;
        return lpOut;
    }

    // ARTIST 0x82BC0E00 has no receiver: resource lane zero is the placement address.
    GeneralResourceAllocator* GeneralResourceAllocator::Initialize(
        const Resource& lrResource, const ResourceDescriptor& lrCapacity)
    {
        void* const lpStorage = lrResource.m_baseResources[0];
        return lpStorage != nullptr
            ? new (lpStorage) GeneralResourceAllocator(lrResource, lrCapacity) : nullptr;
    }

    // FLAG PC-platform leaf: existing host objects adopt exactly the supplied
    // capacities. Their object storage lives elsewhere, so no console header is
    // skipped. In particular an exact 64 MiB backing stays within that 64 MiB.
    void GeneralResourceAllocator::AdoptMemory(const Resource& lrResource, const ResourceDescriptor& lrCapacity)
    {
        m_mainAllocator.SetOption(EA::Allocator::GeneralAllocator::kOptionEnableSystemAlloc, 0);
        m_mainAllocator.Init(lrResource.m_baseResources[0], lrCapacity.m_baseResourceDescriptors[0].m_size,
                             /*bShouldFreeInitialCore=*/false, /*bShouldTrace=*/false, 0, 0);

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
        if (luType == 2)
            return m_hasPhysical ? m_physicalAllocator.MallocAligned(luSize, luAlignment) : nullptr;
        const int liAllocationFlags = (luType == 1 || luType == 4) ? 1 : 0;
        return m_mainAllocator.MallocAligned(luSize, luAlignment, 0, liAllocationFlags);
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
    // Allocate main lanes first, then lane two. Failure in either phase rolls back
    // all partial allocations through DoFree and returns an empty resource.
    ::rw::Resource GeneralResourceAllocator::DoAllocate(
        const ::rw::ResourceDescriptor& lrDescriptor, const char* /*lpcName*/)
    {
        ::rw::Resource lResult;
        bool lbFailed = false;
        for (uint32_t luPool = 0; luPool < KU_RESOURCE_LANE_COUNT; ++luPool)
        {
            if (luPool == 2)
                continue;
            const uint32_t luSize = lrDescriptor.m_baseResourceDescriptors[luPool].m_size;
            if (luSize == 0)
                continue;
            uint32_t luAlignment = lrDescriptor.m_baseResourceDescriptors[luPool].m_alignment;
            if (luAlignment < 1)
                luAlignment = 1;
            lResult.m_baseResources[luPool] = Alloc(luPool, luSize, luAlignment);
            if (lResult.m_baseResources[luPool] == nullptr)
            {
                lbFailed = true;
                break;
            }
        }
        const BaseResourceDescriptor& lrPhysical = lrDescriptor.m_baseResourceDescriptors[2];
        if (lrPhysical.m_size != 0)
        {
            lResult.m_baseResources[2] = Alloc(2, lrPhysical.m_size, lrPhysical.m_alignment);
            if (lResult.m_baseResources[2] == nullptr)
                lbFailed = true;
        }
        if (lbFailed)
        {
            DoFree(lResult);
            return Resource();
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
