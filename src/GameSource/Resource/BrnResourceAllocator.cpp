#include "GameSource/Resource/BrnResourceAllocator.h"
#include "GameSource/Resource/BrnMemoryMapData.h"  // MemoryMapImageInitializer
#include "rw/rwcore_general_alloc.h"       // rw::core::GeneralResourceAllocator (GetGameDataGeneralAllocator)
#include "GameShared/GameClasses/Memory/PC/CgsLowMemoryPC.h"   // LowMemory::Reserve (low-4GB root block)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::gpDebugPrint, CgsDev::Message::gxMessageFilterFlags
#include <new>   // std::nothrow (the GlobalGraphics arena carve)
#include <malloc.h>   // _aligned_malloc (DefaultLinearAllocator PC backing)
#include <Windows.h>   // GlobalMemoryStatus (PrintConsoleMemory / GetAvailableMemory)
#include <cstring>

// Existing platform owner: GameShared/GameClasses/System/PC/CgsXboxPlatformPC.cpp.
extern "C" void* XPhysicalAlloc(unsigned long luSize, unsigned long luPhysicalAddress,
                                unsigned long luAlignment, unsigned long luProtect);

// ARTIST owns one internal allocator pointer and one public named wrapper per global.
// Native lazy backing uses these same roles until the recovered Construct is activated.
namespace BrnResource
{
    namespace
    {
        // 768 MB root debug heap. [PC-LEAF] the X360 reserves a fixed ~smaller budget split across banks;
        // the PC bring-up routes the 27 pools' memory (~266 MB) through the MemoryModule's root banks (a
        // ~384 MB type0+type1 backing carved from here), with a debug-heap FALLBACK if a bank carve fails
        // (another ~266 MB) -- so this is sized to hold the bank backing + a full fallback at once.
        const size_t KN_DEBUG_HEAP_SIZE = 768u * 1024u * 1024u;

        rw::core::GeneralResourceAllocator sNativeDebugAllocator;
        bool                            s_bDebugHeapBacked = false;

        MEMORYSTATUSEX GetHostMemoryStatusPC()
        {
            // FLAG PC-platform leaf: query the host's full-width virtual, physical and page-file status.
            MEMORYSTATUSEX lMemoryStatus = {};
            lMemoryStatus.dwLength = static_cast<DWORD>(sizeof(lMemoryStatus));
            ::GlobalMemoryStatusEx(&lMemoryStatus);
            return lMemoryStatus;
        }
    }

    // FLAG PC-platform leaf: the existing native debug heap object is available
    // before its low-memory backing is reserved. This is an internal General pointer,
    // never an alias for the public Heap wrapper. Construct later replaces it normally.
    rw::core::GeneralResourceAllocator* Allocators::mpInternalDebugAllocator = &sNativeDebugAllocator;
    DefaultLinearAllocator Allocators::mInitialDefaultAllocator;
    rw::Resource Allocators::mGlobalSystemMemory;
    rw::ResourceDescriptor Allocators::mGlobalSystemMemoryDescriptor;
    rw::core::GeneralResourceAllocator* Allocators::mpInternalGlobalSystemAllocator = nullptr;
    HeapResourceAllocator Allocators::mGlobalSystemAllocator;
    rw::Resource Allocators::mGlobalDebugMemory;
    rw::ResourceDescriptor Allocators::mGlobalDebugMemoryDescriptor;
    HeapResourceAllocator Allocators::mGlobalDebugAllocator;
    rw::Resource Allocators::mGlobalGraphicsMemory;
    rw::ResourceDescriptor Allocators::mGlobalGraphicsMemoryDescriptor;
    rw::LinearResourceAllocator* Allocators::mpInternalGraphicsAllocator = nullptr;
    LinearResourceAllocator Allocators::mGlobalGraphicsAllocator;
    rw::Resource Allocators::mGlobalResourceMemory;
    rw::ResourceDescriptor Allocators::mGlobalResourceMemoryDescriptor;
    rw::LinearResourceAllocator* Allocators::mpInternalResourceAllocator = nullptr;
    LinearResourceAllocator Allocators::mGlobalResourceAllocator;
    HeapResourceAllocator Allocators::mRuntimeAllocator;
    LinearResourceAllocator Allocators::mGlobalMemFileAllocator;

    void Allocators::EnsureNativeDebugBacking()
    {
        if (mpInternalDebugAllocator != &sNativeDebugAllocator)
            return;
        if (!s_bDebugHeapBacked)
        {
            // FLAG PC-platform leaf: preserve the approved 768 MiB low-4GB arena.
            // Four-byte serialized pointers require its low addresses. This native
            // object lives separately, so AdoptMemory must not skip an in-arena header.
            rw::Resource lBacking;
            lBacking.m_baseResources[0] = CgsMemory::LowMemory::Reserve(KN_DEBUG_HEAP_SIZE);
            rw::ResourceDescriptor lCapacity;
            lCapacity.m_baseResourceDescriptors[0].m_size =
                lBacking.m_baseResources[0] ? static_cast<u32>(KN_DEBUG_HEAP_SIZE) : 0u;
            lCapacity.m_baseResourceDescriptors[0].m_alignment = 16;
            sNativeDebugAllocator.AdoptMemory(lBacking, lCapacity);
            s_bDebugHeapBacked = lBacking.m_baseResources[0] != nullptr;
        }
        // Native bring-up keeps its existing quiet debug heap. The recovered
        // engine constructor supplies the original print level two instead.
        mGlobalDebugAllocator.Construct(mpInternalDebugAllocator, "GlobalDebug", 0);
    }

    HeapResourceAllocator* GetDebugAllocator()
    {
        Allocators::EnsureNativeDebugBacking();
        CGS_ASSERT(Allocators::mpInternalDebugAllocator, "Allocators::mpInternalDebugAllocator");
        return &Allocators::mGlobalDebugAllocator;
    }

    LinearResourceAllocator* GetResourceAllocator()
    {
        CGS_ASSERT(Allocators::mpInternalResourceAllocator, "Allocators::mpInternalResourceAllocator");
        return &Allocators::mGlobalResourceAllocator;
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
            (void)GetDebugAllocator();
            void* lpRegion = Allocators::mpInternalDebugAllocator->Alloc(0, KN_GAMEDATA_GENERAL_SIZE, 16);
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
                s_bGameDataGeneralInit = true;
            }
            else
                s_GameDataGeneralAllocator.m_mainAllocator.SetOption(
                    EA::Allocator::GeneralAllocator::kOptionEnableSystemAlloc, 0);
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

        void LogAllocatorMemoryBreakdown(u32 luSize, const char* lpcName, const char* lpcPrefix = "  ")
        {
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint << lpcPrefix << (luSize / 0x100000u) << "mb "
                    << ((luSize % 0x100000u) / 0x400u) << "kb " << (luSize % 0x400u)
                    << "b of " << lpcName << "\n";
            }
        }

        void FireResourceAllocatorAssert(const char* lpcMessage, s32 liLine)
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

    // ARTIST 0x82661AF0; DecFIGS confirms the public void() member shape. The console
    // checks the five X360 lanes in order, then asserts when main or physical usage exceeds
    // the fixed startup budgets. The raw console stride is 0x1c; this uses the typed array.
    void DefaultLinearAllocator::CheckRenderwareBudget()
    {
        if ((CgsDev::Message::gxMessageFilterFlags & CgsDev::Message::KX_FILTER_GLOBAL) != 0)
            *CgsDev::Log::gpDebugPrint << "Render budget used:\n";

        for (s32 liType = 0; liType < 5; ++liType)
        {
            const size_t luUsage = maAllocators[liType].GetUsage();
            if (luUsage != 0)
                LogAllocatorMemoryBreakdown(
                    static_cast<u32>(luUsage), KAC_DEFAULT_LINEAR_RESOURCE_NAMES[liType]);
        }

        if (maAllocators[0].GetUsage() > 0x800u)
            FireResourceAllocatorAssert("Renderware over budget in main memory\n", 825);

        if (maAllocators[2].GetUsage() > 0x400000u)
            FireResourceAllocatorAssert("Renderware over budget in physical memory\n", 826);
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
                LogAllocatorMemoryBreakdown(luSize, KAC_DEFAULT_LINEAR_RESOURCE_NAMES[liType]);
        }

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "Remaining pre-allocation\n";
        for (s32 liType = 0; liType < 5; ++liType)
        {
            const u32 luFree = static_cast<u32>(maAllocators[liType].GetFreeMemory());
            if (luFree != 0)
                LogAllocatorMemoryBreakdown(luFree, KAC_DEFAULT_LINEAR_RESOURCE_NAMES[liType]);
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
                    FireResourceAllocatorAssert("Failed to allocate memory\n", 948);
            }
        }

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "Remaining post-allocation\n";
        for (s32 liType = 0; liType < 5; ++liType)
        {
            const u32 luFree = static_cast<u32>(maAllocators[liType].GetFreeMemory());
            if (luFree != 0)
                LogAllocatorMemoryBreakdown(luFree, KAC_DEFAULT_LINEAR_RESOURCE_NAMES[liType]);
        }
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "-----------------------------------------\n";
        return lResource;
    }

    // ARTIST 0x82661CF8: this initial global allocator is monotonic and cannot free a block.
    void DefaultLinearAllocator::DoFree(const rw::Resource& /*lrResource*/)
    {
        FireResourceAllocatorAssert(
            "Should never be freeing memory from the global allocator\n", 984);
    }

    // ARTIST 0x82661D88: disposable allocation/free is not supported on this allocator.
    void DefaultLinearAllocator::DoFreeDisposable(rw::Resource& /*lrResource*/)
    {
        FireResourceAllocatorAssert(
            "Should never be allocating or freeing disposable memory!!!\n", 999);
    }

    // Out-of-line defaulted destructor: anchors the DefaultLinearAllocator vtable in this TU so
    // the compiler emits its polymorphic-delete thunk (the X360 `scalar deleting destructor'
    // @0x82666948). The underlying object teardown is trivial (only the rw::IResourceAllocator
    // base) -- the X360 thunk just rewrites the vptr and conditionally operator-deletes.
    DefaultLinearAllocator::~DefaultLinearAllocator() = default;

    namespace
    {
        void AssertSupportedDescriptor(const rw::ResourceDescriptor& lrDescriptor, s32 liFirstLine)
        {
            if (lrDescriptor.m_baseResourceDescriptors[3].m_size != 0)
                FireResourceAllocatorAssert("We don't support unitialized memory\n", liFirstLine);
            if (lrDescriptor.m_baseResourceDescriptors[4].m_size != 0)
                FireResourceAllocatorAssert("We don't support disposable unitialized memory\n", liFirstLine + 1);
            if (lrDescriptor.m_baseResourceDescriptors[1].m_size != 0)
                FireResourceAllocatorAssert("We don't support disposable memory\n", liFirstLine + 2);
        }

        void AssertSupportedResource(const rw::Resource& lrResource, s32 liFirstLine)
        {
            if (lrResource.m_baseResources[3] != nullptr)
                FireResourceAllocatorAssert("We don't support unitialized memory\n", liFirstLine);
            if (lrResource.m_baseResources[4] != nullptr)
                FireResourceAllocatorAssert("We don't support disposable unitialized memory\n", liFirstLine + 1);
            if (lrResource.m_baseResources[1] != nullptr)
                FireResourceAllocatorAssert("We don't support disposable memory\n", liFirstLine + 2);
        }

        void LogAllocatorName(const char* lpcName)
        {
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                if (lpcName)
                    *CgsDev::Log::gpDebugPrint << "Executing allocations into " << lpcName << "\n";
                else
                    *CgsDev::Log::gpDebugPrint << "Executing allocations: \n";
            }
        }

        void AssertResourceAllocated(const char* lpcType, s32 liLine)
        {
            char lacMessage[256];
            CgsDev::StrStream lStream(lacMessage, sizeof(lacMessage));
            lStream << "Failed to allocate memory type " << lpcType << "\n";
            FireResourceAllocatorAssert(lacMessage, liLine);
        }
    }

    HeapResourceAllocator::HeapResourceAllocator()
        : mpGeneralAllocator(nullptr), mpcName(nullptr), miPrintLevel(0)
    {
    }

    // The three stores in ARTIST 0x82666A54 / 0x82666B58 / 0x82666E48.
    void HeapResourceAllocator::Construct(rw::core::GeneralResourceAllocator* lpAllocator,
                                          const char* lpcName, s32 liPrintLevel)
    {
        mpGeneralAllocator = lpAllocator;
        mpcName = lpcName;
        miPrintLevel = liPrintLevel;
    }

    rw::Resource HeapResourceAllocator::Allocate(const rw::ResourceDescriptor& lrDescriptor,
                                                const char* lpcName)
    {
        return DoAllocate(lrDescriptor, lpcName);
    }

    // ARTIST 0x82664BF0. The public wrapper validates supported lanes and delegates
    // ownership to its internal General allocator, including the physical lane.
    rw::Resource HeapResourceAllocator::DoAllocate(const rw::ResourceDescriptor& lrDescriptor,
                                                   const char* lpcName)
    {
        if (this == &Allocators::mGlobalDebugAllocator)
            Allocators::EnsureNativeDebugBacking();
        AssertSupportedDescriptor(lrDescriptor, 1101);
        if (mpGeneralAllocator == nullptr)
            FireResourceAllocatorAssert("General allocator not set\n", 1113);
        if (miPrintLevel > 0 && miPrintLevel <= 2)
        {
            LogAllocatorName(mpcName);
            for (u32 luLane = 0; luLane < rw::KU_RESOURCE_LANE_COUNT; ++luLane)
                if (lrDescriptor.m_baseResourceDescriptors[luLane].m_size != 0)
                    LogAllocatorMemoryBreakdown(lrDescriptor.m_baseResourceDescriptors[luLane].m_size,
                                                KAC_DEFAULT_LINEAR_RESOURCE_NAMES[luLane]);
        }
        rw::Resource lResult = mpGeneralAllocator->DoAllocate(lrDescriptor, lpcName);
        for (u32 luLane = 0; luLane < rw::KU_RESOURCE_LANE_COUNT; ++luLane)
            if (lrDescriptor.m_baseResourceDescriptors[luLane].m_size != 0
                && lResult.m_baseResources[luLane] == nullptr)
                AssertResourceAllocated(KAC_DEFAULT_LINEAR_RESOURCE_NAMES[luLane], 1153);
        return lResult;
    }

    // ARTIST 0x82664FD8.
    void HeapResourceAllocator::DoFree(const rw::Resource& lrResource)
    {
        AssertSupportedResource(lrResource, 1180);
        if (mpGeneralAllocator == nullptr)
            FireResourceAllocatorAssert("General allocator not set\n", 1192);
        mpGeneralAllocator->DoFree(lrResource);
    }

    // ARTIST 0x82661E18.
    void HeapResourceAllocator::DoFreeDisposable(rw::Resource& /*lrResource*/)
    {
        if (mpGeneralAllocator == nullptr)
            FireResourceAllocatorAssert("General allocator not set\n", 1209);
        FireResourceAllocatorAssert("Should never be allocating or freeing disposable memory!!!\n", 1210);
    }

    LinearResourceAllocator::LinearResourceAllocator()
        : mpLinearAllocator(nullptr), mpcName(nullptr)
    {
    }

    // The two stores in ARTIST 0x82666C48 / 0x82666D38 / 0x82666F18.
    void LinearResourceAllocator::Construct(rw::LinearResourceAllocator* lpAllocator, const char* lpcName)
    {
        mpLinearAllocator = lpAllocator;
        mpcName = lpcName;
    }

    // ARTIST 0x826651C0.
    rw::Resource LinearResourceAllocator::DoAllocate(const rw::ResourceDescriptor& lrDescriptor,
                                                     const char* lpcName)
    {
        AssertSupportedDescriptor(lrDescriptor, 1293);
        if (mpLinearAllocator == nullptr)
            FireResourceAllocatorAssert("General allocator not set\n", 1305);
        const rw::ResourceDescriptor lCapacity = mpLinearAllocator->GetCapacity();
        const rw::ResourceDescriptor lUsage = mpLinearAllocator->GetCurrentUsage();
        LogAllocatorName(mpcName);
        for (u32 luLane = 0; luLane < rw::KU_RESOURCE_LANE_COUNT; ++luLane)
        {
            const u32 luSize = lrDescriptor.m_baseResourceDescriptors[luLane].m_size;
            if (luSize != 0)
            {
                LogAllocatorMemoryBreakdown(luSize, KAC_DEFAULT_LINEAR_RESOURCE_NAMES[luLane], "Size:  ");
                LogAllocatorMemoryBreakdown(lCapacity.m_baseResourceDescriptors[luLane].m_size
                    - lUsage.m_baseResourceDescriptors[luLane].m_size,
                    KAC_DEFAULT_LINEAR_RESOURCE_NAMES[luLane], "Free:  ");
            }
        }
        rw::Resource lResult = mpLinearAllocator->DoAllocate(lrDescriptor, lpcName);
        for (u32 luLane = 0; luLane < rw::KU_RESOURCE_LANE_COUNT; ++luLane)
        {
            if (lrDescriptor.m_baseResourceDescriptors[luLane].m_size == 0)
                continue;
            if (lResult.m_baseResources[luLane] == nullptr)
                AssertResourceAllocated(KAC_DEFAULT_LINEAR_RESOURCE_NAMES[luLane], 1355);
            const u32 luAlignment = lrDescriptor.m_baseResourceDescriptors[luLane].m_alignment;
            if (reinterpret_cast<uintptr_t>(lResult.m_baseResources[luLane]) % luAlignment != 0)
            {
                char lacMessage[256];
                CgsDev::StrStream lStream(lacMessage, sizeof(lacMessage));
                lStream << "Allocated memory is not aligned to " << luAlignment << "bytes\n";
                FireResourceAllocatorAssert(lacMessage, 1358);
            }
        }
        return lResult;
    }

    // ARTIST 0x82665708.
    void LinearResourceAllocator::DoFree(const rw::Resource& lrResource)
    {
        AssertSupportedResource(lrResource, 1385);
        if (mpLinearAllocator == nullptr)
            FireResourceAllocatorAssert("General allocator not set\n", 1397);
        mpLinearAllocator->DoFree(lrResource);
    }

    // ARTIST 0x82661F10.
    void LinearResourceAllocator::DoFreeDisposable(rw::Resource& /*lrResource*/)
    {
        if (mpLinearAllocator == nullptr)
            FireResourceAllocatorAssert("General allocator not set\n", 1414);
        FireResourceAllocatorAssert("Should never be allocating or freeing disposable memory!!!\n", 1415);
    }

    namespace
    {
        rw::LinearResourceAllocator sNativeGraphicsAllocator;
        const u32 KU_GLOBAL_GRAPHICS_ARENA_BYTES = 256u * 1024u;
    }

    rw::IResourceAllocator* Allocators::GetGlobalGraphicsAllocator()
    {
        if (mpInternalGraphicsAllocator == nullptr)
        {
            // FLAG PC-platform leaf: preserve the existing lazy 256 KiB CPU arena.
            // Its object lives outside the pool. This establishes no GPU/DMA equivalence.
            void* lpHeap = ::operator new(KU_GLOBAL_GRAPHICS_ARENA_BYTES, std::nothrow);
            if (lpHeap == nullptr)
                return nullptr;
            rw::Resource lResource;
            lResource.m_baseResources[0] = lpHeap;
            rw::ResourceDescriptor lCapacity;
            lCapacity.m_baseResourceDescriptors[0].m_size = KU_GLOBAL_GRAPHICS_ARENA_BYTES;
            lCapacity.m_baseResourceDescriptors[0].m_alignment = 16;
            sNativeGraphicsAllocator.AdoptMemory(lResource, lCapacity);
            mpInternalGraphicsAllocator = &sNativeGraphicsAllocator;
            mGlobalGraphicsAllocator.Construct(mpInternalGraphicsAllocator, "GlobalGraphics");
        }
        return &mGlobalGraphicsAllocator;
    }

    // ARTIST 0x826647B0 fills every populated allocation, including its object reserve.
    void ClearResource(rw::Resource& lrResource, const rw::ResourceDescriptor& lrDescriptor)
    {
        for (u32 luLane = 0; luLane < rw::KU_RESOURCE_LANE_COUNT; ++luLane)
            if (lrResource.m_baseResources[luLane] != nullptr)
                std::memset(lrResource.m_baseResources[luLane], 0xCD,
                            lrDescriptor.m_baseResourceDescriptors[luLane].m_size);
    }

    // ARTIST 0x82666990. Descriptor capacity excludes the placement-object reserve.
    void Allocators::ConstructGlobalSystemMemory()
    {
        mGlobalSystemMemoryDescriptor.m_baseResourceDescriptors[0].m_size = 0x1B04000;
        mGlobalSystemMemoryDescriptor.m_baseResourceDescriptors[0].m_alignment = 0x10000;
        mGlobalSystemMemoryDescriptor.m_baseResourceDescriptors[2].m_size = 0;
        mGlobalSystemMemoryDescriptor.m_baseResourceDescriptors[2].m_alignment = 0x10000;
        rw::ResourceDescriptor lExtended;
        rw::core::GeneralResourceAllocator::GetResourceDescriptor(&lExtended, &mGlobalSystemMemoryDescriptor);
        mGlobalSystemMemory.m_baseResources[0] =
            XPhysicalAlloc(lExtended.m_baseResourceDescriptors[0].m_size, ~0ul, 0x10000, 0x20000004);
        ClearResource(mGlobalSystemMemory, lExtended);
        mpInternalGlobalSystemAllocator =
            rw::core::GeneralResourceAllocator::Initialize(mGlobalSystemMemory, mGlobalSystemMemoryDescriptor);
        mGlobalSystemAllocator.Construct(mpInternalGlobalSystemAllocator, "GlobalSystem", 1);
    }

    // ARTIST 0x82666A80. Physical lane two uses the distinct 0x20000404 protection flags.
    void Allocators::ConstructGlobalDebugMemory()
    {
        mGlobalDebugMemoryDescriptor.m_baseResourceDescriptors[0].m_size = 0x1240000;
        mGlobalDebugMemoryDescriptor.m_baseResourceDescriptors[0].m_alignment = 0x10000;
        mGlobalDebugMemoryDescriptor.m_baseResourceDescriptors[2].m_size = 0x100000;
        mGlobalDebugMemoryDescriptor.m_baseResourceDescriptors[2].m_alignment = 0x10000;
        rw::ResourceDescriptor lExtended;
        rw::core::GeneralResourceAllocator::GetResourceDescriptor(&lExtended, &mGlobalDebugMemoryDescriptor);
        mGlobalDebugMemory.m_baseResources[0] =
            XPhysicalAlloc(lExtended.m_baseResourceDescriptors[0].m_size, ~0ul, 0x10000, 0x20000004);
        mGlobalDebugMemory.m_baseResources[2] =
            XPhysicalAlloc(lExtended.m_baseResourceDescriptors[2].m_size, ~0ul, 0x10000, 0x20000404);
        ClearResource(mGlobalDebugMemory, lExtended);
        mpInternalDebugAllocator =
            rw::core::GeneralResourceAllocator::Initialize(mGlobalDebugMemory, mGlobalDebugMemoryDescriptor);
        mGlobalDebugAllocator.Construct(mpInternalDebugAllocator, "GlobalDebug", 2);
    }

    // ARTIST 0x82666B70.
    void Allocators::ConstructGlobalGraphicsMemory()
    {
        mGlobalGraphicsMemoryDescriptor.m_baseResourceDescriptors[0].m_size = 0x640000;
        mGlobalGraphicsMemoryDescriptor.m_baseResourceDescriptors[0].m_alignment = 0x1000;
        mGlobalGraphicsMemoryDescriptor.m_baseResourceDescriptors[2].m_size = 0x1D00000;
        mGlobalGraphicsMemoryDescriptor.m_baseResourceDescriptors[2].m_alignment = 0x1000;
        rw::ResourceDescriptor lExtended;
        rw::LinearResourceAllocator::GetResourceDescriptor(&lExtended, &mGlobalGraphicsMemoryDescriptor);
        mGlobalGraphicsMemory.m_baseResources[0] =
            XPhysicalAlloc(lExtended.m_baseResourceDescriptors[0].m_size, ~0ul, 0x1000, 0x20000004);
        mGlobalGraphicsMemory.m_baseResources[2] =
            XPhysicalAlloc(lExtended.m_baseResourceDescriptors[2].m_size, ~0ul, 0x1000, 0x20000404);
        ClearResource(mGlobalGraphicsMemory, lExtended);
        mpInternalGraphicsAllocator =
            rw::LinearResourceAllocator::Initialize(mGlobalGraphicsMemory, mGlobalGraphicsMemoryDescriptor);
        mGlobalGraphicsAllocator.Construct(mpInternalGraphicsAllocator, "GlobalGraphics");
    }

    // ARTIST 0x82666C58. The map's first Root Bank Params contributes lanes zero/two.
    // The descriptors align at 64 KiB, while these backing allocations align at 128 KiB.
    void Allocators::ConstructGlobalResourceMemory(const CgsMemory::MemoryBank::Params* lpParams)
    {
        mGlobalResourceMemoryDescriptor.m_baseResourceDescriptors[0].m_size = lpParams->mauBankSize[0] + 0xFC0000;
        mGlobalResourceMemoryDescriptor.m_baseResourceDescriptors[0].m_alignment = 0x10000;
        mGlobalResourceMemoryDescriptor.m_baseResourceDescriptors[2].m_size = lpParams->mauBankSize[2] + 0x200000;
        mGlobalResourceMemoryDescriptor.m_baseResourceDescriptors[2].m_alignment = 0x10000;
        rw::ResourceDescriptor lExtended;
        rw::LinearResourceAllocator::GetResourceDescriptor(&lExtended, &mGlobalResourceMemoryDescriptor);
        mGlobalResourceMemory.m_baseResources[0] =
            XPhysicalAlloc(lExtended.m_baseResourceDescriptors[0].m_size, ~0ul, 0x20000, 0x20000004);
        mGlobalResourceMemory.m_baseResources[2] =
            XPhysicalAlloc(lExtended.m_baseResourceDescriptors[2].m_size, ~0ul, 0x20000, 0x20000004);
        ClearResource(mGlobalResourceMemory, lExtended);
        mpInternalResourceAllocator =
            rw::LinearResourceAllocator::Initialize(mGlobalResourceMemory, mGlobalResourceMemoryDescriptor);
        mGlobalResourceAllocator.Construct(mpInternalResourceAllocator, "GlobalResource");
    }

    // ARTIST 0x82666D48. Runtime is owned by GlobalSystem; MemFile has its own
    // two physical backing allocations. No extra internal-pointer alias is introduced.
    void Allocators::ConstructChildAllocators()
    {
        rw::ResourceDescriptor lRuntimeCapacity;
        lRuntimeCapacity.m_baseResourceDescriptors[0].m_size = 0x1B00000;
        lRuntimeCapacity.m_baseResourceDescriptors[0].m_alignment = 16;
        if (mpInternalGlobalSystemAllocator == nullptr)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Allocators::mpInternalGlobalSystemAllocator",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\resource\\BrnResourceAllocator.h", 362);
            CgsDev::Assert::EndAssert();
        }
        rw::ResourceDescriptor lRuntimeExtended;
        rw::core::GeneralResourceAllocator::GetResourceDescriptor(&lRuntimeExtended, &lRuntimeCapacity);
        rw::Resource lRuntimeMemory = mGlobalSystemAllocator.DoAllocate(lRuntimeExtended, nullptr);
        ClearResource(lRuntimeMemory, lRuntimeExtended);
        mRuntimeAllocator.Construct(
            rw::core::GeneralResourceAllocator::Initialize(lRuntimeMemory, lRuntimeCapacity), "Runtime", 1);

        rw::ResourceDescriptor lMemFileCapacity;
        lMemFileCapacity.m_baseResourceDescriptors[0].m_size = 0x20000;
        lMemFileCapacity.m_baseResourceDescriptors[0].m_alignment = 0x10000;
        lMemFileCapacity.m_baseResourceDescriptors[2].m_size = 0x300000;
        lMemFileCapacity.m_baseResourceDescriptors[2].m_alignment = 0x10000;
        rw::ResourceDescriptor lMemFileExtended;
        rw::LinearResourceAllocator::GetResourceDescriptor(&lMemFileExtended, &lMemFileCapacity);
        rw::Resource lMemFileMemory;
        lMemFileMemory.m_baseResources[0] =
            XPhysicalAlloc(lMemFileExtended.m_baseResourceDescriptors[0].m_size, ~0ul, 0x10000, 0x20000004);
        lMemFileMemory.m_baseResources[2] =
            XPhysicalAlloc(lMemFileExtended.m_baseResourceDescriptors[2].m_size, ~0ul, 0x10000, 0x20000004);
        // Unlike the other globals, this helper does not call ClearResource for MemFile.
        mGlobalMemFileAllocator.Construct(
            rw::LinearResourceAllocator::Initialize(lMemFileMemory, lMemFileCapacity), nullptr);
    }

    // ARTIST 0x8266AED8. This recovered engine sequence remains uninvoked by the
    // native startup adapter. The existing XPhysicalAlloc host leaf supplies aligned
    // CPU memory only; it does not establish the original graphics/GPU/DMA contract.
    void Allocators::Construct()
    {
        PrintConsoleMemory("Pre-allocators construction");
        mInitialDefaultAllocator.CheckRenderwareBudget();
        const CgsMemory::MemoryBank::Params* const lpRootParams =
            MemoryMapImageInitializer::GetMemoryMap().GetBinaryMemoryMapBank()->GetParams();
        ConstructGlobalSystemMemory();
        ConstructGlobalDebugMemory();
        ConstructGlobalGraphicsMemory();
        ConstructGlobalResourceMemory(lpRootParams);
        ConstructChildAllocators();
        rw::ResourceAllocatorRegistry::SetDefaultAllocator(&mGlobalSystemAllocator);
        PrintConsoleMemory("Post-allocators construction");
    }
}
