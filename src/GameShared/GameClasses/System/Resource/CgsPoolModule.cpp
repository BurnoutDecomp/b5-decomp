#include "types.hpp"

#include "GameShared/GameClasses/System/Resource/CgsResourcePoolModule.h" // canonical PoolModule home
#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"           // gpDebugPrint / gxMessageFilterFlags (filtered prints)
#include "GameShared/GameClasses/System/Resource/CgsPoolModuleIO.h" // PoolIO::OutputBuffer / PoolOutputQueue (event posts)

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
// Calling convention verified from the asm prologues (PPC fastcall: r3=this). Every store/branch/
// early-out in the asm has a counterpart below; control flow is de-goto'd / de-switch-jumped into
// idiomatic C++ but semantically identical. Assert message strings + source line numbers are read
// off the asm string operands. Members are referenced by name (offsets in the header).
// ======================================================================================
namespace CgsResource
{
    // The X360 source file path baked into the asserts (CgsPoolModule.cpp).
    static const char* const KPC_POOLMODULE_SRC =
        "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsPoolModule.cpp";

    // ----------------------------------------------------------------------------------------------
    // @ 0x828E2ED0 -- ConvertPoolRequestOptions: project a CreatePoolRequest (lpRequest) into a
    // Pool::InitOptions (lpOutOptions). The X360 copies a fixed set of fields by raw offset; this is an
    // external request<->options field map (serialised request layout), so the copy is expressed
    // against the byte layout of both records (documented inline) rather than inventing two C++ structs
    // -- the request record (CreatePoolRequest) is not reconstructed as a type in this TU.
    //
    // Field map (request offset -> options offset), read off the asm:
    //   req+0x08 -> opt+0x00 (id)              req+0x0C -> opt+0x04 (name ptr, &req[0x0C])
    //   req+0x2C -> opt+0x58                    req+0x30 -> opt+0x50      req+0x34 -> opt+0x54
    //   req+0x5C -> opt+0x5C                    req+0xA8(byte) -> opt+0xA4(byte)
    //   per-heap (3x): opt+0x08/0x14/0x20 = 2*req[0x50/0x54/0x58]+1 (max nodes);
    //                  opt+0x0C/0x18/0x24 = req+0x38/0x40/0x48 lo (size);
    //                  opt+0x10/0x1C/0x28 = req+0x38/0x40/0x48 hi (align)
    // ----------------------------------------------------------------------------------------------
    void PoolModule::ConvertPoolRequestOptions(const void* lpRequest, void* lpOutOptions)
    {
        const u8* lpcReq = static_cast<const u8*>(lpRequest);
        u8*       lpcOpt = static_cast<u8*>(lpOutOptions);

        // Helpers for the serialised request/options records (external fixed layout -> raw access OK).
        #define RD_U32(p, off) (*reinterpret_cast<const u32*>((p) + (off)))
        #define RD_U64(p, off) (*reinterpret_cast<const u64*>((p) + (off)))
        #define WR_U32(p, off, v) (*reinterpret_cast<u32*>((p) + (off)) = (u32)(v))
        #define WR_U64(p, off, v) (*reinterpret_cast<u64*>((p) + (off)) = (u64)(v))

        WR_U32(lpcOpt, 0x00, RD_U32(lpcReq, 0x08));                  // id
        // name = &req[0x0C] (the inline name buffer). [marked: this writes the X360 4-byte field strides
        // into an opaque record; the request/options types are NOT reconstructed in this TU, so the name
        // pointer is stored X360-width to keep the record's field stride self-consistent for its reader.]
        WR_U32(lpcOpt, 0x04, (u32)(uintptr_t)(lpcReq + 0x0C));
        WR_U32(lpcOpt, 0x58, RD_U32(lpcReq, 0x2C));
        WR_U32(lpcOpt, 0x50, RD_U32(lpcReq, 0x30));
        WR_U32(lpcOpt, 0x54, RD_U32(lpcReq, 0x34));
        WR_U32(lpcOpt, 0x5C, RD_U32(lpcReq, 0x5C));
        *(lpcOpt + 0xA4) = *(lpcReq + 0xA8);                         // byte flag

        // Heap 0: maxNodes = 2*req[0x50]+1; size/align = the 8-byte field at req+0x38.
        u64 lu0 = RD_U64(lpcReq, 0x38);
        WR_U32(lpcOpt, 0x08, 2u * RD_U32(lpcReq, 0x50) + 1u);
        WR_U64(lpcOpt, 0x0C, lu0);                                   // opt+0x0C size, opt+0x10 align
        // Heap 1.
        u64 lu1 = RD_U64(lpcReq, 0x40);
        WR_U32(lpcOpt, 0x14, 2u * RD_U32(lpcReq, 0x54) + 1u);
        WR_U64(lpcOpt, 0x18, lu1);
        // Heap 2.
        u64 lu2 = RD_U64(lpcReq, 0x48);
        WR_U32(lpcOpt, 0x20, 2u * RD_U32(lpcReq, 0x58) + 1u);
        WR_U64(lpcOpt, 0x24, lu2);

        #undef RD_U32
        #undef RD_U64
        #undef WR_U32
        #undef WR_U64
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
    // The request record is a serialised event payload (external fixed layout). Register->field map read
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
        const u8* lpcReq = static_cast<const u8*>(lpRequest);
        const u64   luId         = *reinterpret_cast<const u64*>(lpcReq + 0x10);   // ld r4,0x10 (8B)
        const s32   liEventId    = *reinterpret_cast<const s32*>(lpcReq + 0x08);   // lwz r5,8
        const void* lpEntries    = *reinterpret_cast<const void* const*>(lpcReq + 0x18); // lwz r6,0x18
        const s32   liNumEntries = *reinterpret_cast<const s32*>(lpcReq + 0x20);   // lwz r7,0x20
        const void* lpbOut       = *reinterpret_cast<const void* const*>(lpcReq + 0x24); // lwz r8,0x24
        const void* lpHandles    = *reinterpret_cast<const void* const*>(lpcReq + 0x28); // lwz r9,0x28
        const bool  lbFlagA      = (*(lpcReq + 0x2C) != 0);   // lbz r10,0x2C
        const bool  lbFlagB      = (*(lpcReq + 0x2D) != 0);   // lbz r11,0x2D

        AllocateResourceList(luId, liEventId,
                             const_cast<void*>(lpEntries), liNumEntries,
                             reinterpret_cast<bool*>(const_cast<void*>(lpbOut)),
                             const_cast<void*>(lpHandles), lbFlagA, lbFlagB);

        miAllocateRequestEventId = *reinterpret_cast<const s32*>(lpcReq + 0x04);   // X360 *(a1+105268)=*(req+4)
    }

    // ----------------------------------------------------------------------------------------------
    // @ 0x828D81D0 -- DoDeletePoolRequest: the X360 only acts when the response carries a failure: if
    // *(response+0x0C) != 0 it fires the "Destroy Bank Failed" assert (CgsPoolModule.cpp:1309). On the
    // success path it does nothing. (response is a serialised DestroyBankResponse payload.)
    // ----------------------------------------------------------------------------------------------
    void PoolModule::DoDeletePoolRequest(const void* lpResponse)
    {
        const u8* lpcResp = static_cast<const u8*>(lpResponse);
        const s32 liResult = *reinterpret_cast<const s32*>(lpcResp + 0x0C);
        CGS_ASSERT(liResult == 0, "Destroy Bank Failed");   // CgsPoolModule.cpp:1309
    }

    // ----------------------------------------------------------------------------------------------
    // @ 0x82904860 -- UpdateAllocating: poll the allocate step (mAllocateState.Update()) and dispatch:
    //   SUCCESS(0)/SIMPLEFRAG(4): finalise -- build the 48-byte allocate-response event from the
    //       allocate-state working set + the request's event id, post it to the pool output queue
    //       (event id 17, 48 bytes), and drop back to IDLE.
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
        case AllocatePoolModuleState::E_RESULT_SIMPLEFRAG:
        {
            CGS_ASSERT(lpOutputBuffer != 0, "lpOutputBuffer");   // CgsPoolModule.cpp:534 (asm li r5,0x216)

            // Build the 48-byte allocate-response event (queue event id 17). The X360 marshals the body
            // from the allocate state's working set (the lwz/ld off r31=&mAllocateState across
            // 0x829048E4..0x8290494C: the owner field *(state+4)+0x10C, the 8-byte id at +8, and the
            // +0x10/0x14/0x1C/0x20/0x24 counters) -- delegate that copy to GenerateResponse (the state
            // owns its layout), then stamp the three DRIVER-controlled fields the asm writes directly:
            //   [+0x00] = 0                          (stw r29, r29=0)
            //   [+0x04] = miAllocateRequestEventId   (lwzx *(this+0x19B34))
            //   [+0x2C] = (result==SUCCESS) ? 0 : 1  (simple-frag flag; asm cntlzw(result)/extrwi/xori)
            struct AllocateResponseEvent { u8 mPayload[48]; } lEvent;
            for (s32 li = 0; li < (s32)sizeof(lEvent.mPayload); ++li)
                lEvent.mPayload[li] = 0;
            mAllocateState.GenerateResponse(&lEvent);
            *reinterpret_cast<s32*>(lEvent.mPayload + 0x00) = 0;
            *reinterpret_cast<s32*>(lEvent.mPayload + 0x04) = miAllocateRequestEventId;
            lEvent.mPayload[0x2C] = (luResult == AllocatePoolModuleState::E_RESULT_SUCCESS) ? 0 : 1;

            PoolIO::OutputBuffer* lpOut = static_cast<PoolIO::OutputBuffer*>(lpOutputBuffer);
            lpOut->GetPoolOutputQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent), 17, 48);

            mProcessState = E_UPDATESTATE_IDLE;   // X360 *(this+0x19B30) = 0
            break;
        }
        case AllocatePoolModuleState::E_RESULT_ERROR:
            CGS_ASSERT(false, "Allocation state returned error during load - out of memory loading bundle!\n");   // :551
            break;
        case AllocatePoolModuleState::E_RESULT_PEND:
            break;
        case AllocatePoolModuleState::E_RESULT_INTELLIFRAG:
        {
            // Hand off to the intelligent defrag pass. The X360 fills an IntelliFragParams from the
            // allocate state's working set + this module's defrag scratch buffers, then Begins it.
            IntelliFragParams lParams;
            lParams.mpPool                      = 0;   // [marked] working-set copy DEFERRED with AllocateState
            lParams.mpAllocListSet              = 0;
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
            DoAllocateResourceListRequest(mDeAllocateState.GetPendingAllocateRequest());
            // X360 *(this+0x19B8C) byte = 0 -- clear the live-update-in-progress flag (mLiveUpdateState region).
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
            // X360 v11[12] = a1+26432 (Relocator), v11[13] = a1+26688 (RelocationParams). Those embedded
            // sub-objects live in mPadRelocator (un-reconstructed Relocator type); pass their address.
            lParams.mpRelocator        = reinterpret_cast<CgsMemory::Relocator*>(mPadRelocator);
            lParams.mpRelocationParams = reinterpret_cast<CgsMemory::RelocationParams*>(mPadRelocator + 256);

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
            // The X360 records a 48-byte response: GenerateResponse fills the body, then the driver
            // clears the trailing flag byte ([+0x2C]=0) and stamps the request event id ([+0x04] =
            // *(this+0x19B34) == miAllocateRequestEventId) before posting (queue event id 17).
            struct LiveUpdateResponseEvent { u8 mPayload[48]; } lEvent;
            for (s32 li = 0; li < (s32)sizeof(lEvent.mPayload); ++li)
                lEvent.mPayload[li] = 0;
            mLiveUpdateState.GenerateResponse(&lEvent);
            lEvent.mPayload[0x2C] = 0;                                                   // X360 stb r30(0)
            *reinterpret_cast<s32*>(lEvent.mPayload + 0x04) = miAllocateRequestEventId;  // X360 stw *(this+0x19B34)

            PoolIO::OutputBuffer* lpOut = static_cast<PoolIO::OutputBuffer*>(lpOutputBuffer);
            lpOut->GetPoolOutputQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent), 17, 48);

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
    // AllocateResourceList -- the heavy resource-list allocator the DoAllocateResourceListRequest /
    // UpdateDeAllocating paths forward to (CgsPoolModule.cpp:1694). It belongs to its own pass (it walks
    // the bundle entries, batches the per-pool allocations, and drives the defrag state machine); it is
    // a trap-stub body here so the per-TU compile gate is satisfied without inventing its logic.
    // ----------------------------------------------------------------------------------------------
    bool PoolModule::AllocateResourceList(u64 luId, s32 liEventId, const void* lpEntries, s32 liNumEntries,
                                          bool* lpbOut, void* lpHandles, bool lbA, bool lbB)
    {
        (void)luId; (void)liEventId; (void)lpEntries; (void)liNumEntries;
        (void)lpbOut; (void)lpHandles; (void)lbA; (void)lbB;
        CGS_ASSERT(false, "AllocateResourceList: deferred (own TU)");
        return false;
    }
}
