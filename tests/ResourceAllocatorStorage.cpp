// Exercise whole production RW and EA owners, including host storage boundaries.
#include "rw/rwcore_general_alloc.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <initializer_list>
#include <malloc.h>

#ifndef NATIVE_ADOPT_METHOD
#define NATIVE_ADOPT_METHOD AdoptMemory
#endif

namespace
{
    int checks = 0, failures = 0;
    void Check(bool result, const char* label)
    {
        ++checks;
        failures += !result;
        std::printf("%s %s\n", result ? "PASS" : "FAIL", label);
    }
    struct Arena
    {
        static constexpr size_t guard = 16384;
        unsigned char* allocation;
        unsigned char* data;
        size_t size;
        explicit Arena(size_t count) : size(count)
        {
            allocation = static_cast<unsigned char*>(_aligned_malloc(size + guard * 2, 256));
            if (!allocation) std::abort();
            std::memset(allocation, 0xA5, size + guard * 2);
            data = allocation + guard;
        }
        ~Arena() { _aligned_free(allocation); }
        bool Owns(const void* pointer, size_t bytes = 1) const
        {
            const uintptr_t value = reinterpret_cast<uintptr_t>(pointer);
            return value >= reinterpret_cast<uintptr_t>(data) &&
                value + bytes <= reinterpret_cast<uintptr_t>(data) + size;
        }
        bool Guards() const
        {
            for (size_t i = 0; i < guard; ++i)
                if (allocation[i] != 0xA5 || data[size + i] != 0xA5) return false;
            return true;
        }
    };
    bool Empty(const rw::Resource& resource)
    {
        for (unsigned lane = 0; lane < 5; ++lane)
            if (resource.m_baseResources[lane]) return false;
        return true;
    }
    void Lane(rw::ResourceDescriptor& descriptor, unsigned lane, unsigned bytes, unsigned alignment)
    {
        descriptor.m_baseResourceDescriptors[lane].m_size = bytes;
        descriptor.m_baseResourceDescriptors[lane].m_alignment = alignment;
    }
    void NativeGeneral()
    {
        Arena main(65536), physical(32768);
        rw::Resource backing;
        backing.m_baseResources[0] = main.data;
        backing.m_baseResources[2] = physical.data;
        rw::ResourceDescriptor capacity;
        Lane(capacity, 0, static_cast<unsigned>(main.size), 16);
        Lane(capacity, 2, static_cast<unsigned>(physical.size), 16);
        rw::core::GeneralResourceAllocator allocator;
        allocator.NATIVE_ADOPT_METHOD(backing, capacity);
        const size_t mainFree = allocator.m_mainAllocator.GetLargestFreeBlock(false);
        const size_t physicalFree = allocator.m_physicalAllocator.GetLargestFreeBlock(false);
        void* complete = allocator.Alloc(0, static_cast<unsigned>(mainFree), 16);
        if (complete) std::memset(complete, 0x19, mainFree);
        Check(complete && main.Owns(complete, mainFree) && main.Guards(),
              "native external object uses the supplied extent without a hidden object reserve");
        allocator.Free(complete);

        rw::ResourceDescriptor request;
        for (unsigned lane = 0; lane < 5; ++lane) Lane(request, lane, 64, 32);
        rw::Resource result = allocator.DoAllocate(request, "five lanes");
        bool ownership = true;
        for (unsigned lane = 0; lane < 5; ++lane)
            ownership &= (lane == 2 ? physical : main).Owns(result.m_baseResources[lane], 64);
        Check(ownership, "only resource lane two belongs to the physical heap");
        // Pointer-based cleanup also permits the earlier implementation to finish the red run.
        if (ownership) allocator.DoFree(result);
        else for (void* pointer : result.m_baseResources) allocator.Free(pointer);
        Check(allocator.m_mainAllocator.GetLargestFreeBlock(false) == mainFree &&
              allocator.m_physicalAllocator.GetLargestFreeBlock(false) == physicalFree,
              "five-lane release restores both actual heaps");

        rw::ResourceDescriptor impossible;
        Lane(impossible, 0, 64, 16);
        Lane(impossible, 2, static_cast<unsigned>(physical.size + 256), 16);
        result = allocator.DoAllocate(impossible, "rollback");
        Check(Empty(result) && allocator.m_mainAllocator.GetLargestFreeBlock(false) == mainFree &&
              allocator.m_physicalAllocator.GetLargestFreeBlock(false) == physicalFree,
              "failed physical carve releases earlier main allocations and returns no lanes");
        for (void* pointer : result.m_baseResources) allocator.Free(pointer);
        Check(main.Guards() && physical.Guards(), "both externally owned pool boundaries remain intact");
    }
    void NativeLinear()
    {
        Arena first(1024), second(1024);
        rw::Resource backing;
        backing.m_baseResources[0] = first.data + 1;
        backing.m_baseResources[3] = second.data + 3;
        rw::ResourceDescriptor capacity;
        Lane(capacity, 0, 512, 1);
        Lane(capacity, 3, 512, 1);
        rw::LinearResourceAllocator allocator;
        allocator.NATIVE_ADOPT_METHOD(backing, capacity);
        rw::ResourceDescriptor request;
        Lane(request, 0, 32, 64);
        Lane(request, 3, 32, 64);
        rw::Resource result = allocator.DoAllocate(request, "unaligned bases");
        Check(result.m_baseResources[0] && result.m_baseResources[3] &&
              (reinterpret_cast<uintptr_t>(result.m_baseResources[0]) & 63) == 0 &&
              (reinterpret_cast<uintptr_t>(result.m_baseResources[3]) & 63) == 0,
              "linear requests align absolute addresses from independently unaligned pools");
        Check(allocator.m_numAllocations == 1,
              "one multi-lane resource increments the linear allocation count once");
        const rw::ResourceDescriptor before = allocator.m_currentUsage;
        const rw::ResourceDescriptor padding = allocator.m_paddingUsed;
        const unsigned count = allocator.m_numAllocations;
        Lane(request, 3, 1024, 128);
        result = allocator.DoAllocate(request, "atomic failure");
        Check(Empty(result) && std::memcmp(&before, &allocator.m_currentUsage, sizeof(before)) == 0 &&
              std::memcmp(&padding, &allocator.m_paddingUsed, sizeof(padding)) == 0 &&
              allocator.m_numAllocations == count,
              "failed multi-lane linear allocation leaves all cursors padding and count unchanged");
        Check(first.Guards() && second.Guards(), "linear failure does not touch either arena guard");
    }
#ifndef LEGACY_ADOPTION_ONLY
    void Placement()
    {
        rw::Resource empty;
        rw::ResourceDescriptor capacity;
        Lane(capacity, 0, 32768, 1);
        Check(!rw::LinearResourceAllocator::Initialize(empty, capacity) &&
              !rw::core::GeneralResourceAllocator::Initialize(empty, capacity),
              "both original placement APIs return null for absent object storage");
        for (unsigned alignment : {1u, 16u, 128u})
        {
            Lane(capacity, 0, 32768, alignment);
            rw::ResourceDescriptor needed;
            rw::core::GeneralResourceAllocator::GetResourceDescriptor(&needed, &capacity);
            Arena storage(needed.m_baseResourceDescriptors[0].m_size);
            rw::Resource backing;
            backing.m_baseResources[0] = storage.data;
            auto* allocator = rw::core::GeneralResourceAllocator::Initialize(backing, capacity);
            const size_t bytes = allocator->m_mainAllocator.GetLargestFreeBlock(false);
            void* payload = allocator->Alloc(0, static_cast<unsigned>(bytes), 16);
            if (payload) std::memset(payload, 0x39, bytes);
            Check(allocator == reinterpret_cast<void*>(storage.data) &&
                  reinterpret_cast<uintptr_t>(payload) >= reinterpret_cast<uintptr_t>(storage.data) + sizeof(*allocator) &&
                  storage.Owns(payload, bytes) && storage.Guards(),
                  "General placement object and full advertised heap fit the returned backing descriptor");
            allocator->Free(payload);
            allocator->~GeneralResourceAllocator();
        }
        rw::ResourceDescriptor needed;
        rw::LinearResourceAllocator::GetResourceDescriptor(&needed, &capacity);
        Arena storage(needed.m_baseResourceDescriptors[0].m_size);
        rw::Resource backing;
        backing.m_baseResources[0] = storage.data;
        auto* allocator = rw::LinearResourceAllocator::Initialize(backing, capacity);
        rw::Resource result = allocator->DoAllocate(capacity, "entire linear payload");
        if (result.m_baseResources[0]) std::memset(result.m_baseResources[0], 0x49, 32768);
        Check(allocator == reinterpret_cast<void*>(storage.data) &&
              reinterpret_cast<uintptr_t>(result.m_baseResources[0]) >= reinterpret_cast<uintptr_t>(storage.data) + sizeof(*allocator) &&
              storage.Owns(result.m_baseResources[0], 32768) && storage.Guards(),
              "Linear placement reserves its native object and still supplies the full requested payload");
        allocator->~LinearResourceAllocator();
    }
#endif
}
int main()
{
    NativeGeneral();
    NativeLinear();
#ifndef LEGACY_ADOPTION_ONLY
    Placement();
#endif
    std::printf("ResourceAllocatorStorage: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
