// ===================================================================================
// BrnGui::FlaptHelpItem  -- apt help line (text + two button icons)
//   GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptHelpItem.cpp
//
//   Construct @ 0x8241D198
//   Prepare   @ 0x82428070
//   SetItem   @ 0x8241D338
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. All are non-static members (asm:
// r3 = this). Member access is BY NAME throughout; the X360 Begin/StrStream/Fire/End
// dev-assert sequences fold into CGS_ASSERT(cond,"msg") per the module house style.
//
// The X360 inlined the per-icon base init (state interface + invalidate the movie-clip
// handle) into Construct, and inlined the per-button "set glyph" (store the button enum
// + goto-and-stop the glyph clip on maButtonIdentifiers[button]) into SetItem; both are
// reconstructed here as direct named member access on the embedded button icons (the
// access HelpItem is granted via the friend declaration in BrnFlaptButtonIcon.h).
// ===================================================================================
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptHelpItem.h"

#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"             // BrnFlapt::FileRef
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT

namespace BrnGui
{
    // @ 0x8241D198 -- bind the state interface into the help item and both embedded
    // button icons, and invalidate every clip/text handle.
    void FlaptHelpItem::Construct(const char* lacName,
                                  CgsGui::StateInterface* lpStateInterface,
                                  const char* /*lpcParentName*/)
    {
        CGS_ASSERT(lacName != 0, "Invalid name");
        CGS_ASSERT(lpStateInterface != 0, "Invalid state interface");
        CGS_ASSERT(lpStateInterface != 0, "lpStateInterface");

        // Help item's own base (state interface + invalidate the movie-clip handle).
        mpStateInterface = lpStateInterface;
        mAptRef.mpMovieClipInst = 0;
        mAptRef.mpTransform     = 0;

        // Construct both flanking button icons (X360 inlined their base init here).
        mIconLeft.Construct(lacName, lpStateInterface, 0);
        mIconRight.Construct(lacName, lpStateInterface, 0);

        // Invalidate the text-field handle.
        mTextField.mpTextFieldInstance = 0;
        mTextField.mpParentMovie       = 0;
        mTextField.mpTransform         = 0;
    }

    // @ 0x82428070 -- bind the help item's own clip, prepare + set up both button
    // icons, then bind the "TextField" child text field.
    void FlaptHelpItem::Prepare(const char* lacName, const BrnFlapt::FileRef& lFlaptFile)
    {
        // Bind + reset this help item's own movie clip (bare-name lookup, no parent).
        BrnFlaptComponent::Prepare(lacName, lFlaptFile, 0);

        // Prepare and initialise the two flanking button icons as named child
        // components ("<lacName>_ButtonLeft" / "<lacName>_ButtonRight").
        mIconLeft.BrnFlaptComponent::Prepare("ButtonLeft", lFlaptFile, lacName);
        mIconLeft.Setup();

        mIconRight.BrnFlaptComponent::Prepare("ButtonRight", lFlaptFile, lacName);
        mIconRight.Setup();

        // Bind the help item's "TextField" child text field.
        mAptRef.FindChildTextField(&mTextField, "TextField");
    }

    // The child button-icon component names (the @0x82428188 assert text names the
    // original KAC_ constants; the Prepare(name,file) overload above binds the same
    // children as "<name>_ButtonLeft"/"<name>_ButtonRight" composites).
    static const char* const KAC_ICONLEFT  = "ButtonLeft";
    static const char* const KAC_ICONRIGHT = "ButtonRight";

    // @ 0x82428188 -- rebind the help item onto an already-located movie clip:
    // adopt lpMovieClipRef as this item's own clip (the base Prepare, with its
    // NULL + timeline-reset asserts), recursively locate the two child button-icon
    // clips under it, adopt + set up both icons, then bind the "TextField" child.
    void FlaptHelpItem::Prepare(const BrnFlapt::MovieClipRef* lpMovieClipRef)
    {
        BrnFlaptComponent::Prepare(lpMovieClipRef);

        BrnFlapt::MovieClipRef lLeftButtonMovieClipRef;
        const bool lbFoundLeft = lpMovieClipRef->TryFindChildComponentRecursively(
            KAC_ICONLEFT, &lLeftButtonMovieClipRef);
        CGS_ASSERT(lbFoundLeft,
                   "true == lpMovieClipRef->TryFindChildComponentRecursively( KAC_ICONLEFT, &lLeftButtonMovieClipRef )");

        BrnFlapt::MovieClipRef lRightButtonMovieClipRef;
        const bool lbFoundRight = lpMovieClipRef->TryFindChildComponentRecursively(
            KAC_ICONRIGHT, &lRightButtonMovieClipRef);
        CGS_ASSERT(lbFoundRight,
                   "true == lpMovieClipRef->TryFindChildComponentRecursively( KAC_ICONRIGHT, &lRightButtonMovieClipRef )");

        mIconLeft.BrnFlaptComponent::Prepare(&lLeftButtonMovieClipRef);
        mIconLeft.Setup();

        mIconRight.BrnFlaptComponent::Prepare(&lRightButtonMovieClipRef);
        mIconRight.Setup();

        mAptRef.FindChildTextField(&mTextField, "TextField");
    }

    // @ 0x8241D338 -- set the help text and both flanking button glyphs.
    void FlaptHelpItem::SetItem(const char* lpText,
                                FlaptButtonIconComponent::EPadButton leButtonLeft,
                                FlaptButtonIconComponent::EPadButton leButtonRight,
                                bool /*lbRemapButtons*/)
    {
        CGS_ASSERT(lpText != 0, "Invalid text string");
        // NOTE: matches the X360 asm exactly (0x8241D3DC-0x8241D3EC): the first
        // range check mixes leButtonLeft's sign test with leButtonRight's upper
        // bound (no upper-bound test on leButtonLeft here); the second check
        // below re-tests leButtonRight fully. This looks like an original-source
        // quirk, not a decompiler artifact -- reproduced verbatim.
        CGS_ASSERT(static_cast<s32>(leButtonLeft) >= 0
                       && static_cast<s32>(leButtonRight) < FlaptButtonIconComponent::E_PADBUTTON_COUNT,
                   "Invalid button state");
        CGS_ASSERT(static_cast<s32>(leButtonRight) >= 0
                       && static_cast<s32>(leButtonRight) < FlaptButtonIconComponent::E_PADBUTTON_COUNT,
                   "Invalid button state");

        // Left glyph: remember the button and stop its clip on the matching label.
        mIconLeft.meButton = leButtonLeft;
        mIconLeft.mAptButtonRef.GotoAndStopLabel(
            FlaptButtonIconComponent::maButtonIdentifiers[leButtonLeft]);

        // Right glyph.
        mIconRight.meButton = leButtonRight;
        mIconRight.mAptButtonRef.GotoAndStopLabel(
            FlaptButtonIconComponent::maButtonIdentifiers[leButtonRight]);

        // The help text (literal, not localised here).
        mTextField.SetText(lpText, false);
    }
}
