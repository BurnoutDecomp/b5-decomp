#pragma once

#include "types.hpp"

// Forward declare the rw::Resource passed by reference to FixUp/FixDown. The relocation
// reads only its load base via CgsResource::GetLoadBase; the .cpp pulls the complete
// rw header transitively. A declaration here keeps the header dependency-light.
namespace rw
{
    struct Resource;
}

// CgsMemoryMap.h - the on-disk memory map: a streamed-in description of the title's
// memory banks, pools, raw resources and the four allocator families. Reconstructed
// from the DecFIGS DWARF (CgsMemoryMap.h) + the X360 FixUp pseudocode @ 0x8286C8B8.
//
// Console-faithful layout: 32-bit X360 fields, so the nine leading counts/ids occupy
// dwords 0-8 and the seven element-array pointers occupy dwords 9-15. FixUp relocates
// only those seven pointers by the resource load base (a 32-bit X360 address); on PC we
// replicate the exact 32-bit pointer arithmetic via uintptr-sized words (the committed
// CgsShaderConstants FixUps use the same idiom) so the on-disk layout and the console
// relocation behaviour are preserved.
namespace CgsMemory
{
    // The seven element record types reached by the relocated pointers. FixUp only
    // relocates the pointers (never dereferences them), so forward declarations are
    // sufficient here. DEFERRED: their real homes (record layouts + accessors) land
    // with their own TUs; declare-only for now.
    struct MemoryMapBank;
    struct MemoryMapPool;
    struct MemoryMapRawResource;
    struct MemoryMapLinearAllocator;
    struct MemoryMapHeapAllocator;
    struct MemoryMapRWLinearAllocator;
    struct MemoryMapRWGeneralAllocator;

    // CgsMemoryMap.h:45
    struct MemoryMap
    {
        // CgsMemoryMap.h:48 - the only supported on-disk version. The X360 FixUp asserts
        // the loaded miVersion equals this (the assert text reads "current version is 0").
        static const s32 KI_MEMORY_MAP_VERSION;   // = 0

        // CgsMemoryMap.h:50 - the allocator families enumerated by the map.
        enum EAllocatorType
        {
            E_ALLOCATORTYPE_RAW      = 0,
            E_ALLOCATORTYPE_LINEAR   = 1,
            E_ALLOCATORTYPE_HEAP     = 2,
            E_ALLOCATORTYPE_RWLINEAR = 3,
            E_ALLOCATORTYPE_RWHEAP   = 4
        };

        // CgsMemoryMap.h:60 - relocate the seven element-array pointers by the resource
        // load base. Defined in this TU.
        void FixUp(const rw::Resource& lrResource);

        // CgsMemoryMap.h:63 - the inverse (subtract the load base). DEFERRED to its own TU.
        void FixDown(const rw::Resource& lrResource);

        // CgsMemoryMap.h:66-123 - accessors. DECLARE-ONLY; defined in their own TUs.
        s32 GetVersion() const;
        s32 GetPlatform() const;
        const MemoryMapBank* GetBank(s32 liIndex) const;
        s32 GetNumBanks() const;
        const MemoryMapPool* GetPool(s32 liIndex) const;
        const MemoryMapPool* GetPool(const char* lpcName) const;
        s32 GetPoolIndex(const char* lpcName) const;
        const MemoryMapPool* GetPoolById(s32 liId) const;
        s32 GetNumPools() const;
        const MemoryMapRawResource* GetRawResource(s32 liIndex) const;
        s32 GetNumRawResources() const;
        const MemoryMapLinearAllocator* GetLinearAllocator(s32 liIndex) const;
        s32 GetNumLinearAllocators() const;
        const MemoryMapHeapAllocator* GetHeapAllocator(s32 liIndex) const;
        s32 GetNumHeapAllocators() const;
        const MemoryMapRWLinearAllocator* GetRWLinearAllocator(s32 liIndex) const;
        s32 GetNumRWLinearAllocators() const;
        const MemoryMapRWGeneralAllocator* GetRWGeneralAllocator(s32 liIndex) const;
        s32 GetNumRWGeneralAllocators() const;

    private:
        // ---- console-faithful member layout (X360 dword index in the comments) ----

        s32 miVersion;                       // :127  dword 0
        s32 miPlatform;                      // :128  dword 1
        u32 muNumBanks;                      // :130  dword 2
        u32 muNumPools;                      // :131  dword 3
        u32 muNumRawResources;               // :132  dword 4
        u32 muNumLinearAllocators;           // :133  dword 5
        u32 muNumHeapAllocators;             // :134  dword 6
        u32 muNumRWLinearAllocators;         // :135  dword 7
        u32 muNumRWGeneralAllocators;        // :136  dword 8

        MemoryMapBank*               mpBanks;               // :138  dword 9  (relocated)
        MemoryMapPool*               mpPools;               // :139  dword 10 (relocated)
        MemoryMapRawResource*        mpRawResources;        // :140  dword 11 (relocated)
        MemoryMapLinearAllocator*    mpLinearAllocators;    // :141  dword 12 (relocated)
        MemoryMapHeapAllocator*      mpHeapAllocators;      // :142  dword 13 (relocated)
        MemoryMapRWLinearAllocator*  mpRWLinearAllocators;  // :143  dword 14 (relocated)
        MemoryMapRWGeneralAllocator* mpRWGeneralAllocators; // :144  dword 15 (relocated)
    };
}
