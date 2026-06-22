#pragma once

#include "types.hpp"

// CgsGui::AnimChannelData - one keyframed animation channel for the APT (gui movie)
// interface: a start/end time pair, a start/end value pair, and the interpolation /
// animation-space mode. Recovered from the X360 spine (Construct @0x82848E00,
// SetData @0x82848E30); member set, order and the InterpolateType / AnimationType
// enums are from the DecFIGS DWARF (CgsAptAnimData.h). This is the minimal owning
// slice for the two bodied channel methods; the higher-level AnimData container that
// aggregates six of these is homed by its own TU.
namespace CgsGui
{
    struct AnimChannelData
    {
        typedef f32 Time;

        enum InterpolateType
        {
            E_INTERPOLATE_LINEAR = 0,
            E_INTERPOLATE_MAX    = 1,
        };

        enum AnimationType
        {
            E_ANIMATION_SCREEN_TO_SCREEN_SPACE = 0,
            E_ANIMATION_SCREEN_TO_OBJECT_SPACE = 1,
            E_ANIMATION_OBJECT_TO_SCREEN_SPACE = 2,
            E_ANIMATION_OBJECT_TO_OBJECT_SPACE = 3,
            E_ANIMATION_MAX                    = 4,
        };

        Time            mStartTime;       // +0
        Time            mEndTime;         // +4
        f32             mfStartValue;     // +8
        f32             mfEndValue;       // +12
        InterpolateType meInterpolator;   // +16
        AnimationType   meAnimType;       // +20

        void Construct();
        void SetData(Time lStartTime, Time lEndTime, f32 lfStartValue, f32 lfEndValue,
                     InterpolateType leInterpolator, AnimationType leAnimType);
    };
}
