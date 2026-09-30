#include "GameShared/GameClasses/System/Resource/CgsResourcePoolModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "rw/rwcore_structs.h"                        // rw::IResourceAllocator / Resource (CreatePool test driver)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // [5b TEST] trace
#include "GameShared/GameClasses/Memory/CgsMemoryModuleIO.h"  // CreateResourceRequest/Response + MemoryIO buffers (async dispatch)
#include "GameShared/GameClasses/System/Resource/CgsPoolModuleIO.h"  // PoolIO::OutputBuffer (request target)
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"  // AcquireResourceRequest/Response
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"    // ResourceHandle (acquire-list output array)
#include "GameShared/GameClasses/System/Resource/CgsResourceIdList.h"    // ResourceIdList (acquire-list payload)
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"       // ResourcePtr<ResourceIdList>
#include "GameShared/GameClasses/System/Resource/CgsResourceIdListResourceType.h"
#include <new>

// Original pool manager lifecycle and native storage. The resource-list
// dispatch is integrated separately from construction of its working state.
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

    // ARTIST 828FC0B8: construct the pool bank, type registry, scratch staging,
    // allocation/relocation storage and the five request-processing states.
    void PoolModule::Construct(const void* lpInitOptions, void* lpAllocator)
    {
        mbIsNewModule = true;   // X360 *(this+4)=1 (set at the end of Construct; base Prepare skips old IO)
        CgsModule::ModuleSingleBuffered::Construct();          // X360: ModuleSingleBuffered::Construct(a1)
        mePoolPrepareStage = E_POOLPREPARE_START;              // X360 *(a1+6700)=0
        mePoolReleaseStage = E_POOLRELEASE_DONE;               // X360 *(a1+6704)=3
        for (s32 li = 0; li < KI_MAX_POOLS; ++li)              // X360: Pool::Construct loop (stride 464)
            maPools[li].Construct();

        rw::IResourceAllocator* lpResourceAllocator = static_cast<rw::IResourceAllocator*>(lpAllocator);
        CGS_ASSERT(lpResourceAllocator != nullptr, "lpAllocator");

        // Register the original built-in ID-list handler before game-specific
        // types. Its cache is initialized through the same virtuals as the game types.
        mNumTypes = 0;
        rw::ResourceDescriptor lIdListDescriptor;
        lIdListDescriptor.m_baseResourceDescriptors[0].m_size = sizeof(IdListResourceType);
        lIdListDescriptor.m_baseResourceDescriptors[0].m_alignment = alignof(IdListResourceType);
        void* lpIdListMemory = lpResourceAllocator->DoAllocate(lIdListDescriptor, nullptr).m_baseResources[0];
        CGS_ASSERT(lpIdListMemory != nullptr, "Out of memory\n");
        IdListResourceType* lpIdListType = ::new (lpIdListMemory) IdListResourceType;
        lpIdListType->InitCachedValues();
        maTypes[mNumTypes].mpType = lpIdListType;
        maTypes[mNumTypes].muTypeId = lpIdListType->GetCachedId();
        maTypes[mNumTypes].mpcName = "IDList";
        ++mNumTypes;
        const InitOptions* lpOptions = static_cast<const InitOptions*>(lpInitOptions);
        if (lpOptions != 0 && lpOptions->mpGameSpecificTypes != 0)
        {
            for (s32 li = 0; li < lpOptions->miNumGameSpecificTypes && mNumTypes < KI_MAX_TYPES; ++li)
            {
                const Type* lpType = lpOptions->mpGameSpecificTypes[li].mpType;
                if (lpType == 0)
                    continue;
                maTypes[mNumTypes].mpType   = lpType;
                // X360 keys on the cached id (*(type+8) == GetCachedId()); TypeRegistry::Register
                // runs InitCachedValues on every handler, so the cached id is live here.
                maTypes[mNumTypes].muTypeId = lpType->GetCachedId();
                maTypes[mNumTypes].mpcName  = lpOptions->mpGameSpecificTypes[li].mpcName;
                ++mNumTypes;
            }
        }

        // The original staging pool borrows the caller's resource buffers and
        // owns tables carved from its separately allocated overhead block below.
        mScratchPool.Construct();

        // Bind the receiver queue's backing buffer (create/delete-pool events land here via
        // ProcessMemoryResponse, drained by ProcessReceiverQueue).
        mReceiverQueue.Construct();

        // Clear the copy engine's busy latches before binding its operation
        // storage and bounce buffer below.
        mRelocator.Construct();

        // (The real pool set is created by GameDataModule::CreatePools, which drives CreatePool over the
        // extracted memory-map table -- not here. The earlier single [5b TEST] pool was retired once that
        // data-driven loader landed.)
        // ARTIST 828FC0B8 allocates 4096 request/result records for each memory
        // type. Native records contain 64-bit pointers, so retain the count and
        // use their host sizeof/alignment when carving the parent allocator.
        CGS_ASSERT(lpOptions != nullptr, "lpInitOptions");
        ScratchPool::InitOptions lScratchOptions = {};
        lScratchOptions.muMaxEntries = static_cast<u32>(lpOptions->miMaxResourceToDefrag);
        lScratchOptions.miBankId = 999;
        lScratchOptions.mDescriptor = lpOptions->mDefragBufferDescriptor;
        lScratchOptions.mResource = lpOptions->mDefragBufferResource;
        lScratchOptions.muOverheadMemorySize = ScratchPool::GetOverheadMemoryRequired(&lScratchOptions);
        rw::ResourceDescriptor lScratchDescriptor;
        lScratchDescriptor.m_baseResourceDescriptors[0].m_size = lScratchOptions.muOverheadMemorySize;
        lScratchDescriptor.m_baseResourceDescriptors[0].m_alignment = 16;
        lScratchOptions.mpOverhead = lpResourceAllocator->DoAllocate(lScratchDescriptor, nullptr).m_baseResources[0];
        CGS_ASSERT(lScratchOptions.mpOverhead != nullptr, "Out of memory\n");
        mScratchPool.Prepare();
        mScratchPool.InitPool(&lScratchOptions);
        for (s32 t = 0; t < 3; ++t)
        {
            rw::ResourceDescriptor lDescriptor = {};
            lDescriptor.m_baseResourceDescriptors[0].m_size = sizeof(AllocRequest) * KI_MAX_ALLOCATION_REQUESTS;
            lDescriptor.m_baseResourceDescriptors[0].m_alignment = alignof(AllocRequest);
            mAllocListSet.mapAllocRequests[t] = static_cast<AllocRequest*>(
                lpResourceAllocator->DoAllocate(lDescriptor, nullptr).m_baseResources[0]);
            lDescriptor.m_baseResourceDescriptors[0].m_size = sizeof(AllocResult) * KI_MAX_ALLOCATION_REQUESTS;
            lDescriptor.m_baseResourceDescriptors[0].m_alignment = alignof(AllocResult);
            mAllocListSet.mapAllocResults[t] = static_cast<AllocResult*>(
                lpResourceAllocator->DoAllocate(lDescriptor, nullptr).m_baseResources[0]);
            CGS_ASSERT(mAllocListSet.mapAllocRequests[t] != nullptr, "Out of memory\n");
            CGS_ASSERT(mAllocListSet.mapAllocResults[t] != nullptr, "Out of memory\n");
        }
        mAllocListSet.ClearCountsAndResults();

        // ARTIST 828FC0B8: request arrays, relocation/source arrays, a linear
        // heap with eight look-ahead records, and shared distribution/job storage.
        // FLAG PC-platform leaf: allocation counts are original; native records
        // widen their pointers and therefore require host sizeof/alignment.
        const auto lAllocateWorkingMemory = [lpResourceAllocator](size_t luSize, u32 luAlignment) -> void*
        {
            rw::ResourceDescriptor lDescriptor;
            lDescriptor.m_baseResourceDescriptors[0].m_size = static_cast<u32>(luSize);
            lDescriptor.m_baseResourceDescriptors[0].m_alignment = luAlignment;
            void* lpMemory = lpResourceAllocator->DoAllocate(lDescriptor, nullptr).m_baseResources[0];
            CGS_ASSERT(lpMemory != nullptr, "Out of memory\n");
            return lpMemory;
        };
        mpAddressedAllocRequests = static_cast<AllocRequestAddressed*>(lAllocateWorkingMemory(
            sizeof(AllocRequestAddressed) * KI_MAX_ALLOCATION_REQUESTS, alignof(AllocRequestAddressed)));
        mpRelocateRequests = static_cast<RelocateRequest*>(lAllocateWorkingMemory(
            sizeof(RelocateRequest) * KU_MAX_POOL_ENTRIES, 128));
        mpLinearHeapNodes = static_cast<LinearHeapNode*>(lAllocateWorkingMemory(
            sizeof(LinearHeapNode) * (KU_MAX_LINEAR_HEAP_LENGTH + 8u), 128));
        const size_t luSharedStride = sizeof(DistributionEntry) > sizeof(RelocationEntry)
            ? sizeof(DistributionEntry) : sizeof(RelocationEntry);
        mpDistributionEntries = static_cast<DistributionEntry*>(lAllocateWorkingMemory(
            luSharedStride * KU_MAX_POOL_ENTRIES, 128));
        mpRelocationEntries = reinterpret_cast<RelocationEntry*>(mpDistributionEntries);
        mpRelocateSources = static_cast<RelocateSource*>(lAllocateWorkingMemory(
            sizeof(RelocateSource) * KU_MAX_POOL_ENTRIES, 128));
        mRelocationParams.mpOps = mpRelocationEntries;
        mRelocationParams.miNumOps = 0;
        mRelocationParams.mpBounceBuffer = lScratchOptions.mResource.m_baseResources[0];
        mRelocationParams.miBounceBufferSize = lScratchOptions.mDescriptor.m_baseResourceDescriptors[0].m_size;
        mProcessState = E_UPDATESTATE_IDLE;
        mAllocateState.Construct(this);
        mDeAllocateState.Construct(this);
        mIntelliFragState.Construct(this);
        mLiveUpdateState.Construct(this);
        mEmergencyFragState.Construct(this);
        mPendingAllocationRequests.Construct();


    }
    // ARTIST 829076D8: process requests, advance the module state, then tick each
    // live pool. Defrag escalation may advance again within this same update.
    bool PoolModule::Update(void* lpInputBuffer, void* lpOutputBuffer)
    {
        PoolIO::InputBuffer*  lpIn  = static_cast<PoolIO::InputBuffer*>(lpInputBuffer);
        PoolIO::OutputBuffer* lpOut = static_cast<PoolIO::OutputBuffer*>(lpOutputBuffer);

        lpOut->LockForWrite();
        lpIn->LockForRead();
        ProcessInputBuffer(lpIn, lpOut);
        lpIn->UnlockForRead();
        ProcessReceiverQueue(lpOut);
        if (mProcessState == E_UPDATESTATE_IDLE)
        {
            Events::AllocateResourceListRequest lRequest;
            if (mPendingAllocationRequests.Pop(&lRequest))
                DoAllocateResourceListRequest(&lRequest);
        }
        switch (mProcessState)
        {
        case E_UPDATESTATE_ALLOCATING_LIST: UpdateAllocating(lpOut); break;
        case E_UPDATESTATE_DEALLOCATING_LIST: UpdateDeAllocating(lpOut); break;
        case E_UPDATESTATE_LIVEUPDATE: UpdateLiveUpdate(lpOut); break;
        }
        if (mProcessState == E_UPDATESTATE_INTELLIFRAG)
            UpdateIntelliFrag(lpOut);
        if (mProcessState == E_UPDATESTATE_EMERGENCYFRAG)
            UpdateEmergencyFrag(lpOut);
        lpOut->UnlockForWrite();
        for (s32 li = 0; li < KI_MAX_POOLS; ++li)
            if (maPools[li].GetId() != -1)
                maPools[li].Update();
        return mProcessState == E_UPDATESTATE_EMERGENCYFRAG;
    }

    // @ 0x82906FD0 - drain the receiver queue: dispatch each event by id (10 = CreatePool ->
    // DoCreatePoolRequest, 13 = DeletePool -> deferred), then Clear. The events are the memory responses
    // ProcessMemoryResponse forwarded here.
    void PoolModule::ProcessReceiverQueue(void* /*lpOutputBuffer*/)
    {
        if (mReceiverQueue.GetCount() > 0)
        {
            const CgsModule::Event* lpEvent = 0;
            s32 liSize = 0;
            s32 liId = mReceiverQueue.GetFirstEvent(&lpEvent, &liSize);
            while (liId != -1 && lpEvent != 0)
            {
                if (liId == 10)        // CreatePool
                    DoCreatePoolRequest(reinterpret_cast<const CgsMemory::MemoryIO::CreateResourceResponse*>(lpEvent));
                else if (liId == 13)   // DeletePool
                    DoDeletePoolRequest(lpEvent);
                else
                    CGS_ASSERT(false, "Invalid Event Id.\n");

                const CgsModule::Event* lpNext = 0;
                liId = mReceiverQueue.GetNextEvent(lpEvent, &lpNext, &liSize);
                lpEvent = lpNext;
            }
        }
        mReceiverQueue.Clear();
    }

    // Drain the pool input buffer: each CreatePool request (id 0) -> SendCreatePoolMemoryRequest (queue
    // the pending options + emit the backing-memory request onto the output). The caller (Update) holds
    // the input read-locked + the output write-locked. [Acquire/Validate/Invalidate/etc. (ids 4..8) are
    // deferred -- not used during pool bring-up.]
    void PoolModule::ProcessInputBuffer(void* lpInputBuffer, void* lpOutputBuffer)
    {
        PoolIO::InputBuffer*  lpIn  = static_cast<PoolIO::InputBuffer*>(lpInputBuffer);
        PoolIO::OutputBuffer* lpOut = static_cast<PoolIO::OutputBuffer*>(lpOutputBuffer);
        const PoolIO::InputBuffer::PoolInputQueue* lpQ =
            static_cast<const PoolIO::InputBuffer&>(*lpIn).GetPoolInputQueue();

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liId = lpQ->GetFirstEvent(&lpEvent, &liSize);
        while (liId != -1 && lpEvent != 0)
        {
            if (liId == 0)        // CreatePool
            {
                const CreatePoolRequestEvent* lpReq = reinterpret_cast<const CreatePoolRequestEvent*>(lpEvent);
                SendCreatePoolMemoryRequest(lpReq->mOptions, lpReq->miBankId, lpReq->miParentBankId,
                                            lpReq->mauRegion, lpReq->mauAlign, lpOut);
            }
            else if (liId == 4)   // AcquireResource
            {
                DoAcquireResourceRequest(reinterpret_cast<const Events::AcquireResourceRequest*>(lpEvent), lpOut);
            }
            else if (liId == 5)   // AcquireResourceList (the per-zone collision protocol)
            {
                DoAcquireResourceListRequest(
                    reinterpret_cast<const Events::AcquireResourceListRequest*>(lpEvent), lpOut);
            }
            else if (liId == 16)
            {
                mPendingAllocationRequests.Push(reinterpret_cast<const Events::AllocateResourceListRequest*>(lpEvent));
            }
            else if (liId == 18)
            {
                DoFixUpAndResolveResourceListRequest(
                    reinterpret_cast<const Events::FixUpAndResolveResourceListRequest*>(lpEvent), lpOut);
            }
            else if (liId == 20)
            {
                DoUnloadResourceListRequest(reinterpret_cast<const Events::UnloadResourceListRequest*>(lpEvent), lpOut);
            }
            const CgsModule::Event* lpNext = 0;
            liId = lpQ->GetNextEvent(lpEvent, &lpNext, &liSize);
            lpEvent = lpNext;
        }
    }

    // Forward one CreateResource memory response onto its originating receiver queue as a CreatePool
    // event (id 10). This is the pool-create slice of the ResourceModule's ProcessMemoryResponses shuttle
    // (which routes each memory response to response->GetUser()); here driven per-response by the pump.
    void PoolModule::ProcessMemoryResponse(const CgsMemory::MemoryIO::CreateResourceResponse* lpResponse)
    {
        CGS_ASSERT(lpResponse != 0, "lpResponse");
        if (lpResponse == 0)
            return;
        CgsModule::BaseEventReceiverQueue* lpReceiver = lpResponse->GetUser();
        CGS_ASSERT(lpReceiver != 0, "memory response has no receiver queue");
        if (lpReceiver != 0)
            lpReceiver->AddEvent(reinterpret_cast<const CgsModule::Event*>(lpResponse), 10 /*CreatePool*/,
                                 static_cast<s32>(sizeof(*lpResponse)));
    }

    // Look up a created pool by id (null if absent).
    Pool* PoolModule::GetPool(s32 liPoolId)
    {
        const s32 li = GetPoolIndex(liPoolId);
        return (li >= 0) ? &maPools[li] : 0;
    }

    // @ 0x828FCD48 - acquire a single resource by id from its pool and reply with its handle. Find the pool
    // (GetPoolIndex), look up the resource (Pool::FindResource with status mask 2 = loaded), build the
    // resolved ResourceHandle pair (mpResourceMemory -> the entry's main-memory slot; mpSourceEntry -> the
    // entry) -- or both null if absent -- echo the request's reply target/ids, and post the response on the
    // pool output queue (tag 6). The X360 routes that response back to the requester via the shuttle.
    void PoolModule::DoAcquireResourceRequest(const Events::AcquireResourceRequest* lpRequest, PoolIO::OutputBuffer* lpOutput)
    {
        CGS_ASSERT(lpOutput != 0, "lpOutputBuffer");

        Events::AcquireResourceResponse lResponse;
        static_cast<Events::PoolEvent&>(lResponse) = static_cast<const Events::PoolEvent&>(*lpRequest);   // echo user/id/pool
        lResponse.mResourceId      = lpRequest->mResourceId;
        lResponse.mpResourceMemory = 0;
        lResponse.mpSourceEntry    = 0;

        const s32 liPoolIndex = GetPoolIndex(lpRequest->miPoolId);
        if (liPoolIndex >= 0)
        {
            s32 liIndex = -1;
            Entry* lpEntry = maPools[liPoolIndex].FindResource(lpRequest->mResourceId, lpRequest->mbCheckRefCount, 2, &liIndex);
            if (lpEntry != 0)
            {
                lResponse.mpResourceMemory = &lpEntry->mResource.m_baseResources[E_MEMTYPE_MAINMEMORY];
                lResponse.mpSourceEntry    = lpEntry;
            }
        }

        lpOutput->GetPoolOutputQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResponse), 6,
                                                 static_cast<s32>(sizeof(lResponse)));
    }

    // @ 0x828FCE40 - acquire EVERY resource named by a resource-ID-LIST resource into the caller's handle
    // array, then echo the request back with the count it filled. This is the protocol the world's
    // per-zone collision uses: WorldEntityModule::PrepareZoneCollision @0x82302C38 posts one
    // AcquireResourceListRequest per zone naming "TRK_CLIL<n>", and the reply's handles are what
    // AddCollisionZoneToSceneManager turns into InEventAddPolySoupList events.
    //
    // Faithful to the X360 spine, read from the ASM (the pseudocode fuses the 64-bit id across the
    // register pair and mis-attributes it as a status-mask argument -- both FindResource calls in fact
    // take r5=0 / r6=2 / r7=0, r6 being loaded ONCE at 0x828FCE98 and reused by the loop):
    //     listEntry = maPools[GetPoolIndex(req->miPoolId)].FindResource(req->mListResourceId, 0, 2, 0)
    //     ResourcePtr<ResourceIdList> ptr(listEntry's handle)
    //     count = ptr->GetNumIds();  assert(count <= req->miMaxHandles)
    //     for i: e = pool.FindResource(ptr->GetId(i), 0, 2, 0); req->mpHandles[i] = e's handle
    //     reply {req echoed, miNumHandles = count} on the pool output queue, tag 7, 32 bytes
    // The X360 re-runs GetPoolIndex inside the loop (0x828FD0A0); kept, since the pool table is
    // mutable across the call in principle and the cost is a 128-entry scan the console also pays.
    void PoolModule::DoAcquireResourceListRequest(const Events::AcquireResourceListRequest* lpRequest,
                                                  PoolIO::OutputBuffer* lpOutput)
    {
        CGS_ASSERT(lpOutput != 0, "lpOutputBuffer");   // X360 CgsPoolModule.cpp:1371
        if (lpOutput == 0)
            return;

        // -- resolve the LIST resource itself -------------------------------------------------
        const s32 liListPoolIndex = GetPoolIndex(lpRequest->miPoolId);
        Entry* lpListEntry = (liListPoolIndex >= 0)
            ? maPools[liListPoolIndex].FindResource(lpRequest->mListResourceId, false, 2, 0)
            : 0;
        // X360 line 1377, message "Failed to find list resouce " << id [sic -- the console's own
        // spelling; the streamed id is dropped from the stringized condition per house convention].
        CGS_ASSERT(lpListEntry != 0, "Failed to find list resouce ");
        if (lpListEntry == 0)
            return;

        ResourceHandle lListHandle;
        lListHandle.mpResourceMemory = &lpListEntry->mResource.m_baseResources[E_MEMTYPE_MAINMEMORY];
        lListHandle.mpSourceEntry    = lpListEntry;
        ResourcePtr<ResourceIdList> lList(lListHandle);

        ResourceHandle* const lpaHandles = lpRequest->mpHandles;
        const u32 luNumIds = lList->GetNumIds();
        CGS_ASSERT(static_cast<s32>(luNumIds) <= lpRequest->miMaxHandles,
                   "Too many handles in list\n");      // X360 line 1388

        // -- resolve every listed resource into the caller's array ----------------------------
        for (u32 lu = 0; lu < luNumIds; ++lu)
        {
            const ID lId = lList->GetId(lu);           // bounds assert CgsResourceIdList.h:145

            const s32 liPoolIndex = GetPoolIndex(lpRequest->miPoolId);
            Entry* lpEntry = (liPoolIndex >= 0)
                ? maPools[liPoolIndex].FindResource(lId, false, 2, 0)
                : 0;
            // X360 line 1395, "Failed to find resouce <16 hex digits> from list\n" [sic].
            CGS_ASSERT(lpEntry != 0, "Failed to find resouce  from list\n");

            if (lpaHandles != 0 && lpEntry != 0)
            {
                lpaHandles[lu].mpResourceMemory = &lpEntry->mResource.m_baseResources[E_MEMTYPE_MAINMEMORY];
                lpaHandles[lu].mpSourceEntry    = lpEntry;
            }
        }

        // -- echo the request back, with the count it filled ----------------------------------
        Events::AcquireResourceListResponse lResponse;
        static_cast<Events::PoolEvent&>(lResponse) = static_cast<const Events::PoolEvent&>(*lpRequest);
        lResponse.mListResourceId = lpRequest->mListResourceId;
        lResponse.mpHandles       = lpaHandles;
        lResponse.miNumHandles    = static_cast<s32>(luNumIds);

        lpOutput->GetPoolOutputQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResponse), 7,
                                                 static_cast<s32>(sizeof(lResponse)));
    }

    // @ 0x828F3A50 - queue the pending pool options and emit the CreateResource memory request that makes
    // the MemoryModule carve the pool's backing bank. Faithful to the X360 core: push the request onto the
    // CreatePoolRequest_128 FIFO, build the resource descriptor (here per-type {size,align} from the carved
    // region), then publish a type-6 CREATE_RESOURCE event. [transport deviation -- see the header: the
    // event is written straight to the MemoryModule input buffer instead of through PoolIO::OutputBuffer +
    // the ResourceModule shuttle, which are not yet reconstructed.]
    void PoolModule::SendCreatePoolMemoryRequest(const Pool::InitOptions& lrOptions, s32 liBankId, s32 liParentBankId,
                                                 const u32 lauRegion[3], const u32 lauAlign[3],
                                                 PoolIO::OutputBuffer* lpPoolOutput)
    {
        CGS_ASSERT(lpPoolOutput != 0, "lpOutputBuffer");
        CGS_ASSERT(miPendingCount < KI_MAX_PENDING_CREATE, "CreatePoolRequest FIFO overflow");
        if (lpPoolOutput == 0 || miPendingCount >= KI_MAX_PENDING_CREATE)
            return;

        // Push the pending pool options (FIFO) so DoCreatePoolRequest can match the memory response.
        maPendingCreate[miPendingTail] = lrOptions;
        miPendingTail = (miPendingTail + 1) % KI_MAX_PENDING_CREATE;
        ++miPendingCount;

        // Build the CreateResource request describing the bank the MemoryModule must allocate. The reply
        // target is this module's receiver queue: when the memory response returns, ProcessMemoryResponse
        // routes it there (event 10) and ProcessReceiverQueue -> DoCreatePoolRequest stands the pool up.
        CgsMemory::MemoryIO::CreateResourceRequest lRequest;
        lRequest.Construct(&mReceiverQueue, lrOptions.miId);
        lRequest.SetBankName(lrOptions.mpcName);
        lRequest.SetBankId(liBankId);
        lRequest.SetParentBankId(liParentBankId);

        rw::ResourceDescriptor lDesc;                   // <4>; pool uses memory types 0..2 (type 3 unused)
        for (s32 lt = 0; lt < static_cast<s32>(rw::KU_RESOURCE_LANE_COUNT); ++lt)
        {
            lDesc.m_baseResourceDescriptors[lt].m_size      = (lt < 3) ? lauRegion[lt] : 0u;
            lDesc.m_baseResourceDescriptors[lt].m_alignment = (lt < 3) ? lauAlign[lt]  : 1u;
        }
        lRequest.SetDescriptor(&lDesc);

        // Emit onto the pool module's resource-request output queue (the caller holds the write lock).
        // Queue tag 10 (CreatePool) is the routing id ProcessPoolResourceRequests forwards on (distinct
        // from the embedded request's meEventType=CREATE_RESOURCE, which the MemoryModule dispatches on).
        lpPoolOutput->GetPoolResourceRequestQueue()->AddEvent(&lRequest, 10 /*CreatePool*/,
                                                              static_cast<s32>(sizeof(lRequest)));
    }

    // @ 0x82905000 - pop the matching pending options (FIFO), adopt the memory the MemoryModule allocated
    // (the response's per-type rw::Resource data pointers), and stand up the pool. [deviation -- see the
    // header: reached directly from the pump with the memory response rather than via the receiver queue.]
    Pool* PoolModule::DoCreatePoolRequest(const CgsMemory::MemoryIO::CreateResourceResponse* lpResponse)
    {
        CGS_ASSERT(lpResponse != 0, "lpResponse");
        CGS_ASSERT(miPendingCount > 0, "CreatePoolRequest FIFO underflow");
        if (lpResponse == 0 || miPendingCount <= 0)
            return 0;

        // Pop the pending options (the X360 stores the raw request; we carry the resolved InitOptions).
        Pool::InitOptions lOptions = maPendingCreate[miPendingHead];
        miPendingHead = (miPendingHead + 1) % KI_MAX_PENDING_CREATE;
        --miPendingCount;

        if (lpResponse->GetResult() != CgsMemory::MemoryIO::E_RESULT_OK)
            return 0;

        // Adopt the carved bank's per-type memory into the pool options (dense 0..2 runtime types -- the
        // memory backend returns the bank GetData(type) in the same indices it was requested in).
        const rw::Resource& lrResource = lpResponse->GetResource();
        for (s32 lt = 0; lt < 3; ++lt)
            lOptions.mResource.m_baseResources[lt] = lrResource.m_baseResources[lt];

        return CreatePool(&lOptions);
    }
}
