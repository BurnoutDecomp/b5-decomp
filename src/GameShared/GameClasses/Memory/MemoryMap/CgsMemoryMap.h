#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Memory/CgsMemoryBank.h"
#include "GameShared/GameClasses/Memory/CgsMemoryModuleIO.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"

namespace BrnResource
{
    struct MemoryMapImageInitializer;
}

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
// The X360 header stores nine leading counts/ids and seven 32-bit relative table pointers.
// The generated PC image preserves that producer/reader order in a typed native object: its
// table pointers begin as offsetof-derived relative pointer values, then FixUp adds the full
// native resource base once before any table consumer runs.
namespace CgsMemory
{
    // The map rows use the canonical records they contain. The X360 bank row is its five-lane
    // MemoryBank::Params (84 bytes); the pool row is the resource CreatePoolRequest. Native
    // pointer-bearing request rows widen with their canonical request types on PC.
    struct MemoryMapBaseBank
    {
    };

    struct MemoryMapBank : public MemoryMapBaseBank
    {
        const MemoryBank::Params* GetParams() const { return &mBankParams; }

    private:
        friend struct BrnResource::MemoryMapImageInitializer;
        MemoryBank::Params mBankParams;
    };

    struct MemoryMapPool : public MemoryMapBaseBank
    {
        const CgsResource::Events::CreatePoolRequest* GetRequest() const { return &mRequest; }

    private:
        friend struct BrnResource::MemoryMapImageInitializer;
        CgsResource::Events::CreatePoolRequest mRequest;
    };

    struct MemoryMapRawResource : public MemoryMapBaseBank
    {
        const MemoryIO::CreateResourceRequest* GetRequest() const { return &mRequest; }

    private:
        friend struct BrnResource::MemoryMapImageInitializer;
        MemoryIO::CreateResourceRequest mRequest;
    };

    struct MemoryMapLinearAllocator : public MemoryMapBaseBank
    {
        const MemoryIO::CreateAllocatorRequest* GetRequest() const { return &mRequest; }

    private:
        friend struct BrnResource::MemoryMapImageInitializer;
        MemoryIO::CreateAllocatorRequest mRequest;
    };

    struct MemoryMapHeapAllocator : public MemoryMapBaseBank
    {
        const MemoryIO::CreateAllocatorRequest* GetRequest() const { return &mRequest; }

    private:
        friend struct BrnResource::MemoryMapImageInitializer;
        MemoryIO::CreateAllocatorRequest mRequest;
    };

    struct MemoryMapRWLinearAllocator : public MemoryMapBaseBank
    {
        const MemoryIO::CreateLinearAllocatorRequest* GetRequest() const { return &mRequest; }

    private:
        friend struct BrnResource::MemoryMapImageInitializer;
        MemoryIO::CreateLinearAllocatorRequest mRequest;
    };

    struct MemoryMapRWGeneralAllocator : public MemoryMapBaseBank
    {
        const MemoryIO::CreateGeneralAllocatorRequest* GetRequest() const { return &mRequest; }

    private:
        friend struct BrnResource::MemoryMapImageInitializer;
        MemoryIO::CreateGeneralAllocatorRequest mRequest;
    };

    static_assert(sizeof(MemoryMapBank) == sizeof(MemoryBank::Params), "memory-map bank is the canonical Params row");
    static_assert(sizeof(MemoryMapPool) == sizeof(CgsResource::Events::CreatePoolRequest), "memory-map pool is the canonical CreatePoolRequest row");
    static_assert(sizeof(MemoryMapRawResource) == sizeof(MemoryIO::CreateResourceRequest), "memory-map raw resource uses the canonical request row");
    static_assert(sizeof(MemoryMapLinearAllocator) == sizeof(MemoryIO::CreateAllocatorRequest), "memory-map linear allocator uses the canonical request row");
    static_assert(sizeof(MemoryMapHeapAllocator) == sizeof(MemoryIO::CreateAllocatorRequest), "memory-map heap allocator uses the canonical request row");
    static_assert(sizeof(MemoryMapRWLinearAllocator) == sizeof(MemoryIO::CreateLinearAllocatorRequest), "memory-map rw-linear allocator uses the canonical request row");
    static_assert(sizeof(MemoryMapRWGeneralAllocator) == sizeof(MemoryIO::CreateGeneralAllocatorRequest), "memory-map rw-general allocator uses the canonical request row");

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

        // CgsMemoryMap.h:66-123 - typed table accessors. X360 GetBank/GetPool body pins the
        // index rules; native pointer arithmetic uses the canonical record sizes above.
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
        // The generated typed image helper seeds the serialized header and relative table pointers.
        friend struct BrnResource::MemoryMapImageInitializer;

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
