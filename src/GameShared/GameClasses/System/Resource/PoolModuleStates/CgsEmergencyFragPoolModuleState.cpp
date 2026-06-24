#include "types.hpp"

#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsEmergencyFragPoolModuleState.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"  // Pool::BeginEmergencyDefragmentation / GetHeapAlignment, AllocListSet
#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"           // gpDebugPrint / gxMessageFilterFlags

// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// THIS TU (GameShared/.../PoolModuleStates/CgsEmergencyFragPoolModuleState.cpp) -- the emergency
// defragmentation strategy:
//   CgsResource::EmergencyFragPoolModuleState::Begin                @ 0x828DA9F8
//   CgsResource::EmergencyFragPoolModuleState::RunDefragAlgorithm   @ 0x828E40C8
//   CgsResource::EmergencyFragPoolModuleState::RunPoolDefragmentation @ 0x828F80E0
// Each is bodied store-for-store against the X360 asm; members are referenced by name (offsets
// verified in the owning header). Base private state (mpPool) is reached through the base's public
// GetPool()/GetHeapAlignment accessor rather than a raw cross-object offset. The X360 asserts that
// stream a message into the debug buffer are expressed through CGS_ASSERT.

namespace CgsResource
{
    // -------- Begin @ 0x828DA9F8 --------
    // Run the base Begin first (it copies the working set + null-asserts the pointers), then assert
    // we are currently idle (meState == 0), arm the step machine and latch the Relocator params for
    // the defragmentation pass. The X360 stores 1 at +0x4C (meState = START_DEFRAGMENTING), 2 at
    // +0x50 (miCountdown), then *(params+0x30)/*(params+0x34) at +0x54/+0x58 (mpRelocator /
    // mpRelocationParams). A filtered debug print announces the pass.
    void EmergencyFragPoolModuleState::Begin(EmergencyFragParams* lpParams)
    {
        BaseDefragPoolModuleState::Begin(lpParams);                      // bl ...BaseDefragPoolModuleState__Begin

        CGS_ASSERT(meState == E_STATE_IDLE, "Can not defrag unless in idle state\n");  // :64 (if (a1[19]) assert)

        meState            = E_STATE_START_DEFRAGMENTING;   // stw r11(1), 0x4C
        miCountdown        = E_STATE_DEFRAGMENTING_HEAP;    // stw r10(2), 0x50  (literal 2)
        mpRelocator        = lpParams->mpRelocator;         // lwz r11,0x30(params); stw r11,0x54
        mpRelocationParams = lpParams->mpRelocationParams;  // lwz r11,0x34(params); stw r11,0x58

        if (CgsDev::Message::gxMessageFilterFlags & 1)      // clrldi/cmpldi gate on bit 0
        {
            *CgsDev::Log::gpDebugPrint << "Running emergency defragment\n";   // (*gpDebugPrint)[1]("...")
        }
    }

    // -------- RunDefragAlgorithm @ 0x828E40C8 --------
    // Pack every live block down toward the first free node. Find the first free node; if there is
    // none, bail (false). Otherwise walk the nodes after it (up to liLastNode): each USED node gets a
    // relocate request to the running destination offset, which then advances by that node's size.
    // Finally, for the addressed-alloc requests of memory pool liMemType, emit one
    // AddAddressedAllocRequest per request at the (alignment-rounded) running offset. Returns true.
    bool EmergencyFragPoolModuleState::RunDefragAlgorithm(AllocListSet* lpAllocListSet,
                                                          LinearHeapNode* lpNodes,
                                                          s32 liLastNode, s32 liMemType)
    {
        CGS_ASSERT(GetPool(), "GetPool()");   // :185 (if (!*(a1+12)) assert)

        s32 liFreeNode = FindNextFreeNode(0);   // bl ...FindNextFreeNode (a2 = 0)
        if (liFreeNode == -1)
        {
            return false;                        // li r3,0
        }

        // Pack the USED nodes after the free node down into it.
        u32 luDestOffset = lpNodes[liFreeNode].muOffset;   // v13 = *(16*NextFreeNode + a3)
        for (s32 liNode = liFreeNode + 1; liNode <= liLastNode; ++liNode)
        {
            LinearHeapNode& lNode = lpNodes[liNode];
            if (lNode.muStatus == LinearHeapNode::KU_STATUS_USED)   // lhz *(v14+4) == 1
            {
                AddRelocateRequest(static_cast<u16>(liNode), luDestOffset);   // clrlwi r4 -> u16 node
                luDestOffset += lNode.muSize;   // v13 += *v14
            }
        }

        // Stage the freed-up addressed allocations for this memory pool at the packed tail.
        u32          luNumRequests = lpAllocListSet->manAllocRequestCounts[liMemType];  // *(a2 + a5*4)
        AllocRequest* lpRequest    = lpAllocListSet->mapAllocRequests[liMemType];       // *(a2 + (a5+6)*4)
        u32          luAlignment   = GetPool()->GetHeapAlignment(liMemType);            // *(mpPool + a5*64 + 44)

        for (; luNumRequests; --luNumRequests, ++lpRequest)
        {
            u32 luAlignedSize = (lpRequest->muSize + luAlignment - 1) & ~(luAlignment - 1);
            AddAddressedAllocRequest(luAlignedSize, luDestOffset, lpRequest->mpOwner);  // (size, offset, owner)
            luDestOffset += luAlignedSize;   // v13 += v18
        }

        return true;   // li r3,1
    }

    // -------- RunPoolDefragmentation @ 0x828F80E0 --------
    // Hand the staged relocation plan to the pool, executing it through the latched Relocator. Tail
    // call to Pool::BeginEmergencyDefragmentation(mpRelocator, mpRelocationParams, requests, sources,
    // num, memType) on mpPool (the pool obtained from the base).
    void EmergencyFragPoolModuleState::RunPoolDefragmentation(RelocateRequest* lpRequests,
                                                              RelocateSource* lpSources,
                                                              u32 luNum, s32 liMemType)
    {
        GetPool()->BeginEmergencyDefragmentation(mpRelocator, mpRelocationParams,
                                                 lpRequests, lpSources, luNum, liMemType);
    }
}
