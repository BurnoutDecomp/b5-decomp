#include "types.hpp"

#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsBaseDefragPoolModuleState.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"   // Pool, AllocListSet (GetAllocationResult)
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// includes folded in from the CgsBaseDefragPoolModuleState_w*.cpp partfiles (2026-09-15)
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"   // Type::GetCachedCanDefrag
#include "GameShared/GameClasses/Development/Log/CgsLog.h"            // gpDebugPrint / gxMessageFilterFlags

// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// THIS TU (GameShared/.../PoolModuleStates/CgsBaseDefragPoolModuleState.cpp) -- the pure-logic
// spine functions of the shared defrag base state:
//   CgsResource::BaseDefragPoolModuleState::Begin                   @ 0x828DA090
//   CgsResource::BaseDefragPoolModuleState::FindNextFreeNode        @ 0x828DA248
//   CgsResource::BaseDefragPoolModuleState::BuildFinalRelocationData@ 0x828DA2E8
//   CgsResource::BaseDefragPoolModuleState::AddRelocateRequest      @ 0x828D8010
// Each is bodied store-for-store against the X360 asm; members are referenced by name
// (offsets verified in the owning header). The X360 asserts that stream a formatted message
// into the debug buffer (BasePriorityQueue::Clear + StrStream machinery) are expressed through
// CGS_ASSERT, which forwards the message via the same Begin/Fire/End assert sequence.

namespace CgsResource
{
    // -------- Begin @ 0x828DA090 --------
    // Reset the current mem-type to -1, then copy the caller's BaseDefragParams working set
    // into our members (mpPool .. muMaxRelocateSources -- 12 fields, in declaration order),
    // asserting that each of the seven pointers the algorithm needs is non-null.
    void BaseDefragPoolModuleState::Begin(BaseDefragParams* lpParams)
    {
        miCurrentMemType = -1;

        mpPool                      = lpParams->mpPool;                      // result[3] = *a2
        mpAllocListSet              = lpParams->mpAllocListSet;              // result[4] = a2[1]
        mpAddressedAllocRequests    = lpParams->mpAddressedAllocRequests;   // result[5] = a2[2]
        mpRelocateRequests          = lpParams->mpRelocateRequests;         // result[6] = a2[3]
        mpDistributionEntries       = lpParams->mpDistributionEntries;      // result[7] = a2[4]
        mpLinearHeapNodes           = lpParams->mpLinearHeapNodes;          // result[8] = a2[5]
        mpRelocateSources           = lpParams->mpRelocateSources;          // result[9] = a2[6]
        muMaxAddressedAllocRequests = lpParams->muMaxAddressedAllocRequests;// result[10] = a2[7]
        muMaxRelocateRequests       = lpParams->muMaxRelocateRequests;      // result[11] = a2[8]
        muMaxDistributionRequests   = lpParams->muMaxDistributionRequests;  // result[12] = a2[9]
        muMaxLinearHeapNodes        = lpParams->muMaxLinearHeapNodes;       // result[13] = a2[10]
        muMaxRelocateSources        = lpParams->muMaxRelocateSources;       // result[14] = a2[11]

        CGS_ASSERT(mpPool,                   "mpPool");                    // :87
        CGS_ASSERT(mpAllocListSet,           "mpAllocListSet");            // :88
        CGS_ASSERT(mpAddressedAllocRequests, "mpAddressedAllocRequests");  // :89
        CGS_ASSERT(mpRelocateRequests,       "mpRelocateRequests");        // :90
        CGS_ASSERT(mpDistributionEntries,    "mpDistributionEntries");     // :91
        CGS_ASSERT(mpLinearHeapNodes,        "mpLinearHeapNodes");         // :92
        CGS_ASSERT(mpRelocateSources,        "mpRelocateSources");         // :93
    }

    // -------- FindNextFreeNode @ 0x828DA248 --------
    // From liStartNode, scan the flattened linear-heap node list for the next FREE node
    // (muStatus == KU_STATUS_FREE, i.e. node+8 == 0). Returns its index, or -1 if the scan
    // reaches the end of the list (muNumLinearHeapNodes entries). The X360 walks the list with
    // a raw 16-byte stride from &node.muStatus; we index by node.
    s32 BaseDefragPoolModuleState::FindNextFreeNode(s32 liStartNode)
    {
        CGS_ASSERT(mpLinearHeapNodes, "mpLinearHeapNodes");   // :307

        s32 liNumNodes = muNumLinearHeapNodes;                // v4 = *(a1+68) (lhz @ +0x44)
        s32 liNode = liStartNode;                             // result = a2
        if (liNode >= liNumNodes)
        {
            return -1;
        }

        // i = &mpLinearHeapNodes[liStartNode].muStatus; advance one node per step.
        for (; mpLinearHeapNodes[liNode].muStatus != LinearHeapNode::KU_STATUS_FREE; )
        {
            if (++liNode >= liNumNodes)
            {
                return -1;
            }
        }
        return liNode;
    }

    // -------- BuildFinalRelocationData @ 0x828DA2E8 --------
    // Walk the muRelocationCount relocate requests; for each, look up the linear-heap node it
    // names, copy that node's (offset,size,owner) into the parallel relocate-source entry and
    // rewrite the request's node field to the node's destination node. Returns the total number
    // of bytes being relocated. Asserts guard the three required arrays, the relocate-source
    // capacity, the node index range, and that each named node is actually USED.
    s32 BaseDefragPoolModuleState::BuildFinalRelocationData()
    {
        CGS_ASSERT(mpRelocateRequests, "mpRelocateRequests");   // :337
        CGS_ASSERT(mpRelocateSources,  "mpRelocateSources");    // :338
        CGS_ASSERT(mpLinearHeapNodes,  "mpLinearHeapNodes");    // :339
        CGS_ASSERT(muRelocationCount < muMaxRelocateSources,
                   "Too many relocation entries to fill out relocate sources array\n");  // :340

        s32 liTotalSize = 0;                       // v4
        RelocateRequest* lpRequest = mpRelocateRequests;   // v5
        RelocateSource*  lpSource  = mpRelocateSources;    // v7 = mpRelocateSources (+4 -> .muSize)

        for (u32 luIndex = 0; luIndex < muRelocationCount; ++luIndex)
        {
            u32 luNode = lpRequest->muNode;        // v8 = *v5 (u16 node index)
            CGS_ASSERT(luNode < muNumLinearHeapNodes, "Linear node out of range\n");   // :357

            const LinearHeapNode& lNode = mpLinearHeapNodes[luNode];   // 16 * v8 + mpLinearHeapNodes
            CGS_ASSERT(lNode.muStatus == LinearHeapNode::KU_STATUS_USED,
                       "Can't relocate unused node\n");                // :358

            lpRequest->muNode      = lNode.muNode;     // *v5 = node+10
            lpSource->mpOwner      = lNode.mpOwner;    // v7[1] = node+12
            lpSource->muSize       = lNode.muSize;     // *v7  = node+4
            liTotalSize           += lNode.muSize;     // v4 += v14
            lpSource->muSourceOffset = lNode.muOffset; // *(v7-1) = node+0

            ++lpRequest;   // v5 += 4 words == one RelocateRequest (8 bytes)
            ++lpSource;    // v7 += 3 words == one RelocateSource (12 bytes)
        }

        return liTotalSize;
    }

    // -------- AddRelocateRequest @ 0x828D8010 --------
    // Append one relocate request (a linear-heap node index + its destination offset) to the
    // mpRelocateRequests array, returning the index it was written at and advancing the count.
    // Asserts the array is not already full (muRelocationCount < muMaxRelocateRequests). The X360
    // walks to the slot with an 8-byte stride (slwi r11,r11,3) from mpRelocateRequests; we index
    // by element. The u16 node is stored at +0 (sthx) and the u32 dest offset at +4 (stw).
    u32 BaseDefragPoolModuleState::AddRelocateRequest(u16 luNode, u32 luDestOffset)
    {
        CGS_ASSERT(muRelocationCount < muMaxRelocateRequests, "Out of relocation space\n");  // :205

        RelocateRequest& lRequest = mpRelocateRequests[muRelocationCount];   // 8 * count + mpRelocateRequests
        lRequest.muNode       = luNode;        // sthx r27, ... (u16 @ +0)
        lRequest.muDestOffset = luDestOffset;  // stw  r26, 4   (u32 @ +4)

        u32 luIndex = muRelocationCount;       // result = *(a1+0x40)
        muRelocationCount = luIndex + 1;       // *(a1+0x40) = result + 1
        return luIndex;
    }

    // -------- trivial accessors --------
    // Header-inline on the console; given out-of-line homes here.
    Pool* BaseDefragPoolModuleState::GetPool()
    {
        return mpPool;
    }

    void BaseDefragPoolModuleState::SetMaxToMove(s32 liMaxToMove)
    {
        miMaxToMove = liMaxToMove;
    }

    EBatchAllocResult BaseDefragPoolModuleState::GetAllocationResult(s32 liIndex)
    {
        CGS_ASSERT(mpAllocListSet, "mpAllocListSet");
        return mpAllocListSet->maeAllocRequestResults[liIndex];
    }

    // FLAG (link stub): the base RunDefragAlgorithm / RunPoolDefragmentation are declared
    // virtual but have no X360 base body -- the IntelliFrag / EmergencyFrag subclass
    // overrides carry the real strategy. Stubbed so the base vtable links; the base
    // implementations are never selected at runtime (the concrete states always override).
    bool BaseDefragPoolModuleState::RunDefragAlgorithm(AllocListSet*, LinearHeapNode*, s32, s32)
    {
        return false;
    }

    void BaseDefragPoolModuleState::RunPoolDefragmentation(RelocateRequest*, RelocateSource*, u32, s32)
    {
    }
}

// ============================================================================
// FOLDED FROM CgsBaseDefragPoolModuleState_wG_11.cpp (wave G) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ===========================================================================
// Partfile: the remaining out-of-line members of CgsResource::BaseDefragPoolModuleState
// -- BeginDefragment, DoFinalAllocations and AddAddressedAllocRequest.
// ===========================================================================


namespace CgsResource
{
    // -------- AddAddressedAllocRequest --------
    // Append one addressed allocation request at the running count and return the old count,
    // mirroring the AddRelocateRequest sibling.
    // FLAG: this body is modelled from its two call sites and the sibling's shape, not from a
    // console body; the over-capacity assert wording is unknown, so the guard refuses to write
    // past muMaxAddressedAllocRequests instead of firing an invented assert.
    u32 BaseDefragPoolModuleState::AddAddressedAllocRequest(u32 luSize, u32 luOffset, void* lpOwner)
    {
        CGS_ASSERT(mpAddressedAllocRequests, "mpAddressedAllocRequests");

        if (muAddressedAllocCount >= muMaxAddressedAllocRequests)
        {
            return muAddressedAllocCount;   // FLAG: console assert wording unknown -- refuse instead
        }

        AllocRequestAddressed& lRequest = mpAddressedAllocRequests[muAddressedAllocCount];
        lRequest.muSize   = luSize;
        lRequest.muOffset = luOffset;
        lRequest.mpOwner  = lpOwner;

        const u32 luIndex = muAddressedAllocCount;
        muAddressedAllocCount = luIndex + 1;
        return luIndex;
    }

    // -------- BeginDefragment --------
    // Kick off a defragmentation pass for one memory type: flatten the heap into the linear-node
    // list, validate the used nodes, let the concrete strategy build the plan, then refuse
    // (false) if the plan costs more than miMaxToMove -- that refusal escalates IntelliFrag to
    // EmergencyFrag.
    bool BaseDefragPoolModuleState::BeginDefragment(s32 liMemType)
    {
        CGS_ASSERT(mpPool,                   "mpPool");
        CGS_ASSERT(mpAllocListSet,           "mpAllocListSet");
        CGS_ASSERT(mpAddressedAllocRequests, "mpAddressedAllocRequests");
        CGS_ASSERT(mpRelocateRequests,       "mpRelocateRequests");
        CGS_ASSERT(mpDistributionEntries,    "mpDistributionEntries");
        CGS_ASSERT(mpLinearHeapNodes,        "mpLinearHeapNodes");
        CGS_ASSERT(mpRelocateSources,        "mpRelocateSources");

        miCurrentMemType = liMemType;

        if (!mpPool->GetAllowDefragmentation())   // pool +0x1C4 -- defrag not enabled for this pool
        {
            return true;
        }

        muAddressedAllocCount = 0;
        muRelocationCount     = 0;
        muNumLinearHeapNodes  = mpPool->GenerateLinearHeap(miCurrentMemType, mpLinearHeapNodes,
                                                           static_cast<u16>(muMaxLinearHeapNodes));
        if (muNumLinearHeapNodes <= 1)
        {
            return true;   // a single node is either an empty or an unfragmented heap
        }

        // Validate the live blocks the plan is allowed to move.
        for (u32 luNode = 0; luNode < muNumLinearHeapNodes; ++luNode)
        {
            const LinearHeapNode& lNode = mpLinearHeapNodes[luNode];
            if (lNode.muStatus != LinearHeapNode::KU_STATUS_USED)
            {
                continue;
            }

            // The heap node's owner word is the pool slot index, stored as an opaque owner by
            // Pool::AllocateMemoryForResource.
            const s32 liEntryIndex = static_cast<s32>(reinterpret_cast<uintptr_t>(lNode.mpOwner));
            Entry* lpEntry = mpPool->GetResource(liEntryIndex, false, 2);
            // Both messages here are streamed on the console (the entry index and the node
            // number are formatted into them); the fixed segments are used verbatim.
            CGS_ASSERT(lpEntry, "Entry index  for node  not found\n");
            // FLAG (PC guard): the console dereferences the entry unconditionally after the
            // assert above. A null entry on a non-fatal assert build would fault here, so the
            // second check is skipped instead.
            if (lpEntry != 0)
            {
                CGS_ASSERT(lpEntry->mpResourceType != 0 &&
                           lpEntry->mpResourceType->GetCachedCanDefrag(),
                           "Entry  can not be defragmented\n");
            }
        }

        CGS_ASSERT(mpAllocListSet->manAllocRequestCounts[miCurrentMemType] <= muMaxAddressedAllocRequests,
                   "Too many allocation requests\n");

        // Virtual: the concrete strategy builds the relocate/addressed-alloc plan.
        if (!RunDefragAlgorithm(mpAllocListSet, mpLinearHeapNodes,
                                static_cast<s32>(muNumLinearHeapNodes), miCurrentMemType))
        {
            return true;
        }

        if (BuildFinalRelocationData() > miMaxToMove)
        {
            return false;   // too much data to shift in one pass -- caller escalates
        }

        if (muRelocationCount == 0)
        {
            if (CgsDev::Message::gxMessageFilterFlags & 1)
            {
                *CgsDev::Log::gpDebugPrint
                    << "No relocations required so not doing defragment of memory type "
                    << miCurrentMemType << "\n";
            }
            return true;
        }

        if (CgsDev::Message::gxMessageFilterFlags & 1)
        {
            *CgsDev::Log::gpDebugPrint << "Begin defragmentation of memory type " << miCurrentMemType
                                       << " with relocation count of "
                                       << static_cast<s32>(muRelocationCount) << "\n";
        }

        // Virtual: the concrete strategy executes the plan (ScratchPool or Relocator).
        RunPoolDefragmentation(mpRelocateRequests, mpRelocateSources, muRelocationCount, miCurrentMemType);
        return true;
    }

    // -------- DoFinalAllocations --------
    // Re-run the batch allocation for the current memory type on the compacted heap (at the
    // planned offsets if the pass staged addressed requests) and record the result back into
    // the caller's AllocListSet.
    bool BaseDefragPoolModuleState::DoFinalAllocations()
    {
        AllocListSet& lSet = *mpAllocListSet;

        if (muAddressedAllocCount != 0)
        {
            lSet.maeAllocRequestResults[miCurrentMemType] =
                mpPool->ExecuteBatchAddressedAllocation(miCurrentMemType,
                                                        mpAddressedAllocRequests,
                                                        lSet.mapAllocResults[miCurrentMemType],
                                                        lSet.manAllocRequestCounts[miCurrentMemType],
                                                        false);
        }
        else
        {
            lSet.maeAllocRequestResults[miCurrentMemType] =
                mpPool->ExecuteBatchAllocation(mpAllocListSet, miCurrentMemType);
        }

        // The console streams the memory type into the message; the fixed prefix is used here.
        CGS_ASSERT(lSet.maeAllocRequestResults[miCurrentMemType] == E_BATCHALLOCRESULT_SUCCESS,
                   "Failed to allocate even after defragmentation (MemType=");

        return true;
    }
}
