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

// FOLDED FROM CgsAptAnimData_wS34_00.cpp
// CgsAptAnimData_wS34_00.cpp -- AnimData's channel-list methods (CgsAptAnimData.cpp family),
// reconstructed from the console image.

#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAnimData.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace CgsGui
{
    // empty both lists.
    void AnimData::Construct()
    {
        mChannelData.Construct();
        meChannels.Construct();
    }

    // the keyframe data this animation holds for liChannel, or 0.
    AnimChannelData* AnimData::GetChannelData(s32 liChannel)
    {
        for (u32 luIndex = 0; luIndex < meChannels.GetLength(); ++luIndex)
        {
            if (AnimatorChan(luIndex) == liChannel)
                return &mChannelData.GetItem(luIndex);
        }
        return 0;
    }

    // add one channel's keyframe data.
    void AnimData::AddAnimationChannel(AnimatorChannel leChannel, AnimChannelData lChannelData)
    {
        CGS_ASSERT(GetChannelData(leChannel) == 0, "We already have data for this channel");

        mChannelData.Append(lChannelData);
        meChannels.Append(leChannel);
    }
}
