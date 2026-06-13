#ifndef BRN_GUI_EFFECTS_ARBITRATOR_H
#define BRN_GUI_EFFECTS_ARBITRATOR_H

#include "types.hpp"

namespace BrnGui
{
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827E06B8.
// Six effect slots, each an empty circular list node (the three node pointers
// point back at the slot itself), followed by a trailing list anchor.
class EffectsArbitrator
{
public:
    EffectsArbitrator();

private:
    struct EffectSlot
    {
        u32   mVal0;
        u32   mVal1;
        u32   mVal2;
        void* mpNext;   // initialised to the slot itself (empty list)
        void* mpPrev;
        void* mpOwner;
        u32   mVal6;
        u32   mPad[3];
    };

    EffectSlot mSlots[6];

    // Trailing list anchor (guest index 67).
    u32   mAnchorVal0;
    u32   mAnchorVal1;
    u32   mAnchorVal2;
    void* mpAnchorNext;
    void* mpAnchorPrev;
    void* mpAnchorOwner;
    u32   mAnchorVal6;
};
}

#endif
