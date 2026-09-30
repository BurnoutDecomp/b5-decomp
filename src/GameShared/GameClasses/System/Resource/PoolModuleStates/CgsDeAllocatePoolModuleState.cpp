#include "types.hpp"

#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsDeAllocatePoolModuleState.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"
#include "GameShared/GameClasses/System/Resource/CgsEntryListResource.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// THIS TU (GameShared/.../PoolModuleStates/CgsDeAllocatePoolModuleState.cpp) -- the polled
// "deallocating" step the PoolModule runs once per frame:
//   CgsResource::DeAllocatePoolModuleState::Update @ 0x828DA830
// Bodied store-for-store against the X360 asm; members are referenced by name (offsets verified
// in the owning header). The X360 invalid-state assert that streams a formatted message into the
// debug buffer (BasePriorityQueue::Clear + StrStream machinery) is expressed through CGS_ASSERT.

namespace CgsResource
{
    // Inlined by PoolModule::Construct at ARTIST 828FC0B8.
    void DeAllocatePoolModuleState::Construct(PoolModule* lpPoolModule)
    {
        muState = E_STATE_IDLE;
        mpPoolModule = lpPoolModule;
        miFramesRemaining = 0;
        mbGotPendingAllocationRequest = false;
    }

    // ARTIST 828F7DC8. Leave zero-reference resources in the pool's purgatory
    // countdown. RemoveReference's immediate native cleanup is not this path.
    void DeAllocatePoolModuleState::Begin(Pool* lpPool, ID lListId)
    {
        CGS_ASSERT(muState == E_STATE_IDLE || muState == E_STATE_COUNTING,
                   "Can not de-allocate unless in idle/de-allocating state\n");
        muState = E_STATE_COUNTING;
        const s32 liDelay = lpPool->GetRefCountThreshold();
        if (miFramesRemaining < liDelay)
            miFramesRemaining = liDelay;

        s32 liListIndex;
        Entry* lpListEntry = lpPool->FindResource(lListId, false, 2, &liListIndex);
        CGS_ASSERT(lpListEntry != nullptr, "Dynamically created list resource is not loaded\n");
        const EntryListResource* lpList = static_cast<const EntryListResource*>(
            lpListEntry->mResource.m_baseResources[0]);
        CGS_ASSERT(lpPool->GetEntryRefCount(liListIndex) > 0, "Decrementing ref count on zero count resource\n");
        lpPool->DecEntryRefCount(liListIndex);

        for (u32 lu = 0; lu < lpList->muNumEntries; ++lu)
        {
            Pool* lpOwner = nullptr;
            s32 liIndex;
            Entry* lpEntry = lpPool->FindResourceWithDependencies(
                lpList->mIds[lu], &lpOwner, false, 2, &liIndex);
            CGS_ASSERT(lpEntry != nullptr, "Could not unload resource as it wasn't found in the pool\n");
            CGS_ASSERT(lpOwner->GetEntryRefCount(liIndex) > 0, "Decrementing ref count on zero count resource\n");
            lpOwner->DecEntryRefCount(liIndex);
        }
    }

    // -------- Update @ 0x828DA830 --------
    // Branch on the state token (lwz r10,0(this); cmplwi 1):
    //   state == 0  -> idle, return E_UPDATE_IDLE
    //   state == 1  -> read the settle counter (this+8); if it has reached 0, reset the state to
    //                  idle and return E_UPDATE_IDLE; otherwise decrement it and return
    //                  E_UPDATE_BUSY (still settling)
    //   state >  1  -> invalid: assert and return E_UPDATE_INVALID
    u32 DeAllocatePoolModuleState::Update()
    {
        if (muState == E_STATE_IDLE)            // r10 < 1  -> loc_828DA8E4
        {
            return E_UPDATE_IDLE;
        }

        if (muState == E_STATE_COUNTING)        // r10 == 1 -> loc_828DA8D4
        {
            s32 liFrames = miFramesRemaining;   // lwz r10, 8(r11)
            if (liFrames == 0)                  // cmpwi r10, 0 ; beq-fallthrough
            {
                muState = E_STATE_IDLE;         // stw r10, 0(r11)  (r10 is 0 here)
                return E_UPDATE_IDLE;           // li r3, 0
            }
            miFramesRemaining = liFrames - 1;   // addi r10,r10,-1 ; stw r10, 8(r11)
            return E_UPDATE_BUSY;               // li r3, 2
        }

        CGS_ASSERT(false, "Deallocate state is invalid\n");   // :169
        return E_UPDATE_INVALID;                // li r3, 1
    }
}
