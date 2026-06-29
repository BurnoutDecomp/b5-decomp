#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsBaseDefragPoolModuleState.h"  // BasePoolModuleState / BaseDefragParams

// CgsResource::AllocatePoolModuleState -- the PoolModule's "allocating a resource list" step state.
// PoolModule::UpdateAllocating (X360 0x82904860) polls Update() once per frame and dispatches on the
// EAllocateResult it returns: SUCCESS/SIMPLEFRAG -> finalise + post the response; ERROR -> assert;
// PEND -> wait; INTELLIFRAG -> hand off to IntelliFragPoolModuleState. This header is a MINIMAL,
// gated reconstruction: it declares only the surface PoolModule needs (the step machine's Update +
// the working-set fields the driver reads), so the per-TU compile gate has a complete type. The full
// body + the remaining layout belong to CgsAllocatePoolModuleState.cpp (its own TU) -- the bodies are
// trap-stubbed at link time. Member offsets/sizes are inferred from the X360 UpdateAllocating field
// reads (this+4 = base linear-heap-node ptr; +0x10C off the node owner; the request working set at
// +0x10..+0x24); widths are x64. Identify members by name, not byte offset.
namespace CgsResource
{
    class AllocatePoolModuleState : public BaseDefragPoolModuleState
    {
    public:
        // The per-frame poll result PoolModule::UpdateAllocating dispatches on (mirrors the IntelliFrag
        // result enum: 0=success, 1=error, 2=pend, 3=need-intellifrag, 4=simple-frag).
        enum EAllocateResult
        {
            E_RESULT_SUCCESS    = 0,
            E_RESULT_ERROR      = 1,
            E_RESULT_PEND       = 2,
            E_RESULT_INTELLIFRAG = 3,
            E_RESULT_SIMPLEFRAG = 4,
        };

        // @ 0x82902640 -- run one allocation step; returns the EAllocateResult above.
        u32 Update();

        // Fill the caller's 48-byte resource-list response record from this step's working set (the
        // allocated handle list / counts the X360 UpdateAllocating marshals inline at 0x829048E4: the
        // owner field *(this+4)+0x10C, the 8-byte id at +8, and the +0x10/0x14/0x1C/0x20/0x24 counters).
        // Mirrors LiveUpdatePoolModuleState::GenerateResponse; the body lives in this state's own TU
        // (CgsAllocatePoolModuleState.cpp, deferred -- trap-stubbed at link), so the driver delegates the
        // working-set copy here rather than poking the state's layout by raw offset.
        void GenerateResponse(void* lpOutResponse);
    };
}
