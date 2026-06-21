#include "GameShared/GameClasses/System/Resource/CgsResourcePoolModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsResource::PoolModule - see the header. This pass reconstructs the rw-allocator-
// INDEPENDENT spine (GetPoolIndex / FindResourceType / Prepare / Release / Destruct).
// Construct + the dispatch spine + the DoXxxRequest handlers + the defrag-state machine are
// DEFERRED (rw allocator middleware / defrag subsystem) as inert marked stubs.
namespace CgsResource
{
    // @ 0x828D80E8 - linear search of the 128 pools for the one with this id; -1 if absent.
    s32 PoolModule::GetPoolIndex(s32 liPoolId)
    {
        CGS_ASSERT(liPoolId >= 0, "Invalid pool id");
        for (s32 li = 0; li < KI_MAX_POOLS; ++li)
        {
            if (maPools[li].GetId() == liPoolId)
                return li;
        }
        return -1;
    }

    // @ 0x828D8268 - linear search of the registered types for this id; null if absent.
    const Type* PoolModule::FindResourceType(u32 luTypeId)
    {
        for (s32 li = 0; li < mNumTypes; ++li)
        {
            if (maTypes[li].muTypeId == luTypeId)
                return maTypes[li].mpType;
        }
        return 0;
    }

    // @ 0x828E2C60 - resumable prepare stage machine: bring up the base module, then Prepare
    // all 128 pools (retrying next frame until all report ready), then clear the receiver
    // queue. Falls through the stages within a call; re-enters at the current stage if a
    // sub-step is not yet ready.
    bool PoolModule::Prepare()
    {
        mbIsNewModule = true;   // [reliable] see ResourceModule::Prepare -- set before base Prepare
        switch (mePoolPrepareStage)
        {
        case E_POOLPREPARE_START:
        case E_POOLPREPARE_BASE:
            mePoolPrepareStage = E_POOLPREPARE_BASE;
            if (!CgsModule::ModuleSingleBuffered::Prepare())
                return false;
            // fall through
        case E_POOLPREPARE_POOLS:
        {
            mePoolPrepareStage = E_POOLPREPARE_POOLS;
            bool lbAllReady = true;
            for (s32 li = 0; li < KI_MAX_POOLS; ++li)
            {
                if (!maPools[li].Prepare())
                    lbAllReady = false;
            }
            if (!lbAllReady)
                return false;
            mReceiverQueue.Clear();
            // fall through
        }
        case E_POOLPREPARE_DONE:
            mePoolReleaseStage = E_POOLRELEASE_START;
            mePoolPrepareStage = E_POOLPREPARE_DONE;
            return true;
        default:
            CGS_ASSERT(false, "Invalid prepare stage");
            return false;
        }
    }

    // @ 0x828E2D78 - resumable release stage machine: Release all 128 pools, clear the
    // receiver queue, then tear down the base module.
    bool PoolModule::Release()
    {
        switch (mePoolReleaseStage)
        {
        case E_POOLRELEASE_START:
        case E_POOLRELEASE_POOLS:
        {
            mePoolReleaseStage = E_POOLRELEASE_POOLS;
            bool lbAllReleased = true;
            for (s32 li = 0; li < KI_MAX_POOLS; ++li)
            {
                if (!maPools[li].Release())
                    lbAllReleased = false;
            }
            if (!lbAllReleased)
                return false;
            mReceiverQueue.Clear();
            // fall through
        }
        case E_POOLRELEASE_BASE:
            mePoolReleaseStage = E_POOLRELEASE_BASE;
            if (!CgsModule::ModuleSingleBuffered::Release())
                return false;
            // fall through
        case E_POOLRELEASE_DONE:
            mePoolPrepareStage = E_POOLPREPARE_START;
            mePoolReleaseStage = E_POOLRELEASE_DONE;
            return true;
        default:
            CGS_ASSERT(false, "Invalid release stage");
            return false;
        }
    }

    // @ 0x828E2E90 - clear the receiver queue and tear down the base module.
    void PoolModule::Destruct()
    {
        mReceiverQueue.Clear();
        CgsModule::ModuleSingleBuffered::Destruct();
    }

    // ---- DEFERRED (rw allocator middleware / defrag subsystem) ------------------------
    // Construct (0x828FC0B8) allocates the 128 pools' overhead + the ScratchPool's scratch
    // memory through the rw::IResourceAllocator and builds the type registry - it is gated on
    // the same rwcore allocator layer as MemoryModule's 3 rw handlers. The dispatch spine
    // (Update 0x829076D8 / ProcessReceiverQueue 0x82906FD0 / ProcessInputBuffer) and all the
    // DoXxxRequest handlers (DoCreatePool 0x82905000, DoDeletePool 0x828D81D0, DoValidatePool
    // 0x82901868, DoInvalidatePool 0x828FD448, DoAcquireResource 0x828FCD48, DoAcquireResourceList
    // 0x828FCE40, DoAllocateResourceList 0x828EC590, DoUnloadResourceList 0x828FD310,
    // DoFixUpAndResolveResourceList 0x82901748) plus the UpdateXxx defrag-state drivers create/
    // allocate resources through that same allocator and drive the deferred defrag-state cluster.
    // All inert until the GameDataModule runs; reconstruct with the rwcore allocator + the
    // PoolModule defrag-state machine.
    void PoolModule::Construct(const void* /*lpInitOptions*/, void* /*lpAllocator*/) {}
    bool PoolModule::Update(void* /*lpInputBuffer*/, void* /*lpOutputBuffer*/) { return false; }
    void PoolModule::ProcessReceiverQueue(void* /*lpOutputBuffer*/) {}
    void PoolModule::ProcessInputBuffer(void* /*lpInputBuffer*/, void* /*lpOutputBuffer*/) {}
}
