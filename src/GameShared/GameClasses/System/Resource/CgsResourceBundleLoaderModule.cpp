#include "GameShared/GameClasses/System/Resource/CgsResourceBundleLoaderModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

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
    void BundleLoaderModule::Construct() {}
    bool BundleLoaderModule::Update(void* /*lpInputBuffer*/, void* /*lpOutputBuffer*/) { return false; }
    void BundleLoaderModule::ProcessReceiverQueue() {}
}
