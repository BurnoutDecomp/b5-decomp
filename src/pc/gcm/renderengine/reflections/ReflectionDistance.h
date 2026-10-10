#pragma once

#include "types.hpp"

namespace renderengine
{
    // FLAG PC-platform leaf: a registered reflection-only distance extension.
    // Zero preserves the authored model cutoffs and the original 75 m face frustum.
    const f32 KF_MAX_ENVIRONMENT_MAP_DRAW_DISTANCE_PC = 10000.0f;
    inline f32& EnvironmentMapDrawDistancePC()
    {
        static f32 sfDistance = 0.0f;
        return sfDistance;
    }

    inline f32 ExtendEnvironmentMapDrawDistancePC(f32 lfAuthoredDistance)
    {
        const f32 lfDistance = EnvironmentMapDrawDistancePC();
        return lfDistance > lfAuthoredDistance && lfDistance <= KF_MAX_ENVIRONMENT_MAP_DRAW_DISTANCE_PC
            ? lfDistance : lfAuthoredDistance;
    }
}
