#include "GameShared/GameClasses/System/Resource/CgsResourceModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/System/Resource/CgsPoolModuleIO.h"  // PoolIO::Input/OutputBuffer (shuttle)
#include "GameShared/GameClasses/System/Resource/CgsResourceModuleIO.h" // ResourceIO::InputBuffer (module input)
#include "GameShared/GameClasses/Memory/CgsMemoryModuleIO.h"         // MemoryIO buffers + MemoryResponse
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h" // receiver-queue forward target
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistry.h" // ResolveResourceType (bundle FixUp resolver)
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"     // Events::PoolEvent (pool response routing)
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Module/CgsModuleIOHelper.h"
#include "GameShared/GameClasses/Module/CgsModuleUtils.h"
#include "pc/gcm/renderengine/FrameProfile.h"
#include <cstring>                                    // memset (InitOptions zero-init)

// CgsResource::ResourceModule: native module lifetime and staged request shuttles.
// Update preserves the loader -> pool -> memory order and delayed pool replies.
namespace CgsResource
{
    // Original initial monitor words are -1; trace IDs are reset to zero by Construct.
    s32 ResourceModule::_miUpdateTrace = 0; // target82F34040
    // ARTIST 82F34004..1C raw initial words are all -1.
    s32 ResourceModule::_miResourceUpdatePerfMon = -1;
    s32 ResourceModule::_miProcessResourceRequestsPerfMon = -1;
    s32 ResourceModule::_miUpdateBundleLoaderPerfMon = -1;
    s32 ResourceModule::_miUpdatePoolPerfMon = -1;
    s32 ResourceModule::_miUpdateMemoryPerfMon = -1;
    s32 ResourceModule::_miUpdateFileSystemPerfMon = -1;
    s32 ResourceModule::_miUpdateOutputPerfMon = -1;

    // ARTIST828F4558: r3 +=0x2C380, preserving callback r4 and context r5.
    void ResourceModule::DebugReportPools(PoolModule::FPoolReportCallback lpfnCallback,
                                         void* lpUserData)
    {
        mPoolModule.DebugReport(lpfnCallback, lpUserData);
    }


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
        // [reliable] The X360 sets *(this+4)=1 (mbIsNewModule) in Construct; that Construct chain isn't
        // wired for the embedded static yet, and the header ctor doesn't take effect on it, so set it
        // here (before the base Prepare) so ModuleSingleBuffered skips the old DataStructure IO path
        // (and its virtual CreateInputDataStructure). Move to Construct once the bring-up chain lands.
        mbIsNewModule = true;
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
            mBundleLoaderModule.CancelStreamPC();
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

    // ARTIST829055B0..6568; all allocations use the actual supplied owner.
    void ResourceModule::Construct(InitOptions* lpOptions, rw::IResourceAllocator* lpAllocator,
                                   rw::IResourceAllocator* lpDebugAllocator)
    {
        CgsModule::ModuleSingleBuffered::Construct();
        meReleaseStage = E_RELEASE_DONE;
        mePrepareStage = E_PREPARE_START;
        mMemoryModule.Construct(&lpOptions->mMemoryInitOptions, lpAllocator);
        mBundleLoaderModule.Construct(&lpOptions->mLoaderInitOptions, lpAllocator, lpDebugAllocator);
        mPoolModule.Construct(&lpOptions->mPoolInitOptions, lpAllocator, lpDebugAllocator);
        mDebugComponent.Construct(this, &lpOptions->mDebugParams);

        rw::ResourceDescriptor lCapacity;
        for (u32 luLane = 0; luLane < rw::KU_RESOURCE_LANE_COUNT; ++luLane)
        {
            lCapacity.m_baseResourceDescriptors[luLane].m_size = 0;
            lCapacity.m_baseResourceDescriptors[luLane].m_alignment = 1;
        }
        lCapacity.m_baseResourceDescriptors[0].m_size = 0x160000;
        lCapacity.m_baseResourceDescriptors[0].m_alignment = 16;
        rw::ResourceDescriptor lDescriptor;
        rw::core::GeneralResourceAllocator::GetResourceDescriptor(&lDescriptor, &lCapacity);
        const rw::Resource lResource = lpAllocator->DoAllocate(lDescriptor, nullptr);
        mpFileSystemAllocator = rw::core::GeneralResourceAllocator::Initialize(lResource, lCapacity);
        mFileSystem.Construct(mpFileSystemAllocator, lpDebugAllocator,
                              lpOptions->macFileSearchPath, lpOptions->mpDiskLayout);
        mPendingFileResponses.Construct(maPendingFileResponses, maiPendingFileResponseFreeIndices,
                                       static_cast<s8>(KI_MAX_PENDING_FILE_SYSTEM_RESPONSES));

        const auto RegisterMonitor = [](s32& liMonitor, const char* lpcName,
                                        const char* lpcAssertion, s32 liLine)
        {
            if (liMonitor == -1)
            {
                liMonitor = CgsDev::PerfMonCpu::AddMonitor(lpcName, CgsDev::E_PMP_1, false, 5.0f, false);
                if (liMonitor < 0)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(lpcAssertion,
                        "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp",
                        liLine);
                    CgsDev::Assert::EndAssert();
                }
            }
        };
        RegisterMonitor(ResourceModule::_miResourceUpdatePerfMon, "Resource update",
            "CgsResource::ResourceModule::_miResourceUpdatePerfMon >= 0", 100);
        RegisterMonitor(ResourceModule::_miProcessResourceRequestsPerfMon, "  Process requests",
            "CgsResource::ResourceModule::_miProcessResourceRequestsPerfMon >= 0", 101);
        RegisterMonitor(ResourceModule::_miUpdateBundleLoaderPerfMon, "  Bundle loader",
            "CgsResource::ResourceModule::_miUpdateBundleLoaderPerfMon >= 0", 102);
        RegisterMonitor(ResourceModule::_miUpdatePoolPerfMon, "  Pool",
            "CgsResource::ResourceModule::_miUpdatePoolPerfMon >= 0", 103);
        RegisterMonitor(ResourceModule::_miUpdateMemoryPerfMon, "  Memory",
            "CgsResource::ResourceModule::_miUpdateMemoryPerfMon >= 0", 104);
        RegisterMonitor(ResourceModule::_miUpdateFileSystemPerfMon, "  File system",
            "CgsResource::ResourceModule::_miUpdateFileSystemPerfMon >= 0", 105);
        RegisterMonitor(ResourceModule::_miUpdateOutputPerfMon, "  Output",
            "CgsResource::ResourceModule::_miUpdateOutputPerfMon >= 0", 106);
        RegisterMonitor(Pool::_miPoolModuleStatePerfMon, "Pool state update",
            "CgsResource::Pool::_miPoolModuleStatePerfMon >= 0", 108);
        RegisterMonitor(Pool::_miPoolModuleIdle, "  PS Idle",
            "CgsResource::Pool::_miPoolModuleIdle >= 0", 109);
        RegisterMonitor(Pool::_miPoolModuleAllocating, "  PS Alloc",
            "CgsResource::Pool::_miPoolModuleAllocating >= 0", 110);
        RegisterMonitor(Pool::_miPoolModuleDeallocating, "  PS DeAlloc",
            "CgsResource::Pool::_miPoolModuleDeallocating >= 0", 111);
        RegisterMonitor(Pool::_miPoolModuleLiveupdate, "  PS LiveUpd",
            "CgsResource::Pool::_miPoolModuleLiveupdate >= 0", 112);
        RegisterMonitor(Pool::_miPoolModuleIntellifrag, "  PS Intellifrag",
            "CgsResource::Pool::_miPoolModuleIntellifrag >= 0", 113);
        RegisterMonitor(Pool::_miPoolModuleEmergencyFrag, "  PS Emergencyfrag",
            "CgsResource::Pool::_miPoolModuleEmergencyFrag >= 0", 114);
        RegisterMonitor(CgsMemory::Relocator::_miExecutePerfMon, "    Relocator Exec",
            "CgsMemory::Relocator::_miExecutePerfMon >= 0", 115);
        RegisterMonitor(CgsMemory::Relocator::_miGPUCopyPerfMon, "      Relocator GPU",
            "CgsMemory::Relocator::_miGPUCopyPerfMon >= 0", 116);
        RegisterMonitor(CgsMemory::Relocator::_miUpdatePerfMon, "    Relocator Update",
            "CgsMemory::Relocator::_miUpdatePerfMon >= 0", 117);
        RegisterMonitor(Pool::_miPoolUpdatePerfMon, "Pool update",
            "CgsResource::Pool::_miPoolUpdatePerfMon >= 0", 119);
        RegisterMonitor(Pool::_miPoolDeleteHashEntryPerfMon, "  Delete hash entry",
            "CgsResource::Pool::_miPoolDeleteHashEntryPerfMon >= 0", 120);
        RegisterMonitor(Pool::_miPoolDeleteMemoryPerfMon, "  Delete memory",
            "CgsResource::Pool::_miPoolDeleteMemoryPerfMon >= 0", 121);
        RegisterMonitor(Pool::_miPoolDeleteResourcePerfMon, "  Delete resource",
            "CgsResource::Pool::_miPoolDeleteResourcePerfMon >= 0", 122);
        RegisterMonitor(Pool::_miPoolAllocateResourceListPerfMon, "Pool alloc list",
            "CgsResource::Pool::_miPoolAllocateResourceListPerfMon >= 0", 124);
        RegisterMonitor(Pool::_miPoolFixupPerfMon, "Pool fixup",
            "CgsResource::Pool::_miPoolFixupPerfMon >= 0", 138);
        RegisterMonitor(Pool::_miPoolFindByIndexPerfMon, "Pool search by index",
            "CgsResource::Pool::_miPoolFindByIndexPerfMon >= 0", 139);
        RegisterMonitor(Pool::_miPoolFixupResourcePerfMon, "Pool fixup resource",
            "CgsResource::Pool::_miPoolFixupResourcePerfMon >= 0", 140);
        RegisterMonitor(Pool::_miPoolFindTypePerfMon, "Pool find resource type",
            "CgsResource::Pool::_miPoolFindTypePerfMon >= 0", 141);
        RegisterMonitor(Pool::_miPoolDefragPerfMon, "Pool defrag",
            "CgsResource::Pool::_miPoolDefragPerfMon >= 0", 143);
        RegisterMonitor(Pool::_miPoolDefragWFInitialDeathsPerfMon, "  Pool WF Initial Deaths",
            "CgsResource::Pool::_miPoolDefragWFInitialDeathsPerfMon >= 0", 144);
        RegisterMonitor(Pool::_miPoolDefragMakeTempPerfMon, "  Pool defrag make temp",
            "CgsResource::Pool::_miPoolDefragMakeTempPerfMon >= 0", 145);
        RegisterMonitor(Pool::_miPoolDefragMoveToTempPerfMon, "    Move temp",
            "CgsResource::Pool::_miPoolDefragMoveToTempPerfMon >= 0", 146);
        RegisterMonitor(Pool::_miPoolDefragResolveTempPerfMon, "    Temp resolve",
            "CgsResource::Pool::_miPoolDefragResolveTempPerfMon >= 0", 147);
        RegisterMonitor(Pool::_miPoolDefragWFSourceDeathsPerfMon, "  Pool WF Source Deaths",
            "CgsResource::Pool::_miPoolDefragWFSourceDeathsPerfMon >= 0", 148);
        RegisterMonitor(Pool::_miPoolDefragMakeDestPerfMon, "  Pool defrag make dest",
            "CgsResource::Pool::_miPoolDefragMakeDestPerfMon >= 0", 149);
        RegisterMonitor(Pool::_miPoolDefragMoveToDestPerfMon, "    Move dest",
            "CgsResource::Pool::_miPoolDefragMoveToDestPerfMon >= 0", 150);
        RegisterMonitor(Pool::_miPoolDefragMoveDestRebasePerfMon, "      Dest rebase",
            "CgsResource::Pool::_miPoolDefragMoveDestRebasePerfMon >= 0", 151);
        RegisterMonitor(Pool::_miPoolDefragResolveDestPerfMon, "    Dest resolve",
            "CgsResource::Pool::_miPoolDefragResolveDestPerfMon >= 0", 152);
        RegisterMonitor(Pool::_miPoolDefragWFTempDeathsPerfMon, "  Pool WF Temp Deaths",
            "CgsResource::Pool::_miPoolDefragWFTempDeathsPerfMon >= 0", 153);
        RegisterMonitor(Pool::_miPoolDefragWFDonePerfMon, "  Pool Done",
            "CgsResource::Pool::_miPoolDefragWFDonePerfMon >= 0", 154);
        RegisterMonitor(BundleLoaderModule::_miUpdatePerfMon, "Loader update",
            "CgsResource::BundleLoaderModule::_miUpdatePerfMon >= 0", 156);
        RegisterMonitor(BundleLoaderModule::_miStreamUpdate, "  Stream update",
            "CgsResource::BundleLoaderModule::_miStreamUpdate >= 0", 157);
        RegisterMonitor(BundleLoaderModule::_miStreamEntryCopy, "    Entry MemCpy",
            "CgsResource::BundleLoaderModule::_miStreamEntryCopy >= 0", 158);
        RegisterMonitor(BundleLoaderModule::_miStreamDataCopy, "    Data MemCpy",
            "CgsResource::BundleLoaderModule::_miStreamDataCopy >= 0", 159);

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
        mbPendingPatch = false;
    }

    // Retained native two-input construction path.
    // FileSystem uses its existing native device-manager adapter.
    void ResourceModule::Construct(const void* lpInitOptions, void* lpAllocator)
    {
        if (lpInitOptions == 0 || lpAllocator == 0)
            return;
        const InitOptions* lpOptions = static_cast<const InitOptions*>(lpInitOptions);
        rw::IResourceAllocator* lpRwAllocator = static_cast<rw::IResourceAllocator*>(lpAllocator);

        mMemoryModule.Construct(
            const_cast<CgsMemory::MemoryModule::InitOptions*>(&lpOptions->mMemoryInitOptions),
            lpRwAllocator);
        mFileSystem.Construct();
        mPoolModule.Construct(&lpOptions->mPoolInitOptions, lpAllocator);
        mBundleLoaderModule.ConstructNativePC(&lpOptions->mLoaderInitOptions, lpRwAllocator,
                                      lpOptions->mDebugParams.mpDebugAllocator);
        mDebugComponent.Construct(this, &lpOptions->mDebugParams);
    }
    // ARTIST828EC6B0, with retirement of native pending-response allocations.
    void ResourceModule::Destruct()
    {
        // FLAG PC-platform leaf: temporary native response records are owned
        // by this shuttle. Release has closed the stream slots before teardown.
        PendingFileResponse* lapPending[KI_MAX_PENDING_FILE_SYSTEM_RESPONSES] = {};
        const s32 liCount = mPendingFileResponses.Get(lapPending, KI_MAX_PENDING_FILE_SYSTEM_RESPONSES);
        for (s32 li = 0; li < liCount; ++li)
        {
            if (lapPending[li]->meEvent == 16)
                delete static_cast<Events::OpenReadStreamResponse*>(lapPending[li]->mpResponse);
            else if (lapPending[li]->meEvent == 18)
                delete static_cast<Events::CloseReadStreamResponse*>(lapPending[li]->mpResponse);
            mPendingFileResponses.Push(mPendingFileResponses.GetObjectIndex(lapPending[li]));
        }
        // ARTIST828EC6B0.
        mFileSystem.Destruct();
        CgsModule::ModuleSingleBuffered::Destruct();
    }

    // @ 0x82907268 - route inbound resource requests to the sub-module inputs. Pool-create slice: a
    // CreatePool request (id 0) is forwarded to the pool input queue. [The bundle/file/memory request
    // routes (ids 2/3/9..0xD) are deferred -- pool bring-up only sends CreatePool. X360 forwards size 172;
    // we use the recorded event size so the native canonical CreatePoolRequest copies whole.]
    void ResourceModule::ProcessResourceRequests(ResourceIO::InputBuffer* lpResIn, PoolIO::InputBuffer* lpPoolIn)
    {
        const ResourceIO::ResourceRequestQueue<16384>* lpQ =
            static_cast<const ResourceIO::InputBuffer&>(*lpResIn).GetResourceQueue();
        PoolIO::InputBuffer::PoolInputQueue* lpPoolQ = lpPoolIn->GetPoolInputQueue();

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liId = lpQ->GetFirstEvent(&lpEvent, &liSize);
        while (liId != -1 && lpEvent != 0)
        {
            if (liId == 0)        // CreatePool -> pool input
                lpPoolQ->AddEvent(lpEvent, 0, liSize);
            else if (liId == 2)   // LoadBundle -> bundle loader's load-request intake
                mBundleLoaderModule.EnqueueLoadRequest(*reinterpret_cast<const Events::LoadBundleRequest*>(lpEvent));
            else if (liId == 3)   // UnloadBundle -> bundle loader's unload-request intake
                mBundleLoaderModule.EnqueueUnloadRequest(*reinterpret_cast<const Events::UnloadBundleRequest*>(lpEvent));
            else if (liId == 4)   // AcquireResource -> pool input (X360 routes id 4 -> pool tag 4)
                lpPoolQ->AddEvent(lpEvent, 4, liSize);
            else if (liId == 5)   // AcquireResourceList -> pool input (X360 routes id 5 -> pool tag 5)
                lpPoolQ->AddEvent(lpEvent, 5, liSize);
            else if (liId == 16)  // OpenReadStream -> asynchronous FileSystem + pending response
                AddOpenReadStreamRequestNativePC(
                    reinterpret_cast<const Events::OpenReadStreamRequest*>(lpEvent));
            else if (liId == 18)  // CloseReadStream -> asynchronous FileSystem + pending response
                AddCloseReadStreamRequestNativePC(
                    reinterpret_cast<const Events::CloseReadStreamRequest*>(lpEvent));
            const CgsModule::Event* lpNext = 0;
            liId = lpQ->GetNextEvent(lpEvent, &lpNext, &liSize);
            lpEvent = lpNext;
        }
    }

    // ARTIST82907268: native event sizes are carried by the queue. Pool and
    // memory tags below are the original switch values, not resource tags.
    void ResourceModule::ProcessResourceRequests(const ResourceIO::InputBuffer* lpResIn,
        CgsMemory::MemoryIO::InputBuffer* lpMemIn,
        BundleLoaderIO::InputBuffer_Update* lpLoaderIn, PoolIO::InputBuffer* lpPoolIn)
    {
        const auto* lpQueue = static_cast<const ResourceIO::InputBuffer*>(lpResIn)->GetResourceQueue();
        auto* lpPoolQueue = lpPoolIn->GetPoolInputQueue();
        const CgsModule::Event* lpEvent = nullptr;
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
            case 20: AddOpenFileRequest(reinterpret_cast<const Events::OpenFileRequest*>(lpEvent)); break;
            case 21: AddCloseFileRequest(reinterpret_cast<const Events::CloseFileRequest*>(lpEvent)); break;
            default: CGS_ASSERT(false, "Invalid event id\n"); break;
            }
            const CgsModule::Event* lpNext = nullptr;
            liTag = lpQueue->GetNextEvent(lpEvent, &lpNext, &liSize);
            lpEvent = lpNext;
        }
    }

    // ARTIST82907580..82907630 (export hole).
    void ResourceModule::ProcessBundleLoaderStreamRequests(const BundleLoaderIO::OutputBuffer* lpLoaderOut)
    {
        const auto* lpQueue = lpLoaderOut->GetStreamRequestQueue();
        const CgsModule::Event* lpEvent = nullptr;
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
            const CgsModule::Event* lpNext = nullptr;
            liTag = lpQueue->GetNextEvent(lpEvent, &lpNext, &liSize);
            lpEvent = lpNext;
        }
    }

    // ARTIST828EC788: bundle replies precede externally addressed pool replies.
    void ResourceModule::ProcessResourceResponses(const BundleLoaderIO::OutputBuffer* lpLoaderOut,
                                                  const PoolIO::OutputBuffer* lpPoolOut)
    {
        const auto* lpLoads = lpLoaderOut->GetLoadBundleResponseQueue();
        for (s32 li = 0; li < lpLoads->GetLength(); ++li)
        {
            const auto& lrResponse = lpLoads->GetEvent(li);
            if (lrResponse.mpUser)
                lrResponse.mpUser->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lrResponse), 2, sizeof(lrResponse));
        }
        const auto* lpUnloads = lpLoaderOut->GetUnloadBundleResponseQueue();
        for (s32 li = 0; li < lpUnloads->GetLength(); ++li)
        {
            const auto& lrResponse = lpUnloads->GetEvent(li);
            if (lrResponse.mpUser)
                lrResponse.mpUser->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lrResponse), 3, sizeof(lrResponse));
        }
        ProcessPoolOutputResponses(lpPoolOut);
    }







    // ARTIST828F2900/2998/2A30/2AC8: real owned response storage, not C++ new.
    template <typename T>
    void ResourceModule::FileSystemAlloc(T** lppResult)
    {
        rw::ResourceDescriptor lDescriptor;
        // FLAG PC-platform leaf: actual event sizeof follows native pointer width;
        // preserve original main-only allocation, alignment16 and null name.
        lDescriptor.m_baseResourceDescriptors[0].m_size = static_cast<u32>(sizeof(T));
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 16;
        const rw::Resource lResource = mpFileSystemAllocator->DoAllocate(lDescriptor, nullptr);
        *lppResult = static_cast<T*>(lResource.m_baseResources[0]);
    }

    // ARTIST828EA560/5C0/620/680: five zero lanes, actual main pointer, same owner.
    template <typename T>
    void ResourceModule::FileSystemFree(T* lpAddress)
    {
        rw::Resource lResource;
        lResource.m_baseResources[0] = lpAddress;
        mpFileSystemAllocator->DoFree(lResource);
    }

    // Original request assertions stream only these actual messages/source lines.
    namespace
    {
        void AssertFileResponseAvailable(bool lbAvailable, const char* lpcMessage, s32 liLine)
        {
            if (!lbAvailable)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::MessageBufferPC lBuffer;
                CgsDev::StrStream lStream(lBuffer.GetBuffer(), lBuffer.GetSize());
                lStream << lpcMessage;
                CgsDev::Assert::FireAssert(lBuffer.GetBuffer(),
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.h",
                    liLine);
                CgsDev::Assert::EndAssert();
            }
        }
    }

    // ARTIST829070B0. The last original bool is stack-passed at829070FC.
    bool ResourceModule::AddOpenReadStreamRequest(const Events::OpenReadStreamRequest* lpRequest)
    {
        const bool lbUseHDCache = lpRequest->GetUseHDCache();
        const s32 liHighPriority = lpRequest->GetHighPriority();
        const s32 liNormalPriority = lpRequest->GetNormalPriority();
        const u32 luNumBlocks = lpRequest->GetNumBlocks();
        const u32 luBufferSize = lpRequest->GetBufferSize();
        void* const lpBuffer = lpRequest->GetBuffer();
        const CgsFileSystem::ReadStream lStream = mFileSystem.OpenReadStream(
            lpRequest->GetFileName(), lpBuffer, luBufferSize, luNumBlocks,
            liNormalPriority, liHighPriority, lbUseHDCache);
        Events::OpenReadStreamResponse* lpResponse;
        FileSystemAlloc(&lpResponse);
        AssertFileResponseAvailable(lpResponse != nullptr,
            "Failed to allocate temporary memory for file system request\n", 784);
        lpResponse->Construct(lpRequest->GetUser(), lpRequest->GetEventId(), lStream);
        PendingFileResponse* const lpPending = mPendingFileResponses.Pop();
        AssertFileResponseAvailable(lpPending != nullptr,
            "Failed to allocate record for file system request\n", 788);
        lpPending->mpResponse = lpResponse;
        lpPending->meEvent = 16;
        lpPending->muFileId = 0xFFFFFFFFu;
        return lStream.IsValid();
    }

    // ARTIST82905428: allocate/copy response, retain real slot, then start close.
    bool ResourceModule::AddCloseReadStreamRequest(const Events::CloseReadStreamRequest* lpRequest)
    {
        Events::CloseReadStreamResponse* lpResponse;
        FileSystemAlloc(&lpResponse);
        AssertFileResponseAvailable(lpResponse != nullptr,
            "Failed to allocate temporary memory for file system request\n", 860);
        lpResponse->Construct(lpRequest->GetUser(), lpRequest->GetEventId(), lpRequest->GetStream());
        PendingFileResponse* const lpPending = mPendingFileResponses.Pop();
        AssertFileResponseAvailable(lpPending != nullptr,
            "Failed to allocate record for file system request\n", 864);
        lpPending->mpResponse = lpResponse;
        lpPending->meEvent = 18;
        lpPending->muFileId = mFileSystem.GetReadStreamIndex(lpRequest->GetStream());
        mFileSystem.CloseReadStream(lpRequest->GetStream());
        return true;
    }

    // ARTIST828F3D30: the negative open result produces the actual null/zero handle.
    bool ResourceModule::AddOpenFileRequest(const Events::OpenFileRequest* lpRequest)
    {
        const bool lbUseHDCache = lpRequest->GetUseHDCache();
        const s32 liPriority = lpRequest->GetPriority();
        const CgsFileSystem::FileAccess leAccess = lpRequest->GetFileAccess();
        const s32 liFileId = mFileSystem.Open(lpRequest->GetFileName(), leAccess,
                                             liPriority, lbUseHDCache);
        CgsFileSystem::FileHandle lFile;
        if (liFileId >= 0)
            lFile.Construct(&mFileSystem, static_cast<u32>(liFileId));
        else
            lFile.Construct(nullptr, 0);
        Events::OpenFileResponse* lpResponse;
        FileSystemAlloc(&lpResponse);
        AssertFileResponseAvailable(lpResponse != nullptr,
            "Failed to allocate temporary memory for file system request\n", 937);
        lpResponse->Construct(lpRequest->GetUser(), lpRequest->GetEventId());
        lpResponse->SetFileHandle(lFile);
        PendingFileResponse* const lpPending = mPendingFileResponses.Pop();
        AssertFileResponseAvailable(lpPending != nullptr,
            "Failed to allocate record for file system request\n", 941);
        lpPending->mpResponse = lpResponse;
        lpPending->meEvent = 20;
        lpPending->muFileId = static_cast<u32>(liFileId);
        return true;
    }

    // ARTIST828F3EE0: close uses this FileSystem and the request handle's actual ID.
    bool ResourceModule::AddCloseFileRequest(const Events::CloseFileRequest* lpRequest)
    {
        Events::CloseFileResponse* lpResponse;
        FileSystemAlloc(&lpResponse);
        AssertFileResponseAvailable(lpResponse != nullptr,
            "Failed to allocate temporary memory for file system request\n", 965);
        lpResponse->Construct(lpRequest->GetUser(), lpRequest->GetEventId());
        lpResponse->SetFileHandle(lpRequest->GetFileHandle());
        mFileSystem.Close(lpRequest->GetFileHandle().GetFileId());
        PendingFileResponse* const lpPending = mPendingFileResponses.Pop();
        AssertFileResponseAvailable(lpPending != nullptr,
            "Failed to allocate record for file system request\n", 971);
        lpPending->mpResponse = lpResponse;
        lpPending->meEvent = 21;
        lpPending->muFileId = lpRequest->GetFileHandle().GetFileId();
        return true;
    }

    // ARTIST828F42B8: actual four completion predicates and matched allocator/free.
    void ResourceModule::ProcessPendingFileSystemResponses()
    {
        PendingFileResponse* lapPending[KI_MAX_PENDING_FILE_SYSTEM_RESPONSES];
        const s32 liCount = mPendingFileResponses.Get(lapPending,
            KI_MAX_PENDING_FILE_SYSTEM_RESPONSES);
        for (s32 li = 0; li < liCount; ++li)
        {
            PendingFileResponse* const lpPending = lapPending[li];
            switch (lpPending->meEvent)
            {
            case 16:
            {
                Events::OpenReadStreamResponse* const lpResponse =
                    static_cast<Events::OpenReadStreamResponse*>(lpPending->mpResponse);
                if (mFileSystem.IsReadStreamOpen(lpResponse->GetStream()))
                {
                    if (lpResponse->GetUser())
                        lpResponse->GetUser()->AddEvent(
                            reinterpret_cast<const CgsModule::Event*>(lpResponse),
                            lpPending->meEvent, static_cast<s32>(sizeof(*lpResponse)));
                    FileSystemFree(lpResponse);
                    const s8 liIndex = mPendingFileResponses.GetObjectIndex(lpPending);
                    mPendingFileResponses.Push(liIndex);
                }
                break;
            }
            case 17:
            case 19:
                break;
            case 18:
            {
                Events::CloseReadStreamResponse* const lpResponse =
                    static_cast<Events::CloseReadStreamResponse*>(lpPending->mpResponse);
                if (mFileSystem.IsReadStreamClosed(static_cast<s32>(lpPending->muFileId)))
                {
                    if (lpResponse->GetUser())
                        lpResponse->GetUser()->AddEvent(
                            reinterpret_cast<const CgsModule::Event*>(lpResponse),
                            lpPending->meEvent, static_cast<s32>(sizeof(*lpResponse)));
                    FileSystemFree(lpResponse);
                    const s8 liIndex = mPendingFileResponses.GetObjectIndex(lpPending);
                    mPendingFileResponses.Push(liIndex);
                }
                break;
            }
            case 20:
            {
                Events::OpenFileResponse* const lpResponse =
                    static_cast<Events::OpenFileResponse*>(lpPending->mpResponse);
                if (lpResponse->GetFileHandle().GetStatus() == CgsFileSystem::E_FILESTATE_OPEN)
                {
                    if (lpResponse->GetUser())
                        lpResponse->GetUser()->AddEvent(
                            reinterpret_cast<const CgsModule::Event*>(lpResponse),
                            lpPending->meEvent, static_cast<s32>(sizeof(*lpResponse)));
                    FileSystemFree(lpResponse);
                    const s8 liIndex = mPendingFileResponses.GetObjectIndex(lpPending);
                    mPendingFileResponses.Push(liIndex);
                }
                break;
            }
            case 21:
            {
                Events::CloseFileResponse* const lpResponse =
                    static_cast<Events::CloseFileResponse*>(lpPending->mpResponse);
                if (mFileSystem.GetStatus(lpPending->muFileId) == CgsFileSystem::E_FILESTATE_CLOSED)
                {
                    if (lpResponse->GetUser())
                        lpResponse->GetUser()->AddEvent(
                            reinterpret_cast<const CgsModule::Event*>(lpResponse),
                            lpPending->meEvent, static_cast<s32>(sizeof(*lpResponse)));
                    FileSystemFree(lpResponse);
                    const s8 liIndex = mPendingFileResponses.GetObjectIndex(lpPending);
                    mPendingFileResponses.Push(liIndex);
                }
                break;
            }
            default:
                CgsDev::Assert::BeginAssert();
                {
                    CgsDev::Assert::MessageBufferPC lBuffer;
                    CgsDev::StrStream lStream(lBuffer.GetBuffer(), lBuffer.GetSize());
                    lStream << "Unexpected pending file response\n";
                    CgsDev::Assert::FireAssert(lBuffer.GetBuffer(),
                        "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsResourceModule.cpp",
                        1160);
                    CgsDev::Assert::EndAssert();
                }
                break;
            }
        }
    }


    // Drain the pool module's output queue (DoAcquireResourceRequest etc. responses) and forward each to
    // its requester's receiver queue (response->mpUser). The pool-response slice of ProcessResourceResponses
    // (X360 0x828EC788): the console translates each pool-output tag through the static table
    // dword_820F7194[tag] before posting to mpUser -- the ACQUIRE response (pool tag 6) reaches the
    // requester as receiver id 4, which is the id GameDataModule's internal drain (and every X360
    // receiver-side consumer) switches on. Only the tags the PC pool module emits are mapped; unmapped
    // tags pass through unchanged (their producers land with their own passes).
    void ResourceModule::ProcessPoolOutputResponses(const PoolIO::OutputBuffer* lpPoolOut)
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
    // ARTIST 82907948: the original stack-backed resource dispatcher.
    ResourceModule::EUpdateResult ResourceModule::Update(
        CgsModule::IOBufferStack* lpInputStack, CgsModule::IOBufferStack* lpOutputStack,
        const ResourceIO::InputBuffer* lpInputBuffer, ResourceIO::OutputBuffer* lpOutputBuffer)
    {
        const s32 liResourceMonitor = _miResourceUpdatePerfMon;
        if (liResourceMonitor >= 0)
            CgsDev::PerfMonCpu::StartMonitor(liResourceMonitor);
        EUpdateResult leResult = E_RESULT_OK;
        {
            CgsModule::IOHelper<CgsMemory::MemoryIO::InputBuffer> lMemoryInput(lpInputStack, "Memory");
            CgsModule::IOHelper<BundleLoaderIO::InputBuffer_Update> lLoaderInput(lpInputStack, "BundleLoader");
            CgsModule::IOHelper<PoolIO::InputBuffer> lPoolInput(lpInputStack, "Pool");
            CgsModule::IOHelper<CgsMemory::MemoryIO::OutputBuffer> lMemoryOutput(lpOutputStack, "Memory");
            CgsModule::IOHelper<BundleLoaderIO::OutputBuffer> lLoaderOutput(lpOutputStack, "BundleLoader");
            CgsModule::IOHelper<PoolIO::OutputBuffer> lPoolOutput(lpOutputStack, "Pool");
            CgsMemory::MemoryIO::InputBuffer* const lpMemoryInput = lMemoryInput;
            BundleLoaderIO::InputBuffer_Update* const lpLoaderInput = lLoaderInput;
            PoolIO::InputBuffer* const lpPoolInput = lPoolInput;
            CgsMemory::MemoryIO::OutputBuffer* const lpMemoryOutput = lMemoryOutput;
            BundleLoaderIO::OutputBuffer* const lpLoaderOutput = lLoaderOutput;
            PoolIO::OutputBuffer* const lpPoolOutput = lPoolOutput;

            const s32 liFileMonitor = _miUpdateFileSystemPerfMon;
            if (liFileMonitor >= 0)
                CgsDev::PerfMonCpu::StartMonitor(liFileMonitor);
            ProcessPendingFileSystemResponses();
            if (liFileMonitor >= 0)
                CgsDev::PerfMonCpu::StopMonitor(liFileMonitor);

            const s32 liRequestMonitor = _miProcessResourceRequestsPerfMon;
            if (liRequestMonitor >= 0)
                CgsDev::PerfMonCpu::StartMonitor(liRequestMonitor);
            lpInputBuffer->LockForRead();
            lpMemoryInput->LockForWrite();
            lpLoaderInput->LockForWrite();
            lpPoolInput->LockForWrite();
            ProcessResourceRequests(lpInputBuffer, lpMemoryInput, lpLoaderInput, lpPoolInput);
            lpPoolInput->UnlockForWrite();
            lpLoaderInput->UnlockForWrite();
            lpMemoryInput->UnlockForWrite();
            lpInputBuffer->UnlockForRead();
            if (liRequestMonitor >= 0)
                CgsDev::PerfMonCpu::StopMonitor(liRequestMonitor);

            const s32 liLoaderMonitor = _miUpdateBundleLoaderPerfMon;
            if (liLoaderMonitor >= 0)
                CgsDev::PerfMonCpu::StartMonitor(liLoaderMonitor);
            mBundleLoaderModule.Update(lpInputStack, lpOutputStack, lpLoaderInput, lpLoaderOutput);
            lpLoaderOutput->LockForRead();
            ProcessBundleLoaderStreamRequests(lpLoaderOutput);
            lpLoaderOutput->UnlockForRead();
            if (liLoaderMonitor >= 0)
                CgsDev::PerfMonCpu::StopMonitor(liLoaderMonitor);

            const s32 liPoolMonitor = _miUpdatePoolPerfMon;
            if (liPoolMonitor >= 0)
                CgsDev::PerfMonCpu::StartMonitor(liPoolMonitor);
            CgsModule::LockBuffersForIO(lpPoolInput, lpLoaderOutput);
            lpPoolInput->GetPoolInputQueue()->Append(
                *static_cast<const BundleLoaderIO::OutputBuffer*>(lpLoaderOutput)->GetPoolSendQueue());
            CgsModule::UnlockBuffersForIO(lpPoolInput, lpLoaderOutput);
            if (mPoolModule.Update(lpInputStack, lpOutputStack, lpPoolInput, lpPoolOutput)
                == PoolModule::E_RESULT_REQUIRE_STALL)
                leResult = E_RESULT_REQUIRE_STALL;
            if (liPoolMonitor >= 0)
                CgsDev::PerfMonCpu::StopMonitor(liPoolMonitor);

            {
                CgsModule::IOHelper<BundleLoaderIO::InputBuffer_Record> lLoaderRecord(lpInputStack, "BundleLoaderRecord");
                BundleLoaderIO::InputBuffer_Record* const lpLoaderRecord = lLoaderRecord;
                CgsModule::LockBuffersForIO(lpLoaderRecord, lpPoolOutput);
                lpLoaderRecord->GetPoolReceiveQueue()->Append(
                    *static_cast<const PoolIO::OutputBuffer*>(lpPoolOutput)->GetPoolOutputQueue());
                CgsModule::UnlockBuffersForIO(lpLoaderRecord, lpPoolOutput);
                mBundleLoaderModule.RecordPostUpdateEvents(lpLoaderRecord);
            }

            const s32 liMemoryMonitor = _miUpdateMemoryPerfMon;
            if (liMemoryMonitor >= 0)
                CgsDev::PerfMonCpu::StartMonitor(liMemoryMonitor);
            CgsModule::LockBuffersForIO(lpMemoryInput, lpPoolOutput);
            ProcessPoolResourceRequests(lpMemoryInput, lpPoolOutput);
            CgsModule::UnlockBuffersForIO(lpMemoryInput, lpPoolOutput);
            mMemoryModule.Update(lpInputStack, lpOutputStack, lpMemoryInput, lpMemoryOutput);
            if (liMemoryMonitor >= 0)
                CgsDev::PerfMonCpu::StopMonitor(liMemoryMonitor);

            const s32 liOutputMonitor = _miUpdateOutputPerfMon;
            if (liOutputMonitor >= 0)
                CgsDev::PerfMonCpu::StartMonitor(liOutputMonitor);
            lpMemoryOutput->LockForRead();
            ProcessMemoryResponses(lpMemoryOutput);
            lpMemoryOutput->UnlockForRead();
            lpPoolOutput->LockForRead();
            lpLoaderOutput->LockForRead();
            ProcessResourceResponses(lpLoaderOutput, lpPoolOutput);
            lpLoaderOutput->UnlockForRead();
            lpPoolOutput->UnlockForRead();
            lpOutputBuffer->LockForWrite();
            lpOutputBuffer->GetFileSystemStatusInterface()->SetDiskErrorOccured(mFileSystem.HasDiskErrorOccured());
            lpOutputBuffer->UnlockForWrite();
            if (liOutputMonitor >= 0)
                CgsDev::PerfMonCpu::StopMonitor(liOutputMonitor);
        }
        if (liResourceMonitor >= 0)
            CgsDev::PerfMonCpu::StopMonitor(liResourceMonitor);
        return leResult;
    }


    // FLAG PC-platform leaf: complete existing native body and allocation/failure policy retained.
    void ResourceModule::ProcessResourceRequestsNativePC(const ResourceIO::InputBuffer* lpResIn,
        CgsMemory::MemoryIO::InputBuffer* lpMemIn,
        BundleLoaderIO::InputBuffer_Update* lpLoaderIn, PoolIO::InputBuffer* lpPoolIn)
    {
        const auto* lpQueue = static_cast<const ResourceIO::InputBuffer*>(lpResIn)->GetResourceQueue();
        auto* lpPoolQueue = lpPoolIn->GetPoolInputQueue();
        const CgsModule::Event* lpEvent = nullptr;
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
            case 16: AddOpenReadStreamRequestNativePC(reinterpret_cast<const Events::OpenReadStreamRequest*>(lpEvent)); break;
            case 17: AddOpenWriteStreamRequest(reinterpret_cast<const Events::OpenWriteStreamRequest*>(lpEvent)); break;
            case 18: AddCloseReadStreamRequestNativePC(reinterpret_cast<const Events::CloseReadStreamRequest*>(lpEvent)); break;
            case 19: AddCloseWriteStreamRequest(reinterpret_cast<const Events::CloseWriteStreamRequest*>(lpEvent)); break;
            default: CGS_ASSERT(false, "Invalid event id\n"); break;
            }
            const CgsModule::Event* lpNext = nullptr;
            liTag = lpQueue->GetNextEvent(lpEvent, &lpNext, &liSize);
            lpEvent = lpNext;
        }
    }

    // FLAG PC-platform leaf: complete existing native body and allocation/failure policy retained.
    void ResourceModule::ProcessBundleLoaderStreamRequestsNativePC(const BundleLoaderIO::OutputBuffer* lpLoaderOut)
    {
        const auto* lpQueue = lpLoaderOut->GetStreamRequestQueue();
        const CgsModule::Event* lpEvent = nullptr;
        s32 liSize = 0;
        s32 liTag = lpQueue->GetFirstEvent(&lpEvent, &liSize);
        while (lpEvent)
        {
            if (liTag == 16)
                AddOpenReadStreamRequestNativePC(reinterpret_cast<const Events::OpenReadStreamRequest*>(lpEvent));
            else if (liTag == 18)
                AddCloseReadStreamRequestNativePC(reinterpret_cast<const Events::CloseReadStreamRequest*>(lpEvent));
            else
                CGS_ASSERT(false, "Unexpected request from bundle loader\n");
            const CgsModule::Event* lpNext = nullptr;
            liTag = lpQueue->GetNextEvent(lpEvent, &lpNext, &liSize);
            lpEvent = lpNext;
        }
    }

    // FLAG PC-platform leaf: complete existing native body and allocation/failure policy retained.
    bool ResourceModule::AddOpenReadStreamRequestNativePC(
        const Events::OpenReadStreamRequest* lpRequest)
    {
        CgsFileSystem::ReadStream lStream = mFileSystem.OpenReadStream(
            lpRequest->GetFileName(), lpRequest->GetBuffer(),
            lpRequest->GetBufferSize(), lpRequest->GetNumBlocks(),
            lpRequest->GetNormalPriority(), lpRequest->GetHighPriority(),
            lpRequest->GetUseHDCache());

        Events::OpenReadStreamResponse* lpResponse =
            new Events::OpenReadStreamResponse;
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
            delete lpResponse;
            return false;
        }
        lpPending->mpResponse = lpResponse;
        lpPending->meEvent = 16;
        lpPending->muFileId = 0xFFFFFFFFu;
        return lStream.IsValid();
    }

    // FLAG PC-platform leaf: complete existing native body and allocation/failure policy retained.
    bool ResourceModule::AddCloseReadStreamRequestNativePC(
        const Events::CloseReadStreamRequest* lpRequest)
    {
        const CgsFileSystem::ReadStream lStream = lpRequest->GetStream();
        Events::CloseReadStreamResponse* lpResponse =
            new Events::CloseReadStreamResponse;
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
            delete lpResponse;
            return false;
        }
        lpPending->mpResponse = lpResponse;
        lpPending->meEvent = 18;
        lpPending->muFileId = mFileSystem.GetReadStreamIndex(lStream);
        mFileSystem.CloseReadStream(lStream);
        return true;
    }

    // FLAG PC-platform leaf: complete existing native body and allocation/failure policy retained.
    void ResourceModule::ProcessPendingFileSystemResponsesNativePC()
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
                // FLAG PC-platform leaf: finish a failed open only after its
                // stream slot has closed. The invalid response handle carries
                // failure without changing the original response record shape.
                if (lpPending->muFileId != 0xFFFFFFFFu)
                {
                    lbComplete = mFileSystem.IsReadStreamClosed(lpPending->muFileId);
                }
                else if (mFileSystem.HasReadStreamFailedPC(lpResponse->GetStream()))
                {
                    const auto lStream = lpResponse->GetStream();
                    if (lStream.IsValid())
                    {
                        lpPending->muFileId = mFileSystem.GetReadStreamIndex(lStream);
                        mFileSystem.CloseReadStream(lStream);
                        lbComplete = mFileSystem.IsReadStreamClosed(lpPending->muFileId);
                    }
                    else
                        lbComplete = true;
                    CgsFileSystem::ReadStream lInvalid;
                    lInvalid.Construct(nullptr);
                    lpResponse->Construct(lpResponse->GetUser(), lpResponse->GetEventId(), lInvalid);
                }
                else
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
                delete static_cast<Events::OpenReadStreamResponse*>(
                    lpPending->mpResponse);
            else
                delete static_cast<Events::CloseReadStreamResponse*>(
                    lpPending->mpResponse);

            const s8 liIndex = mPendingFileResponses.GetObjectIndex(lpPending);
            mPendingFileResponses.Push(liIndex);
        }
    }

    bool ResourceModule::Update(void* lpInputBuffer, void* /*lpOutputBuffer*/)
    {
        auto* lpResIn = static_cast<ResourceIO::InputBuffer*>(lpInputBuffer);
        if (!lpResIn) return false;

        // FLAG PC-platform leaf: the current module adapter receives no IO stacks.
        // Reuse its serialized scratch buffers; Construct resets queue metadata,
        // not the backing event arrays. Stage ordering is ARTIST82907948.
        static PoolIO::InputBuffer s_poolIn;
        static PoolIO::OutputBuffer s_poolOut;
        static CgsMemory::MemoryIO::InputBuffer s_memIn;
        static CgsMemory::MemoryIO::OutputBuffer s_memOut;
        static BundleLoaderIO::InputBuffer_Update s_loaderIn;
        static BundleLoaderIO::OutputBuffer s_loaderOut;
        static BundleLoaderIO::InputBuffer_Record s_loaderRecord;
        s_poolIn.Construct(); s_poolOut.Construct();
        s_memIn.Construct(); s_memOut.Construct();
        s_loaderIn.Construct(); s_loaderOut.Construct(); s_loaderRecord.Construct();

        ProcessPendingFileSystemResponsesNativePC();
        lpResIn->LockForRead();
        s_memIn.LockForWrite(); s_loaderIn.LockForWrite(); s_poolIn.LockForWrite();
        ProcessResourceRequestsNativePC(lpResIn, &s_memIn, &s_loaderIn, &s_poolIn);
        s_poolIn.UnlockForWrite(); s_loaderIn.UnlockForWrite(); s_memIn.UnlockForWrite();
        lpResIn->UnlockForRead();
        lpResIn->LockForWrite(); lpResIn->GetResourceQueue()->Clear(); lpResIn->UnlockForWrite();

        {
            renderengine::FrameProfile::Scope lProfile(renderengine::FrameProfile::RESOURCE_LOAD);
            mBundleLoaderModule.Update(&s_loaderIn, &s_loaderOut);
            s_loaderOut.LockForRead();
            ProcessBundleLoaderStreamRequestsNativePC(&s_loaderOut);
            s_loaderOut.UnlockForRead();
        }
        s_poolIn.LockForWrite(); s_loaderOut.LockForRead();
        s_poolIn.GetPoolInputQueue()->Append(
            *static_cast<const BundleLoaderIO::OutputBuffer&>(s_loaderOut).GetPoolSendQueue());
        s_loaderOut.UnlockForRead(); s_poolIn.UnlockForWrite();
        bool lbBusy;
        {
            renderengine::FrameProfile::CycleScope lProfile(renderengine::FrameProfile::RESOURCE_POOL);
            lbBusy = mPoolModule.Update(&s_poolIn, &s_poolOut);
        }
        // Retain pool replies for the NEXT loader update. Its current source
        // buffers stay owned by this load until allocation/fixup completion.
        s_loaderRecord.LockForWrite(); s_poolOut.LockForRead();
        s_loaderRecord.GetPoolReceiveQueue()->Append(
            *static_cast<const PoolIO::OutputBuffer&>(s_poolOut).GetPoolOutputQueue());
        s_poolOut.UnlockForRead(); s_loaderRecord.UnlockForWrite();
        mBundleLoaderModule.RecordPostUpdateEvents(&s_loaderRecord);

        s_memIn.LockForWrite(); s_poolOut.LockForRead();
        ProcessPoolResourceRequests(&s_memIn, &s_poolOut);
        s_poolOut.UnlockForRead(); s_memIn.UnlockForWrite();
        {
            renderengine::FrameProfile::Scope lProfile(renderengine::FrameProfile::RESOURCE_MEMORY);
            mMemoryModule.Update(nullptr, nullptr, &s_memIn, &s_memOut);
        }
        s_memOut.LockForRead(); ProcessMemoryResponses(&s_memOut); s_memOut.UnlockForRead();
        s_loaderOut.LockForRead(); s_poolOut.LockForRead();
        ProcessResourceResponses(&s_loaderOut, &s_poolOut);
        s_poolOut.UnlockForRead(); s_loaderOut.UnlockForRead();
        return lbBusy;
    }

    // @ 0x829019F0 - forward the pool module's emitted resource-memory requests into the MemoryModule
    // input queue. Each pool output event tagged 10 (CreatePool) / 13 (DeletePool) becomes a memory
    // request (queue tag 1). [X360 hardcodes the per-type size 92/16; we use the recorded event size so
    // the x64-width CreateResourceRequest copies whole.]
    void ResourceModule::ProcessPoolResourceRequests(CgsMemory::MemoryIO::InputBuffer* lpMemInput,
                                                     const PoolIO::OutputBuffer* lpPoolOutput)
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
    void ResourceModule::ProcessMemoryResponses(const CgsMemory::MemoryIO::OutputBuffer* lpMemOutput)
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
                return nullptr;
            return lpPool->mpRecords + 12 * lcIndex;
        }
    }
}
