#include "types.hpp"

#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsBaseDefragPoolModuleState.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// THIS TU (GameShared/.../PoolModuleStates/CgsBaseDefragPoolModuleState.cpp) -- the three
// pure-logic spine functions of the shared defrag base state:
//   CgsResource::BaseDefragPoolModuleState::Begin                   @ 0x828DA090
//   CgsResource::BaseDefragPoolModuleState::FindNextFreeNode        @ 0x828DA248
//   CgsResource::BaseDefragPoolModuleState::BuildFinalRelocationData@ 0x828DA2E8
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
}
