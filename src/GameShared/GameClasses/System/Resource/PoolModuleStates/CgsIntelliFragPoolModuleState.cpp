#include "types.hpp"

#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsIntelliFragPoolModuleState.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"  // Pool::BeginDefragmentation / GetHeapAlignment, AllocListSet
#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"           // gpDebugPrint / gxMessageFilterFlags

// The intelligent (linear-merge) defragmentation strategy: Begin, RunDefragAlgorithm,
// RunPoolDefragmentation and Update. Base private state (mpPool / miMaxToMove) is reached through
// the base's public GetPool()/GetHeapAlignment/SetMaxToMove accessors rather than raw offsets.

namespace CgsResource
{
    // -------- Begin --------
    // Run the base Begin first (working-set copy + null asserts), assert we are idle, then arm the
    // step machine: cap a single pass at 2MB (the base's miMaxToMove, written at +0x48), set
    // meState = START_DEFRAGMENTING (+0x4C) and latch the ScratchPool from the params (+0x50). A
    // filtered debug print announces the pass.
    void IntelliFragPoolModuleState::Begin(IntelliFragParams* lpParams)
    {
        BaseDefragPoolModuleState::Begin(lpParams);

        CGS_ASSERT(meState == E_STATE_IDLE, "Can not defrag unless in idle state\n");

        meState       = E_STATE_START_DEFRAGMENTING;
        SetMaxToMove(0x200000);                  // cap one pass at 2MB (the base's miMaxToMove)
        mpScratchPool = lpParams->mpScratchPool;

        if (CgsDev::Message::gxMessageFilterFlags & 1)
        {
            *CgsDev::Log::gpDebugPrint << "Running intelligent defragment\n";
        }
    }

    // -------- RunDefragAlgorithm --------
    // The linear-merge planner. Find the first free node; if there is none, bail (false). Otherwise
    // walk the addressed-alloc requests of memory pool liMemType. The running free region starts as
    // the first free node's (offset,size). For each request, round its size up to the heap alignment:
    //   - if the free region is large enough, stage an addressed allocation at the running offset and
    //     shrink the free region by the aligned size;
    //   - otherwise merge the NEXT contiguous USED node(s) down (a relocate request each, advancing
    //     the running offset and growing the free region by the merged node's size) until the region
    //     is large enough. An assert tripwire fires if the merge walks past the last node.
    // Returns true.
    bool IntelliFragPoolModuleState::RunDefragAlgorithm(AllocListSet* lpAllocListSet,
                                                        LinearHeapNode* lpNodes,
                                                        s32 liLastNode, s32 liMemType)
    {
        if (CgsDev::Message::gxMessageFilterFlags & 1)
        {
            *CgsDev::Log::gpDebugPrint << "Running linear merge defrag algorithm\n";
        }

        s32 liFreeNode = FindNextFreeNode(0);
        if (liFreeNode == -1)
        {
            return false;
        }

        s32           liNode        = liFreeNode;
        u32           luNumRequests = lpAllocListSet->manAllocRequestCounts[liMemType];
        AllocRequest* lpRequest     = lpAllocListSet->mapAllocRequests[liMemType];
        u32           luAlignment   = GetPool()->GetHeapAlignment(liMemType);

        u32 luDestOffset  = lpNodes[liFreeNode].muOffset;
        u32 luFreeSize    = lpNodes[liFreeNode].muSize;
        u32 luAlignMask   = ~(luAlignment - 1);

        for (u32 luIndex = 0; luIndex < luNumRequests; )
        {
            // Tripwire: a merge must never run off the end of the node list.
            CGS_ASSERT(liNode <= liLastNode, "Gone past end of heap\n");

            u32 luAlignedSize = (lpRequest->muSize + luAlignment - 1) & luAlignMask;

            if (luFreeSize < luAlignedSize)
            {
                // Not enough room: merge the next contiguous USED node(s) down.
                s32 liScan = liNode + 1;
                if (liScan <= liLastNode)
                {
                    while (lpNodes[liScan].muStatus == LinearHeapNode::KU_STATUS_USED)
                    {
                        AddRelocateRequest(static_cast<u16>(liScan), luDestOffset);
                        luDestOffset += lpNodes[liScan].muSize;
                        ++liScan;
                        if (liScan > liLastNode)
                        {
                            break;   // goto LABEL_18
                        }
                    }
                    if (liScan <= liLastNode)
                    {
                        liNode = liScan;
                        luFreeSize += lpNodes[liScan].muSize;
                    }
                }
            }
            else
            {
                // Enough room: stage the addressed allocation and shrink the free region.
                AddAddressedAllocRequest(luAlignedSize, luDestOffset, lpRequest->mpOwner);
                luDestOffset += luAlignedSize;
                luFreeSize   -= luAlignedSize;
                ++luIndex;
                ++lpRequest;
            }
        }

        return true;
    }

    // -------- RunPoolDefragmentation --------
    // Hand the staged relocation plan to the pool, executing it through the latched ScratchPool. Tail
    // call to Pool::BeginDefragmentation(mpScratchPool, requests, sources, num, memType) on mpPool.
    void IntelliFragPoolModuleState::RunPoolDefragmentation(RelocateRequest* lpRequests,
                                                            RelocateSource* lpSources,
                                                            u32 luNum, s32 liMemType)
    {
        GetPool()->BeginDefragmentation(mpScratchPool, lpRequests, lpSources, luNum, liMemType);
    }

    // -------- Update --------
    // Poll the intellifrag step machine once (driven by PoolModule::UpdateIntelliFrag). Dispatch on
    // meState by NUMERIC value exactly as the console does:
    //   IDLE(0):                nothing to do -> SUCCESS.
    //   START_DEFRAGMENTING(1): while the pool still holds a resource in purgatory (a defrag pass
    //                           may not start until purgatory is empty) stay PEND; once clear, scan
    //                           the three per-memtype batch-alloc results -- the first that reports
    //                           NEED_DEFRAG kicks off BeginDefragment for that memtype (PEND on
    //                           success; on failure fall back to IDLE and escalate to EMERGENCY). If
    //                           none need defrag, return to IDLE and report SUCCESS.
    //   DEFRAGMENTING_HEAP(2):  once the pool's defrag latch has cleared (GetDefragMemType()==-1),
    //                           re-latch and arm the final addressed allocations; return PEND.
    //   >=3 (invalid):          assert tripwire, return ERROR.
    IntelliFragPoolModuleState::EIntelliFragResult IntelliFragPoolModuleState::Update()
    {
        switch (meState)
        {
        case E_STATE_IDLE:
            return E_RESULT_SUCCESS;

        case E_STATE_START_DEFRAGMENTING:               // meState == 1
            if (GetPool()->GetNumEntriesInPurgatory() != 0)
            {
                return E_RESULT_PEND;
            }
            // Pool has finished its current relocation batch: find the first memtype whose batch-alloc
            // still needs defragmenting.
            for (s32 liMemType = 0; liMemType < 3; ++liMemType)
            {
                if (GetAllocationResult(liMemType) == E_BATCHALLOCRESULT_FAIL_NEED_DEFRAG)  // ==1
                {
                    meState = E_STATE_DEFRAGMENTING_HEAP;
                    if (BeginDefragment(liMemType))
                    {
                        return E_RESULT_PEND;
                    }
                    meState = E_STATE_IDLE;
                    if (CgsDev::Message::gxMessageFilterFlags & 1)
                    {
                        *CgsDev::Log::gpDebugPrint << "Intellifrag moving too much data - returning emergency\n";
                    }
                    return E_RESULT_EMERGENCY;
                }
            }
            meState = E_STATE_IDLE;
            return E_RESULT_SUCCESS;

        case E_STATE_DEFRAGMENTING_HEAP:                // meState == 2
            if (GetPool()->GetDefragMemType() == -1)    // pool defrag done
            {
                meState = E_STATE_START_DEFRAGMENTING;
                DoFinalAllocations();
            }
            return E_RESULT_PEND;

        default:
            // meState >= 3 -- an impossible step value.
            CGS_ASSERT(false, "Defrag state is invalid\n");
            return E_RESULT_ERROR;
        }
    }
}
