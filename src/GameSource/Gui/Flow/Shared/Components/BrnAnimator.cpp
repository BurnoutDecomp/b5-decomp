#include "GameSource/Gui/Flow/Shared/Components/BrnAnimator.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnGui::Animator -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (1 ledger function, DWARF primary file
// GameSource/Gui/Flow/Shared/Components/BrnAnimator.cpp):
//   Animator::Construct @0x824EA318  (called by BrnGui::HelpBar::Construct)
//
// X360 asm walk: assert the target ("Invalid target", cpp:40 -- the StrStream message
// build folds to CGS_ASSERT per project convention; the assert does NOT early-out), then
//   CgsGui::Animator::Construct(target, &unk_820E08EC, 6)
//     -- unk_820E08EC is the base's maDefaultChannelTypeNames table (6 apt transform
//        property names), so the call binds the default name table.
//   zero this+0x250 / this+0x284 -- the two library Arrays' live-count words
//     (Array<AnimData,2> count after the 0xF0 base; Array<AnimChannelData,2> count after
//      it) == mAnimationLibrary.Construct() / mChannelAnimationLibrary.Construct().
//   build the slide-in animation: AnimChannelData{0s..5s, 0 -> 640, linear,
//     screen-to-screen} (rodata flt_82001CC0 = 0.0 / flt_8200426C = 5.0 / flt_82057930 =
//     640.0, read from the decrypted XEX) added to the X and Y animator channels of a
//     fresh AnimData, then both pushed into the libraries;
//   rebuild for the slide-out (640 -> 0) and push that pair too.

namespace BrnGui
{
namespace
{
    // The slide pair's keyframe constants (X360 rodata, verified against the XEX image).
    const CgsGui::AnimChannelData::Time KF_SLIDE_START_TIME = 0.0f;   // flt_82001CC0
    const CgsGui::AnimChannelData::Time KF_SLIDE_END_TIME   = 5.0f;   // flt_8200426C
    const f32                           KF_SLIDE_OFFSCREEN  = 640.0f; // flt_82057930 (screen width)
}

// @ 0x824EA318
void Animator::Construct(CgsGui::GuiComponent* lpTarget)
{
    CGS_ASSERT(lpTarget != 0, "Invalid target");

    // Bind the base animator to the target with the default apt transform-property
    // name table (all six animator channels).
    CgsGui::Animator::Construct(lpTarget, CgsGui::Animator::maDefaultChannelTypeNames,
                                CgsGui::AnimData::ANIMATOR_CHANNEL_MAX);

    // Reset the two prebuilt-animation libraries (X360: the two count-word zero stores).
    mAnimationLibrary.Construct();
    mChannelAnimationLibrary.Construct();

    CgsGui::AnimData        lAnimData;       // ctor seeds the -1 unconstructed sentinels
    CgsGui::AnimChannelData lChannelData;
    lChannelData.Construct();

    // ---- ANIM_IN: slide on-screen, 0 -> 640 over 5s on the X and Y channels. ----
    lAnimData.Construct();
    lChannelData.SetData(KF_SLIDE_START_TIME, KF_SLIDE_END_TIME,
                         0.0f, KF_SLIDE_OFFSCREEN,
                         CgsGui::AnimChannelData::E_INTERPOLATE_LINEAR,
                         CgsGui::AnimChannelData::E_ANIMATION_SCREEN_TO_SCREEN_SPACE);
    lAnimData.AddAnimationChannel(CgsGui::AnimData::ANIMATOR_CHANNEL_X, lChannelData);
    lAnimData.AddAnimationChannel(CgsGui::AnimData::ANIMATOR_CHANNEL_Y, lChannelData);
    AddAnimationChannelToLibrary(&lChannelData);
    AddAnimationToLibrary(&lAnimData);

    // ---- ANIM_OUT: slide off-screen, 640 -> 0 with the same timing. ----
    lChannelData.SetData(KF_SLIDE_START_TIME, KF_SLIDE_END_TIME,
                         KF_SLIDE_OFFSCREEN, 0.0f,
                         CgsGui::AnimChannelData::E_INTERPOLATE_LINEAR,
                         CgsGui::AnimChannelData::E_ANIMATION_SCREEN_TO_SCREEN_SPACE);
    lAnimData.Construct();
    lAnimData.AddAnimationChannel(CgsGui::AnimData::ANIMATOR_CHANNEL_X, lChannelData);
    lAnimData.AddAnimationChannel(CgsGui::AnimData::ANIMATOR_CHANNEL_Y, lChannelData);
    AddAnimationChannelToLibrary(&lChannelData);
    AddAnimationToLibrary(&lAnimData);
}
}
