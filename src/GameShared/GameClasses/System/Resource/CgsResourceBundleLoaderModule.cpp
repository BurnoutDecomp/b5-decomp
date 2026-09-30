#include "GameShared/GameClasses/System/Resource/CgsResourceBundleLoaderModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/System/Resource/CgsResourcePoolModule.h"  // PoolModule::GetPool (resolve poolId)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                  // [stream] trace
#include "GameShared/GameClasses/System/CgsHardwareInit.h"
#include <new>

// Bundle loader lifetime and native adapters. ResourceModule drives the staged
// protocol in CgsBundleLoaderModule.cpp with allocator-backed stream buffers.
namespace CgsResource
{
    // FLAG PC-platform leaf: resource destinations must outlive native workers.
    void BundleLoaderModule::CancelStreamPC()
    {
        mDecompressionJobInterface.CancelStreamPC();
    }
    // @ 0x828E2678 - resumable prepare stage machine: free every stream slot, clear the
    // receiver + load/unload queues, then bring up the base module.
    bool BundleLoaderModule::Prepare()
    {
        mbIsNewModule = true;   // [reliable] see ResourceModule::Prepare -- set before base Prepare
        switch (mePrepareStage)
        {
        case E_STAGE_START:
            maStreams[0] = nullptr;
            maStreams[1] = nullptr;
            mabStreamOpenFailed[0] = mabStreamOpenFailed[1] = false;
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
        mLoadRequests.Construct();
        mUnloadRequests.Construct();
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
        mabStreamOpenFailed[0] = mabStreamOpenFailed[1] = false;
        // The allocator-backed overload supplies the loaded-bundle table.
        mpLoadedBundles = nullptr;
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

        const auto lAllocate = [lpAllocator](size_t luSize, u32 luAlignment) -> void*
        {
            rw::ResourceDescriptor lDescriptor;
            lDescriptor.m_baseResourceDescriptors[0].m_size = static_cast<u32>(luSize);
            lDescriptor.m_baseResourceDescriptors[0].m_alignment = luAlignment;
            return lpAllocator->DoAllocate(lDescriptor, nullptr).m_baseResources[0];
        };
        mpDecompressionEntries = static_cast<CompressedData*>(lAllocate(
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
        mpcDebugDataBuffer = nullptr;
        if (miDebugBufferSize > 0 && lpDebugAllocator)
        {
            rw::ResourceDescriptor lDebugDescriptor;
            lDebugDescriptor.m_baseResourceDescriptors[0].m_size = miDebugBufferSize;
            lDebugDescriptor.m_baseResourceDescriptors[0].m_alignment = 16;
            mpcDebugDataBuffer = static_cast<char*>(
                lpDebugAllocator->DoAllocate(lDebugDescriptor, nullptr).m_baseResources[0]);
        }
        mDecompressionJobInterface.Construct(CgsSystem::JobManager(), mpDecompressionEntries,
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
    bool BundleLoaderModule::Update(void* lpInputBuffer, void* lpOutputBuffer)
    {
        // ARTIST82907638; the legacy native adapter has a bool return, unused
        // by the resource dispatcher. Input and output lifetime matches ARTIST.
        auto* lpInput = static_cast<BundleLoaderIO::InputBuffer_Update*>(lpInputBuffer);
        auto* lpOutput = static_cast<BundleLoaderIO::OutputBuffer*>(lpOutputBuffer);
        lpOutput->LockForWrite();
        lpInput->LockForRead();
        ProcessReceiverQueue();
        ProcessBundleLoadRequests(lpInput, lpOutput);
        ProcessPoolResponses();
        lpInput->UnlockForRead();
        UpdateStream(lpOutput);
        lpOutput->UnlockForWrite();
        return false;
    }

    // Queue a LoadBundleRequest routed here by the ResourceModule shuttle (resource request id 2).
    void BundleLoaderModule::EnqueueLoadRequest(const Events::LoadBundleRequest& lrRequest)
    {
        mLoadRequests.AddEvent(lrRequest);
    }

    // Drain the queued LoadBundleRequests: for each, resolve the target pool by id and load its bundle
    // into that pool via the PC synchronous BundleLoader (the load leaf: read file -> per-resource
    // Pool::CreateEntry -> FixUp/ResolveImports/PostFixUp), then reply with a LoadBundleResponse to the
    // request's reply target. [The async StreamHeader/EntryList/Data FSM + FileSystem + EA Jobs are the
    // deferred remainder; the request/response machinery + the per-resource pool create/fixup are faithful.]
    void BundleLoaderModule::ProcessLoadRequests(PoolModule* lpPoolModule, FTypeResolver lpfnResolveType)
    {
        // FLAG PC-platform leaf: ONE BUNDLE PER UPDATE.
        // The console's bundle loader is a DMA/async engine -- a LoadBundle request posted
        // this frame replies on a LATER frame, so every requester sees at most one completion
        // per Update. This PC loader is synchronous (ReadWholeFile + an in-place fix-up), so
        // draining the whole queue in one pass delivered N replies in the same frame and broke
        // that invariant: BrnWorld::InternalBaseStreamer::UpdateLoading pipelines two world-unit
        // loads and asserts `mGDReceiverQueue.GetLength() == 1` in its WAIT stage, then stalls
        // because its state machine only consumes one reply per visit. Servicing exactly one
        // request per Update restores the console pacing (and bounds the per-frame hitch); the
        // rest stay queued for the next Update, which is what the hardware queue does anyway.
        const s32 liCount = (mLoadRequests.GetLength() > 0) ? 1 : 0;
        for (s32 li = 0; li < liCount; ++li)
        {
            const Events::LoadBundleRequest& lrRequest = mLoadRequests.GetEvent(li);

            Pool* lpPool = (lpPoolModule != 0) ? lpPoolModule->GetPool(lrRequest.miPoolId) : 0;
            Events::LoadBundleResponse lResponse;
            static_cast<Events::BundleLoaderEvent&>(lResponse) = static_cast<const Events::BundleLoaderEvent&>(lrRequest);
            lResponse.meResult = Events::LoadBundleResponse::E_RESULT_SUCCESS;

            if (lpPool == 0)
            {
                CGS_ASSERT(false, "LoadBundle: target pool id not found");
                lResponse.meResult = Events::LoadBundleResponse::E_RESULT_OUT_OF_MEMORY;
            }
            else
            {
                BundleLoader lLoader;
                // Ids are stored RAW/untagged (the X360 form -- CreateResourceList
                // 0x828FF480 registers the on-disc u64 verbatim; acquires are untagged
                // zero-extended hashes and select the pool via miPoolId; see LoadBundle).
                const s32 liLoaded = lLoader.LoadBundle(lrRequest.macFileName, lpPool, lpfnResolveType);
                *CgsDev::Log::gpDebugPrint << "[stream] LoadBundle '" << lrRequest.macFileName
                                          << "' -> pool " << (s32)lrRequest.miPoolId
                                          << ": " << (s32)liLoaded << " resources\n";
                if (liLoaded == BundleLoader::KI_LOAD_FILE_MISSING)
                {
                    // [FLAG PC bring-up] NOT A DEFECT IN THE GAME: this bundle was never
                    // converted into build/game. Asserting here HALTS the process on a dialog
                    // (Assert::Manager::DoAssert waits for END), which on a player's machine
                    // turns a missing sound into a frozen game -- e.g. every car class whose
                    // soundems\Boost_Bank_<class>.bundle is unported (7 of the retail 17
                    // AEMS banks are in build/game; aems_native64_port.py needs the Xbox One
                    // bank bodies and [inputs].xb1_root is unset here). The failure reply below
                    // is unchanged, so the requester still runs its own error path.
                    // DELETE-WHEN the asset set is complete.
                    static bool sbSaidIt = false;
                    if (!sbSaidIt && CgsDev::Log::gpDebugPrint != 0)
                    {
                        sbSaidIt = true;
                        *CgsDev::Log::gpDebugPrint
                            << "[FLAG PC bring-up] bundle '" << lrRequest.macFileName
                            << "' is not present in this port -- continuing without it "
                               "(further missing bundles show only as a [stream] line)\n";
                    }
                    lResponse.meResult = Events::LoadBundleResponse::E_RESULT_OUT_OF_MEMORY;
                }
                else if (liLoaded < 0)
                {
                    // The request's OWN failure policy, finally consumed. Every producer in
                    // the tree fills mbAllowFailiure (from the GameData request's fail flag,
                    // or a literal at the fixed-asset sites) and until now NOTHING read it:
                    // an asset the caller declared mandatory disappeared as quietly as an
                    // optional one, which is precisely the silent-drop shape this codebase
                    // keeps getting bitten by. The console consumes the flag inside the async
                    // stream FSM (deferred here) for exactly this: a disallowed failure is
                    // loud. The RESULT is unchanged either way -- the requester still gets
                    // the failure reply and its own error path -- so this adds a diagnostic,
                    // not a behaviour.
                    CGS_ASSERT(lrRequest.mbAllowFailiure,
                               "LoadBundle failed for a request that did not allow failure");
                    lResponse.meResult = Events::LoadBundleResponse::E_RESULT_OUT_OF_MEMORY;
                }
            }

            // Reply to the request's originating receiver queue (the X360 ProcessPoolResponses path; here a
            // direct post). Event id 2 == the LoadBundle event family.
            if (lrRequest.mpUser != 0)
                lrRequest.mpUser->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResponse), 2,
                                           static_cast<s32>(sizeof(lResponse)));
        }
        // Only the serviced head is retired; the tail stays queued for the next Update
        // (see the pacing note above).
        for (s32 li = 0; li < liCount; ++li)
        {
            mLoadRequests.PopFront();
        }
    }

    void BundleLoaderModule::EnqueueUnloadRequest(const Events::UnloadBundleRequest& lrRequest)
    {
        mUnloadRequests.AddEvent(lrRequest);
    }

    // Drain the queued UnloadBundleRequests: resolve each request's pool and unload its bundle's resources
    // (BundleLoader::UnloadBundle -> ref-count-release each), then reply with an UnloadBundleResponse.
    void BundleLoaderModule::ProcessUnloadRequests(PoolModule* lpPoolModule)
    {
        const s32 liCount = mUnloadRequests.GetLength();
        for (s32 li = 0; li < liCount; ++li)
        {
            const Events::UnloadBundleRequest& lrRequest = mUnloadRequests.GetEvent(li);

            Pool* lpPool = (lpPoolModule != 0) ? lpPoolModule->GetPool(lrRequest.miPoolId) : 0;
            if (lpPool != 0)
            {
                BundleLoader lLoader;
                const s32 liUnloaded = lLoader.UnloadBundle(lrRequest.macFileName, lpPool);
                *CgsDev::Log::gpDebugPrint << "[stream] UnloadBundle '" << lrRequest.macFileName
                                          << "' <- pool " << (s32)lrRequest.miPoolId
                                          << ": " << (s32)liUnloaded << " resources\n";
            }

            Events::UnloadBundleResponse lResponse;
            static_cast<Events::BundleLoaderEvent&>(lResponse) = static_cast<const Events::BundleLoaderEvent&>(lrRequest);
            if (lrRequest.mpUser != 0)
                lrRequest.mpUser->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResponse), 3,
                                           static_cast<s32>(sizeof(lResponse)));
        }
        mUnloadRequests.Clear();
    }
}
