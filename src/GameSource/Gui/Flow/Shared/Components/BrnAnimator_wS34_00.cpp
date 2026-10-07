// BrnAnimator_wS34_00.cpp -- BrnGui::Animator's two library appenders (BrnAnimator.cpp family),
// reconstructed from the console image.

#include "GameSource/Gui/Flow/Shared/Components/BrnAnimator.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnGui
{
    // append a channel animation; returns its library slot.
    u32 Animator::AddAnimationChannelToLibrary(const CgsGui::AnimChannelData* lpChannelData)
    {
        CGS_ASSERT(lpChannelData != 0, "Invalid animation channel");

        mChannelAnimationLibrary.Append(*lpChannelData);
        return mChannelAnimationLibrary.GetLength() - 1;
    }

    // append a whole animation; returns its library slot.
    u32 Animator::AddAnimationToLibrary(const CgsGui::AnimData* lpAnimData)
    {
        CGS_ASSERT(lpAnimData != 0, "Invalid animation data");

        mAnimationLibrary.Append(*lpAnimData);
        return mAnimationLibrary.GetLength() - 1;
    }
}
