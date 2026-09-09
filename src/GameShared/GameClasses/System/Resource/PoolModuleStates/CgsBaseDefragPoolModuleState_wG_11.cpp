// ===========================================================================
// PARKED PARTFILE, not on the build list. The remaining out-of-line members of
// CgsResource::BaseDefragPoolModuleState: BeginDefragment, DoFinalAllocations and
// AddAddressedAllocRequest.
//
// Missing declarations (declared, defined nowhere in the tree):
//   Pool::BeginDefragmentation, Pool::BeginEmergencyDefragmentation,
//   Pool::GetHeapAlignment, Pool::IsDefragmenting, Pool::GenerateLinearHeap,
//   Pool::ExecuteBatchAllocation, Pool::ExecuteBatchAddressedAllocation,
//   Heap::GenerateLinearHeap, Heap::ExecuteBatchAllocation,
//   Heap::ExecuteBatchAddressedAllocation, Heap::ExecuteBatchRelocation
// ===========================================================================

#include "types.hpp"

#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsBaseDefragPoolModuleState.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"   // Pool, AllocListSet, Entry
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"   // Type::GetCachedCanDefrag
#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"            // gpDebugPrint / gxMessageFilterFlags

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
