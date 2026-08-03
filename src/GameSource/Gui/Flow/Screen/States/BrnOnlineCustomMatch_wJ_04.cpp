// ===================================================================================
// BrnGui::OnlineCustomMatch -- wave-J partfile 04: the three "list / no games" controller
// handlers of the online custom-match screen.
//   HandleControllerInputSelectGame     @0x82497570  (assert cpp:634)
//   HandleControllerInputNoGames        @0x82497880  (assert cpp:738)
//   HandleControllerInputNoGamesInGame  @0x82497A68  (assert cpp:830)
//
// Each body is COMPLETE, reconstructed store-for-store from the X360 ARTIST assembly, and
// all three now compile against the committed headers. The three shared-header changes the
// wave-J understand phase asked the conductor for (spec §8) have all landed:
//
//   §8.1  BrnGuiCache.h:803 -- `friend struct OnlineCustomMatch;` (plus the forward
//         declaration at h:55). All three bodies read GuiCache::mbOnlineMatchUnranked
//         (private, h:770; setter-only at h:456; `lbz r10, 0x4B52(r11)` at 0x824976F8 /
//         0x82497A10 / 0x82497BFC).
//   §8.2  BrnGuiDemangledEventTypes.h:374 -- GuiEventNetworkCustomMatchResults is the
//         headerless 604-byte record with its DWARF field names, so
//         mSearchResults.maiFoundGameIndex / .miNumGames resolve. (Needed by SelectGame.)
//   §8.3  BrnTable.h:210/:211 -- Table::HighlightNext() / HighlightPrevious(), the two
//         NO-ARGUMENT overrides the X360 dispatches through Table's own vtable slots
//         +0x34 / +0x38 (only r3 loaded at 0x824977E8 / 0x82497860), so the calls no longer
//         resolve to the inherited SelectableGroup::Highlight*(bool). (SelectGame.)
//
// WIRE SHAPES MEASURED ON THE HOST, NOT ASSUMED (scratchpad/waveJ/probe/g04_sizes.cpp).
// Every host size/offset below matches the X360's AddEvent immediates, and none of them is
// written into the code as a console literal (they are all offsetof / sizeof expressions):
// join wire payload @12 / record 16, id-253 stop wire payload @12 / record 16, overlay wire
// payload @16 / record 304 (sizeof(GuiOverlayRequest) == 288),
// CgsGui::GuiEventNetworkSuspension == 16.
//
// MEASURED CORRECTION TO THE WAVE-J SPEC (§3, the event-wire catalog) -- FOR THE CONDUCTOR.
// The spec's wire table lists the id-253 {1,253,12} record under "SelectGame '2' / NoGames*
// '2' arms". THE ASSEMBLY SAYS ONLY SelectGame POSTS IT: its cancel arm reaches loc_82497758
// (`li r5,0x28` / `li r6,0x10` / AddEvent) before ShowParamSelection, whereas
// HandleControllerInputNoGames (0x82497A1C..0x82497A20) and HandleControllerInputNoGamesInGame
// (0x82497C08..0x82497C0C) branch STRAIGHT to ShowParamSelection, with no AddEvent anywhere in
// either function. The spec's own §5.11/§5.12 per-function triage agrees; §3's table row is
// the one that is wrong, and only SelectGame below posts the id-253 record.
//
// MEASURED CORRECTION TO THE WAVE-J SPEC (§7, the link-gap list) -- FOR THE CONDUCTOR.
// §7 lists `GuiOverlayRequest::Construct @0x823B1CC8` as a declared-but-body-less link gap.
// That is STALE: the body IS committed, at
// b5-decomp/src/GameSource/Gui/BrnGuiEventTypeDefs.cpp:232. SelectGame calls it.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineCustomMatch.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface / GuiEventNetworkSuspension
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache

namespace BrnGui
{
    namespace
    {
        const s32 KI_CHANNEL_GUI_OUT = 40;   // X360 `li r5, 0x28`

        const s32 KI_ACTION_GUI_UP     = 0x29;   // 41 GUI_UP
        const s32 KI_ACTION_GUI_DOWN   = 0x2A;   // 42 GUI_DOWN
        const s32 KI_ACTION_GUI_SELECT = 0x31;   // 49 GUI_SELECT
        const s32 KI_ACTION_GUI_CANCEL = 0x32;   // 50 GUI_CANCEL

        // The message-button row this page treats as the affirmative answer. The X360 tests
        // the highlighted index against 0, and the sibling in-game handler's assert text
        // names the row E_OK_BUTTON_OK. FLAG: the enum itself is not in the recovered DWARF
        // slice, so the value is carried as a named constant rather than an invented enum.
        const s32 KI_MESSAGE_BUTTON_AFFIRMATIVE = 0;

        // The state in-queue hands handlers the HEADER-STRIPPED payload; this body reads
        // only the second word (`lwz r11, 4(r25)` at 0x82497918).
        struct ControllerButtonPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miButtonId;   // +0x04 (the input action id)
        };
    }

    // ================================================================================
    //  HandleControllerInputNoGames  @ 0x82497880
    //
    //  The "no games found -- search again?" page: a two-button (YES/NO) message screen.
    //  YES takes the flow to the game-options screen; anything else leaves the screen the
    //  same way Cancel does.
    // ================================================================================
    void OnlineCustomMatch::HandleControllerInputNoGames(const CgsModule::Event* lpEvent)
    {
        // Non-fatal (BeginAssert / streamed message / FireAssert / EndAssert, no early-out).
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event sent to OnlineCustomMatch::HandleControllerInputNoGames");   // cpp:738

        const ControllerButtonPayload* lpInput =
            reinterpret_cast<const ControllerButtonPayload*>(lpEvent);

        switch (lpInput->miButtonId)
        {
        case KI_ACTION_GUI_UP:
            mMessageButtons.HighlightPrevious();   // virtual slot +0x38
            break;

        case KI_ACTION_GUI_DOWN:
            mMessageButtons.HighlightNext();       // virtual slot +0x34
            break;

        // ---- accept the highlighted answer ----------------------------------------------
        // MEASURED: the non-affirmative answer branches INTO the cancel arm PAST its
        // mbOnlineMatchUnranked gate -- 0x82497970 `bne loc_8249798C`, and loc_8249798C is
        // the cancel arm's `lwz mpGuiCache` / `lbz mbOnlineStartPending` pair, one
        // instruction AFTER the `lbz r10, 0x4B52` test at 0x82497A10. So "NO" leaves the
        // screen unconditionally, without consulting the unranked flag. The X360 shares the
        // tail by branching into it; it is written out in both arms here (gotos are not
        // preserved).
        case KI_ACTION_GUI_SELECT:
            if (mMessageButtons.miHighlightedIndex == KI_MESSAGE_BUTTON_AFFIRMATIVE)
            {
                SendStateEvent("TO_GAME_OPT");
            }
            else if (mpGuiCache->IsOnlineStartPending())
            {
                CgsGui::GuiEventNetworkSuspension lNetworkSuspension(false);
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lNetworkSuspension),
                    KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(lNetworkSuspension)));         // X360 record size 16

                mpGuiCache->SetOnlineStartPending(false);
                SendStateEvent("GO_BACK_EASY");
            }
            else
            {
                SendStateEvent("GO_BACK");
            }
            break;

        // ---- back out ------------------------------------------------------------------
        case KI_ACTION_GUI_CANCEL:
            CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");                     // cpp:772

            if (mpGuiCache->mbOnlineMatchUnranked)
            {
                if (mpGuiCache->IsOnlineStartPending())
                {
                    CgsGui::GuiEventNetworkSuspension lNetworkSuspension(false);
                    mpStateInterface->GetOutputEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lNetworkSuspension),
                        KI_CHANNEL_GUI_OUT,
                        static_cast<s32>(sizeof(lNetworkSuspension)));     // X360 record size 16

                    mpGuiCache->SetOnlineStartPending(false);
                    SendStateEvent("GO_BACK_EASY");
                }
                else
                {
                    SendStateEvent("GO_BACK");
                }
            }
            else
            {
                // Unlike the SelectGame cancel arm, this one publishes NOTHING before
                // returning to the parameter page -- see the spec correction in the banner.
                ShowParamSelection();
            }
            break;

        default:
            break;
        }
    }
}

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineCustomMatch.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface / GuiEventNetworkSuspension
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache

namespace BrnGui
{
    namespace
    {


        // The message-button row the in-game "no games found" page expects to be highlighted
        // (its message menu carries a single OK button). FLAG: the E_OK_BUTTON_OK enum the
        // assert text names is not in the recovered DWARF slice, so the value is carried as a
        // named constant rather than an invented enum.

        // The state in-queue hands handlers the HEADER-STRIPPED payload; this body reads
        // only the second word (`lwz r11, 4(r25)` at 0x82497B00).
    }

    // ================================================================================
    //  HandleControllerInputNoGamesInGame  @ 0x82497A68
    //
    //  The in-game variant of the "no games found" page: a single OK button, so Select has
    //  no affirmative branch at all -- it asserts that OK is what is highlighted and then
    //  leaves the screen exactly the way Cancel does.
    // ================================================================================
    void OnlineCustomMatch::HandleControllerInputNoGamesInGame(const CgsModule::Event* lpEvent)
    {
        // Non-fatal (BeginAssert / streamed message / FireAssert / EndAssert, no early-out).
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event sent to OnlineCustomMatch::HandleControllerInputNoGamesInGame");   // cpp:830

        const ControllerButtonPayload* lpInput =
            reinterpret_cast<const ControllerButtonPayload*>(lpEvent);

        switch (lpInput->miButtonId)
        {
        case KI_ACTION_GUI_UP:
            mMessageButtons.HighlightPrevious();   // virtual slot +0x38
            break;

        case KI_ACTION_GUI_DOWN:
            mMessageButtons.HighlightNext();       // virtual slot +0x34
            break;

        // ---- accept ---------------------------------------------------------------------
        // The assert is non-fatal and there is no branch around the tail: whatever the
        // highlighted index is, the handler falls into the leave path (0x82497B78 follows
        // the assert block unconditionally, and 0x82497B7C is the same
        // `lbz mbOnlineStartPending` the cancel arm branches back to).
        case KI_ACTION_GUI_SELECT:
            // Assert text kept VERBATIM from the X360 image. GetHighlightedIndex() and
            // E_OK_BUTTON_OK are named by the original source only -- neither exists in the
            // committed SelectableGroup, so the check reads the member the console reads
            // (`lbz r11, 0x31A5(r28)` == mMessageButtons.miHighlightedIndex, tested != 0).
            CGS_ASSERT(mMessageButtons.miHighlightedIndex == KI_MESSAGE_BUTTON_AFFIRMATIVE,
                       "mMessageButtons.GetHighlightedIndex() == E_OK_BUTTON_OK");   // cpp:836

            if (mpGuiCache->IsOnlineStartPending())
            {
                CgsGui::GuiEventNetworkSuspension lNetworkSuspension(false);
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lNetworkSuspension),
                    KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(lNetworkSuspension)));         // X360 record size 16

                mpGuiCache->SetOnlineStartPending(false);
                SendStateEvent("GO_BACK_EASY");
            }
            else
            {
                SendStateEvent("GO_BACK");
            }
            break;

        // ---- back out ------------------------------------------------------------------
        case KI_ACTION_GUI_CANCEL:
            CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");                     // cpp:858

            if (mpGuiCache->mbOnlineMatchUnranked)
            {
                if (mpGuiCache->IsOnlineStartPending())
                {
                    CgsGui::GuiEventNetworkSuspension lNetworkSuspension(false);
                    mpStateInterface->GetOutputEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lNetworkSuspension),
                        KI_CHANNEL_GUI_OUT,
                        static_cast<s32>(sizeof(lNetworkSuspension)));     // X360 record size 16

                    mpGuiCache->SetOnlineStartPending(false);
                    SendStateEvent("GO_BACK_EASY");
                }
                else
                {
                    SendStateEvent("GO_BACK");
                }
            }
            else
            {
                // No AddEvent on this path (the X360 branches straight to ShowParamSelection
                // at 0x82497C08..0x82497C0C).
                ShowParamSelection();
            }
            break;

        default:
            break;
        }
    }
}

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineCustomMatch.h"

#include <cstddef>                                                        // offsetof (wire header words)
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface / GuiEventNetworkSuspension
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiOverlayRequest

namespace BrnGui
{
    namespace
    {

        // ---- controller action ids (the in-queue payload's second word) ----------------
        // BrnGui's EGameInputActions values -- the enum is recovered
        // (references/DecFIGS/dwarfdump/GameSource/Input/GameInputActions.h:24) but has no
        // committed home under b5-decomp/src yet, so these stay s32. Same names/values the
        // wave-I OnlineGameOptions partfiles use. The X360 subtracts 0x29 and jump-tables
        // ten slots, so 0x2B..0x30 exist as cases but do nothing here.

        // The bottom slot of the found-games scroll window. The X360 bakes `cmplwi r11, 4`
        // (0x82497808) -- one less than the five-row table the screen constructs (OnEnter
        // passes `li r9, 5` as Table::Construct's row count).
        const s32 KI_LAST_VISIBLE_TABLE_ROW = 4;

        // ---- in-queue payload view -----------------------------------------------------
        // The state in-queue hands handlers the HEADER-STRIPPED payload; this body reads
        // only the second word (`lwz r11, 4(r25)` at 0x82497608). Same view the sibling
        // screens carry for CgsGui::GuiEventControllerInput*.
        // ---- out-queue wire records ----------------------------------------------------

        // OutputGuiEvent<BrnGui::GuiEventNetworkCustomMatchJoin> (instantiation @0x82493C48):
        // the record is { 4, 255, 12 } + the game id, channel 40, 16 bytes. The homed payload
        // type (BrnGuiDemangledEventTypes.h:347) carries the four bytes as an opaque
        // `maData[4]` with no field names, so the wire names the word it actually publishes.
        struct GuiEventNetworkCustomMatchJoinWire : public CgsGui::GuiEvent<255>
        {
            s32 miGameId;   // payload +0x00 == mSearchResults.maiFoundGameIndex[<row>]

            explicit GuiEventNetworkCustomMatchJoinWire(s32 liGameId)
                : CgsGui::GuiEvent<255>(
                      static_cast<u32>(sizeof(s32)),                                            // X360 4
                      static_cast<u32>(offsetof(GuiEventNetworkCustomMatchJoinWire, miGameId))) // X360 12
                , miGameId(liGameId)
            {
            }
        };

        // The id-253 record the cancel arm publishes before dropping back to the parameter
        // page: { 1, 253, 12 } + one payload byte, channel 40, 16 bytes (0x82497758..
        // 0x82497784 store only the three header words, then `li r6, 0x10`).
        //
        // FLAG -- consumer-named. Id 253 has no entry in BrnGuiDemangledEventTypes.h and no
        // producer-side name anywhere in the recovered DWARF; the role ("stop the custom-match
        // search that is in flight") comes from the call site alone, so this record is named
        // for what it does and is deliberately NOT homed as a shared type.
        //
        // PAYLOAD BYTE: the X360 NEVER writes it -- the console record's fourth word keeps
        // whatever the caller's stack frame held (the AddEvent size is 16, so the byte IS
        // published). It is zero-initialised here because the host cannot reproduce an
        // uninitialised console stack slot and must not pretend to.
        struct CustomMatchSearchStopWire : public CgsGui::GuiEvent<253>
        {
            u8 muUnwrittenPayload;   // payload +0x00 (never stored by the X360 -- see above)

            CustomMatchSearchStopWire()
                : CgsGui::GuiEvent<253>(
                      static_cast<u32>(sizeof(u8)),                                              // X360 1
                      static_cast<u32>(offsetof(CustomMatchSearchStopWire, muUnwrittenPayload))) // X360 12
                , muUnwrittenPayload(0)
            {
            }
        };

        // OutputGuiEvent<BrnGui::GuiOverlayRequest> (instantiation @0x82436BE0): the record is
        // { 288, 184, 16, <pad>, the 288-byte request }, channel 40, 304 bytes -- the same
        // wire BrnCarSelectMain_wG_02.cpp builds for its overlay posts.
        struct GuiOverlayRequestWire : public CgsGui::GuiEvent<184>
        {
            u32               muPad0C;    // +0x0C (payload is 16-aligned past the header)
            GuiOverlayRequest mRequest;   // +0x10

            GuiOverlayRequestWire()
                : CgsGui::GuiEvent<184>(
                      static_cast<u32>(sizeof(GuiOverlayRequest)),                          // X360 288
                      static_cast<u32>(offsetof(GuiOverlayRequestWire, mRequest)))          // X360 16
                , muPad0C(0)
            {
            }
        };

        // The overlay the join path raises while the network match is being entered.
        const char KAC_ENTER_GAME_OVERLAY[] = "CNOnlEntGame";
    }

    // ================================================================================
    //  HandleControllerInputSelectGame  @ 0x82497570
    //
    //  The found-games list is live: Up/Down walk the highlight and scroll the five-row
    //  window over the (up to ten) search results, Select joins the highlighted game,
    //  Cancel either cancels the search and returns to the parameter page or -- in the
    //  unranked/free-burn flow -- backs out of the screen entirely.
    //
    //  The Hex-Rays rendering of the two scroll arms is inverted-flattened (it tests
    //  `highlight != 0 || first <= 0` and puts the highlight move first); the asm order is
    //  reproduced below as the positive scroll condition.
    // ================================================================================
    void OnlineCustomMatch::HandleControllerInputSelectGame(const CgsModule::Event* lpEvent)
    {
        // Non-fatal (BeginAssert / streamed message / FireAssert / EndAssert, no early-out).
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event sent to OnlineCustomMatch::HandleControllerInputSelectGame");   // cpp:634

        const ControllerButtonPayload* lpInput =
            reinterpret_cast<const ControllerButtonPayload*>(lpEvent);

        switch (lpInput->miButtonId)
        {
        // ---- move up / scroll the window up --------------------------------------------
        // The highlight only moves when it is NOT already parked on the window's top row;
        // once it is, an earlier result (if there is one) is scrolled into view instead and
        // BOTH window edges step back together.
        //   0x82497798  lbz r11, 0x44BD(r31)   ; mTable.miHighlightedIndex, tested == 0
        //   0x824977AC  lbz + extsb miFirstRow ; then `cmpwi 0` + `ble` -> SIGNED > 0 test
        case KI_ACTION_GUI_UP:
            if (mTable.miHighlightedIndex == 0 && miFirstRow > 0)
            {
                --miFirstRow;
                --miLastRow;
                FillInTable();
            }
            else
            {
                mTable.HighlightPrevious();   // virtual slot +0x38, no arguments
            }
            break;

        // ---- move down / scroll the window down ----------------------------------------
        //   0x82497804  lbz r11, 0x44BD(r31)   ; tested == 4 (the bottom window slot)
        //   0x82497824  lbz + extsb miLastRow  ; `cmpw` + `bge` vs the s32 result count
        case KI_ACTION_GUI_DOWN:
            if (mTable.miHighlightedIndex == KI_LAST_VISIBLE_TABLE_ROW &&
                miLastRow < mSearchResults.miNumGames)
            {
                ++miLastRow;
                ++miFirstRow;
                FillInTable();
            }
            else
            {
                mTable.HighlightNext();   // virtual slot +0x34, no arguments
            }
            break;

        // ---- join the highlighted game --------------------------------------------------
        // The window's first row plus the in-window highlight index give the result slot;
        // both are s8 and the X360 sign-extends both (`extsb`) before adding.
        case KI_ACTION_GUI_SELECT:
            {
                const s32 liSelectedGame = miFirstRow + mTable.miHighlightedIndex;

                GuiEventNetworkCustomMatchJoinWire lJoin(mSearchResults.maiFoundGameIndex[liSelectedGame]);
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lJoin), KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(lJoin)));                      // X360 record size 16

                // Raise the "entering game" overlay over the top of the list.
                GuiOverlayRequestWire lRequest;
                lRequest.mRequest.Construct(KAC_ENTER_GAME_OVERLAY);
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lRequest), KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(lRequest)));                   // X360 record size 304

                // The X360 stores the sub-state BEFORE the ShowMessage call
                // (`stw r10, 0x38(r31)` at 0x824976BC, `bl` at 0x824976C0) -- keep the order.
                meSubState = E_SUBSTATE_JOINING;
                ShowMessage("$ONLINE_GAME_SEARCH_JOINING");
            }
            break;

        // ---- back out ------------------------------------------------------------------
        case KI_ACTION_GUI_CANCEL:
            CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");                     // cpp:659

            if (mpGuiCache->mbOnlineMatchUnranked)
            {
                // The free-burn/unranked entry path owns the whole screen: leave it rather
                // than falling back to the parameter page.
                if (mpGuiCache->IsOnlineStartPending())
                {
                    // This flow armed the online start: lift the network suspension it put
                    // in place, then back out on the quiet path.
                    CgsGui::GuiEventNetworkSuspension lNetworkSuspension(false);
                    mpStateInterface->GetOutputEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lNetworkSuspension),
                        KI_CHANNEL_GUI_OUT,
                        static_cast<s32>(sizeof(lNetworkSuspension)));     // X360 record size 16

                    mpGuiCache->SetOnlineStartPending(false);
                    SendStateEvent("GO_BACK_EASY");
                }
                else
                {
                    SendStateEvent("GO_BACK");
                }
            }
            else
            {
                // Ranked/normal flow: stop the search that produced this list and go back
                // to the parameter page.
                CustomMatchSearchStopWire lStopSearch;
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lStopSearch), KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(lStopSearch)));                // X360 record size 16

                ShowParamSelection();
            }
            break;

        default:
            break;
        }
    }
}
