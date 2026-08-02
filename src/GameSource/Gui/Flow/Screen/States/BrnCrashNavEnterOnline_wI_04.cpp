// wave-I partfile

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavEnterOnline.h"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"               // CgsGui::ELoginQuestion
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event

namespace BrnGui
{

// ===================================================================================
// BrnGui::CrashNavEnterOnlineBase -- wave-I partfile 04: answering the login question,
// and the two controller-input handlers that drive the single-menu question pages.
//   AnswerLoginQuestion       @0x824CAE00  cpp:1068
//   HandleLoginInput          @0x824D85F0  cpp:606
//   HandleLoginInputShowSignIn@0x824D8720  cpp:658
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode + asm).
//
// AnswerLoginQuestion is the one exit every question page shares: it publishes the
// player's answer on the GUI out-queue, arms the "this disconnect is expected, don't
// popup" flag for the one question whose refusal deliberately drops the connection,
// forgets which question was on screen and drops the screen back into its connecting
// sub-state with every component hidden.
//
// The two handlers are the input front-ends for the pages that carry a single two-row
// button menu (mMessageButtons): the plain question pages go through HandleLoginInput,
// the "you are not signed in" page through HandleLoginInputShowSignIn, which answers
// YES by opening the platform sign-in UI instead of posting an answer. Both compile on
// the X360 to the same 10-entry jump table over (action - 41); only ids 41/42/49/50 have
// arms, everything in between (43..48) and outside the span falls through the default
// and is consumed.
// ===================================================================================
    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) --------------------------
        const s32 KI_CHANNEL_GUI_OUT = 40;

        // ---- controller action ids (the payload's second word) -----------------------
        // The same four ids the sibling question handler consumes
        // (BrnCarSelectLivery_Input.cpp names the identical set).
        const s32 KI_ACTION_TOGGLE_PREV = 41;   // 0x29 -- step the button menu backwards
        const s32 KI_ACTION_TOGGLE_NEXT = 42;   // 0x2A -- step the button menu forwards
        const s32 KI_ACTION_ACCEPT      = 49;   // 0x31 -- EGameInputActions GUI_SELECT
        const s32 KI_ACTION_BACK        = 50;   // 0x32 -- GUI_CANCEL

        // ---- in-queue payload view ----------------------------------------------------
        // ARTIST controller event payload: pad id at +0x00, the button/action id at +0x04.
        // Only the action word is read here (`lwz r11, 4(r4)` @0x824D8600 / @0x824D8730).
        struct ControllerInputPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miButtonId;   // +0x04
        };

        // ---- out-queue wire records ---------------------------------------------------
        // Both header words are HOST expressions, never the console's baked immediates:
        // the first is the host size of the payload tail, the second the host size of the
        // three-word GuiEvent header (12 on the X360, and 12 here too because
        // CgsModule::Event is empty -- written as sizeof so it tracks the host layout).

        // The answer record. X360 stack-builds { 8, 48, 12 } followed by the 8 payload
        // bytes it assembles in a scratch slot first (@0x824CAE28-0x824CAE48: stw the
        // question at +0, stb the two answers at +4 / +5, then one ld/std pair copies the
        // whole 8 bytes into the record) and AddEvent()s it with size 20 on channel 40.
        // The DWARF name for id 48 is CgsGui::GuiEventAnswerLoginQuestion.
        typedef CgsGui::GuiEvent<48> AnswerLoginQuestionHeader;

        struct GuiEventAnswerLoginQuestionWire : public AnswerLoginQuestionHeader
        {
            CgsGui::ELoginQuestion meLoginQuestion;   // +0x0C -- the question being answered
            bool                   mbAccept;          // +0x10 -- first answer (row 0 / "yes")
            bool                   mbAccept2;         // +0x11 -- second answer (share-info row 1)
            u16                    muPad12;           // +0x12 -- X360 ships the scratch slot's
                                                      //          two trailing bytes as-is; zeroed here

            GuiEventAnswerLoginQuestionWire(CgsGui::ELoginQuestion leLoginQuestion,
                                            bool lbAccept,
                                            bool lbAccept2)
                : AnswerLoginQuestionHeader(
                      static_cast<u32>(sizeof(GuiEventAnswerLoginQuestionWire)
                                       - sizeof(AnswerLoginQuestionHeader)),
                      static_cast<u32>(sizeof(AnswerLoginQuestionHeader)))
                , meLoginQuestion(leLoginQuestion)
                , mbAccept(lbAccept)
                , mbAccept2(lbAccept2)
                , muPad12(0)
            {
            }
        };

        // The "cancel the sign-in attempt" request. X360 stack-builds { 1, 49, 12 } and
        // AddEvent()s it with size 16 on channel 40 (@0x824D8690-0x824D86BC), leaving the
        // single payload byte on the stack UNWRITTEN -- so its value is whatever the frame
        // held. It is written as 0 here (a host must not publish an indeterminate byte).
        // The DWARF name for id 49 is CgsGui::GuiEventCancelLogin (CgsGuiEventTypeDefs.h:427,
        // reached through GuiEventWrapper<CgsGui::GuiEvent<49>,40>); the event carries no
        // payload of its own, which is why the payload-size word is the empty struct's 1.
        struct GuiEventCancelLoginWire : public CgsGui::GuiEventCancelLogin
        {
            u8 muPayload;   // +0x0C -- X360 leaves this byte uninitialised

            GuiEventCancelLoginWire()
                : CgsGui::GuiEventCancelLogin(
                      static_cast<u32>(sizeof(u8)),
                      static_cast<u32>(sizeof(CgsGui::GuiEventCancelLogin)))
                , muPayload(0)
            {
            }
        };
    }

    // ---- AnswerLoginQuestion @ 0x824CAE00 (cpp:1068) -------------------------------
    // Publish the player's answer to whichever login question is currently on screen and
    // hand the screen back to the connecting sub-state.
    //
    // The one piece of state that outlives the answer is mbIgnoreDisconnectedPopup: the
    // NO_AGREEMENT question is the one whose refusal is expected to tear the session down,
    // so declining it arms the flag and the disconnect that follows leaves quietly instead
    // of raising the "connection failed" overlay.
    void CrashNavEnterOnlineBase::AnswerLoginQuestion(bool lbAccept, bool lbAccept2)
    {
        // The question is captured into the record BEFORE it is reset below (the X360
        // reads this+0xC8 @0x824CAE28, well ahead of the `li r10, 7` store @0x824CAEA4).
        GuiEventAnswerLoginQuestionWire lAnswer(meCurrentLoginQuestion, lbAccept, lbAccept2);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lAnswer),
            KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lAnswer)));      // X360 record size 20

        mfTimeInState = 0.0f;                        // flt_82001CC0 == 0.0f

        // Only a DECLINED no-agreement question suppresses the disconnect popup
        // (@0x824CAE6C-0x824CAE98: question == 4 and the accept byte zero).
        mbIgnoreDisconnectedPopup =
            (meCurrentLoginQuestion == CgsGui::E_LOGIN_QUESTION_NO_AGREEMENT) && !lbAccept;

        meCurrentLoginQuestion = CgsGui::E_LOGIN_QUESTION_COUNT;   // no question on screen
        meSubState             = E_SUBSTATE_CONNECTING;

        HideAllComponents();
    }

    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) --------------------------

        // ---- controller action ids (the payload's second word) -----------------------

        // ---- in-queue payload view ----------------------------------------------------
        // ---- out-queue wire record ----------------------------------------------------
        // The "cancel the sign-in attempt" request (CgsGui::GuiEventCancelLogin, id 49) is
        // declared once above and reused here; both header words are HOST expressions,
        // never the console's baked immediates.

    }

    // ---- HandleLoginInput @ 0x824D85F0 (cpp:606) -----------------------------------
    // The ordinary question page's input: the two menu-step actions walk the button row,
    // GUI_SELECT answers the question with "did the player leave the highlight on row 0",
    // and GUI_CANCEL cancels the sign-in attempt.
    void CrashNavEnterOnlineBase::HandleLoginInput(const CgsModule::Event* lpEvent)
    {
        const s32 liAction =
            reinterpret_cast<const ControllerInputPayload*>(lpEvent)->miButtonId;

        switch (liAction)
        {
            case KI_ACTION_TOGGLE_PREV:
                mMessageButtons.HighlightPrevious();   // vtable slot 14 (lwz 0x38)
                break;

            case KI_ACTION_TOGGLE_NEXT:
                mMessageButtons.HighlightNext();       // vtable slot 13 (lwz 0x34)
                break;

            case KI_ACTION_ACCEPT:
                // Row 0 is the affirmative option on every page that lands here; the
                // second answer belongs to the share-info page and is always false.
                AnswerLoginQuestion(mMessageButtons.miHighlightedIndex == 0, false);
                break;

            case KI_ACTION_BACK:
                // The chat-restriction page has no way out but forward -- it is a notice,
                // not a question, so backing out of it is ignored entirely.
                if (meCurrentLoginQuestion != CgsGui::E_LOGIN_QUESTION_CHAT_RESTRICTION)
                {
                    GuiEventCancelLoginWire lCancelLogin;
                    mpStateInterface->GetOutputEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lCancelLogin),
                        KI_CHANNEL_GUI_OUT,
                        static_cast<s32>(sizeof(lCancelLogin)));   // X360 record size 16

                    // Backing out of the no-agreement question is the expected way to end
                    // the session, so the disconnect it causes must not raise the popup.
                    mbIgnoreDisconnectedPopup =
                        (meCurrentLoginQuestion == CgsGui::E_LOGIN_QUESTION_NO_AGREEMENT);
                }
                break;

            default:
                break;
        }
    }

    namespace
    {
        // ---- controller action ids (the payload's second word) -----------------------

        // ---- in-queue payload view ----------------------------------------------------
        // ARTIST controller event payload: pad id at +0x00, the button/action id at +0x04.
        // Only the action word is read here (`lwz r11, 4(r4)` @0x824D8730).
    }

    // ---- HandleLoginInputShowSignIn @ 0x824D8720 (cpp:658) -------------------------
    // The "you are not signed in -- sign in now?" page. Saying YES opens the platform
    // sign-in UI rather than answering the question; saying NO, and backing out, both take
    // the same exit: answer the question negatively and arm the ignore-popup flag, because
    // the sign-in that never happened is about to disconnect the session on purpose.
    //
    // The X360 reaches that shared exit by jumping from the GUI_SELECT arm straight into
    // the GUI_CANCEL arm's body (@0x824D8790 bne -> loc_824D87A8); it is written here as
    // the two ids sharing one arm with an explicit test.
    void CrashNavEnterOnlineBase::HandleLoginInputShowSignIn(const CgsModule::Event* lpEvent)
    {
        const s32 liAction =
            reinterpret_cast<const ControllerInputPayload*>(lpEvent)->miButtonId;

        switch (liAction)
        {
            case KI_ACTION_TOGGLE_PREV:
                mMessageButtons.HighlightPrevious();   // vtable slot 14 (lwz 0x38)
                break;

            case KI_ACTION_TOGGLE_NEXT:
                mMessageButtons.HighlightNext();       // vtable slot 13 (lwz 0x34)
                break;

            case KI_ACTION_ACCEPT:
            case KI_ACTION_BACK:
                if (liAction == KI_ACTION_ACCEPT && mMessageButtons.miHighlightedIndex == 0)
                {
                    // YES: clear the page and hand over to the platform sign-in UI (the
                    // base's pure virtual, X360 vtable slot 10). The X360 tail-calls it and
                    // returns its result; nothing here consumes that result.
                    HideAllComponents();
                    ShowSignInUI();
                }
                else
                {
                    AnswerLoginQuestion(false, false);
                    mbIgnoreDisconnectedPopup = true;
                }
                break;

            default:
                break;
        }
    }
}
