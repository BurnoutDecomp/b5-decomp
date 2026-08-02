// ===================================================================================
// BrnGui::OnlineScoreboards -- wave-I partfile 07: the table-mode controller-input handler.
//   HandleControllerInputPressedUsingTable  @0x824A9B78  (DWARF cpp:697, assert cpp:761)
//
//
// The complete, drop-in-ready partfile lives at
// with a banner naming the exact declaration lines that unblock it. Copy it over this file
// once the header changes below land; no edit is needed.
//
// the compile gate, not assumed.
//
//  (1) GameSource/Gui/Flow/Screen/States/BrnOnlineScoreboards.h is still the 29-line minimal
//      Engine-cone-wave-19 version (the GetResourcesToLoad inline plus the two resource
//      statics); the wave-I spec's full-shape class has not been applied, so neither the
//      method nor any member it touches exists. Needed (all private, all already part of the
//      spec's full-shape header):
//          void HandleControllerInputPressedUsingTable(const CgsModule::Event* lpEvent);  // @0x824A9B78
//          void PageUp(bool lbMoveHighlight);    // @0x8249F820
//          void PageDown(bool lbMoveHighlight);  // @0x8249F8A8
//          void SetupButtons();                  // @0x82486A58
//          s32                       miCurrentCategory;               // X360 +344
//          s32                       miCurrentIndex;                  // X360 +880
//          s32                       miCurrentVariation;              // X360 +1240
//          s32                       miCurrentFilterHighlighted;      // X360 +3560
//          CgsNetwork::PlayerName    mCurrentTargetScorePlayerName;   // X360 +3568
//          bool                      mbUsingFilters;                  // X360 +3584
//          bool                      mbEventLeaderboard;              // X360 +3586
//          bool                      mbWaitingForTargetScoreResponse; // X360 +3587
//          MenuToggleGroupVarSize<3> mFilterToggleGroup;              // X360 +3592
//          LeaderboardTableComponent mTable;                          // X360 +15376
//      plus the member-type includes CgsVariableEventQueue.h, BrnCgsPlayerName.h,
//      BrnMenuToggleGroup.h and BrnLeaderboardTableComponent.h.
//
//  (2) GameSource/Gui/Flow/Screen/Components/BrnLeaderboardTableComponent.h is missing the one
//      accessor this body calls twice. DWARF (BrnLeaderboardTableComponent.h:66 /
//      BrnLeaderboardTableComponent.cpp:371) gives the shape verbatim -- the out-param is a
//      PlayerName, NOT a char*, and the method is not const:
//          void GetHighlightedGamertag(CgsNetwork::PlayerName* lpPlayerName);   // @0x82419208
//      plus #include "GameSource/GameState/BrnCgsPlayerName.h" in that header.
//
//  (3+4) The two scoreboard REQUEST event types must MOVE from
//      GameSource/Gui/BrnGuiDemangledEventTypes.h:335/336 into
//      GameSource/Gui/BrnGuiEventTypeDefs.h with their real field shapes (the spec's cross-TU
//      finding 1, resolved the way this repo already resolves every recovered payload -- the
//      Demangled header keeps a "MOVED to BrnGuiEventTypeDefs.h ... Do not re-add it here"
//      note, exactly as it does for GuiEventProgressionProfileData). Two reasons force the
//      move rather than an in-place edit:
//        (a) the current `: public CgsGui::GuiEvent<N>` + opaque-maPayload shells park our
//            12-byte GuiEvent header exactly where the emitter writes real fields;
//        (b) this TU cannot include the Demangled header at all -- the class header must
//            include BrnMenuToggleGroup.h for the by-value MenuToggleGroupVarSize<3> member,
//            that pulls BrnMenuToggle.h, and BrnMenuToggle.h includes BrnGuiEventTypeDefs.h;
//            TypeDefs and Demangled hard-collide (measured: C2011 on GuiEventRunFsm and
//            GuiAudioTriggerEvent). The DecFIGS DWARF names TypeDefs as their home anyway
//            (BrnGuiEventTypeDefs.h:740 declares the gamercard event's one member
//            `PlayerName mPlayerName`).
//
// against SHADOW copies of those four headers carrying only those additions --
// scratchpad/waveI/probe_sb07/ (run_probe.py prints PROBE_STATUS=pass). The faithfulness lint
// reports 0 new findings on it. No other declaration is needed.
//
// CgsGuiStateInterface.h + CgsVariableEventQueue.h + BrnGuiEventTypeDefs.h. TypeDefs is the
// side of the TypeDefs/Demangled hard collision this TU is forced onto (see 3+4 above); the
// dirtysdk comparator LobbyNameCmp is declared file-locally, as BrnChallengeHighScoreEntry.cpp
// and BrnGuiCache_wB_11/12.cpp already do.
//
// LINK NOTE for the conductor: the body calls LeaderboardTableComponent::GetHighlight /
// SetHighlight / GetRowsUsed / GetHighlightedGamertag (declared in
// BrnLeaderboardTableComponent.h, bodies owned by their own ledger TUs and defined nowhere in
// the tree yet), OnlineScoreboards::PageUp / PageDown / SetupButtons (wave-I groups 01 and 00,
// tree). All are invisible to cl /c and may be unresolved at link until those TUs land --
// same as wave H.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineScoreboards.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface::OutputGuiEvent
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // the two scoreboard request events
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // the scoreboard request / gamercard / ev-score-target payloads

// DirtySock SDK import: the lobby-safe gamertag compare the X360 bl's here. The tree
// already declares it this way (never defines it) in CgsServerInterfaceGames.cpp -- it
// links from the SDK, so declare, do not stub.
namespace CgsNetwork { namespace DirtySock { s32 LobbyNameCmp(const char* pNameA, const char* pNameB); } }
using CgsNetwork::DirtySock::LobbyNameCmp;

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

                        mTable.GetHighlightedGamertag(lRequestEvScoreTarget.mPlayerName.macName);
                        if (LobbyNameCmp(lRequestEvScoreTarget.mPlayerName.macName,
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
                mTable.GetHighlightedGamertag(lRequestGamerCard.mPlayerName.macName);
                mpStateInterface->OutputGuiEvent(lRequestGamerCard);
                break;
            }

            default:
                // Every other input is ignored in table mode.
                break;
        }
    }
}
