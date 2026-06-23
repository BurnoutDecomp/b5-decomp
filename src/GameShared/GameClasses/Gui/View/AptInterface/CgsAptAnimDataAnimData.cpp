#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAnimData.h"

// CgsGui::AnimData member functions, reconstructed from BURNOUT_X360_ARTIST.XEX.
// This TU (class:CgsGui::AnimData) bodies the two X360-emitted AnimData methods:
//
//   AnimData::AnimData    @ 0x824E8FC8  (default ctor)
//   AnimData::AnimatorChan @ 0x8284D940 (checked indexed access into meChannels)
//
// AnimData aggregates two fixed Array<> members (mChannelData @+0x00, meChannels
// @+0x94); the layout is pinned by CgsAptAnimDataArray2.cpp (sizeof == 0xB0 from the
// Array<AnimData,2>::Append element memcpy of 176 bytes).

namespace CgsGui
{
    // @ 0x824E8FC8 - default the animation: leave both embedded Array<> members in the
    // pre-Construct state. The X360 body is two stores of the -1 sentinel:
    //     li  r11, -1
    //     stw r11, 0x90(r3)   ; mChannelData.miCount  (Array<AnimChannelData,6> count @ +0x90)
    //     stw r11, 0xAC(r3)   ; meChannels.miCount    (Array<AnimatorChannel,6> count @ +0xAC)
    // i.e. it marks both arrays unconstructed (no element/Construct work). A later
    // Construct/Clear flips the sentinels off so the bounds-checked accessors stop firing
    // the "Array used before Construct/Clear was called" assert.
    AnimData::AnimData()
    {
        mChannelData.MarkUnconstructed();   // stw -1, 0x90(this)
        meChannels.MarkUnconstructed();     // stw -1, 0xAC(this)
    }

    // @ 0x8284D940 - checked indexed access into the meChannels list. The X360 inlines
    // Array<AnimatorChannel,6>::operator[](luIndex) directly here: it asserts the array
    // was Construct/Clear'd (count @ +0x18 of the array subobject != -1, CgsArray.h:538),
    // bounds-checks luIndex against the live count (CgsArray.h:539), then returns
    // &maElements[luIndex] (`return 4 * a2 + a1`, stride 4). Routing through the committed
    // Array<>::operator[] reproduces both asserts and the &element return.
    AnimData::AnimatorChannel& AnimData::AnimatorChan(u32 luIndex)
    {
        return meChannels[luIndex];
    }

    const AnimData::AnimatorChannel& AnimData::AnimatorChan(u32 luIndex) const
    {
        return meChannels[luIndex];
    }
}
