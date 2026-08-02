// ===================================================================================
// BrnGui::CrashNavEnterOnlineBase -- wave-I partfile 03: the show-function table and
// the share-info question (its page and its controller handling).
//   InitialiseFunctionArrays  @0x824C1748  cpp:201 (assert cpp:220)
//   ShowShareInfo             @0x824BEF68  cpp:1158
//   HandleLoginInputShareInfo @0x824D8490  cpp:547
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode + asm).
//
// InitialiseFunctionArrays builds the per-question dispatch table OnEnter installs: one
// member-function pointer per CgsGui::ELoginQuestion, which HandleShowLoginQuestion then
// calls to dress the screen for whichever question the sign-in flow asked for. The
// console stores raw function addresses into a 7-word array; on the host the same array
// is a pointer-to-member array, so the table is written and checked BY NAME and no
// console word size appears anywhere in the body.
//
// The other two are the share-info question itself -- the only login question that is a
// pair of YES/NO toggle ROWS rather than a message plus a button bar. ShowShareInfo
// re-arms the two rows and brings the toggle panel up; HandleLoginInputShareInfo walks
// the rows/options with the controller and answers the question with one accept flag per
// row. The row-index reads and the group vtable slots behind them were MEASURED, not
// inferred (see the notes on each).
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavEnterOnline.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"               // CgsGui::ELoginQuestion
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuToggle.h"          // BrnGui::MenuToggle (the group's rows)

namespace BrnGui
{
    namespace
    {
        // ---- AddEvent channel (the out-queue selector word) ---------------------------
        const s32 KI_CHANNEL_GUI_OUT = 40;

        // ---- controller action ids (the payload's second word) ------------------------
        // The X360 compiles the switch as a jump table over (action - 0x29) with a
        // ten-entry span, so cases 45..48 fall through the default with the rest.
        // FLAG: the producer-side enum for these is not in the recovered DWARF slice; they
        // are named for the role this handler gives them (49 is the GUI_SELECT action the
        // sibling screens also spell 49, 50 the matching back action).
        const s32 KI_ACTION_TOGGLE_PREV = 41;   // 0x29 -- previous toggle ROW
        const s32 KI_ACTION_TOGGLE_NEXT = 42;   // 0x2A -- next toggle ROW
        const s32 KI_ACTION_OPTION_PREV = 43;   // 0x2B -- previous OPTION within the row
        const s32 KI_ACTION_OPTION_NEXT = 44;   // 0x2C -- next OPTION within the row
        const s32 KI_ACTION_ACCEPT      = 49;   // 0x31
        const s32 KI_ACTION_BACK        = 50;   // 0x32

        // ---- in-queue payload view ----------------------------------------------------
        // ARTIST controller event payload: pad id at +0x00, the button/action id at +0x04.
        // Only the action word is read here (`lwz r11, 4(r4)` @0x824D84A4). The queue hands
        // the handler the HEADER-STRIPPED payload, which is why the parameter stays a bare
        // CgsModule::Event* and the view has no GuiEvent header in front of it.
        struct ControllerInputPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miButtonId;   // +0x04
        };

        // ---- out-queue wire record ----------------------------------------------------
        // The X360 stack-builds { 1, 49, 12 } at [sp+0x50] and AddEvent()s it on channel 40
        // with record size 16 (@0x824D8548-0x824D8574). Event 49 is
        // CgsGui::GuiEventCancelLogin -- the "cancel the sign-in attempt" request the back
        // action posts (the name is the DWARF's, CgsGuiEventTypeDefs.h:427, reached through
        // GuiEventWrapper<CgsGui::GuiEvent<49>,40>). It carries no payload of its own, which
        // is why the console's payload-size word is 1: the size of the empty struct.
        //
        // The two header words are HOST expressions -- the payload size is sizeof(the
        // payload byte) and the payload offset is the host sizeof of the 3-word GuiEvent
        // header (12 on the console, and 12 here as well because CgsModule::Event is empty,
        // but written as sizeof so it tracks the host).
        struct GuiEventCancelLoginWire : public CgsGui::GuiEventCancelLogin
        {
            u8 muPayload;   // +0x0C

            explicit GuiEventCancelLoginWire(u8 luPayload)
                : CgsGui::GuiEventCancelLogin(static_cast<u32>(sizeof(u8)),
                                              static_cast<u32>(sizeof(CgsGui::GuiEventCancelLogin)))
                , muPayload(luPayload)
            {
            }
        };
    }

    // ---- InitialiseFunctionArrays @ 0x824C1748 (cpp:201) ---------------------------
    // Fill the per-login-question show-function table. The X360 clears all seven slots
    // with a counted loop (ctr = 7 @0x824C1768), writes the seven entries, then re-walks
    // the same seven slots asserting none of them stayed null -- the belt-and-braces shape
    // that catches a question added to the enum without a page to show for it.
    void CrashNavEnterOnlineBase::InitialiseFunctionArrays()
    {
        for (s32 li = 0; li < CgsGui::E_LOGIN_QUESTION_COUNT; ++li)
        {
            maShowControlsFunc[li] = 0;
        }

        maShowControlsFunc[CgsGui::E_LOGIN_QUESTION_TOS]              = &CrashNavEnterOnlineBase::ShowTOS;
        maShowControlsFunc[CgsGui::E_LOGIN_QUESTION_CREATE_ACCOUNT]   = &CrashNavEnterOnlineBase::ShowCreateAccount;
        maShowControlsFunc[CgsGui::E_LOGIN_QUESTION_SHARE]            = &CrashNavEnterOnlineBase::ShowShareInfo;
        maShowControlsFunc[CgsGui::E_LOGIN_QUESTION_OPEN_US_ACCOUNT]  = &CrashNavEnterOnlineBase::ShowOpenAccountInUS;
        maShowControlsFunc[CgsGui::E_LOGIN_QUESTION_NO_AGREEMENT]     = &CrashNavEnterOnlineBase::ShowNoAgreement;
        maShowControlsFunc[CgsGui::E_LOGIN_QUESTION_SHOW_SIGN_IN]     = &CrashNavEnterOnlineBase::ShowSignIn;
        // The chat-restriction page is owned by a different ledger TU; only its address is
        // needed here, which is why taking it is enough.
        maShowControlsFunc[CgsGui::E_LOGIN_QUESTION_CHAT_RESTRICTION] = &CrashNavEnterOnlineBase::ShowChatRestrictedPopup;

        for (s32 li = 0; li < CgsGui::E_LOGIN_QUESTION_COUNT; ++li)
        {
            // cpp:220. Non-fatal in the X360 build -- the loop keeps walking the table.
            CGS_ASSERT(maShowControlsFunc[li] != 0,
                       "No show control function supplied for this login question");
        }
    }

    // ---- ShowShareInfo @ 0x824BEF68 (cpp:1158) -------------------------------------
    // The "may we share your info?" page. Unlike the other six questions this one is a
    // two-row toggle group: row 0 asks the first sharing question and row 1 the second,
    // each with the same YES/NO option pair. Both rows are re-armed from scratch, then
    // parked on option 1 (== NO, the second entry of the YES/NO pair) so the page opens on
    // the conservative answer; a row that actually moved is dirtied so it repaints.
    void CrashNavEnterOnlineBase::ShowShareInfo()
    {
        mShareInfoToggles.SetupGroup(2, 0);

        // r5 = 2 options, r6 = 1 active, r7 = the row caption, r8 = the option pair,
        // r9 = 0 (no id array -- the rows are answered by option INDEX, not by id).
        // The option table is const in this class but SetupToggle's owning header takes a
        // plain `const char**`, so the pointer is cast through; the console passes the
        // rodata table address straight into r8.
        mShareInfoToggles.SetupToggle(0, 2, true,
                                      KAPC_LOGIN_QUESTION_STRING_ID[CgsGui::E_LOGIN_QUESTION_SHARE],
                                      const_cast<const char**>(KAPC_YES_NO_BUTTON_STRING_ID), 0);
        // Row 1's caption is an inline literal, NOT an entry of the question table: only
        // the first share question has a table slot (the table is indexed by question, and
        // both rows belong to the one SHARE question).
        mShareInfoToggles.SetupToggle(1, 2, true, "$ONLINE_LOGIN_QUESTION_SHARE_2",
                                      const_cast<const char**>(KAPC_YES_NO_BUTTON_STRING_ID), 0);

        // Both rows come back through the BASE group's GetSelectable (the X360 calls
        // BrnGui::SelectableGroup::GetSelectable @0x824BEFE8 / @0x824BF028), so they arrive
        // as Selectable* and are the group's own MenuToggle rows. Highlighting option 1 is
        // the group vtable slot 12 call on the row's embedded option selection
        // (`(*(*(row + 0xA8) + 0x30))(row + 0xA8, 1)` -- +0xA8 IS MenuToggle::mItemText and
        // slot 12 IS SelectableGroup::HighlightIndex, both measured off off_820746F0).
        for (s32 li = 0; li < 2; ++li)
        {
            MenuToggle* lpRow = static_cast<MenuToggle*>(mShareInfoToggles.SelectableGroup::GetSelectable(li));
            if (lpRow->mItemText.HighlightIndex(1))
            {
                lpRow->SetDirty();
            }
        }

        // The seven view-state pushes, in the X360's call order (@0x824BF060 onwards): the
        // share-info toggle panel and the button prompt come up, both message panels and
        // the TOS panel go away, and the TOS body is blanked on the way past (the empty
        // literal is unk_820046A7, a lone NUL in the pool).
        mTOSDisplayAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);   // "Invisible"
        mTOSText.SetText("");
        mMessageAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mMessageButtonsAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mSignInBackgroundAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mShareInfoTogglesAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[0], false); // "Visible"
        mButtonPromptAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[0], false);
    }

    // ---- HandleLoginInputShareInfo @ 0x824D8490 (cpp:547) --------------------------
    // Controller handling while the share-info question is up. The four navigation actions
    // split two ways: 41/42 move between the two toggle ROWS, 43/44 move between the two
    // OPTIONS of whichever row is highlighted. The dispatch targets were MEASURED off the
    // group's component vtable off_820746F0 (slot 10 HighlightNext, 11 HighlightPrevious,
    // 13 HighlightNextItem, 14 HighlightPreviousItem), so the displacements the pseudocode
    // shows (0x28/0x2C/0x34/0x38) are the named methods below.
    void CrashNavEnterOnlineBase::HandleLoginInputShareInfo(const CgsModule::Event* lpEvent)
    {
        const ControllerInputPayload* lpPayload =
            reinterpret_cast<const ControllerInputPayload*>(lpEvent);

        switch (lpPayload->miButtonId)
        {
            case KI_ACTION_TOGGLE_PREV:
                mShareInfoToggles.HighlightPrevious(false);
                break;

            case KI_ACTION_TOGGLE_NEXT:
                mShareInfoToggles.HighlightNext(false);
                break;

            case KI_ACTION_OPTION_PREV:
                mShareInfoToggles.HighlightPreviousItem();
                break;

            case KI_ACTION_OPTION_NEXT:
                mShareInfoToggles.HighlightNextItem();
                break;

            case KI_ACTION_ACCEPT:
            {
                // Answer the question with one flag per row: option 0 of the YES/NO pair is
                // YES, so a row still sitting on index 0 is an accept. The X360 samples row
                // 1 FIRST (@0x824D84F8-0x824D8518) and row 0 second, then passes row 0's
                // answer in r4 and row 1's in r5 (@0x824D852C / @0x824D853C) -- the sample
                // order is kept here, the argument mapping with it.
                //
                // The index itself is the row's embedded option selection's highlight
                // cursor, read by name: MenuToggle::mItemText is at +0xA8 and
                // SelectableGroup::miHighlightedIndex at +0xA5 within it, which is exactly
                // the console's `lbz +0x14D(row)`.
                MenuToggle* lpShare2Row =
                    static_cast<MenuToggle*>(mShareInfoToggles.SelectableGroup::GetSelectable(1));
                const bool lbAccept2 = (lpShare2Row->mItemText.miHighlightedIndex == 0);

                MenuToggle* lpShare1Row =
                    static_cast<MenuToggle*>(mShareInfoToggles.SelectableGroup::GetSelectable(0));
                const bool lbAccept = (lpShare1Row->mItemText.miHighlightedIndex == 0);

                AnswerLoginQuestion(lbAccept, lbAccept2);
                break;
            }

            case KI_ACTION_BACK:
            {
                // Backing out of the share-info question cancels the sign-in attempt. The
                // console never writes the record's payload byte -- it builds only the three
                // header words on the stack and hands AddEvent 16 bytes -- so the byte it
                // sends is whatever was left in that stack slot. The host sends 0.
                GuiEventCancelLoginWire lCancelLogin(0);
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lCancelLogin),
                    KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(lCancelLogin)));   // X360 record size 16
                break;
            }

            default:
                break;
        }
    }
}
