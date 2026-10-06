#pragma once
#include "types.hpp"
#include <cmath>

namespace renderengine {
// FLAG PC-platform leaf: observation budget only; never advances simulation.
// A held clock is correlated with the actual Driver Details pause by the case.
struct TrailPausedClockPC
{
    bool mbSeen = false;
    f32 mfNow = 0.0f;
    u32 muHeldSince = 0u, muLastRecord = 0u, muRecords = 0u;
    void Observe(f32 lfNow, u32 luPresent)
    {
        if (!std::isfinite(lfNow)) { mbSeen = false; return; }
        if (!mbSeen || lfNow != mfNow)
        {
            mbSeen = true; mfNow = lfNow; muHeldSince = luPresent;
        }
    }
    bool Take(u32 luPresent)
    {
        if (!mbSeen || muRecords >= 12u || luPresent - muHeldSince < 90u
            || (muRecords && luPresent - muLastRecord < 90u)) return false;
        ++muRecords; muLastRecord = luPresent; return true;
    }
};

// Source matrix/time of the unchanged original TrailRenderer::BeginRender.
// No native state is touched; the later original draw reads actual constants.
void TrailPausedDiag_ExpectedPC(f32 lfNow, const f32* lpfMatrix);
}
