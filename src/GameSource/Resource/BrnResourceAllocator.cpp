#include "GameSource/Resource/BrnResourceAllocator.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include <cstring>
#include "GameSource/Resource/BrnMemoryMapData.h"  // MemoryMapImageInitializer
#include "ppmalloc/EAGeneralAllocator.h"   // the backing EA general allocator
#include "rw/rwcore_general_alloc.h"       // rw::core::GeneralResourceAllocator (GetGameDataGeneralAllocator)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::gpDebugPrint, CgsDev::Message::gxMessageFilterFlags
#include <new>   // std::nothrow (the GlobalGraphics arena carve)
#include <malloc.h>   // _aligned_malloc (DefaultLinearAllocator PC backing)
#include <xtl.h>   // GlobalMemoryStatus (PrintConsoleMemory / GetAvailableMemory)

// BrnResource::Allocators / HeapResourceAllocator - the engine's root debug resource allocator,
// the heap the whole GameData resource system (ConstructResourceModule -> ResourceModule::Construct)
// carves its module/bank memory from, and the one ICE::ICEMemory::Construct allocates its debug pool
// from.
//
// Allocators::mGlobalDebugAllocator - an rw allocator over a memory region the boot path reserves;
// here it is a big malloc-backed region with an EA::Allocator::GeneralAllocator over it. The backing
// block is allocated lazily on first use; mpInternalDebugAllocator is published non-null at static
// init (it points at the allocator object) so the ICE null-check passes regardless of init order.
namespace BrnResource
{
    // Native startup wrappers. ARTIST mRuntimeAllocator is 82F2C854 and
    // mEmbeddedTextureAllocator is 82F2C870; ConstructChildAllocators supplies
    // their backing allocators. Static zero initialization is not backing memory.
    HeapResourceAllocator Allocators::mRuntimeAllocator;
    LinearResourceAllocator Allocators::mEmbeddedTextureAllocator;
    LinearResourceAllocator Allocators::mGlobalResourceAllocator;

    LinearResourceAllocator* GetGlobalResourceAllocator()
    {
        return &Allocators::mGlobalResourceAllocator;
    }

    // ARTIST82666E48/50/68: Runtime backing allocator, name and print level.
    void HeapResourceAllocator::Construct(rw::core::GeneralResourceAllocator* lpAllocator,
                                          const char* lpcName, s32 liPrintLevel)
    {
        mpGeneralAllocator = lpAllocator;
        mpcName = lpcName;
        miPrintLevel = liPrintLevel;
    }

    HeapResourceAllocator* GetRuntimeAllocator()
    {
        return &Allocators::mRuntimeAllocator;
    }

    LinearResourceAllocator* GetEmbeddedTextureAllocator()
    {
        return &Allocators::mEmbeddedTextureAllocator;
    }

    namespace
    {
        // the PC bring-up routes the 27 pools' memory (~266 MB) through the MemoryModule's root banks (a
        // ~384 MB type0+type1 backing carved from here), with a debug-heap FALLBACK if a bank carve fails
        // (another ~266 MB) -- so this is sized to hold the bank backing + a full fallback at once.
        const size_t KN_DEBUG_HEAP_SIZE = 768u * 1024u * 1024u;

        EA::Allocator::GeneralAllocator s_DebugGeneralAllocator;   // ctor -> Init (empty) at static init
        HeapResourceAllocator           s_DebugResourceAllocator;
        bool                            s_bDebugHeapBacked = false;

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

                s_GameDataGeneralAllocator.AdoptMemory(lResource, lCapacity);
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

        MEMORYSTATUS lMemoryStatus;
        GlobalMemoryStatus(&lMemoryStatus);

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "XBox 360 memory status: \n";
        }
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "    " << static_cast<u32>(lMemoryStatus.dwTotalVirtual >> 20)
                                       << " total MB of virtual memory.\n";
        }
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "    " << static_cast<u32>(lMemoryStatus.dwAvailVirtual >> 20)
                                       << " free MB of virtual memory.\n";
        }
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "    " << static_cast<u32>(lMemoryStatus.dwTotalPhys >> 20)
                                       << " total MB of physical memory.\n";
        }
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "    " << static_cast<u32>(lMemoryStatus.dwAvailPhys >> 20)
                                       << " free MB of physical memory.\n";
        }
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "    " << static_cast<u32>(lMemoryStatus.dwTotalPageFile >> 20)
                                       << " total MB of paging file.\n";
        }
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "    " << static_cast<u32>(lMemoryStatus.dwAvailPageFile >> 20)
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
        void* gaInitialDefaultLinearBacking[2] = { 0, 0 };

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
                if (lResource.m_baseResources[liType] == 0)
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
    DefaultLinearAllocator::~DefaultLinearAllocator() {}

    // rw::IResourceAllocator virtual entry point -> forward to Allocate.
    rw::Resource HeapResourceAllocator::DoAllocate(const rw::ResourceDescriptor& lDescriptor, const char* lpcName)
    {
        return Allocate(lDescriptor, lpcName);
    }

    // Carve a resource matching lDescriptor from the root debug heap. The returned rw::Resource's
    // m_baseResources[0] is the memory block (the X360 DoAllocate fills the per-pool base pointers;
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
        sGlobalGraphicsAllocator.AdoptMemory(lHeapResource, lHeapCapacity);

        mGlobalGraphicsAllocator.Construct(mpInternalGraphicsAllocator, "GlobalGraphics");

        sbGlobalGraphicsReady = true;
        return &sGlobalGraphicsAllocator;
    }
}

// Canonical shipping platform declaration, shared with rw/audio/core/System.cpp.
extern "C" void* XPhysicalAlloc(unsigned long dwSize, unsigned long ulPhysicalAddress,
                               unsigned long dwAlignment, unsigned long flProtect);

namespace BrnResource
{
    // ARTIST's initial wrapper bytes are {vptr,0,0}; the descriptor's first
    // lane is initialized to {0,1} by82C5FEE8, matching the other four data lanes.
    rw::Resource Allocators::mGlobalGraphicsMemory;
    rw::ResourceDescriptor Allocators::mGlobalGraphicsMemoryDescriptor;
    rw::LinearResourceAllocator* Allocators::mpInternalGraphicsAllocator = 0;
    LinearResourceAllocator Allocators::mGlobalGraphicsAllocator;

    void LinearResourceAllocator::Construct(rw::LinearResourceAllocator* lpAllocator,
                                            const char* lpcName)
    {
        mpLinearAllocator = lpAllocator;
        mpcName = lpcName;
    }

    // DecFIGS h387; ARTIST8240A7A4..7C0 and other inlined callers return this
    // exact public wrapper, without selecting the separate native rw allocator.
    LinearResourceAllocator* GetGraphicsAllocator()
    {
        return &Allocators::mGlobalGraphicsAllocator;
    }

    // ARTIST826647B0: fill every present resource lane with the original0xCD pattern.
    void ClearResource(rw::Resource& lrResource, const rw::ResourceDescriptor& lrDescriptor)
    {
        for (u32 luLane = 0; luLane < rw::KU_RESOURCE_LANE_COUNT; ++luLane)
            if (lrResource.m_baseResources[luLane])
                std::memset(lrResource.m_baseResources[luLane], 0xCD,
                            lrDescriptor.m_baseResourceDescriptors[luLane].m_size);
    }

    // ARTIST82666B70. Unactivated original engine body; the existing native
    // GetGlobalGraphicsAllocator helper and its256KiB arena are unchanged.
    void Allocators::ConstructGlobalGraphicsMemory()
    {
        mGlobalGraphicsMemoryDescriptor.m_baseResourceDescriptors[0].m_size = 0x640000;
        mGlobalGraphicsMemoryDescriptor.m_baseResourceDescriptors[0].m_alignment = 4096;
        mGlobalGraphicsMemoryDescriptor.m_baseResourceDescriptors[2].m_size = 0x1D00000;
        mGlobalGraphicsMemoryDescriptor.m_baseResourceDescriptors[2].m_alignment = 4096;
        rw::ResourceDescriptor lDescriptor;
        rw::LinearResourceAllocator::GetResourceDescriptor(&lDescriptor, &mGlobalGraphicsMemoryDescriptor);
        mGlobalGraphicsMemory.m_baseResources[0] = XPhysicalAlloc(
            lDescriptor.m_baseResourceDescriptors[0].m_size, 0xFFFFFFFFUL, 4096, 0x20000004UL);
        mGlobalGraphicsMemory.m_baseResources[2] = XPhysicalAlloc(
            lDescriptor.m_baseResourceDescriptors[2].m_size, 0xFFFFFFFFUL, 4096, 0x20000404UL);
        ClearResource(mGlobalGraphicsMemory, lDescriptor);
        mpInternalGraphicsAllocator = rw::LinearResourceAllocator::Initialize(
            mGlobalGraphicsMemory, mGlobalGraphicsMemoryDescriptor);
        mGlobalGraphicsAllocator.Construct(mpInternalGraphicsAllocator, "GlobalGraphics");
    }

    // ARTIST826651C0: real descriptor validation, diagnostics, allocation and
    // per-requested-lane pointer/alignment validation. No native arena fallback.
    rw::Resource LinearResourceAllocator::DoAllocate(
        const rw::ResourceDescriptor& lrDescriptor, const char* lpcName)
    {
        const char* const lpcFile = "..\\..\\..\\GameSource\\Resource/BrnResourceAllocator.cpp";
        const auto Assert = [lpcFile](bool lbCondition, const char* lpcMessage, s32 liLine)
        {
            if (!lbCondition)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(lpcMessage, lpcFile, liLine);
                CgsDev::Assert::EndAssert();
            }
        };
        Assert(lrDescriptor.m_baseResourceDescriptors[3].m_size == 0,
               "We don't support unitialized memory\n", 1293);
        Assert(lrDescriptor.m_baseResourceDescriptors[4].m_size == 0,
               "We don't support disposable unitialized memory\n", 1294);
        Assert(lrDescriptor.m_baseResourceDescriptors[1].m_size == 0,
               "We don't support disposable memory\n", 1295);
        Assert(mpLinearAllocator != 0, "General allocator not set\n", 1305);

        const rw::ResourceDescriptor lAvailable = mpLinearAllocator->GetAvailable();
        if (CgsDev::Message::gxMessageFilterFlags & 1)
        {
            if (mpcName)
                *CgsDev::Log::gpDebugPrint << "Executing allocations into " << mpcName << "\n";
            else
                *CgsDev::Log::gpDebugPrint << "Executing allocations: \n";
        }
        // Exact five pointer-table strings at ARTIST8209FAD0, read from primary.
        const char* const lapcMemoryNames[5] = {
            "main memory", "disposable", "physical", "uninitialized", "disposable uninitialized" };
        char lacMessage[416];
        for (u32 luLane = 0; luLane < rw::KU_RESOURCE_LANE_COUNT; ++luLane)
        {
            const u32 luSize = lrDescriptor.m_baseResourceDescriptors[luLane].m_size;
            if (luSize)
            {
                CgsCore::SPrintf(lacMessage, 256, "Size:  %umb %ukb %ub of %s\n",
                    luSize >> 20, (luSize >> 10) & 1023u, luSize & 1023u, lapcMemoryNames[luLane]);
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                    *CgsDev::Log::gpDebugPrint << lacMessage;
                const u32 luFree = lAvailable.m_baseResourceDescriptors[luLane].m_size;
                CgsCore::SPrintf(lacMessage, 256, "Free:  %umb %ukb %ub of %s\n",
                    luFree >> 20, (luFree >> 10) & 1023u, luFree & 1023u, lapcMemoryNames[luLane]);
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                    *CgsDev::Log::gpDebugPrint << lacMessage;
            }
        }
        rw::Resource lResource = mpLinearAllocator->DoAllocate(lrDescriptor, lpcName);
        for (u32 luLane = 0; luLane < rw::KU_RESOURCE_LANE_COUNT; ++luLane)
        {
            if (lrDescriptor.m_baseResourceDescriptors[luLane].m_size)
            {
                if (!lResource.m_baseResources[luLane])
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::StrStream lStream(lBuffer.GetBuffer(), lBuffer.GetSize());
                    lStream << "Failed to allocate memory type " << lapcMemoryNames[luLane] << "\n";
                    CgsDev::Assert::FireAssert(lBuffer.GetBuffer(), lpcFile, 1355);
                    CgsDev::Assert::EndAssert();
                }
                const u32 luAlignment = lrDescriptor.m_baseResourceDescriptors[luLane].m_alignment;
                // The original zero-divisor trap is retained by the modulo operation;
                // no fallback alignment or failed-allocation early return is added.
                if (reinterpret_cast<uintptr_t>(lResource.m_baseResources[luLane]) % luAlignment)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::StrStream lStream(lBuffer.GetBuffer(), lBuffer.GetSize());
                    lStream << "Allocated memory is not aligned to " << luAlignment << "bytes\n";
                    CgsDev::Assert::FireAssert(lBuffer.GetBuffer(), lpcFile, 1358);
                    CgsDev::Assert::EndAssert();
                }
            }
        }
        return lResource;
    }

    void LinearResourceAllocator::DoFree(const rw::Resource& lrResource)
    {
        const auto Assert = [](bool lbCondition, const char* lpcMessage, s32 liLine)
        {
            if (!lbCondition)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(lpcMessage,
                    "..\\..\\..\\GameSource\\Resource/BrnResourceAllocator.cpp", liLine);
                CgsDev::Assert::EndAssert();
            }
        };
        Assert(lrResource.m_baseResources[3] == 0, "We don't support unitialized memory\n", 1385);
        Assert(lrResource.m_baseResources[4] == 0, "We don't support disposable unitialized memory\n", 1386);
        Assert(lrResource.m_baseResources[1] == 0, "We don't support disposable memory\n", 1387);
        Assert(mpLinearAllocator != 0, "General allocator not set\n", 1397);
        mpLinearAllocator->DoFree(lrResource);
    }

    void LinearResourceAllocator::DoFreeDisposable(rw::Resource& /*lrResource*/)
    {
        if (!mpLinearAllocator)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("General allocator not set\n",
                "..\\..\\..\\GameSource\\Resource/BrnResourceAllocator.cpp", 1414);
            CgsDev::Assert::EndAssert();
        }
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Should never be allocating or freeing disposable memory!!!\n",
            "..\\..\\..\\GameSource\\Resource/BrnResourceAllocator.cpp", 1415);
        CgsDev::Assert::EndAssert();
    }
}
