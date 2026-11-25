#pragma once

#include "types.hpp"

namespace CgsSystem
{
    class Time;
}

namespace BrnNetwork
{
    struct NetworkRounder
    {
        static void RoundFloatToAccuracy(f32* lpfFloat, f32 lfAccuracy);
        static void RoundDistanceToNetworkAccuracy(f32* lpfDistance);
        static void RoundTimeToNetworkAccuracy(CgsSystem::Time* lpTime);
    };
}
