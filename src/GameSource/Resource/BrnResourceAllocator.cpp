#include "GameSource/Resource/BrnResourceAllocator.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include <cstring>
#include "GameSource/Resource/BrnMemoryMapData.h"  // MemoryMapImageInitializer
#include "ppmalloc/EAGeneralAllocator.h"   // the backing EA general allocator
#include "rw/rwcore_general_alloc.h"       // native embedded-heap allocator
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::gpDebugPrint, CgsDev::Message::gxMessageFilterFlags
#include <xtl.h>   // GlobalMemoryStatus (PrintConsoleMemory / GetAvailableMemory)

namespace BrnResource
{
    namespace
    {
        void CheckResourceAllocator(bool lbCondition, const char* lpcMessage, s32 liLine)
        {
            if (!lbCondition)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::StrStream lStream(CgsDev::Assert::gpcMessageBuffer,
                                         CgsDev::Assert::giMessageBufferSize);
                lStream << lpcMessage;
                CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer,
                    "..\\..\\..\\GameSource\\Resource/BrnResourceAllocator.cpp", liLine);
                CgsDev::Assert::EndAssert();
            }
        }
    }

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

    rw::core::GeneralResourceAllocator* Allocators::mpInternalDebugAllocator = 0;
    rw::core::GeneralResourceAllocator* Allocators::mpInternalGlobalSystemAllocator = 0;
    rw::LinearResourceAllocator* Allocators::mpInternalResourceAllocator = 0;
    DefaultLinearAllocator Allocators::mInitialDefaultAllocator;
    HeapResourceAllocator Allocators::mGlobalSystemAllocator;
    HeapResourceAllocator Allocators::mGlobalDebugAllocator;
    rw::Resource Allocators::mGlobalSystemMemory;
    rw::ResourceDescriptor Allocators::mGlobalSystemMemoryDescriptor;
    rw::Resource Allocators::mGlobalDebugMemory;
    rw::ResourceDescriptor Allocators::mGlobalDebugMemoryDescriptor;
    rw::Resource Allocators::mGlobalResourceMemory;
    rw::ResourceDescriptor Allocators::mGlobalResourceMemoryDescriptor;

    // ARTIST823A7CB8: the backing check does not initialize or replace the heap.
    HeapResourceAllocator* GetDebugAllocator()
    {
        if (!Allocators::mpInternalDebugAllocator)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Allocators::mpInternalDebugAllocator",
                "..\\..\\..\\GameSource\\Resource/BrnResourceAllocator.h", 369);
            CgsDev::Assert::EndAssert();
        }
        return &Allocators::mGlobalDebugAllocator;
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

        maAllocators[0].Create(XPhysicalAlloc(0x800u, MAXULONG_PTR, 0x10000u, PAGE_READWRITE), 0x800u);
        maAllocators[2].Create(XPhysicalAlloc(0x400000u, MAXULONG_PTR, 0x10000u,
                                            PAGE_READWRITE | PAGE_WRITECOMBINE), 0x400000u);
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

    rw::IResourceAllocator* Allocators::GetGlobalGraphicsAllocator()
    {
        return &mGlobalGraphicsAllocator;
    }

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

    // ARTIST82666B70: the native graphics heap and wrapper.
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


    // ARTIST82661AF0: report all five lanes and enforce the two fixed boot budgets.
    void DefaultLinearAllocator::CheckRenderwareBudget()
    {
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "Render budget used:\n";
        char lacMessage[256];
        for (s32 liLane = 0; liLane < 5; ++liLane)
        {
            const u32 luUsage = static_cast<u32>(maAllocators[liLane].GetUsage());
            if (luUsage)
            {
                CgsCore::SPrintf(lacMessage, 256, "  %umb %ukb %ub of %s\n",
                    luUsage >> 20, (luUsage >> 10) & 1023u, luUsage & 1023u,
                    KAC_DEFAULT_LINEAR_RESOURCE_NAMES[liLane]);
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                    *CgsDev::Log::gpDebugPrint << lacMessage;
            }
        }
        CheckResourceAllocator(maAllocators[0].GetUsage() <= 0x800u,
            "Renderware over budget in main memory\n", 825);
        CheckResourceAllocator(maAllocators[2].GetUsage() <= 0x400000u,
            "Renderware over budget in physical memory\n", 826);
    }

    // ARTIST82664BF0: preserve unsupported-lane checks, print-level gates and
    // the backing allocator's virtual dispatch. Each requested lane is checked.
    rw::Resource HeapResourceAllocator::DoAllocate(
        const rw::ResourceDescriptor& lrDescriptor, const char* lpcName)
    {
        CheckResourceAllocator(lrDescriptor.m_baseResourceDescriptors[3].m_size == 0,
            "We don't support unitialized memory\n", 1101);
        CheckResourceAllocator(lrDescriptor.m_baseResourceDescriptors[4].m_size == 0,
            "We don't support disposable unitialized memory\n", 1102);
        CheckResourceAllocator(lrDescriptor.m_baseResourceDescriptors[1].m_size == 0,
            "We don't support disposable memory\n", 1103);
        CheckResourceAllocator(mpGeneralAllocator != 0, "General allocator not set\n", 1113);
        if (miPrintLevel > 0 && miPrintLevel <= 2)
        {
            if (CgsDev::Message::gxMessageFilterFlags & 1)
            {
                if (mpcName)
                    *CgsDev::Log::gpDebugPrint << "Executing allocations into " << mpcName << "\n";
                else
                    *CgsDev::Log::gpDebugPrint << "Executing allocations: \n";
            }
            char lacMessage[256];
            for (u32 luLane = 0; luLane < rw::KU_RESOURCE_LANE_COUNT; ++luLane)
            {
                const u32 luSize = lrDescriptor.m_baseResourceDescriptors[luLane].m_size;
                if (luSize)
                {
                    CgsCore::SPrintf(lacMessage, 256, "  %umb %ukb %ub of %s\n",
                        luSize >> 20, (luSize >> 10) & 1023u, luSize & 1023u,
                        KAC_DEFAULT_LINEAR_RESOURCE_NAMES[luLane]);
                    if (CgsDev::Message::gxMessageFilterFlags & 1)
                        *CgsDev::Log::gpDebugPrint << lacMessage;
                }
            }
        }
        rw::Resource lResource = mpGeneralAllocator->DoAllocate(lrDescriptor, lpcName);
        for (u32 luLane = 0; luLane < rw::KU_RESOURCE_LANE_COUNT; ++luLane)
        {
            if (lrDescriptor.m_baseResourceDescriptors[luLane].m_size && !lResource.m_baseResources[luLane])
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::StrStream lStream(CgsDev::Assert::gpcMessageBuffer,
                                         CgsDev::Assert::giMessageBufferSize);
                lStream << "Failed to allocate memory type " << KAC_DEFAULT_LINEAR_RESOURCE_NAMES[luLane] << "\n";
                CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer,
                    "..\\..\\..\\GameSource\\Resource/BrnResourceAllocator.cpp", 1153);
                CgsDev::Assert::EndAssert();
            }
        }
        return lResource;
    }

    rw::Resource HeapResourceAllocator::Allocate(
        const rw::ResourceDescriptor& lrDescriptor, const char* lpcName)
    {
        return DoAllocate(lrDescriptor, lpcName);
    }

    // ARTIST82664FD8 and82661E18, including the assertion-only disposable path.
    void HeapResourceAllocator::DoFree(const rw::Resource& lrResource)
    {
        CheckResourceAllocator(lrResource.m_baseResources[3] == 0,
            "We don't support unitialized memory\n", 1180);
        CheckResourceAllocator(lrResource.m_baseResources[4] == 0,
            "We don't support disposable unitialized memory\n", 1181);
        CheckResourceAllocator(lrResource.m_baseResources[1] == 0,
            "We don't support disposable memory\n", 1182);
        CheckResourceAllocator(mpGeneralAllocator != 0, "General allocator not set\n", 1192);
        mpGeneralAllocator->DoFree(lrResource);
    }

    void HeapResourceAllocator::DoFreeDisposable(rw::Resource& /*lrResource*/)
    {
        CheckResourceAllocator(mpGeneralAllocator != 0, "General allocator not set\n", 1209);
        CheckResourceAllocator(false,
            "Should never be allocating or freeing disposable memory!!!\n", 1210);
    }

    // ARTIST82666990/82666A80: independently backed native heaps.
    void Allocators::ConstructGlobalSystemMemory()
    {
        mGlobalSystemMemoryDescriptor.m_baseResourceDescriptors[0].m_size = 0x1B04000;
        mGlobalSystemMemoryDescriptor.m_baseResourceDescriptors[0].m_alignment = 0x10000;
        mGlobalSystemMemoryDescriptor.m_baseResourceDescriptors[2].m_size = 0;
        mGlobalSystemMemoryDescriptor.m_baseResourceDescriptors[2].m_alignment = 0x10000;
        rw::ResourceDescriptor lDescriptor;
        rw::core::GeneralResourceAllocator::GetResourceDescriptor(&lDescriptor, &mGlobalSystemMemoryDescriptor);
        mGlobalSystemMemory.m_baseResources[0] = XPhysicalAlloc(
            lDescriptor.m_baseResourceDescriptors[0].m_size, MAXULONG_PTR, 0x10000, 0x20000004);
        ClearResource(mGlobalSystemMemory, lDescriptor);
        mpInternalGlobalSystemAllocator = rw::core::GeneralResourceAllocator::Initialize(
            mGlobalSystemMemory, mGlobalSystemMemoryDescriptor);
        mGlobalSystemAllocator.Construct(mpInternalGlobalSystemAllocator, "GlobalSystem", 1);
    }

    void Allocators::ConstructGlobalDebugMemory()
    {
        mGlobalDebugMemoryDescriptor.m_baseResourceDescriptors[0].m_size = 0x1240000;
        mGlobalDebugMemoryDescriptor.m_baseResourceDescriptors[0].m_alignment = 0x10000;
        mGlobalDebugMemoryDescriptor.m_baseResourceDescriptors[2].m_size = 0x100000;
        mGlobalDebugMemoryDescriptor.m_baseResourceDescriptors[2].m_alignment = 0x10000;
        rw::ResourceDescriptor lDescriptor;
        rw::core::GeneralResourceAllocator::GetResourceDescriptor(&lDescriptor, &mGlobalDebugMemoryDescriptor);
        mGlobalDebugMemory.m_baseResources[0] = XPhysicalAlloc(
            lDescriptor.m_baseResourceDescriptors[0].m_size, MAXULONG_PTR, 0x10000, 0x20000004);
        mGlobalDebugMemory.m_baseResources[2] = XPhysicalAlloc(
            lDescriptor.m_baseResourceDescriptors[2].m_size, MAXULONG_PTR, 0x10000, 0x20000404);
        ClearResource(mGlobalDebugMemory, lDescriptor);
        mpInternalDebugAllocator = rw::core::GeneralResourceAllocator::Initialize(
            mGlobalDebugMemory, mGlobalDebugMemoryDescriptor);
        mGlobalDebugAllocator.Construct(mpInternalDebugAllocator, "GlobalDebug", 2);
    }

    // ARTIST82666C58: root bank sizes are at Params+28/+30; both physical
    // allocations use128KiB alignment while the descriptors retain64KiB.
    void Allocators::ConstructGlobalResourceMemory(const CgsMemory::MemoryBank::Params* lpRootBankParams)
    {
        mGlobalResourceMemoryDescriptor.m_baseResourceDescriptors[0].m_size = lpRootBankParams->mauBankSize[0] + 0xFC0000;
        mGlobalResourceMemoryDescriptor.m_baseResourceDescriptors[0].m_alignment = 0x10000;
        mGlobalResourceMemoryDescriptor.m_baseResourceDescriptors[2].m_size = lpRootBankParams->mauBankSize[2] + 0x200000;
        mGlobalResourceMemoryDescriptor.m_baseResourceDescriptors[2].m_alignment = 0x10000;
        rw::ResourceDescriptor lDescriptor;
        rw::LinearResourceAllocator::GetResourceDescriptor(&lDescriptor, &mGlobalResourceMemoryDescriptor);
        mGlobalResourceMemory.m_baseResources[0] = XPhysicalAlloc(
            lDescriptor.m_baseResourceDescriptors[0].m_size, MAXULONG_PTR, 0x20000, 0x20000004);
        mGlobalResourceMemory.m_baseResources[2] = XPhysicalAlloc(
            lDescriptor.m_baseResourceDescriptors[2].m_size, MAXULONG_PTR, 0x20000, 0x20000004);
        ClearResource(mGlobalResourceMemory, lDescriptor);
        mpInternalResourceAllocator = rw::LinearResourceAllocator::Initialize(
            mGlobalResourceMemory, mGlobalResourceMemoryDescriptor);
        mGlobalResourceAllocator.Construct(mpInternalResourceAllocator, "GlobalResource");
    }

    // ARTIST82666D48 raw assembly: Runtime is carved through GlobalSystem;
    // EmbeddedTexture owns two separate physical allocations and is not cleared.
    void Allocators::ConstructChildAllocators()
    {
        rw::ResourceDescriptor lRuntimeCapacity;
        lRuntimeCapacity.m_baseResourceDescriptors[0].m_size = 0x1B00000;
        lRuntimeCapacity.m_baseResourceDescriptors[0].m_alignment = 16;
        if (!mpInternalGlobalSystemAllocator)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Allocators::mpInternalGlobalSystemAllocator",
                "..\\..\\..\\GameSource\\Resource/BrnResourceAllocator.h", 362);
            CgsDev::Assert::EndAssert();
        }
        rw::ResourceDescriptor lDescriptor;
        rw::core::GeneralResourceAllocator::GetResourceDescriptor(&lDescriptor, &lRuntimeCapacity);
        rw::Resource lRuntimeMemory = mGlobalSystemAllocator.DoAllocate(lDescriptor, 0);
        ClearResource(lRuntimeMemory, lDescriptor);
        mRuntimeAllocator.Construct(rw::core::GeneralResourceAllocator::Initialize(
            lRuntimeMemory, lRuntimeCapacity), "Runtime", 1);

        rw::ResourceDescriptor lTextureCapacity;
        lTextureCapacity.m_baseResourceDescriptors[0].m_size = 0x20000;
        lTextureCapacity.m_baseResourceDescriptors[0].m_alignment = 0x10000;
        lTextureCapacity.m_baseResourceDescriptors[2].m_size = 0x300000;
        lTextureCapacity.m_baseResourceDescriptors[2].m_alignment = 0x10000;
        rw::LinearResourceAllocator::GetResourceDescriptor(&lDescriptor, &lTextureCapacity);
        rw::Resource lTextureMemory;
        lTextureMemory.m_baseResources[0] = XPhysicalAlloc(
            lDescriptor.m_baseResourceDescriptors[0].m_size, MAXULONG_PTR, 0x10000, 0x20000004);
        lTextureMemory.m_baseResources[2] = XPhysicalAlloc(
            lDescriptor.m_baseResourceDescriptors[2].m_size, MAXULONG_PTR, 0x10000, 0x20000004);
        mEmbeddedTextureAllocator.Construct(rw::LinearResourceAllocator::Initialize(
            lTextureMemory, lTextureCapacity), 0);
    }

    void Allocators::Construct()
    {
        PrintConsoleMemory("Pre-allocators construction");
        mInitialDefaultAllocator.CheckRenderwareBudget();
        const CgsMemory::MemoryBank::Params* lpRootBankParams =
            MemoryMapImageInitializer::GetImage().maBanks[0].GetParams();
        ConstructGlobalSystemMemory();
        ConstructGlobalDebugMemory();
        ConstructGlobalGraphicsMemory();
        ConstructGlobalResourceMemory(lpRootBankParams);
        ConstructChildAllocators();
        rw::ResourceAllocatorRegistry::SetDefaultAllocator(&mGlobalSystemAllocator);
        PrintConsoleMemory("Post-allocators construction");
    }

    // ARTIST826651C0: real descriptor validation, diagnostics, allocation and
    // per-requested-lane pointer/alignment validation. No native arena fallback.
    rw::Resource LinearResourceAllocator::DoAllocate(
        const rw::ResourceDescriptor& lrDescriptor, const char* lpcName)
    {
        const char* const lpcFile = "..\\..\\..\\GameSource\\Resource/BrnResourceAllocator.cpp";
        CheckResourceAllocator(lrDescriptor.m_baseResourceDescriptors[3].m_size == 0,
               "We don't support unitialized memory\n", 1293);
        CheckResourceAllocator(lrDescriptor.m_baseResourceDescriptors[4].m_size == 0,
               "We don't support disposable unitialized memory\n", 1294);
        CheckResourceAllocator(lrDescriptor.m_baseResourceDescriptors[1].m_size == 0,
               "We don't support disposable memory\n", 1295);
        CheckResourceAllocator(mpLinearAllocator != 0, "General allocator not set\n", 1305);

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
                    CgsDev::StrStream lStream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
                    lStream << "Failed to allocate memory type " << lapcMemoryNames[luLane] << "\n";
                    CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer, lpcFile, 1355);
                    CgsDev::Assert::EndAssert();
                }
                const u32 luAlignment = lrDescriptor.m_baseResourceDescriptors[luLane].m_alignment;
                // The original zero-divisor trap is retained by the modulo operation;
                // no fallback alignment or failed-allocation early return is added.
                if (reinterpret_cast<uintptr_t>(lResource.m_baseResources[luLane]) % luAlignment)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::StrStream lStream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
                    lStream << "Allocated memory is not aligned to " << luAlignment << "bytes\n";
                    CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer, lpcFile, 1358);
                    CgsDev::Assert::EndAssert();
                }
            }
        }
        return lResource;
    }

    void LinearResourceAllocator::DoFree(const rw::Resource& lrResource)
    {
        CheckResourceAllocator(lrResource.m_baseResources[3] == 0, "We don't support unitialized memory\n", 1385);
        CheckResourceAllocator(lrResource.m_baseResources[4] == 0, "We don't support disposable unitialized memory\n", 1386);
        CheckResourceAllocator(lrResource.m_baseResources[1] == 0, "We don't support disposable memory\n", 1387);
        CheckResourceAllocator(mpLinearAllocator != 0, "General allocator not set\n", 1397);
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
