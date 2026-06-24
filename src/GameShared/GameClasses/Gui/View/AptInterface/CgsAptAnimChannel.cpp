#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAnimChannel.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (flagged substitute for the StrStream assert path)

// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// CgsGui::AnimChannel is one live APT animation channel. This TU homes the channel
// constructor plus the three X360-emitted behavioural methods recovered against the asm:
//
//   GetInterpolator(InterpolateType) @ 0x82848D30 -> &mLinearInterpolator for E_INTERPOLATE_LINEAR,
//                                                    else null with the "Invalid interpolator
//                                                    specified" / "Could not find a matching
//                                                    interpolator" tripwires.
//   SetAnimation(AnimChannelData)    @ 0x82850178 -> stop-if-active, rebind mpCurrentInterpolator
//                                                    via GetInterpolator, drive its SetInterpolator
//                                                    with the keyframe's time/value pairs, set active.
//   Update(Time)                     @ 0x82848C18 -> advance the bound interpolator, latch the new
//                                                    current value, drop the binding past the end.
//
// X360 store-for-store notes (member offsets: mpCurrentInterpolator@+0, mbActive@+4 [byte],
// mfCurrentValue@+8 [f32], mLinearInterpolator@+12; Interpolator vtable: [0]=SetInterpolator,
// [1]=GetCurrentValue, [2]=Update, with mfEndLambda at Interpolator+8):
//   - GetInterpolator: `cmpwi r4,0; beq` LINEAR path -> `addi r28,this,0xC` (&mLinearInterpolator);
//     the `cmplwi r28,0` null check (this+12 == 0) never trips in practice. The non-LINEAR path
//     fires "Invalid interpolator specified" (line 157) then "Could not find a matching
//     interpolator" (line 161) and returns 0.
//   - SetAnimation: `lbz 4(this)` (mbActive) -> if set, `stb 0,4(this)` + `stw 0,0(this)` (stop).
//     `GetInterpolator(this, animData.meInterpolator)` -> `stw r3,0(this)` (mpCurrentInterpolator).
//     null tripwire "Invalid interpolator" (line 64). Then the vtable[0] SetInterpolator call with
//     `lfs f1=mStartTime, f2=mEndTime, f3=mfStartValue, f4=mfEndValue`, then `stb 1,4(this)` (active).
//   - Update: `fcmpu f31,0.0; bge` -> if lTime < 0 fire "Invalid time specified" (line 86). If
//     `mpCurrentInterpolator != 0 && mbActive`: vtable[2] Update(lTime), then `f1 = vtable[1]
//     GetCurrentValue()`, `stfs f1,8(this)` (mfCurrentValue). If `lTime > interp->mfEndLambda(+8)`
//     -> `stw 0,0(this)` (drop the binding). Else (not bound/active): `stb 0,4(this)` (clear active).
//   - The X360-baked file/line cites are discarded per project policy; CGS_ASSERT supplies them.

namespace CgsGui
{
    // @ 0x824E9198 - the X360 body is a single store of the embedded interpolator's
    // vtable into +0x0C:
    //     lis  r11, off_8206C7CC@ha ; addi r11, r11, off_8206C7CC@l
    //     stw  r11, 0xC(r3)         ; blr
    // That store is the construction of the mLinearInterpolator subobject (a
    // CgsGui::Interpolator, which carries a vtable). Default-constructing that member
    // installs the same vtable pointer; the three scalar members are left
    // uninitialised here exactly as in the X360 (they are set up by Construct()/
    // SetAnimation()).
    AnimChannel::AnimChannel()
    {
    }

    // X360 0x82848D30. Resolve the interpolator object for the requested interpolation type.
    // Only E_INTERPOLATE_LINEAR is supported -> the embedded linear interpolator (this+0xC);
    // any other value falls through to the "could not find" tripwire and returns null.
    Interpolator* AnimChannel::GetInterpolator(AnimChannelData::InterpolateType leInterpolator)
    {
        Interpolator* lpInterpolator = nullptr;

        if (leInterpolator != AnimChannelData::E_INTERPOLATE_LINEAR)
        {
            CGS_ASSERT(false, "Invalid interpolator specified");
        }
        else
        {
            lpInterpolator = &mLinearInterpolator;
        }

        CGS_ASSERT(lpInterpolator != nullptr, "Could not find a matching interpolator");
        return lpInterpolator;
    }

    // X360 0x82850178. Bind this channel to a new keyframed animation.
    void AnimChannel::SetAnimation(AnimChannelData lAnimData)
    {
        if (mbActive)
        {
            mbActive = false;
            mpCurrentInterpolator = nullptr;
        }

        mpCurrentInterpolator = GetInterpolator(lAnimData.meInterpolator);
        CGS_ASSERT(mpCurrentInterpolator != nullptr, "Invalid interpolator");

        mpCurrentInterpolator->SetInterpolator(lAnimData.mStartTime, lAnimData.mEndTime,
                                               lAnimData.mfStartValue, lAnimData.mfEndValue);
        mbActive = true;
    }

    // X360 0x82848C18. Advance the channel's bound interpolator to lTime and latch the value;
    // once lTime passes the interpolator's end lambda the binding is dropped.
    void AnimChannel::Update(AnimChannelData::Time lTime)
    {
        CGS_ASSERT(lTime >= 0.0f, "Invalid time specified");

        if (mpCurrentInterpolator && mbActive)
        {
            mpCurrentInterpolator->Update(lTime);
            mfCurrentValue = mpCurrentInterpolator->GetCurrentValue();
            if (lTime > mpCurrentInterpolator->GetEndLambda())
            {
                mpCurrentInterpolator = nullptr;
            }
        }
        else
        {
            mbActive = false;
        }
    }
}
