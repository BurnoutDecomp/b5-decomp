#include "types.hpp"

#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsDeAllocatePoolModuleState.h"
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
            u32 luFrames = muFramesRemaining;   // lwz r10, 8(r11)
            if (luFrames == 0)                  // cmpwi r10, 0 ; beq-fallthrough
            {
                muState = E_STATE_IDLE;         // stw r10, 0(r11)  (r10 is 0 here)
                return E_UPDATE_IDLE;           // li r3, 0
            }
            muFramesRemaining = luFrames - 1;   // addi r10,r10,-1 ; stw r10, 8(r11)
            return E_UPDATE_BUSY;               // li r3, 2
        }

        CGS_ASSERT(false, "Deallocate state is invalid\n");   // :169
        return E_UPDATE_INVALID;                // li r3, 1
    }
}
