#include "GameShared/GameClasses/System/Resource/CgsResourceBundleLoaderModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/System/Resource/CgsResourcePoolModule.h"  // PoolModule::GetPool (resolve poolId)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                  // [stream] trace

// CgsResource::BundleLoaderModule - see the header. This pass reconstructs the rw/file-
// INDEPENDENT spine (Prepare / Release / Destruct). Construct + the streaming state machine
// + dispatch are DEFERRED (rw allocator / FileSystem / job system) as inert marked stubs.
namespace CgsResource
{
    // @ 0x828E2678 - resumable prepare stage machine: free every stream slot, clear the
    // receiver + load/unload queues, then bring up the base module.
    bool BundleLoaderModule::Prepare()
    {
        mbIsNewModule = true;   // [reliable] see ResourceModule::Prepare -- set before base Prepare
        switch (mePrepareStage)
        {
        case E_STAGE_START:
            mField426 = 0;
            mField427 = 0;
            mField140 = 0;
            muStreamSlotCursor = 0;
            for (u32 lu = 0; lu < muNumStreamSlots; ++lu)
            {
                mpStreamSlots[lu].miId     = -1;
                mpStreamSlots[lu].mField24 = 0;
            }
            mReceiverQueue.Clear();
            mLoadQueue.Clear();
            mUnloadQueue.Clear();
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

    // ---- DEFERRED (rw allocator / FileSystem / job system) ---------------------------
    // Construct (0x828EBAF8) embeds the EA::Allocator::GeneralAllocator (the deferred PPMalloc)
    // and the two EA::Jobs::Job, and carves the stream-slot pool. The streaming state machine
    // (Update 0x82907638 / UpdateStream 0x82906B30 / the StreamIdle/Header/EntryList/Data/Done
    // Func steps), ProcessReceiverQueue (0x828E2888) / ProcessPoolResponses (0x828EC148),
    // CheckForLoads (0x828FB758) / CheckForUnloads (0x828FB308) and the bundle parse path read
    // .BUNDLE files off disk through the FileSystem, decompress via the job system, and create
    // resources through the PoolModule + rw allocator. All inert until the GameDataModule runs.
    void BundleLoaderModule::Construct() { mLoadRequests.Construct(); }
    bool BundleLoaderModule::Update(void* /*lpInputBuffer*/, void* /*lpOutputBuffer*/) { return false; }
    void BundleLoaderModule::ProcessReceiverQueue() {}

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
        const s32 liCount = mLoadRequests.GetLength();
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
                const s32 liLoaded = lLoader.LoadBundle(lrRequest.macFileName, lpPool, lpfnResolveType);
                *CgsDev::Log::gpDebugPrint << "[stream] LoadBundle '" << lrRequest.macFileName
                                          << "' -> pool " << (s32)lrRequest.miPoolId
                                          << ": " << (s32)liLoaded << " resources\n";
                if (liLoaded < 0)
                    lResponse.meResult = Events::LoadBundleResponse::E_RESULT_OUT_OF_MEMORY;
            }

            // Reply to the request's originating receiver queue (the X360 ProcessPoolResponses path; here a
            // direct post). Event id 2 == the LoadBundle event family.
            if (lrRequest.mpUser != 0)
                lrRequest.mpUser->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResponse), 2,
                                           static_cast<s32>(sizeof(lResponse)));
        }
        mLoadRequests.Clear();
    }
}
