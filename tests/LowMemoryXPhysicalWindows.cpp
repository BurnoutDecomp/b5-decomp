// Hosted Windows check of the real XPhysical pair, unchanged between variants.
// No allocator implementation, CRT free, or fallback producer lives in this test.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>

extern "C" void* XPhysicalAlloc(unsigned long, unsigned long, unsigned long, unsigned long);
extern "C" void XPhysicalFree(void*);
static_assert(sizeof(unsigned long) == 4 && sizeof(void*) == 8, "existing Windows x64 XPhysical ABI");

struct Request { const char* name; unsigned long bytes, alignment, protection; };
static const Request kRequests[] = {
    { "default16", 4096, 0, 4 },
    { "xma_input", 0x120000, 2048, 0x404 },
    { "xma_output", 0x180000, 2048, 4 },
    { "gui_movie", 0x1000000, 128, 4 },
};
static const std::uintptr_t kLowLimit = 0x100000000ull;

// Inspect OS metadata rather than reproducing the reservation algorithm.
static bool WritableSpan(std::uintptr_t begin, std::size_t bytes)
{
    if (bytes > (std::numeric_limits<std::uintptr_t>::max)() - begin) return false;
    const std::uintptr_t end = begin + bytes;
    for (std::uintptr_t cursor = begin; cursor < end; ) {
        MEMORY_BASIC_INFORMATION part = {};
        if (VirtualQuery(reinterpret_cast<void*>(cursor), &part, sizeof(part)) != sizeof(part) ||
            part.State != MEM_COMMIT || part.Type != MEM_PRIVATE || part.Protect != PAGE_READWRITE)
            return false;
        const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(part.BaseAddress);
        if (part.RegionSize == 0 || part.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - base)
            return false;
        const std::uintptr_t next = base + part.RegionSize;
        if (next <= cursor) return false;
        cursor = next < end ? next : end;
    }
    return true;
}

static bool WholeReservationLow(void* allocation, std::size_t requested, std::size_t& span)
{
    const std::uintptr_t start = reinterpret_cast<std::uintptr_t>(allocation);
    span = 0;
    for (std::uintptr_t cursor = start; ; ) {
        MEMORY_BASIC_INFORMATION part = {};
        if (VirtualQuery(reinterpret_cast<void*>(cursor), &part, sizeof(part)) != sizeof(part)) return false;
        if (part.AllocationBase != allocation) return span >= requested;
        const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(part.BaseAddress);
        if (base != cursor || part.State != MEM_COMMIT || part.Type != MEM_PRIVATE ||
            part.Protect != PAGE_READWRITE || part.AllocationProtect != PAGE_READWRITE ||
            part.RegionSize == 0 || cursor >= kLowLimit ||
            part.RegionSize > kLowLimit - cursor) return false;
        span += part.RegionSize;
        // The largest legitimate request is16MiB; bound malformed metadata walks.
        if (span > 32u * 1024u * 1024u) return false;
        cursor += part.RegionSize;
    }
}

static bool ReleasedSpan(std::uintptr_t begin, std::size_t bytes)
{
    const std::uintptr_t end = begin + bytes; // the whole reservation was bounded above
    for (std::uintptr_t cursor = begin; cursor < end; ) {
        MEMORY_BASIC_INFORMATION part = {};
        if (VirtualQuery(reinterpret_cast<void*>(cursor), &part, sizeof(part)) != sizeof(part) ||
            part.State != MEM_FREE || part.RegionSize == 0)
            return false;
        const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(part.BaseAddress);
        if (part.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - base) return false;
        const std::uintptr_t next = base + part.RegionSize;
        if (next <= cursor) return false;
        cursor = next < end ? next : end;
    }
    return true;
}

static bool TouchRequestedPages(void* memory, std::size_t bytes, std::size_t page, unsigned long& count)
{
    volatile unsigned char* data = static_cast<volatile unsigned char*>(memory);
    count = 0;
    for (std::size_t offset = 0; offset < bytes; offset += page) {
        const unsigned char value = static_cast<unsigned char>((offset / page) ^ 0xA5u);
        data[offset] = value;
        if (data[offset] != value) return false;
        ++count;
    }
    data[bytes - 1] = 0x5Au;
    return data[bytes - 1] == 0x5Au;
}

int main()
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    SYSTEM_INFO system = {};
    GetSystemInfo(&system);
    if (system.dwPageSize == 0) return 70;
    unsigned passes = 0, families = 0, others = 0;
    for (const Request& request : kRequests) {
        // All sizes are nonzero and alignments are valid after the real API's
        // default16 handling. The unchanged control never receives invalid CRT args.
        void* memory = XPhysicalAlloc(request.bytes, 0xFFFFFFFFul, request.alignment, request.protection);
        const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(memory);
        MEMORY_BASIC_INFORMATION info = {};
        const bool queried = memory && VirtualQuery(memory, &info, sizeof(info)) == sizeof(info);
        const bool family = queried && info.AllocationBase == memory;
        const unsigned long alignment = request.alignment ? request.alignment : 16ul;
        const bool aligned = memory && (address % alignment) == 0;
        const bool requestedLow = memory && address < kLowLimit && request.bytes <= kLowLimit - address;
        std::size_t reservationSpan = 0;
        const bool reservationLow = family && WholeReservationLow(memory, request.bytes, reservationSpan);
        const bool writable = queried && WritableSpan(address, request.bytes);
        unsigned long touchedPages = 0;
        const bool touched = writable && TouchRequestedPages(memory, request.bytes, system.dwPageSize, touchedPages);
        // Always use this variant's actual paired provider. Never VirtualFree a
        // CRT pointer, never _aligned_free a Windows reservation, never dereference
        // the pointer after release. VirtualQuery of its old address is metadata only.
        if (memory) XPhysicalFree(memory);
        MEMORY_BASIC_INFORMATION released = {};
        const bool releaseQueried = memory && VirtualQuery(reinterpret_cast<void*>(address), &released, sizeof(released)) == sizeof(released);
        const bool reservationReleased = family && reservationLow && releaseQueried &&
            released.State == MEM_FREE && ReleasedSpan(address, reservationSpan);
        const char* outcome;
        if (!memory || !queried || !aligned || !writable || !touched || !releaseQueried) {
            outcome = "OTHER_FAILURE"; ++others;
        } else if (!family) {
            // This deterministic backing-family witness is selected before any
            // address-range result. A low CRT allocation still fails the control.
            outcome = "ALLOCATION_BASE_MISMATCH"; ++families;
        } else if (!requestedLow || !reservationLow || !reservationReleased) {
            outcome = "OTHER_FAILURE"; ++others;
        } else {
            outcome = "PASS"; ++passes;
        }
        std::printf("ALLOCATION_CASE {\"name\":\"%s\",\"bytes\":%lu,\"requested_alignment\":%lu,\"effective_alignment\":%lu,\"xbox_protection\":%lu,\"pointer\":\"0x%llx\",\"allocation_base\":\"0x%llx\",\"region_base\":\"0x%llx\",\"region_bytes\":%llu,\"region_state\":%lu,\"region_protect\":%lu,\"region_allocation_protect\":%lu,\"region_type\":%lu,\"queried\":%d,\"allocation_base_equal\":%d,\"aligned\":%d,\"requested_span_low\":%d,\"whole_reservation_low\":%d,\"reservation_bytes\":%llu,\"writable\":%d,\"touched\":%d,\"touched_pages\":%lu,\"release_called\":%d,\"release_queried\":%d,\"release_state\":%lu,\"reservation_released\":%d,\"outcome\":\"%s\"}\n",
            request.name, request.bytes, request.alignment, alignment, request.protection,
            static_cast<unsigned long long>(address),
            static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(info.AllocationBase)),
            static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(info.BaseAddress)),
            static_cast<unsigned long long>(info.RegionSize), info.State, info.Protect, info.AllocationProtect, info.Type,
            queried ? 1 : 0, family ? 1 : 0, aligned ? 1 : 0, requestedLow ? 1 : 0,
            reservationLow ? 1 : 0, static_cast<unsigned long long>(reservationSpan),
            writable ? 1 : 0, touched ? 1 : 0, touchedPages, memory ? 1 : 0,
            releaseQueried ? 1 : 0, releaseQueried ? released.State : 0ul,
            reservationReleased ? 1 : 0, outcome);
    }
    std::printf("ALLOCATION_FAMILY_RESULT cases=4 passes=%u family_failures=%u other_failures=%u page_bytes=%lu peak_requested_bytes=16777216 ulong_bytes=4 pointer_bytes=8\n",
                passes, families, others, system.dwPageSize);
    return others ? 70 : families ? 31 : 0;
}
