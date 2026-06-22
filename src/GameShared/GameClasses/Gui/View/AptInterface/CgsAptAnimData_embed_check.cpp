#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAnimData.h"
#include <cstddef>

// Embed check: exercises the bodied AnimChannelData methods and pins the member layout
// proven from the X360 Construct/SetData stores (offsets 0/4/8/12/16/20).
namespace
{
    using CgsGui::AnimChannelData;

    static_assert(offsetof(AnimChannelData, mStartTime)   == 0,  "mStartTime @ +0");
    static_assert(offsetof(AnimChannelData, mEndTime)     == 4,  "mEndTime @ +4");
    static_assert(offsetof(AnimChannelData, mfStartValue) == 8,  "mfStartValue @ +8");
    static_assert(offsetof(AnimChannelData, mfEndValue)   == 12, "mfEndValue @ +12");
    static_assert(offsetof(AnimChannelData, meInterpolator) == 16, "meInterpolator @ +16");
    static_assert(offsetof(AnimChannelData, meAnimType)     == 20, "meAnimType @ +20");

    void EmbedCheck()
    {
        AnimChannelData lData;
        lData.Construct();
        lData.SetData(1.0f, 2.0f, 0.0f, 1.0f,
                      AnimChannelData::E_INTERPOLATE_LINEAR,
                      AnimChannelData::E_ANIMATION_SCREEN_TO_SCREEN_SPACE);
    }
}
