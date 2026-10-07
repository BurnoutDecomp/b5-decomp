// ===================================================================================
// BrnGui::OnlineScoreboards -- wave-I partfile 07: the table-mode controller-input handler.
//   HandleControllerInputPressedUsingTable  @0x824A9B78  (DWARF cpp:697, assert cpp:761)
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineScoreboards.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface::OutputGuiEvent
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // the two scoreboard request events
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // the scoreboard request / gamercard / ev-score-target payloads

// ⭐ 2026-09-16 -- the file-local `CgsNetwork::DirtySock::LobbyNameCmp` declaration plus its
// `using` that used to sit here are GONE. They were written when no header exposed the
// comparator; CgsStringUtils.h:27 now declares the real one, and it is `extern "C"` at
// GLOBAL scope because the X360 exports one plain symbol. Keeping the namespaced stand-in
// and `using`-ing it into the global namespace put TWO candidates in the overload set, so
// every call -- even `::`-qualified -- was a hard C2668 the moment this TU was mounted, and
// the non-extern-"C" copy would have been a different symbol at link time anyway.
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // the REAL extern "C" LobbyNameCmp

namespace BrnGui
{

    namespace
    {
        // ---- in-queue payload view: GuiEventControllerInputPressed (in-queue id 6) -----
        // The GUI in-queue hands each handler a HEADER-STRIPPED payload, so the record starts
        // at the pad id and the pressed-button id is the second word (X360 `lwz r11, 4(r27)`).
        // DWARF names the parameter of this handler `const GuiEventControllerInputPressed*`;
        // that type has no committed home yet, so the two words it carries are viewed locally.
        struct ControllerInputPayload : public CgsModule::Event
        {
            s32 miPadId;     // +0x00
            s32 miButtonId;  // +0x04
        };

        // ---- the EGameInputActions this screen answers to in table mode ----------------
        // The X360 body is a 12-entry jump table biased by 0x29, of which only these four
        // and the two navigation moves are live (cases 0x2B..0x31 land on the default arm).
        const s32 KI_GAMEINPUT_MOVE_UP        = 0x29;
        const s32 KI_GAMEINPUT_MOVE_DOWN      = 0x2A;
        const s32 KI_GAMEINPUT_BACK_TO_FILTER = 0x32;
        const s32 KI_GAMEINPUT_SET_EV_TARGET  = 0x33;
        const s32 KI_GAMEINPUT_VIEW_GAMERCARD = 0x34;

        // The table shows eight rows at a time; the highlight cursor therefore runs 0..7 and
        // "sitting on the last visible row" is the paging trigger (X360 `cmpwi r11, 7`).
        const s32 KI_LAST_VISIBLE_ROW = 7;

        // SetHighlight(-1) clears the highlight (the table's own "no row selected" value).
        const s8 KI_NO_HIGHLIGHT = -1;
    } // anonymous namespace

    // -------------------------------------------------------------------------------------
    // HandleControllerInputPressedUsingTable  @ 0x824A9B78   (DWARF cpp:697)
    // Table-mode input: move the highlight within the visible page (paging up/down when it
    // runs off either end), hand focus back to the filter toggles, challenge the highlighted
    // event score, or ask for the highlighted player's gamercard.
    // -------------------------------------------------------------------------------------
    void OnlineScoreboards::HandleControllerInputPressedUsingTable(const CgsModule::Event* lpEvent)
    {
        // cpp:761. The X360 streams the message through a CgsDev::StrStream; per project policy
        // that is lowered to the static text. Non-fatal: the body reads the event either way.
        CGS_ASSERT(lpEvent, "Invalid event sent to OnlineScoreboards::HandleControllerInput");

        const ControllerInputPayload* lpControllerEvent =
            reinterpret_cast<const ControllerInputPayload*>(lpEvent);

        switch (lpControllerEvent->miButtonId)
        {
            case KI_GAMEINPUT_MOVE_UP:
            {
                // cpp:727. Step the highlight one row up; page up when it is already on the
                // first row (or cleared). The X360 tail-duplicates the redraw into both arms.
                const s8 liCurrentHighlight = mTable.GetHighlight();
                if (liCurrentHighlight <= 0)
                {
                    PageUp(true);
                }
                else
                {
                    mTable.SetHighlight(static_cast<s8>(liCurrentHighlight - 1));
                }
                mTable.DrawScoreboard();
                break;
            }

            case KI_GAMEINPUT_MOVE_DOWN:
            {
                // cpp:743. Step the highlight one row down while there is a row below it;
                // otherwise page down, but only from the last visible row (a short final page
                // leaves the highlight where it is and just redraws).
                const s8 liCurrentHighlight = mTable.GetHighlight();
                if (liCurrentHighlight >= mTable.GetRowsUsed() - 1)
                {
                    if (liCurrentHighlight == KI_LAST_VISIBLE_ROW)
                    {
                        PageDown(true);
                    }
                }
                else
                {
                    mTable.SetHighlight(static_cast<s8>(liCurrentHighlight + 1));
                }
                mTable.DrawScoreboard();
                break;
            }

            case KI_GAMEINPUT_BACK_TO_FILTER:
                // Focus goes back to the category/index/variation toggles: drop the table
                // highlight, redraw the (now unhighlighted) table, restore the filter row that
                // was highlighted when the table took focus, and re-prompt the buttons.
                mbUsingFilters = true;
                mTable.SetHighlight(KI_NO_HIGHLIGHT);
                mTable.DrawScoreboard();
                mFilterToggleGroup.HighlightIndex(miCurrentFilterHighlighted);
                SetupButtons();
                break;

            case KI_GAMEINPUT_SET_EV_TARGET:
                if (mbEventLeaderboard)
                {
                    // FLAGGED: gated on the same two DLC/entitlement capability bits the sibling
                    // ScoreboardManager / EventScoresManager flag un-recovered ((dword_82FFA7F4 &
                    // dword_82FFA7F8[dword_82FFA864]) == the mask && byte_82FFA886, and the
                    // 868/887 twin). Runtime .data (dumped all-zero) -- reconstruct the real test
                    // when the DLC manager lands.
                    const bool lbEvScoreTargetEnabled = false;   // FLAGGED placeholder (un-recovered runtime state)

                    if (lbEvScoreTargetEnabled)
                    {
                        // The 36-byte challenge record: the highlighted row's score plus the
                        // leaderboard slot it was read from. miVariation is the RAW selection --
                        // this path does not run it through maiAlphabeticalRoadIndex the way the
                        // per-road table request does.
                        GuiEventScoreboardRequestEvScoreTarget lRequestEvScoreTarget;
                        lRequestEvScoreTarget.miCategory        = miCurrentCategory;
                        lRequestEvScoreTarget.miIndex           = miCurrentIndex;
                        lRequestEvScoreTarget.miVariation       = miCurrentVariation;
                        lRequestEvScoreTarget.miScore           = mTable.GetHighlightedScore();
                        lRequestEvScoreTarget.mbIsCurrentTarget = false;

                        mTable.GetHighlightedGamertag(&lRequestEvScoreTarget.mPlayerName);
                        // ::-qualified: the X360 exports ONE plain global symbol for this
                        // (CgsStringUtils.h:21-27 says so, and its body home wraps the
                        // definition in extern "C"). Unqualified, the DirtySock file-scope
                        // re-declaration also enters the overload set and the call is
                        // ambiguous. CgsNetworkUtils.cpp:89 resolves it exactly this way.
                        if (::LobbyNameCmp(lRequestEvScoreTarget.mPlayerName.macName,
                                           mCurrentTargetScorePlayerName.macName) == 0)
                        {
                            lRequestEvScoreTarget.mbIsCurrentTarget = true;
                        }

                        mpStateInterface->OutputGuiEvent(lRequestEvScoreTarget);
                        mbWaitingForTargetScoreResponse = true;
                    }
                }
                break;

            case KI_GAMEINPUT_VIEW_GAMERCARD:
            {
                // The whole record is the highlighted player's name; the table writes it
                // straight into the request.
                GuiEventScoreboardRequestGamercardEvent lRequestGamerCard;
                mTable.GetHighlightedGamertag(&lRequestGamerCard.mPlayerName);
                mpStateInterface->OutputGuiEvent(lRequestGamerCard);
                break;
            }

            default:
                // Every other input is ignored in table mode.
                break;
        }
    }
}
