#include "types.hpp"

#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsEmergencyFragPoolModuleState.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"  // Pool::BeginEmergencyDefragmentation / GetHeapAlignment, AllocListSet
#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"           // gpDebugPrint / gxMessageFilterFlags

// The emergency defragmentation strategy: Begin / RunDefragAlgorithm / RunPoolDefragmentation /
// Update. Members are referenced by name (offsets in the owning header). Base private state
// (mpPool) is reached through the base's public GetPool() / GetHeapAlignment accessor rather than
// a raw cross-object offset.

namespace CgsResource
{
    // -------- Begin --------
    // Run the base Begin first (it copies the working set + null-asserts the pointers), then assert
    // we are currently idle, arm the step machine and latch the Relocator plus its parameter block
    // for the pass. A filtered debug print announces it.
    void EmergencyFragPoolModuleState::Begin(EmergencyFragParams* lpParams)
    {
        BaseDefragPoolModuleState::Begin(lpParams);

        CGS_ASSERT(meState == E_STATE_IDLE, "Can not defrag unless in idle state\n");

        meState            = E_STATE_START_DEFRAGMENTING;
        miCountdown        = 2;                             // the arm-up countdown, a literal
        mpRelocator        = lpParams->mpRelocator;
        mpRelocationParams = lpParams->mpRelocationParams;

        if (CgsDev::Message::gxMessageFilterFlags & 1)
        {
            *CgsDev::Log::gpDebugPrint << "Running emergency defragment\n";   // (*gpDebugPrint)[1]("...")
        }
    }

    // -------- RunDefragAlgorithm --------
    // Pack every live block down toward the first free node. Find the first free node; if there is
    // none, bail (false). Otherwise walk the nodes after it (up to liLastNode): each USED node gets a
    // relocate request to the running destination offset, which then advances by that node's size.
    // Finally, for the addressed-alloc requests of memory pool liMemType, emit one
    // AddAddressedAllocRequest per request at the (alignment-rounded) running offset. Returns true.
    bool EmergencyFragPoolModuleState::RunDefragAlgorithm(AllocListSet* lpAllocListSet,
                                                          LinearHeapNode* lpNodes,
                                                          s32 liLastNode, s32 liMemType)
    {
        CGS_ASSERT(GetPool(), "GetPool()");

        s32 liFreeNode = FindNextFreeNode(0);
        if (liFreeNode == -1)
        {
            return false;
        }

        // Pack the USED nodes after the free node down into it.
        u32 luDestOffset = lpNodes[liFreeNode].muOffset;
        for (s32 liNode = liFreeNode + 1; liNode <= liLastNode; ++liNode)
        {
            LinearHeapNode& lNode = lpNodes[liNode];
            if (lNode.muStatus == LinearHeapNode::KU_STATUS_USED)
            {
                AddRelocateRequest(static_cast<u16>(liNode), luDestOffset);
                luDestOffset += lNode.muSize;
            }
        }

        // Stage the freed-up addressed allocations for this memory pool at the packed tail.
        u32          luNumRequests = lpAllocListSet->manAllocRequestCounts[liMemType];
        AllocRequest* lpRequest    = lpAllocListSet->mapAllocRequests[liMemType];
        u32          luAlignment   = GetPool()->GetHeapAlignment(liMemType);

        for (; luNumRequests; --luNumRequests, ++lpRequest)
        {
            u32 luAlignedSize = (lpRequest->muSize + luAlignment - 1) & ~(luAlignment - 1);
            AddAddressedAllocRequest(luAlignedSize, luDestOffset, lpRequest->mpOwner);
            luDestOffset += luAlignedSize;
        }

        return true;
    }

    // -------- RunPoolDefragmentation --------
    // Hand the staged relocation plan to the pool, executing it through the latched Relocator.
    void EmergencyFragPoolModuleState::RunPoolDefragmentation(RelocateRequest* lpRequests,
                                                              RelocateSource* lpSources,
                                                              u32 luNum, s32 liMemType)
    {
        GetPool()->BeginEmergencyDefragmentation(mpRelocator, mpRelocationParams,
                                                 lpRequests, lpSources, luNum, liMemType);
    }

    // -------- Update --------
    // Poll the emergency-defrag step machine. IDLE -> success. START_DEFRAGMENTING -> tick the
    // arm-up countdown, then wait while the pool is still defragmenting; once clear, scan the batch
    // alloc-list results for a memory type that still needs defragmenting and kick BeginDefragment
    // on it (advancing to DEFRAGMENTING_HEAP). DEFRAGMENTING_HEAP -> once the pool reports its defrag
    // mem-type idle (-1), re-arm and do the final allocations. Any other state is invalid.
    //
    // The console body reads the base-private alloc-result array directly; this reconstruction
    // reaches it through the attested base accessor GetAllocationResult(memType) (the same array
    // the IntelliFrag sibling reads), avoiding an unattested GetAllocListSet() accessor. The
    // console's repeated re-test of the same slot is behaviourally a single per-memtype test and
    // is expressed as one here.
    EmergencyFragPoolModuleState::EEmergencyFragResult EmergencyFragPoolModuleState::Update()
    {
        if (meState == E_STATE_IDLE)
        {
            return E_RESULT_SUCCESS;
        }

        if (meState == E_STATE_START_DEFRAGMENTING)
        {
            if (miCountdown > 0)   // still arming
            {
                --miCountdown;
                return E_RESULT_PEND;
            }

            if (GetPool()->GetNumEntriesInPurgatory() != 0)   // a pass may not start until purgatory is empty
            {
                return E_RESULT_PEND;
            }

            // First memory type whose batch alloc result still flags a defrag need.
            for (s32 liMemType = 0; liMemType < 3; ++liMemType)
            {
                if (GetAllocationResult(liMemType) == E_BATCHALLOCRESULT_FAIL_NEED_DEFRAG)
                {
                    meState = E_STATE_DEFRAGMENTING_HEAP;
                    // A refused BeginDefragment escalates; an accepted one just pends.
                    return BeginDefragment(liMemType) ? E_RESULT_PEND : E_RESULT_EMERGENCY;
                }
            }

            meState = E_STATE_IDLE;   // nothing to defrag
            return E_RESULT_SUCCESS;
        }

        if (meState == E_STATE_DEFRAGMENTING_HEAP)
        {
            if (GetPool()->GetDefragMemType() == -1)   // pool defrag done
            {
                meState = E_STATE_START_DEFRAGMENTING;
                DoFinalAllocations();
            }
            return E_RESULT_PEND;
        }

        CGS_ASSERT(false, "Defrag state is invalid\n");
        return E_RESULT_ERROR;
    }
}
