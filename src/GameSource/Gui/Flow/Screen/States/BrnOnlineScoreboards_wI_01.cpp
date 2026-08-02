// ===================================================================================
// BrnGui::OnlineScoreboards -- wave-I partfile 01: the two table pagers + the RUNNING pump.
//   PageUp        @0x8249F820  (DWARF cpp:1470)
//   PageDown      @0x8249F8A8  (DWARF cpp:1503)
//   UpdateRunning @0x824ADA18  (DWARF cpp:541)
//
//
// The committed leaf header BrnOnlineScoreboards.h is still the 29-line minimal version
// (the GetResourcesToLoad inline plus the two resource statics). The wave-I spec's
// full-shape class had not been applied when this partfile was written, and headers are
// frozen for implementers, so none of the three bodies can be declared as members and none
// of the members/methods they touch exists. Measured with the compile gate, not assumed:
// cl /c reports "PageUp/PageDown/UpdateRunning ist kein Member von
// BrnGui::OnlineScoreboards" plus undeclared mTable, meLeaderboardRequestState,
// E_LEADERBOARD_REQUEST_STATE_WF_TABLE, mpStateInterface, mpInGuiEventQueue,
// mbUsingFilters, mFilterToggleGroup, HandleControllerInputPressedUsingFilters and
// HandleControllerInputPressedUsingTable.
//
// The complete, drop-in-ready partfile (single `namespace BrnGui { ... }`, one anonymous
// namespace) lives at
// with a banner naming the exact declaration lines that unblock it. Copy it over this file
// once the spec's full-shape header lands; no edit is needed.
//
// EXACT DECLARATIONS THAT UNBLOCK IT (all inside
// `struct BrnGui::OnlineScoreboards : public CgsGui::State`, private section -- every one
// of them is already part of the spec's full-shape header):
//     enum ELeaderboardRequestState { ... E_LEADERBOARD_REQUEST_STATE_WF_TABLE = 7, ... };
//     void UpdateRunning();                                                    // @0x824ADA18
//     void PageUp(bool lbMoveHighlight);                                       // @0x8249F820
//     void PageDown(bool lbMoveHighlight);                                     // @0x8249F8A8
//     void HandleControllerInputPressedUsingFilters(const CgsModule::Event*);  // declare-only
//     void HandleControllerInputPressedUsingTable(const CgsModule::Event*);    // declare-only
//     ELeaderboardRequestState  meLeaderboardRequestState;  // X360 +340
//     bool                      mbUsingFilters;             // X360 +3584
//     MenuToggleGroupVarSize<3> mFilterToggleGroup;         // X360 +3592   "filter_mc"
//     LeaderboardTableComponent mTable;                     // X360 +15376  "Table_mc"
// plus the two member-type includes (BrnMenuToggleGroup.h, BrnLeaderboardTableComponent.h).
//
// against a SHADOW copy of the header carrying only those declarations --
// scratchpad/waveI/probe01/ (run_probe.py prints PROBE_STATUS=pass). The faithfulness lint
// reports 0 new findings on it. No other declaration is needed.
//
// CgsGuiStateInterface.h + CgsVariableEventQueue.h. Neither side of the
// TypeDefs/Demangled hard collision is needed here: the two page commands are the bare
// { 1, N, 12, <unwritten payload byte> } records, re-spelled as the file-local
// GuiCommandWire16<N> template (the wH_00 twin).
//
// LINK NOTE for the conductor: the pagers call
// LeaderboardTableComponent::GetRowsBefore / GetRowsAfter / SetHighlight (declared in
// BrnLeaderboardTableComponent.h, bodies owned by their own ledger TUs and defined nowhere
// in the tree yet); UpdateRunning calls
// OnlineScoreboards::HandleControllerInputPressedUsingFilters (foreign-owned, declare-only,
// nobody in this wave implements it) and HandleControllerInputPressedUsingTable (wave-I
// group 07). All are invisible to cl /c and may be unresolved at link until those land --
// same as wave H.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineScoreboards.h"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / the in-queue

namespace BrnGui
{

// BrnGui::OnlineScoreboards -- wave-I group 01: PageUp @0x8249F820, PageDown @0x8249F8A8
// and UpdateRunning @0x824ADA18.
//
// PARKED, drop-in ready. Copy this file over
// b5-decomp/src/GameSource/Gui/Flow/Screen/States/BrnOnlineScoreboards_wI_01.cpp once the
// wave-I spec's full-shape header lands; no edit is needed. It was parked because the
// committed BrnOnlineScoreboards.h is still the 29-line minimal leaf (the GetResourcesToLoad
// inline plus the two resource statics) and headers are frozen for implementers.
//
// EXACT DECLARATIONS THAT UNBLOCK IT (all inside
// `struct BrnGui::OnlineScoreboards : public CgsGui::State`; every one is already part of
// the spec's full-shape header):
//     enum ELeaderboardRequestState { ... E_LEADERBOARD_REQUEST_STATE_WF_TABLE = 7, ... };
//     void UpdateRunning();                                                    // @0x824ADA18
//     void PageUp(bool lbMoveHighlight);                                       // @0x8249F820
//     void PageDown(bool lbMoveHighlight);                                     // @0x8249F8A8
//     void HandleControllerInputPressedUsingFilters(const CgsModule::Event*);  // declare-only
//     void HandleControllerInputPressedUsingTable(const CgsModule::Event*);    // declare-only
//     ELeaderboardRequestState  meLeaderboardRequestState;  // X360 +340
//     bool                      mbUsingFilters;             // X360 +3584
//     MenuToggleGroupVarSize<3> mFilterToggleGroup;         // X360 +3592   "filter_mc"
//     LeaderboardTableComponent mTable;                     // X360 +15376  "Table_mc"
// plus the two member-type includes (BrnMenuToggleGroup.h, BrnLeaderboardTableComponent.h).
//
// PROVEN, not assumed: this file compiles clean under the repo's own compile gate against a
// SHADOW copy of the header carrying only those declarations -- scratchpad/waveI/probe01/
// (run_probe.py prints PROBE_STATUS=pass). The faithfulness lint reports 0 new findings.
//
// LINK NOTE for the conductor: LeaderboardTableComponent::GetRowsBefore / GetRowsAfter /
// SetHighlight are declared in BrnLeaderboardTableComponent.h but their bodies are owned by
// their own ledger TUs and are defined nowhere in the tree yet;
// HandleControllerInputPressedUsingFilters is the foreign-owned sibling (declare-only, nobody
// in this wave implements it) and HandleControllerInputPressedUsingTable is wave-I group 07.
// All are invisible to cl /c and may be unresolved at link until those land -- same as wave H.
//
// The two pagers ask the leaderboard back end for the previous/next page of the table and
// park the request machine on "waiting for table data"; UpdateRunning is the RUNNING-state
// pump, routing pad presses to either the filter row or the table and then refreshing the
// filter toggles.
    namespace
    {
        // ---- the state in-queue (CgsGui::State::mpInGuiEventQueue's real instantiation) ---
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // ---- AddEvent channels (the out-queue selector word) ---------------------------
        const s32 KI_CHANNEL_GUI_OUT = 40;

        // ---- observed wire ids ---------------------------------------------------------
        // The only in-queue id UpdateRunning acts on; every other observed event is handled
        // by UpdatePermanent, which the owning Update() runs alongside this pump.
        const s32 KI_EVENT_CONTROLLER_PRESSED = 6;

        // The two page commands the leaderboard back end listens for. FLAG: named for what
        // this screen does with them -- the producer-side names are not in the recovered
        // DWARF slice.
        const s32 KI_EVENT_SCOREBOARD_PAGE_UP   = 114;
        const s32 KI_EVENT_SCOREBOARD_PAGE_DOWN = 115;

        // A leaderboard page is this many rows, so a page step lands the highlight on the
        // far end of the page that is coming in (both pagers `cmpwi cr6, r9, 8`).
        const s8 KI_TABLE_ROWS_PER_PAGE = 8;

        // ---- out-queue wire records -----------------------------------------------------
        // The bare one-byte-payload command record { 1, N, 12, <payload byte> }, X360 record
        // size 16 (the wH_00 template). Both pagers stack-build ONLY the three header words,
        // so the payload byte keeps whatever the reused stack slot held -- the constructor
        // leaves it alone rather than inventing a stored value.
        template <s32 TI_EVENT_ID>
        struct GuiCommandWire16 : public CgsGui::GuiEvent<TI_EVENT_ID>
        {
            u8 mu8Payload;   // +0x0C (not written by the X360)

            GuiCommandWire16()
                : CgsGui::GuiEvent<TI_EVENT_ID>(
                      static_cast<u32>(sizeof(u8)),
                      static_cast<u32>(sizeof(CgsGui::GuiEvent<TI_EVENT_ID>)))
            {
            }
        };
    }

    // ================================================================================
    //  PageUp  @ 0x8249F820  (cpp:1470)
    //
    //  Step the leaderboard back one page. lbMoveHighlight also carries the highlight to
    //  the bottom row of the page that is about to arrive (the "held up past the top row"
    //  path); the filter row pages without moving it.
    // ================================================================================
    void OnlineScoreboards::PageUp(bool lbMoveHighlight)
    {
        // Nothing above the current page means there is nothing to page back to.
        const s8 liRowsBefore = mTable.GetRowsBefore();
        if (liRowsBefore == 0)
        {
            return;
        }

        if (lbMoveHighlight)
        {
            // At most a whole page arrives, so the incoming page's last row is
            // min(rows above, page size) - 1.
            s8 liHighlight = liRowsBefore;
            if (liHighlight >= KI_TABLE_ROWS_PER_PAGE)
            {
                liHighlight = KI_TABLE_ROWS_PER_PAGE;
            }
            mTable.SetHighlight(static_cast<s8>(liHighlight - 1));
        }

        // The new page comes back as a table-data event, so the request machine goes back
        // to waiting for one.
        meLeaderboardRequestState = E_LEADERBOARD_REQUEST_STATE_WF_TABLE;

        GuiCommandWire16<KI_EVENT_SCOREBOARD_PAGE_UP> lPageUpCommand;
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lPageUpCommand), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lPageUpCommand)));   // X360 record size 16
    }

    // ================================================================================
    //  PageDown  @ 0x8249F8A8  (cpp:1503)
    //
    //  The PageUp mirror: step forward a page, and with lbMoveHighlight put the highlight
    //  on the top row of the incoming page.
    // ================================================================================
    void OnlineScoreboards::PageDown(bool lbMoveHighlight)
    {
        // Nothing below the current page means there is nothing to page on to.
        const s8 liRowsAfter = mTable.GetRowsAfter();
        if (liRowsAfter == 0)
        {
            return;
        }

        if (lbMoveHighlight)
        {
            // A short final page is bottom-aligned in the table, so the first of its rows
            // sits page-size - min(rows below, page size) down the view.
            s8 liRowsMoved = liRowsAfter;
            if (liRowsMoved >= KI_TABLE_ROWS_PER_PAGE)
            {
                liRowsMoved = KI_TABLE_ROWS_PER_PAGE;
            }
            mTable.SetHighlight(static_cast<s8>(KI_TABLE_ROWS_PER_PAGE - liRowsMoved));
        }

        meLeaderboardRequestState = E_LEADERBOARD_REQUEST_STATE_WF_TABLE;

        GuiCommandWire16<KI_EVENT_SCOREBOARD_PAGE_DOWN> lPageDownCommand;
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lPageDownCommand), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lPageDownCommand)));   // X360 record size 16
    }

    // ================================================================================
    //  UpdateRunning  @ 0x824ADA18  (cpp:541)
    //
    //  The RUNNING sub-state pump. Only pad presses are consumed here; which handler gets
    //  them depends on whether the filter row or the table currently has focus.
    // ================================================================================
    void OnlineScoreboards::UpdateRunning()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;

        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            if (liEventId == KI_EVENT_CONTROLLER_PRESSED)
            {
                if (mbUsingFilters)
                {
                    HandleControllerInputPressedUsingFilters(lpEvent);
                }
                else
                {
                    HandleControllerInputPressedUsingTable(lpEvent);
                }
            }
        }

        // Repaint the filter row if anything the handlers did marked it dirty
        // (SelectableGroup::Update, the group's component vtable slot 5).
        // The in-queue is deliberately NOT cleared here -- the owning Update() drains it
        // once, after this pump and UpdatePermanent have both walked it.
        mFilterToggleGroup.Update();
    }
}
