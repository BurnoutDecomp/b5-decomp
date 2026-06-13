#include "BrnGuiEffectsArbitrator.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827E06B8
//   BrnGui::EffectsArbitrator::EffectsArbitrator
//
// Initialises six effect slots as empty circular lists, then the trailing
// list anchor the same way. The compiler unrolled/strength-reduced the slot
// loop; it is re-rolled here.

namespace BrnGui
{
EffectsArbitrator::EffectsArbitrator()
{
    for (int i = 0; i < 6; ++i)
    {
        mSlots[i].mVal0   = 0;
        mSlots[i].mVal1   = 0;
        mSlots[i].mVal2   = 0;
        mSlots[i].mpNext  = &mSlots[i];
        mSlots[i].mpPrev  = &mSlots[i];
        mSlots[i].mpOwner = &mSlots[i];
        mSlots[i].mVal6   = 0;
    }

    mAnchorVal0   = 0;
    mAnchorVal1   = 0;
    mAnchorVal2   = 0;
    mpAnchorNext  = &mAnchorVal0;
    mpAnchorPrev  = &mAnchorVal0;
    mpAnchorOwner = &mAnchorVal0;
    mAnchorVal6   = 0;
}
}
