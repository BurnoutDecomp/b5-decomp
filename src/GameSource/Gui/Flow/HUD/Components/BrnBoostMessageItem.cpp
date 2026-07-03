#include "GameSource/Gui/Flow/HUD/Components/BrnBoostMessageItem.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                   // CGS_ASSERT
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"                                    // BrnFlapt::FileRef
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponentUtils.h"    // BrnGui::AttachToTextFieldComponent

// BrnGui::BoostMessageItem -- reconstructed from BURNOUT_X360_ARTIST.XEX (DWARF
// primary file GameSource/Gui/Flow/HUD/Components/BrnBoostMessageItem.cpp).
//
// Bodied here (5 ledger functions):
//   Construct @0x82411A18   Prepare @0x82420050   SetText @0x82411B00
//   SetTransition @0x82420158                     UpdateBoostType @0x82411BD0

namespace BrnGui
{

namespace
{
    const s32 KI_STRING_ID_LOOKUP  = 9;    // the id-lookup StringIdType both SetText paths pass
    const s32 KI_FORMAT_INTEGER    = 11;   // E_FORMAT_INTEGER (the boost-amount parameter format)
}

const char* const BoostMessageItem::KAPC_TRANSITION_FRAMES[3] =
{
    "TransitionIn", "TransitionOut", "Reset",             // XEX .data @0x82F24988
};
const char* const BoostMessageItem::KAPC_BOOST_TYPE_FRAMES[3] =
{
    "danger", "aggression", "stunt",                      // XEX .data @0x82F24994
};

// @ 0x82411A18 -- cpp:69. The streamed "Invalid state interface" tripwire fires
// ahead of the base Construct's own h:113 one (both non-gating on the X360);
// then the field/backing invalidations and the latch seeds.
void BoostMessageItem::Construct(const char* /*lpacName*/, CgsGui::StateInterface* lpStateInterface,
                                 const char* /*lpacParentName*/)
{
    CGS_ASSERT(lpStateInterface != 0, "Invalid state interface");   // :69 (streamed; folded)
    BrnFlaptComponent::Construct(lpStateInterface);                 // the h:113 tripwire + iface store + clip clear
    mMessageText.SetInvalid();
    mBackingRef.SetInvalid();
    mbInTransition = false;
    meCurrentBoostType     = -1;
}

// @ 0x82420050 -- the base Prepare (find + bind + the MovieClipRef.h:272 tripwire
// + timeline reset; no parent prefix), then the text field under
// "MessageTextField" within this component's name and the "backing_mc" child.
void BoostMessageItem::Prepare(const char* lpacName, const BrnFlapt::FileRef& lrFile)
{
    BrnFlaptComponent::Prepare(lpacName, lrFile, 0);   // inlined on the X360

    BrnFlapt::TextFieldRef lTextField;
    mMessageText = *AttachToTextFieldComponent(
        &lTextField, "BoostMessage_text", "MessageTextField", lpacName, lrFile);

    BrnFlapt::MovieClipRef lBacking;
    mBackingRef = *mAptRef.FindChildMovieClip(&lBacking, "backing_mc");
}

// @ 0x82411B00 -- cpp:179. Localised text with the boost amount as the single
// integer parameter when non-negative, else the plain id lookup.
void BoostMessageItem::SetText(const char* lpcText, s32 liBoostAmount)
{
    CGS_ASSERT(lpcText != 0, "Invalid text pointer");   // :179 (streamed; folded)

    if (liBoostAmount >= 0)
        mMessageText.SetLocalisedText(lpcText, KI_STRING_ID_LOOKUP,
                                           liBoostAmount, KI_FORMAT_INTEGER);   // @0x8246D2B0
    else
        mMessageText.SetLocalisedText(lpcText, KI_STRING_ID_LOOKUP);
}

// @ 0x82420158 -- cpp:211/:213. Both tripwires streamed on the X360 (folded
// static, non-gating -- the transition still plays); then the latch + the frame
// + the backing refresh.
void BoostMessageItem::SetTransition(u32 luTransition, s32 leBoostType)
{
    CGS_ASSERT(luTransition <= 2, "Unrecognised transition type");                            // :211
    CGS_ASSERT(!mbInTransition,
               "Should not transition when transition already in progress");                  // :213

    mbInTransition = true;
    mAptRef.GotoAndPlayLabel(KAPC_TRANSITION_FRAMES[luTransition]);   // @0x8246F3E8
    UpdateBoostType(leBoostType);
}

// @ 0x82411BD0 -- cpp:268/:269. The bounds asserts keep the original BrnWorld
// enum spelling; -1 leaves the backing untouched.
void BoostMessageItem::UpdateBoostType(s32 leBoostType)
{
    CGS_ASSERT(leBoostType >= -1, "leBoostType >= BrnWorld::E_BOOST_TYPE_NONE");   // :268 (non-gating)
    CGS_ASSERT(leBoostType < 3,   "leBoostType < BrnWorld::E_BOOST_TYPE_COUNT");   // :269 (non-gating)

    if (leBoostType != -1 && leBoostType != meCurrentBoostType)
    {
        meCurrentBoostType = leBoostType;
        mBackingRef.GotoAndStopLabel(KAPC_BOOST_TYPE_FRAMES[leBoostType]);   // @0x8246F498
    }
}

}
