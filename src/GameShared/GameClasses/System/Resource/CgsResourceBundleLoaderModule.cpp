#include "GameShared/GameClasses/System/Resource/CgsResourceBundleLoaderModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/System/Resource/CgsResourcePoolModule.h"  // PoolModule::GetPool (resolve poolId)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                  // [stream] trace
#include "GameShared/GameClasses/System/CgsHardwareInit.h"
#include <new>
#include <cstddef>

// Bundle loader lifetime and native adapters. ResourceModule drives the staged
// protocol in CgsBundleLoaderModule.cpp with allocator-backed stream buffers.
namespace CgsResource
{
    namespace
    {
        class BundleAllocation
        {
        public:
            explicit BundleAllocation(rw::IResourceAllocator* lpAllocator)
                : mpAllocator(lpAllocator) {}
            void* operator()(size_t luSize, u32 luAlignment) const
            {
                rw::ResourceDescriptor lDescriptor;
                lDescriptor.m_baseResourceDescriptors[0].m_size = static_cast<u32>(luSize);
                lDescriptor.m_baseResourceDescriptors[0].m_alignment = luAlignment;
                return mpAllocator->DoAllocate(lDescriptor, 0).m_baseResources[0];
            }
        private:
            rw::IResourceAllocator* mpAllocator;
        };
    }

    // @ 0x828E2678 - resumable prepare stage machine: free every stream slot, clear the
    // receiver + load/unload queues, then bring up the base module.
    bool BundleLoaderModule::Prepare()
    {
        mbIsNewModule = true;   // [reliable] see ResourceModule::Prepare -- set before base Prepare
        switch (mePrepareStage)
        {
        case E_STAGE_START:
            maStreams[0] = 0;
            maStreams[1] = 0;
            meStreamStage = STREAMSTAGE_IDLE;
            miNumLoadedBundles = 0;
            for (s32 li = 0; li < miMaxLoadedBundles; ++li)
            {
                mpLoadedBundles[li].miPoolId = -1;
                mpLoadedBundles[li].miRefCount = 0;
            }
            mReceiverQueue.Clear();
            mLoadRequestQueue.Clear();
            mUnloadRequestQueue.Clear();
            // fall through
        case E_STAGE_RUNNING:
            mePrepareStage = E_STAGE_RUNNING;
            if (!CgsModule::ModuleSingleBuffered::Prepare())
                return false;
            // fall through
        case E_STAGE_DONE:
            meReleaseStage = E_STAGE_START;
            mePrepareStage = E_STAGE_DONE;
            return true;
        default:
            CGS_ASSERT(false, "Invalid stage");
            return false;
        }
    }

    // @ 0x828E27A0 - resumable release stage machine: clear the receiver queue, then tear
    // down the base module.
    bool BundleLoaderModule::Release()
    {
        switch (meReleaseStage)
        {
        case E_STAGE_START:
            mReceiverQueue.Clear();
            // fall through
        case E_STAGE_RUNNING:
            meReleaseStage = E_STAGE_RUNNING;
            if (!CgsModule::ModuleSingleBuffered::Release())
                return false;
            // fall through
        case E_STAGE_DONE:
            mePrepareStage = E_STAGE_START;
            meReleaseStage = E_STAGE_DONE;
            return true;
        default:
            CGS_ASSERT(false, "Invalid stage");
            return false;
        }
    }

    // @ 0x828E2850 - clear the receiver queue and tear down the base module.
    void BundleLoaderModule::Destruct()
    {
        mReceiverQueue.Clear();
        CgsModule::ModuleSingleBuffered::Destruct();
    }

    // Shared queue initialization. Production supplies the backing allocations
    // through the ARTIST828EBAF8 overload; protocol fixtures supply their own.
    void BundleLoaderModule::Construct()
    {
        CgsModule::ModuleSingleBuffered::Construct();
        mbIsNewModule = true;
        mReceiverQueue.Construct();
        mLoadRequestQueue.Construct(128);
        mUnloadRequestQueue.Construct(128);
        mQueuedLoads.Construct();
        mPoolReceiveQueueCache.Construct();
        mePrepareStage = E_STAGE_START;
        meReleaseStage = E_STAGE_DONE;
        miCurrentStream = 0;
        mabStreamBuffersUsed[0] = false;
        mabStreamBuffersUsed[1] = false;
        // The allocator-backed overload supplies the loaded-bundle table.
        mpLoadedBundles = 0;
        miMaxLoadedBundles = 0;
        miMaxPartialFixups = 60;
    }

    // ARTIST828EBAF8..828EC144; descriptors contain five native resource lanes.
    // Backing blocks belong to the supplied resource allocator's lifetime.
    void BundleLoaderModule::Construct(const InitOptions* lpOptions,
                                      rw::IResourceAllocator* lpAllocator,
                                      rw::IResourceAllocator* lpDebugAllocator)
    {
        Construct();
        miBundleHeaderBufferSize = lpOptions->miBundleHeaderBufferSize;
        miDebugBufferSize = lpOptions->miDebugDataBufferSize;
        miStreamBufferSize = lpOptions->miStreamBufferSize;
        miMaxBundles = lpOptions->miMaxBundles;
        miMaxResourcesPerBundle = lpOptions->miMaxResourcesPerBundle;
        miMaxLoadedBundles = miMaxBundles;
        mbForceUpperCaseFileNames = lpOptions->mbForceUpperCaseFileNames;
        miNumLoadedBundles = 0;

        const BundleAllocation lAllocate(lpAllocator);
        mpJobEntryBuffer = static_cast<CompressedData*>(lAllocate(
            sizeof(CompressedData) * miMaxResourcesPerBundle, 128));
        mapcStreamBuffers[0] = static_cast<char*>(lAllocate(miStreamBufferSize, 128));
        mapcStreamBuffers[1] = static_cast<char*>(lAllocate(miStreamBufferSize, 128));
        mpcSecondaryStreamBuffer = static_cast<char*>(lAllocate(KU_SECONDARY_STREAM_BUFFER_SIZE, 128));
        mpcHeaderBuffer = static_cast<char*>(lAllocate(miBundleHeaderBufferSize, 16));
        mpNeeds = static_cast<bool*>(lAllocate(sizeof(bool) * miMaxResourcesPerBundle, 16));
        mpResources = static_cast<SmallResource*>(lAllocate(sizeof(SmallResource) * miMaxResourcesPerBundle, 16));
        mpLoadedBundles = static_cast<LoadedBundleData*>(lAllocate(sizeof(LoadedBundleData) * miMaxLoadedBundles, 16));
        if (mpResources)
            for (s32 li = 0; li < miMaxResourcesPerBundle; ++li) ::new (mpResources + li) SmallResource;
        if (mpLoadedBundles)
            for (s32 li = 0; li < miMaxLoadedBundles; ++li) ::new (mpLoadedBundles + li) LoadedBundleData;
        mpcDebugDataBuffer = 0;
        if (miDebugBufferSize > 0 && lpDebugAllocator)
        {
            rw::ResourceDescriptor lDebugDescriptor;
            lDebugDescriptor.m_baseResourceDescriptors[0].m_size = miDebugBufferSize;
            lDebugDescriptor.m_baseResourceDescriptors[0].m_alignment = 16;
            mpcDebugDataBuffer = static_cast<char*>(
                lpDebugAllocator->DoAllocate(lDebugDescriptor, 0).m_baseResources[0]);
        }
        mStreamJobData.mpTargets = static_cast<StreamDistributeTargetEntry*>(mpJobEntryBuffer);
        mDecompressionJobInterface.Construct(CgsSystem::JobManager(), static_cast<CompressedData*>(mpJobEntryBuffer),
                                             miMaxResourcesPerBundle);
        CGS_ASSERT(mpcHeaderBuffer && mpNeeds && mpResources && mpLoadedBundles,
                   "Ran out of memory allocating bundle loader\n");
        CGS_ASSERT(mapcStreamBuffers[0], "Ran out of memory allocating stream buffer 0\n");
        CGS_ASSERT(mapcStreamBuffers[1], "Ran out of memory allocating stream buffer 1\n");
        miCurrentMemoryType = 0;
        miCurrentResourcePosition = 0;
        miCurrentResource = 0;
        LARGE_INTEGER lTicks;
        QueryPerformanceCounter(&lTicks);
        muTimerTrace = static_cast<u64>(lTicks.QuadPart);
    }
    void BundleLoaderModule::Update(CgsModule::IOBufferStack*, CgsModule::IOBufferStack*,
        const BundleLoaderIO::InputBuffer_Update* lpInput, BundleLoaderIO::OutputBuffer* lpOutput)
    {
        // ARTIST82907638: stacks are forwarded in the native module ABI but unused here.
        lpOutput->LockForWrite();
        lpInput->LockForRead();
        ProcessReceiverQueue();
        ProcessBundleLoadRequests(lpInput, lpOutput);
        ProcessPoolResponses();
        lpInput->UnlockForRead();
        UpdateStream(lpOutput);
        lpOutput->UnlockForWrite();
    }

    void BundleLoaderModule::AssertNativeLayout()
    {
        CGS_STATIC_ASSERT(sizeof(DecompressionJobData) == 128, "native decompression descriptor slot");
        CGS_STATIC_ASSERT(sizeof(StreamDistributeData) == 128, "native stream distribution descriptor slot");
        CGS_STATIC_ASSERT(offsetof(BundleLoaderModule, mePrepareStage) == 0x228, "loader prepare stage");
        CGS_STATIC_ASSERT(offsetof(BundleLoaderModule, mpcHeaderBuffer) == 0x24C, "loader header buffer");
        CGS_STATIC_ASSERT(offsetof(BundleLoaderModule, mDecompressionJobInterface) == 0xA200, "loader decompression interface");
        CGS_STATIC_ASSERT(offsetof(BundleLoaderModule, mStreamJob) == 0x2AC90, "loader stream job");
        CGS_STATIC_ASSERT(offsetof(BundleLoaderModule, mStreamJobData) == 0x2B000, "loader stream job data");
        CGS_STATIC_ASSERT(offsetof(BundleLoaderModule, mpJobEntryBuffer) == 0x2B080, "loader job entries");
        CGS_STATIC_ASSERT(offsetof(BundleLoaderModule, mPoolReceiveQueueCache) == 0x2B084, "loader response cache");
        CGS_STATIC_ASSERT(offsetof(BundleLoaderModule, muTimerTrace) == 0x2C0A8, "loader timer");
        CGS_STATIC_ASSERT(sizeof(BundleLoaderModule) == 0x2C100, "native loader size");
    }

}
