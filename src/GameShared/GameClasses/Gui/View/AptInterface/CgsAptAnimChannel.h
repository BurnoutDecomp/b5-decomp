#ifndef CGS_APT_ANIM_CHANNEL_H
#define CGS_APT_ANIM_CHANNEL_H

#include "types.hpp"
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAnimData.h"        // AnimChannelData (keyframe payload)
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptInterpolator.h"    // CgsGui::Interpolator (embedded linear interpolator)

// CgsGui::AnimChannel - one live animation channel for the APT (Scaleform-style)
// GUI interface. It tracks the interpolator currently driving the channel, an
// active flag, the latest interpolated value, and an embedded linear interpolator
// it can bind to.
//
// Layout recovered from the X360 spine:
//   AnimChannel::AnimChannel      @ 0x824E9198  (ctor: stores the embedded
//                                                Interpolator vtable @ +0x0C)
//   AnimChannel::GetInterpolator  @ 0x82848D30  (returns this + 0x0C, i.e.
//                                                &mLinearInterpolator)
//   AnimChannel::Update           @ 0x82848C18  (reads mpCurrentInterpolator @ +0,
//                                                mbActive @ +4, writes mfCurrentValue
//                                                @ +8 via a1[2])
//   AnimChannel::SetAnimation     @ 0x82850178  (clears mbActive/mpCurrentInterpolator,
//                                                rebinds via GetInterpolator)
// Member set / order / names are from the DecFIGS DWARF (CgsAptAnimChannel.h):
//   mpCurrentInterpolator  [+0x00]  Interpolator*   (h:82)
//   mbActive               [+0x04]  bool            (h:84)
//   mfCurrentValue         [+0x08]  f32             (h:85)
//   mLinearInterpolator    [+0x0C]  Interpolator    (h:87; the 0x18-byte subobject
//                                                    whose vtable the ctor installs)
// sizeof == 0x24 (36): +0x0C base + a 0x18-byte Interpolator = 0x24, matching the
// Array<AnimChannel,6>::GetItem stride of 36 (X360 0x8284DA48).
namespace CgsGui
{
    class AnimChannel
    {
    public:
        // @ 0x824E9198 - default the channel. The X360 body installs the embedded
        // linear interpolator's vtable (the only store), which the Interpolator base
        // subobject's construction performs; everything else is set up later by
        // Construct()/SetAnimation().
        AnimChannel();

        // The remaining channel methods are homed by the CgsAptAnimChannel.cpp TU
        // (separate ledger entries). Declared here for the member surface they touch.
        void Construct();
        void SetAnimation(AnimChannelData lAnimData);
        f32  GetCurrentValue() const { return mfCurrentValue; }
        void Update(AnimChannelData::Time lTime);
        void Stop();
        bool IsActive() const { return mbActive; }

    private:
        Interpolator* GetInterpolator(AnimChannelData::InterpolateType leInterpolator);

        Interpolator* mpCurrentInterpolator;  // [+0x00] currently bound interpolator (h:82)
        bool          mbActive;               // [+0x04] channel running flag       (h:84)
        f32           mfCurrentValue;         // [+0x08] latest interpolated value  (h:85)
        Interpolator  mLinearInterpolator;    // [+0x0C] embedded linear interpolator (h:87)
    };
}

#endif // CGS_APT_ANIM_CHANNEL_H
