#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAnimData.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX. CgsGui::AnimChannelData holds one
// keyframed APT animation channel. Both methods are plain field initialisers (the
// X360 emits straight-line stores into the six members); behaviour-faithful to the
// pseudocode at the addresses noted below.

namespace CgsGui
{
    // @ 0x82848E00 - default the channel: zeroed times/values, and the "max" sentinels
    // for the interpolation and animation-space modes.
    void AnimChannelData::Construct()
    {
        mStartTime     = 0.0f;
        mEndTime       = 0.0f;
        mfStartValue   = 0.0f;
        mfEndValue     = 0.0f;
        meInterpolator = E_INTERPOLATE_MAX;
        meAnimType     = E_ANIMATION_MAX;
    }

    // @ 0x82848E30 - set every field from the supplied keyframe parameters.
    void AnimChannelData::SetData(Time lStartTime, Time lEndTime, f32 lfStartValue, f32 lfEndValue,
                                  InterpolateType leInterpolator, AnimationType leAnimType)
    {
        mStartTime     = lStartTime;
        mEndTime       = lEndTime;
        mfStartValue   = lfStartValue;
        mfEndValue     = lfEndValue;
        meInterpolator = leInterpolator;
        meAnimType     = leAnimType;
    }
}
