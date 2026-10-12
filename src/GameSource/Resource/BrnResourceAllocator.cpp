#include "GameSource/Resource/BrnResourceAllocator.h"
#include "GameSource/Resource/BrnMemoryMapData.h"  // MemoryMapImageInitializer
#include "ppmalloc/EAGeneralAllocator.h"   // the backing EA general allocator
#include "rw/rwcore_general_alloc.h"       // rw::core::GeneralResourceAllocator (GetGameDataGeneralAllocator)
#include "GameShared/GameClasses/Memory/PC/CgsLowMemoryPC.h"   // LowMemory::Reserve (low-4GB root block)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::gpDebugPrint, CgsDev::Message::gxMessageFilterFlags
#include <new>   // std::nothrow (the GlobalGraphics arena carve)
#include <malloc.h>   // _aligned_malloc (DefaultLinearAllocator PC backing)
#include <Windows.h>   // GlobalMemoryStatus (PrintConsoleMemory / GetAvailableMemory)

// BrnResource::Allocators / HeapResourceAllocator - the engine's root debug resource allocator,
// the heap the whole GameData resource system (ConstructResourceModule -> ResourceModule::Construct)
// carves its module/bank memory from, and the one ICE::ICEMemory::Construct allocates its debug pool
// from.
//
// [PC-LEAF, consistent with the allocator-layer decision]: on X360 this is
// Allocators::mGlobalDebugAllocator - an rw allocator over a memory region the boot path reserves;
// here it is a big malloc-backed region with an EA::Allocator::GeneralAllocator over it. The backing
// block is allocated lazily on first use; mpInternalDebugAllocator is published non-null at static
// init (it points at the allocator object) so the ICE null-check passes regardless of init order.
namespace BrnResource
{
    namespace
    {
        // 768 MB root debug heap. [PC-LEAF] the X360 reserves a fixed ~smaller budget split across banks;
        // the PC bring-up routes the 27 pools' memory (~266 MB) through the MemoryModule's root banks (a
        // ~384 MB type0+type1 backing carved from here), with a debug-heap FALLBACK if a bank carve fails
        // (another ~266 MB) -- so this is sized to hold the bank backing + a full fallback at once.
        const size_t KN_DEBUG_HEAP_SIZE = 768u * 1024u * 1024u;

        EA::Allocator::GeneralAllocator s_DebugGeneralAllocator;   // ctor -> Init (empty) at static init
        HeapResourceAllocator           s_DebugResourceAllocator;
        bool                            s_bDebugHeapBacked = false;

        // Lazily reserve + adopt the backing block (avoids the reservation at static init).
        //
        // FLAG PC-platform leaf: the block MUST live below 4 GB. Everything the resource
        // system touches is carved from this one root -- the MemoryModule root banks and
        // all 27 pools (so every bnd2 resource payload and its in-place FixUp), plus all
        // 24 memory-map allocators (so the AttribSys package heap: every Attrib::Alloc,
        // the schema.vlt/.bin blobs and the Attrib vault's PtrN pointer fixups). The
        // shipped serialised format stores those pointers in FOUR-BYTE slots, read back
        // with the committed PointerFromU32 idiom; on the 32-bit X360 that is free, on
        // x64 it only round-trips when the pointee is under 4 GB. The plain CRT malloc
        // this used to be handed out ~0x21e_00000000 on Windows x64, so the first fixed-up
        // slot dereference faulted (strlen on Attrib's truncated typenames pointer).
        // See GameShared/GameClasses/Memory/PC/CgsLowMemoryPC.h.
        void EnsureDebugHeapBacked()
        {
            if (s_bDebugHeapBacked)
                return;
            void* lpBlock = CgsMemory::LowMemory::Reserve(KN_DEBUG_HEAP_SIZE);
            if (lpBlock)
                s_DebugGeneralAllocator.AddCore(lpBlock, KN_DEBUG_HEAP_SIZE);
            s_bDebugHeapBacked = true;
        }

        MEMORYSTATUSEX GetHostMemoryStatusPC()
        {
            // FLAG PC-platform leaf: query the host's full-width virtual, physical and page-file status.
            MEMORYSTATUSEX lMemoryStatus = {};
            lMemoryStatus.dwLength = static_cast<DWORD>(sizeof(lMemoryStatus));
            ::GlobalMemoryStatusEx(&lMemoryStatus);
            return lMemoryStatus;
        }
    }

    // Published non-null at static init; points at the root HeapResourceAllocator (which IS-A
    // rw::IResourceAllocator) so the resource-module Construct chain can carve memory through its
    // virtual DoAllocate. The backing 64 MB block is adopted lazily on first Allocate. (ICE null-checks
    // this; the resource modules cast it to rw::IResourceAllocator* and call DoAllocate.)
    void* Allocators::mpInternalDebugAllocator = static_cast<rw::IResourceAllocator*>(&s_DebugResourceAllocator);
    DefaultLinearAllocator Allocators::mInitialDefaultAllocator;

    HeapResourceAllocator* GetDebugAllocator()
    {
        EnsureDebugHeapBacked();
        return &s_DebugResourceAllocator;
    }

    namespace
    {
        // The single GameData general allocator (allocator-gate leaf). 64 MB carved from the debug
        // heap, lazily on first use. [PC-LEAF] stands in for the per-bank rw general allocators the
        // X360's GameDataModule::CreateAllocators builds from the memory map; the loading-screen
        // module loads (e.g. SoundModule) carve their module memory from it.
        const size_t                       KN_GAMEDATA_GENERAL_SIZE = 64u * 1024u * 1024u;
        rw::core::GeneralResourceAllocator s_GameDataGeneralAllocator;   // EA allocator members ctor at static init
        bool                               s_bGameDataGeneralInit = false;
    }

    rw::core::GeneralResourceAllocator* GetGameDataGeneralAllocator()
    {
        if (!s_bGameDataGeneralInit)
        {
            EnsureDebugHeapBacked();
            void* lpRegion = s_DebugGeneralAllocator.MallocAligned(KN_GAMEDATA_GENERAL_SIZE, 16);
            if (lpRegion)
            {
                rw::Resource lResource;
                for (uint32_t lu = 0; lu < rw::KU_RESOURCE_LANE_COUNT; ++lu)
                    lResource.m_baseResources[lu] = 0;
                lResource.m_baseResources[0] = lpRegion;

                rw::ResourceDescriptor lCapacity;
                for (uint32_t lu = 0; lu < rw::KU_RESOURCE_LANE_COUNT; ++lu)
                {
                    lCapacity.m_baseResourceDescriptors[lu].m_size      = 0;
                    lCapacity.m_baseResourceDescriptors[lu].m_alignment = 1;
                }
                lCapacity.m_baseResourceDescriptors[0].m_size      = KN_GAMEDATA_GENERAL_SIZE;
                lCapacity.m_baseResourceDescriptors[0].m_alignment = 16;

                s_GameDataGeneralAllocator.Initialize(lResource, lCapacity);
            }
            s_bGameDataGeneralInit = true;
        }
        return &s_GameDataGeneralAllocator;
    }

    // Never written by the game (the flag sits in initialised data with readers only), so a
    // failed memory check logs but does not assert.
    bool Allocators::mbAssertOnMemoryChange = false;

    // One line per memory figure, each behind its own message-filter test. The field values are
    // shifted down to MB before they are streamed.
    void PrintConsoleMemory(const char* lpcMessage)
    {
        if (lpcMessage != NULL && (CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << lpcMessage << " ";
        }

        const MEMORYSTATUSEX lMemoryStatus = GetHostMemoryStatusPC();

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "XBox 360 memory status: \n";
        }
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "    " << static_cast<u64>(lMemoryStatus.ullTotalVirtual >> 20)
                                       << " total MB of virtual memory.\n";
        }
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "    " << static_cast<u64>(lMemoryStatus.ullAvailVirtual >> 20)
                                       << " free MB of virtual memory.\n";
        }
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "    " << static_cast<u64>(lMemoryStatus.ullTotalPhys >> 20)
                                       << " total MB of physical memory.\n";
        }
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "    " << static_cast<u64>(lMemoryStatus.ullAvailPhys >> 20)
                                       << " free MB of physical memory.\n";
        }
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "    " << static_cast<u64>(lMemoryStatus.ullTotalPageFile >> 20)
                                       << " total MB of paging file.\n";
        }
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "    " << static_cast<u64>(lMemoryStatus.ullAvailPageFile >> 20)
                                       << " free MB of paging file.\n";
        }
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "    " << static_cast<u32>(lMemoryStatus.dwMemoryLoad)
                                       << " percent of memory is in use.\n";
        }
    }

    // ARTIST 0x82664728. Relocate the generated typed image once, then install the original
    // initial default allocator before GameDataModule construction and its map consumers.
    void Allocators::InitMemoryMap()
    {
        PrintConsoleMemory("Pre-memory map initialisation");

        CgsMemory::MemoryMap& lrMap = MemoryMapImageInitializer::GetMemoryMap();
        rw::Resource lMapResource;  // its native ctor nulls all five slots as main does explicitly on X360
        lMapResource.m_baseResources[0] = &lrMap;
        lrMap.FixUp(lMapResource);

        mInitialDefaultAllocator.Construct(0);
        rw::ResourceAllocatorRegistry::SetDefaultAllocator(&mInitialDefaultAllocator);

        PrintConsoleMemory("Post-memory map initialisation");
    }

    // FLAG PC-platform note: the host MEMORYSTATUS fields are pointer-sized, so a free figure of
    // 4 GB or more wraps in the u32 the console returns. The memory checks only compare two
    // readings, so a wrapped value still reports a change.
    u32 GetAvailableMemory()
    {
        MEMORYSTATUS lMemoryStatus;
        GlobalMemoryStatus(&lMemoryStatus);
        return static_cast<u32>(lMemoryStatus.dwAvailPhys);
    }

    namespace
    {
        // X360 backs this startup-global allocator with two XPhysicalAlloc regions and has one
        // InitMemoryMap caller in main. The host substitutions keep the same sizes/alignment and
        // process lifetime; the operating system reclaims these static-allocator backing blocks.
        // No fallback size or allocator policy is added. A failed host allocation is passed to
        // LinearMalloc::Create so its original lpBuffer assertion and post-assert stores run.
        void* gaInitialDefaultLinearBacking[2] = { nullptr, nullptr };

        const char* const KAC_DEFAULT_LINEAR_RESOURCE_NAMES[5] =
        {
            "main memory",
            "disposable",
            "physical",
            "uninitialized",
            "disposable uninitialized"
        };

        void LogDefaultLinearMemoryBreakdown(u32 luSize, const char* lpcName)
        {
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint << "  " << (luSize / 0x100000u) << "mb "
                    << ((luSize % 0x100000u) / 0x400u) << "kb " << (luSize % 0x400u)
                    << "b of " << lpcName << "\n";
            }
        }

        void FireDefaultLinearAllocatorAssert(const char* lpcMessage, s32 liLine)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                lpcMessage,
                "..\\..\\..\\GameSource\\Resource/BrnResourceAllocator.cpp",
                liLine);
            CgsDev::Assert::EndAssert();
        }
    }

    // ARTIST 0x82661A60. The argument is unused; construct five typed X360 allocator lanes,
    // then create lanes 0 and 2 over the original fixed-size allocations.
    void DefaultLinearAllocator::Construct(u32 luPreAllocatedSize)
    {
        (void)luPreAllocatedSize;
        for (s32 liType = 0; liType < 5; ++liType)
            maAllocators[liType].Construct();

        // FLAG PC-platform leaf: XPhysicalAlloc's native replacement preserves its 64 KiB
        // alignment and exact sizes. The two blocks live for the process lifetime of the
        // static mInitialDefaultAllocator, as do the corresponding console allocations.
        gaInitialDefaultLinearBacking[0] = _aligned_malloc(0x800u, 0x10000u);
        maAllocators[0].Create(gaInitialDefaultLinearBacking[0], 0x800u);

        gaInitialDefaultLinearBacking[1] = _aligned_malloc(0x400000u, 0x10000u);
        maAllocators[2].Create(gaInitialDefaultLinearBacking[1], 0x400000u);
    }

    // ARTIST 0x82664800. All five descriptor lanes are allocated from the matching
    // LinearMalloc; the original diagnostics, allocation assertion and resource slots remain.
    rw::Resource DefaultLinearAllocator::DoAllocate(
        const rw::ResourceDescriptor& lrDescriptor, const char* /*lpcName*/)
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "-----------------------------------------\n";
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "Allocating into default linear allocator:\n";

        for (s32 liType = 0; liType < 5; ++liType)
        {
            const u32 luSize = lrDescriptor.m_baseResourceDescriptors[liType].m_size;
            if (luSize != 0)
                LogDefaultLinearMemoryBreakdown(luSize, KAC_DEFAULT_LINEAR_RESOURCE_NAMES[liType]);
        }

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "Remaining pre-allocation\n";
        for (s32 liType = 0; liType < 5; ++liType)
        {
            const u32 luFree = static_cast<u32>(maAllocators[liType].GetFreeMemory());
            if (luFree != 0)
                LogDefaultLinearMemoryBreakdown(luFree, KAC_DEFAULT_LINEAR_RESOURCE_NAMES[liType]);
        }

        rw::Resource lResource;
        for (s32 liType = 0; liType < 5; ++liType)
        {
            const rw::BaseResourceDescriptor& lrLane =
                lrDescriptor.m_baseResourceDescriptors[liType];
            if (lrLane.m_size != 0)
            {
                maAllocators[liType].SetAlignment(lrLane.m_alignment);
                lResource.m_baseResources[liType] = maAllocators[liType].Malloc(lrLane.m_size);
                if (lResource.m_baseResources[liType] == nullptr)
                    FireDefaultLinearAllocatorAssert("Failed to allocate memory\n", 948);
            }
        }

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "Remaining post-allocation\n";
        for (s32 liType = 0; liType < 5; ++liType)
        {
            const u32 luFree = static_cast<u32>(maAllocators[liType].GetFreeMemory());
            if (luFree != 0)
                LogDefaultLinearMemoryBreakdown(luFree, KAC_DEFAULT_LINEAR_RESOURCE_NAMES[liType]);
        }
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "-----------------------------------------\n";
        return lResource;
    }

    // ARTIST 0x82661CF8: this initial global allocator is monotonic and cannot free a block.
    void DefaultLinearAllocator::DoFree(const rw::Resource& /*lrResource*/)
    {
        FireDefaultLinearAllocatorAssert(
            "Should never be freeing memory from the global allocator\n", 984);
    }

    // ARTIST 0x82661D88: disposable allocation/free is not supported on this allocator.
    void DefaultLinearAllocator::DoFreeDisposable(rw::Resource& /*lrResource*/)
    {
        FireDefaultLinearAllocatorAssert(
            "Should never be allocating or freeing disposable memory!!!\n", 999);
    }

    // Out-of-line defaulted destructor: anchors the DefaultLinearAllocator vtable in this TU so
    // the compiler emits its polymorphic-delete thunk (the X360 `scalar deleting destructor'
    // @0x82666948). The underlying object teardown is trivial (only the rw::IResourceAllocator
    // base) -- the X360 thunk just rewrites the vptr and conditionally operator-deletes.
    DefaultLinearAllocator::~DefaultLinearAllocator() = default;

    // rw::IResourceAllocator virtual entry point -> forward to Allocate.
    rw::Resource HeapResourceAllocator::DoAllocate(const rw::ResourceDescriptor& lDescriptor, const char* lpcName)
    {
        return Allocate(lDescriptor, lpcName);
    }

    // Carve a resource matching lDescriptor from the root debug heap. The returned rw::Resource's
    // m_baseResources[0] is the memory block (the X360 DoAllocate fills the per-pool base pointers;
    // PC leaf serves pool 0 from the general heap).
    rw::Resource HeapResourceAllocator::Allocate(const rw::ResourceDescriptor& lDescriptor, const char* /*lpcName*/)
    {
        EnsureDebugHeapBacked();
        rw::Resource lResource;
        for (uint32_t lu = 0; lu < rw::KU_RESOURCE_LANE_COUNT; ++lu)
            lResource.m_baseResources[lu] = 0;

        const uint32_t luSize  = lDescriptor.m_baseResourceDescriptors[0].m_size;
        uint32_t       luAlign = lDescriptor.m_baseResourceDescriptors[0].m_alignment;
        if (luAlign < 16)
            luAlign = 16;
        if (luSize)
            lResource.m_baseResources[0] = s_DebugGeneralAllocator.MallocAligned(luSize, luAlign);
        return lResource;
    }

    // =====================================================================================
    // GetGlobalGraphicsAllocator -- the process-wide "GlobalGraphics" linear resource
    // allocator (X360 object @ dword_82F2C814). ADDITIVE 2026-09-02 (tyre-mark wave):
    // BrnResourceAllocator.h has declared it since the shadow wave, BrnRendererMemory.cpp's
    // banner has called it out as "declaration-only, defined nowhere" ever since, and the
    // effects link is the first consumer that cannot proceed without it (LNK2019, measured).
    //
    // ⚠ FLAG PC-platform leaf. The console builds this object in
    // Allocators::ConstructGlobalGraphicsMemory @0x82666B70 over an XPhysicalAlloc'd region --
    // physically contiguous, GPU-visible memory sized from the X360 memory map
    // (Allocators::InitMemoryMap @0x82664728). Neither exists on the host and neither is
    // reconstructed. What every CALLER needs is the abstract rw::IResourceAllocator contract
    // (DoAllocate hands back a lane-0 block), so this supplies exactly that over one host heap
    // block -- the SAME shape BrnRendererModule.cpp's `sWorldDispatchAllocator` already uses for
    // the sky dome's buffers. A platform substitution, not a stand-in for game logic.
    //
    // WHO TAKES IT: ParticleModule::Prepare @0x8229BEA0 hands `off_82F2C814` to all five
    // contained Im3d renderers; the one that matters for the tyre mark, Im3dSkidsRenderer::
    // Construct @0x82295150, allocates the skid vertex/pixel ProgramBuffers through it (456 +
    // 228 bytes of microcode plus their headers). 256 KB leaves room for the rest of that family
    // as it lands; the arena is carved lazily, so a build that never touches effects pays nothing.
    // =====================================================================================
    namespace
    {
        rw::LinearResourceAllocator sGlobalGraphicsAllocator;
        bool                        sbGlobalGraphicsReady = false;
        const uint32_t              KU_GLOBAL_GRAPHICS_ARENA_BYTES = 256u * 1024u;
    }

    rw::IResourceAllocator* Allocators::GetGlobalGraphicsAllocator()
    {
        if (sbGlobalGraphicsReady)
            return &sGlobalGraphicsAllocator;

        void* lpHeap = ::operator new(KU_GLOBAL_GRAPHICS_ARENA_BYTES, std::nothrow);
        if (lpHeap == 0)
            return 0;

        rw::Resource           lHeapResource;
        rw::ResourceDescriptor lHeapCapacity;
        for (uint32_t luLane = 0; luLane < rw::KU_RESOURCE_LANE_COUNT; ++luLane)
        {
            lHeapResource.m_baseResources[luLane] = (luLane == 0) ? lpHeap : 0;
            lHeapCapacity.m_baseResourceDescriptors[luLane].m_size =
                (luLane == 0) ? KU_GLOBAL_GRAPHICS_ARENA_BYTES : 0u;
            lHeapCapacity.m_baseResourceDescriptors[luLane].m_alignment = (luLane == 0) ? 16u : 1u;
        }
        sGlobalGraphicsAllocator.Initialize(lHeapResource, lHeapCapacity);

        sbGlobalGraphicsReady = true;
        return &sGlobalGraphicsAllocator;
    }
}
