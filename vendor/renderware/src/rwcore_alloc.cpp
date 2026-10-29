#include "rw/rwcore_structs.h"
#include "rw/rwcore_general_alloc.h"
#include "rw/core/memory/memoryfunctions.h"
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <new>
#include <xtl.h>

// ARTIST 0x82F91A10: one global object, initially pointing at the lazy CRT entries.
RwMemoryFunctions rwMemoryFunctions = {
    RwMallocMemStub, RwReallocMemStub, RwFreeMemStub
};

RwMallocAlignedFunc RwMallocMemAligned = RwMallocMemAlignedStub;
RwReallocAlignedFunc RwReallocMemAligned = RwReallocMemAlignedStub;
RwFreeAlignedFunc RwFreeMemAligned = RwFreeMemAlignedStub;

// ARTIST82BBC680/6B8/6F0: each entry lazily installs all three genuine
// native CRT functions, then forwards its original arguments unchanged.
void* RwMallocMemAlignedStub(std::size_t lnSize, std::size_t lnAlignment)
{
    RwMallocMemAligned = _aligned_malloc;
    RwReallocMemAligned = _aligned_realloc;
    RwFreeMemAligned = _aligned_free;
    return RwMallocMemAligned(lnSize, lnAlignment);
}

void* RwReallocMemAlignedStub(void* lpMemory, std::size_t lnSize, std::size_t lnAlignment)
{
    RwMallocMemAligned = _aligned_malloc;
    RwReallocMemAligned = _aligned_realloc;
    RwFreeMemAligned = _aligned_free;
    return RwReallocMemAligned(lpMemory, lnSize, lnAlignment);
}

void RwFreeMemAlignedStub(void* lpMemory)
{
    RwMallocMemAligned = _aligned_malloc;
    RwReallocMemAligned = _aligned_realloc;
    RwFreeMemAligned = _aligned_free;
    RwFreeMemAligned(lpMemory);
}

// ARTIST 0x82BBC728. Every lazy entry installs the complete three-slot CRT table.
void* RwMallocMemStub(std::size_t lnSize)
{
    rwMemoryFunctions.rwmalloc = std::malloc;
    rwMemoryFunctions.rwrealloc = std::realloc;
    rwMemoryFunctions.rwfree = std::free;
    return rwMemoryFunctions.rwmalloc(lnSize);
}

// ARTIST 0x82BBC760. The pointer and size are both preserved for realloc.
void* RwReallocMemStub(void* lpMemory, std::size_t lnSize)
{
    rwMemoryFunctions.rwmalloc = std::malloc;
    rwMemoryFunctions.rwrealloc = std::realloc;
    rwMemoryFunctions.rwfree = std::free;
    return rwMemoryFunctions.rwrealloc(lpMemory, lnSize);
}

// ARTIST 0x82BBC798. Release uses the unaligned CRT free operation.
void RwFreeMemStub(void* lpMemory)
{
    rwMemoryFunctions.rwmalloc = std::malloc;
    rwMemoryFunctions.rwrealloc = std::realloc;
    rwMemoryFunctions.rwfree = std::free;
    std::free(lpMemory);
}

// rw::LinearResourceAllocator - the resource-memory linear (bump) allocator the game-data
// faithful Paradise structure and bump semantics over the same fixed pools.
namespace rw
{
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
        lrResource.m_baseResources[1] = 0;
        lrResource.m_baseResources[4] = 0;
        DoFree(lDisposable);
    }

    // ARTIST @0x82BBCA38 / 0x82BBCAE8. Every populated resource lane is
    // independently allocated and freed. Lane two uses XMem physical memory.
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

    // ARTIST 0x82BBCD90 reserves the 0x94-byte placement object in lane zero.
    // Alignment zero/one preserves that size; other alignments round it upward.
    ResourceDescriptor* LinearResourceAllocator::GetResourceDescriptor(ResourceDescriptor* lpOut, const ResourceDescriptor* lpIn)
    {
        *lpOut = *lpIn;
        ResourceDescriptor lStorage;
        const uint32_t luAlignment = lpIn->m_baseResourceDescriptors[0].m_alignment;
        lStorage.m_baseResourceDescriptors[0].m_size = luAlignment > 1
            ? (sizeof(LinearResourceAllocator) + luAlignment - 1u) & ~(luAlignment - 1u)
            : sizeof(LinearResourceAllocator);
        lStorage.m_baseResourceDescriptors[0].m_alignment = luAlignment;
        *lpOut += lStorage;
        return lpOut;
    }

    // ARTIST 0x82BBCE30 adjusts only lane zero; r5 retains the original capacity.
    LinearResourceAllocator* LinearResourceAllocator::Initialize(
        const Resource& lrResource, const ResourceDescriptor& lrCapacity)
    {
        void* const lpStorage = lrResource.m_baseResources[0];
        if (lpStorage == 0)
            return 0;
        Resource lHeapResource = lrResource;
        const uint32_t luAlignment = lrCapacity.m_baseResourceDescriptors[0].m_alignment;
        const uint32_t luStorageSize = luAlignment > 1
            ? (sizeof(LinearResourceAllocator) + luAlignment - 1u) & ~(luAlignment - 1u)
            : sizeof(LinearResourceAllocator);
        lHeapResource.m_baseResources[0] = reinterpret_cast<void*>(
            reinterpret_cast<uintptr_t>(lpStorage) + luStorageSize);
        return new (lpStorage) LinearResourceAllocator(lHeapResource, lrCapacity);
    }

    // ARTIST 0x82BBCC70 receives already-adjusted heap bases.
    LinearResourceAllocator::LinearResourceAllocator(
        const Resource& lrResource, const ResourceDescriptor& lrCapacity)
        : m_heapResource(lrResource), m_heapCapacity(lrCapacity), m_numAllocations(0)
    {
        // Both descriptor members default to five identity entries {0, 1}.
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
                lResult.m_baseResources[luLane] = 0;
        m_currentUsage += lPadding;
        m_currentUsage += lrDescriptor;
        for (uint32_t luLane = 0; luLane < KU_RESOURCE_LANE_COUNT; ++luLane)
            m_currentUsage.m_baseResourceDescriptors[luLane].m_alignment = 1;
        m_paddingUsed += lPadding;
        ++m_numAllocations;
        return lResult;
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
    // ARTIST 0x82BC0AA8. The two heaps are constructed before main memory is
    // adopted. The physical heap is shut down when the capacity has no lane two.
    GeneralResourceAllocator::GeneralResourceAllocator(
        const Resource& lrResource, const ResourceDescriptor& lrCapacity)
        : m_mainAllocator(0, 0, true, false, 0, 0),
          m_physicalAllocator(0, 0, true, false, 0, 0)
    {
        m_mainAllocator.SetOption(EA::Allocator::GeneralAllocator::kOptionEnableSystemAlloc, 0);
        const uint32_t luAlignmentMask = lrCapacity.m_baseResourceDescriptors[0].m_alignment - 1u;
        const uint32_t luStorageSize = (sizeof(GeneralResourceAllocator) + luAlignmentMask)
            & ~luAlignmentMask;
        m_mainAllocator.Init(reinterpret_cast<void*>(
            reinterpret_cast<uintptr_t>(lrResource.m_baseResources[0]) + luStorageSize),
            lrCapacity.m_baseResourceDescriptors[0].m_size, false, false, 0, 0);
        if (lrCapacity.m_baseResourceDescriptors[2].m_size != 0)
        {
            m_physicalAllocator.SetOption(EA::Allocator::GeneralAllocator::kOptionEnableSystemAlloc, 0);
            m_physicalAllocator.Init(lrResource.m_baseResources[2],
                lrCapacity.m_baseResourceDescriptors[2].m_size, false, false, 0, 0);
            m_hasPhysical = true;
        }
        else
        {
            m_physicalAllocator.Shutdown();
            m_hasPhysical = false;
        }
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
            (sizeof(GeneralResourceAllocator) + luAlignment0 - 1u) & ~(luAlignment0 - 1u);
        lOverhead.m_baseResourceDescriptors[0].m_alignment = luAlignment0;

        *lpOut += lOverhead;
        return lpOut;
    }

    // ARTIST 0x82BC0E00 has no receiver: resource lane zero is the placement address.
    GeneralResourceAllocator* GeneralResourceAllocator::Initialize(
        const Resource& lrResource, const ResourceDescriptor& lrCapacity)
    {
        void* const lpStorage = lrResource.m_baseResources[0];
        return lpStorage != 0
            ? new (lpStorage) GeneralResourceAllocator(lrResource, lrCapacity) : 0;
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
            const uint32_t luAlignment = lrDescriptor.m_baseResourceDescriptors[luPool].m_alignment;
            const int liAllocationFlags = (luPool == 1 || luPool == 4) ? 1 : 0;
            lResult.m_baseResources[luPool] =
                m_mainAllocator.MallocAligned(luSize, luAlignment, 0, liAllocationFlags);
            if (lResult.m_baseResources[luPool] == 0)
            {
                lbFailed = true;
                break;
            }
        }
        const BaseResourceDescriptor& lrPhysical = lrDescriptor.m_baseResourceDescriptors[2];
        if (lrPhysical.m_size != 0)
        {
            lResult.m_baseResources[2] = m_hasPhysical
                ? m_physicalAllocator.MallocAligned(lrPhysical.m_size, lrPhysical.m_alignment, 0, 0)
                : 0;
            if (lResult.m_baseResources[2] == 0)
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

namespace rw
{
    // ARTIST82BBC840 calls descriptor subtraction82B63E50: copy capacity,
    // subtract the five unsigned size words, and preserve each alignment.
    ResourceDescriptor LinearResourceAllocator::GetAvailable() const
    {
        ResourceDescriptor lAvailable = m_heapCapacity;
        for (uint32_t luLane = 0; luLane < KU_RESOURCE_LANE_COUNT; ++luLane)
            lAvailable.m_baseResourceDescriptors[luLane].m_size -=
                m_currentUsage.m_baseResourceDescriptors[luLane].m_size;
        return lAvailable;
    }
}
