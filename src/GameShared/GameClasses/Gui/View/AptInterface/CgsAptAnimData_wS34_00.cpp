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
