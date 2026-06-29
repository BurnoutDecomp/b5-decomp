#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsBaseDefragPoolModuleState.h"  // BasePoolModuleState

// CgsResource::LiveUpdatePoolModuleState -- the PoolModule's "live update" step state. While a bundle
// is being hot-reloaded in place, PoolModule::UpdateLiveUpdate (X360 0x82906E70) polls Update() and on
// completion (result 0) calls GenerateResponse() to fill the response record it posts to the pool
// output queue. This header is a MINIMAL, gated reconstruction declaring only the surface PoolModule
// needs, so the per-TU compile gate has a complete type; the full body + layout belong to
// CgsLiveUpdatePoolModuleState.cpp (its own TU, trap-stubbed at link). Identify members by name.
namespace CgsResource
{
    class LiveUpdatePoolModuleState : public BasePoolModuleState
    {
    public:
        // The per-frame poll result PoolModule::UpdateLiveUpdate dispatches on (0=done, 1=error,
        // 2/3=still working).
        enum ELiveUpdateResult
        {
            E_RESULT_DONE  = 0,
            E_RESULT_ERROR = 1,
            E_RESULT_BUSY  = 2,
            E_RESULT_BUSY2 = 3,
        };

        // @ 0x829066E0 -- run one live-update step; returns the ELiveUpdateResult above. (IDA names the
        // export "Up" -- a truncated "Update".)
        u32 Update();

        // @ 0x828E4200 -- fill the caller's response record on completion.
        void GenerateResponse(void* lpOutResponse);
    };
}
