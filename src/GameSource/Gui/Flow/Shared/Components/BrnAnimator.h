#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsArray.h"                      // Array<T,N>
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAnimator.h"     // CgsGui::Animator (base)
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAnimData.h"     // CgsGui::AnimData / AnimChannelData

// BrnGui::Animator - the game-side apt animator: a CgsGui::Animator plus a small
// library of prebuilt animations (the slide-in / slide-out pair Construct seeds).
// DWARF home BrnAnimator.h:43; member/method set from the DecFIGS DWARF, gated on
// the X360 ledger. Construct is bodied in BrnAnimator.cpp (this TU); the Play* /
// AddAnimation*ToLibrary members are their own ledger functions (declaration-only
// here).
namespace BrnGui
{
    struct Animator : public CgsGui::Animator
    {
        // DWARF BrnAnimator.h:46 / :54.
        enum BrnGuiAnimation
        {
            ANIM_IN  = 0,
            ANIM_OUT = 1,
            ANIM_MAX = 2,
        };

        enum BrnGuiAnimationChannel
        {
            ANIM_CHANNEL_IN  = 0,
            ANIM_CHANNEL_OUT = 1,
            ANIM_CHANNEL_MAX = 2,
        };

        // @0x824EA318 (this TU, DWARF cpp:38) -- bind the base animator to the target
        // component with the default channel-name table, then seed the animation library
        // with the slide-in (0 -> 640 over 5s) and slide-out (640 -> 0) pair.
        void Construct(CgsGui::GuiComponent* lpTarget);

        // DWARF cpp:91 / cpp:109 / cpp:130 -- declaration-only (their own ledger TUs).
        void PlayAnimationChannel(CgsGui::AnimData::AnimatorChannel leChannel,
                                  BrnGuiAnimationChannel leLibraryChannel,
                                  CgsGui::AnimChannelData::Time lStartTime);
        void PlayAnimation(BrnGuiAnimation leAnimation, CgsGui::AnimChannelData::Time lStartTime);
        void PlayAnimationChannel(CgsGui::AnimData::AnimatorChannel leChannel,
                                  f32 lfStartValue, f32 lfEndValue,
                                  CgsGui::AnimChannelData::Time lStartTime,
                                  CgsGui::AnimChannelData::Time lEndTime,
                                  CgsGui::AnimChannelData::InterpolateType leInterpolator,
                                  CgsGui::AnimChannelData::AnimationType leAnimType);

    protected:
        // DWARF cpp:165 / cpp:147 -- append to the respective library, returning the
        // slot index. Declaration-only (their own ledger TUs).
        u32 AddAnimationToLibrary(const CgsGui::AnimData* lpAnimData);
        u32 AddAnimationChannelToLibrary(const CgsGui::AnimChannelData* lpChannelData);

    private:
        // DWARF BrnAnimator.h:105.
        static const s32 ANIMATOR_LIBRARY_SIZE = 2;

        // DWARF h:107 / h:108 (X360: counts zeroed by Construct at +0x250 / +0x284,
        // matching Array<AnimData,2> after the 0xF0 base and Array<AnimChannelData,2>
        // right behind it).
        Array<CgsGui::AnimData, ANIMATOR_LIBRARY_SIZE>        mAnimationLibrary;
        Array<CgsGui::AnimChannelData, ANIMATOR_LIBRARY_SIZE> mChannelAnimationLibrary;
    };
}
