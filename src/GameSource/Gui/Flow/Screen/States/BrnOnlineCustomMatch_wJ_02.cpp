// ===================================================================================
// wave-J partfile -- BrnGui::OnlineCustomMatch controller-input group (group 2 of the wave).
//
//   HandleControllerInputSelectParams @0x824972D0  (assert cpp:551)
//   HighlightLastSearchParams         @0x8248E920
//   HandleControllerInput             @0x824A35B8  (assert cpp:503)
//
// All three bodies compile against the grown headers. The two that read/write the FIELDS of
// BrnGui::GuiEventNetworkCustomMatchSearch -- the type of the class member mLastSearchParams
// (BrnOnlineCustomMatch.h:184) -- now find them: BrnGuiDemangledEventTypes.h:417 carries the
// DWARF-attested meGameMode / meSearchOpponentTypes / mbRanked / mbFreeburn payload. The
// SelectParams body's reads of GuiCache::mbOnlineMatchRanked / mbOnlineMatchUnranked
// (BrnGuiCache.h:769/770, private, setter-only) are legal because BrnGuiCache.h:803 now
// carries `friend struct OnlineCustomMatch;`, the same grant the wave-C/H/I screens hold.
//
// CONDUCTOR NOTE (merge): like the group-01 partfile, this file DEFINES NO OnlineCustomMatch
// class statics -- the single definition of KA_GAME_MODE_SEARCH_OPTION_STRING_IDS /
// KAPC_ANIMATION_STATES / KAC_SEARCHING_STRING_ID belongs in the consolidated
// BrnOnlineCustomMatch.cpp (wave-I placed OnlineGameOptions' statics the same way). The
// anonymous-namespace constants below are the same names/values group 01 and the wave-I
// partfiles use -- keep ONE copy on merge.
//
// NO CONSOLE LAYOUT LITERALS in the body below: the only X360 displacement it touches
// (`lwz r11, 0x38(r28)` == meSubState) is reached by member name, and the number survives
// only in a comment. No float compares anywhere, so there is no PPC NaN-polarity decision.
//
// LINK NOTE (`cl /c` cannot see these): the four sibling handlers HandleControllerInput
// dispatches to all have bodies in this wave now -- SelectParams here, SelectGame /
// NoGames / NoGamesInGame in BrnOnlineCustomMatch_wJ_04.cpp. What is still bodyless is
// BrnGui::OnlineCustomMatch::ShowMessage @0x82484A90 (declared BrnOnlineCustomMatch.h:100,
// owned by the FOREIGN ledger TU BrnAnimationComponent.h) and the class statics this file
// reads, whose single definition belongs in the consolidated .cpp (see the merge note).
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineCustomMatch.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface / GuiEventNetworkSuspension
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache (we are a friend)

namespace BrnGui
{
    namespace
    {
        // ---- AddEvent channel (the out-queue selector word) ---------------------------
        const s32 KI_CHANNEL_GUI_OUT = 40;   // X360 `li r5, 0x28`

        // ---- controller action ids (the in-queue payload's second word) ----------------
        // BrnGui's EGameInputActions values; the enum is recovered in the DecFIGS DWARF
        // (GameSource/Input/GameInputActions.h:24) but has no committed home under
        // b5-decomp/src yet, which is why these stay s32. Same names/values the wave-I
        // partfiles use.
        const s32 KI_ACTION_GUI_UP     = 0x29;   // 41 GUI_UP
        const s32 KI_ACTION_GUI_DOWN   = 0x2A;   // 42 GUI_DOWN
        const s32 KI_ACTION_GUI_LEFT   = 0x2B;   // 43 GUI_LEFT
        const s32 KI_ACTION_GUI_RIGHT  = 0x2C;   // 44 GUI_RIGHT
        const s32 KI_ACTION_GUI_SELECT = 0x31;   // 49 GUI_SELECT
        const s32 KI_ACTION_GUI_CANCEL = 0x32;   // 50 GUI_CANCEL

        // The apt view whose state the button-prompt animator is driven through, and the
        // KAPC_ANIMATION_STATES slot it is driven to (@0x82F266B0 == { "Visible",
        // "Invisible", "Refresh" }; the asm reaches slot 1 as off_82F266B4).
        const char KAC_APT_TRANSITION_NAME[]   = "apt_Transition";
        const s32  KI_ANIMATION_STATE_INVISIBLE = 1;

        // The two search-parameter rows of mSearchParms, in the order ShowParamSelection
        // sets the group up (row 0 = game mode, row 1 = opponents).
        const s32 KI_SEARCH_ROW_GAME_MODE = 0;
        const s32 KI_SEARCH_ROW_OPPONENTS = 1;

        // ---- in-queue payload view -----------------------------------------------------
        // The state in-queue hands handlers the HEADER-STRIPPED payload; this handler reads
        // only the second word (`lwz r11, 4(r25)` at 0x82497368). Same view the wave-I
        // siblings carry for CgsGui::GuiEventControllerInput*.
        struct ControllerButtonPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miButtonId;   // +0x04 (the input action id)
        };
    }

    // ================================================================================
    //  HandleControllerInputSelectParams  @ 0x824972D0
    //
    //  The search-parameter form's controller handler: move between the two toggle rows,
    //  change the highlighted row's value, fire the search, or back out of the screen.
    // ================================================================================
    void OnlineCustomMatch::HandleControllerInputSelectParams(const CgsModule::Event* lpEvent)
    {
        // Non-fatal (BeginAssert / FireAssert / EndAssert, no early-out).
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event sent to OnlineCustomMatch::HandleControllerInputSelectParams");   // cpp:551

        const ControllerButtonPayload* lpInput =
            reinterpret_cast<const ControllerButtonPayload*>(lpEvent);

        switch (lpInput->miButtonId)
        {
        // ---- move between the two parameter rows --------------------------------------
        case KI_ACTION_GUI_UP:
            mSearchParms.HighlightPrevious(false);
            break;

        case KI_ACTION_GUI_DOWN:
            mSearchParms.HighlightNext(false);
            break;

        // ---- change the highlighted row's value ---------------------------------------
        case KI_ACTION_GUI_LEFT:
            mSearchParms.HighlightPreviousItem();
            break;

        case KI_ACTION_GUI_RIGHT:
            mSearchParms.HighlightNextItem();
            break;

        // ---- fire the search ----------------------------------------------------------
        case KI_ACTION_GUI_SELECT:
            {
                // Non-fatal (no early-out): the cache is dereferenced either way.
                CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:557

                // The match KIND is not a form field -- it comes from how the player
                // entered online play, so it is copied straight off the cache.
                mLastSearchParams.mbRanked   = mpGuiCache->mbOnlineMatchRanked;     // cache +0x4B51
                mLastSearchParams.mbFreeburn = mpGuiCache->mbOnlineMatchUnranked;   // cache +0x4B52

                // Row 0's highlighted option indexes the mode table; the console calls the
                // BASE GetSelectable and reads the row's inner TextSelection highlight as a
                // SIGNED byte, with NO range check on the table index. Both kept.
                MenuToggle* lpGameModeRow = static_cast<MenuToggle*>(
                    mSearchParms.SelectableGroup::GetSelectable(KI_SEARCH_ROW_GAME_MODE));
                mLastSearchParams.meGameMode =
                    KA_GAME_MODE_SEARCH_OPTION_STRING_IDS[lpGameModeRow->mItemText.miHighlightedIndex]
                        .meGameMode;

                // Row 1's highlighted option IS the opponents filter value (widened from the
                // s8 highlight to the payload's word).
                MenuToggle* lpOpponentRow = static_cast<MenuToggle*>(
                    mSearchParms.SelectableGroup::GetSelectable(KI_SEARCH_ROW_OPPONENTS));
                mLastSearchParams.meSearchOpponentTypes = lpOpponentRow->mItemText.miHighlightedIndex;

                // Publish the request. See the WIRE NOTE in the banner for why this goes
                // onto channel 40 directly.
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&mLastSearchParams),
                    KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(mLastSearchParams)));   // X360 record size 24

                // Swap the page over to the "searching..." message and drop the prompts.
                meSubState = E_SUBSTATE_SEARCHING;
                ShowMessage(KAC_SEARCHING_STRING_ID);
                mButtonPromptAnimation.AddOutputAptViewState(
                    KAC_APT_TRANSITION_NAME,
                    KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_INVISIBLE],
                    false);
            }
            break;

        // ---- back out of the screen ---------------------------------------------------
        // NOTE: unlike the three sibling handlers in this TU, this arm does NOT gate on
        // mbOnlineMatchUnranked -- it goes straight to the pending-start test.
        case KI_ACTION_GUI_CANCEL:
            if (mpGuiCache->IsOnlineStartPending())   // cache +0x4B53
            {
                // This flow armed the online start: lift the network suspension it put in
                // place, then back out on the quiet path.
                CgsGui::GuiEventNetworkSuspension lNetworkSuspension(false);
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lNetworkSuspension),
                    KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(lNetworkSuspension)));   // X360 record size 16

                mpGuiCache->SetOnlineStartPending(false);
                SendStateEvent("GO_BACK_EASY");
            }
            else
            {
                SendStateEvent("GO_BACK");
            }
            break;

        default:
            // 0x2D..0x30 land here through the jump table; every other id misses it.
            break;
        }
    }
}

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineCustomMatch.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnGui
{
    namespace
    {
        // The extent of OnlineCustomMatch::KA_GAME_MODE_SEARCH_OPTION_STRING_IDS, declared
        // [3] in the class header. The console spells the same bound as the scan's end
        // pointer (0x8205E97C == the table base 0x8205E964 plus three 8-byte entries) and
        // re-checks it as `cmpwi r21, 3` before the assert. Shared with the parked
        // SelectParams body -- keep one copy on merge.
        const s32 KI_NUM_GAME_MODE_SEARCH_OPTIONS = 3;

        // The two search-parameter rows of mSearchParms, in the order the group was set up
        // by ShowParamSelection (row 0 = game mode, row 1 = opponents).
    }

    // ================================================================================
    //  HighlightLastSearchParams  @ 0x8248E920
    //
    //  Re-arm the two search-parameter toggles from the search the player last ran, so
    //  re-entering the form shows the same mode/opponents choice it was left on.
    // ================================================================================
    void OnlineCustomMatch::HighlightLastSearchParams()
    {
        // ---- resolve the stored mode value back to its row in the option table --------
        // Linear scan; the console walks a pointer over the table and counts, so a miss
        // leaves the index sitting one past the end.
        s32 liGameModeIndex = 0;
        while (liGameModeIndex < KI_NUM_GAME_MODE_SEARCH_OPTIONS &&
               KA_GAME_MODE_SEARCH_OPTION_STRING_IDS[liGameModeIndex].meGameMode !=
                   mLastSearchParams.meGameMode)
        {
            ++liGameModeIndex;
        }

        // Non-fatal (BeginAssert / FireAssert / EndAssert, no early-out): the console
        // highlights with the out-of-range index anyway when the mode is not in the table.
        CGS_ASSERT(liGameModeIndex < KI_NUM_GAME_MODE_SEARCH_OPTIONS,
                   "Invalid game mode");   // cpp:1377

        // ---- row 0: the game-mode toggle ---------------------------------------------
        // The console inlines SelectableGroup::GetSelectable (its bounds assert and its
        // queried-flag store are the inlined body) and calls the BASE, not the group's own
        // covariant override -- so the base is named explicitly here and the row type is
        // recovered by the downcast the console's use of the pointer implies.
        MenuToggle* lpGameModeRow = static_cast<MenuToggle*>(
            mSearchParms.SelectableGroup::GetSelectable(KI_SEARCH_ROW_GAME_MODE));
        if (lpGameModeRow->mItemText.HighlightIndex(liGameModeIndex))
        {
            // The highlight actually moved: the row has to repaint.
            lpGameModeRow->SetDirty();
        }

        // ---- row 1: the opponents toggle ---------------------------------------------
        // Read before the second GetSelectable, exactly as the console orders it.
        const s32 liOpponentIndex = mLastSearchParams.meSearchOpponentTypes;

        MenuToggle* lpOpponentRow = static_cast<MenuToggle*>(
            mSearchParms.SelectableGroup::GetSelectable(KI_SEARCH_ROW_OPPONENTS));
        if (lpOpponentRow->mItemText.HighlightIndex(liOpponentIndex))
        {
            lpOpponentRow->SetDirty();
        }
    }
}

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineCustomMatch.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnGui
{

//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x824A35B8.json, asm arbitrated over Hex-Rays).
//
// WHAT THE FUNCTION DOES
// ----------------------
// The screen's single controller-input sink: Update routes every input-pressed event here
// and this body forwards it, unread, to whichever per-sub-state handler owns the page right
// now. It looks at nothing but meSubState -- the payload is passed straight through.
//
// NOTES TAKEN FROM THE ASM RATHER THAN HEX-RAYS
// ---------------------------------------------
//  * The null-event check is the streamed non-fatal assert idiom (BeginAssert / the
//    CgsDev message-stream operator through `off_82000D08` / FireAssert / EndAssert) with
//    NO early-out: 0x824A35D0 `bne cr6, loc_824A364C` skips the assert when the pointer is
//    good and the assert block falls straight through into the switch when it is not. Same
//    shape the wave-I siblings reproduce with CGS_ASSERT.
//  * The dispatch is a jump table, not a chain: `lwz r11, 0x38(r28)` (meSubState) then
//    `addi r11, r11, -2 / cmplwi cr6, r11, 5 / bgt def_824A3670`, so the live range is the
//    six sub-states 2..7 and the table sends
//        slot 0 -> substate 2 (SELECTING_PARAMS)       HandleControllerInputSelectParams
//        slot 2 -> substate 4 (SELECTING_GAME)         HandleControllerInputSelectGame
//        slot 4 -> substate 6 (NO_GAMES_FOUND)         HandleControllerInputNoGames
//        slot 5 -> substate 7 (NO_GAMES_FOUND_IN_GAME) HandleControllerInputNoGamesInGame
//    with slots 1 and 3 (substates 3 SEARCHING and 5 JOINING) folded into the default. The
//    compare is UNSIGNED, so substates 0/1 (the two loading states) wrap past the bound and
//    also land in the default -- a plain switch reproduces that exactly.
//  * Each arm is a tail call (`bl <handler>` then `b __restgprlr_27`) passing r3 = this and
//    r4 = the event pointer verbatim; nothing is read out of the event here.
//
    // ================================================================================
    //  HandleControllerInput  @ 0x824A35B8
    //
    //  Hand the input event to the handler that owns the current page.
    // ================================================================================
    void OnlineCustomMatch::HandleControllerInput(const CgsModule::Event* lpEvent)
    {
        // Non-fatal (BeginAssert / FireAssert / EndAssert, no early-out) -- the X360 falls
        // straight through into the dispatch even with a null event.
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event sent to OnlineCustomMatch::HandleControllerInput");   // cpp:503

        switch (meSubState)   // X360 this+0x38
        {
        case E_SUBSTATE_SELECTING_PARAMS:
            HandleControllerInputSelectParams(lpEvent);
            break;

        case E_SUBSTATE_SELECTING_GAME:
            HandleControllerInputSelectGame(lpEvent);
            break;

        case E_SUBSTATE_NO_GAMES_FOUND:
            HandleControllerInputNoGames(lpEvent);
            break;

        case E_SUBSTATE_NO_GAMES_FOUND_IN_GAME:
            HandleControllerInputNoGamesInGame(lpEvent);
            break;

        default:
            // The two loading sub-states plus SEARCHING and JOINING: the page is not
            // interactive, so the input is dropped.
            break;
        }
    }
}
