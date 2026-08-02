// wave-I partfile

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavEnterOnline.h"

namespace BrnGui
{

    // @0x824BC7C8 (cpp:1185) -- the "create an account" page. Same TOS-panel dressing as
    // ShowTOS, but with the create-account title, the create-account question body and
    // the SUBMIT/CANCEL option pair; its button prompt is the ordinary "Visible" state.
    // The question string keeps its leading '~' verbatim -- that prefix is the
    // database-text marker, not stray punctuation.
    void CrashNavEnterOnlineBase::ShowCreateAccount()
    {
        mTOSQuestion.SetText(KAC_CREATE_ACCOUNT_TITLE_STRING_ID);
        mTOSText.SetText(KAPC_LOGIN_QUESTION_STRING_ID[CgsGui::E_LOGIN_QUESTION_CREATE_ACCOUNT]);
        mTOSText.ResetScroll();          // X360 inlines the single stb 1, 0x125(field) @0x824BC808
        mTOSText.OutputAptData();

        mMessageButtons.SetupMenu(2, true);
        for (s32 li = 0; li < 2; ++li)
        {
            mMessageButtons.SetText(li, KAPC_SUBMIT_CANCEL_BUTTON_STRING_ID[li]);   // SUBMIT / CANCEL
        }

        // The six view-state pushes, in the X360's call order (@0x824BC870 onwards).
        mMessageAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);      // "Invisible"
        mShareInfoTogglesAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mSignInBackgroundAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mTOSDisplayAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[0], false);   // "Visible"
        mMessageButtonsAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[0], false);
        mButtonPromptAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[0], false);
    }

    // @0x824BCC08 (cpp:1312) -- the sign-in failed page, entered from
    // HandleDisconnectedEvent when the disconnect carried an error code but no reason
    // text. liError is the raw server-interface error the network layer reported; only
    // three of them get their own wording, everything else falls back to the generic
    // "connecting failed" line. The values are kept raw here because the producer-side
    // enum is not recovered -- the compares are the X360's own cmpwi 8 / 0xE / 0x19
    // (@0x824BCC18-0x824BCC30).
    void CrashNavEnterOnlineBase::ShowLoginError(s32 liError)
    {
        const char* lpacMessage;
        if (liError == 8)
        {
            lpacMessage = "$ONLINE_POPUP_LOBBY_DISCONNECT";
        }
        else if (liError == 14)
        {
            lpacMessage = "$ONLINE_POPUP_ACCOUNT_LOCKED";
        }
        else if (liError == 25)
        {
            lpacMessage = "$ONLINE_NOT_SIGNED_IN";
        }
        else
        {
            lpacMessage = "$ONLINE_CONNECTING_FAILED";
        }
        mMessageText.SetText(lpacMessage);

        // One button only: acknowledge and go back.
        mMessageButtons.SetupMenu(1, true);
        mMessageButtons.SetText(0, KAPC_OK_BUTTON_STRING_ID[0]);

        // The seven pushes, in the X360's call order (@0x824BCCA8 onwards) -- the
        // message panel comes up and everything else goes away. The TOS body is blanked
        // in the middle of the run exactly as the X360 does it (the empty literal is
        // unk_820046A7, a lone NUL in the pool).
        mTOSDisplayAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);   // "Invisible"
        mTOSText.SetText("");
        mShareInfoTogglesAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mSignInBackgroundAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mMessageAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[0], false);      // "Visible"
        mMessageButtonsAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[0], false);
        mButtonPromptAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[0], false);

        // Last store in the X360 (stw 5, 0x37E4) -- after all seven pushes.
        meSubState = E_SUBSTATE_LOGIN_ERROR;
    }

// ===================================================================================
// BrnGui::CrashNavEnterOnlineBase -- wave-I partfile 02: two more of the seven
// login-question "show" pages, plus the login-error page.
//   ShowTOS           @0x824BC598  cpp:1095
//   ShowCreateAccount @0x824BC7C8  cpp:1185
//   ShowLoginError    @0x824BCC08  cpp:1312
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode + asm). The
// screen carries two panels: the MESSAGE panel (mMessageText + mMessageButtons) and the
// TOS panel (the mTOSQuestion title over the scrolling mTOSText body). Each page dresses
// the panel it needs from the question-string table, re-arms the button menu from its
// own option-pair table, and then pushes one apt view state at each of the six animation
// carriers so exactly one panel ends up Visible. Every one of those six pushes was
// transcribed from its own asm -- ShowTOS and ShowCreateAccount are the TOS-panel
// flavour, ShowLoginError is the message-panel flavour, so their Visible/Invisible
// assignments are mirror images and were never copied across bodies.
//
// The button loop in the two TOS pages is the X360's two-entry pointer walk (r30
// marching over the table until it reaches the NEXT table's address, @0x824BC5FC-
// 0x824BC620 and @0x824BC830-0x824BC854): the bound is the table's own length, so it is
// written here as the plain 2-iteration index loop.
// ===================================================================================
    // @0x824BC598 (cpp:1095) -- the terms-of-service page. TOS-panel flavour: the TOS
    // question goes in the title field and the (long) agreement body into mTOSText,
    // which is rewound to the top and re-published before the panel is shown. The one
    // thing that makes this page special is its button prompt: it is the only site in
    // the class that pushes KAPC_ANIMATION_STATES[2] "Visible_tos" (off_82F26E6C), the
    // scroll-aware prompt layout, instead of the plain "Visible".
    void CrashNavEnterOnlineBase::ShowTOS()
    {
        mTOSQuestion.SetText(KAPC_LOGIN_QUESTION_STRING_ID[CgsGui::E_LOGIN_QUESTION_TOS]);
        mTOSText.SetText(KAC_TOS_TEXT_STRING_ID);   // "~TOS_TEXT" -- the '~' is the database-text marker
        mTOSText.ResetScroll();                     // X360 inlines the single stb 1, 0x125(field) @0x824BC5D4
        mTOSText.OutputAptData();

        mMessageButtons.SetupMenu(2, true);
        for (s32 li = 0; li < 2; ++li)
        {
            mMessageButtons.SetText(li, KAPC_TOS_BUTTON_STRING_ID[li]);   // ACCEPT / DECLINE
        }

        // The six view-state pushes, in the X360's call order (@0x824BC63C onwards).
        mMessageAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);      // "Invisible"
        mShareInfoTogglesAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mSignInBackgroundAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mTOSDisplayAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[0], false);   // "Visible"
        mMessageButtonsAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[0], false);
        mButtonPromptAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[2], false); // "Visible_tos"
    }
}
