#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsBaseDefragPoolModuleState.h"  // BasePoolModuleState

// CgsResource::DeAllocatePoolModuleState - the pool module's "deallocating" step state. While a
// pool is tearing down a resource, the PoolModule keeps this state active and polls it once per
// frame (CgsResource::PoolModule::UpdateDeAllocating). The state is a tiny machine guarded by a
// settle counter: it sits idle until armed (state 1), counts down a frame budget, and once the
// budget reaches zero clears itself back to idle so the pool can finish the deallocation.
//
// DECOMPILED from BURNOUT_X360_ARTIST.XEX. The polled step is @ 0x828DA830. Member offsets are
// read from that body's loads/stores: the state token is the leading word (lwz r10,0(r11) /
// stw r10,0(r11)) and the frame counter is at +8 (lwz r10,8(r11) / stw r10,8(r11)). We identify
// members by their X360 offsets but do NOT byte-match (PC widths apply). The DWARF home for this
// TU is CgsDeAllocatePoolModuleState.cpp; field +4 is not touched by this step and is modelled
// as reserved storage so the +8 counter sits at its observed offset.

namespace CgsResource
{
    // CgsDeAllocatePoolModuleState.h - one of the PoolModule's step states.
    class DeAllocatePoolModuleState : public BasePoolModuleState
    {
    public:
        // The per-frame poll result the PoolModule acts on.
        //   E_UPDATE_IDLE (0) - not active (or just finished and reset to idle this frame)
        //   E_UPDATE_INVALID (1) - the state token was out of range (asserted)
        //   E_UPDATE_BUSY (2) - still settling; counter decremented, deallocation not yet done
        enum EUpdateResult
        {
            E_UPDATE_IDLE    = 0,
            E_UPDATE_INVALID = 1,
            E_UPDATE_BUSY    = 2
        };

        // Internal state token: 0 = idle, 1 = armed/counting. (Values > 1 are invalid.)
        enum EState
        {
            E_STATE_IDLE    = 0,
            E_STATE_COUNTING = 1
        };

        // The polled step @ 0x828DA830 (called by PoolModule::UpdateDeAllocating).
        u32 Update();   // THIS PASS @ 0x828DA830

        // On completion (Update result 3) PoolModule::UpdateDeAllocating forwards the originating
        // allocate request back to DoAllocateResourceListRequest. The X360 inlines the selection
        // (mbHaveRequest ? &mRequest : 0) at +0xC/+0x10 of this state; expose it as a named accessor
        // so the driver never pokes this object's layout by raw offset. Body lives in this state's own
        // TU (CgsDeAllocatePoolModuleState.cpp; deferred -- trap-stubbed at link).
        const void* GetPendingAllocateRequest() const;

    private:
        u32 muState;            // +0x00  state token (EState)
        u32 muReserved04;       // +0x04  not referenced by the poll step (kept for +8 placement)
        u32 muFramesRemaining;  // +0x08  settle counter
        u8  mbHaveRequest;      // +0x0C  "a request is stashed" flag (read by UpdateDeAllocating)
        // +0x10 the stashed AllocateResourceListRequest record (returned by GetPendingAllocateRequest;
        // its full layout belongs to this state's own TU) -- reserved here as opaque storage.
        u8  maPendingRequest[64];
    };
}
