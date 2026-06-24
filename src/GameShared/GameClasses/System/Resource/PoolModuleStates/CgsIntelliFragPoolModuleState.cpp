#include "types.hpp"

#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsIntelliFragPoolModuleState.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"  // Pool::BeginDefragmentation / GetHeapAlignment, AllocListSet
#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"           // gpDebugPrint / gxMessageFilterFlags

// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// THIS TU (GameShared/.../PoolModuleStates/CgsIntelliFragPoolModuleState.cpp) -- the intelligent
// (linear-merge) defragmentation strategy:
//   CgsResource::IntelliFragPoolModuleState::Begin                @ 0x828DA908
//   CgsResource::IntelliFragPoolModuleState::RunDefragAlgorithm   @ 0x828E3EB8
//   CgsResource::IntelliFragPoolModuleState::RunPoolDefragmentation @ 0x828F80C0
// Each is bodied store-for-store against the X360 asm; members are referenced by name (offsets
// verified in the owning header). Base private state (mpPool / miMaxToMove) is reached through the
// base's public GetPool()/GetHeapAlignment/SetMaxToMove accessors rather than raw offsets. The X360
// asserts that stream a message into the debug buffer are expressed through CGS_ASSERT.

namespace CgsResource
{
    // -------- Begin @ 0x828DA908 --------
    // Run the base Begin first (working-set copy + null asserts), assert we are idle, then arm the
    // step machine: cap a single pass at 2MB (the base's miMaxToMove, written at +0x48), set
    // meState = START_DEFRAGMENTING (+0x4C) and latch the ScratchPool from the params (+0x50). A
    // filtered debug print announces the pass.
    void IntelliFragPoolModuleState::Begin(IntelliFragParams* lpParams)
    {
        BaseDefragPoolModuleState::Begin(lpParams);                      // bl ...BaseDefragPoolModuleState__Begin

        CGS_ASSERT(meState == E_STATE_IDLE, "Can not defrag unless in idle state\n");  // :63 (if (a1[19]) assert)

        meState       = E_STATE_START_DEFRAGMENTING;   // stw r11(1), 0x4C
        SetMaxToMove(0x200000);                         // stw r10(0x200000), 0x48 (base miMaxToMove)
        mpScratchPool = lpParams->mpScratchPool;        // lwz r11,0x30(params); stw r11,0x50

        if (CgsDev::Message::gxMessageFilterFlags & 1)  // clrldi/cmpldi gate on bit 0
        {
            *CgsDev::Log::gpDebugPrint << "Running intelligent defragment\n";   // (*gpDebugPrint)[1]("...")
        }
    }

    // -------- RunDefragAlgorithm @ 0x828E3EB8 --------
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

        s32 liFreeNode = FindNextFreeNode(0);   // bl ...FindNextFreeNode (a2 = 0)
        if (liFreeNode == -1)
        {
            return false;                        // li r3,0
        }

        s32           liNode        = liFreeNode;                                        // v14
        u32           luNumRequests = lpAllocListSet->manAllocRequestCounts[liMemType];  // v17 = *(a2 + a5*4)
        AllocRequest* lpRequest     = lpAllocListSet->mapAllocRequests[liMemType];       // v22 = *(a2 + (a5+6)*4)
        u32           luAlignment   = GetPool()->GetHeapAlignment(liMemType);            // v20 = *(mpPool + a5*64 + 44)

        u32 luDestOffset  = lpNodes[liFreeNode].muOffset;   // v19 = *v16
        u32 luFreeSize    = lpNodes[liFreeNode].muSize;     // v21 = v16[1]
        u32 luAlignMask   = ~(luAlignment - 1);             // v29

        for (u32 luIndex = 0; luIndex < luNumRequests; )
        {
            // Tripwire: a merge must never run off the end of the node list.
            CGS_ASSERT(liNode <= liLastNode, "Gone past end of heap\n");   // :222

            u32 luAlignedSize = (lpRequest->muSize + luAlignment - 1) & luAlignMask;   // v25 = *v22+v20-1 ; v26 = v25 & v29

            if (luFreeSize < luAlignedSize)
            {
                // Not enough room: merge the next contiguous USED node(s) down.
                s32 liScan = liNode + 1;                              // v27 = v14 + 1
                if (liScan <= liLastNode)
                {
                    while (lpNodes[liScan].muStatus == LinearHeapNode::KU_STATUS_USED)   // *(v28+4)==1
                    {
                        AddRelocateRequest(static_cast<u16>(liScan), luDestOffset);   // (v27++, v19)
                        luDestOffset += lpNodes[liScan].muSize;   // v19 += *v28
                        ++liScan;
                        if (liScan > liLastNode)
                        {
                            break;   // goto LABEL_18
                        }
                    }
                    if (liScan <= liLastNode)
                    {
                        liNode = liScan;                                  // v14 = v27
                        luFreeSize += lpNodes[liScan].muSize;             // v21 += *(16*v27 + a3 + 4)
                    }
                }
            }
            else
            {
                // Enough room: stage the addressed allocation and shrink the free region.
                AddAddressedAllocRequest(luAlignedSize, luDestOffset, lpRequest->mpOwner);  // (v26, v19, v22[3])
                luDestOffset += luAlignedSize;   // v19 += v26
                luFreeSize   -= luAlignedSize;   // v21 -= v26
                ++luIndex;                       // ++v18
                ++lpRequest;                     // v22 += 4 (one AllocRequest)
            }
        }

        return true;   // li r3,1
    }

    // -------- RunPoolDefragmentation @ 0x828F80C0 --------
    // Hand the staged relocation plan to the pool, executing it through the latched ScratchPool. Tail
    // call to Pool::BeginDefragmentation(mpScratchPool, requests, sources, num, memType) on mpPool.
    void IntelliFragPoolModuleState::RunPoolDefragmentation(RelocateRequest* lpRequests,
                                                            RelocateSource* lpSources,
                                                            u32 luNum, s32 liMemType)
    {
        GetPool()->BeginDefragmentation(mpScratchPool, lpRequests, lpSources, luNum, liMemType);
    }
}
