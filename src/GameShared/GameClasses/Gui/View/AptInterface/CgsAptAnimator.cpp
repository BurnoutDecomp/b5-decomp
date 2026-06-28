#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAnimator.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsGui::Animator - apt display-object animation driver.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (CgsAptAnimator.cpp):
//   Construct            @ 0x82858F58
//   Destruct             @ 0x82850280
//   Update               @ 0x828534E8
//   SetAnimationChannel  @ 0x82853678
//   SetAnimation         @ 0x828538D0
//   SetAptValues         @ 0x828539E8
//
// X360 idioms de-optimized here:
//   - The Begin/Fire/EndAssert triplet + plain string folds to CGS_ASSERT(cond,"msg").
//   - Construct's inlined `new ObjectController` (AptExtObject::operator new(28) +
//     AptExtObject ctor + the four field stores + vtable install) is restored to a
//     plain `new ObjectController()`; SetControlledObject/Register are the trailing calls.
//   - Construct's "append a default AnimChannel N times" loop (the X360 builds a default
//     AnimChannel on the stack and Array::Append-copies it N times) is re-rolled.
//   - The X360 inlines AnimChannel::Stop (clear mbActive + mpCurrentInterpolator) inside
//     SetAnimation; restored to lpChannel->Stop().
//   - The Array<AnimChannel,6> live-count word the X360 reads directly (a1[59]) is read
//     through Array::GetLength() (which keeps the "used before Construct/Clear" assert).
//   - The virtual SetAptValues call at the tail of Update is a `SetAptValues()` self-call.

namespace CgsGui
{
    // CgsAptAnimator.h:127 - default channel -> ActionScript display-object property
    // name table, one fixed-width KI_MAX_CHANNEL_NAME_LEN name per AnimData::
    // AnimatorChannel (X, Y, WIDTH, HEIGHT, ROTATION, ALPHA in that enum order). These
    // are the apt/Flash MovieClip transform-property names the animator binds each
    // channel to when a caller does not supply its own name table.
    const char Animator::maDefaultChannelTypeNames[6][Animator::KI_MAX_CHANNEL_NAME_LEN] =
    {
        "_x",
        "_y",
        "_width",
        "_height",
        "_rotation",
        "_alpha",
    };

    // X360 0x82858F58.
    void Animator::Construct(GuiComponent* lpTarget,
                             const char (*lpaChannelTypeNames)[KI_MAX_CHANNEL_NAME_LEN],
                             s32 liNumChannels)
    {
        CGS_ASSERT(lpTarget != 0, "Invalid target");
        CGS_ASSERT(lpaChannelTypeNames != 0, "Invalid pointer to channel names");
        CGS_ASSERT(liNumChannels > 0 && liNumChannels <= 6, "Invalid number of channels");

        mpObjectController  = 0;
        mpTarget            = lpTarget;
        maAnimChannels.Construct();          // a1[59] = 0  (live count off the -1 sentinel)
        miNumChannels       = liNumChannels;
        mpaChannelTypeNames = lpaChannelTypeNames;

        // Fill the channel list with default (inactive) animation channels.
        for (s32 liChannel = 0; liChannel < liNumChannels; ++liChannel)
        {
            maAnimChannels.Append(AnimChannel());
        }

        // Build the apt control bridge to the target component and register it.
        mpObjectController = new ObjectController();
        if (mpObjectController != 0)
        {
            mpObjectController->SetControlledObject(mpTarget);
        }
        mpObjectController->Register();
    }

    // X360 0x82850280.
    void Animator::Destruct()
    {
        CGS_ASSERT(mpObjectController != 0, "Must have valid ObjectController first");

        mpObjectController->UnRegister();
        mpObjectController = 0;
    }

    // X360 0x828534E8.
    void Animator::Update(AnimChannelData::Time lTime)
    {
        CGS_ASSERT(mpTarget != 0, "No valid target set");

        for (u32 luChannel = 0; luChannel < maAnimChannels.GetLength(); ++luChannel)
        {
            AnimChannel* lpChannel = &maAnimChannels.GetItem(luChannel);
            CGS_ASSERT(lpChannel != 0, "Invalid animation channel");
            lpChannel->Update(lTime);
        }

        SetAptValues();
    }

    // X360 0x82853678.
    void Animator::SetAnimationChannel(AnimData::AnimatorChannel eChannel,
                                       const AnimChannelData* lpChannelData,
                                       AnimChannelData::Time lStartTime)
    {
        CGS_ASSERT(lpChannelData != 0, "Invalid channel data");

        AnimChannel* lpChannel = &maAnimChannels.GetItem(static_cast<u32>(eChannel));
        CGS_ASSERT(lpChannel != 0, "Invalid channel");

        // Copy the keyframe payload and slide its key times to start at lStartTime.
        AnimChannelData lLocalData = *lpChannelData;
        lLocalData.mStartTime += lStartTime;
        lLocalData.mEndTime   += lStartTime;

        // For object-space modes, fold the object's current variable value into the
        // matching endpoint(s) so the animation is relative to where the object is now.
        const f32 lfCurrentValue =
            mpObjectController->GetObjectVariableFloat(&mpaChannelTypeNames[eChannel][0]);

        switch (lpChannelData->meAnimType)
        {
            case AnimChannelData::E_ANIMATION_SCREEN_TO_SCREEN_SPACE:
                break;
            case AnimChannelData::E_ANIMATION_SCREEN_TO_OBJECT_SPACE:
                lLocalData.mfEndValue += lfCurrentValue;
                break;
            case AnimChannelData::E_ANIMATION_OBJECT_TO_SCREEN_SPACE:
                lLocalData.mfStartValue += lfCurrentValue;
                break;
            case AnimChannelData::E_ANIMATION_OBJECT_TO_OBJECT_SPACE:
                lLocalData.mfStartValue += lfCurrentValue;
                lLocalData.mfEndValue   += lfCurrentValue;
                break;
            default:
                CGS_ASSERT(false, "Unhandled animation type");
                break;
        }

        lpChannel->SetAnimation(lLocalData);
    }

    // X360 0x828538D0.
    void Animator::SetAnimation(AnimData* lpAnimData, AnimChannelData::Time lStartTime)
    {
        CGS_ASSERT(lpAnimData != 0, "Invalid AnimData pointer");

        for (s32 liChannel = 0; liChannel < AnimData::ANIMATOR_CHANNEL_MAX; ++liChannel)
        {
            AnimChannelData* lpChannelData = lpAnimData->GetChannelData(liChannel);
            AnimChannel* lpChannel = &maAnimChannels.GetItem(static_cast<u32>(liChannel));

            if (lpChannel->IsActive())
            {
                lpChannel->Stop();
            }

            if (lpChannelData != 0)
            {
                SetAnimationChannel(static_cast<AnimData::AnimatorChannel>(liChannel),
                                    lpChannelData, lStartTime);
            }
        }
    }

    // X360 0x828539E8.
    void Animator::SetAptValues()
    {
        for (u32 luChannel = 0; luChannel < maAnimChannels.GetLength(); ++luChannel)
        {
            AnimChannel* lpChannel = &maAnimChannels.GetItem(luChannel);
            CGS_ASSERT(lpChannel != 0, "Invalid animation channel");

            if (lpChannel->IsActive())
            {
                SetAptValue(&mpaChannelTypeNames[luChannel][0], lpChannel->GetCurrentValue());
            }
        }
    }
}
