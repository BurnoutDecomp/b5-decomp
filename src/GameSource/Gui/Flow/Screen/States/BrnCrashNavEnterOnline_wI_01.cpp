// wave-I partfile

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavEnterOnline.h"

namespace BrnGui
{

    // @0x824BCA08 (cpp:1249) -- the "no agreement in place" page. Same message-panel
    // skeleton as ShowSignIn, with the no-agreement question text and the
    // RETRY/CANCEL option pair.
    void CrashNavEnterOnlineBase::ShowNoAgreement()
    {
        mMessageText.SetText(KAPC_LOGIN_QUESTION_STRING_ID[CgsGui::E_LOGIN_QUESTION_NO_AGREEMENT]);

        mMessageButtons.SetupMenu(2, true);
        for (s32 li = 0; li < 2; ++li)
        {
            mMessageButtons.SetText(li, KAPC_RETRY_CANCEL_BUTTON_STRING_ID[li]);
        }

        // The six view-state pushes, in the X360's call order (@0x824BCA94 onwards).
        mTOSDisplayAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);   // "Invisible"
        mTOSText.SetText("");
        mShareInfoTogglesAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mSignInBackgroundAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mMessageAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[0], false);      // "Visible"
        mMessageButtonsAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[0], false);
        mButtonPromptAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[0], false);
    }

    // @0x824BC8E8 (cpp:1217) -- the "you must open your account in the US" page. This
    // is the TOS-panel flavour: the create-account title goes into mTOSQuestion and the
    // question body into the scrolling mTOSText, which is rewound to the top and
    // re-published before the panel is shown. The question string keeps its leading '~'
    // verbatim -- that prefix is the database-text marker, not stray punctuation.
    void CrashNavEnterOnlineBase::ShowOpenAccountInUS()
    {
        mTOSQuestion.SetText(KAC_CREATE_ACCOUNT_TITLE_STRING_ID);
        mTOSText.SetText(KAPC_LOGIN_QUESTION_STRING_ID[CgsGui::E_LOGIN_QUESTION_OPEN_US_ACCOUNT]);
        mTOSText.ResetScroll();          // X360 inlines the single stb 1, 0x125(field)
        mTOSText.OutputAptData();

        mMessageButtons.SetupMenu(2, true);
        for (s32 li = 0; li < 2; ++li)
        {
            mMessageButtons.SetText(li, KAPC_YES_NO_BUTTON_STRING_ID[li]);
        }

        // The six view-state pushes, in the X360's call order (@0x824BC998 onwards).
        // Mirror image of ShowSignIn: the message panel hides, the TOS panel shows.
        mMessageAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);      // "Invisible"
        mShareInfoTogglesAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mSignInBackgroundAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mTOSDisplayAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[0], false);   // "Visible"
        mMessageButtonsAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[0], false);
        mButtonPromptAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[0], false);
    }

// ===================================================================================
// BrnGui::CrashNavEnterOnlineBase -- wave-I partfile 01: three of the seven
// login-question "show" pages.
//   ShowSignIn           @0x824BC6B8  cpp:1127
//   ShowOpenAccountInUS  @0x824BC8E8  cpp:1217
//   ShowNoAgreement      @0x824BCA08  cpp:1249
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode + asm). All
// three are the same page-dressing skeleton the sign-in flow runs when it is told to
// put a login question on screen: fill the text field(s) from the question string
// table, re-arm the two-row button menu from the matching option-pair table, then push
// one apt view state at each of the six animation carriers so exactly the panel this
// question needs is Visible and the rest are Invisible.
//
// The two panels are the MESSAGE panel (mMessageText + mMessageButtons) and the TOS
// panel (mTOSQuestion title + mTOSText body, which scrolls). ShowSignIn and
// ShowNoAgreement dress the message panel and blank the TOS body; ShowOpenAccountInUS
// is the TOS-panel flavour, so its Visible/Invisible assignment is the mirror image --
// each of the six pushes below was transcribed from its own asm, never copied across
// the three bodies.
//
// The button loop is the X360's two-entry pointer walk (r30 marching over the table
// until it reaches the NEXT table's address, @0x824BC700-0x824BC720): the loop bound is
// the table's own length, so it is written here as the plain 2-iteration index loop.
// ===================================================================================
    // @0x824BC6B8 (cpp:1127) -- the "you are not signed in, sign in now?" page.
    // Message panel with the YES/NO option pair; the TOS body is blanked (the X360
    // pushes the empty literal at unk_820046A7, a lone NUL in the pool) and the TOS,
    // share-info and sign-in-background carriers all go Invisible.
    void CrashNavEnterOnlineBase::ShowSignIn()
    {
        mMessageText.SetText(KAPC_LOGIN_QUESTION_STRING_ID[CgsGui::E_LOGIN_QUESTION_SHOW_SIGN_IN]);

        mMessageButtons.SetupMenu(2, true);
        for (s32 li = 0; li < 2; ++li)
        {
            mMessageButtons.SetText(li, KAPC_YES_NO_BUTTON_STRING_ID[li]);
        }

        // The six view-state pushes, in the X360's call order (@0x824BC744 onwards).
        mTOSDisplayAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);   // "Invisible"
        mTOSText.SetText("");
        mShareInfoTogglesAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mSignInBackgroundAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mMessageAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[0], false);      // "Visible"
        mMessageButtonsAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[0], false);
        mButtonPromptAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[0], false);
    }
}
