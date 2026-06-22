// Embed/usage check for BrnAI::RaceBalancingRoute::ComputeRaceCompletionRatio
// (@0x8277AD98). Exercises the public bodied function and pins the two member
// offsets the X360 asm reads directly (miCurrentCheckpointIndex @+0xA08,
// mfDistance @+0xA0C).

#include "GameSource/World/AI/RaceBalancing/BrnRaceBalancingRoute.h"

#include <cstddef>

namespace BrnAI
{
// The bodied function is const + public; reference it to force emission.
f32 (RaceBalancingRoute::*const gpfRatio)(f32, s32) const =
    &RaceBalancingRoute::ComputeRaceCompletionRatio;

// Befriended probe: the offset checks need access to the private members.
struct RaceBalancingRouteEmbedProbe
{
    static void Touch()
    {
        // mafTimes[2][320] = 2560 bytes precedes the int block; the asm reads
        // miCurrentCheckpointIndex at +0xA08 and mfDistance at +0xA0C.
        static_assert(offsetof(RaceBalancingRoute, mafTimes) == 0, "mafTimes at +0");
        static_assert(offsetof(RaceBalancingRoute, miCurrentCheckpointIndex) == 0xA08,
                      "miCurrentCheckpointIndex must sit at +0xA08");
        static_assert(offsetof(RaceBalancingRoute, mfDistance) == 0xA0C,
                      "mfDistance must sit at +0xA0C");
    }
};
}

namespace { void EmbedCheck() { BrnAI::RaceBalancingRouteEmbedProbe::Touch(); } }
