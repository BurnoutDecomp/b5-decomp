// BrnGui::OnlineScoreboards -- static-member definitions (X360 rodata values, dumped:
// scratchpad/waveI/scoreboards_rodata.txt) and the constructor.
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineScoreboards.h"
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnOnlineScoreboards.cpp"
namespace BrnGui
{
    // the scoreboards screen.
    const CgsGui::sResourceTuple OnlineScoreboards::maResourcesToLoad[] =
        { { 182, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const u32 OnlineScoreboards::muNumResourcesToLoad = 1;

    // @0x8205F648 -- the observed in-queue event set (ARTIST count 12; Dec-07 DWARF said 10).
    // 14/21 resource-completion / apt ONLOAD (cache layer), 6 controller, 26 frame tick,
    // 44 network-disconnected, 64 gui-cache, 116..119 scoreboard responses, 122 target-score
    // response, 123 downloaded-challengeable.
    const s32 OnlineScoreboards::maiEventToObserve[12] =
        { 14, 21, 6, 44, 26, 64, 116, 117, 118, 119, 122, 123 };
    const s32 OnlineScoreboards::miNumEventsObserved = 12;   // @0x8205F678

    // @0x82F2687C -- the three filter toggle titles (rows 0/1/2).
    const char* const OnlineScoreboards::KAPC_FILTER_TITLE_STRINGS[3] =
        { "$SCOREBOARD_EVENT", "$SCOREBOARD_FILTER", "$SCOREBOARD_ROAD" };

    // The apt component names (the OnEnter constructor literals).
    const char OnlineScoreboards::KAC_FILTER_GROUP_NAME[10]         = "filter_mc";
    const char OnlineScoreboards::KAC_LEADERBOARD_TABLE_NAME[9]     = "Table_mc";
    const char OnlineScoreboards::KAC_LOADING_ICON_NAME[11]         = "Loading_mc";
    const char OnlineScoreboards::KAC_BUTTON_ANIMATOR_NAME[19]      = "ButtonPrompts_anim";
    const char OnlineScoreboards::KAC_TABLE_EMPTY_ANIMATOR_NAME[16] = "TableEmpty_anim";

    // The table-request debounce armed by the variation-list response (the 0x3F000000
    // literal in UpdatePermanent's case 118).
    const f32 OnlineScoreboards::KF_DELAY_TO_REQUEST_TABLE = 0.5f;

}

// FOLDED FROM BrnOnlineScoreboards_wI_00.cpp
// ===================================================================================
// BrnGui::OnlineScoreboards -- wave-I partfile 00: the two animator leaves + the audio wire.
//   ShowTableEmpty @0x82486A08  (DWARF cpp:1390)
//   SetupButtons   @0x82486A58  (DWARF cpp:1535)
//   TriggerSound   @0x8249F710  (DWARF cpp:1418, assert cpp:1570)
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineScoreboards.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiAudioTriggerEvent

namespace BrnGui
{

    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) --------------------------
        const s32 KI_CHANNEL_GUI_OUT = 40;   // X360 `li r5, 0x28`
        const s32 KI_APT_MOVIE_LEVEL = 3;
        // ---- apt view-state names (rodata literals) ------------------------------------
        // Every component transition on this screen is pushed down the one "apt_Transition"
        // channel (aAptTransition_1 -- the same string all three call sites load).

        // TableEmpty_anim's two states.

        // ButtonPrompts_anim's five states. "SingleX360" is the TABLE-focus prompt set: the
        // X360 picks it when mbUsingFilters is 0 (`lbz r11, 0xE00(r3)` / `bne` at
        // 0x82486A58-64), and 0 is the table-focus value -- UpdateRunning @0x824ADA54 routes a
        // 0 to HandleControllerInputPressedUsingTable, that handler stores 1 there to hand
        // focus to the filters (@0x824A9C64), and HandleControllerInputPressedUsingFilters
        // stores 0 to hand it back to the table (@0x824A9F64). The four "General*" variants
        // are therefore the FILTER-row prompt set, and which one is picked says whether the
        // table below can still page up, page down, both, or neither. Consistent with the
        // DWARF, which sizes KAE_HELP_BAR_TABLE at 1 entry and KAE_HELP_BAR_FILTER at 4.

        // ---- controller action ids (the in-queue payload's second word) ----------------
        // FLAG: the producer-side enum name (EGameInputActions) is not in the recovered
        // DWARF slice, so these are named for the role the X360 code gives them. The X360
        // switches on `r4 - 0x25` over 8 contiguous cases. Same family split as the two
        // committed twins (BrnOnlineGameRoomPlayerInfo_wH_08.cpp, BrnOnlineGameOptions
        // group 02): 0x25/0x26/0x29/0x2A are ROW moves, 0x27/0x28/0x2B/0x2C are toggle-
        // VALUE moves.
        const s32 KI_ACTION_MENU_MOVE_A   = 0x25;   // '%'  row-move family
        const s32 KI_ACTION_MENU_MOVE_B   = 0x26;   // '&'  row-move family
        const s32 KI_ACTION_ITEM_MOVE_A   = 0x27;   // '\'' value-move family
        const s32 KI_ACTION_ITEM_MOVE_B   = 0x28;   // '('  value-move family
        const s32 KI_ACTION_MENU_PREVIOUS = 0x29;   // ')'  row-move family
        const s32 KI_ACTION_MENU_NEXT     = 0x2A;   // '*'  row-move family
        const s32 KI_ACTION_ITEM_PREVIOUS = 0x2B;   // '+'  value-move family
        const s32 KI_ACTION_ITEM_NEXT     = 0x2C;   // ','  value-move family

        // The GuiAudioTriggerEvent::meAction value every menu cue in the GUI flow uses
        // (X360 `li r4, 7`). FLAG: the action enum's name is not in the recovered DWARF
        // slice.
        const s32 KI_AUDIO_ACTION_MENU_CUE = 7;

        // The two audio labels the X360 selects between (rodata literals).

        // ---- out-queue wire record -----------------------------------------------------
        // The record the inlined OutputGuiEvent<BrnGui::GuiAudioTriggerEvent> builds:
        // { 100, 457, 12, the 100-byte payload }, channel 40, 112 bytes (X360 stores
        // `li r11, 0x64` / `li r11, 0x1C9` / `li r11, 0xC` then AddEvent with r5 = 0x28,
        // r6 = 0x70). The committed PC GuiAudioTriggerEvent already carries that 12-byte
        // queue header in front of its 100-byte payload, so the event object IS the record
        // -- but its GuiEvent<201> base seeds the payload-size word 0 and the id 201, while
        // the X360 wire words are the payload size and the id 457. This view restores them
        // without forking the payload type (BrnOnlineGameRoomPlayerInfo_wH_08.cpp carries
        // the identical view).
        const u32 KU_WIRE_ID_AUDIO_TRIGGER = 457;   // X360 `li r11, 0x1C9`

        typedef CgsGui::GuiEventOut<GuiAudioTriggerEvent> GuiAudioTriggerWire;

        // Record-size pin. The payload is pointer-free, so the X360's AddEvent size
        // immediate must survive unchanged on the x64 host; if a host layout ever drifts,
        // this is where it is caught rather than on the wire.
        CGS_STATIC_ASSERT((sizeof(GuiAudioTriggerWire) == 112), "audio trigger record is 112 bytes");
    }

    // ---- ShowTableEmpty @ 0x82486A08 -----------------------------------------------
    // Raise or drop the "no scores to show" overlay. The state is latched so the
    // transition is only pushed when it actually changes (the X360 returns straight out of
    // the compare when the flag already matches).
    void OnlineScoreboards::ShowTableEmpty(bool lbShow)
    {
        if (mbTableEmptyShowing == lbShow)
        {
            return;
        }

        mbTableEmptyShowing = lbShow;

        mTableEmptyAnimator.AddOutputAptViewState(
            "apt_Transition",
            lbShow ? "visible" : "invisible",
            false);
    }

    // ---- SetupButtons @ 0x82486A58 -------------------------------------------------
    // Put the right button-prompt set up for whatever the player is driving. mbUsingFilters
    // is 0 while the TABLE has focus and nonzero while the filter row has focus --
    // UpdateRunning @0x824ADA54 dispatches to HandleControllerInputPressedUsingTable when it
    // reads 0. So while the table has focus there is a single prompt layout ("SingleX360");
    // while the filter row has focus the prompt set advertises the paging the table below
    // still has, which the table's rows-before / rows-after counters answer.
    void OnlineScoreboards::SetupButtons()
    {
        if (!mbUsingFilters)
        {
            mButtonAnimator.AddOutputAptViewState("apt_Transition",
                                                  "SingleX360", false);
            return;
        }

        // The X360 reads the two byte-wide counters straight out of the table component
        // (`lbz 0x4DAD` / `lbz 0x4DAE`) because both accessors inline; restored as the
        // declared accessor calls.
        const s8 liRowsBefore = mTable.GetRowsBefore();
        const s8 liRowsAfter  = mTable.GetRowsAfter();

        const char* lpcViewState;

        if (liRowsBefore == 0)
        {
            lpcViewState = (liRowsAfter != 0) ? "GeneralDown"
                                              : "General";
        }
        else
        {
            lpcViewState = (liRowsAfter != 0) ? "GeneralBoth"
                                              : "GeneralUp";
        }

        mButtonAnimator.AddOutputAptViewState("apt_Transition", lpcViewState, false);
    }

    // ---- TriggerSound @ 0x8249F710 -------------------------------------------------
    // Raise the menu-move audio cue for a controller action: pick the label for the
    // action's family and publish a GuiAudioTriggerEvent with it.
    void OnlineScoreboards::TriggerSound(s32 leGameInputAction)
    {
        const char* lpcLabel = 0;

        switch (leGameInputAction)
        {
            case KI_ACTION_MENU_MOVE_A:
            case KI_ACTION_MENU_MOVE_B:
            case KI_ACTION_MENU_PREVIOUS:
            case KI_ACTION_MENU_NEXT:
                lpcLabel = "MenuToggleDefault";
                break;

            case KI_ACTION_ITEM_MOVE_A:
            case KI_ACTION_ITEM_MOVE_B:
            case KI_ACTION_ITEM_PREVIOUS:
            case KI_ACTION_ITEM_NEXT:
                lpcLabel = "MenuItemToggleDefault";
                break;

            default:
                break;
        }

        // cpp:1570 -- non-fatal, and the X360 sinks it into the switch's default arm (the
        // other arms provably set lpcLabel and branch past it). Either placement runs the
        // same code: the event is constructed with the null label on the unrecognised path.
        CGS_ASSERT_AT(lpcLabel, "lpcLabel", CGS_ARTIST_FILE, 1570);

        // X360: Construct(r3 = the stack event, r4 = 7, r5 = "", r6 = the label, r7 = "").
        GuiAudioTriggerEvent lAudioPayload;
        lAudioPayload.Construct(KI_AUDIO_ACTION_MENU_CUE, "", lpcLabel, "");

        mpStateInterface->OutputGuiEvent(lAudioPayload);
    }
}

// FOLDED FROM BrnOnlineScoreboards_wI_01.cpp
// ===================================================================================
// BrnGui::OnlineScoreboards -- wave-I partfile 01: the two table pagers + the RUNNING pump.
//   PageUp        @0x8249F820  (DWARF cpp:1470)
//   PageDown      @0x8249F8A8  (DWARF cpp:1503)
//   UpdateRunning @0x824ADA18  (DWARF cpp:541)
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
// The two pagers ask the leaderboard back end for the previous/next page of the table and
// park the request machine on "waiting for table data"; UpdateRunning is the RUNNING-state
// pump, routing pad presses to either the filter row or the table and then refreshing the
// filter toggles.
    namespace
    {
        // ---- the state in-queue (CgsGui::State::mpInGuiEventQueue's real instantiation) ---
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // ---- AddEvent channels (the out-queue selector word) ---------------------------


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
        mpStateInterface->OutputGuiEvent(lPageUpCommand);   // X360 record size 16
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
        mpStateInterface->OutputGuiEvent(lPageDownCommand);   // X360 record size 16
    }

    // ================================================================================
    //  UpdateRunning  @ 0x824ADA18  (cpp:541)
    //
    //  The RUNNING sub-state pump. Only pad presses are consumed here; which handler gets
    //  them depends on whether the filter row or the table currently has focus.
    // ================================================================================
    void OnlineScoreboards::UpdateRunning()
    {

        const CgsModule::Event* lpEvent;
        s32 liSize;

        for (s32 liEventId = mpInGuiEventQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = mpInGuiEventQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
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

// FOLDED FROM BrnOnlineScoreboards_wI_02.cpp
// ===================================================================================
// BrnGui::OnlineScoreboards -- wave-I partfile 02: the GuiCache handoff and the
// expected-apt-component bookkeeping the screen's internal-state machine runs before it
// can drive its components.
//
//   ClearExpectedComponent @0x82486928  (asserts cpp:430)
//   UpdateWFInit           @0x824869A0
//   UpdateGetCache         @0x824917E0  (asserts cpp:473 / 483 / 492)
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX. The committed twin for the
// cache handoff is BrnCarSelectUnlock.cpp:118-150 (same three assert strings, same queue
// walk); this file matches its idiom.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineScoreboards.h"

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::Event / the in-queue
#include "GameSource/Gui/BrnGuiCache.h"                           // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                   // BrnGui::GuiFlow / E_GUIFLOW_SCREEN

#include <cstring>                                                // std::memset

namespace BrnGui
{
    namespace
    {
        // The state in-queue (CgsGui::State::mpInGuiEventQueue's real instantiation -- the
        // base holds it as the incomplete InputBuffer::GuiEventQueue alias).
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // The GuiCache handoff event: id 64, maiEventToObserve[5]. Its payload is the bare
        // cache pointer (the queue delivers header-stripped payloads).
        const s32 KI_EVENT_GUI_CACHE = 64;
    }

    // ---- ClearExpectedComponent @ 0x82486928 (DWARF cpp:420) ----------------------
    // Drop the flow layer's expected-apt-component watch list, then reset this screen's own
    // pending-id bookkeeping.
    void OnlineScoreboards::ClearExpectedComponent()
    {
        CGS_ASSERT(mpGuiCache, CGS_ASSERT_MESSAGE_AT("mpGuiCache", CGS_ARTIST_FILE, 430 /* ARTIST 0x82486960 */));   // cpp:430 (non-fatal -- the call below runs regardless)

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);

        // X360 emits five unrolled word stores (this+0x40..0x50): the four id slots then the
        // live count. Re-rolled; the span is the host sizeof of the array, never a console byte count.
        std::memset(mauExpectedComponentIds, 0, sizeof(mauExpectedComponentIds));
        muNumExpectedComponents = 0;
    }

    // ---- UpdateWFInit @ 0x824869A0 (DWARF cpp:497) --------------------------------
    // The WFINIT arm of the internal-state machine: hold until every expected apt component
    // on the screen flow layer has finished initialising, then release the watch list.
    bool OnlineScoreboards::UpdateWFInit()
    {
        if (!mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
        {
            return false;
        }

        ClearExpectedComponent();
        return true;
    }

    // ---- UpdateGetCache @ 0x824917E0 (DWARF cpp:459) ------------------------------
    // The GETCACHE arm of the internal-state machine: scan the in-event queue for the
    // GuiCache event (type 64) and latch its carried cache pointer into mpGuiCache.
    void OnlineScoreboards::UpdateGetCache()
    {
        CGS_ASSERT(mpGuiCache == NULL, CGS_ASSERT_MESSAGE_AT("NULL == mpGuiCache", CGS_ARTIST_FILE, 473 /* ARTIST 0x82491818 */));   // cpp:473 (the cache must still be unset here)

        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = NULL;
        s32 liSize = 0;
        s32 liEventType = lpInQueue->GetFirstEvent(&lpEvent, &liSize);

        if (lpEvent != NULL)
        {
            while (liEventType != KI_EVENT_GUI_CACHE)
            {
                liEventType = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
                if (lpEvent == NULL)
                    break;
            }

            if (lpEvent != NULL)
            {
                // The cache event carries the GuiCache pointer in its leading word.
                GuiCache* lpCache = *reinterpret_cast<GuiCache* const*>(lpEvent);
                CGS_ASSERT(lpCache != NULL, CGS_ASSERT_MESSAGE_AT("NULL != lpCacheEvent->mpCachePointer", CGS_ARTIST_FILE, 483 /* ARTIST 0x82491888 */));   // cpp:483

                // X360 stores unconditionally, after the (non-fatal) assert above.
                mpGuiCache = lpCache;
            }
        }

        CGS_ASSERT(mpGuiCache != NULL, CGS_ASSERT_MESSAGE_AT("NULL != mpGuiCache", CGS_ARTIST_FILE, 492 /* ARTIST 0x824918B8 */));   // cpp:492
    }
}

// FOLDED FROM BrnOnlineScoreboards_wI_03.cpp
// ===================================================================================
// BrnGui::OnlineScoreboards -- wave-I partfile 03: the alphabetical road index + the
// pool-construction wiring.
//   _RoadQSortFunction           @0x824867C8  (DWARF cpp:164, asserts cpp:173/174)
//   BuildAlphabeticalRoadIndexes @0x82486860  (DWARF cpp:183)
//   Construct                    @0x824866F0  (DWARF cpp:123)
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineScoreboards.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface::GetLanguageManager
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"           // FormatText / E_FORMAT_ID_LOOKUP
#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"   // KAA_SAVE_GAME_CHALLENGE_ROAD_IDS
#include <cstdlib>   // qsort

namespace BrnGui
{

    namespace
    {
        // The screen offers one leaderboard per road, and the road filter lists them
        // alphabetically in the CURRENT language -- so the order cannot be baked into the
        // table and has to be rebuilt from the localised names every time the screen opens.
        // The road ids are the street manager's save-game challenge road-id table (its 64
        // entries are localisation-database string ids -- BuildAlphabeticalRoadIndexes
        // renders each one decimally and resolves it through E_FORMAT_ID_LOOKUP).

        // The decimal id is rendered into this many characters before it is handed to the
        // localisation lookup. Source constant (X360 `li r4, 0xA`), not a buffer size -- the
        // buffer itself is 16 bytes wide.
        const u32 KU_ROAD_ID_DIGITS = 10;
    }

    // ================================================================================
    //  _RoadQSortFunction  @ 0x824867C8  (cpp:164)
    //
    //  The qsort comparator BuildAlphabeticalRoadIndexes sorts the road records with:
    //  an ordinary lexicographic compare of the two localised road names. Written out
    //  by hand in the original (the X360 emits no strcmp call), so it is written out
    //  here too rather than forwarded to the CRT.
    // ================================================================================
    int OnlineScoreboards::_RoadQSortFunction(const void* lpRoadData1, const void* lpRoadData2)
    {
        // cpp:173 / cpp:174 -- both non-fatal on the X360: the compare runs regardless.
        CGS_ASSERT(lpRoadData1 != 0, CGS_ASSERT_MESSAGE_AT("lpRoadData1", CGS_ARTIST_FILE, 173 /* ARTIST 0x82486800 */));
        CGS_ASSERT(lpRoadData2 != 0, CGS_ASSERT_MESSAGE_AT("lpRoadData2", CGS_ARTIST_FILE, 174 /* ARTIST 0x82486824 */));

        // X360 `addi rN, rN, 4`: the compared field is the record's name, not the record.
        // Reached through the named member so the host's own offset is used.
        const char* lpcName1 = static_cast<const RoadSortData*>(lpRoadData1)->lacRoadString;
        const char* lpcName2 = static_cast<const RoadSortData*>(lpRoadData2)->lacRoadString;

        // The bytes are loaded with `lbz` (zero-extending), so the difference is taken
        // between UNSIGNED characters -- high-range localised bytes must sort above ASCII.
        s32 liResult;
        do
        {
            liResult = static_cast<s32>(static_cast<u8>(*lpcName1))
                     - static_cast<s32>(static_cast<u8>(*lpcName2));

            // Running off the end of the first name ends the compare whatever the
            // difference is (it is 0 only when both names ended together).
            if (*lpcName1 == '\0')
            {
                break;
            }

            ++lpcName1;
            ++lpcName2;
        }
        while (liResult == 0);

        return liResult;
    }

    // ================================================================================
    //  BuildAlphabeticalRoadIndexes  @ 0x82486860  (cpp:183)
    //
    //  Resolve all 64 road-name string ids in the current language, sort them by that
    //  localised name, and keep the permutation in maiAlphabeticalRoadIndex. The road
    //  filter then walks 0..63 and maps each slot through this index to get the table
    //  entry to request -- which is why the sort result is stored as indices and the
    //  names themselves are thrown away with the local array.
    //  Called once, from OnEnter.
    // ================================================================================
    void OnlineScoreboards::BuildAlphabeticalRoadIndexes()
    {
        CGS_STATIC_ASSERT((KI_NUM_ALPHABETICAL_ROADS == BrnGameState::KI_MAX_CHALLENGES),
                      "one alphabetical slot per challenge road");
        RoadSortData laRecords[KI_NUM_ALPHABETICAL_ROADS];

        for (s32 liRoad = 0; liRoad < KI_NUM_ALPHABETICAL_ROADS; ++liRoad)
        {
            // Remember where this name started out, so the sort carries it along
            // (X360 `stw` -- the full index word, written before the name is filled in).
            laRecords[liRoad].liIndex = liRoad;

            // The localisation lookup takes the id as text.
            //
            // The X360 loads the whole doubleword into the vararg slot; "%d" consumes the
            // low word of that slot and every entry's high word is zero, so the rendered
            // digits are identical. Narrowing here makes the host's vararg slot unambiguous
            // rather than relying on that.
            char lacNumber[16];
            CgsCore::SPrintf(lacNumber, KU_ROAD_ID_DIGITS, "%d",
                             BrnGameState::KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[liRoad]);   // the whole doubleword (ARTIST ld r6)

            // X360 r7 == 9 == E_FORMAT_ID_LOOKUP: resolve the digits as a database id.
            mpStateInterface->GetLanguageManager()->FormatText(
                laRecords[liRoad].lacRoadString,
                static_cast<u32>(sizeof(laRecords[liRoad].lacRoadString)),   // KI_MAX_ROAD_STRING
                lacNumber,
                CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
        }

        // The record stride is the HOST sizeof, never the X360's baked 0x18 (24) -- the
        // console record is 4 + 20 with no tail padding; the host record is not.
        qsort(laRecords,
              KI_NUM_ALPHABETICAL_ROADS,
              sizeof(RoadSortData),
              &OnlineScoreboards::_RoadQSortFunction);

        // Keep the permutation; the localised names were only ever the sort key.
        for (s32 liRoad = 0; liRoad < KI_NUM_ALPHABETICAL_ROADS; ++liRoad)
        {
            maiAlphabeticalRoadIndex[liRoad] = laRecords[liRoad].liIndex;
        }
    }

    // ================================================================================
    //  Construct  @ 0x824866F0  (cpp:123)
    //
    //  Pool-construction hook (BrnScreenFlow's NewPoolState<OnlineScoreboards> +
    //  Construct(CgsIDCompress("ON_SCOREB"), &lStateMachine)). Runs the base first, parks
    //  the three filter cursors on their first entry, and points each filter's pointer
    //  table at its own row of text storage -- the toggle group is handed the pointer
    //  table, so this wiring is what lets the list handlers just SPrintf into the rows.
    // ================================================================================
    void OnlineScoreboards::Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm)
    {
        CgsGui::State::Construct(liId, lpFsm);

        miCurrentCategory  = 0;
        miCurrentIndex     = 0;
        miCurrentVariation = 0;

        // Each of the three tables is `mapcX[i] = maacX[i]`. The X360 strides the text
        // storage by its baked row pitch (0x1F == 31) and unrolls the 10-entry index loop
        // outright; indexing the arrays by name reproduces both without either console
        // number appearing. Counts come from the declared arrays for the same reason
        // (X360: 15 categories, 10 indexes, 66 variations).
        const s32 liNumCategorySlots =
            static_cast<s32>(sizeof(mapcCategories) / sizeof(mapcCategories[0]));
        for (s32 liCategory = 0; liCategory < liNumCategorySlots; ++liCategory)
        {
            mapcCategories[liCategory] = maacCategories[liCategory];
        }

        const s32 liNumIndexSlots =
            static_cast<s32>(sizeof(mapcIndexes) / sizeof(mapcIndexes[0]));
        for (s32 liIndex = 0; liIndex < liNumIndexSlots; ++liIndex)
        {
            mapcIndexes[liIndex] = maacIndexes[liIndex];
        }

        const s32 liNumVariationSlots =
            static_cast<s32>(sizeof(mapcVariations) / sizeof(mapcVariations[0]));
        for (s32 liVariation = 0; liVariation < liNumVariationSlots; ++liVariation)
        {
            mapcVariations[liVariation] = maacVariations[liVariation];
        }
    }
}

// FOLDED FROM BrnOnlineScoreboards_wI_04.cpp
// ===================================================================================
// BrnGui::OnlineScoreboards -- wave-I partfile 04: the filter-toggle setup family.
//   SetupCategories @0x8248F838  (DWARF cpp:1277)
//   SetupIndexes    @0x8248F8D8  (DWARF cpp:1299)
//   SetupVariations @0x8248F998  (DWARF cpp:1330)
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineScoreboards.h"
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuToggle.h"

namespace BrnGui
{

    namespace
    {
        // The three rows of mFilterToggleGroup, in the order the screen's apt movie lays
        // them out. The X360 passes each as a literal 0/1/2 to SetupToggle / HighlightItem /
        // GetSelectable, and indexes KAPC_FILTER_TITLE_STRINGS with the same value
        // (off_82F2687C[0] / [1] / [2] == "$SCOREBOARD_EVENT" / "$SCOREBOARD_FILTER" /
        // "$SCOREBOARD_ROAD").
        const s32 KI_FILTER_ROW_CATEGORY  = 0;
        const s32 KI_FILTER_ROW_INDEX     = 1;
        const s32 KI_FILTER_ROW_VARIATION = 2;
    }

    // ================================================================================
    //  SetupCategories  @ 0x8248F838  (DWARF cpp:1277)
    //
    //  Re-stock the leaderboard-category filter row from the category names the back end
    //  sent, then park its option cursor on the category the screen is currently showing.
    // ================================================================================
    void OnlineScoreboards::SetupCategories()
    {
        // X360 argument registers: r4 = row, r5 = miMaxCategories (option count),
        // r6 = 1 (active), r7 = the title string, r8 = mapcCategories, r9 = 0 (no id
        // array -- the row falls back to the option indices).
        mFilterToggleGroup.SetupToggle(KI_FILTER_ROW_CATEGORY, miMaxCategories, true,
                                       KAPC_FILTER_TITLE_STRINGS[KI_FILTER_ROW_CATEGORY],
                                       mapcCategories, 0);

        // The X360 inlines MenuToggleGroupVarSize<3>::HighlightItem here -- GetSelectable(0),
        // the row's mItemText.HighlightIndex through its inner group vtable slot 12, then
        // row +0xC |= 0x10 on success. The declared group method IS exactly that body, so
        // this is the inlining reversed, not a re-implementation.
        mFilterToggleGroup.HighlightItem(KI_FILTER_ROW_CATEGORY, miCurrentCategory);
    }

    // ================================================================================
    //  SetupIndexes  @ 0x8248F8D8  (DWARF cpp:1299)
    //
    //  The same for the middle filter row (the category's sub-index, e.g. which of a
    //  category's boards), with the extra rule that a row offering a single choice is
    //  taken out of the filter row's highlight cycle.
    // ================================================================================
    void OnlineScoreboards::SetupIndexes()
    {
        mFilterToggleGroup.SetupToggle(KI_FILTER_ROW_INDEX, miMaxIndexes, true,
                                       KAPC_FILTER_TITLE_STRINGS[KI_FILTER_ROW_INDEX],
                                       mapcIndexes, 0);

        // HighlightItem inlined by the X360, as in SetupCategories.
        mFilterToggleGroup.HighlightItem(KI_FILTER_ROW_INDEX, miCurrentIndex);

        // One index is no choice at all, so the row stops accepting the highlight. The X360
        // hoists the test above the call and so emits GetSelectable(1) once per arm with a
        // different `li r4`; it is one logical call. The vtable slot dispatched is the row's
        // +4 == Selectable slot 1 == SetHighlightable(bool).
        mFilterToggleGroup.GetSelectable(KI_FILTER_ROW_INDEX)
            ->SetHighlightable(miMaxIndexes > 1);
    }

    // ================================================================================
    //  SetupVariations  @ 0x8248F998  (DWARF cpp:1330)
    //
    //  The third filter row is optional: with a single variation there is nothing to pick,
    //  so the row is switched off entirely rather than stocked. Its title depends on what
    //  the variations mean for this board -- events for an event leaderboard, otherwise the
    //  per-road list.
    // ================================================================================
    void OnlineScoreboards::SetupVariations()
    {
        if (miMaxVariations > 1)
        {
            // Row vtable +0 == Selectable slot 0 == SetActive(bool).
            mFilterToggleGroup.GetSelectable(KI_FILTER_ROW_VARIATION)->SetActive(true);

            const char* lpacTitle = mbEventLeaderboard
                                        ? "$HUD_INFO_EVENT"
                                        : KAPC_FILTER_TITLE_STRINGS[KI_FILTER_ROW_VARIATION];

            mFilterToggleGroup.SetupToggle(KI_FILTER_ROW_VARIATION, miMaxVariations, true,
                                           lpacTitle, mapcVariations, 0);

            // Unlike the two rows above, the X360 leaves this HighlightItem out of line
            // (a real `bl` to MenuToggleGroupVarSize<3>::HighlightItem) -- same call either
            // way here.
            mFilterToggleGroup.HighlightItem(KI_FILTER_ROW_VARIATION, miCurrentVariation);
        }
        else
        {
            mFilterToggleGroup.GetSelectable(KI_FILTER_ROW_VARIATION)->SetActive(false);
        }
    }
}

// FOLDED FROM BrnOnlineScoreboards_wI_06.cpp
// ===================================================================================
// BrnGui::OnlineScoreboards -- wave-I partfile 06: the request ladder and the permanent
// event pump.
//   HandleLeaderboardRequestState @0x8249F458  DWARF cpp:953  (assert cpp:1155)
//   UpdatePermanent               @0x824A96A0  DWARF cpp:590  (asserts 614/698/711/726/730)
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (asm arbitrates; Hex-Rays is a
// hint only). Two corrections to the understand-phase spec, both read off the asm:
//   * event 122's status field is read with `lbz r11, 0x10(r29)` -- a ONE-BYTE flag at
//     payload+16, not an s32 word. On the big-endian console an s32 read would have been
//     `lwz`; testing a host s32 would test four bytes where the console tests one.
//   * event 119's "have we got a target?" test and event 122's name-clear are `lbz`/`stb` on
//     mCurrentTargetScorePlayerName -- byte-wide on the name's leading character, not the
//     leading word the spec described.
// Every record size below is a host sizeof of the record type; the console's 16 / 32
// immediates appear only in comments.
//
// HEADER-COLLISION NOTE (resolved during wave-I consolidation): this file was written while
// BrnGuiEventTypeDefs.h and BrnGuiDemangledEventTypes.h both defined GuiAudioTriggerEvent
// and GuiEventRunFsm, which made including both a hard C2011. The catalogue's two shadowing
// placeholders have since been removed in favour of the real hand-reconstructed homes, so
// that collision no longer exists. The file-local wire views below are kept as written --
// they are correct and self-contained -- but they are now an option, not a necessity.
//
// SUPERSEDED CROSS-TU NOTE: an earlier version of this banner argued that
// GuiTelemetryEvent should be { s32 miTelemetryType; char macParams[16]; } with NO
// GuiEvent<323> base, and that ids 111/112/113 were "correctly sized at 4 bytes". That
// reading measured the console PAYLOAD only. The conductor re-dumped the four
// OutputGuiEvent instantiation bodies (0x824940E8 / 0x82493FF8 / 0x82493D38 / 0x82493F88):
// each builds the record {payload_size, id, 12} followed by the payload and calls AddEvent
// with 12 + payload_size (0x20 / 0x10 / 0x1C / 0x30). Because the committed
// StateInterface::OutputGuiEvent passes sizeof(TEvent) and prepends NOTHING, the host type
// must CONTAIN the 12-byte header -- i.e. the GuiEvent<N> base. The catalogue shapes were
// corrected accordingly; do not "fix" them back.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineScoreboards.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"               // CgsGui::GuiEventTimeInfo (id 26)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface / GuiEventNetworkSuspension
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / the in-queue
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache (pulls BrnGuiEventTypeDefs.h)
#include "GameSource/GameState/BrnCgsPlayerName.h"                        // CgsNetwork::KI_USERNAME_LENGTH
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // the homed 111/112/113 + 323 wire records

#include <cstddef>                                                        // offsetof
#include <cstring>                                                        // std::memcpy

// BrnGuiDemangledEventTypes.h is deliberately NOT included: it hard-collides with
// BrnGuiEventTypeDefs.h (GuiAudioTriggerEvent / GuiEventRunFsm / the two road-rule records
// are defined in both), and BrnGuiCache.h above already puts this TU on the TypeDefs side.
// The four Demangled payload shapes needed here are carried as file-local wire views below.
// CgsGui::GuiEventNetworkSuspension lives in CgsGuiStateInterface.h and is safe either way.

// ⭐ 2026-09-16 -- the reduced local re-declaration of
// BrnNetwork::BrnNetworkModuleIO::TelemetryData that used to sit here is GONE. It existed
// because "the record layout is owned by the network telemetry TU"; that TU has since
// landed and owns the real type (Network/SharedIO/BrnNetworkSharedIO.h:197, with meHook,
// macBuffer[16], Construct, both AddParameter overloads and a size static_assert). Keeping
// the stripped copy made this TU a hard C2011 redefinition the moment it was mounted --
// the documented reduced-re-declaration trap. Include the real home instead.
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"   // the REAL TelemetryData

namespace BrnGui
{
    namespace
    {
        // ---- the state in-queue (CgsGui::State::mpInGuiEventQueue's real instantiation) ---
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // ---- AddEvent channels (the out-queue selector word) ---------------------------


        // ---- in-queue ids this pump acts on --------------------------------------------
        // The screen registers for {14, 21, 6, 44, 26, 64, 116..119, 122, 123}; the ids below
        // are the subset UpdatePermanent handles (6 goes to UpdateRunning, 14/21/64 to the
        // The six leaderboard reply ids are NOT anonymous: they are named in the recovered
        // DWARF and homed in BrnGuiDemangledEventTypes.h (116 category / 117 index /
        // 118 variation / 119 table / 122 target-score / 123 downloaded-challengeable).
        // An earlier draft of this banner claimed they were absent from the DWARF slice --
        // that was false; do not reinstate it.
        const s32 KI_EVENT_TIME_INFO                        = 26;
        const s32 KI_EVENT_NETWORK_DISCONNECTED             = 44;
        const s32 KI_EVENT_SCOREBOARD_CATEGORY_LIST         = 116;
        const s32 KI_EVENT_SCOREBOARD_INDEX_LIST            = 117;
        const s32 KI_EVENT_SCOREBOARD_VARIATION_LIST        = 118;
        const s32 KI_EVENT_SCOREBOARD_TABLE_DATA            = 119;
        const s32 KI_EVENT_SCOREBOARD_TARGET_SCORE_RESPONSE = 122;
        const s32 KI_EVENT_SCOREBOARD_DLD_CHALLENGEABLE     = 123;

        // ---- out-queue id --------------------------------------------------------------
        // The "send me the leaderboard category list" command (X360 `li r11, 0x6E`).
        const s32 KI_EVENT_SCOREBOARD_REQUEST_CATEGORIES = 110;

        // ---- telemetry -----------------------------------------------------------------
        // The telemetry record this screen raises when it asks for a table (X360 `li r11, 0x2B`).
        // FLAG: consumer-named -- the telemetry-type enum's home is not recovered.
        const s32 KI_TELEMETRY_TYPE_SCOREBOARD_TABLE = 43;
        // Scratch length the X360 int-parameter wrapper passes to SPrintf (`li r4, 0x10`).
        const u32 KU_TELEMETRY_PARAM_LENGTH = 16;

        // The loading-icon state identifier the request ladder pushes (aLoading_0).

        // The two state-machine events the disconnect path raises on the owning flow.

        // ---- out-queue wire records -----------------------------------------------------
        // The bare one-byte-payload command record { 1, N, 12, <payload byte> }, X360 record
        // size 16 (the wH_00 template, shared with the two pagers in group 01). The X360
        // stack-builds ONLY the three header words, so the payload byte keeps whatever the
        // reused stack slot held -- the constructor leaves it alone rather than inventing a
        // stored value.




        // ---- out-queue wire records -----------------------------------------------------
        // The bare one-byte-payload command record { 1, N, 12, <payload byte> }, X360 record
        // size 16 (the wH_00 template, shared with the two pagers in group 01). The X360
        // stack-builds ONLY the three header words, so the payload byte keeps whatever the
        // reused stack slot held -- the constructor leaves it alone rather than inventing a
        // stored value.










        // ---- in-queue payload views ------------------------------------------------------
        // The queue hands back HEADER-STRIPPED payloads, so these views start at the payload's
        // first field. Each group redeclares only the ones it reads.

        // ev 26 -- the per-frame tick -- needs no view here: its payload has a committed home,
        // CgsGui::GuiEventTimeInfo (CgsGuiEventTypeDefs.h, static_assert sizeof == 8). The X360
        // reads the frame delta with `lfs f13, 0(r29)` @0x824A9994, which is that type's
        // mfTimeDelta at +0x00, i.e. exactly what GetTimeStep() returns.

        // ev 122 -- the reply to a "who is the score target?" request. X360 memcpy's 16 bytes
        // from +0 and tests +0x10 with `lbz`, so the status is a single byte, not a word.
        // FLAG: consumer-named. Nonzero means no target came back.
        struct TargetScoreResponsePayload
        {
            char macPlayerName[CgsNetwork::KI_USERNAME_LENGTH];   // +0x00
            u8   mu8Status;                                       // +0x10
        };

        // ev 123 -- "this player is challengeable". Only the name is read (memcpy 16 from +0).
        struct DldChallengeablePayload
        {
            char macPlayerName[CgsNetwork::KI_USERNAME_LENGTH];   // +0x00
        };

        // X360 sub_82354200 -- the out-of-line "append one integer telemetry parameter"
        // wrapper: SPrintf "%i" into a 16-byte scratch, then TelemetryData::AddParameter.
        // (The "%i" comes from the raw asm's aI operand, not the pseudocode.) It has no
        // recovered home of its own, so it is carried file-locally; the committed
        // GameBridgeGUIToX.cpp case-286 site spells the same two calls out inline.
        void AddIntTelemetryParameter(BrnNetwork::BrnNetworkModuleIO::TelemetryData* lpTelemetry,
                                      s32 liValue)
        {
            char lacParameter[KU_TELEMETRY_PARAM_LENGTH];
            CgsCore::SPrintf(lacParameter, KU_TELEMETRY_PARAM_LENGTH, "%i", liValue);
            lpTelemetry->AddParameter(lacParameter);
        }
    }

    // ================================================================================
    //  HandleLeaderboardRequestState  @ 0x8249F458  (cpp:953)
    //
    //  The leaderboard request ladder. Each GET_* arm fires one request at the network
    //  back end, resets the table to its loading look, and drops through into the matching
    //  WF_* arm, which is nothing but the "now wait for the reply" store. The replies land
    //  in UpdatePermanent, which steps the ladder on and calls back in here.
    // ================================================================================
    void OnlineScoreboards::HandleLeaderboardRequestState()
    {
        switch (meLeaderboardRequestState)
        {
            case E_LEADERBOARD_REQUEST_STATE_GET_CATEGORIES:
            {
                GuiCommandWire16<KI_EVENT_SCOREBOARD_REQUEST_CATEGORIES> lCategoriesRequest;
                mpStateInterface->OutputGuiEvent(lCategoriesRequest);   // X360 record size 16

                mTable.SetupScoreboard(0);
                // SetLoading(true) inlined, DWARF cpp:1362
                if (mbCurrentlyLoading != true)
                {
                    mbCurrentlyLoading = true;
                    mLoadingIcon.SetState("loading");
                }
                ShowTableEmpty(false);
            }
            // X360 falls straight through into the WF_CATEGORIES arm, which is the shared
            // "wait for the reply" store -- mirrored here rather than duplicated.
            // fall through
            case E_LEADERBOARD_REQUEST_STATE_WF_CATEGORIES:
                meLeaderboardRequestState = E_LEADERBOARD_REQUEST_STATE_WF_CATEGORIES;
                break;

            case E_LEADERBOARD_REQUEST_STATE_GET_INDEX:
            {
                GuiEventScoreboardRequestIndexEvent lIndexRequest;
                lIndexRequest.miCategory = miCurrentCategory;   // OutputGuiEvent: {4,111,12}+4
                mpStateInterface->OutputGuiEvent(lIndexRequest);

                mTable.SetupScoreboard(0);
                // SetLoading(true) inlined, DWARF cpp:1362
                if (mbCurrentlyLoading != true)
                {
                    mbCurrentlyLoading = true;
                    mLoadingIcon.SetState("loading");
                }
                ShowTableEmpty(false);
            }
            // fall through
            case E_LEADERBOARD_REQUEST_STATE_WF_INDEX:
                meLeaderboardRequestState = E_LEADERBOARD_REQUEST_STATE_WF_INDEX;
                break;

            case E_LEADERBOARD_REQUEST_STATE_GET_VARIATIONS:
            {
                GuiEventScoreboardRequestVariationEvent lVariationRequest;
                lVariationRequest.miIndex = miCurrentIndex;   // OutputGuiEvent: {4,112,12}+4
                mpStateInterface->OutputGuiEvent(lVariationRequest);

                mTable.SetupScoreboard(0);
                // SetLoading(true) inlined, DWARF cpp:1362
                if (mbCurrentlyLoading != true)
                {
                    mbCurrentlyLoading = true;
                    mLoadingIcon.SetState("loading");
                }
                ShowTableEmpty(false);
            }
            // fall through
            case E_LEADERBOARD_REQUEST_STATE_WF_VARIATIONS:
                meLeaderboardRequestState = E_LEADERBOARD_REQUEST_STATE_WF_VARIATIONS;
                break;

            case E_LEADERBOARD_REQUEST_STATE_GET_TABLE:
            {
                // A per-road board lists its roads alphabetically, so the row the player is
                // sitting on has to be mapped back through the sort order before the back end
                // is asked for it; every other board indexes its variations directly.
                const s32 liTargetVariation = mbPerRoadLeaderboard
                    ? maiAlphabeticalRoadIndex[miCurrentVariation]
                    : miCurrentVariation;

                GuiEventScoreboardRequestTableEvent lTableRequest;
                lTableRequest.miVariation = liTargetVariation;   // OutputGuiEvent: {4,113,12}+4
                mpStateInterface->OutputGuiEvent(lTableRequest);

                // ...and log which board was asked for. The telemetry record's parameter
                // buffer starts one word past its type, and TelemetryData is the accessor for
                // that buffer, so the builder addresses the record through it.
                CGS_STATIC_ASSERT((sizeof(GuiTelemetryEvent) == 20),
                              "event 323 payload is 20 bytes");
                CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiTelemetryEvent>) == 32),
                              "event 323 record is the three wrapper words plus the payload");
                CGS_STATIC_ASSERT((offsetof(GuiTelemetryEvent, macParams) == 4),
                              "event 323 parameter string follows the payload type word");
                GuiTelemetryEvent lTelemetry;
                lTelemetry.miTelemetryType = KI_TELEMETRY_TYPE_SCOREBOARD_TABLE;
                lTelemetry.macParams[0] = 0;
                BrnNetwork::BrnNetworkModuleIO::TelemetryData* lpTelemetry =
                    reinterpret_cast<BrnNetwork::BrnNetworkModuleIO::TelemetryData*>(
                        &lTelemetry.miTelemetryType);

                AddIntTelemetryParameter(lpTelemetry, miCurrentCategory);
                AddIntTelemetryParameter(lpTelemetry, miCurrentIndex);
                AddIntTelemetryParameter(lpTelemetry, liTargetVariation);

                mpStateInterface->OutputGuiEvent(lTelemetry);
            }
            // fall through
            case E_LEADERBOARD_REQUEST_STATE_WF_TABLE:
                meLeaderboardRequestState = E_LEADERBOARD_REQUEST_STATE_WF_TABLE;
                break;

            case E_LEADERBOARD_REQUEST_STATE_DONE:
                // Everything has arrived; the ladder idles here until an input restarts it.
                break;

            default:
                // The X360 streams the offending state value into the message
                // ("invalid request state : " << meLeaderboardRequestState << "\n"); project
                // policy lowers a streamed assert to its static text. Non-fatal.
                CGS_ASSERT(false, "invalid request state : ");
                break;
        }
    }

    // ================================================================================
    //  UpdatePermanent  @ 0x824A96A0  (cpp:590)
    //
    //  The always-on event pump: it runs in every sub-state, drains nothing (the owning
    //  Update() clears the in-queue once, after this and UpdateRunning have both walked
    //  it), and finishes by stepping the request ladder.
    // ================================================================================
    void OnlineScoreboards::UpdatePermanent()
    {

        const CgsModule::Event* lpEvent;
        s32 liSize;

        for (s32 liEventId = mpInGuiEventQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = mpInGuiEventQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            switch (liEventId)
            {
                case KI_EVENT_TIME_INFO:
                    // The table is asked for a short beat after the variation list arrives, so
                    // a player scrolling the filters does not fire a request per row.
                    if (mfCurrentTableWaitTime > 0.0f)
                    {
                        mfCurrentTableWaitTime -=
                            reinterpret_cast<const CgsGui::GuiEventTimeInfo*>(lpEvent)
                                ->GetTimeStep();

                        // PPC NaN polarity: the X360 skips the store on `bgt` alone, so an
                        // unordered compare falls INTO it. `<= 0.0f` is false for a NaN and
                        // would invert that path, hence the negated ordered predicate.
                        if (!(mfCurrentTableWaitTime > 0.0f))
                        {
                            meLeaderboardRequestState = E_LEADERBOARD_REQUEST_STATE_GET_TABLE;
                        }
                    }
                    break;

                case KI_EVENT_NETWORK_DISCONNECTED:
                    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

                    // Latch the error the disconnect popup will show. The committed GuiCache
                    // inline IS the X360's store here (the event's leading word, or 0 when no
                    // event rode along).
                    mpGuiCache->SetDoDisconnectPopup(lpEvent);

                    if (mpGuiCache->IsOnlineStartPending())
                    {
                        // A session start was already in flight, so let the network layer run
                        // again on the way out and take the quiet exit.
                        CgsGui::GuiEventNetworkSuspension lResumeNetwork;
                        lResumeNetwork.meSuspensionType = CgsGui::GuiEventNetworkSuspension::E_SUSPENSION_TYPE_SUSPEND;
                        mpStateInterface->OutputGuiEvent(lResumeNetwork);   // X360 record size 16

                        mpGuiCache->SetOnlineStartPending(false);
                        SendStateEvent("GO_BACK_EASY");
                    }
                    else
                    {
                        SendStateEvent("GO_BACK");
                    }
                    break;

                case KI_EVENT_SCOREBOARD_CATEGORY_LIST:
                    HandleCategoryList(lpEvent);
                    meLeaderboardRequestState = E_LEADERBOARD_REQUEST_STATE_GET_INDEX;
                    break;

                case KI_EVENT_SCOREBOARD_INDEX_LIST:
                    HandleIndexList(lpEvent);
                    meLeaderboardRequestState = E_LEADERBOARD_REQUEST_STATE_GET_VARIATIONS;
                    break;

                case KI_EVENT_SCOREBOARD_VARIATION_LIST:
                    HandleVariationList(lpEvent);

                    // The variation list is the last thing the filters need, so the table
                    // request is armed on a timer and the table goes back to its loading look.
                    mfCurrentTableWaitTime = KF_DELAY_TO_REQUEST_TABLE;
                    mTable.SetupScoreboard(0);
                    // SetLoading(true) inlined, DWARF cpp:1362
                    if (mbCurrentlyLoading != true)
                    {
                        mbCurrentlyLoading = true;
                        mLoadingIcon.SetState("loading");
                    }
                    // The X360 inlines ShowTableEmpty's body here; the method is its home.
                    ShowTableEmpty(false);
                    break;

                case KI_EVENT_SCOREBOARD_TABLE_DATA:
                    HandleTableData(lpEvent);
                    meLeaderboardRequestState = E_LEADERBOARD_REQUEST_STATE_DONE;

                    // Re-apply the highlighted target to the freshly filled table. X360 `lbz`
                    // on the name -- the "is it empty?" test is its leading character.
                    if (mCurrentTargetScorePlayerName.macName[0] != 0)
                    {
                        mTable.SetTargetGamertag(mCurrentTargetScorePlayerName.macName);
                    }
                    break;

                case KI_EVENT_SCOREBOARD_TARGET_SCORE_RESPONSE:
                    // Unsolicited replies are ignored; only the screen that asked cares.
                    if (mbWaitingForTargetScoreResponse)
                    {
                        CGS_ASSERT_AT(lpEvent != 0, "lpTargetScoreResponse", CGS_ARTIST_FILE, 698);

                        const TargetScoreResponsePayload* lpTargetScoreResponse =
                            reinterpret_cast<const TargetScoreResponsePayload*>(lpEvent);

                        if (lpTargetScoreResponse->mu8Status != 0)
                        {
                            // No target came back -- empty the cached name (X360 `stb` of 0
                            // onto its leading character).
                            mCurrentTargetScorePlayerName.macName[0] = 0;
                        }
                        else
                        {
                            std::memcpy(mCurrentTargetScorePlayerName.macName,
                                        lpTargetScoreResponse->macPlayerName,
                                        sizeof(mCurrentTargetScorePlayerName.macName));
                            CGS_ASSERT_AT(mCurrentTargetScorePlayerName.macName[0] != 0,
                                       "!mCurrentTargetScorePlayerName.IsEmpty()", CGS_ARTIST_FILE, 711);
                        }

                        mTable.SetTargetGamertag(mCurrentTargetScorePlayerName.macName);
                    }
                    break;

                case KI_EVENT_SCOREBOARD_DLD_CHALLENGEABLE:
                {
                    CGS_ASSERT_AT(lpEvent != 0, "lpDldChallengeableEvent", CGS_ARTIST_FILE, 726);

                    const DldChallengeablePayload* lpDldChallengeable =
                        reinterpret_cast<const DldChallengeablePayload*>(lpEvent);

                    std::memcpy(mCurrentTargetScorePlayerName.macName,
                                lpDldChallengeable->macPlayerName,
                                sizeof(mCurrentTargetScorePlayerName.macName));
                    CGS_ASSERT_AT(mCurrentTargetScorePlayerName.macName[0] != 0,
                               "!mCurrentTargetScorePlayerName.IsEmpty()", CGS_ARTIST_FILE, 730);

                    mTable.SetTargetGamertag(mCurrentTargetScorePlayerName.macName);
                    break;
                }

                default:
                    break;
            }
        }

        // Whatever the replies changed, the ladder gets its turn at the end of the pass.
        HandleLeaderboardRequestState();
    }
}

// FOLDED FROM BrnOnlineScoreboards_wI_07.cpp
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
                // The {16,120,12}+name[16] record on channel 40.
                mpStateInterface->OutputGuiEvent(lRequestGamerCard);
                break;
            }

            default:
                // Every other input is ignored in table mode.
                break;
        }
    }
}

// FOLDED FROM BrnOnlineScoreboards_wS34_00.cpp
// ===================================================================================
// BrnGui::OnlineScoreboards -- stub-wave partfile 00: the internal-state pump, the four
// scoreboard responses and the filter-mode input.
//   HandleTableData
//   HandleCategoryList
//   HandleIndexList
//   HandleVariationList
//   HandleControllerInputPressedUsingFilters
//   Update
// Reconstructed from the console image; the assembly arbitrates over the decompiler.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineScoreboards.h"

#include <cstring>                                                        // memcpy / strlen / strcpy
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStreamBase
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface, GuiEventNetworkSuspension
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // the state in-queue
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiShared.h"                                  // gGuiResourceIdentifier
#include "GameSource/Input/GameInputActions.h"                            // E_GAMEINPUTACTIONS_GUI_*
#include "GameSource/Network/Managers/BrnNetworkScoreboard.h"             // BrnNetwork::Scoreboard

namespace BrnGui
{
    namespace
    {
        // The state in-queue concrete type (GuiEventQueue is its pointer-only face).
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // The apt movie the screen plays, and its level.
        // KI_APT_MOVIE_LEVEL is declared above in this translation unit.

        // The three filter rows of mFilterToggleGroup.
        const s32 KI_FILTER_CATEGORY  = 0;
        const s32 KI_FILTER_INDEX     = 1;
        const s32 KI_FILTER_VARIATION = 2;
        const s32 KI_NUM_FILTERS      = 3;

        // One leaderboard name in the response lists (31 chars including the terminator).
        const s32 KI_LIST_NAME_LENGTH = 31;

        // Controller event payload: the action id rides in the second word.
        struct ControllerButtonPayload : public CgsModule::Event
        {
            s32 miDevice;     // +0x00
            s32 miButtonId;   // +0x04
        };

        // Event payload views (the queue hands out header-stripped payloads).
        struct GuiEventScoreboardResponseCategoryPayload : public CgsModule::Event
        {
            s32  miNumCategories;                                                     // +0x000
            char maacCategories[OnlineScoreboards::KI_MAX_CATEGORIES][KI_LIST_NAME_LENGTH];  // +0x004
        };

        struct GuiEventScoreboardResponseIndexPayload : public CgsModule::Event
        {
            s32  miNumIndexes;                                                        // +0x000
            char maacIndexes[OnlineScoreboards::KI_MAX_INDEXES][KI_LIST_NAME_LENGTH];  // +0x004
        };

        struct GuiEventScoreboardResponseVariationPayload : public CgsModule::Event
        {
            s32  miNumVariations;                                                         // +0x000
            char maacVariations[OnlineScoreboards::KI_MAX_VARIATIONS][KI_LIST_NAME_LENGTH];  // +0x004
            bool mbPerRoadLeaderboard;                                                    // +0x802
            bool mbEventLeaderboard;                                                      // +0x803
        };

        bool IsDebugPrintOn()
        {
            return (CgsDev::Message::gxMessageFilterFlags & 1) != 0;
        }

        // The banner every response handler prints around its list dump.
        void PrintListBanner(const char* lpacRule, const char* lpacTitle)
        {
            if (IsDebugPrintOn()) *CgsDev::Log::gpDebugPrint << "\n\n\n";
            if (IsDebugPrintOn()) *CgsDev::Log::gpDebugPrint << lpacRule;
            if (IsDebugPrintOn()) *CgsDev::Log::gpDebugPrint << lpacTitle;
            if (IsDebugPrintOn()) *CgsDev::Log::gpDebugPrint << lpacRule;
        }

        void PrintNameList(const char* lpacRule, const char* lpacEntryLabel,
                           const char (*lpaacNames)[KI_LIST_NAME_LENGTH], s32 liCount)
        {
            if (IsDebugPrintOn())
                *CgsDev::Log::gpDebugPrint << "Number in list : " << liCount << "\n";
            for (s32 liEntry = 0; liEntry < liCount; ++liEntry)
            {
                if (IsDebugPrintOn())
                {
                    *CgsDev::Log::gpDebugPrint << lpacEntryLabel << liEntry << "] : "
                                               << lpaacNames[liEntry] << "\n";
                }
            }
            if (IsDebugPrintOn()) *CgsDev::Log::gpDebugPrint << lpacRule;
            if (IsDebugPrintOn()) *CgsDev::Log::gpDebugPrint << "\n\n\n";
        }
    }

    // ================================================================================
    //  HandleTableData
    //
    //  The requested leaderboard table arrived: show it, drop the loading icon, flag an
    //  empty table and refresh the button prompts.
    // ================================================================================
    void OnlineScoreboards::HandleTableData(const CgsModule::Event* lpEvent)
    {
        const BrnNetwork::Scoreboard* lpScoreboard =
            reinterpret_cast<const BrnNetwork::Scoreboard*>(lpEvent);

        mTable.SetupScoreboard(lpScoreboard);

        if (mbCurrentlyLoading)
        {
            mbCurrentlyLoading = false;
            mLoadingIcon.SetState("invisible");
        }

        if (lpScoreboard->GetNumberOfRows() == 0)
            ShowTableEmpty(true);

        SetupButtons();

        const char* const lpacRule = "-------------------\n";
        PrintListBanner(lpacRule, "TABLE DATA RECEIVED\n");
        if (IsDebugPrintOn())
            *CgsDev::Log::gpDebugPrint << "Name : " << lpScoreboard->GetTitle() << "\n";
        if (IsDebugPrintOn())
            *CgsDev::Log::gpDebugPrint << "Titles :";
        for (s32 liColumn = 0; liColumn < lpScoreboard->GetNumberOfColumns(); ++liColumn)
        {
            if (IsDebugPrintOn())
                *CgsDev::Log::gpDebugPrint << "\t\t" << lpScoreboard->GetColumn(liColumn)->GetTitle();
        }
        if (IsDebugPrintOn())
            *CgsDev::Log::gpDebugPrint << "\n";

        for (s32 liRow = 0; liRow < lpScoreboard->GetNumberOfRows(); ++liRow)
        {
            if (IsDebugPrintOn())
                *CgsDev::Log::gpDebugPrint << "Row[" << liRow << "] :";
            const BrnNetwork::ScoreboardRow* lpRow = lpScoreboard->GetRow(liRow);
            for (s32 liColumn = 0; liColumn < lpScoreboard->GetNumberOfColumns(); ++liColumn)
            {
                if (IsDebugPrintOn())
                    *CgsDev::Log::gpDebugPrint << "\t\t" << lpRow->GetData(liColumn);
            }
            if (IsDebugPrintOn())
                *CgsDev::Log::gpDebugPrint << "\n";
        }

        if (IsDebugPrintOn()) *CgsDev::Log::gpDebugPrint << lpacRule;
        if (IsDebugPrintOn()) *CgsDev::Log::gpDebugPrint << "\n\n\n";
    }

    // ================================================================================
    //  HandleCategoryList
    // ================================================================================
    void OnlineScoreboards::HandleCategoryList(const CgsModule::Event* lpEvent)
    {
        const GuiEventScoreboardResponseCategoryPayload* lpList =
            static_cast<const GuiEventScoreboardResponseCategoryPayload*>(lpEvent);

        miMaxCategories = lpList->miNumCategories;
        std::memcpy(maacCategories, lpList->maacCategories,
                    static_cast<size_t>(KI_LIST_NAME_LENGTH * lpList->miNumCategories));

        const char* const lpacRule = "----------------------\n";
        PrintListBanner(lpacRule, "CATEGORY LIST RECEIVED\n");
        PrintNameList(lpacRule, "Category [", maacCategories, miMaxCategories);

        SetupCategories();
    }

    // ================================================================================
    //  HandleIndexList
    // ================================================================================
    void OnlineScoreboards::HandleIndexList(const CgsModule::Event* lpEvent)
    {
        const GuiEventScoreboardResponseIndexPayload* lpList =
            static_cast<const GuiEventScoreboardResponseIndexPayload*>(lpEvent);

        miMaxIndexes = lpList->miNumIndexes;
        std::memcpy(maacIndexes, lpList->maacIndexes,
                    static_cast<size_t>(KI_LIST_NAME_LENGTH * lpList->miNumIndexes));
        if (miCurrentIndex >= miMaxIndexes)
            miCurrentIndex = 0;

        const char* const lpacRule = "-------------------\n";
        PrintListBanner(lpacRule, "INDEX LIST RECEIVED\n");
        PrintNameList(lpacRule, "Index [", maacIndexes, miMaxIndexes);

        SetupIndexes();
    }

    // ================================================================================
    //  HandleVariationList
    //
    //  A per-road leaderboard's variations are the roads: they are copied in the
    //  alphabetical order BuildAlphabeticalRoadIndexes worked out.
    // ================================================================================
    void OnlineScoreboards::HandleVariationList(const CgsModule::Event* lpEvent)
    {
        const GuiEventScoreboardResponseVariationPayload* lpList =
            static_cast<const GuiEventScoreboardResponseVariationPayload*>(lpEvent);

        miMaxVariations = lpList->miNumVariations;
        if (lpList->mbPerRoadLeaderboard)
        {
            for (s32 liRoadIndex = 0; liRoadIndex < miMaxVariations; ++liRoadIndex)
            {
                CGS_ASSERT_AT(maiAlphabeticalRoadIndex[liRoadIndex] >= 0,
                           "maiAlphabeticalRoadIndex[ liRoadIndex ] >= 0", CGS_ARTIST_FILE, 1269);
                CGS_ASSERT_AT(maiAlphabeticalRoadIndex[liRoadIndex] < KI_MAX_VARIATIONS,
                           "maiAlphabeticalRoadIndex[ liRoadIndex ] < GuiEventScoreboardResponseVariationEvent::KI_MAX_VARIATIONS", CGS_ARTIST_FILE, 1270);

                const char* lpacRoad = lpList->maacVariations[maiAlphabeticalRoadIndex[liRoadIndex]];
                // The console's checked string copy: assert the name fits, then copy it.
                CGS_ASSERT(std::strlen(lpacRoad) < static_cast<size_t>(KI_LIST_NAME_LENGTH),
                           "String is too long");
                std::strcpy(maacVariations[liRoadIndex], lpacRoad);
            }
        }
        else
        {
            std::memcpy(maacVariations, lpList->maacVariations,
                        static_cast<size_t>(KI_LIST_NAME_LENGTH * lpList->miNumVariations));
        }

        mbPerRoadLeaderboard = lpList->mbPerRoadLeaderboard;
        mbEventLeaderboard   = lpList->mbEventLeaderboard;
        if (miCurrentVariation >= miMaxVariations)
            miCurrentVariation = 0;

        const char* const lpacRule = "-----------------------\n";
        PrintListBanner(lpacRule, "VARIATION LIST RECEIVED\n");
        PrintNameList(lpacRule, "Variation [", maacVariations, miMaxVariations);

        SetupVariations();
    }

    // ================================================================================
    //  HandleControllerInputPressedUsingFilters
    //
    //  Filter mode: up/down move between the three filter rows, left/right change the
    //  highlighted filter (and request the next rung of the leaderboard ladder), select
    //  drops into the table, cancel leaves, and the two option buttons page the table.
    // ================================================================================
    void OnlineScoreboards::HandleControllerInputPressedUsingFilters(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, "Invalid event sent to OnlineScoreboards::HandleControllerInput");

        const s32 leAction = static_cast<const ControllerButtonPayload*>(lpEvent)->miButtonId;

        switch (leAction)
        {
            case E_GAMEINPUTACTIONS_GUI_UP:
                if (mFilterToggleGroup.HighlightPrevious(false))
                    TriggerSound(leAction);
                miCurrentFilterHighlighted = mFilterToggleGroup.GetHighlightedIndex();
                break;

            case E_GAMEINPUTACTIONS_GUI_DOWN:
                if (mFilterToggleGroup.HighlightNext(false))
                    TriggerSound(leAction);
                miCurrentFilterHighlighted = mFilterToggleGroup.GetHighlightedIndex();
                break;

            case E_GAMEINPUTACTIONS_GUI_LEFT:
            case E_GAMEINPUTACTIONS_GUI_RIGHT:
            {
                const bool lbChanged = (leAction == E_GAMEINPUTACTIONS_GUI_LEFT)
                                           ? mFilterToggleGroup.HighlightPreviousItem()
                                           : mFilterToggleGroup.HighlightNextItem();
                if (!lbChanged)
                    break;

                TriggerSound(leAction);
                const s32 liFilter = mFilterToggleGroup.GetHighlightedIndex();
                if (liFilter == KI_FILTER_CATEGORY)
                {
                    meLeaderboardRequestState = E_LEADERBOARD_REQUEST_STATE_GET_INDEX;
                    miCurrentCategory =
                        mFilterToggleGroup.GetSelectable(KI_FILTER_CATEGORY)->mItemText.GetHighlightedIndex();
                }
                else if (liFilter == KI_FILTER_INDEX)
                {
                    meLeaderboardRequestState = E_LEADERBOARD_REQUEST_STATE_GET_VARIATIONS;
                    miCurrentIndex =
                        mFilterToggleGroup.GetSelectable(KI_FILTER_INDEX)->mItemText.GetHighlightedIndex();
                }
                else if (static_cast<u32>(liFilter) < static_cast<u32>(KI_NUM_FILTERS))
                {
                    // A new road: wait a moment before asking for its table.
                    mfCurrentTableWaitTime = KF_DELAY_TO_REQUEST_TABLE;
                    mTable.SetupScoreboard(0);
                    if (!mbCurrentlyLoading)
                    {
                        mbCurrentlyLoading = true;
                        mLoadingIcon.SetState("loading");
                    }
                    ShowTableEmpty(false);
                    miCurrentVariation =
                        mFilterToggleGroup.GetSelectable(liFilter)->mItemText.GetHighlightedIndex();
                }
                else
                {
                    // The console streams the filter index after this text.
                    CGS_ASSERT(false, "Invalid filter selected : ");
                }

                // A changed filter forgets the challenge target.
                mCurrentTargetScorePlayerName.macName[0] = 0;
                mbWaitingForTargetScoreResponse = false;
                break;
            }

            case E_GAMEINPUTACTIONS_GUI_SELECT:
                // Into the table, when it has rows.
                if (mTable.GetRowsUsed() > 0)
                {
                    mbUsingFilters = false;
                    mTable.SetHighlight(0);
                    mTable.DrawScoreboard();
                    mFilterToggleGroup.HighlightIndex(-1);
                    SetupButtons();
                }
                break;

            case E_GAMEINPUTACTIONS_GUI_CANCEL:
                // Leaving while an online start is pending also suspends the network.
                if (mpGuiCache->IsOnlineStartPending())
                {
                    CgsGui::GuiEventNetworkSuspension lSuspend;
                    lSuspend.meSuspensionType = CgsGui::GuiEventNetworkSuspension::E_SUSPENSION_TYPE_SUSPEND;
                    mpStateInterface->OutputGuiEvent(lSuspend);
                    mpGuiCache->SetOnlineStartPending(false);
                    SendStateEvent("GO_BACK_EASY");
                }
                else
                {
                    SendStateEvent("GO_BACK");
                }
                break;

            case E_GAMEINPUTACTIONS_GUI_OPTION0:
                PageDown(false);
                break;

            case E_GAMEINPUTACTIONS_GUI_OPTION1:
                PageUp(false);
                break;

            default:
                break;
        }
    }

    // ================================================================================
    //  Update
    //
    //  The internal-state machine. Each state falls into the next once its work is done,
    //  so a frame can run several of them; the permanent update and the queue clear run
    //  every frame.
    // ================================================================================
    void OnlineScoreboards::Update()
    {
        switch (meInternalState)
        {
            case E_INTERNALSTATE_GETCACHE:
                UpdateGetCache();
                // fall through
            case E_INTERNALSTATE_LOADRESOURCES:
                meInternalState = E_INTERNALSTATE_LOADRESOURCES;
                if (!mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad))
                    break;
                // fall through
            case E_INTERNALSTATE_PLAYSWF:
                meInternalState = E_INTERNALSTATE_PLAYSWF;
                ClearExpectedComponent();
                mFilterToggleGroup.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache, false);
                mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[maResourcesToLoad[0].muId],
                                               KI_APT_MOVIE_LEVEL);
                // fall through
            case E_INTERNALSTATE_WFINIT:
                meInternalState = E_INTERNALSTATE_WFINIT;
                if (!UpdateWFInit())
                    break;
                // fall through
            case E_INTERNALSTATE_WFDATA:
                meInternalState = E_INTERNALSTATE_WFDATA;
                if (meLeaderboardRequestState != E_LEADERBOARD_REQUEST_STATE_DONE)
                    break;
                // fall through
            case E_INTERNALSTATE_SETUPCOMPONENTS:
                UpdateSetupComponents();
                // fall through
            case E_INTERNALSTATE_RUNNING:
                meInternalState = E_INTERNALSTATE_RUNNING;
                UpdateRunning();
                break;

            case E_INTERNALSTATE_LEFT:
                break;

            default:
                // The console streams the state value after this text.
                CGS_ASSERT(false, "Invalid internal state : ");
                break;
        }

        UpdatePermanent();
        reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue)->Clear();
    }
}

#undef CGS_ARTIST_FILE
