#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsArray.h"   // Array<T,N> (mChannelData / meChannels)

// CgsGui::AnimChannelData - one keyframed animation channel for the APT (gui movie)
// interface: a start/end time pair, a start/end value pair, and the interpolation /
// animation-space mode. Recovered from the X360 spine (Construct @0x82848E00,
// SetData @0x82848E30); member set, order and the InterpolateType / AnimationType
// enums are from the DecFIGS DWARF (CgsAptAnimData.h). This is the minimal owning
// slice for the two bodied channel methods; the higher-level AnimData container that
// aggregates six of these is homed by its own TU.
namespace CgsGui
{
    struct AnimChannelData
    {
        typedef f32 Time;

        enum InterpolateType
        {
            E_INTERPOLATE_LINEAR = 0,
            E_INTERPOLATE_MAX    = 1,
        };

        enum AnimationType
        {
            E_ANIMATION_SCREEN_TO_SCREEN_SPACE = 0,
            E_ANIMATION_SCREEN_TO_OBJECT_SPACE = 1,
            E_ANIMATION_OBJECT_TO_SCREEN_SPACE = 2,
            E_ANIMATION_OBJECT_TO_OBJECT_SPACE = 3,
            E_ANIMATION_MAX                    = 4,
        };

        Time            mStartTime;       // +0
        Time            mEndTime;         // +4
        f32             mfStartValue;     // +8
        f32             mfEndValue;       // +12
        InterpolateType meInterpolator;   // +16
        AnimationType   meAnimType;       // +20

        void Construct();
        void SetData(Time lStartTime, Time lEndTime, f32 lfStartValue, f32 lfEndValue,
                     InterpolateType leInterpolator, AnimationType leAnimType);
    };

    // CgsGui::AnimData - a single APT animation: up to six keyframed channel-data
    // entries (one per animatable property) plus the list of which animator channels
    // are driven. Recovered from the X360 spine:
    //   AnimData::AnimData            @ 0x824E8FC8  (ctor: stores the -1 unconstructed
    //                                                sentinel into the two embedded
    //                                                Array count words @ +0x90 / +0xAC)
    //   AnimData::AnimatorChan        @ 0x8284D940  (Array<AnimatorChannel,6>::operator[]:
    //                                                array-local count @ +0x18, stride 4)
    //   Array<AnimData,2>::Append     @ 0x824E5BD0  (outer container memcpy's 0xB0 == 176)
    // Member set / order / names and the AnimatorChannel enum are from the DecFIGS
    // DWARF (CgsAptAnimData.h):
    //   mChannelData  [+0x00]  Array<AnimChannelData,6>   (h:137; 6*0x18 buf + count @ +0x90)
    //   meChannels    [+0x94]  Array<AnimatorChannel,6>   (h:138; 6*4 buf + count @ +0xAC)
    // sizeof == 0xB0 (176): 0x94 (Array<AnimChannelData,6>) + 0x1C (Array<AnimatorChannel,6>),
    // matching the Array<AnimData,2>::Append element memcpy size of 176.
    struct AnimData
    {
        // CgsAptAnimData.h:104 - which screen-property channel a keyframe track drives.
        enum AnimatorChannel
        {
            ANIMATOR_CHANNEL_X        = 0,
            ANIMATOR_CHANNEL_Y        = 1,
            ANIMATOR_CHANNEL_WIDTH    = 2,
            ANIMATOR_CHANNEL_HEIGHT   = 3,
            ANIMATOR_CHANNEL_ROTATION = 4,
            ANIMATOR_CHANNEL_ALPHA    = 5,
            ANIMATOR_CHANNEL_MAX      = 6,
        };

        Array<AnimChannelData, 6> mChannelData;  // [+0x00] keyframe payload per active channel (h:137)
        Array<AnimatorChannel, 6> meChannels;    // [+0x94] which channels this animation drives (h:138)

        // ----- methods homed by class:CgsGui::AnimData (CgsAptAnimDataAnimData.cpp) -----

        // Default ctor (X360 0x824E8FC8): leaves both embedded Array<> members in the
        // pre-Construct state -- the X360 body stores the KI_UNCONSTRUCTED(-1) sentinel
        // into mChannelData.miCount (@+0x90) and meChannels.miCount (@+0xAC). It does
        // NOT zero/Construct them; a later Construct/Clear flips the sentinels.
        AnimData();

        // X360 0x8284D940 (AnimData::AnimatorChan): checked indexed access into the
        // meChannels list -- returns a reference to the AnimatorChannel at luIndex
        // (the X360 inlined Array<AnimatorChannel,6>::operator[] here: count @ +0x18 of
        // the array subobject, stride 4, with the CgsArray.h:538/539 bounds asserts).
        // Called by AnimData::GetChannelData.
        AnimatorChannel&       AnimatorChan(u32 luIndex);
        const AnimatorChannel& AnimatorChan(u32 luIndex) const;

        // X360 AnimData::GetChannelData: return the keyframe payload for the animator
        // channel at luIndex, or null when this animation does not drive that channel.
        // Walked by CgsGui::Animator::SetAnimation across all six animator channels.
        // Bodied by its own ledger TU; declared here for the Animator caller surface.
        AnimChannelData* GetChannelData(s32 liChannel);

        // ADDITIVE GROW (BrnGui::Animator::Construct @0x824EA318): the two builder
        // methods that caller drives. Bodies land with their own ledger TUs; the X360
        // caller's ABI pins the shapes: Construct takes no args (re-initialises both
        // embedded Arrays); AddAnimationChannel takes the target animator channel and
        // the 24-byte AnimChannelData BY VALUE (the X360 passes it in r5/r6/r7 as three
        // 64-bit register pairs).
        void Construct();
        void AddAnimationChannel(AnimatorChannel leChannel, AnimChannelData lChannelData);
    };
}
