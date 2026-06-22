// BrnAI::RaceBalancingRoute -- ComputeRaceCompletionRatio (@0x8277AD98).
// Maps a within-segment distance-to-next-checkpoint into a global 0..1
// race-completion ratio, interpolating between the current checkpoint's
// start/end fractions of the whole route.

#include "GameSource/World/AI/RaceBalancing/BrnRaceBalancingRoute.h"

namespace BrnAI
{
// @0x8277AD98
// The X360 form uses fsel-based branchless clamps (the rwmath Clamp<float>/
// Max<float> helpers inlined): the segment fraction is clamped to [0,1], the
// per-checkpoint start/end fractions are computed from the checkpoint index and
// count, and the lerped result is clamped to [0,1].
f32 RaceBalancingRoute::ComputeRaceCompletionRatio(f32 lfDistanceToNextCheckpoint,
                                                   s32 liCheckpointCount) const
{
    if (mfDistance == 0.0f)
    {
        return 0.0f;
    }

    // Fraction of the current segment already travelled, clamped to [0,1].
    f32 lfCheckpointRatio = (mfDistance - lfDistanceToNextCheckpoint) / mfDistance;
    if (lfCheckpointRatio < 0.0f)
    {
        lfCheckpointRatio = 0.0f;
    }
    if (lfCheckpointRatio > 1.0f)
    {
        lfCheckpointRatio = 1.0f;
    }

    // This checkpoint's start/end fractions of the whole route.
    const f32 lfCheckpointStartRatio =
        static_cast<f32>(miCurrentCheckpointIndex) / static_cast<f32>(liCheckpointCount);
    const f32 lfCheckpointEndRatio =
        static_cast<f32>(miCurrentCheckpointIndex + 1) / static_cast<f32>(liCheckpointCount);

    // Interpolate then clamp to [0,1].
    f32 lfResult = (lfCheckpointEndRatio - lfCheckpointStartRatio) * lfCheckpointRatio
                   + lfCheckpointStartRatio;
    if (lfResult < 0.0f)
    {
        lfResult = 0.0f;
    }
    if (lfResult > 1.0f)
    {
        lfResult = 1.0f;
    }
    return lfResult;
}
}
