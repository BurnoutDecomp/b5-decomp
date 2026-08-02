// ===================================================================================
// BrnGui::OnlineGameOptions -- wave-I partfile 09: the two big handlers.
//   HandleGuiCacheEvent             @0x824A85E8  (assert cpp:1048)
//   HandleControllerInputCreateGame @0x824A7878  (assert cpp:494)
//
//
// The committed leaf header BrnOnlineGameOptions.h is still the MINIMAL pre-wave version
// (the GetResourcesToLoad inline plus the two resource statics): the wave-I spec's §H1 class
// extension, §H2 CreateMatchOption fix, §H3 HelpBar declarations and §H4 GuiCache carve had
// not been applied when this partfile was written, and headers are frozen for implementers.
// Neither function can name itself as a member of BrnGui::OnlineGameOptions, nor reach
// mpGuiCache / mGameOptions / miStartItem / the component members / the class statics / the
// sibling methods it calls. Both additionally need the GuiCache friendship grant the wave-C
// HudMessageAnalyzer and wave-H OnlineGameRoomPlayerInfo keystones already hold
// (BrnGuiCache.h:44 / :661) -- mbOnlineMatchRanked, mbOnlineMatchUnranked,
// mbOnlineStartPending and maOnlineGameModeOptionsStorage are private with no read accessors.
//
// The two complete bodies, each with a banner naming the EXACT declaration lines that
// unblock it and the measured evidence behind that list, live at:
// They concatenate into this file (a single `namespace BrnGui { ... }`) once §H1..§H4 and
// the friendship land. Both were re-compiled VERBATIM against a stand-in class shaped like
// §H1 (scratchpad/waveI/probe_g09.cpp) => STATUS=pass, so only the header gaps are missing.
//
// MEASURED CORRECTION TO THE WAVE-I SPEC (spec §5 TRAP 2) -- THE CONDUCTOR SHOULD READ THIS.
// The spec says HandleControllerInputCreateGame's '+' / ',' arms compare a ZERO-extended
// miHighlightedIndex against GetIndexFromId(0), so a -1 (no row highlighted) reads back as
// 255 and can never match. THE ASSEMBLY SAYS OTHERWISE -- both arms do
//     lbz   r11, 0x11A5(r31)     ; the s8 SelectableGroup::miHighlightedIndex
//     extsb r30, r11             ; SIGN-extend
//     cmpw  cr6, r30, r3         ; signed word compare against GetIndexFromId's s32
// (0x824A7A34/0x824A7A40/0x824A7A48 and 0x824A7AA8/0x824A7AB4/0x824A7ABC). `extsb` is a
// console by reading the s8 member straight into an s32; the `static_cast<u8>` the spec
// prescribes would be a live behaviour change. (0x11A5 == 4517 == mCreateGameToggles(4352)
// + SelectableGroup::miHighlightedIndex(+0xA5), which corroborates the member exactly.)
//
// VTABLE SLOTS MEASURED, NOT ASSUMED (headless IDA -> scratchpad/waveI/g09_vt.txt). The
// MenuToggleGroupVarSize<5> constructor @0x824F9AA0 stores off_820730AC at +0x00 (and
// off_820730A8, its GuiComponent sub-object table, at +0x18), so the dispatch displacements
// these bodies show resolve against 0x820730AC as:
//     +0x18 -> MenuToggleGroupVarSize<5>::Clear
//     +0x28 -> SelectableGroup::HighlightNext        (arg 0 == lbQuiet false)
//     +0x2C -> SelectableGroup::HighlightPrevious    (arg 0 == lbQuiet false)
//     +0x30 -> SelectableGroup::HighlightIndex
//     +0x34 -> MenuToggleGroupVarSize<5>::HighlightNextItem
//     +0x38 -> MenuToggleGroupVarSize<5>::HighlightPreviousItem
// KPC_ARROW_ANIMATION_STATES @0x82F2683C = { "invisible", "visible", "animate" }.
//
// NO CONSOLE LAYOUT LITERALS. Every X360 displacement in these two bodies (this+0xA500 ==
// mpGuiCache, this+0xA0C0 == mGameOptions, this+0xA50C == miStartItem, this+0x7A90/0x7B1C ==
// the two arrow animators, this+0x7A94.. == the six component names, cache+0xA800 == the
// params mirror, cache+0xA9E0 == the changed flag, cache+0x12B80/0x12B84 == the two profile
// counters) is reached BY NAME; the numbers survive only in comments. The two out-queue
// record sizes (0x824 and 0x10) and both 0x1E0 params copies are host `sizeof` expressions,
//
// NO FLOAT COMPARES in either function, so there is no PPC NaN-polarity decision to make.
//
// MERGE NOTES for the conductor:
//    exactly one on merge.
//  * the controller-action id constants (KI_ACTION_GUI_UP .. KI_ACTION_GUI_CANCEL) are also
//    defined by the group-02 partfile with the same names and values; if the partfiles are
//    consolidated into one .cpp keep a single copy, and add KI_ACTION_GUI_OPTION0 (0x33), which
//    only this group uses.
//  * LINK NOTE -- callees these bodies use that are DECLARED but defined NOWHERE in the tree
//    today (`cl /c` cannot see any of them). Re-checked against the committed tree:
//      OnlineGameOptions::{RequestPresetEvents, ResetGameOptions}  (FOREIGN ledger TUs),
//      MenuComponent::AppendExpectedAptComponent          (decl BrnMenuComponent.h:76),
//      GuiNetworkRouteInfo::AppendExpectedAptComponent    (decl BrnGuiNetworkRouteInfo.h:117),
//      HelpBar::AppendExpectedAptComponent                (decl BrnHelpBar.h:95 -- the §H3
//        grow HAS landed, so it is declared now; still no body in BrnHelpBar.cpp),
//      OptionsDataProfile::{GetNumCreatedOnlineGameOptions,
//                           GetNumReceivedOnlineGameOptions}  (decl h:212/213).
//    NOT link gaps -- these are all DEFINED, do not go hunting for them:
//      MenuToggleGroupVarSize<5>::{Clear, SetupGroup, HighlightNextItem,
//        HighlightPreviousItem, AppendExpectedAptComponent} -- BrnMenuToggleGroup.cpp
//        (Clear:103, HighlightNextItem:194, HighlightPreviousItem:204, SetupGroup:216,
//        AppendExpectedAptComponent:76) with an explicit BRN_INSTANTIATE_MENUTOGGLEGROUP(5)
//        at line 322,
//      SelectableGroup::{GetIndexFromId, HighlightIndex, HighlightNext, HighlightPrevious}
//        -- BrnSelectableGroup.cpp:116/131/171/211,
//      GuiCache::AppendExpectedAptComponent(GuiFlow, const char*) -- BrnGuiCache.cpp:853
//        (and GuiCache::GetOptionsDataProfile at :886),
//      CgsGui::GuiComponent::AddOutputAptViewState -- CgsGuiComponent.cpp:40.
//    GuiCache::IsMultiplayerAllowed is genuinely body-less (decl BrnGuiCache.h:451) but is
//    NOT a callee of either body here -- it is CheckPrivileges, group 02, that calls it.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptions.h"
#include <cstring>                                                       // memcpy
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface / GuiEventNetworkSuspension
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache (we are a friend)
#include "GameSource/Gui/BrnGuiOptionsDataProfile.h"                      // BrnGui::OptionsDataProfile
#include <cstring>                                                       // memcpy / memset / strncpy
#include "GameSource/GameState/BrnGameStateSharedIO.h"                    // GsmIO::E_MODE_ONLINE_FREE_BURN_LOBBY
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiFlow

namespace BrnGui
{

//
// WHAT THE FUNCTION DOES
// ----------------------
// The create-match page's controller handler: a jump-table switch over the input action id
// (`r28 - 0x29`, 11 slots covering 0x29..0x33; 0x2D..0x30 fall into the default and are
// ignored). Two arms scroll the five-row option WINDOW, two move the highlighted row's
// VALUE, one accepts the page, one backs out, one opens the load-options page.
//
// ⭐⭐ MEASURED CORRECTION TO THE WAVE-I SPEC (spec §5 TRAP 2). The spec says the '+' / ','
// arms compare a ZERO-extended miHighlightedIndex against GetIndexFromId(0), so a -1 (no row
// highlighted) reads back as 255 and can never match a -1 index. THE ASSEMBLY SAYS OTHERWISE:
//     0x824A7A34  lbz   r11, 0x11A5(r31)   ; the s8 SelectableGroup::miHighlightedIndex
//     0x824A7A40  extsb r30, r11           ; SIGN-extend
//     0x824A7A48  cmpw  cr6, r30, r3       ; signed word compare vs GetIndexFromId's s32
// and identically at 0x824A7AA8/0x824A7AB4/0x824A7ABC in the ',' arm. `extsb` sign-extends,
// so -1 == -1 IS taken. Reproduced verbatim below by reading the s8 member straight into an
// s32; do NOT re-introduce a u8 cast. (0x11A5 == 4517 == mCreateGameToggles(4352) +
// SelectableGroup::miHighlightedIndex(+0xA5) -- the offset corroborates the member exactly.)
//
// ⭐ VTABLE SLOTS MEASURED, NOT ASSUMED (headless IDA, scratchpad/waveI/g09_vt.txt). The
// MenuToggleGroupVarSize<5> ctor @0x824F9AA0 stores off_820730AC at +0x00, so the dispatch
// displacements resolve as +0x18 Clear, +0x28 SelectableGroup::HighlightNext, +0x2C
// SelectableGroup::HighlightPrevious, +0x30 SelectableGroup::HighlightIndex, +0x34
// HighlightNextItem, +0x38 HighlightPreviousItem. All are called by name below.
//
// NOTES TAKEN FROM THE ASM RATHER THAN HEX-RAYS
// ---------------------------------------------
//  * Hex-Rays lost the live range of the miStartItem base register in BOTH scroll arms and
//    printed `*v20 = v19;` / `*v27 = v28 + 1;` with v20/v27 never assigned. The asm computes
//    it as `addis r7, r31, 1 / addi r7, r7, -0x5AF4` == this + 0xA50C == miStartItem in each
//    arm (0x824A7B2C and 0x824A7C9C).
//  * The five-call window rebuild (StoreCreateGameOptions / Clear / SetupGroup(5,false) /
//    SetupCommonCreateGameOptions / HighlightCreateGameOptions) appears FOUR times, written
//    out each time -- there is no `bl` to a shared helper and no such helper in the ledger,
//    so it is written out here too rather than inventing one.
//  * The arrow states come from KPC_ARROW_ANIMATION_STATES (@0x82F2683C), dumped with
//    headless IDA: [0] "invisible", [1] "visible", [2] "animate". Scrolling UP animates the
//    up arrow and leaves the down arrow merely visible; scrolling DOWN is the mirror; with
//    one option or fewer BOTH go invisible. The up arrow is written first, the down arrow
//    second, in both arms (this+0x7A90 then this+0x7B1C).
//  * The suspension event on the '2' arm is posted onto the out-queue directly rather than
//    through CgsGui::StateInterface::OutputGuiEvent, whose committed body passes the event id
//    (45) as the AddEvent channel where the X360 passes 40. Same accommodation the group-03
//    partfile makes.
//  * The '1' arm copies mGameOptions OUT to the cache mirror (Dst = cache + 0xA800, Src =
//    this + 0xA0C0) -- the opposite direction to the copy HandleGuiCacheEvent makes.
//  * The '3' arm reads the two profile counters the X360 inlines as cache + 0x12B80 /
//    0x12B84; those are GetOptionsDataProfile() (+0xB878) plus miNumCreated/
//    ReceivedOnlineGameOptions (+0x7308/+0x730C), reached through the profile's accessors.
//  * No float compares anywhere in this body, so there is no NaN-polarity decision.
    namespace
    {
        // ---- AddEvent channel (the out-queue selector word) ---------------------------
        const s32 KI_CHANNEL_GUI_OUT = 40;   // X360 `li r5, 0x28`

        // ---- controller action ids (the in-queue payload's second word) ----------------
        // BrnGui's EGameInputActions values. The enum IS fully recovered
        // (references/DecFIGS/dwarfdump/GameSource/Input/GameInputActions.h:24) -- it just
        // has no committed home under b5-decomp/src yet, which is why these stay s32. Same
        // names and values the group-02 partfile's TriggerSound uses.
        const s32 KI_ACTION_GUI_UP      = 0x29;   // 41 GUI_UP      scroll the option window up
        const s32 KI_ACTION_GUI_DOWN    = 0x2A;   // 42 GUI_DOWN    scroll the option window down
        const s32 KI_ACTION_GUI_LEFT    = 0x2B;   // 43 GUI_LEFT    previous value on the current row
        const s32 KI_ACTION_GUI_RIGHT   = 0x2C;   // 44 GUI_RIGHT   next value on the current row
        const s32 KI_ACTION_GUI_SELECT  = 0x31;   // 49 GUI_SELECT  accept the page
        const s32 KI_ACTION_GUI_CANCEL  = 0x32;   // 50 GUI_CANCEL  back out of the page
        const s32 KI_ACTION_GUI_OPTION0 = 0x33;   // 51 GUI_OPTION0 open the saved-options page

        // ---- KPC_ARROW_ANIMATION_STATES indices (@0x82F2683C, measured) ----------------
        const s32 KI_ARROW_STATE_INVISIBLE = 0;   // "invisible"
        const s32 KI_ARROW_STATE_VISIBLE   = 1;   // "visible"
        const s32 KI_ARROW_STATE_ANIMATE   = 2;   // "animate"

        // The apt view whose state the two arrow animators are driven through.
        const char KAC_APT_TRANSITION[] = "apt_Transition";

        // ---- in-queue payload view -----------------------------------------------------
        // The state in-queue hands handlers the HEADER-STRIPPED payload; this handler reads
        // only the second word (`lwz r28, 4(r26)` at 0x824A790C). Same view the sibling
        // screens carry for CgsGui::GuiEventControllerInput*.
        struct ControllerButtonPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miButtonId;   // +0x04 (the input action id)
        };
    }

    // ------------------------------------------- HandleControllerInputCreateGame @0x824A7878
    void OnlineGameOptions::HandleControllerInputCreateGame(const CgsModule::Event* lpEvent)
    {
        // Non-fatal (BeginAssert / FireAssert / EndAssert, no early-out).
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event sent to OnlineGameOptions::HandleControllerInputCreateGame");   // cpp:494

        const ControllerButtonPayload* lpInput =
            reinterpret_cast<const ControllerButtonPayload*>(lpEvent);
        const s32 leAction = lpInput->miButtonId;

        switch (leAction)
        {
        // ---- scroll the five-row option window UP -------------------------------------
        case KI_ACTION_GUI_UP:
            {
                bool lbHandled = false;

                if (mCreateGameToggles.HighlightPrevious(false))
                {
                    // There was another row above: the highlight moved, nothing scrolls.
                    lbHandled = true;
                }
                else if (miStartItem > 0)
                {
                    // At the top of the window but not the top of the list: scroll one row.
                    lbHandled = true;
                    --miStartItem;

                    StoreCreateGameOptions();
                    mCreateGameToggles.Clear();
                    mCreateGameToggles.SetupGroup(KI_MAX_CREATE_GAME_OPTIONS, false);
                    SetupCommonCreateGameOptions();
                    HighlightCreateGameOptions();
                }
                else if (GetNumberOptions() > 1)
                {
                    // At the very top: wrap round to the last window and highlight its
                    // bottom row.
                    lbHandled = true;

                    const s32 liLastWindowStart = GetNumberOptions() - KI_MAX_CREATE_GAME_OPTIONS;
                    miStartItem = (liLastWindowStart > 0) ? liLastWindowStart : 0;

                    StoreCreateGameOptions();
                    mCreateGameToggles.Clear();
                    mCreateGameToggles.SetupGroup(KI_MAX_CREATE_GAME_OPTIONS, false);
                    SetupCommonCreateGameOptions();
                    HighlightCreateGameOptions();

                    s32 liVisibleRows = GetNumberOptions();
                    if (liVisibleRows >= KI_MAX_CREATE_GAME_OPTIONS)
                    {
                        liVisibleRows = KI_MAX_CREATE_GAME_OPTIONS;
                    }
                    mCreateGameToggles.HighlightIndex(liVisibleRows - 1);
                }

                if (lbHandled)
                {
                    // Scrolling up: the up arrow plays its "there is more above" animation.
                    if (GetNumberOptions() <= 1)
                    {
                        mUpArrowAnimator.AddOutputAptViewState(
                            KAC_APT_TRANSITION, KPC_ARROW_ANIMATION_STATES[KI_ARROW_STATE_INVISIBLE], false);
                        mDownArrowAnimator.AddOutputAptViewState(
                            KAC_APT_TRANSITION, KPC_ARROW_ANIMATION_STATES[KI_ARROW_STATE_INVISIBLE], false);
                    }
                    else
                    {
                        mUpArrowAnimator.AddOutputAptViewState(
                            KAC_APT_TRANSITION, KPC_ARROW_ANIMATION_STATES[KI_ARROW_STATE_ANIMATE], false);
                        mDownArrowAnimator.AddOutputAptViewState(
                            KAC_APT_TRANSITION, KPC_ARROW_ANIMATION_STATES[KI_ARROW_STATE_VISIBLE], false);
                    }

                    TriggerSound(leAction);
                }
            }
            break;

        // ---- scroll the five-row option window DOWN -----------------------------------
        case KI_ACTION_GUI_DOWN:
            {
                bool lbHandled = false;

                if (mCreateGameToggles.HighlightNext(false))
                {
                    lbHandled = true;
                }
                else
                {
                    const s32 liStartItem = miStartItem;

                    if (liStartItem + KI_MAX_CREATE_GAME_OPTIONS < GetNumberOptions())
                    {
                        // More list below the window: scroll one row and stay on the bottom.
                        lbHandled = true;
                        miStartItem = liStartItem + 1;

                        StoreCreateGameOptions();
                        mCreateGameToggles.Clear();
                        mCreateGameToggles.SetupGroup(KI_MAX_CREATE_GAME_OPTIONS, false);
                        SetupCommonCreateGameOptions();
                        HighlightCreateGameOptions();

                        s32 liVisibleRows = GetNumberOptions();
                        if (liVisibleRows >= KI_MAX_CREATE_GAME_OPTIONS)
                        {
                            liVisibleRows = KI_MAX_CREATE_GAME_OPTIONS;
                        }
                        mCreateGameToggles.HighlightIndex(liVisibleRows - 1);
                    }
                    else if (GetNumberOptions() > 1)
                    {
                        // At the very bottom: wrap round to the first window, top row.
                        lbHandled = true;
                        miStartItem = 0;

                        StoreCreateGameOptions();
                        mCreateGameToggles.Clear();
                        mCreateGameToggles.SetupGroup(KI_MAX_CREATE_GAME_OPTIONS, false);
                        SetupCommonCreateGameOptions();
                        HighlightCreateGameOptions();

                        mCreateGameToggles.HighlightIndex(0);
                    }
                }

                if (lbHandled)
                {
                    // Scrolling down: the DOWN arrow is the one that animates.
                    if (GetNumberOptions() <= 1)
                    {
                        mUpArrowAnimator.AddOutputAptViewState(
                            KAC_APT_TRANSITION, KPC_ARROW_ANIMATION_STATES[KI_ARROW_STATE_INVISIBLE], false);
                        mDownArrowAnimator.AddOutputAptViewState(
                            KAC_APT_TRANSITION, KPC_ARROW_ANIMATION_STATES[KI_ARROW_STATE_INVISIBLE], false);
                    }
                    else
                    {
                        mUpArrowAnimator.AddOutputAptViewState(
                            KAC_APT_TRANSITION, KPC_ARROW_ANIMATION_STATES[KI_ARROW_STATE_VISIBLE], false);
                        mDownArrowAnimator.AddOutputAptViewState(
                            KAC_APT_TRANSITION, KPC_ARROW_ANIMATION_STATES[KI_ARROW_STATE_ANIMATE], false);
                    }

                    TriggerSound(leAction);
                }
            }
            break;

        // ---- previous value on the highlighted row ------------------------------------
        case KI_ACTION_GUI_LEFT:
            if (mCreateGameToggles.HighlightPreviousItem())
            {
                // Changing the GAME MODE row rebuilds the whole option set below it.
                // See the banner: the console SIGN-extends the s8 highlight (`extsb`), so a
                // -1 highlight does match a -1 game-mode row.
                const s32 liHighlightedRow = mCreateGameToggles.miHighlightedIndex;
                const s32 liGameModeRow = mCreateGameToggles.GetIndexFromId(
                    static_cast<u64>(static_cast<u32>(CreateMatchOption::E_OPTION_GAME_MODE)));

                if (liHighlightedRow == liGameModeRow)
                {
                    StoreGameMode();
                    ResetGameOptions();
                    HighlightCreateGameOptions();
                    RequestPresetEvents();
                }

                TriggerSound(leAction);
            }
            break;

        // ---- next value on the highlighted row ----------------------------------------
        case KI_ACTION_GUI_RIGHT:
            if (mCreateGameToggles.HighlightNextItem())
            {
                const s32 liHighlightedRow = mCreateGameToggles.miHighlightedIndex;
                const s32 liGameModeRow = mCreateGameToggles.GetIndexFromId(
                    static_cast<u64>(static_cast<u32>(CreateMatchOption::E_OPTION_GAME_MODE)));

                if (liHighlightedRow == liGameModeRow)
                {
                    StoreGameMode();
                    ResetGameOptions();
                    HighlightCreateGameOptions();
                    RequestPresetEvents();
                }

                TriggerSound(leAction);
            }
            break;

        // ---- accept the page ----------------------------------------------------------
        case KI_ACTION_GUI_SELECT:
            StoreCreateGameOptions();

            // Publish the edited options back into the cache's mirror and move on.
            memcpy(reinterpret_cast<GuiEventNetworkGameParams*>(
                       &mpGuiCache->maOnlineGameModeOptionsStorage[0]),
                   &mGameOptions,
                   sizeof(mGameOptions));   // X360 size 0x1E0 == the whole cache mirror

            TriggerSound(leAction);
            SendStateEvent("ADVANCE");
            break;

        // ---- back out of the page ------------------------------------------------------
        case KI_ACTION_GUI_CANCEL:
            {
                const char* lpacStateEvent;

                if (mpGuiCache->mbOnlineStartPending)   // GuiCache +0x4B53
                {
                    // This screen armed the online start: lift the network suspension it
                    // put in place, then back out on the quiet path.
                    CgsGui::GuiEventNetworkSuspension lNetworkSuspension(false);
                    mpStateInterface->GetOutputEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lNetworkSuspension),
                        KI_CHANNEL_GUI_OUT,
                        static_cast<s32>(sizeof(lNetworkSuspension)));   // X360 record size 16

                    mpGuiCache->mbOnlineStartPending = false;
                    lpacStateEvent = "GO_BACK_EASY";
                }
                else
                {
                    lpacStateEvent = "GO_BACK";
                }

                SendStateEvent(lpacStateEvent);
                TriggerSound(leAction);
            }
            break;

        // ---- open the saved / recent options page --------------------------------------
        case KI_ACTION_GUI_OPTION0:
            {
                // Only worth opening when the profile actually holds a saved or a received
                // set of options.
                OptionsDataProfile* lpProfile = mpGuiCache->GetOptionsDataProfile();
                if (lpProfile->GetNumCreatedOnlineGameOptions() > 0 ||
                    lpProfile->GetNumReceivedOnlineGameOptions() > 0)
                {
                    ShowLoadScreen();
                }
            }
            break;

        default:
            // 0x2D..0x30 land here through the jump table; every other id misses it.
            break;
        }
    }

//
// WHAT THE FUNCTION DOES
// ----------------------
// The screen's GuiCache-arrival sink, and its one-shot page setup. The GUI module
// broadcasts the cache pointer to every state; this screen acts only on the FIRST one
// (`if (!v3[10560])` == mpGuiCache still null -- everything below sits inside that guard).
// On that first arrival it: latches the cache, publishes the online ticker line, and then
// either registers every apt component the page waits on (when the player is allowed to
// play multiplayer) or backs straight out again (when they are not). Either way it then
// pulls the cache's 480-byte network-game-params mirror into its own editable copy and, if
// the options changed while this screen was away -- or the lobby is a free-burn lobby --
// publishes the id-409 refresh record and clears the changed flag. Finally it asks for the
// preset events that populate the option rows.
//
// NOTES TAKEN FROM THE ASM RATHER THAN HEX-RAYS
// ---------------------------------------------
//  * The ticker payload is built in a stack scratch at sp+0x80 and memcpy'd into the record
//    payload at sp+0x8AC; built directly in the record here (the CarSelectVehicle_Input
//    precedent), which is the same bytes on the wire.
//  * The payload seeds differ from CarSelectVehicle::SetTicker's. Measured at
//    0x824A86A4..0x824A86C0 (the offsets are payload-relative):
//        +0x810 = 0   mi8NumStrings
//        +0x811 = 1   maFlags[0]      <-- SetTicker leaves this 0
//        +0x812 = 0   maFlags[1]
//        +0x813 = 1   maFlags[2]      <-- the seed SetTicker also sets
//        +0x814 = 0   maFlags[3]
//    with maiStringTypes (+0x00..+0x0F) zeroed by two `std` and the 0x800-byte string block
//    zeroed by the memset. A whole-struct memset plus the two flag stores is identical.
//  * The six name-registrations are `sub_824F87C0(cache, 0, component + 4)`. +4 is
//    CgsGui::GuiComponent::macName, i.e. the component's GetName(); and sub_824F87C0 is the
//    name-taking entry of GuiCache::AppendExpectedAptComponent already declared at
//    BrnGuiCache.h:231. Their order is up / down / load-header ANIM / load-header TEXT /
//    title TEXT / map-border (this+0x7A94, 0x7B20, 0x7BAC, 0x7CC4, 0x7DEC, 0x7C38) -- the
//    map border is registered LAST, out of declaration order; kept as the asm has it.
//  * The suspension event is posted onto the out-queue directly rather than through
//    CgsGui::StateInterface::OutputGuiEvent, whose committed body passes the event id (45)
//    as the AddEvent channel where the X360 passes 40. Same accommodation the group-03
//    partfile makes.
//  * The id-409 record's two payload bytes are built as a halfword in a scratch slot and
//    stored with a single `sth` (0x824A88B0..0x824A88CC); modelled as the two bytes they are.
//    Id 409 has no declared event type anywhere in the recovered DWARF slice, so the record
//    is a file-local wire with a FLAG role-name taken from what the call site does with it.
    namespace
    {
        // ---- AddEvent channel (the out-queue selector word) ---------------------------

        // The ticker string's format/kind selector (X360 `li r5, 2` at 0x824A870C) -- the
        // same word BrnGui::CarSelectVehicle::SetTicker passes to AddString.
        const s32 KI_TICKER_STRING_TYPE = 2;

        // ---- the three ticker lines (rodata literals) ----------------------------------
        const char KAC_RANKED_TICKER_TEXT[]   = "ONLINE_RANKED_TICKER_TEXT";
        const char KAC_FREEBURN_TICKER_TEXT[] = "ONLINE_FREEBURN_TICKER_TEXT";
        const char KAC_UNRANKED_TICKER_TEXT[] = "ONLINE_UNRANKED_TICKER_TEXT";

        // ---- in-queue payload view -----------------------------------------------------
        // The state in-queue hands handlers the HEADER-STRIPPED payload, so the incoming
        // cache pointer is the payload's first word (`lwz r11, 0(r26)` at 0x824A8604,
        // re-read at 0x824A8698). The DWARF types the parameter const GuiEventCache*, whose
        // home header hard-collides with BrnGuiEventTypeDefs.h -- the same file-local view
        // the wave-H twin (BrnOnlineGameRoomPlayerInfo_wH_18.cpp) carries.
        struct GuiEventCachePayload : public CgsModule::Event
        {
            GuiCache* mpGuiCache;   // +0x00
        };

        // ---- out-queue wire records ----------------------------------------------------

        // The custom ticker message payload (0x818 bytes). Layout recovered store-for-store
        // from BrnGui::GuiEventTickerCustomMessage::AddString @0x823A6940, whose asserts bake
        // "GameSource/Gui/BrnGuiEventTypeDefs.h" lines 390/391/392:
        //   +0x000  s32  maiStringTypes[4]
        //   +0x010  char maacStrings[4][512]
        //   +0x810  s8   mi8NumStrings          (`lbz`/`extsb`, bounded < 4)
        //   +0x811..+0x814  four flag bytes
        // Kept TU-LOCAL rather than promoted into BrnGuiEventTypeDefs.h: the type already has
        // an opaque twin in BrnGuiDemangledEventTypes.h (GuiEvent<537> + a 2060-byte blob) and
        // the two headers are mutually exclusive by construction, so a second definition would
        // be a live ODR fork. Identical to the view BrnCarSelectVehicle_Input.cpp carries.
        struct GuiTickerCustomMessagePayload
        {
            static const s32 KI_MAX_NUM_STRINGS   = 4;     // AddString's bound (h:391)
            static const s32 KI_MAX_STRING_LENGTH = 512;   // AddString's strncpy count

            s32  maiStringTypes[KI_MAX_NUM_STRINGS];                       // +0x000
            char maacStrings[KI_MAX_NUM_STRINGS][KI_MAX_STRING_LENGTH];    // +0x010
            s8   mi8NumStrings;                                            // +0x810
            // FLAG: four flag bytes at +0x811..+0x814 whose roles are not recovered. This
            // producer seeds them { 1, 0, 1, 0 }; CarSelectVehicle::SetTicker seeds
            // { 0, 0, 1, 0 }, so flag 0 is what distinguishes the two ticker kinds.
            u8   maFlags[4];                                               // +0x811
            u8   maPad815[3];                                              // +0x815 (sizeof == 0x818)

            // @0x823A6940 -- copy lpString into the next free 512-byte slot and record its
            // format type. The count is read as a SIGNED byte (X360 `lbz` + `extsb`).
            void AddString(const char* lpString, s32 liType)
            {
                CGS_ASSERT(mi8NumStrings >= 0, "mi8NumStrings >= 0");                   // h:390
                CGS_ASSERT(mi8NumStrings < KI_MAX_NUM_STRINGS,
                           "mi8NumStrings < KI_MAX_NUM_STRINGS");                       // h:391
                CGS_ASSERT(lpString != 0, "lpString");                                  // h:392

                std::strncpy(maacStrings[mi8NumStrings], lpString,
                             static_cast<size_t>(KI_MAX_STRING_LENGTH));
                maiStringTypes[mi8NumStrings] = liType;
                ++mi8NumStrings;
            }
        };

        // { 0x818, 537, 12, <the message> }, channel 40, 0x824 bytes -- all three written as
        // host expressions.
        struct GuiTickerCustomMessageWire : public CgsGui::GuiEvent<537>
        {
            GuiTickerCustomMessagePayload mMessage;   // +0x0C

            GuiTickerCustomMessageWire()
                : CgsGui::GuiEvent<537>(static_cast<u32>(sizeof(GuiTickerCustomMessagePayload)),
                                        static_cast<u32>(sizeof(CgsGui::GuiEvent<537>)))
            {
                std::memset(&mMessage, 0, sizeof(mMessage));
                mMessage.maFlags[0] = 1;   // +0x811 (this producer's distinguishing seed)
                mMessage.maFlags[2] = 1;   // +0x813
            }
        };

        // Id 409 -- the "the online game options changed, re-publish them" refresh record.
        // FLAG: id 409 carries no declared event struct anywhere in the recovered DWARF
        // slice, so the payload shape and the role-name both come from this single call
        // site: the X360 stack-builds { 2, 409, 12 } + the two payload bytes { 1, 0 } and
        // publishes 16 bytes on channel 40 (0x824A88A8..0x824A88E0). The two flag names are
        // deliberately role-free -- nothing in the binary names them.
        struct GuiEventOnlineGameOptionsRefresh
        {
            u8 mu8FlagA;   // +0x00 (set)
            u8 mu8FlagB;   // +0x01 (clear)
        };

        typedef CgsGui::GuiEvent<409> OnlineGameOptionsRefreshHeader;

        struct GuiEventOnlineGameOptionsRefreshWire : public OnlineGameOptionsRefreshHeader
        {
            GuiEventOnlineGameOptionsRefresh mRefresh;   // +0x0C
            u8                               maPad0E[2]; // +0x0E (record is 16 bytes)

            GuiEventOnlineGameOptionsRefreshWire()
                : OnlineGameOptionsRefreshHeader(
                      static_cast<u32>(sizeof(GuiEventOnlineGameOptionsRefresh)),
                      static_cast<u32>(sizeof(OnlineGameOptionsRefreshHeader)))
            {
                mRefresh.mu8FlagA = 1;
                mRefresh.mu8FlagB = 0;
                maPad0E[0] = 0;
                maPad0E[1] = 0;
            }
        };

        // Layout pins for the two records above: the console posts 0x824 and 0x10 bytes with
        // payload-size words 0x818 and 2 and payload-offset words 12 and 12.
        typedef char KAC_ASSERT_TICKER_PAYLOAD_SIZE[
            sizeof(GuiTickerCustomMessagePayload) == 0x818 ? 1 : -1];
        typedef char KAC_ASSERT_TICKER_WIRE_SIZE[
            sizeof(GuiTickerCustomMessageWire) == 0x824 ? 1 : -1];
        typedef char KAC_ASSERT_REFRESH_PAYLOAD_SIZE[
            sizeof(GuiEventOnlineGameOptionsRefresh) == 2 ? 1 : -1];
        typedef char KAC_ASSERT_REFRESH_WIRE_SIZE[
            sizeof(GuiEventOnlineGameOptionsRefreshWire) == 16 ? 1 : -1];
    }

    // ------------------------------------------------------ HandleGuiCacheEvent @0x824A85E8
    void OnlineGameOptions::HandleGuiCacheEvent(const CgsModule::Event* lpEvent)
    {
        const GuiEventCachePayload* lpCacheEvent =
            reinterpret_cast<const GuiEventCachePayload*>(lpEvent);

        // Non-fatal (BeginAssert / FireAssert / EndAssert, no early-out) -- the X360 falls
        // straight through into the body, so a null cache would be latched as-is.
        CGS_ASSERT(lpCacheEvent->mpGuiCache != 0,
                   "Invalid cache in HandleGuiCacheEvent::Update");   // cpp:1048

        // ---- first arrival only: the whole page setup is a one-shot -------------------
        if (mpGuiCache != 0)
        {
            return;
        }

        mpGuiCache = lpCacheEvent->mpGuiCache;

        // ---- the ticker line ----------------------------------------------------------
        // Which of the three online blurbs runs along the ticker depends on how the match
        // was entered: ranked, free-burn, or plain unranked.
        {
            const char* lpacTickerText;
            if (mpGuiCache->mbOnlineMatchRanked)             // GuiCache +0x4B51
            {
                lpacTickerText = KAC_RANKED_TICKER_TEXT;
            }
            else if (mpGuiCache->mbOnlineMatchUnranked)      // GuiCache +0x4B52
            {
                lpacTickerText = KAC_FREEBURN_TICKER_TEXT;
            }
            else
            {
                lpacTickerText = KAC_UNRANKED_TICKER_TEXT;
            }

            GuiTickerCustomMessageWire lTicker;
            lTicker.mMessage.AddString(lpacTickerText, KI_TICKER_STRING_TYPE);

            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lTicker), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lTicker)));   // X360 record size 0x824
        }

        // ---- either arm the page, or back straight out --------------------------------
        if (CheckPrivileges())
        {
            // Tell the cache which apt components this page has to wait on before it can
            // report the screen flow ready. The four components with their own helper go
            // through it; the four animators and two text fields are registered by name.
            mMenuOptions.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);
            mCreateGameToggles.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache, true);
            mRouteInfoDisplay.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);
            mHelpBar.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);

            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mUpArrowAnimator.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mDownArrowAnimator.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mLoadHeaderAnimator.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mLoadHeaderText.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mTitleText.GetName());
            // The map border comes LAST on the console, out of declaration order.
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mMapBorderAnimator.GetName());
        }
        else if (mpGuiCache->mbOnlineStartPending)          // GuiCache +0x4B53
        {
            // No multiplayer privilege, and this screen owns the pending online start:
            // lift the network suspension it armed, then back out on the quiet path.
            CgsGui::GuiEventNetworkSuspension lNetworkSuspension(false);
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lNetworkSuspension),
                KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lNetworkSuspension)));   // X360 record size 16

            mpGuiCache->mbOnlineStartPending = false;
            SendStateEvent("GO_BACK_EASY");
        }
        else
        {
            SendStateEvent("GO_BACK");
        }

        // ---- take a working copy of the cache's params mirror -------------------------
        memcpy(&mGameOptions,
               reinterpret_cast<const GuiEventNetworkGameParams*>(
                   &mpGuiCache->maOnlineGameModeOptionsStorage[0]),
               sizeof(mGameOptions));   // X360 size 0x1E0 == the whole cache mirror

        // ---- ask for a fresh publish when the options went stale ----------------------
        // Either something changed the options while this screen was away, or the lobby is
        // a free-burn lobby (whose options this page always re-publishes on entry).
        if (mpGuiCache->mbOnlineGameOptionsChanged ||          // GuiCache +0xA9E0
            mGameOptions.meGameMode ==
                BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY)
        {
            GuiEventOnlineGameOptionsRefreshWire lRefresh;
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lRefresh), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lRefresh)));   // X360 record size 16

            mpGuiCache->mbOnlineGameOptionsChanged = false;
        }

        // Populate the option rows from the preset the current mode selects.
        RequestPresetEvents();
    }
}
