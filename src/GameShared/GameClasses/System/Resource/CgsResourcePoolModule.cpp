#include "GameShared/GameClasses/System/Resource/CgsResourcePoolModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "rw/rwcore_structs.h"                        // rw::IResourceAllocator / Resource (CreatePool test driver)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // [5b TEST] trace

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

    // @ 0x82904B20 - create a pool from InitOptions into a free slot. Validates the id (>=0), asserts no
    // existing pool already uses it, finds the first free slot (GetId()==-1) and Pool::InitPool's it there
    // (which adopts the InitOptions' per-type memory + builds the heaps/entry arrays). Returns the pool, or
    // null + asserts if all 128 slots are in use. (The X360 also logs "Created pool <name>".)
    Pool* PoolModule::CreatePool(const Pool::InitOptions* lpOptions)
    {
        CGS_ASSERT(lpOptions->miId >= 0, "Pool id is invalid");                    // CgsPoolModule.cpp:916
        for (s32 li = 0; li < KI_MAX_POOLS; ++li)
            CGS_ASSERT(maPools[li].GetId() != lpOptions->miId, "Pool id is already in use");  // :922
        for (s32 li = 0; li < KI_MAX_POOLS; ++li)
        {
            if (maPools[li].GetId() == -1)
            {
                maPools[li].InitPool(lpOptions);
                return &maPools[li];
            }
        }
        CGS_ASSERT(false, "Failed to find free pool - increase max pools or get rid of some other ones!");  // :937
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

    // @ 0x828FC0B8 - bring up the pool manager. This pass lands the rw-allocator-INDEPENDENT
    // structural front half: base module Construct, the prepare/release stage init, and the per-pool
    // Construct of all 128 pools (each pool to its clean empty state). The allocator-gated back half
    // is DEFERRED (see below) - it needs the richer ResourceModule::InitOptions (type list + scratch
    // params) + the resource-type-object subsystem + ScratchPool::InitPool, none of which are wired
    // yet; nothing drives the pools (only Prepare, which already tolerates the constructed pools), so
    // running just the front half is safe + faithful to the X360 ordering.
    void PoolModule::Construct(const void* lpInitOptions, void* lpAllocator)
    {
        mbIsNewModule = true;   // X360 *(this+4)=1 (set at the end of Construct; base Prepare skips old IO)
        CgsModule::ModuleSingleBuffered::Construct();          // X360: ModuleSingleBuffered::Construct(a1)
        mePoolPrepareStage = E_POOLPREPARE_START;              // X360 *(a1+6700)=0
        mePoolReleaseStage = E_POOLRELEASE_DONE;               // X360 *(a1+6704)=3
        for (s32 li = 0; li < KI_MAX_POOLS; ++li)              // X360: Pool::Construct loop (stride 464)
            maPools[li].Construct();

        // ---- Type registry (allocator-INDEPENDENT portion) ----------------------------------------
        // Register the game-specific resource types (mPoolInitOptions.mpGameSpecificTypes) into maTypes
        // so FindResourceType can resolve them by id. The X360 first registers a built-in "IDList" type
        // (allocated via the allocator + given the IDListResourceType vtable, then GetTypeID/CanDefrag
        // cached) -- that, being allocator + IDListResourceType gated, is DEFERRED, so the registry
        // currently holds only the game-specific list (which is empty until RegisterResourceTypes fills
        // it -> this loop is a no-op for now, behaviour-preserving). The X360 id key is the type's cached
        // id (*(type+8) == Type::GetCachedId()).
        mNumTypes = 0;
        const InitOptions* lpOptions = static_cast<const InitOptions*>(lpInitOptions);
        if (lpOptions != 0 && lpOptions->mpGameSpecificTypes != 0)
        {
            for (s32 li = 0; li < lpOptions->miNumGameSpecificTypes && mNumTypes < KI_MAX_TYPES; ++li)
            {
                const Type* lpType = lpOptions->mpGameSpecificTypes[li].mpType;
                if (lpType == 0)
                    continue;
                maTypes[mNumTypes].mpType   = lpType;
                // X360 keys on the cached id (*(type+8) == GetCachedId()); our handler singletons don't run
                // InitCachedValues (deferred), so read the virtual GetTypeID() (the real id). [marked]
                maTypes[mNumTypes].muTypeId = lpType->GetTypeID();
                maTypes[mNumTypes].mpcName  = lpOptions->mpGameSpecificTypes[li].mpcName;
                ++mNumTypes;
            }
        }

        // ScratchPool: construct the defrag-staging object (allocator-INDEPENDENT: the X360 inlines its
        // 4x LinearMalloc::Construct + 2x stream Construct + budget/zero here). Its InitPool (which carves
        // the scratch overhead from the allocator) stays DEFERRED -- nothing defragments yet.
        mScratchPool.Construct();

        // (The real pool set is created by GameDataModule::CreatePools, which drives CreatePool over the
        // extracted memory-map table -- not here. The earlier single [5b TEST] pool was retired once that
        // data-driven loader landed.)
        (void)lpAllocator;

        // ---- DEFERRED back half (rw-allocator + subsystem gated) ----------------------------------
        // Still deferred (need the allocator + subsystems): (1) the built-in "IDList" type (allocate +
        // IDListResourceType vtable + cache); (2) ScratchPool::InitPool over an allocator-carved
        // OverheadMemoryRequired block (CgsPoolModule.cpp:119 "Out of memory"); (3) the 5 pool-type
        // resource regions through the allocator (:130/131/164-168); (4) Relocator::Construct + two
        // ID::HashString-keyed defrag sub-objects; (5) zeroing the defrag-state cluster.
    }
    bool PoolModule::Update(void* /*lpInputBuffer*/, void* /*lpOutputBuffer*/) { return false; }
    void PoolModule::ProcessReceiverQueue(void* /*lpOutputBuffer*/) {}
    void PoolModule::ProcessInputBuffer(void* /*lpInputBuffer*/, void* /*lpOutputBuffer*/) {}
}
