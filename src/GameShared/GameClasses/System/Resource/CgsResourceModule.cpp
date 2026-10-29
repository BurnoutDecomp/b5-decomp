#include "GameShared/GameClasses/System/Resource/CgsResourceModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/System/Resource/CgsPoolModuleIO.h"  // PoolIO::Input/OutputBuffer (shuttle)
#include "GameShared/GameClasses/System/Resource/CgsResourceModuleIO.h" // ResourceIO::InputBuffer (module input)
#include "GameShared/GameClasses/Memory/CgsMemoryModuleIO.h"         // MemoryIO buffers + MemoryResponse
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h" // receiver-queue forward target
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistry.h" // ResolveResourceType (bundle FixUp resolver)
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"     // Events::PoolEvent (pool response routing)
#include "GameShared/GameClasses/Module/CgsModuleIOHelper.h"
#include "rw/rwcore_general_alloc.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/Jobs/Relocator/CgsRelocator.h"
#include <cstddef>
#include <cstring>                                    // memset (InitOptions zero-init)

// CgsResource::ResourceModule: native module lifetime and staged request shuttles.
// Update preserves the loader -> pool -> memory order and delayed pool replies.
namespace CgsResource
{
    s32 ResourceModule::_miResourceUpdatePerfMon = -1;
    s32 ResourceModule::_miProcessResourceRequestsPerfMon = -1;
    s32 ResourceModule::_miUpdateBundleLoaderPerfMon = -1;
    s32 ResourceModule::_miUpdatePoolPerfMon = -1;
    s32 ResourceModule::_miUpdateMemoryPerfMon = -1;
    s32 ResourceModule::_miUpdateFileSystemPerfMon = -1;
    s32 ResourceModule::_miUpdateOutputPerfMon = -1;
    s32 ResourceModule::_miUpdateTrace = -1;

    // ResourceModule::InitOptions ctor - zero-init. The X360 ConstructResourceModule constructs this
    // then memset(0)s the whole 1216B block before filling fields, so a zero-init is the faithful net
    // state. (memset over the already-constructed members matches that ctor-then-memset sequence.)
    ResourceModule::InitOptions::InitOptions()
    {
        memset(this, 0, sizeof(*this));
    }

    // @ 0x828F4140 - resumable bring-up: base module, then Memory -> FileSystem -> Bundle ->
    // Pool, then register the debug component. Re-enters at the current stage if a sub-module
    // is not yet ready.
    bool ResourceModule::Prepare()
    {
        switch (mePrepareStage)
        {
        case E_PREPARE_START:
        case E_PREPARE_BASE:
            mePrepareStage = E_PREPARE_BASE;
            if (!CgsModule::ModuleSingleBuffered::Prepare())
                return false;
            // fall through
        case E_PREPARE_MEMORY:
            mePrepareStage = E_PREPARE_MEMORY;
            if (!mMemoryModule.Prepare())
                return false;
            // fall through
        case E_PREPARE_FILESYSTEM:
            mePrepareStage = E_PREPARE_FILESYSTEM;
            if (!mFileSystem.Prepare())
                return false;
            // fall through
        case E_PREPARE_BUNDLE:
            mePrepareStage = E_PREPARE_BUNDLE;
            if (!mBundleLoaderModule.Prepare())
                return false;
            // fall through
        case E_PREPARE_POOL:
            mePrepareStage = E_PREPARE_POOL;
            if (!mPoolModule.Prepare())
                return false;
            mDebugComponent.Register();
            // fall through
        case E_PREPARE_DONE:
            meReleaseStage = E_RELEASE_START;
            mePrepareStage = E_PREPARE_DONE;
            return true;
        default:
            CGS_ASSERT(false, "Invalid stage");
            return false;
        }
    }

    // @ 0x82906570 - resumable tear-down: Pool -> Bundle -> FileSystem -> Memory, then the
    // base module (reverse of Prepare).
    bool ResourceModule::Release()
    {
        switch (meReleaseStage)
        {
        case E_RELEASE_START:
            // fall through
        case E_RELEASE_POOL:
            meReleaseStage = E_RELEASE_POOL;
            if (!mPoolModule.Release())
                return false;
            // fall through
        case E_RELEASE_BUNDLE:
            meReleaseStage = E_RELEASE_BUNDLE;
            if (!mBundleLoaderModule.Release())
                return false;
            // fall through
        case E_RELEASE_FILESYSTEM:
            meReleaseStage = E_RELEASE_FILESYSTEM;
            if (!mFileSystem.Release())
                return false;
            // fall through
        case E_RELEASE_MEMORY:
            meReleaseStage = E_RELEASE_MEMORY;
            if (!mMemoryModule.Release())
                return false;
            // fall through
        case E_RELEASE_BASE:
            meReleaseStage = E_RELEASE_BASE;
            if (!CgsModule::ModuleSingleBuffered::Release())
                return false;
            // fall through
        case E_RELEASE_DONE:
            mePrepareStage = E_PREPARE_START;
            meReleaseStage = E_RELEASE_DONE;
            return true;
        default:
            CGS_ASSERT(false, "Invalid stage");
            return false;
        }
    }

    // ARTIST829055B0: preserve submodule order and construct the dedicated
    // 0x160000-byte general allocator used by filesystem and response storage.
    void ResourceModule::Construct(InitOptions* lpOptions, rw::IResourceAllocator* lpAllocator,
                                   rw::IResourceAllocator* lpDebugAllocator)
    {
        CgsModule::ModuleSingleBuffered::Construct();
        mePrepareStage = E_PREPARE_START;
        meReleaseStage = E_RELEASE_DONE;
        mMemoryModule.Construct(&lpOptions->mMemoryInitOptions, lpAllocator);
        mBundleLoaderModule.Construct(&lpOptions->mLoaderInitOptions, lpAllocator, lpDebugAllocator);
        mPoolModule.Construct(&lpOptions->mPoolInitOptions, lpAllocator, lpDebugAllocator);
        mDebugComponent.Construct(this, &lpOptions->mDebugParams);
        rw::ResourceDescriptor lCapacity;
        lCapacity.m_baseResourceDescriptors[0].m_size = 0x160000;
        lCapacity.m_baseResourceDescriptors[0].m_alignment = 16;
        rw::ResourceDescriptor lRequired;
        rw::core::GeneralResourceAllocator::GetResourceDescriptor(&lRequired, &lCapacity);
        rw::Resource lResource = lpAllocator->Allocate(lRequired, 0);
        mpFileSystemAllocator = rw::core::GeneralResourceAllocator::Initialize(lResource, lCapacity);
        mFileSystem.Construct(mpFileSystemAllocator, lpDebugAllocator,
                              lpOptions->macFileSearchPath, lpOptions->mpDiskLayout);
        mPendingFileResponses.Construct(maPendingFileResponses,
            maiPendingFileResponseFreeIndices, static_cast<s8>(KI_MAX_PENDING_FILE_SYSTEM_RESPONSES));
        // ARTIST8290571C..82906554: preserve idempotent registration order and
        // class-static homes shared with the timed native callers.
        if (ResourceModule::_miResourceUpdatePerfMon == -1)
        {
            ResourceModule::_miResourceUpdatePerfMon = CgsDev::PerfMonCpu::AddMonitor("Resource update", CgsDev::E_PMP_1, false, 5.0f, false);
            if (ResourceModule::_miResourceUpdatePerfMon < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::ResourceModule::_miResourceUpdatePerfMon >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 100);
                CgsDev::Assert::EndAssert();
            }
        }
        if (ResourceModule::_miProcessResourceRequestsPerfMon == -1)
        {
            ResourceModule::_miProcessResourceRequestsPerfMon = CgsDev::PerfMonCpu::AddMonitor("  Process requests", CgsDev::E_PMP_1, false, 5.0f, false);
            if (ResourceModule::_miProcessResourceRequestsPerfMon < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::ResourceModule::_miProcessResourceRequestsPerfMon >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 101);
                CgsDev::Assert::EndAssert();
            }
        }
        if (ResourceModule::_miUpdateBundleLoaderPerfMon == -1)
        {
            ResourceModule::_miUpdateBundleLoaderPerfMon = CgsDev::PerfMonCpu::AddMonitor("  Bundle loader", CgsDev::E_PMP_1, false, 5.0f, false);
            if (ResourceModule::_miUpdateBundleLoaderPerfMon < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::ResourceModule::_miUpdateBundleLoaderPerfMon >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 102);
                CgsDev::Assert::EndAssert();
            }
        }
        if (ResourceModule::_miUpdatePoolPerfMon == -1)
        {
            ResourceModule::_miUpdatePoolPerfMon = CgsDev::PerfMonCpu::AddMonitor("  Pool", CgsDev::E_PMP_1, false, 5.0f, false);
            if (ResourceModule::_miUpdatePoolPerfMon < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::ResourceModule::_miUpdatePoolPerfMon >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 103);
                CgsDev::Assert::EndAssert();
            }
        }
        if (ResourceModule::_miUpdateMemoryPerfMon == -1)
        {
            ResourceModule::_miUpdateMemoryPerfMon = CgsDev::PerfMonCpu::AddMonitor("  Memory", CgsDev::E_PMP_1, false, 5.0f, false);
            if (ResourceModule::_miUpdateMemoryPerfMon < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::ResourceModule::_miUpdateMemoryPerfMon >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 104);
                CgsDev::Assert::EndAssert();
            }
        }
        if (ResourceModule::_miUpdateFileSystemPerfMon == -1)
        {
            ResourceModule::_miUpdateFileSystemPerfMon = CgsDev::PerfMonCpu::AddMonitor("  File system", CgsDev::E_PMP_1, false, 5.0f, false);
            if (ResourceModule::_miUpdateFileSystemPerfMon < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::ResourceModule::_miUpdateFileSystemPerfMon >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 105);
                CgsDev::Assert::EndAssert();
            }
        }
        if (ResourceModule::_miUpdateOutputPerfMon == -1)
        {
            ResourceModule::_miUpdateOutputPerfMon = CgsDev::PerfMonCpu::AddMonitor("  Output", CgsDev::E_PMP_1, false, 5.0f, false);
            if (ResourceModule::_miUpdateOutputPerfMon < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::ResourceModule::_miUpdateOutputPerfMon >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 106);
                CgsDev::Assert::EndAssert();
            }
        }
        if (Pool::_miPoolModuleStatePerfMon == -1)
        {
            Pool::_miPoolModuleStatePerfMon = CgsDev::PerfMonCpu::AddMonitor("Pool state update", CgsDev::E_PMP_1, false, 5.0f, false);
            if (Pool::_miPoolModuleStatePerfMon < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::Pool::_miPoolModuleStatePerfMon >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 108);
                CgsDev::Assert::EndAssert();
            }
        }
        if (Pool::_miPoolModuleIdle == -1)
        {
            Pool::_miPoolModuleIdle = CgsDev::PerfMonCpu::AddMonitor("  PS Idle", CgsDev::E_PMP_1, false, 5.0f, false);
            if (Pool::_miPoolModuleIdle < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::Pool::_miPoolModuleIdle >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 109);
                CgsDev::Assert::EndAssert();
            }
        }
        if (Pool::_miPoolModuleAllocating == -1)
        {
            Pool::_miPoolModuleAllocating = CgsDev::PerfMonCpu::AddMonitor("  PS Alloc", CgsDev::E_PMP_1, false, 5.0f, false);
            if (Pool::_miPoolModuleAllocating < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::Pool::_miPoolModuleAllocating >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 110);
                CgsDev::Assert::EndAssert();
            }
        }
        if (Pool::_miPoolModuleDeallocating == -1)
        {
            Pool::_miPoolModuleDeallocating = CgsDev::PerfMonCpu::AddMonitor("  PS DeAlloc", CgsDev::E_PMP_1, false, 5.0f, false);
            if (Pool::_miPoolModuleDeallocating < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::Pool::_miPoolModuleDeallocating >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 111);
                CgsDev::Assert::EndAssert();
            }
        }
        if (Pool::_miPoolModuleLiveupdate == -1)
        {
            Pool::_miPoolModuleLiveupdate = CgsDev::PerfMonCpu::AddMonitor("  PS LiveUpd", CgsDev::E_PMP_1, false, 5.0f, false);
            if (Pool::_miPoolModuleLiveupdate < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::Pool::_miPoolModuleLiveupdate >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 112);
                CgsDev::Assert::EndAssert();
            }
        }
        if (Pool::_miPoolModuleIntellifrag == -1)
        {
            Pool::_miPoolModuleIntellifrag = CgsDev::PerfMonCpu::AddMonitor("  PS Intellifrag", CgsDev::E_PMP_1, false, 5.0f, false);
            if (Pool::_miPoolModuleIntellifrag < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::Pool::_miPoolModuleIntellifrag >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 113);
                CgsDev::Assert::EndAssert();
            }
        }
        if (Pool::_miPoolModuleEmergencyFrag == -1)
        {
            Pool::_miPoolModuleEmergencyFrag = CgsDev::PerfMonCpu::AddMonitor("  PS Emergencyfrag", CgsDev::E_PMP_1, false, 5.0f, false);
            if (Pool::_miPoolModuleEmergencyFrag < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::Pool::_miPoolModuleEmergencyFrag >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 114);
                CgsDev::Assert::EndAssert();
            }
        }
        if (CgsMemory::Relocator::_miExecutePerfMon == -1)
        {
            CgsMemory::Relocator::_miExecutePerfMon = CgsDev::PerfMonCpu::AddMonitor("    Relocator Exec", CgsDev::E_PMP_1, false, 5.0f, false);
            if (CgsMemory::Relocator::_miExecutePerfMon < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsMemory::Relocator::_miExecutePerfMon >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 115);
                CgsDev::Assert::EndAssert();
            }
        }
        if (CgsMemory::Relocator::_miGPUCopyPerfMon == -1)
        {
            CgsMemory::Relocator::_miGPUCopyPerfMon = CgsDev::PerfMonCpu::AddMonitor("      Relocator GPU", CgsDev::E_PMP_1, false, 5.0f, false);
            if (CgsMemory::Relocator::_miGPUCopyPerfMon < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsMemory::Relocator::_miGPUCopyPerfMon >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 116);
                CgsDev::Assert::EndAssert();
            }
        }
        if (CgsMemory::Relocator::_miUpdatePerfMon == -1)
        {
            CgsMemory::Relocator::_miUpdatePerfMon = CgsDev::PerfMonCpu::AddMonitor("    Relocator Update", CgsDev::E_PMP_1, false, 5.0f, false);
            if (CgsMemory::Relocator::_miUpdatePerfMon < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsMemory::Relocator::_miUpdatePerfMon >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 117);
                CgsDev::Assert::EndAssert();
            }
        }
        if (Pool::_miPoolUpdatePerfMon == -1)
        {
            Pool::_miPoolUpdatePerfMon = CgsDev::PerfMonCpu::AddMonitor("Pool update", CgsDev::E_PMP_1, false, 5.0f, false);
            if (Pool::_miPoolUpdatePerfMon < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::Pool::_miPoolUpdatePerfMon >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 119);
                CgsDev::Assert::EndAssert();
            }
        }
        if (Pool::_miPoolDeleteHashEntryPerfMon == -1)
        {
            Pool::_miPoolDeleteHashEntryPerfMon = CgsDev::PerfMonCpu::AddMonitor("  Delete hash entry", CgsDev::E_PMP_1, false, 5.0f, false);
            if (Pool::_miPoolDeleteHashEntryPerfMon < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::Pool::_miPoolDeleteHashEntryPerfMon >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 120);
                CgsDev::Assert::EndAssert();
            }
        }
        if (Pool::_miPoolDeleteMemoryPerfMon == -1)
        {
            Pool::_miPoolDeleteMemoryPerfMon = CgsDev::PerfMonCpu::AddMonitor("  Delete memory", CgsDev::E_PMP_1, false, 5.0f, false);
            if (Pool::_miPoolDeleteMemoryPerfMon < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::Pool::_miPoolDeleteMemoryPerfMon >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 121);
                CgsDev::Assert::EndAssert();
            }
        }
        if (Pool::_miPoolDeleteResourcePerfMon == -1)
        {
            Pool::_miPoolDeleteResourcePerfMon = CgsDev::PerfMonCpu::AddMonitor("  Delete resource", CgsDev::E_PMP_1, false, 5.0f, false);
            if (Pool::_miPoolDeleteResourcePerfMon < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::Pool::_miPoolDeleteResourcePerfMon >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 122);
                CgsDev::Assert::EndAssert();
            }
        }
        if (Pool::_miPoolAllocateResourceListPerfMon == -1)
        {
            Pool::_miPoolAllocateResourceListPerfMon = CgsDev::PerfMonCpu::AddMonitor("Pool alloc list", CgsDev::E_PMP_1, false, 5.0f, false);
            if (Pool::_miPoolAllocateResourceListPerfMon < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::Pool::_miPoolAllocateResourceListPerfMon >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 124);
                CgsDev::Assert::EndAssert();
            }
        }
        if (Pool::_miPoolFixupPerfMon == -1)
        {
            Pool::_miPoolFixupPerfMon = CgsDev::PerfMonCpu::AddMonitor("Pool fixup", CgsDev::E_PMP_1, false, 5.0f, false);
            if (Pool::_miPoolFixupPerfMon < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::Pool::_miPoolFixupPerfMon >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 138);
                CgsDev::Assert::EndAssert();
            }
        }
        if (Pool::_miPoolFindByIndexPerfMon == -1)
        {
            Pool::_miPoolFindByIndexPerfMon = CgsDev::PerfMonCpu::AddMonitor("Pool search by index", CgsDev::E_PMP_1, false, 5.0f, false);
            if (Pool::_miPoolFindByIndexPerfMon < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::Pool::_miPoolFindByIndexPerfMon >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 139);
                CgsDev::Assert::EndAssert();
            }
        }
        if (Pool::_miPoolFixupResourcePerfMon == -1)
        {
            Pool::_miPoolFixupResourcePerfMon = CgsDev::PerfMonCpu::AddMonitor("Pool fixup resource", CgsDev::E_PMP_1, false, 5.0f, false);
            if (Pool::_miPoolFixupResourcePerfMon < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::Pool::_miPoolFixupResourcePerfMon >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 140);
                CgsDev::Assert::EndAssert();
            }
        }
        if (Pool::_miPoolFindTypePerfMon == -1)
        {
            Pool::_miPoolFindTypePerfMon = CgsDev::PerfMonCpu::AddMonitor("Pool find resource type", CgsDev::E_PMP_1, false, 5.0f, false);
            if (Pool::_miPoolFindTypePerfMon < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::Pool::_miPoolFindTypePerfMon >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 141);
                CgsDev::Assert::EndAssert();
            }
        }
        if (Pool::_miPoolDefragPerfMon == -1)
        {
            Pool::_miPoolDefragPerfMon = CgsDev::PerfMonCpu::AddMonitor("Pool defrag", CgsDev::E_PMP_1, false, 5.0f, false);
            if (Pool::_miPoolDefragPerfMon < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::Pool::_miPoolDefragPerfMon >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 143);
                CgsDev::Assert::EndAssert();
            }
        }
        if (Pool::_miPoolDefragWFInitialDeathsPerfMon == -1)
        {
            Pool::_miPoolDefragWFInitialDeathsPerfMon = CgsDev::PerfMonCpu::AddMonitor("  Pool WF Initial Deaths", CgsDev::E_PMP_1, false, 5.0f, false);
            if (Pool::_miPoolDefragWFInitialDeathsPerfMon < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::Pool::_miPoolDefragWFInitialDeathsPerfMon >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 144);
                CgsDev::Assert::EndAssert();
            }
        }
        if (Pool::_miPoolDefragMakeTempPerfMon == -1)
        {
            Pool::_miPoolDefragMakeTempPerfMon = CgsDev::PerfMonCpu::AddMonitor("  Pool defrag make temp", CgsDev::E_PMP_1, false, 5.0f, false);
            if (Pool::_miPoolDefragMakeTempPerfMon < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::Pool::_miPoolDefragMakeTempPerfMon >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 145);
                CgsDev::Assert::EndAssert();
            }
        }
        if (Pool::_miPoolDefragMoveToTempPerfMon == -1)
        {
            Pool::_miPoolDefragMoveToTempPerfMon = CgsDev::PerfMonCpu::AddMonitor("    Move temp", CgsDev::E_PMP_1, false, 5.0f, false);
            if (Pool::_miPoolDefragMoveToTempPerfMon < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::Pool::_miPoolDefragMoveToTempPerfMon >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 146);
                CgsDev::Assert::EndAssert();
            }
        }
        if (Pool::_miPoolDefragResolveTempPerfMon == -1)
        {
            Pool::_miPoolDefragResolveTempPerfMon = CgsDev::PerfMonCpu::AddMonitor("    Temp resolve", CgsDev::E_PMP_1, false, 5.0f, false);
            if (Pool::_miPoolDefragResolveTempPerfMon < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::Pool::_miPoolDefragResolveTempPerfMon >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 147);
                CgsDev::Assert::EndAssert();
            }
        }
        if (Pool::_miPoolDefragWFSourceDeathsPerfMon == -1)
        {
            Pool::_miPoolDefragWFSourceDeathsPerfMon = CgsDev::PerfMonCpu::AddMonitor("  Pool WF Source Deaths", CgsDev::E_PMP_1, false, 5.0f, false);
            if (Pool::_miPoolDefragWFSourceDeathsPerfMon < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::Pool::_miPoolDefragWFSourceDeathsPerfMon >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 148);
                CgsDev::Assert::EndAssert();
            }
        }
        if (Pool::_miPoolDefragMakeDestPerfMon == -1)
        {
            Pool::_miPoolDefragMakeDestPerfMon = CgsDev::PerfMonCpu::AddMonitor("  Pool defrag make dest", CgsDev::E_PMP_1, false, 5.0f, false);
            if (Pool::_miPoolDefragMakeDestPerfMon < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::Pool::_miPoolDefragMakeDestPerfMon >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 149);
                CgsDev::Assert::EndAssert();
            }
        }
        if (Pool::_miPoolDefragMoveToDestPerfMon == -1)
        {
            Pool::_miPoolDefragMoveToDestPerfMon = CgsDev::PerfMonCpu::AddMonitor("    Move dest", CgsDev::E_PMP_1, false, 5.0f, false);
            if (Pool::_miPoolDefragMoveToDestPerfMon < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::Pool::_miPoolDefragMoveToDestPerfMon >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 150);
                CgsDev::Assert::EndAssert();
            }
        }
        if (Pool::_miPoolDefragMoveDestRebasePerfMon == -1)
        {
            Pool::_miPoolDefragMoveDestRebasePerfMon = CgsDev::PerfMonCpu::AddMonitor("      Dest rebase", CgsDev::E_PMP_1, false, 5.0f, false);
            if (Pool::_miPoolDefragMoveDestRebasePerfMon < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::Pool::_miPoolDefragMoveDestRebasePerfMon >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 151);
                CgsDev::Assert::EndAssert();
            }
        }
        if (Pool::_miPoolDefragResolveDestPerfMon == -1)
        {
            Pool::_miPoolDefragResolveDestPerfMon = CgsDev::PerfMonCpu::AddMonitor("    Dest resolve", CgsDev::E_PMP_1, false, 5.0f, false);
            if (Pool::_miPoolDefragResolveDestPerfMon < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::Pool::_miPoolDefragResolveDestPerfMon >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 152);
                CgsDev::Assert::EndAssert();
            }
        }
        if (Pool::_miPoolDefragWFTempDeathsPerfMon == -1)
        {
            Pool::_miPoolDefragWFTempDeathsPerfMon = CgsDev::PerfMonCpu::AddMonitor("  Pool WF Temp Deaths", CgsDev::E_PMP_1, false, 5.0f, false);
            if (Pool::_miPoolDefragWFTempDeathsPerfMon < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::Pool::_miPoolDefragWFTempDeathsPerfMon >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 153);
                CgsDev::Assert::EndAssert();
            }
        }
        if (Pool::_miPoolDefragWFDonePerfMon == -1)
        {
            Pool::_miPoolDefragWFDonePerfMon = CgsDev::PerfMonCpu::AddMonitor("  Pool Done", CgsDev::E_PMP_1, false, 5.0f, false);
            if (Pool::_miPoolDefragWFDonePerfMon < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::Pool::_miPoolDefragWFDonePerfMon >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 154);
                CgsDev::Assert::EndAssert();
            }
        }
        if (BundleLoaderModule::_miUpdatePerfMon == -1)
        {
            BundleLoaderModule::_miUpdatePerfMon = CgsDev::PerfMonCpu::AddMonitor("Loader update", CgsDev::E_PMP_1, false, 5.0f, false);
            if (BundleLoaderModule::_miUpdatePerfMon < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::BundleLoaderModule::_miUpdatePerfMon >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 156);
                CgsDev::Assert::EndAssert();
            }
        }
        if (BundleLoaderModule::_miStreamUpdate == -1)
        {
            BundleLoaderModule::_miStreamUpdate = CgsDev::PerfMonCpu::AddMonitor("  Stream update", CgsDev::E_PMP_1, false, 5.0f, false);
            if (BundleLoaderModule::_miStreamUpdate < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::BundleLoaderModule::_miStreamUpdate >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 157);
                CgsDev::Assert::EndAssert();
            }
        }
        if (BundleLoaderModule::_miStreamEntryCopy == -1)
        {
            BundleLoaderModule::_miStreamEntryCopy = CgsDev::PerfMonCpu::AddMonitor("    Entry MemCpy", CgsDev::E_PMP_1, false, 5.0f, false);
            if (BundleLoaderModule::_miStreamEntryCopy < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::BundleLoaderModule::_miStreamEntryCopy >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 158);
                CgsDev::Assert::EndAssert();
            }
        }
        if (BundleLoaderModule::_miStreamDataCopy == -1)
        {
            BundleLoaderModule::_miStreamDataCopy = CgsDev::PerfMonCpu::AddMonitor("    Data MemCpy", CgsDev::E_PMP_1, false, 5.0f, false);
            if (BundleLoaderModule::_miStreamDataCopy < 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CgsResource::BundleLoaderModule::_miStreamDataCopy >= 0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp", 159);
                CgsDev::Assert::EndAssert();
            }
        }
        ResourceModule::_miUpdateTrace = 0;
        PoolModule::_miAllocateListTraceId = 0;
        PoolModule::_miFixupListTraceId = 0;
        PoolModule::_miAllocateStateTraceId = 0;
        PoolModule::_miIntellifragStateTraceId = 0;
        Pool::_miPoolAddTempTrace = 0;
        Pool::_miResolveAllTrace = 0;
        Pool::_miFixupTrace = 0;
        BundleLoaderModule::_miTraceStreamUpdate_StreamHeader = 0;
        BundleLoaderModule::_miTraceStreamUpdate_StreamEntries = 0;
        BundleLoaderModule::_miTraceStreamUpdate_StreamData = 0;
        mbIsNewModule = true;
    }

    // ARTIST828F2900/2998 and828EA560/A5C0: lane-zero allocations are16-aligned,
    // and responses return to the same allocator through the native DoFree slot.
    template<class Response> Response* ResourceModule::FileSystemAlloc()
    {
        rw::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size = sizeof(Response);
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 16;
        return static_cast<Response*>(mpFileSystemAllocator->Allocate(lDescriptor, 0).m_baseResources[0]);
    }
    template<class Response> void ResourceModule::FileSystemFree(Response* lpResponse)
    {
        rw::Resource lResource;
        lResource.m_baseResources[0] = lpResponse;
        mpFileSystemAllocator->DoFree(lResource);
    }

    // ARTIST828EC6B0 owns no pending-response drain.
    void ResourceModule::Destruct()
    {
        mFileSystem.Destruct();
        CgsModule::ModuleSingleBuffered::Destruct();
    }

    void ResourceModule::AssertNativeInitOptions()
    {
        CGS_STATIC_ASSERT(sizeof(CgsMemory::MemoryModule::InitOptions) == 72, "ARTIST memory options");
        CGS_STATIC_ASSERT(offsetof(InitOptions, mLoaderInitOptions) == 72, "ARTIST loader options");
        CGS_STATIC_ASSERT(offsetof(InitOptions, mPoolInitOptions) == 96, "ARTIST pool options");
        CGS_STATIC_ASSERT(offsetof(InitOptions, mpDiskLayout) == 172, "ARTIST disk layout option");
        CGS_STATIC_ASSERT(offsetof(InitOptions, mDebugParams) == 176, "ARTIST debug options");
        CGS_STATIC_ASSERT(offsetof(InitOptions, macFileSearchPath) == 188, "ARTIST file search option");
        CGS_STATIC_ASSERT(sizeof(InitOptions) == 1216, "ARTIST resource options");
        CGS_STATIC_ASSERT(offsetof(ResourceModule, mBundleLoaderModule) == 640, "ARTIST bundle module owner");
        CGS_STATIC_ASSERT(offsetof(ResourceModule, mPoolModule) == 181120, "ARTIST pool module owner");
        CGS_STATIC_ASSERT(offsetof(ResourceModule, mMemoryModule) == 288128, "ARTIST memory module owner");
        CGS_STATIC_ASSERT(offsetof(ResourceModule, mFileSystem) == 288816, "ARTIST filesystem owner");
        CGS_STATIC_ASSERT(offsetof(ResourceModule, mDebugComponent) == 339224, "ARTIST resource debug owner");
        CGS_STATIC_ASSERT(offsetof(ResourceModule, mpFileSystemAllocator) == 0x5802c, "ARTIST filesystem allocator owner");
        CGS_STATIC_ASSERT(offsetof(ResourceModule, mPendingFileResponses) == 0x58030, "ARTIST pending-response pool");
        CGS_STATIC_ASSERT(offsetof(ResourceModule, maPendingFileResponses) == 0x5803c, "ARTIST pending-response records");
        CGS_STATIC_ASSERT(offsetof(ResourceModule, maiPendingFileResponseFreeIndices) == 0x580fc, "ARTIST pending-response free indices");
        CGS_STATIC_ASSERT(sizeof(ResourceModule) == 0x58180, "ARTIST resource module span");
    }

    // ARTIST82907268: native event sizes are carried by the queue. Pool and
    // memory tags below are the original switch values, not resource tags.
    void ResourceModule::ProcessResourceRequests(const ResourceIO::InputBuffer* lpResIn,
        CgsMemory::MemoryIO::InputBuffer* lpMemIn,
        BundleLoaderIO::InputBuffer_Update* lpLoaderIn, PoolIO::InputBuffer* lpPoolIn)
    {
        const ResourceIO::ResourceRequestQueue<16384>* lpQueue = static_cast<const ResourceIO::InputBuffer*>(lpResIn)->GetResourceQueue();
        PoolIO::InputBuffer::PoolInputQueue* lpPoolQueue = lpPoolIn->GetPoolInputQueue();
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liTag = lpQueue->GetFirstEvent(&lpEvent, &liSize);
        while (lpEvent)
        {
            switch (liTag)
            {
            case 0: lpPoolQueue->AddEvent(lpEvent, 0, liSize); break;
            case 1: break;
            case 2: lpLoaderIn->GetLoadBundleRequestQueue()->AddEvent(
                        *reinterpret_cast<const Events::LoadBundleRequest*>(lpEvent)); break;
            case 3: lpLoaderIn->GetUnloadBundleRequestQueue()->AddEvent(
                        *reinterpret_cast<const Events::UnloadBundleRequest*>(lpEvent)); break;
            case 4: case 5: lpPoolQueue->AddEvent(lpEvent, liTag, liSize); break;
            case 6: lpPoolQueue->AddEvent(lpEvent, 8, liSize); break;
            case 7: lpPoolQueue->AddEvent(lpEvent, 11, liSize); break;
            case 8: lpPoolQueue->AddEvent(lpEvent, 12, liSize); break;
            case 9: case 10: case 11: case 12: case 13: case 14: case 15:
                lpMemIn->GetMemoryRequestQueue()->AddEvent(lpEvent, 1, liSize); break;
            case 16: AddOpenReadStreamRequest(reinterpret_cast<const Events::OpenReadStreamRequest*>(lpEvent)); break;
            case 17: AddOpenWriteStreamRequest(reinterpret_cast<const Events::OpenWriteStreamRequest*>(lpEvent)); break;
            case 18: AddCloseReadStreamRequest(reinterpret_cast<const Events::CloseReadStreamRequest*>(lpEvent)); break;
            case 19: AddCloseWriteStreamRequest(reinterpret_cast<const Events::CloseWriteStreamRequest*>(lpEvent)); break;
            default: CGS_ASSERT(false, "Invalid event id\n"); break;
            }
            const CgsModule::Event* lpNext = 0;
            liTag = lpQueue->GetNextEvent(lpEvent, &lpNext, &liSize);
            lpEvent = lpNext;
        }
    }

    // ARTIST82907580..82907630 (export hole).
    void ResourceModule::ProcessBundleLoaderStreamRequests(const BundleLoaderIO::OutputBuffer* lpLoaderOut)
    {
        const BundleLoaderIO::OutputBuffer::StreamRequestQueue* lpQueue = lpLoaderOut->GetStreamRequestQueue();
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liTag = lpQueue->GetFirstEvent(&lpEvent, &liSize);
        while (lpEvent)
        {
            if (liTag == 16)
                AddOpenReadStreamRequest(reinterpret_cast<const Events::OpenReadStreamRequest*>(lpEvent));
            else if (liTag == 18)
                AddCloseReadStreamRequest(reinterpret_cast<const Events::CloseReadStreamRequest*>(lpEvent));
            else
                CGS_ASSERT(false, "Unexpected request from bundle loader\n");
            const CgsModule::Event* lpNext = 0;
            liTag = lpQueue->GetNextEvent(lpEvent, &lpNext, &liSize);
            lpEvent = lpNext;
        }
    }

    // ARTIST828EC788: bundle replies precede externally addressed pool replies.
    void ResourceModule::ProcessResourceResponses(const BundleLoaderIO::OutputBuffer* lpLoaderOut,
                                                  PoolIO::OutputBuffer* lpPoolOut)
    {
        const BundleLoaderIO::OutputBuffer::LoadBundleResponseQueue* lpLoads = lpLoaderOut->GetLoadBundleResponseQueue();
        for (s32 li = 0; li < lpLoads->GetLength(); ++li)
        {
            const Events::LoadBundleResponse& lrResponse = lpLoads->GetEvent(li);
            if (lrResponse.mpUser)
                lrResponse.mpUser->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lrResponse), 2, sizeof(lrResponse));
        }
        const BundleLoaderIO::OutputBuffer::UnloadBundleResponseQueue* lpUnloads = lpLoaderOut->GetUnloadBundleResponseQueue();
        for (s32 li = 0; li < lpUnloads->GetLength(); ++li)
        {
            const Events::UnloadBundleResponse& lrResponse = lpUnloads->GetEvent(li);
            if (lrResponse.mpUser)
                lrResponse.mpUser->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lrResponse), 3, sizeof(lrResponse));
        }
        ProcessPoolOutputResponses(lpPoolOut);
    }

    // ARTIST @0x829070B0. Start the asynchronous read-stream open, build the
    // response carrying the returned handle, and retain it in the 16-slot
    // pending pool until FileSystem reports the stream OPEN.
    bool ResourceModule::AddOpenReadStreamRequest(
        const Events::OpenReadStreamRequest* lpRequest)
    {
        CgsFileSystem::ReadStream lStream = mFileSystem.OpenReadStream(
            lpRequest->GetFileName(), lpRequest->GetBuffer(),
            lpRequest->GetBufferSize(), lpRequest->GetNumBlocks(),
            lpRequest->GetNormalPriority(), lpRequest->GetHighPriority(),
            lpRequest->GetUseHDCache());

        Events::OpenReadStreamResponse* lpResponse =
            FileSystemAlloc<Events::OpenReadStreamResponse>();
        CGS_ASSERT(lpResponse != 0,
                   "Failed to allocate temporary memory for file system request\n");
        if (!lpResponse)
            return false;
        lpResponse->Construct(lpRequest->GetUser(), lpRequest->GetEventId(), lStream);

        PendingFileResponse* lpPending = mPendingFileResponses.Pop();
        CGS_ASSERT(lpPending != 0,
                   "Failed to allocate record for file system request\n");
        if (!lpPending)
        {
            FileSystemFree(lpResponse);
            return false;
        }
        lpPending->mpResponse = lpResponse;
        lpPending->meEvent = 16;
        lpPending->muFileId = 0xFFFFFFFFu;
        return lStream.IsValid();
    }

    // ARTIST @0x82905428. Retain the close response and its stream-table index,
    // then begin the asynchronous close. Completion is published only after the
    // FileSystem slot reaches CLOSED.
    bool ResourceModule::AddCloseReadStreamRequest(
        const Events::CloseReadStreamRequest* lpRequest)
    {
        const CgsFileSystem::ReadStream lStream = lpRequest->GetStream();
        Events::CloseReadStreamResponse* lpResponse =
            FileSystemAlloc<Events::CloseReadStreamResponse>();
        CGS_ASSERT(lpResponse != 0,
                   "Failed to allocate temporary memory for file system request\n");
        if (!lpResponse)
            return false;
        lpResponse->Construct(lpRequest->GetUser(), lpRequest->GetEventId(), lStream);

        PendingFileResponse* lpPending = mPendingFileResponses.Pop();
        CGS_ASSERT(lpPending != 0,
                   "Failed to allocate record for file system request\n");
        if (!lpPending)
        {
            FileSystemFree(lpResponse);
            return false;
        }
        lpPending->mpResponse = lpResponse;
        lpPending->meEvent = 18;
        lpPending->muFileId = mFileSystem.GetReadStreamIndex(lStream);
        mFileSystem.CloseReadStream(lStream);
        return true;
    }

    // ARTIST @0x828F42B8. Snapshot the live pending records so completing one
    // can safely return its index to the pool during the walk.
    void ResourceModule::ProcessPendingFileSystemResponses()
    {
        PendingFileResponse*
            lapPending[KI_MAX_PENDING_FILE_SYSTEM_RESPONSES] = {};
        const s32 liCount = mPendingFileResponses.Get(
            lapPending, KI_MAX_PENDING_FILE_SYSTEM_RESPONSES);

        for (s32 li = 0; li < liCount; ++li)
        {
            PendingFileResponse* lpPending = lapPending[li];
            bool lbComplete = false;
            CgsModule::BaseEventReceiverQueue* lpUser = 0;
            s32 liResponseSize = 0;

            if (lpPending->meEvent == 16)
            {
                Events::OpenReadStreamResponse* lpResponse =
                    static_cast<Events::OpenReadStreamResponse*>(
                        lpPending->mpResponse);
                lbComplete = mFileSystem.IsReadStreamOpen(lpResponse->GetStream());
                lpUser = lpResponse->GetUser();
                liResponseSize = sizeof(*lpResponse);
            }
            else if (lpPending->meEvent == 18)
            {
                Events::CloseReadStreamResponse* lpResponse =
                    static_cast<Events::CloseReadStreamResponse*>(
                        lpPending->mpResponse);
                lbComplete = mFileSystem.IsReadStreamClosed(
                    static_cast<s32>(lpPending->muFileId));
                lpUser = lpResponse->GetUser();
                liResponseSize = sizeof(*lpResponse);
            }
            else
            {
                CGS_ASSERT(false, "Unexpected pending file response\n");
            }

            if (!lbComplete)
                continue;

            if (lpUser)
                lpUser->AddEvent(
                    static_cast<const CgsModule::Event*>(
                        lpPending->mpResponse),
                    lpPending->meEvent, liResponseSize);

            if (lpPending->meEvent == 16)
                FileSystemFree(static_cast<Events::OpenReadStreamResponse*>(
                    lpPending->mpResponse));
            else
                FileSystemFree(static_cast<Events::CloseReadStreamResponse*>(
                    lpPending->mpResponse));

            const s8 liIndex = mPendingFileResponses.GetObjectIndex(lpPending);
            mPendingFileResponses.Push(liIndex);
        }
    }

    // Drain the pool module's output queue (DoAcquireResourceRequest etc. responses) and forward each to
    // its requester's receiver queue (response->mpUser). The pool-response slice of ProcessResourceResponses
    // (X360 0x828EC788): the console translates each pool-output tag through the static table
    // dword_820F7194[tag] before posting to mpUser -- the ACQUIRE response (pool tag 6) reaches the
    // requester as receiver id 4, which is the id GameDataModule's internal drain (and every X360
    // receiver-side consumer) switches on. Only the tags the PC pool module emits are mapped; unmapped
    // tags pass through unchanged (their producers land with their own passes).
    void ResourceModule::ProcessPoolOutputResponses(PoolIO::OutputBuffer* lpPoolOut)
    {
        const PoolIO::OutputBuffer::PoolOutputQueue* lpQ =
            static_cast<const PoolIO::OutputBuffer&>(*lpPoolOut).GetPoolOutputQueue();

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liId = lpQ->GetFirstEvent(&lpEvent, &liSize);
        while (liId != -1 && lpEvent != 0)
        {
            CgsModule::BaseEventReceiverQueue* lpUser =
                reinterpret_cast<const Events::PoolEvent*>(lpEvent)->mpUser;
            if (lpUser != 0 && liId >= 0 && liId < 15)
            {
                // dword_820F7194[tag]: [6] == 4 (acquire), [7] == 5 (acquire-list). Both values are
                // read straight out of the ARTIST image's translation table, so the acquire mapping
                // that was already committed here doubles as the calibration for the new one.
                // ARTIST820F7194, calibrated by acquire6->4/acquire-list7->5.
                static const s32 kaiReceiverTags[15] =
                    {26,0,26,1,26,26,4,5,26,6,24,7,8,7,8};
                const s32 liUserId = kaiReceiverTags[liId];
                lpUser->AddEvent(lpEvent, liUserId, liSize);
            }
            const CgsModule::Event* lpNext = 0;
            liId = lpQ->GetNextEvent(lpEvent, &lpNext, &liSize);
            lpEvent = lpNext;
        }
    }

    // ARTIST82907948: pump loader, pool and memory in order. Allocation/fixup
    // responses are retained until the next update, as in the original pipeline.
    ResourceModule::EUpdateResult ResourceModule::Update(CgsModule::IOBufferStack* lpInputStack,
        CgsModule::IOBufferStack* lpOutputStack, const ResourceIO::InputBuffer* lpInput,
        ResourceIO::OutputBuffer* lpOutput)
    {
        // ARTIST82907948: these buffers belong to this invocation and return to
        // their original stacks in reverse construction order.
        CgsModule::IOHelper<CgsMemory::MemoryIO::InputBuffer> lMemIn(lpInputStack, "Memory");
        CgsModule::IOHelper<BundleLoaderIO::InputBuffer_Update> lLoaderIn(lpInputStack, "BundleLoader");
        CgsModule::IOHelper<PoolIO::InputBuffer> lPoolIn(lpInputStack, "Pool");
        CgsModule::IOHelper<CgsMemory::MemoryIO::OutputBuffer> lMemOut(lpOutputStack, "Memory");
        CgsModule::IOHelper<BundleLoaderIO::OutputBuffer> lLoaderOut(lpOutputStack, "BundleLoader");
        CgsModule::IOHelper<PoolIO::OutputBuffer> lPoolOut(lpOutputStack, "Pool");

        ProcessPendingFileSystemResponses();
        lpInput->LockForRead();
        lMemIn->LockForWrite(); lLoaderIn->LockForWrite(); lPoolIn->LockForWrite();
        ProcessResourceRequests(lpInput, lMemIn, lLoaderIn, lPoolIn);
        lPoolIn->UnlockForWrite(); lLoaderIn->UnlockForWrite(); lMemIn->UnlockForWrite();
        lpInput->UnlockForRead();

        mBundleLoaderModule.Update(lpInputStack, lpOutputStack, lLoaderIn, lLoaderOut);
        lLoaderOut->LockForRead();
        ProcessBundleLoaderStreamRequests(lLoaderOut);
        lLoaderOut->UnlockForRead();

        lPoolIn->LockForWrite(); lLoaderOut->LockForRead();
        lPoolIn->GetPoolInputQueue()->Append(
            *static_cast<const BundleLoaderIO::OutputBuffer*>(lLoaderOut)->GetPoolSendQueue());
        lLoaderOut->UnlockForRead(); lPoolIn->UnlockForWrite();
        const EUpdateResult leResult = mPoolModule.Update(lpInputStack, lpOutputStack, lPoolIn, lPoolOut)
            == PoolModule::E_RESULT_REQUIRE_STALL ? E_RESULT_REQUIRE_STALL : E_RESULT_OK;
        {
            CgsModule::IOHelper<BundleLoaderIO::InputBuffer_Record> lRecord(lpInputStack, "BundleLoaderRecord");
            lRecord->LockForWrite(); lPoolOut->LockForRead();
            lRecord->GetPoolReceiveQueue()->Append(
                *static_cast<const PoolIO::OutputBuffer*>(lPoolOut)->GetPoolOutputQueue());
            lPoolOut->UnlockForRead(); lRecord->UnlockForWrite();
            mBundleLoaderModule.RecordPostUpdateEvents(lRecord);
        }
        lMemIn->LockForWrite(); lPoolOut->LockForRead();
        ProcessPoolResourceRequests(lMemIn, lPoolOut);
        lPoolOut->UnlockForRead(); lMemIn->UnlockForWrite();
        mMemoryModule.Update(lpInputStack, lpOutputStack, lMemIn, lMemOut);
        lMemOut->LockForRead(); ProcessMemoryResponses(lMemOut); lMemOut->UnlockForRead();
        lPoolOut->LockForRead(); lLoaderOut->LockForRead();
        ProcessResourceResponses(lLoaderOut, lPoolOut);
        lLoaderOut->UnlockForRead(); lPoolOut->UnlockForRead();
        lpOutput->LockForWrite();
        lpOutput->GetFileSystemStatusInterface()->SetDiskErrorOccured(mFileSystem.HasDiskErrorOccured());
        lpOutput->UnlockForWrite();
        return leResult;
    }

    // @ 0x829019F0 - forward the pool module's emitted resource-memory requests into the MemoryModule
    // input queue. Each pool output event tagged 10 (CreatePool) / 13 (DeletePool) becomes a memory
    // request (queue tag 1). [X360 hardcodes the per-type size 92/16; we use the recorded event size so
    // the x64-width CreateResourceRequest copies whole.]
    void ResourceModule::ProcessPoolResourceRequests(CgsMemory::MemoryIO::InputBuffer* lpMemInput,
                                                     PoolIO::OutputBuffer* lpPoolOutput)
    {
        const PoolIO::OutputBuffer::PoolResourceRequestQueue* lpQ =
            static_cast<const PoolIO::OutputBuffer&>(*lpPoolOutput).GetPoolResourceRequestQueue();
        CgsMemory::MemoryIO::InputBuffer::MemoryRequestQueue* lpMemQ = lpMemInput->GetMemoryRequestQueue();

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liId = lpQ->GetFirstEvent(&lpEvent, &liSize);
        while (liId != -1 && lpEvent != 0)
        {
            if (liId == 10 || liId == 13)   // CreatePool / DeletePool memory requests
                lpMemQ->AddEvent(lpEvent, 1, liSize);
            else
                CGS_ASSERT(false, "Unexpected request from bundle loader\n");

            const CgsModule::Event* lpNext = 0;
            liId = lpQ->GetNextEvent(lpEvent, &lpNext, &liSize);
            lpEvent = lpNext;
        }
    }

    // @ 0x828EC6F0 - route each MemoryModule response to its originating receiver queue (the request's
    // reply target, response->GetUser()) tagged for that response type. The X360 indexes a static table
    // dword_820F71D0[meEventType]; the pool-create flow carries only CREATE_RESOURCE(6) -> CreatePool(10).
    void ResourceModule::ProcessMemoryResponses(CgsMemory::MemoryIO::OutputBuffer* lpMemOutput)
    {
        const CgsMemory::MemoryIO::OutputBuffer::MemoryResponseQueue* lpQ =
            static_cast<const CgsMemory::MemoryIO::OutputBuffer&>(*lpMemOutput).GetMemoryResponseQueue();

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liId = lpQ->GetFirstEvent(&lpEvent, &liSize);
        while (liId != -1 && lpEvent != 0)
        {
            const CgsMemory::MemoryIO::MemoryResponse* lpResp =
                static_cast<const CgsMemory::MemoryIO::MemoryResponse*>(lpEvent);
            CgsModule::BaseEventReceiverQueue* lpReceiver = lpResp->GetUser();
            if (lpReceiver != 0)
            {
                // ARTIST820F71D0; CREATE_RESOURCE6->10 is the pool-create path.
                static const s32 kaiReceiverTags[7] = {9,13,11,12,14,15,10};
                const s32 liReceiverTag = kaiReceiverTags[lpResp->GetEventType()];
                lpReceiver->AddEvent(lpEvent, liReceiverTag, liSize);
            }
            const CgsModule::Event* lpNext = 0;
            liId = lpQ->GetNextEvent(lpEvent, &lpNext, &liSize);
            lpEvent = lpNext;
        }
    }

    // @ 0x828D8310 - write streams are deprecated: unconditionally assert and reject. The
    // request param is unused (the X360 body is a bare de-inlined assert + return false).
    bool ResourceModule::AddOpenWriteStreamRequest(const Events::OpenWriteStreamRequest* /*lpRequest*/)
    {
        CGS_ASSERT(false, "Write streams deprecated\n");
        return false;
    }

    // @ 0x828D83A0 - write streams are deprecated: unconditionally assert and reject. The
    // request param is unused (the X360 body is a bare de-inlined assert + return false).
    bool ResourceModule::AddCloseWriteStreamRequest(const Events::CloseWriteStreamRequest* /*lpRequest*/)
    {
        CGS_ASSERT(false, "Write streams deprecated\n");
        return false;
    }

    // CgsResource:: (unnamed by IDA) @ X360 0x828F2890 -- fixed-slot free-list pop.
    // Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX. Pops the head entry of a
    // small u8-indexed free list and returns the pointer to its stride-12 record; returns
    // null when the list is empty. Called by ResourceModule's file-system request adders
    // (AddOpenFileRequest / AddCloseFileRequest / AddOpenReadStreamRequest /
    // AddCloseReadStreamRequest) to reserve a request record slot.
    //
    // FLAG (confidence low): the owning class/member names are not recovered (IDA left the
    // symbol unnamed). The pool shape is fully attested by the asm; only the naming/home is
    // provisional. Modelled as a free helper over an opaque 4-field POD pool so the byte
    // behaviour is exact.
    //
    // Pool layout (X360 byte offsets, base is u8*):
    //   +0x00  u8*  mpRecords      // stride-12 record array base
    //   +0x04  u8*  mpFreeIndices  // free-index ring (u8 entries)
    //   +0x08  u8   muUsedCount    // slots in use
    //   +0x09  u8   muFreeCount    // slots free (loop count)
    namespace Detail
    {
        struct RequestSlotPool
        {
            u8* mpRecords;       // +0x00
            u8* mpFreeIndices;   // +0x04
            u8  muUsedCount;     // +0x08
            u8  muFreeCount;     // +0x09
        };

        // X360 0x828F2890: pop the head free index, compact the ring, bump the used count,
        // and return &mpRecords[12 * index]; null when the pool is empty.
        u8* AllocRequestSlot(RequestSlotPool* lpPool)
        {
            s8 lcFree = static_cast<s8>(lpPool->muFreeCount);
            s8 lcIndex;
            if (lcFree != 0)
            {
                u8* lpFree = lpPool->mpFreeIndices;
                lcIndex = static_cast<s8>(lpFree[0]);
                lpFree[0] = lpFree[lpPool->muFreeCount - 1];   // pull tail into head
                ++lpPool->muUsedCount;
                lpPool->muFreeCount = static_cast<u8>(lpPool->muFreeCount - 1);
            }
            else
            {
                lcIndex = -1;
            }

            if (lcIndex == -1)
                return 0;
            return lpPool->mpRecords + 12 * lcIndex;
        }
    }
}
