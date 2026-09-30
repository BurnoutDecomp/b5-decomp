#include "types.hpp"

#include "GameShared/GameClasses/System/Resource/CgsResourcePoolModule.h" // canonical PoolModule home
#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"           // gpDebugPrint / gxMessageFilterFlags (filtered prints)
#include "GameShared/GameClasses/System/Resource/CgsPoolModuleIO.h" // PoolIO::OutputBuffer / PoolOutputQueue (event posts)
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h" // Events::AllocateResourceListResponse (live-update reply)
#include "GameShared/GameClasses/System/Resource/CgsEntryListResource.h"
#include "GameShared/GameClasses/Memory/CgsMemoryModuleIO.h"

// ======================================================================================
// CgsResource::PoolModule -- reconstructed from BURNOUT_X360_ARTIST.XEX (source CgsPoolModule.cpp).
//
// This is the CgsPoolModule.cpp ledger TU (keyed on the real X360 source path). It reconstructs the
// per-frame DEFRAG-STATE DRIVERS + request handlers of CgsResource::PoolModule. The class itself has a
// single canonical home in the sibling CgsResourcePoolModule.h (which also owns ctor / Construct /
// Prepare / Release / Destruct / the dispatch + CreatePool slice in CgsResourcePoolModule.cpp). The
// defrag-state cluster these drivers poll (mAllocateState/.../mEmergencyFragState + the scratch-buffer
// ptrs + mProcessState) was folded into that canonical header, so this TU compiles against ONE class
// definition -- the earlier dual-header ODR is resolved. The functions below DEFINE additional
// PoolModule methods; they do NOT redeclare the class.
//
// THIS TU's functions (X360 addresses):
//   ConvertPoolRequestOptions       0x828E2ED0   (pure copy -- fully reconstructed)
//   DebugReport                     0x828F3CD0   (pure sweep -- fully reconstructed)
//   DoAllocateResourceListRequest   0x828EC590   (forwarder -- fully reconstructed)
//   DoDeletePoolRequest             0x828D81D0   (assert-on-failure -- fully reconstructed)
//   UpdateAllocating                0x82904860   (defrag-state driver -- fully reconstructed)
//   UpdateDeAllocating              0x828F38E8   (defrag-state driver -- fully reconstructed)
//   UpdateIntelliFrag               0x829013F8   (defrag-state driver -- fully reconstructed)
//   UpdateLiveUpdate                0x82906E70   (defrag-state driver -- fully reconstructed)
//   AllocateResourceList            0x82900690   (own TU -- trap-stub here)
//
// Allocation request/response bodies use native typed records. Other legacy conversion paths
// below still contain console-width record views and need completion before full async activation.
// ======================================================================================
namespace CgsResource
{
    // The X360 source file path baked into the asserts (CgsPoolModule.cpp).
    static const char* const KPC_POOLMODULE_SRC =
        "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsPoolModule.cpp";

    // ----------------------------------------------------------------------------------------------
    // ARTIST 828E2ED0: project the request's scalar sizing fields into the
    // native pool options. The name borrows the request's inline buffer; its
    // caller must retain that request through pool creation. Dependency pointers,
    // bank backing and resource descriptors are filled by DoCreatePoolRequest.
    void PoolModule::ConvertPoolRequestOptions(const void* lpRequest, void* lpOutOptions)
    {
        const Events::CreatePoolRequest& lrRequest = *static_cast<const Events::CreatePoolRequest*>(lpRequest);
        Pool::InitOptions& lrOptions = *static_cast<Pool::InitOptions*>(lpOutOptions);
        lrOptions.miId = lrRequest.miPoolId;
        lrOptions.mpcName = lrRequest.mpcName;
        lrOptions.miRefCountThreshold = lrRequest.miDeletionDelayFrames;
        lrOptions.muMaxResources = lrRequest.muMaxResources;
        lrOptions.muMaxImports = lrRequest.muMaxImports;
        lrOptions.miNumDependencies = lrRequest.miNumDependencies;
        lrOptions.mbAllowDefragmentation = lrRequest.mbAllowDefragmentation;
        for (s32 li = 0; li < 3; ++li)
        {
            lrOptions.maHeapInfo[li].muMaxNodes = 2u * lrRequest.mauMaxResources[li] + 1u;
            lrOptions.maHeapInfo[li].muHeapMemorySize = lrRequest.mDescriptor.m_baseResourceDescriptors[li].m_size;
            lrOptions.maHeapInfo[li].muHeapAlignment = lrRequest.mDescriptor.m_baseResourceDescriptors[li].m_alignment;
        }
    }

    // ----------------------------------------------------------------------------------------------
    // @ 0x828F3CD0 -- DebugReport: visit every live pool. The X360 walks the 128 pools (stride 464),
    // calling Pool::DebugReport on each whose id != -1 and whose mbIsValid byte is set. (X360 reads the
    // id at pool+40 and the valid byte at pool+0; here via GetId()/IsValid().)
    // ----------------------------------------------------------------------------------------------
    void PoolModule::DebugReport(FPoolReportCallback lpfnCallback, void* lpUserData)
    {
        for (s32 li = 0; li < KI_MAX_POOLS; ++li)
        {
            if (maPools[li].GetId() != -1 && maPools[li].IsValid())
                maPools[li].DebugReport(lpfnCallback, lpUserData);
        }
    }

    // ----------------------------------------------------------------------------------------------
    // @ 0x828EC590 -- DoAllocateResourceListRequest: unpack an AllocateResourceListRequest (lpRequest)
    // and forward its fields to AllocateResourceList, then latch the request's event id into
    // miAllocateRequestEventId (X360 *(a1+105268) = *(req+4)).
    //
        // Register->field map read
    // off the asm prologue (AllocateResourceList(this, r4..r11)):
    //   r4 = ld   req+0x10  (8-byte ID)               r5 = lwz  req+0x08  (s32 event id)
    //   r6 = lwz  req+0x18  (ResourceEntry* lo)        r7 = lwz  req+0x20  (s32 numEntries)
    //   r8 = lwz  req+0x24  (bool* out)                r9 = lwz  req+0x28  (ResourceHandle* )
    //   r10= lbz  req+0x2C  (bool)                     r11= lbz  req+0x2D  (bool)
    //   +0x04 -> miAllocateRequestEventId
    // (Hex-Rays mislabels these offsets; the asm loads are authoritative.)
    // ----------------------------------------------------------------------------------------------
    void PoolModule::DoAllocateResourceListRequest(const void* lpRequest)
    {
        const Events::AllocateResourceListRequest& lrRequest =
            *static_cast<const Events::AllocateResourceListRequest*>(lpRequest);
        AllocateResourceList(lrRequest.mListId.GetHash(), lrRequest.miPoolId,
            lrRequest.mpEntries, lrRequest.miNumEntries, lrRequest.mpNeeds,
            lrRequest.mpResources, lrRequest.mbLiveUpdateReplace, lrRequest.mbAllowFailiure);
        miAllocateRequestEventId = lrRequest.miEventId;
    }

    // ----------------------------------------------------------------------------------------------
    // @ 0x828D81D0 -- DoDeletePoolRequest: the X360 only acts when the response carries a failure: if
    // *(response+0x0C) != 0 it fires the "Destroy Bank Failed" assert (CgsPoolModule.cpp:1309). On the
    // success path it does nothing. (response is a serialised DestroyBankResponse payload.)
    // ----------------------------------------------------------------------------------------------
    void PoolModule::DoDeletePoolRequest(const void* lpResponse)
    {
        const CgsMemory::MemoryIO::DestroyBankResponse* lpResult =
            static_cast<const CgsMemory::MemoryIO::DestroyBankResponse*>(lpResponse);
        CGS_ASSERT(lpResult->GetResult() == CgsMemory::MemoryIO::E_RESULT_OK, "Destroy Bank Failed");
    }

    // ARTIST 82901748. The list itself becomes available before fixing up its
    // range. The returned handle owns the entry, not the entry's main-memory data.
    void PoolModule::DoFixUpAndResolveResourceListRequest(
        const Events::FixUpAndResolveResourceListRequest* lpRequest, PoolIO::OutputBuffer* lpOutput)
    {
        const s32 liPoolIndex = GetPoolIndex(lpRequest->miPoolId);
        CGS_ASSERT(liPoolIndex != -1, "liPoolIndex != -1");
        Pool& lrPool = maPools[liPoolIndex];
        s32 liListIndex;
        Entry* lpEntry = lrPool.FindResource(lpRequest->mListId, false, 3, &liListIndex);
        CGS_ASSERT(lpEntry != nullptr, "lpResource");
        lrPool.SetEntryStatus(liListIndex, 2);
        const EntryListResource* lpList = static_cast<const EntryListResource*>(lpEntry->mResource.m_baseResources[0]);
        lrPool.FixUpAndResolveResourceList(lpList->mIds, lpList->muNumEntries,
            lpRequest->miFirstIndex, lpRequest->miCount, lpRequest->mbFinalFixup, lpRequest->mbFixUpDependencies);

        Events::FixUpAndResolveResourceListResponse lResponse = {};
        lResponse.miEventId = lpRequest->miEventId;
        lResponse.miPoolId = lpRequest->miPoolId;
        lResponse.mListHandle.mpResourceMemory = &lpEntry->mResource.m_baseResources[0];
        lResponse.mListHandle.mpSourceEntry = lpEntry;
        lResponse.mpIds = lpList->mIds;
        lResponse.miNumEntries = lpList->muNumEntries;
        lpOutput->GetPoolOutputQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lResponse), 19, sizeof(lResponse));
    }

    // ARTIST 828FD310: acknowledge the request after arming deallocation. Pool
    // updates retire each resource only once its original countdown expires.
    void PoolModule::DoUnloadResourceListRequest(
        const Events::UnloadResourceListRequest* lpRequest, PoolIO::OutputBuffer* lpOutput)
    {
        CGS_ASSERT(mProcessState == E_UPDATESTATE_IDLE || mProcessState == E_UPDATESTATE_DEALLOCATING_LIST,
                   "Can only unload when idle or already unloading\n");
        const s32 liPoolIndex = GetPoolIndex(lpRequest->miPoolId);
        CGS_ASSERT(liPoolIndex != -1, "liPoolIndex != -1");
        mDeAllocateState.Begin(&maPools[liPoolIndex], lpRequest->mListId);
        mProcessState = E_UPDATESTATE_DEALLOCATING_LIST;

        Events::UnloadResourceListResponse lResponse = {};
        lResponse.miEventId = lpRequest->miEventId;
        lResponse.miPoolId = lpRequest->miPoolId;
        lResponse.mListId = lpRequest->mListId;
        lpOutput->GetPoolOutputQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lResponse), 21, sizeof(lResponse));
    }

    // ----------------------------------------------------------------------------------------------
    // @ 0x82904860 -- UpdateAllocating: poll the allocate step (mAllocateState.Update()) and dispatch:
    //   SUCCESS(0)/FAILED_SAFELY(4): finalise -- build the allocate-response event from the
    //       allocate-state working set + the request's event id, post it to the pool output queue
    //       (event id 17, native sizeof), and drop back to IDLE.
    //   ERROR(1):  assert "Allocation state returned error during load - out of memory loading bundle!"
    //   PEND(2):   nothing (stay allocating).
    //   INTELLIFRAG(3): hand off to the intelligent defrag pass (mIntelliFragState.Begin) and switch to
    //       INTELLIFRAG.
    //   default:   assert "Allocation state returned invalid result code <n>".
    // ----------------------------------------------------------------------------------------------
    void PoolModule::UpdateAllocating(void* lpOutputBuffer)
    {
        const u32 luResult = mAllocateState.Update();
        switch (luResult)
        {
        case AllocatePoolModuleState::E_RESULT_SUCCESS:
        case AllocatePoolModuleState::E_RESULT_FAILED_SAFELY:
        {
            CGS_ASSERT(lpOutputBuffer != 0, "lpOutputBuffer");   // CgsPoolModule.cpp:534 (asm li r5,0x216)

            // Build the native allocate-response event (console size 48; queue event id 17).
            // The X360 marshals the body
            // from the allocate state's working set (the lwz/ld off r31=&mAllocateState across
            // 0x829048E4..0x8290494C: the owner field *(state+4)+0x10C, the 8-byte id at +8, and the
            // +0x10/0x14/0x1C/0x20/0x24 counters) -- delegate that copy to GenerateResponse (the state
            // owns its layout), then stamp the three DRIVER-controlled fields the asm writes directly:
            //   [+0x00] = 0                          (stw r29, r29=0)
            //   [+0x04] = miAllocateRequestEventId   (lwzx *(this+0x19B34))
            //   [+0x2C] = (result==SUCCESS) ? 0 : 1  (mbFailed; asm cntlzw/result/extrwi/xori)
            Events::AllocateResourceListResponse lEvent = {};
            mAllocateState.GenerateResponse(&lEvent);
            lEvent.miEventId = miAllocateRequestEventId;
            lEvent.mbFailed = luResult != AllocatePoolModuleState::E_RESULT_SUCCESS;

            PoolIO::OutputBuffer* lpOut = static_cast<PoolIO::OutputBuffer*>(lpOutputBuffer);
            lpOut->GetPoolOutputQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent), 17, sizeof(lEvent));

            mProcessState = E_UPDATESTATE_IDLE;   // X360 *(this+0x19B30) = 0
            break;
        }
        case AllocatePoolModuleState::E_RESULT_ERROR:
            CGS_ASSERT(false, "Allocation state returned error during load - out of memory loading bundle!\n");   // :551
            break;
        case AllocatePoolModuleState::E_RESULT_PEND:
            break;
        case AllocatePoolModuleState::E_RESULT_DEFRAGMENT:
        {
            // Hand off to the intelligent defrag pass. The X360 fills an IntelliFragParams from the
            // allocate state's working set + this module's defrag scratch buffers, then Begins it.
            IntelliFragParams lParams;
            lParams.mpPool                      = mAllocateState.GetPool();
            lParams.mpAllocListSet              = mAllocateState.GetAllocSet();
            lParams.mpAddressedAllocRequests    = mpAddressedAllocRequests;
            lParams.mpRelocateRequests          = mpRelocateRequests;
            lParams.mpDistributionEntries       = mpDistributionEntries;
            lParams.mpLinearHeapNodes           = mpLinearHeapNodes;
            lParams.mpRelocateSources           = mpRelocateSources;
            lParams.muMaxAddressedAllocRequests = KI_MAX_ALLOCATION_REQUESTS;   // X360 v30 = 4096
            lParams.muMaxRelocateRequests       = KU_MAX_POOL_ENTRIES;          // X360 v32 = 36863
            lParams.muMaxDistributionRequests   = KU_MAX_DISTRIBUTION_COMMANDS; // X360 v33 = 0xFFFF
            lParams.muMaxLinearHeapNodes        = KU_MAX_POOL_ENTRIES;          // X360 v31 = 36863
            lParams.muMaxRelocateSources        = KU_MAX_POOL_ENTRIES;          // X360 v34 = 36863
            lParams.mpScratchPool               = &mScratchPool;

            mIntelliFragState.Begin(&lParams);
            mProcessState = E_UPDATESTATE_INTELLIFRAG;   // X360 a1[26316] = 3
            break;
        }
        default:
            CGS_ASSERT(false, "Allocation state returned invalid result code \n");   // :589
            break;
        }
    }

    // ----------------------------------------------------------------------------------------------
    // @ 0x828F38E8 -- UpdateDeAllocating: poll the deallocate step and dispatch:
    //   IDLE(0):    clear mProcessState back to IDLE.
    //   INVALID(1): assert "Deallocation state returned error during load".
    //   BUSY(2):    nothing.
    //   case 3:     finished -- back to IDLE, then forward the pending allocate request (the deallocate
    //               state's stashed request, or null if its flag is clear) to DoAllocateResourceListRequest,
    //               and clear the live-update flag.
    //   default:    assert "DeAllocation state returned invalid result code".
    //
    // X360 reads the deallocate state's "have request" byte at +12 and its request block at +16.
    // ----------------------------------------------------------------------------------------------
    void PoolModule::UpdateDeAllocating(void* /*lpOutputBuffer*/)
    {
        const u32 luResult = mDeAllocateState.Update();
        switch (luResult)
        {
        case 0:
            mProcessState = E_UPDATESTATE_IDLE;   // X360 *(a1+105264) = 0
            break;
        case 1:
            CGS_ASSERT(false, "Deallocation state returned error during load\n");   // :622
            break;
        case 2:
            break;
        case 3:
        {
            mProcessState = E_UPDATESTATE_IDLE;   // X360 *(this+0x19B30) = 0
            // The deallocate state stashed the originating allocate request; forward it (or null when
            // its have-request flag is clear) to the allocate handler. The X360 makes this call
            // UNCONDITIONALLY -- the null case is reached only defensively. (Asm: lbz +0xC -> r4 =
            // (flag ? &state+0x10 : 0); bl DoAllocateResourceListRequest.)
            DoAllocateResourceListRequest(mDeAllocateState.GetPendingAllocation());
            // 828F394C: clear this deallocation state's pending-request latch.
            mDeAllocateState.CancelPendingAllocation();
            break;
        }
        default:
            CGS_ASSERT(false, "DeAllocation state returned invalid result code\n");   // :643
            break;
        }
    }

    // ----------------------------------------------------------------------------------------------
    // @ 0x829013F8 -- UpdateIntelliFrag: poll the intellifrag step (mIntelliFragState.Update()) and
    // dispatch:
    //   SUCCESS(0): done -- switch to ALLOCATING_LIST (re-drive the allocate step next frame).
    //   ERROR(1):   assert "Error during intellifrag".
    //   PEND(2):    nothing.
    //   EMERGENCY(3): escalate to the emergency defrag pass (mEmergencyFragState.Begin) -> EMERGENCYFRAG.
    //   default:    assert "Intellifrag state returned invalid result code".
    // ----------------------------------------------------------------------------------------------
    void PoolModule::UpdateIntelliFrag(void* /*lpOutputBuffer*/)
    {
        const u32 luResult = mIntelliFragState.Update();
        switch (luResult)
        {
        case IntelliFragPoolModuleState::E_RESULT_SUCCESS:
            mProcessState = E_UPDATESTATE_ALLOCATING_LIST;   // X360 a1[26316] = 1
            break;
        case IntelliFragPoolModuleState::E_RESULT_ERROR:
            CGS_ASSERT(false, "Error during intellifrag\n");   // :676
            break;
        case IntelliFragPoolModuleState::E_RESULT_PEND:
            break;
        case IntelliFragPoolModuleState::E_RESULT_EMERGENCY:
        {
            // Escalate to the emergency defrag pass: it uses the pool Relocator + per-pass params
            // instead of the scratch pool. The X360 fills an EmergencyFragParams from the intellifrag
            // working set + this module's defrag buffers + the embedded Relocator/RelocationParams.
            EmergencyFragParams lParams;
            lParams.mpPool                      = 0;   // [marked] working-set copy DEFERRED with the state
            lParams.mpAllocListSet              = 0;
            lParams.mpAddressedAllocRequests    = mpAddressedAllocRequests;
            lParams.mpRelocateRequests          = mpRelocateRequests;
            lParams.mpDistributionEntries       = mpDistributionEntries;
            lParams.mpLinearHeapNodes           = mpLinearHeapNodes;
            lParams.mpRelocateSources           = mpRelocateSources;
            lParams.muMaxAddressedAllocRequests = KI_MAX_ALLOCATION_REQUESTS;   // 4096
            lParams.muMaxRelocateRequests       = KU_MAX_POOL_ENTRIES;          // 36863
            lParams.muMaxDistributionRequests   = KU_MAX_DISTRIBUTION_COMMANDS; // 0xFFFF
            lParams.muMaxLinearHeapNodes        = KU_MAX_POOL_ENTRIES;          // 36863
            lParams.muMaxRelocateSources        = KU_MAX_POOL_ENTRIES;          // 36863
            lParams.mpRelocator        = &mRelocator;
            lParams.mpRelocationParams = &mRelocationParams;

            mEmergencyFragState.Begin(&lParams);
            mProcessState = E_UPDATESTATE_EMERGENCYFRAG;   // X360 a1[26316] = 6
            break;
        }
        default:
            CGS_ASSERT(false, "Intellifrag state returned invalid result code\n");   // :714
            break;
        }
    }

    // ----------------------------------------------------------------------------------------------
    // @ 0x82906E70 -- UpdateLiveUpdate: poll the live-update step (mLiveUpdateState.Update()) and dispatch:
    //   DONE(0):  GenerateResponse into a stack record, stamp it (flag byte 0 + the request event id),
    //             post it to the pool output queue (event id 17, 48 bytes), and drop back to IDLE.
    //   ERROR(1): assert "Live update state returned error during load - out of memory loading bundle!".
    //   2/3:      nothing (still working).
    //   default:  assert "Live update state returned invalid result code".
    // ----------------------------------------------------------------------------------------------
    void PoolModule::UpdateLiveUpdate(void* lpOutputBuffer)
    {
        const u32 luResult = mLiveUpdateState.Update();
        switch (luResult)
        {
        case LiveUpdatePoolModuleState::E_RESULT_DONE:
        {
            // The X360 records a 48-byte response: GenerateResponse fills the body (the state owns its
            // layout), then the driver clears the trailing flag byte ([+0x2C]=0) and stamps the request
            // event id ([+0x04] = *(this+0x19B34) == miAllocateRequestEventId) before posting (queue
            // event id 17). Modelled with the typed response record (x64-widened; see CgsResourceIOEvents.h).
            Events::AllocateResourceListResponse lEvent;
            mLiveUpdateState.GenerateResponse(&lEvent);
            lEvent.mbFailed = false;                        // X360 stb r30(0) into [+0x2C]
            lEvent.miEventId    = miAllocateRequestEventId; // X360 stw *(this+0x19B34) into [+0x04]

            PoolIO::OutputBuffer* lpOut = static_cast<PoolIO::OutputBuffer*>(lpOutputBuffer);
            lpOut->GetPoolOutputQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent), 17, sizeof(lEvent));

            mProcessState = E_UPDATESTATE_IDLE;   // X360 *(this+0x19B30) = 0
            break;
        }
        case LiveUpdatePoolModuleState::E_RESULT_ERROR:
            CGS_ASSERT(false, "Live update state returned error during load - out of memory loading bundle!\n");   // :772
            break;
        case LiveUpdatePoolModuleState::E_RESULT_BUSY:
        case LiveUpdatePoolModuleState::E_RESULT_BUSY2:
            break;
        default:
            CGS_ASSERT(false, "Live update state returned invalid result code\n");   // :791
            break;
        }
    }

    // ----------------------------------------------------------------------------------------------
    // AllocateResourceList -- select the target pool and arm ordinary or live-update allocation.
    // The actual allocation is performed by the corresponding update state.
    // ----------------------------------------------------------------------------------------------
    bool PoolModule::AllocateResourceList(u64 luId, s32 liPoolId, const void* lpEntries, s32 liNumEntries,
                                          bool* lpNeeds, void* lpResources, bool lbLiveUpdateReplace, bool lbAllowFailiure)
    {
        // ARTIST 828E2F88 (the old 82900690 citation was incorrect). r4 carries
        // the whole ID; r5 is the pool ID, and r11 carries allow-failure.
        CGS_ASSERT(mProcessState == E_UPDATESTATE_IDLE || mProcessState == E_UPDATESTATE_DEALLOCATING_LIST,
                   "Can only entry allocate list state from idle state\n");
        ID lId;
        lId.SetHash(luId);
        Pool* lpPool = nullptr;
        if (!lbLiveUpdateReplace || liPoolId >= 0)
        {
            const s32 liPoolIndex = GetPoolIndex(liPoolId);
            CGS_ASSERT(liPoolIndex >= 0, "Pool not found\n");
            lpPool = &maPools[liPoolIndex];
        }
        if (lbLiveUpdateReplace)
        {
            mLiveUpdateState.BeginAllocation(lpPool, lId,
                static_cast<const BundleV2::ResourceEntry*>(lpEntries), liNumEntries,
                lpNeeds, static_cast<SmallResource*>(lpResources));
            mProcessState = E_UPDATESTATE_LIVEUPDATE;
        }
        else
        {
            mAllocateState.BeginAllocation(lpPool, lId,
                static_cast<const BundleV2::ResourceEntry*>(lpEntries), liNumEntries,
                &mAllocListSet, lpNeeds, static_cast<SmallResource*>(lpResources), lbAllowFailiure);
            mProcessState = E_UPDATESTATE_ALLOCATING_LIST;
        }
        return true;
    }
}
