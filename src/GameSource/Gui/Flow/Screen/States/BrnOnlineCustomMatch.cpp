// GameSource/Gui/Flow/Screen/States/BrnOnlineCustomMatch.cpp
//
// Created 2026-09-15 by tools/work/fold_partfiles.py --create-parent (b5-decomp issue #20).
// This family had NO parent TU: its bodies lived in 7 wave partfile(s), each
// with its own hand-written mount line in tools/build/build_game_exe.bat. They are folded
// here in MOUNT ORDER; every partfile's own header comment block is kept verbatim above
// its bodies (the address annotations are the evidence trail). No body was edited.
//
// Folded, in mount order:
//     BrnOnlineCustomMatch_wJ_01.cpp
//     BrnOnlineCustomMatch_wJ_02.cpp
//     BrnOnlineCustomMatch_wJ_03.cpp
//     BrnOnlineCustomMatch_wJ_04.cpp
//     BrnOnlineCustomMatch_wJ_05.cpp
//     BrnOnlineCustomMatch_wJ_06.cpp
//     BrnOnlineCustomMatch_wJ_07.cpp
//
// The header of the first of them (BrnOnlineCustomMatch_wJ_01.cpp) follows verbatim, as this file's own.

// wave-J partfile -- BrnGui::OnlineCustomMatch screen transitions (group 1 of the wave).
//   ShowInitialScreen   @0x824971D0
//   ShowParamSelection  @0x8248FE30
//   ShowNoGamesFound    @0x8248BD78
//
// CONDUCTOR NOTE (merge): this partfile deliberately DEFINES NO OnlineCustomMatch class
// statics. Every KAC_/KAPC_/KA_ static it reads (KAPC_ANIMATION_STATES,
// KAC_NO_GAMES_FOUND_STRING_ID, KAPC_YES_NO_BUTTON_STRING_ID, KAC_GAME_MODE_STRING_ID,
// KAC_OPPONENT_OPTION_STRING_ID, KAPC_OPPONENT_OPTION_STRING_IDS,
// KA_GAME_MODE_SEARCH_OPTION_STRING_IDS) is shared with the other wave-J partfiles, so the
// single definition belongs in the consolidated BrnOnlineCustomMatch.cpp -- exactly how
// wave I placed OnlineGameOptions' statics (BrnOnlineGameOptions.cpp:12-32) rather than in
// any wI_* partfile. Until that file exists these are LINK-time unresolved externals; the
// per-TU `cl /c` gate cannot see them. Measured values are in the spec (§2) and the header.

// the union of the BrnOnlineCustomMatch_w*.cpp partfiles' #include lines, first occurrence wins, mount order (2026-09-15)
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineCustomMatch.h"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"       // AddOutputAptViewState
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface / the out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache (friend)
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include <cstddef>                                                       // offsetof (wire pins)
#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsID / CgsIDCompress
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiOverlayWaitFinishRequest (the 188 payload)
#include <cstring>                                                        // memcpy (the 604-byte results copy)
#include "GameSource/Gui/Flow/Shared/Components/BrnTable.h"               // Table::Update
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuToggleGroup.h"     // MenuToggleGroupVarSize<3>::Update (SelectableGroup)
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuComponent.h"       // MenuComponent::Update (SelectableGroup)

// ============================================================================
// FOLDED FROM BrnOnlineCustomMatch_wJ_01.cpp (wave J) on 2026-09-15 by tools/work/fold_partfiles.py.
// Its header is THIS FILE'S header, at the top -- not repeated here.
// ============================================================================

namespace BrnGui
{
    namespace
    {
        // The apt view every component transition in this file is posted on: X360
        // r4 = aAptTransition_1 ("apt_Transition") for all fifteen calls, r6 = 0
        // (lbImmediate false). Shared with the other wave-J partfiles of this TU --
        // keep only one copy on merge.
        const char KAC_APT_TRANSITION_NAME[] = "apt_Transition";
        // Indices into OnlineCustomMatch::KAPC_ANIMATION_STATES @0x82F266B0
        // == { "Visible", "Invisible", "Refresh" }. The asm reaches them as
        // off_82F266B0[0] / off_82F266B4[0], i.e. slots 0 and 1 of that pointer table.
        const s32  KI_ANIMATION_STATE_VISIBLE   = 0;   // KAPC_ANIMATION_STATES[0] == "Visible"
        const s32  KI_ANIMATION_STATE_INVISIBLE = 1;   // KAPC_ANIMATION_STATES[1] == "Invisible"

        // The out-queue channel selector (X360 `li r5, 0x28` at 0x82497250).
        const s32 KI_CHANNEL_GUI_OUT = 40;

        // The BrnNetwork::ESearchGameModes value this screen forces when it enters
        // straight into a freeburn-lobby search (X360 `li r30, 4` at 0x824971F4; the same
        // 4 appears as the third row of KA_GAME_MODE_SEARCH_OPTION_STRING_IDS, whose
        // caption is "$ONLINE_GAME_OPTION_MODE_FREEBURN_LOBBY"). That enum has no
        // committed home yet, so the value is carried as a named local constant.
        const s32 KI_SEARCH_GAME_MODE_FREEBURN_LOBBY = 4;

        // ---- the out-queue wire record ---------------------------------------------
        // Id 252 == BrnGui::GuiEventNetworkCustomMatchSearch, the network module's
        // custom-match search request. The X360 stack-builds it at sp+0x50 as
        //     word0 = 12 (payload bytes), word1 = 0xFC (252), word2 = 12 (payload offset),
        //     word3 = meGameMode, word4 = meSearchOpponentTypes, word5 = the whole word at
        //     mLastSearchParams+8 (the two flag bytes plus their padding),
        // then AddEvent(queue, record, 40, 24) at 0x82497270.
        //
        // HEADER0 IS THE PAYLOAD BYTE COUNT. Here the payload is exactly 12 bytes with no
        // trailing padding, so sizeof(record) - offsetof(payload) happens to agree -- but
        // it is still taken from the payload members, per the wave-I rule.
        struct GuiEventNetworkCustomMatchSearchWire : public CgsGui::GuiEvent<252>
        {
            s32  meGameMode;        // payload +0x00
            s32  meSearchOpponentTypes;  // payload +0x04
            bool mbRanked;          // payload +0x08
            bool mbFreeburn;        // payload +0x09
            u8   maPad[2];          // payload +0x0A -- the console copies word +8 whole

            explicit GuiEventNetworkCustomMatchSearchWire(
                const GuiEventNetworkCustomMatchSearch& lrParams)
                : CgsGui::GuiEvent<252>(
                      static_cast<u32>(sizeof(s32) + sizeof(s32) + sizeof(s32)),   // X360 12
                      static_cast<u32>(offsetof(GuiEventNetworkCustomMatchSearchWire,
                                                meGameMode)))                      // X360 12
                , meGameMode(lrParams.meGameMode)
                , meSearchOpponentTypes(lrParams.meSearchOpponentTypes)
                , mbRanked(lrParams.mbRanked)
                , mbFreeburn(lrParams.mbFreeburn)
            {
                // The console's third payload word is a straight `lwz` of the member's
                // +8 word, so the two pad bytes carry whatever the member holds. The host
                // copies the two flags by name and zeroes the padding rather than
                // publishing uninitialised stack bytes.
                maPad[0] = 0;
                maPad[1] = 0;
            }
        };
    }

//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x824971D0.json, asm arbitrated over Hex-Rays).
//
// Notes taken from the asm rather than the pseudocode:
//  * `lwz r11, 0x4410(r31)` (17424) is mpGuiCache; `lbz r10, 0x4B52(r11)` (19282) is
//    GuiCache::mbOnlineMatchUnranked and `lbz r9, 0x4B51(r11)` (19281) is
//    mbOnlineMatchRanked. All three console offsets are DOCUMENTATION ONLY -- the reads
//    below are by member name.
//  * The four member stores are register-indexed, which is why Hex-Rays renders them as
//    bare `*(a1 + N)` writes: `addis r11, r31, 1` / `addi r11, r11, -0x1DB0` forms
//    this + 65536 - 7600 == this + 57936 == &mLastSearchParams (so `stw r30, 0(r11)` is
//    meGameMode = 4), `stwx r9, r31, 0xE254` (57940) is meSearchOpponentTypes = 0,
//    `stbx r9, r31, 0xE258` (57944) is mbRanked, `stbx r4, r31, 0xE259` (57945) is
//    mbFreeburn = true.
//  * The record's payload words are then RE-READ from the member (`lwz r10, 4(r11)` /
//    `lwz r11, 8(r11)`), i.e. the wire is built from mLastSearchParams after it is
//    filled in -- not from the registers. Reproduced by passing the member to the wire.
//  * `addi r3, r10, 0xC` on `lwz r10, 0x1C(r31)` is mpStateInterface->GetOutputEventQueue()
//    (the queue sits 12 bytes into the state interface); `li r5, 0x28` == channel 40 and
//    `li r6, 0x18` == 24, the record size -- published here as the HOST sizeof.
//  * `stw r10(=3), 0x38(r31)` is meSubState = E_SUBSTATE_SEARCHING, stored AFTER the post
//    and BEFORE ShowMessage -- keep that order. The else arm's `li r11, 2` is
//    E_SUBSTATE_SELECTING_PARAMS, stored before the ShowParamSelection tail call (which
//    sets it again; the X360 really does both).
//  * No floats anywhere in this body, so there is no NaN-polarity decision to make.
//
    // ================================================================================
    //  ShowInitialScreen  @ 0x824971D0
    //
    //  First screen the custom-match page shows once its components are up. An unranked
    //  (freeburn) entry skips the parameter form entirely and fires a fixed
    //  freeburn-lobby search straight away; everything else drops into the form.
    // ================================================================================
    void OnlineCustomMatch::ShowInitialScreen()
    {
        if (mpGuiCache->mbOnlineMatchUnranked)
        {
            // Fixed search: freeburn lobbies, any opponent, carrying the cache's ranked
            // flag through and marking the request as the unranked/freeburn flavour.
            mLastSearchParams.meGameMode       = KI_SEARCH_GAME_MODE_FREEBURN_LOBBY;
            mLastSearchParams.meSearchOpponentTypes = 0;
            mLastSearchParams.mbRanked         = mpGuiCache->mbOnlineMatchRanked;
            mLastSearchParams.mbFreeburn       = true;

            GuiEventNetworkCustomMatchSearchWire lSearch(mLastSearchParams);
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lSearch), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lSearch)));   // X360 record size 24

            meSubState = E_SUBSTATE_SEARCHING;

            ShowMessage(KAC_SEARCHING_STRING_ID);

            // The button prompts belong to the parameter form, so they go away while the
            // search runs.
            mButtonPromptAnimation.AddOutputAptViewState(
                KAC_APT_TRANSITION_NAME,
                KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_INVISIBLE], false);
        }
        else
        {
            meSubState = E_SUBSTATE_SELECTING_PARAMS;
            ShowParamSelection();
        }
    }
}

namespace BrnGui
{

//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x8248FE30.json, asm arbitrated over Hex-Rays).
//
// Notes taken from the asm rather than the pseudocode:
//  * The five AddOutputAptViewState calls and the state each one selects are read off the
//    `addi r3, r31, <off>` / `lwz r5, (off_82F266B0|off_82F266B4)` pairs:
//        +0x3C  (60)  mMessageAnimation        -> off_82F266B4 "Invisible"
//        +0xC8  (200) mMessageButtonsAnimation -> off_82F266B4 "Invisible"
//        +0x1E0 (480) mFoundGamesAnimation     -> off_82F266B4 "Invisible"
//        +0x26C (620) mButtonPromptAnimation   -> off_82F266B0 "Visible"
//        +0x154 (340) mSearchParamsAnimation   -> off_82F266B0 "Visible"
//    Those console offsets are DOCUMENTATION ONLY -- every access below is by member name,
//    so the host's own (wider) layout applies.
//  * SetupToggle's argument roles come from the 2026-08-02 corrected declaration
//    (BrnMenuToggleGroup.h:96) and match the asm exactly:
//    r4 = index, r5 = numOptions, r6 = active, r7 = caption, r8 = options[], r9 = ids[].
//    Both calls pass `li r9, 0` -- no id array, the rows answer by option INDEX.
//  * The first call's option array is built on the STACK at 0x8248FEF0..0x8248FF04 from
//    three `lwz` loads out of off_8205E964 at strides 0/8/16 -- i.e. the .mpcStringID of
//    KA_GAME_MODE_SEARCH_OPTION_STRING_IDS[0..2], not three inline string literals (a
//    literal would have emitted lis/addi per string). The compiler unrolled the copy; it is
//    re-rolled here. The console stride 8 is sizeof(StringGameModeMapping) ON X360 and is
//    NOT used as a host stride -- the loop indexes the array.
//  * The second call passes the rodata table address straight into r8, so the option array
//    is KAPC_OPPONENT_OPTION_STRING_IDS itself.
//  * `stw r11(=2), 0x38(r31)` is meSubState = E_SUBSTATE_SELECTING_PARAMS, stored BEFORE
//    the tail call to HighlightLastSearchParams -- keep that order (the callee reads it).
//  * No floats anywhere in this body, so there is no NaN-polarity decision to make.
//
    // ================================================================================
    //  ShowParamSelection  @ 0x8248FE30
    //
    //  Swap the page over to the search-parameter form: hide the message text, its button
    //  row and the found-games table, show the button prompts and the two search toggles,
    //  then re-arm the toggles from the last search the player ran.
    // ================================================================================
    void OnlineCustomMatch::ShowParamSelection()
    {
        mMessageAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_INVISIBLE],
                                                false);
        mMessageButtonsAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                       KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_INVISIBLE],
                                                       false);
        mFoundGamesAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                   KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_INVISIBLE],
                                                   false);
        mButtonPromptAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                     KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_VISIBLE],
                                                     false);
        mSearchParamsAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                     KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_VISIBLE],
                                                     false);

        // Two live rows, no wrap (X360 `li r4, 2` / `li r5, 0`).
        mSearchParms.SetupGroup(2, 0);

        // Row 0 -- the game-mode filter. Its option captions are the display strings of the
        // mode-mapping table (the compiler unrolled this copy into three lwz/stw pairs).
        const s32   KI_NUM_GAME_MODE_OPTIONS = 3;   // X360 `li r5, 3` == the table's extent
        const char* lapcGameModeOptions[KI_NUM_GAME_MODE_OPTIONS];
        for (s32 li = 0; li < KI_NUM_GAME_MODE_OPTIONS; ++li)
        {
            lapcGameModeOptions[li] = KA_GAME_MODE_SEARCH_OPTION_STRING_IDS[li].mpcStringID;
        }
        mSearchParms.SetupToggle(0, KI_NUM_GAME_MODE_OPTIONS, true, KAC_GAME_MODE_STRING_ID,
                                 lapcGameModeOptions, 0);

        // Row 1 -- the opponents filter (any / friends+rivals / friends / rivals). The option
        // table is const in this class but SetupToggle's owning header takes a plain
        // `const char**`, so the pointer is cast through; the console passes the rodata table
        // address (off_82F266DC) straight into r8.
        const s32 KI_NUM_OPPONENT_OPTIONS = 4;      // X360 `li r5, 4` == the table's extent
        mSearchParms.SetupToggle(1, KI_NUM_OPPONENT_OPTIONS, true, KAC_OPPONENT_OPTION_STRING_ID,
                                 const_cast<const char**>(KAPC_OPPONENT_OPTION_STRING_IDS), 0);

        meSubState = E_SUBSTATE_SELECTING_PARAMS;

        HighlightLastSearchParams();
    }

//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x8248BD78.json, asm arbitrated over Hex-Rays).
//
// Notes taken from the asm rather than the pseudocode:
//  * The five transitions, in the console's order (offsets documentation only):
//        +0x3C  (60)  mMessageAnimation        -> off_82F266B0 "Visible"
//        +0xC8  (200) mMessageButtonsAnimation -> off_82F266B0 "Visible"
//        +0x26C (620) mButtonPromptAnimation   -> off_82F266B0 "Visible"
//        +0x154 (340) mSearchParamsAnimation   -> off_82F266B4 "Invisible"
//        +0x1E0 (480) mFoundGamesAnimation     -> off_82F266B4 "Invisible"
//  * `addi r3, r31, 0x41C0` (16832) is mMessageText and `addi r29, r31, 0x3100` (12544) is
//    mMessageButtons -- the single-argument TextField::SetText(const char*) overload
//    (BrnGuiTextField.h:67) and MenuComponent::SetupMenu/SetText (BrnMenuComponent.h:48/51).
//  * The caption loop at 0x8248BE30..0x8248BE50 walks a POINTER, from off_82F266A4 to the
//    exclusive bound off_82F266AC, stepping 4 (a console pointer). Those two addresses are
//    KAPC_YES_NO_BUTTON_STRING_ID and the array that follows it in rodata
//    (KAPC_OK_BUTTON_STRING_ID), so the bound is simply the end of the two-entry YES/NO
//    table: the loop is re-rolled here over the array's own extent. The console stride 4 is
//    a 32-bit pointer and is deliberately NOT reproduced -- host pointers are 8 bytes.
//  * `stw r11(=6), 0x38(r31)` is meSubState = E_SUBSTATE_NO_GAMES_FOUND, stored last.
//  * No floats anywhere in this body, so there is no NaN-polarity decision to make.
//
    // ================================================================================
    //  ShowNoGamesFound  @ 0x8248BD78
    //
    //  The search came back empty while the player is out of a game: show the message text
    //  and its YES/NO button row over the (hidden) search form and found-games table, and
    //  ask whether to search again.
    // ================================================================================
    void OnlineCustomMatch::ShowNoGamesFound()
    {
        mMessageAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_VISIBLE],
                                                false);
        mMessageButtonsAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                       KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_VISIBLE],
                                                       false);
        mButtonPromptAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                     KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_VISIBLE],
                                                     false);
        mSearchParamsAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                     KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_INVISIBLE],
                                                     false);
        mFoundGamesAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                   KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_INVISIBLE],
                                                   false);

        mMessageText.SetText(KAC_NO_GAMES_FOUND_STRING_ID);

        // Two live buttons, no wrap (X360 `li r4, 2` / `li r5, 0`).
        mMessageButtons.SetupMenu(2, false);

        const s32 KI_NUM_YES_NO_BUTTONS =
            static_cast<s32>(sizeof(KAPC_YES_NO_BUTTON_STRING_ID) /
                             sizeof(KAPC_YES_NO_BUTTON_STRING_ID[0]));
        for (s32 li = 0; li < KI_NUM_YES_NO_BUTTONS; ++li)
        {
            mMessageButtons.SetText(li, KAPC_YES_NO_BUTTON_STRING_ID[li]);
        }

        meSubState = E_SUBSTATE_NO_GAMES_FOUND;
    }

}

// ============================================================================
// FOLDED FROM BrnOnlineCustomMatch_wJ_02.cpp (wave J) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
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


namespace BrnGui
{
    namespace
    {
// (fold: an identical definition of KI_CHANNEL_GUI_OUT was dropped here -- this TU defines it once, above)

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

// (fold: an identical definition of KAC_APT_TRANSITION_NAME was dropped here -- this TU defines it once, above)
// (fold: an identical definition of KI_ANIMATION_STATE_INVISIBLE was dropped here -- this TU defines it once, above)

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

// ============================================================================
// FOLDED FROM BrnOnlineCustomMatch_wJ_03.cpp (wave J) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ===================================================================================
// BrnGui::OnlineCustomMatch -- wave-J partfile 03: the found-games list group.
//   FillInTable         @0x8248BA90
//   HandleSearchResults @0x8248FD30  (DWARF cpp:915 assert)
//   ShowFoundGames      @0x8248E738
//
// RECONCILED against the grown component headers. The three declarations this partfile
// was parked on are all committed now, so none of the three bodies is blocked:
//
//   * GameSource/Gui/BrnGuiDemangledEventTypes.h:374 -- GuiEventNetworkCustomMatchResults
//     is the HEADERLESS 604-byte record (its own GetEventType() == 254, NO
//     CgsGui::GuiEvent<254> base) carrying the DWARF field names these bodies read:
//     maiNumPlayers / maiMaxNumPlayers / maiGameFlags / maiFoundGameIndex / maeGameMode /
//     maePreviousGameMode / miNumGames / maacGameNames, plus the two flag masks
//     KI_HAS_FRIENDS == 2 and KI_HAS_RIVALS == 4 (DWARF BrnGuiEventTypeDefs.h:3183/:3184),
//     which is why this file mints no local flag constants for them.
//     Headerless is the MEASURED shape, not a convenience: the producer
//     BrnNetworkModule::AddOutputGuiEvent<GuiEventNetworkCustomMatchResults> @0x82565E88
//     posts `AddEvent(queue, &event, 254, 0x25C == 604)` with the record size taken from
//     sizeof(TEvent), and Update @0x824ACD70 memcpy's all 604 bytes onto the member base,
//     so FillInTable's first index base (4*0x37FD == this+57332 == mSearchResults + 0)
//     really is the record's first byte. 244 + 10*36 == 604 closes it exactly.
//
//   * GameSource/Gui/Flow/Shared/Components/BrnTable.h:195/:200/:211 -- Table::SetText,
//     Table::SetIconState(s32, s32, u32) and Table::HighlightPrevious() are declared.
//     HighlightPrevious is Table's OWN vtable slot 14 (+0x38 of off_820747D4), a different
//     method from the inherited SelectableGroup::HighlightPrevious(bool) at slot 11
//     (+0x2C): the call at 0x8248E894 sets only r3, i.e. it takes NO argument.
//
// The console index bases FillInTable walks, all reached BY NAME below (documentation
// only -- never arithmetic on the LLP64 host): 0x4418 == mTable (+17432), 0x44BD ==
// mTable.miHighlightedIndex, 0xDFF0/0xDFF1 == miFirstRow/miLastRow, +65536-0x1F1C ==
// +57572 == mSearchResults.miNumGames, 0xE0E8 == the 36-byte name rows, and the word
// bases 4*0x37FD / 4*0x3807 / 4*0x3811 / 4*0x3825 / 4*0x382F == mSearchResults + 0 /
// +40 / +80 / +160 / +200. Both the miFirstRow read (`lbz` + `extsb`) and the
// miHighlightedIndex read are SIGNED bytes. No floats in either body, so there is no PPC
// NaN-polarity decision to make.
//
// LINK NOTE for the conductor (invisible to cl /c). CHECKED against the tree, not assumed
// -- of everything this group calls, only these have no DEFINITION anywhere:
//   BrnGui::Table::SetText            @0x824895F8  (declared BrnTable.h:195, body pending)
//   BrnGui::Table::SetIconState       @0x82489928  (declared BrnTable.h:200, body pending)
//   BrnGui::Table::HighlightPrevious  @0x824E6D70  (declared BrnTable.h:211, body pending)
//   BrnGui::TextField::SetLocalisedText @0x824E7A20 -- the ARRAY overload (declared
//       BrnGuiTextField.h:102; BrnGuiTextField.cpp carries only SetColour and operator=)
//   BrnGui::OnlineCustomMatch::ShowNoGamesFoundInGame @0x8248BE68 (declared in the class
//       header; body belongs to the FOREIGN ledger TU BrnAnimationComponent.h)
//   the class statics this group reads -- KAPC_ANIMATION_STATES, KAPC_GAME_MODE_STRING_IDS,
//       KAC_NO_PREVIOUS_GAME_MODE_STRING_ID, KAC_NUM_PLAYERS_STRING_ID,
//       KAC_NUM_GAMES_FOUND_STRING_ID, KAC_NUM_GAMES_FOUND_SINGULAR_STRING_ID -- which are
//       declared in the header with their definitions expected in the consolidated .cpp;
//       this partfile deliberately does NOT define them (a sibling would duplicate them).
// Everything else resolves today: Table::GetSelectable in BrnTable.cpp:38,
// Table::SetLocalisedText in BrnTable.cpp:60, Selectable::SetActive in BrnSelectable.cpp:25,
// GuiComponent::AddOutputAptViewState in CgsGuiComponent.cpp:40, CgsCore::SPrintf in
// CgsStringUtils.cpp:31, GuiCache::IsOnlineStartInProgress is a header inline
// (BrnGuiCache.h:499), and ShowNoGamesFound lands in BrnOnlineCustomMatch_wJ_01.cpp.
// ===================================================================================


namespace
{
    // ---- the found-games table geometry -------------------------------------------
    // Five rows: `cmpwi cr6, r11, 5` / `li r20, 5` @0x8248BAAC/0x8248BAB8, the same 5 in
    // ShowFoundGames (@0x8248E84C / 0x8248E8CC / 0x8248E900) and the same number the class
    // header carries as miTableNumRows and OnEnter passes to Table::Construct.
    const s32 KI_NUM_TABLE_ROWS = 5;

    // Column indices -- the `li r5, <n>` immediate of each Table::Set* call in FillInTable.
    // Seven columns in all, matching the header's miTableNumColumns == 7 and its
    // maTableRowComponentTypes[7] == { ICON, TEXT, TEXT, TEXT, TEXT, ICON, ICON }.
    const s32 KI_COLUMN_SCROLL_ICON  = 0;   // 0x8248BD1C  SetIconState
    const s32 KI_COLUMN_GAME_NAME    = 1;   // 0x8248BB28  SetText
    const s32 KI_COLUMN_GAME_MODE    = 2;   // 0x8248BB58  SetText
    const s32 KI_COLUMN_NUM_PLAYERS  = 3;   // 0x8248BC50  SetLocalisedText
    const s32 KI_COLUMN_PREV_MODE    = 4;   // 0x8248BBC4  SetText
    const s32 KI_COLUMN_FRIENDS_ICON = 5;   // 0x8248BC64  SetIconState
    const s32 KI_COLUMN_RIVALS_ICON  = 6;   // 0x8248BCA0  SetIconState

    // The half-open game-mode range KAPC_GAME_MODE_STRING_IDS is indexed over as
    // [mode - KI_GAME_MODE_FIRST]: `cmpwi cr6, r11, 0xA` / `cmpwi cr6, r11, 0x12`
    // @0x8248BBA0 / 0x8248BBA8 with the `addi r11, r11, -0xA` bias @0x8248BB78 and
    // 0x8248BBB0. FLAG: these are BrnGameState::GameStateModuleIO::EGameModeType values
    // (the DWARF type of maeGameMode / maePreviousGameMode); that enum has no committed
    // home, so the bounds are carried as measured constants rather than named enumerators.
    const s32 KI_GAME_MODE_FIRST = 0xA;    // 10
    const s32 KI_GAME_MODE_END   = 0x12;   // 18 (exclusive)

    // maIconStates slots (class header, @0x8205E9C8 == { "Top", "Middle", "Bottom",
    // "Friends", "Rivals", "Empty" }) -- the `li r6, <n>` immediates of the SetIconState
    // calls. KI_ICON_STATE_EMPTY is the same 5 the sibling partfile wJ_06 seeds the icon
    // columns with; keep one copy on merge.
    const s32 KI_ICON_STATE_TOP     = 0;   // 0x8248BCFC
    const s32 KI_ICON_STATE_MIDDLE  = 1;   // 0x8248BD08
    const s32 KI_ICON_STATE_BOTTOM  = 2;   // 0x8248BD14
    const s32 KI_ICON_STATE_FRIENDS = 3;   // 0x8248BC7C
    const s32 KI_ICON_STATE_RIVALS  = 4;   // 0x8248BCB8
    const s32 KI_ICON_STATE_EMPTY   = 5;   // 0x8248BC94 / 0x8248BCD0

    // The SPrintf scratch capacity and format. The 7 is the `li r4, 7` the call passes
    // (0x8248BBD8 / 0x8248BC04 here, 0x8248E7E8 in ShowFoundGames); FillInTable's two twin
    // buffers provably sit 7 bytes apart on the console frame (var_80 / var_79).
    const u32  KU_NUMBER_BUFFER_SIZE = 7;
    const char KAC_INTEGER_FORMAT[]  = "%d";   // rodata aD_16

    // The apt view the animation components are driven through, and the
    // KAPC_ANIMATION_STATES slots ShowFoundGames drives them to (@0x82F266B0 ==
    // { "Visible", "Invisible", "Refresh" }; the asm reaches the three slots as
    // off_82F266B0 / off_82F266B4 / off_82F266B8). Shared with the other wave-J partfiles
    // of this TU -- keep exactly one copy on merge.
    const char KAC_APT_TRANSITION_NAME[]    = "apt_Transition";
    const s32  KI_ANIMATION_STATE_VISIBLE   = 0;
    const s32  KI_ANIMATION_STATE_INVISIBLE = 1;
    const s32  KI_ANIMATION_STATE_REFRESH   = 2;
}

namespace BrnGui
{

    // ================================================================================
    //  FillInTable  @ 0x8248BA90
    //
    //  Repaint the found-games table from mSearchResults through the current scroll
    //  window: one row per visible result, then the scroll-position icon, then blank out
    //  whatever rows the window no longer covers.
    // ================================================================================
    void OnlineCustomMatch::FillInTable()
    {
        // The visible row count is the result count clamped to the table height.
        s32 liVisibleRows = mSearchResults.miNumGames;
        if (liVisibleRows >= KI_NUM_TABLE_ROWS)
        {
            liVisibleRows = KI_NUM_TABLE_ROWS;
        }

        for (s32 liRow = 0; liRow < liVisibleRows; ++liRow)
        {
            // The result this screen row shows. The X360 re-reads the miFirstRow byte for
            // every single use inside the iteration; nothing writes it here, so one read
            // per iteration is equivalent.
            const s32 liGame = miFirstRow + liRow;

            // Table::GetSelectable hands back the TableRow; slot 0 of its component vtable
            // is Selectable::SetActive (ocm_rodata.txt:169), called with 1.
            mTable.GetSelectable(liRow)->SetActive(true);

            // Column 1 -- the host's game name, straight out of the 36-byte name row.
            mTable.SetText(liRow, KI_COLUMN_GAME_NAME, mSearchResults.maacGameNames[liGame]);

            // Column 2 -- the current game mode. NOTE there is deliberately NO range check
            // here, unlike column 4 below: the X360 indexes KAPC_GAME_MODE_STRING_IDS with
            // (mode - 10) unguarded (0x8248BB6C..0x8248BB80).
            mTable.SetText(liRow, KI_COLUMN_GAME_MODE,
                           KAPC_GAME_MODE_STRING_IDS[mSearchResults.maeGameMode[liGame]
                                                     - KI_GAME_MODE_FIRST]);

            // Column 4 -- the mode the host played PREVIOUSLY, this one range-checked;
            // anything outside 10..17 renders the "no previous mode" string.
            const s32   liPreviousMode = mSearchResults.maePreviousGameMode[liGame];
            const char* lpacPreviousMode;
            if (liPreviousMode < KI_GAME_MODE_FIRST || liPreviousMode >= KI_GAME_MODE_END)
            {
                lpacPreviousMode = KAC_NO_PREVIOUS_GAME_MODE_STRING_ID;
            }
            else
            {
                lpacPreviousMode = KAPC_GAME_MODE_STRING_IDS[liPreviousMode - KI_GAME_MODE_FIRST];
            }
            mTable.SetText(liRow, KI_COLUMN_PREV_MODE, lpacPreviousMode);

            // Column 3 -- "<current>/<max> players", both counts handed to the language
            // manager as INTEGER parameters of one localised string.
            char lacCurrentPlayers[KU_NUMBER_BUFFER_SIZE];
            char lacMaxPlayers[KU_NUMBER_BUFFER_SIZE];
            CgsCore::SPrintf(lacCurrentPlayers, KU_NUMBER_BUFFER_SIZE, KAC_INTEGER_FORMAT,
                             mSearchResults.maiNumPlayers[liGame]);
            CgsCore::SPrintf(lacMaxPlayers, KU_NUMBER_BUFFER_SIZE, KAC_INTEGER_FORMAT,
                             mSearchResults.maiMaxNumPlayers[liGame]);

            const char* lapacParams[2];
            CgsLanguage::LanguageManager::ParameterFormatType laeParamFormats[2];
            lapacParams[0]     = lacCurrentPlayers;
            laeParamFormats[0] = CgsLanguage::LanguageManager::E_FORMAT_INTEGER;
            lapacParams[1]     = lacMaxPlayers;
            laeParamFormats[1] = CgsLanguage::LanguageManager::E_FORMAT_INTEGER;
            mTable.SetLocalisedText(liRow, KI_COLUMN_NUM_PLAYERS, KAC_NUM_PLAYERS_STRING_ID,
                                    CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
                                    2, lapacParams, laeParamFormats);

            // Columns 5 and 6 -- the friends / rivals badges, or the empty icon. The X360
            // re-loads the same flags word for each of the two tests, and masks it with the
            // record's own KI_HAS_FRIENDS / KI_HAS_RIVALS bits (`rlwinm r11, r11, 0,30,30`
            // == &2 at 0x8248BC88, `rlwinm r11, r11, 0,29,29` == &4 at 0x8248BCC4).
            const s32 liFlags = mSearchResults.maiGameFlags[liGame];
            mTable.SetIconState(liRow, KI_COLUMN_FRIENDS_ICON,
                                ((liFlags & GuiEventNetworkCustomMatchResults::KI_HAS_FRIENDS)
                                     == GuiEventNetworkCustomMatchResults::KI_HAS_FRIENDS)
                                    ? KI_ICON_STATE_FRIENDS : KI_ICON_STATE_EMPTY);
            mTable.SetIconState(liRow, KI_COLUMN_RIVALS_ICON,
                                ((liFlags & GuiEventNetworkCustomMatchResults::KI_HAS_RIVALS)
                                     == GuiEventNetworkCustomMatchResults::KI_HAS_RIVALS)
                                    ? KI_ICON_STATE_RIVALS : KI_ICON_STATE_EMPTY);
        }

        // ---- column 0: the scroll-position badge on the highlighted row ---------------
        // The absolute index of the highlighted result inside the whole list.
        const s32 liHighlighted  = mTable.miHighlightedIndex;
        const s32 liAbsoluteRow  = miFirstRow + liHighlighted;

        s32 liScrollIconState;
        if (liAbsoluteRow == 0)
        {
            liScrollIconState = KI_ICON_STATE_TOP;
        }
        else if (liAbsoluteRow == mSearchResults.miNumGames)
        {
            // MEASURED at 0x8248BD04..0x8248BD14: the console really compares against
            // miNumGames itself, not miNumGames - 1 (`lwz r10, 0(r19)` / `cmpw r11, r10`).
            // Kept EXACTLY as shipped. INFERENCE, not measurement: given the scroll-window
            // invariants ShowFoundGames maintains (miFirstRow <= miNumGames - 5 and the
            // highlight <= 4), the sum tops out at miNumGames - 1, so this "Middle" arm
            // looks unreachable and every row below the first draws "Bottom". That reading
            // is not proven here -- only the comparison is.
            liScrollIconState = KI_ICON_STATE_MIDDLE;
        }
        else
        {
            liScrollIconState = KI_ICON_STATE_BOTTOM;
        }
        mTable.SetIconState(liHighlighted, KI_COLUMN_SCROLL_ICON, liScrollIconState);

        // ---- blank the rows the window no longer covers -------------------------------
        // miLastRow is read once (0x8248BD30) and the loop runs to the table height.
        for (s32 liRow = miLastRow; liRow < KI_NUM_TABLE_ROWS; ++liRow)
        {
            mTable.GetSelectable(liRow)->SetActive(false);
        }
    }

}

namespace BrnGui
{

    // ================================================================================
    //  HandleSearchResults  @ 0x8248FD30
    //
    //  Update pumps this once a custom-match search result record has been latched into
    //  mSearchResults: pick the screen the result implies, then drop the "results are
    //  waiting" latch.
    // ================================================================================
    void OnlineCustomMatch::HandleSearchResults()
    {
        // cpp:915. Non-fatal (the X360 falls straight through into the dispatch below --
        // `bl EndAssert` returns into the meSubState load at 0x8248FDCC).
        CGS_ASSERT(mbHasRecievedSearchResults, "HANDLING NON-RECIEVED SEARCH RESULTS\n");

        // Already joining a game: the incoming results are stale, so nothing is shown and
        // only the latch is dropped (0x8248FDD0 `cmpwi 5` / `beq` straight to the store).
        if (meSubState != E_SUBSTATE_JOINING)
        {
            if (mSearchResults.miNumGames != 0)
            {
                ShowFoundGames();
            }
            else if (mpGuiCache->IsOnlineStartInProgress())
            {
                // Searching from inside a game session -- the "no games" page keeps the
                // in-game button set.
                ShowNoGamesFoundInGame();
            }
            else
            {
                ShowNoGamesFound();
            }
        }

        // The X360 stores the 0 on every one of the four exits (0x8248FE04 / 0x8248FE14 /
        // 0x8248FE24), i.e. the latch clears unconditionally; the goto-flattened pseudocode
        // is de-gotoed here. Note it does NOT null-check mpGuiCache on the else path --
        // reproduced as-is.
        mbHasRecievedSearchResults = false;
    }

}

namespace BrnGui
{

    // ================================================================================
    //  ShowFoundGames  @ 0x8248E738
    //
    //  Swap the screen over to the found-games list: hide the message/parameter panels,
    //  caption the list with "<n> games found", clamp the 5-row scroll window onto the
    //  new result count, then repaint the rows.
    // ================================================================================
    void OnlineCustomMatch::ShowFoundGames()
    {
        // Hide the message text, its buttons and the search-parameter rows; show the
        // button prompts. lbImmediate is 0 on all five calls (`li r6, 0`).
        mMessageAnimation.AddOutputAptViewState(
            KAC_APT_TRANSITION_NAME, KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_INVISIBLE], false);
        mMessageButtonsAnimation.AddOutputAptViewState(
            KAC_APT_TRANSITION_NAME, KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_INVISIBLE], false);
        mSearchParamsAnimation.AddOutputAptViewState(
            KAC_APT_TRANSITION_NAME, KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_INVISIBLE], false);
        mButtonPromptAnimation.AddOutputAptViewState(
            KAC_APT_TRANSITION_NAME, KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_VISIBLE], false);

        // The list panel's transition depends on whether it is ALREADY up: a re-search from
        // the list is a "Refresh", the first entry is a "Visible". meSubState is sampled
        // here (0x8248E7B0) and only set to SELECTING_GAME further down (0x8248E844) --
        // keep that order.
        const char* lpacFoundGamesState =
            (meSubState == E_SUBSTATE_SELECTING_GAME)
                ? KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_REFRESH]
                : KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_VISIBLE];
        mFoundGamesAnimation.AddOutputAptViewState(
            KAC_APT_TRANSITION_NAME, lpacFoundGamesState, false);

        // "<n> GAMES FOUND", with a separate singular string id (note the shipped
        // "SEACH" typo in it). The count goes through the language manager as an INTEGER
        // parameter rather than being pasted in, so the caption localises.
        // The capacity the call passes is 7 (`li r4, 7` at 0x8248E7E8), which is what the
        // buffer is sized to here. (The console's stack gap for this local runs 0x60..0xA0
        // of the 0xA0 frame and Hex-Rays typed it char[64]; only the 7 is a measurement of
        // the CALL, so the declared size follows the capacity, as in FillInTable where the
        // two twin buffers provably sit 7 bytes apart.)
        char lacNumGames[KU_NUMBER_BUFFER_SIZE];
        CgsCore::SPrintf(lacNumGames, KU_NUMBER_BUFFER_SIZE, KAC_INTEGER_FORMAT,
                         mSearchResults.miNumGames);

        const char* lpacCaptionID = KAC_NUM_GAMES_FOUND_STRING_ID;
        if (mSearchResults.miNumGames == 1)
        {
            lpacCaptionID = KAC_NUM_GAMES_FOUND_SINGULAR_STRING_ID;
        }

        // sub_824E7A20 == TextField::SetLocalisedText, the ARRAY overload: r5 = 9
        // (E_FORMAT_ID_LOOKUP), r6 = 1 parameter, r7 = the parameter-text array,
        // r8 = the parameter-format array.
        const char* lapacParams[1];
        CgsLanguage::LanguageManager::ParameterFormatType laeParamFormats[1];
        lapacParams[0]     = lacNumGames;
        laeParamFormats[0] = CgsLanguage::LanguageManager::E_FORMAT_INTEGER;
        mNumGamesFoundText.SetLocalisedText(lpacCaptionID,
                                            CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
                                            1, lapacParams, laeParamFormats);

        const s32 liNumGames = mSearchResults.miNumGames;
        meSubState = E_SUBSTATE_SELECTING_GAME;

        // ---- fit the 5-row scroll window onto the new result count --------------------
        if (liNumGames > 0)
        {
            if (liNumGames >= KI_NUM_TABLE_ROWS)
            {
                // More results than fit. If the window was not full yet, open it to its
                // full height and repaint straight away -- this arm deliberately SKIPS the
                // first-row clamp below (the X360 tail-calls FillInTable at 0x8248E8E0).
                if (miLastRow < KI_NUM_TABLE_ROWS)
                {
                    miLastRow = static_cast<s8>(KI_NUM_TABLE_ROWS);
                    FillInTable();
                    return;
                }

                // The window was full: pull it back if it now hangs off the end of the
                // (possibly shorter) result list.
                if (liNumGames - miFirstRow < KI_NUM_TABLE_ROWS)
                {
                    miFirstRow = static_cast<s8>(liNumGames - KI_NUM_TABLE_ROWS);
                }
            }
            else
            {
                // Everything fits: park the window at the top and shrink it to the result
                // count. miHighlightedIndex is sampled BEFORE the two stores (0x8248E858).
                const s32 liHighlighted = mTable.miHighlightedIndex;

                miFirstRow = 0;
                miLastRow  = static_cast<s8>(liNumGames);

                // Walk the highlight back up if it now sits past the last live row. The
                // X360 guards the loop with the sampled index and then re-reads the byte
                // (and the count) on each turn (0x8248E898..0x8248E8A8).
                if (liHighlighted >= liNumGames)
                {
                    do
                    {
                        mTable.HighlightPrevious();
                    }
                    while (mTable.miHighlightedIndex >= mSearchResults.miNumGames);
                }
            }
        }

        FillInTable();
    }

}

// ============================================================================
// FOLDED FROM BrnOnlineCustomMatch_wJ_04.cpp (wave J) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
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


namespace BrnGui
{
    namespace
    {
// (fold: an identical definition of KI_CHANNEL_GUI_OUT was dropped here -- this TU defines it once, above)

// (fold: an identical definition of KI_ACTION_GUI_UP was dropped here -- this TU defines it once, above)
// (fold: an identical definition of KI_ACTION_GUI_DOWN was dropped here -- this TU defines it once, above)
// (fold: an identical definition of KI_ACTION_GUI_SELECT was dropped here -- this TU defines it once, above)
// (fold: an identical definition of KI_ACTION_GUI_CANCEL was dropped here -- this TU defines it once, above)

        // The message-button row this page treats as the affirmative answer. The X360 tests
        // the highlighted index against 0, and the sibling in-game handler's assert text
        // names the row E_OK_BUTTON_OK. FLAG: the enum itself is not in the recovered DWARF
        // slice, so the value is carried as a named constant rather than an invented enum.
        const s32 KI_MESSAGE_BUTTON_AFFIRMATIVE = 0;

// (fold: an identical definition of ControllerButtonPayload was dropped here -- this TU defines it once, above)
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

// ============================================================================
// FOLDED FROM BrnOnlineCustomMatch_wJ_05.cpp (wave J) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ===================================================================================
// BrnGui::OnlineCustomMatch -- wave-J partfile 05.
//   HandleInGameFailedEvent  @0x82497F50  (assert cpp:1032)
//   CheckForCompletedLoads   @0x824A34A0  (assert cpp:432)
//   HandleGuiCacheEvent      @0x82497C50
//
// All three bodies compile against the grown headers: BrnTable.h now declares
// Table::HasData() (h:190) and Table::SetupTable(TableDataSet*, bool, bool) (h:185), and
// BrnGuiCache.h now carries `friend struct OnlineCustomMatch;` (h:803) over the two
// mbOnlineMatch* bytes the ticker-line choice reads.
//
// ===================================================================================
// WHAT HandleInGameFailedEvent DOES  (@0x82497F50)
// -----------------------------------------------
// The custom-match screen observes network event id 51 ("in-game start failed"). When it
// arrives the screen tells the overlay system that the "entering game" wait overlay is
// finished (so the spinner comes down), then rewinds itself to its initial screen.
//
// MEASURED FROM THE ASM (rung 1), not from Hex-Rays
// ------------------------------------------------
//  * The assert message is a COPY-PASTE in the ORIGINAL: it names
//    "OnlineSelectRoute::HandleInGameEvent" while living in BrnOnlineCustomMatch.cpp
//    (line 1032 == `li r5, 0x408` at 0x82497FCC). Kept verbatim -- it is what the X360
//    image carries. The assert is non-fatal (BeginAssert / FireAssert / EndAssert with no
//    early-out at 0x82497F68..0x82497FDC), so a null event falls straight into the body;
//    nothing below dereferences it, which is why that is harmless.
//  * The posted record is built on the stack at 0x82497FF0..0x82498020 as
//        +0x00  8      payload byte count   (`li r11, 8`   / stw var_40)
//        +0x04  188    event id             (`li r11, 0xBC`/ stw var_3C)
//        +0x08  16     payload offset       (`li r11, 0x10`/ stw var_38)
//        +0x10  the 8-byte CgsID            (`ld` from the Construct out-param, `std`)
//    and published with AddEvent(queue, record, 40 /*channel*/, 0x18 /*24 bytes*/).
//    NOTE the payload-offset word is 16, NOT the usual 12: the payload is an 8-byte
//    aligned CgsID, so it starts at +0x10 and the record is 24 bytes. On the host the
//    identical alignment falls out of `CgsID mOverlayId` following the 12-byte
//    CgsGui::GuiEvent<188> base -- both numbers below are host expressions (offsetof /
//    sizeof), never the console literals, and are pinned by static_assert.
//  * The queue is `mpStateInterface + 12` on the console (`lwz r11,0x1C(r28)` +
//    `addi r3,r11,0xC`) == &StateInterface::mOutEventQueue == GetOutputEventQueue().
//  * No float compares anywhere in this body, so there is no PPC NaN-polarity decision.
//
// LINK NOTES (`cl /c` cannot see any of these -- do not go hunting)
// -----------------------------------------------------------------
//  * BrnGui::OnlineCustomMatch::ShowInitialScreen -- declared BrnOnlineCustomMatch.h:91,
//    body owned by the wave-J group-1 partfile; no body in the tree yet.
//  * CgsIDCompress @0x82815A20 -- declared CgsID.h:29 and DEFINED in CgsID.cpp; NOT a gap.
//  * BrnGui::GuiOverlayWaitFinishRequest::Construct -- inline-defined in
//    BrnGuiEventTypeDefs.h; NOT a gap.
// ===================================================================================


namespace BrnGui
{
    namespace
    {
// (fold: an identical definition of KI_CHANNEL_GUI_OUT was dropped here -- this TU defines it once, above)

        // The overlay this screen raised when it began joining a game and now dismisses
        // (X360 rodata aCnonlentgame, loaded at 0x82497FE0/0x82497FE8). The '1' arm of
        // HandleControllerInputSelectGame raises the very same overlay id, so that
        // partfile carries its own copy -- keep exactly one on merge.
        const char KAC_ENTER_GAME_OVERLAY_ID[] = "CNOnlEntGame";

        // ---- out-queue wire record -----------------------------------------------------
        // The id-188 "this wait overlay has finished" request. Its payload is the homed
        // BrnGui::GuiOverlayWaitFinishRequest -- one compressed overlay id, 8-aligned, so
        // it seats at +0x10 and the record totals the console's 24 bytes.
        struct GuiOverlayWaitFinishRequestWire : public CgsGui::GuiEvent<188>
        {
            GuiOverlayWaitFinishRequest mRequest;   // +0x10

            explicit GuiOverlayWaitFinishRequestWire(const char* lpcOverlayName)
                : CgsGui::GuiEvent<188>(
                      static_cast<u32>(sizeof(GuiOverlayWaitFinishRequest)),
                      static_cast<u32>(offsetof(GuiOverlayWaitFinishRequestWire, mRequest)))
            {
                mRequest.Construct(lpcOverlayName);
            }
        };

        // Layout pins: the console publishes payload-size 8, payload-offset 16, record 24.
        typedef char KAC_ASSERT_WAIT_FINISH_PAYLOAD_SIZE[
            sizeof(GuiOverlayWaitFinishRequest) == 8 ? 1 : -1];
        typedef char KAC_ASSERT_WAIT_FINISH_WIRE_SIZE[
            sizeof(GuiOverlayWaitFinishRequestWire) == 24 ? 1 : -1];
        typedef char KAC_ASSERT_WAIT_FINISH_PAYLOAD_OFFSET[
            offsetof(GuiOverlayWaitFinishRequestWire, mRequest) == 16 ? 1 : -1];
    }

    // ------------------------------------------------- HandleInGameFailedEvent @0x82497F50
    void OnlineCustomMatch::HandleInGameFailedEvent(const CgsModule::Event* lpEvent)
    {
        // Non-fatal (BeginAssert / FireAssert / EndAssert, no early-out). The message text
        // is the ORIGINAL's copy-paste from the online-select-route screen -- kept verbatim.
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event sent to OnlineSelectRoute::HandleInGameEvent");   // cpp:1032

        // Take down the "entering game" wait overlay.
        const GuiOverlayWaitFinishRequestWire lRequest(KAC_ENTER_GAME_OVERLAY_ID);

        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequest), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lRequest)));   // X360 record size 24

        // ...and put the screen back to where it started.
        ShowInitialScreen();
    }
}

// WHAT THE FUNCTION DOES  (@0x824A34A0)
// -------------------------------------
// The screen's two-step load pump, driven from Update(). Step 0 waits for the custom-match
// screen's static resource list to finish loading, then starts its apt movie. Step 1 waits
// for every apt component the movie declares to finish initialising, then binds the
// found-games table to its row-data set, drops the now-satisfied expected-component list,
// and hands over to the initial screen. Every other sub-state does nothing.
//
// MEASURED FROM THE ASM (rung 1), not from Hex-Rays
// ------------------------------------------------
//  * The leading assert is non-fatal (BeginAssert / FireAssert / EndAssert, no early-out,
//    0x824A34C0..0x824A34DC): "mpGuiCache", cpp:432 (`li r5, 0x1B0`). The sub-state-0 arm
//    then dereferences mpGuiCache WITHOUT re-checking it, while the sub-state-1 arm DOES
//    re-check it (`cmplwi cr6, r3, 0 / beq` @0x824A354C) -- both reproduced as written.
//  * ⭐ THE DOSSIER PSEUDOCODE DROPS TWO ARGUMENTS. Hex-Rays prints
//    `GuiCache::EnsureResourcesAreLoaded(*(v1 + 17424))`, but the asm @0x824A34EC..0x824A34FC
//    sets up THREE registers before the `bl`:
//        lis   r11, unk_8205E77C@ha        ; &maResourceTuplesToLoad
//        lwz   r3,  0x4410(r31)            ; this->mpGuiCache
//        li    r5,  1                      ; the tuple count
//        addi  r4,  r11, unk_8205E77C@l
//    which is exactly the committed two-arg declaration (BrnGuiCache.h:212). The count is
//    written as miNumResourcesToLoad (== 1, BrnScreenStatesDataLinkStubs.cpp:167), never
//    as the console literal.
//  * The return value is consumed as a BYTE (`clrlwi r11, r3, 24` @0x824A3500 and again at
//    0x824A355C) -- both callees really do return bool, as declared.
//  * PlayAptMovie's movie name is a POINTER loaded from the screen-flow's per-screen
//    name table (`lwz r4, (off_82F27B9C - 0x82F278E0)(r11)` @0x824A351C -> "ON_CUSTM"),
//    with level 3 (`li r5, 3`); the table is not this TU's static, so the literal is
//    file-local below.
//  * `lwz r11, 0x76D0(r31)` @0x824A3568 is this+30416 == mTable(17432) + 0x32B8 ==
//    Table::mpData (SelectableGroup base + maRows[16] * 0x308 = 0x238 + 0x3080 = 0x32B8),
//    i.e. the INLINED Table::HasData(); the branch is taken when it is non-null, so the
//    guard is !mTable.HasData().
//  * SetupTable's second argument is `addis r4, r31, 1 / addi r4, r4, -0x655C` ==
//    this + 0x10000 - 0x655C == this + 39588 == &mTableData; r5 and r6 are both `li 0`.
//    Reached by member name here -- no console displacement survives in the code.
//  * ClearExpectedAptComponentList and AreAllAptComponentsInitialised both take `li r4, 0`
//    == E_GUIFLOW_SCREEN (BrnGuiEventTypeDefs.h:74).
//  * The console's flat `if (substate) { if (substate == 1) ... } else ...` is the compiler's
//    rendering of the source switch; written back as a switch over ESubState.
//  * No float compares anywhere in this body, so there is no PPC NaN-polarity decision.
// =============================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineCustomMatch.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface::PlayAptMovie
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache

namespace BrnGui
{
    namespace
    {
        // The custom-match screen's apt movie. The X360 does not hold this string as a
        // static of this class: PlayAptMovie is handed the pointer parked at 0x82F27B9C,
        // one slot of the screen-flow's per-screen movie-name table (0x82F27B90..).
        const char KAC_APT_MOVIE_NAME[] = "ON_CUSTM";

        // The apt movie's layer/level argument (X360 `li r5, 3` @0x824A3514). FLAG: the
        // parameter is `s32 liLevelNum` (CgsGuiStateInterface.h:136); nothing in the image
        // names the 3, so it stays a bare measured constant.
        const s32 KI_APT_MOVIE_LEVEL = 3;
    }

    // ------------------------------------------------- CheckForCompletedLoads @0x824A34A0
    void OnlineCustomMatch::CheckForCompletedLoads()
    {
        // Non-fatal (BeginAssert / FireAssert / EndAssert, no early-out).
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:432

        switch (meSubState)
        {
        case E_SUBSTATE_LOADING_SCREEN:
            // Nothing can be shown until the screen's own resource list is resident; the
            // moment it is, kick the apt movie off and move to waiting on its components.
            if (mpGuiCache->EnsureResourcesAreLoaded(maResourceTuplesToLoad,
                                                     static_cast<u32>(miNumResourcesToLoad)))
            {
                mpStateInterface->PlayAptMovie(KAC_APT_MOVIE_NAME, KI_APT_MOVIE_LEVEL);
                meSubState = E_SUBSTATE_LOADING_COMPONENTS;
            }
            break;

        case E_SUBSTATE_LOADING_COMPONENTS:
            // Wait for every expected apt component; bind the table exactly once (the
            // HasData test is what makes this a one-shot), then start the screen proper.
            if (mpGuiCache != 0 &&
                mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN) &&
                !mTable.HasData())
            {
                mTable.SetupTable(&mTableData, false, false);
                mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
                ShowInitialScreen();
            }
            break;

        default:
            break;
        }
    }
}

// WHAT THE FUNCTION DOES  (@0x82497C50)
// -------------------------------------
// The one-shot arrival of the GUI cache pointer. On the FIRST cache event (and only the
// first -- a second one returns immediately because mpGuiCache is already latched) the
// screen latches the pointer, publishes the online ticker line that matches how the match
// was entered, and then either registers every apt component this page must wait on, or --
// if the profile is not allowed to play multiplayer at all -- backs straight out, lifting
// the network suspension first if this screen owns the pending online start.
//
// MEASURED FROM THE ASM (rung 1), not from Hex-Rays
// ------------------------------------------------
//  * The in-queue hands handlers the HEADER-STRIPPED payload, so the incoming cache
//    pointer is the payload's first word (`lwz r11, 0(r25)` @0x82497C6C, re-read as
//    `lwz r30, 0(r25)` @0x82497CFC). Same file-local view the wave-I/H twins carry.
//  * Both asserts are non-fatal (BeginAssert / FireAssert / EndAssert, no early-out):
//    "Invalid cache in HandleGuiCacheEvent::Update" at cpp:955 (`li r5, 0x3BB`) -- the
//    "::Update" is a copy-paste in the ORIGINAL, kept verbatim -- and a second bare
//    "mpGuiCache" at cpp:1340 (`li r5, 0x53C`) after the ticker post.
//  * TICKER PAYLOAD SEEDS, measured payload-relative at 0x82497D08..0x82497D20:
//        +0x810 = 0   mi8NumStrings
//        +0x811 = 1   maFlags[0]   <-- this producer's distinguishing seed
//        +0x812 = 0   maFlags[1]
//        +0x813 = 1   maFlags[2]
//        +0x814 = 0   maFlags[3]
//    with maiStringTypes (+0x00..+0x0F) zeroed by two `std` @0x82497D30/D34 and the
//    0x800-byte string block zeroed by the memset @0x82497D28. A whole-struct memset plus
//    the two flag stores is identical. (Seeds {1,0,1,0}; BrnGui::CarSelectVehicle::SetTicker
//    seeds {0,0,1,0}, so flag 0 is what distinguishes the two ticker kinds.)
//  * The console builds the payload in a stack scratch and memcpy's it into the record
//    payload (@0x82497D7C..D88); built directly in the record here -- the same bytes on
//    the wire, and the precedent BrnCarSelectVehicle_Input.cpp / wI_09 both do this.
//  * The ticker record is { 0x818, 537, 12 } + the 0x818-byte payload, channel 40, 0x824
//    bytes (@0x82497D8C..DB8). All three numbers are host `sizeof` expressions below.
//  * The nine name registrations are `sub_824F87C0(cache, 0, component + 4)`. +4 is
//    CgsGui::GuiComponent::macName, i.e. the component's GetName(); sub_824F87C0 is the
//    name-taking entry of GuiCache::AppendExpectedAptComponent (decl BrnGuiCache.h:233).
//    Order measured from the `addi r5, r31, <disp>` chain @0x82497E74..0x82497ED0:
//        0x41C4 = 16836 = mMessageText            + 4
//        0x42EC = 17132 = mNumGamesFoundText      + 4
//        0x0040 =    64 = mMessageAnimation       + 4
//        0x00CC =   204 = mMessageButtonsAnimation+ 4
//        0x0158 =   344 = mSearchParamsAnimation  + 4
//        0x01E4 =   484 = mFoundGamesAnimation    + 4
//        0x0270 =   624 = mButtonPromptAnimation  + 4
//    then the two loops (@0x82497EE8 stride 0x128 base 0x76DC == maTextFields[i].macName,
//    bound 0x14 == 20; @0x82497F1C stride 0x94 base 0x8DFC == maIcons[j].macName, bound
//    0xF == 15). NO CONSOLE LITERAL SURVIVES BELOW: every one of those displacements is
//    reached by member name and the numbers live only in this comment.
//  * The loop counters are held as bytes (`addi`/`extsb` @0x82497F00/F04 and F34/F38) --
//    a console codegen detail, not a behaviour; written as ordinary s32 loops.
//  * The suspension event is posted onto the out-queue DIRECTLY rather than through
//    CgsGui::StateInterface::OutputGuiEvent, whose committed body passes the event id (45)
//    to AddEvent as the channel where the X360 passes 40. Same documented accommodation
//    BrnOnlineGameOptions_wI_09.cpp makes.
//  * No float compares anywhere in this body, so there is no PPC NaN-polarity decision.
// =============================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineCustomMatch.h"
#include <cstring>                                                       // memset / strncpy
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"       // GuiComponent::GetName
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface / GuiEventNetworkSuspension
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache (friend)

namespace BrnGui
{
    namespace
    {
        // KI_CHANNEL_GUI_OUT (== 40, X360 `li r5, 0x28`) is already declared in this
        // partfile's HandleInGameFailedEvent section above -- one copy per TU.

        // The ticker string's format/kind selector (X360 `li r5, 2` @0x82497D70) -- the same
        // word BrnGui::CarSelectVehicle::SetTicker and OnlineGameOptions pass to AddString.
        const s32 KI_TICKER_STRING_TYPE = 2;

        // ---- the three ticker lines (rodata literals @0x82497D44/D5C/D68) ---------------
        const char KAC_RANKED_TICKER_TEXT[]   = "ONLINE_RANKED_TICKER_TEXT";
        const char KAC_FREEBURN_TICKER_TEXT[] = "ONLINE_FREEBURN_TICKER_TEXT";
        const char KAC_UNRANKED_TICKER_TEXT[] = "ONLINE_UNRANKED_TICKER_TEXT";

        // ---- the two state-machine events this handler can send -------------------------
        const char KAC_STATE_EVENT_GO_BACK[]      = "GO_BACK";
        const char KAC_STATE_EVENT_GO_BACK_EASY[] = "GO_BACK_EASY";

        // ---- in-queue payload view -----------------------------------------------------
        // The DWARF types the parameter const GuiEventCache*, whose home header is one of
        // the mutually-exclusive event-type-def pair, so this is the same file-local view
        // the wave-H/I twins (BrnOnlineGameRoomPlayerInfo_wH_18.cpp,
        // BrnOnlineGameOptions_wI_09.cpp) carry.
        struct GuiEventCachePayload : public CgsModule::Event
        {
            GuiCache* mpGuiCache;   // +0x00
        };

        // ---- out-queue wire record -----------------------------------------------------
        // The custom ticker message payload (0x818 bytes). Layout recovered store-for-store
        // from BrnGui::GuiEventTickerCustomMessage::AddString @0x823A6940, whose asserts bake
        // "GameSource/Gui/BrnGuiEventTypeDefs.h" lines 390/391/392:
        //   +0x000  s32  maiStringTypes[4]
        //   +0x010  char maacStrings[4][512]
        //   +0x810  s8   mi8NumStrings          (`lbz`/`extsb`, bounded < 4)
        //   +0x811..+0x814  four flag bytes
        // Kept TU-LOCAL rather than promoted: the homed twin in BrnGuiDemangledEventTypes.h:182
        // is an opaque `GuiEvent<537> + u8 maPayload[2060]` whose 2072-byte total treats the
        // payload size as the whole record, so it cannot carry these fields; a second
        // definition of the real name would be a live ODR fork. Identical to the view
        // BrnCarSelectVehicle_Input.cpp and BrnOnlineGameOptions_wI_09.cpp carry.
        struct GuiTickerCustomMessagePayload
        {
            static const s32 KI_MAX_NUM_STRINGS   = 4;     // AddString's bound (h:391)
            static const s32 KI_MAX_STRING_LENGTH = 512;   // AddString's strncpy count

            s32  maiStringTypes[KI_MAX_NUM_STRINGS];                       // +0x000
            char maacStrings[KI_MAX_NUM_STRINGS][KI_MAX_STRING_LENGTH];    // +0x010
            s8   mi8NumStrings;                                            // +0x810
            // FLAG: four flag bytes at +0x811..+0x814 whose roles are not recovered.
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

        // Layout pins: the console posts 0x824 bytes with payload-size word 0x818 and
        // payload-offset word 12.
        typedef char KAC_ASSERT_TICKER_PAYLOAD_SIZE[
            sizeof(GuiTickerCustomMessagePayload) == 0x818 ? 1 : -1];
        typedef char KAC_ASSERT_TICKER_WIRE_SIZE[
            sizeof(GuiTickerCustomMessageWire) == 0x824 ? 1 : -1];
    }

    // ---------------------------------------------------- HandleGuiCacheEvent @0x82497C50
    void OnlineCustomMatch::HandleGuiCacheEvent(const CgsModule::Event* lpEvent)
    {
        const GuiEventCachePayload* lpCacheEvent =
            reinterpret_cast<const GuiEventCachePayload*>(lpEvent);

        // Non-fatal (BeginAssert / FireAssert / EndAssert, no early-out) -- the X360 falls
        // straight through, so a null cache would be latched as-is. "::Update" is the
        // ORIGINAL's copy-paste, kept verbatim.
        CGS_ASSERT(lpCacheEvent->mpGuiCache != 0,
                   "Invalid cache in HandleGuiCacheEvent::Update");   // cpp:955

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

        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:1340

        // ---- either arm the page, or back straight out --------------------------------
        if (mpGuiCache->IsMultiplayerAllowed())
        {
            // Tell the cache which apt components this page has to wait on before it can
            // report the screen flow ready. The two composite components have their own
            // helper; everything else is registered by name.
            mSearchParms.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache, true);
            mMessageButtons.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);

            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mMessageText.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mNumGamesFoundText.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mMessageAnimation.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mMessageButtonsAnimation.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mSearchParamsAnimation.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mFoundGamesAnimation.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mButtonPromptAnimation.GetName());

            // ...then every cell component of the found-games table.
            for (s32 liTextField = 0; liTextField < KI_NUM_ONLINE_FOUND_GAMES_TEXT_FIELDS;
                 ++liTextField)
            {
                mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                       maTextFields[liTextField].GetName());
            }

            for (s32 liIcon = 0; liIcon < KI_NUM_ONLINE_FOUND_GAMES_ICONS; ++liIcon)
            {
                mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                       maIcons[liIcon].GetName());
            }
        }
        else if (mpGuiCache->IsOnlineStartPending())        // GuiCache +0x4B53
        {
            // No multiplayer privilege, and this screen owns the pending online start:
            // lift the network suspension it armed, then back out on the quiet path.
            CgsGui::GuiEventNetworkSuspension lNetworkSuspension(false);
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lNetworkSuspension),
                KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lNetworkSuspension)));   // X360 record size 16

            mpGuiCache->SetOnlineStartPending(false);
            SendStateEvent(KAC_STATE_EVENT_GO_BACK_EASY);
        }
        else
        {
            SendStateEvent(KAC_STATE_EVENT_GO_BACK);
        }
    }
}

// ============================================================================
// FOLDED FROM BrnOnlineCustomMatch_wJ_06.cpp (wave J) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// wave-J partfile (group 6)
//
// WHAT IS AND IS NOT HERE
// -----------------------
// This partfile was assigned exactly one function -- BrnGui::OnlineCustomMatch::OnEnter
// @0x82496C10 -- and it ships below. The three shared-header changes it was parked on have
// all landed: BrnTable.h now declares TableDataSet::Construct() / AddRowData(TableRowDataSet*)
// with the row array typed TableRowDataSet* (the old u32 was a live x64 pointer truncation)
// and Table::Construct's 9-argument form (BrnTable.h:169), and
// BrnGuiDemangledEventTypes.h:417 carries the four measured payload fields on
// GuiEventNetworkCustomMatchSearch.
//
// Also shipping here is the block of class statics OnEnter consumes. Their values are
// dumped, not inferred (scratchpad/waveJ/ocm_rodata.txt), they are declared by the class
// header, and nothing else in the tree defines them. When the conductor merges the wave
// into the consolidated BrnOnlineCustomMatch.cpp these definitions move there -- this is
// the one copy, so nothing needs dropping.
//
// maResourceTuplesToLoad / miNumResourcesToLoad are DELIBERATELY absent: they are already
// defined in BrnScreenStatesDataLinkStubs.cpp:165 and redefining them would be a link-time
// duplicate.

// wave-J partfile (group 6)


namespace BrnGui
{
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82496C10.json, asm arbitrated over Hex-Rays).
//
// Notes taken from the asm rather than the pseudocode:
//  * Hex-Rays renders EVERY CgsCore::SPrintf here with a dozen bogus varargs (the
//    v4/v6/v21..v29 register soup) and even folds the format+argument pair into one
//    __SPAIR64__ literal. The asm shows the real shapes: the two name builders are
//    SPrintf(buf, 0x40, "%s_%d", <base name>, <index>) (r3..r7 set, nothing above r7),
//    and the row-data blanker is SPrintf(buf, 8, "") -- r3/r4/r5 only, NO vararg at all
//    (r5 = &unk_820046A7, the empty string sitting immediately after "%s%s%s\0").
//  * `stb r14, 0x42E6(r31)` @0x82496D6C is 17126 == mMessageText + 0x126 == TextField::
//    mbAutosize, i.e. mMessageText.SetAutoSize(true) -- NOT mNumGamesFoundText, whose
//    base is the neighbouring 0x42E8 the very next instruction loads. The two are one
//    instruction apart in the listing, which is what makes this easy to mis-attribute.
//  * The pseudocode's stray `*a1 = 0` is NOT a store to this+0: it is the register-indexed
//    `stbx r26, r31, r10` with r10 == 0xDFF2 == 57330 == mbHasRecievedSearchResults
//    (Hex-Rays lost the index register). The two other stbx (0xDFF0/0xDFF1) are
//    miFirstRow/miLastRow, and 0xE258/0xE259/0xE250/0xE254 are the four mLastSearchParams
//    fields. Every one of those console numbers is DOCUMENTATION ONLY -- the host layout
//    is name-based and the object contains pointers that widen.
//  * The cell-pointer store `4 * (16 * row + col + 9641)` is the flat form of
//    maapTableCellComponentPtrs[row][col]: 9641 * 4 == 38564 is the array base and the
//    16 is the SECOND dimension's pitch. Written by name here; 16 is the host array's own
//    dimension, not a transplanted console stride.
//  * The icon/text-field counters are read back with `extsb` on every iteration, so they
//    really are signed bytes in the original source.
//  * Both AddEvent records are stack-built at sp+0x60 and reuse the same slots, which is
//    why the pseudocode shows the `ld`/`std` doubleword shuffle; what reaches the wire is
//    decoded per record below. Both go out on channel 0x28 == 40.
//  * No floats anywhere in this body, so there is no NaN-polarity decision to make.
    namespace
    {
// (fold: an identical definition of KI_CHANNEL_GUI_OUT was dropped here -- this TU defines it once, above)

        // The apt id the two group Constructs pass. The X360 builds it once into r18 with
        // `li r11, -1` + `clrldi r18, r11, 32` (0x82496CF8/0x82496D00) and reuses that
        // register for MenuToggleGroupVarSize<3>::Construct, MenuComponent::Construct and
        // the trailing Table::Construct stack argument: the 32-bit all-ones apt id
        // ZERO-EXTENDED into the u64 parameter -- NOT a 64-bit -1. Same literal as
        // BrnOnlineGameOptions_wI_05.cpp / BrnOnlinePlay.cpp.
        const u64 KU_INVALID_APT_ID = 0xFFFFFFFFull;

        // ---- plain string literals the X360 pools (they are NOT class statics) ----------
        // aSD_2 / aCellcomponent / aCell: ordinary literals in the rodata string pool, unlike
        // the "FoundGames"/"SearchParam"/... names, which live in the class's own static
        // block at 0x8205E9A0 / 0x8205E788 and are declared in the header.
        const char KAC_NAME_INDEX_FORMAT[]   = "%s_%d";
        const char KAC_CELL_COMPONENT_NAME[] = "CellComponent";
        const char KAC_CELL_NAME[]           = "Cell";

        // @0x820046A7 -- the empty format string (the byte immediately after "%s%s%s\0").
        // OnEnter runs it through SPrintf into an 8-byte stack buffer purely to obtain a
        // blank string to seed the four text columns with.
        const char KAC_EMPTY_FORMAT[] = "";

        // Component-name scratch capacity: the X360 passes `li r4, 0x40` to both name
        // builders (0x82496E08 / 0x82496E24) over 64-byte stack buffers.
        const u32 KU_COMPONENT_NAME_BUFFER_LEN = 64;

        // Blank-text scratch capacity: `li r4, 8` @0x82496F60.
        const u32 KU_EMPTY_TEXT_BUFFER_LEN = 8;

        // The text-field-count assert bound. MEASURED `cmpwi r28, 0x28` @0x82496EA0: the
        // console really bounds the count against 40 while maTextFields is only 20 entries
        // long. The assert string names the constant it came from --
        // KI_NUM_ONLINE_INSTANT_RESULTS_TEXT_FIELDS (DWARF
        // GameSource/Gui/Flow/PostEvent/States/Online/BrnOnlineInstantResults.h:34 == 40) --
        // so this loop is a copy-paste of the instant-results screen's cell builder that
        // kept the wrong bound. It is benign (only 20 text cells are ever constructed:
        // 5 rows x 4 TEXTFIELD columns), and it is reproduced verbatim, string and all,
        // rather than "corrected" to 20.
        const s32 KI_NUM_ONLINE_INSTANT_RESULTS_TEXT_FIELDS = 40;

        // maIconStates index the row-data seed writes into the three icon columns.
        // MEASURED: `li r25, 5` @0x82496F48, stored into cells 5 and 6; maIconStates[5]
        // (off_8205E9C8+20) is the string "Empty".
        const s32 KI_ICON_STATE_EMPTY = 5;

        // ---- out-queue wire records -----------------------------------------------------
        // A posted event is a CgsGui::GuiEvent<N> header { payload bytes, event id, payload
        // offset } followed by the payload, published through
        // mpStateInterface->GetOutputEventQueue()->AddEvent(record, channel, sizeof(record)).

        // Id 148 == BrnGui::GuiEventShowHideHud. The homed type in
        // BrnGuiDemangledEventTypes.h:174 is the RAW one-byte payload view (u8 maData[1])
        // with no GuiEvent<148> base, so it cannot carry the 12-byte header this call site
        // stack-builds; the wire form is rebuilt here exactly as
        // BrnOnlineGameOptions_wI_05.cpp does.
        //
        // HEADER0 IS THE PAYLOAD BYTE COUNT, NOT sizeof(record) - offsetof(payload). A lone
        // bool pads the record out to 16, so the subtraction would publish 4 where the X360
        // publishes 1: `stw r14(==1), 0x60(r1)` @0x824970A8 with word2 = 12 and the AddEvent
        // size `li r6, 0x10` == 16.
        struct GuiEventShowHideHudWire : public CgsGui::GuiEvent<148>
        {
            bool mbShowHud;   // payload +0x00

            explicit GuiEventShowHideHudWire(bool lbShowHud)
                : CgsGui::GuiEvent<148>(
                      static_cast<u32>(sizeof(bool)),                                  // X360 1
                      static_cast<u32>(offsetof(GuiEventShowHideHudWire, mbShowHud)))   // X360 12
                , mbShowHud(lbShowHud)
            {
            }
        };
    }

    // ====================================================================================
    //  Class statics
    //
    //  CONSOLIDATION NOTE: these are the statics OnEnter consumes, defined here because
    //  this partfile is their only consumer in the wave. The class has no BrnOnlineCustomMatch.cpp
    //  yet; when the conductor merges the wave-J partfiles, these definitions belong in
    //  that file and must appear exactly once.
    //
    //  maResourceTuplesToLoad / miNumResourcesToLoad are DELIBERATELY absent -- they are
    //  already defined in BrnScreenStatesDataLinkStubs.cpp:165 and redefining them here
    //  would be a link-time duplicate.
    // ====================================================================================

    // @0x8205E758 -- the event ids this screen observes. 6 is controller-input-pressed,
    // 254 the custom-match search results and 51 network-in-game-failed (all three are
    // attested by this TU's own handlers); the remaining five are dumped values whose
    // roles are not attested here, so they are left as plain data.
    const s32 OnlineCustomMatch::maiEventToObserve[8] = { 14, 21, 6, 64, 254, 50, 51, 44 };
    const s32 OnlineCustomMatch::miNumEventsObserved  = 8;   // @0x8205E778

    // @0x8205E788 onwards -- the apt component names the state constructs.
    const char OnlineCustomMatch::KAC_SEARCH_PARAMS_COMPONENT[12]           = "SearchParam";
    const char OnlineCustomMatch::KAC_MESSAGE_BUTTONS_COMPONENT[7]          = "Button";
    const char OnlineCustomMatch::KAC_MESSAGE_TEXT_COMPONENT[12]            = "MessageText";
    const char OnlineCustomMatch::KAC_NUM_GAMES_FOUND_TEXT_COMPONENT[14]    = "NumGamesFound";
    const char OnlineCustomMatch::KAC_MESSAGE_ANIMATION_COMPONENT[22]       = "MessageTextTransition";
    const char OnlineCustomMatch::KAC_MESSAGE_BUTTONS_ANIMATION_COMPONENT[25]= "MessageButtonsTransition";
    const char OnlineCustomMatch::KAC_SEARCH_PARAMS_ANIMATION_COMPONENT[23] = "SearchParamsTransition";
    const char OnlineCustomMatch::KAC_FOUND_GAMES_ANIMATION_COMPONENT[21]   = "FoundGamesTransition";
    const char OnlineCustomMatch::KAC_BUTTON_PROMPT_ANIMATION_COMPONENT[24] = "ButtonPromptsTransition";

    // @0x8205E9A0 -- the table's own component name. OnEnter uses it twice: as the base of
    // every row name ("FoundGames_<row>") and as the Table's component name.
    const char OnlineCustomMatch::macTableName[11] = "FoundGames";

    // The two dimensions exist in the binary only as the `li r9, 5` / `li r10, 7`
    // immediates the compiler folded out of the s8 statics (0x82497004 / 0x82496FFC);
    // there is no rodata slot for either.
    const s8 OnlineCustomMatch::miTableNumRows    = 5;
    const s8 OnlineCustomMatch::miTableNumColumns = 7;

    // @0x8205E9AC -- what kind of child component each of the seven columns holds:
    // an icon, four text fields, then two more icons. 5 rows x 4 text columns == the 20
    // maTextFields entries and 5 x 3 == the 15 maIcons entries.
    const TableCell::TableCellComponentTypes OnlineCustomMatch::maTableRowComponentTypes[7] =
    {
        TableCell::E_TABLECELLCOMPONENTTYPES_ICON,
        TableCell::E_TABLECELLCOMPONENTTYPES_TEXTFIELD,
        TableCell::E_TABLECELLCOMPONENTTYPES_TEXTFIELD,
        TableCell::E_TABLECELLCOMPONENTTYPES_TEXTFIELD,
        TableCell::E_TABLECELLCOMPONENTTYPES_TEXTFIELD,
        TableCell::E_TABLECELLCOMPONENTTYPES_ICON,
        TableCell::E_TABLECELLCOMPONENTTYPES_ICON,
    };

    // @0x8205E9C8 -- the apt state identifiers every found-games icon cell selects from.
    // The first three are the scroll-position markers the highlight row uses; the last
    // three are the per-game badges (friends / rivals / nothing).
    const char* const OnlineCustomMatch::maIconStates[6] =
    {
        "Top", "Middle", "Bottom", "Friends", "Rivals", "Empty"
    };

    // ================================================================================
    //  OnEnter  @ 0x82496C10
    //
    //  Bring the custom-match screen up: register the screen's event set, construct every
    //  embedded component, build the 5x7 grid of found-games cell components and the row
    //  data behind it, reset the screen's own state and publish the two entry-time records.
    // ================================================================================
    void OnlineCustomMatch::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // ---- the five transition animators (GuiComponent Construct, vtable slot 0) ------
        mMessageAnimation.Construct(KAC_MESSAGE_ANIMATION_COMPONENT, mpStateInterface, 0);
        mMessageButtonsAnimation.Construct(KAC_MESSAGE_BUTTONS_ANIMATION_COMPONENT, mpStateInterface, 0);
        mSearchParamsAnimation.Construct(KAC_SEARCH_PARAMS_ANIMATION_COMPONENT, mpStateInterface, 0);
        mFoundGamesAnimation.Construct(KAC_FOUND_GAMES_ANIMATION_COMPONENT, mpStateInterface, 0);
        mButtonPromptAnimation.Construct(KAC_BUTTON_PROMPT_ANIMATION_COMPONENT, mpStateInterface, 0);

        // The search-parameter toggles (3 rows == the group's TI_SIZE) and the two-button
        // message prompt. Both take (no parent name, invalid apt id).
        mSearchParms.Construct(KAC_SEARCH_PARAMS_COMPONENT, mpStateInterface, 3, 0, KU_INVALID_APT_ID);
        mMessageButtons.Construct(KAC_MESSAGE_BUTTONS_COMPONENT, mpStateInterface, 2, 0, KU_INVALID_APT_ID);

        mMessageText.Construct(KAC_MESSAGE_TEXT_COMPONENT, mpStateInterface, 0);

        // The message body grows to fit whatever prompt ShowMessage pushes at it. The X360
        // raises the flag between the two Construct calls (0x82496D6C).
        mMessageText.SetAutoSize(true);

        mNumGamesFoundText.Construct(KAC_NUM_GAMES_FOUND_TEXT_COMPONENT, mpStateInterface, 0);

        // ---- the screen's own state (asm store order; the members are independent) ------
        meSubState = E_SUBSTATE_LOADING_SCREEN;
        miFirstRow = 0;
        mpGuiCache = 0;
        miLastRow  = 0;

        // ---- the 5x7 grid of found-games cell components --------------------------------
        // Every cell gets its own component named "CellComponent_<column>" parented on the
        // row's "FoundGames_<row>". Icons and text fields are drawn from the two flat member
        // arrays in column order, so the counters run across rows rather than restarting.
        s8 liTextFieldCount = 0;
        s8 liIconCount      = 0;

        for (s8 liRow = 0; liRow < miTableNumRows; ++liRow)
        {
            char lacRowName[KU_COMPONENT_NAME_BUFFER_LEN];
            CgsCore::SPrintf(lacRowName, KU_COMPONENT_NAME_BUFFER_LEN, KAC_NAME_INDEX_FORMAT,
                             macTableName, liRow);

            for (s8 liColumn = 0; liColumn < miTableNumColumns; ++liColumn)
            {
                char lacCellName[KU_COMPONENT_NAME_BUFFER_LEN];
                CgsCore::SPrintf(lacCellName, KU_COMPONENT_NAME_BUFFER_LEN, KAC_NAME_INDEX_FORMAT,
                                 KAC_CELL_COMPONENT_NAME, liColumn);

                CgsGui::GuiComponent* lpCellComponent = 0;

                if (maTableRowComponentTypes[liColumn] == TableCell::E_TABLECELLCOMPONENTTYPES_ICON)
                {
                    CGS_ASSERT(liIconCount < Table::KI_MAX_ROWS_PER_TABLE,
                               "liIconCount < Table::KI_MAX_ROWS_PER_TABLE");   // X360 cpp:215

                    maIcons[liIconCount].Construct(lacCellName, mpStateInterface,
                                                   maIconStates, lacRowName);
                    lpCellComponent = &maIcons[liIconCount];
                    ++liIconCount;
                }
                else if (maTableRowComponentTypes[liColumn] ==
                             TableCell::E_TABLECELLCOMPONENTTYPES_TEXTFIELD)
                {
                    // The bound is the instant-results screen's constant, not this screen's
                    // KI_NUM_ONLINE_FOUND_GAMES_TEXT_FIELDS -- see the note on the constant.
                    CGS_ASSERT(liTextFieldCount < KI_NUM_ONLINE_INSTANT_RESULTS_TEXT_FIELDS,
                               "liTextFieldCount < KI_NUM_ONLINE_INSTANT_RESULTS_TEXT_FIELDS"); // X360 cpp:222

                    maTextFields[liTextFieldCount].Construct(lacCellName, mpStateInterface,
                                                             lacRowName);
                    lpCellComponent = &maTextFields[liTextFieldCount];
                    ++liTextFieldCount;
                }
                else
                {
                    // A NOTYPE column would build nothing AND store nothing (the X360 skips
                    // the pointer store entirely). No such column exists in the table above.
                    continue;
                }

                maapTableCellComponentPtrs[liRow][liColumn] = lpCellComponent;
            }
        }

        // ---- the row data behind the table ---------------------------------------------
        // Five blank rows: the scroll-marker icon column cleared to 0, the four text columns
        // blanked, and the two badge icon columns parked on maIconStates "Empty".
        mTableData.Construct();

        for (s8 liRowData = 0; liRowData < miTableNumRows; ++liRowData)
        {
            char lacBlankText[KU_EMPTY_TEXT_BUFFER_LEN];
            CgsCore::SPrintf(lacBlankText, KU_EMPTY_TEXT_BUFFER_LEN, KAC_EMPTY_FORMAT);

            TableRowDataSet* lpRowData = &maTableRowDataSets[liRowData];

            lpRowData->SetInteger(0, 0);
            lpRowData->SetText(1, lacBlankText);
            lpRowData->SetText(2, lacBlankText);
            lpRowData->SetText(3, lacBlankText);
            lpRowData->SetText(4, lacBlankText);
            lpRowData->SetInteger(5, KI_ICON_STATE_EMPTY);
            lpRowData->SetInteger(6, KI_ICON_STATE_EMPTY);

            mTableData.AddRowData(lpRowData);
        }

        // The cell components and the row data are both in place, so the table itself can
        // be built over them. The trailing pair is (no parent name, invalid apt id).
        mTable.Construct(macTableName, mpStateInterface, KAC_CELL_NAME,
                         maapTableCellComponentPtrs, maTableRowComponentTypes,
                         miTableNumRows, miTableNumColumns, 0, KU_INVALID_APT_ID);

        // ---- the search state the screen re-enters with ---------------------------------
        // ShowInitialScreen / HandleControllerInputSelectParams overwrite all four before
        // they post a search; OnEnter only has to leave them in a defined state.
        mLastSearchParams.meGameMode       = 0;
        mLastSearchParams.meSearchOpponentTypes = 0;
        mLastSearchParams.mbRanked         = true;
        mLastSearchParams.mbFreeburn       = true;

        mbHasRecievedSearchResults = false;

        // ---- the two entry-time records, both on channel 40 ------------------------------
        // Take the front-end map (CrashNav) down while the custom-match page owns the screen.
        // Byte-identical to the record BrnOnlinePlay.cpp:172 posts.
        GuiEventActivateCrashNav lActivateCrashNav(false);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lActivateCrashNav), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lActivateCrashNav)));   // X360 record size 20

        GuiEventShowHideHudWire lHideHud(false);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lHideHud), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lHideHud)));            // X360 record size 16
    }
}

// ============================================================================
// FOLDED FROM BrnOnlineCustomMatch_wJ_07.cpp (wave J) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/Gui/Flow/Screen/States/BrnOnlineCustomMatch_wJ_07.cpp
//
// BrnGui::OnlineCustomMatch -- the two lifecycle bodies wave J left to a "foreign TU":
//
//   OnlineCustomMatch::OnLeave  @0x824970D0  ( 63 insns)
//   OnlineCustomMatch::Update   @0x824AC808  (246 insns)
//
// With these the class is complete (16 wave-J bodies + these two + OnEnter in _wJ_06), so
// this partfile is what lets the six _wJ_ TUs mount and retires the three
// BrnScreenStatesLinkStubs.cpp scaffolds (OnEnter / OnLeave / Update) that stood in for
// the screen's vtable since 2026-08-03. Neither function has a DecFIGS scope (the PS3 unity
// build compiled them into another unit), so everything below is read off the X360 ARTIST
// assembly; the sibling partfiles' names are reused wherever the same store appears.
//
// ---- OnLeave @0x824970D0 -- store-for-store ----------------------------------------------
//   0x824970F8  UnRegisterForEvents(mpStateInterface, maiEventToObserve (unk_8205E758), 8)
//   0x82497100..0x82497144  the inlined StateInterface::PlayAptMovie: record
//               { 8, 18, 12, name = unk_820046A7 (the shared EMPTY rodata byte), level = 3 }
//               on the interface's out queue (this+0x1C, +0xC), channel 41, 20 bytes --
//               i.e. "clear apt level 3", the same call OnlineScoreboards::OnLeave spells.
//   0x82497148..0x82497170  { 1, 253, 12 } + one never-written payload byte, channel 40,
//               16 bytes -- the search-stop record _wJ_04's cancel arm also posts.
//   0x82497174..0x824971B0  { 2, 536, 12, u16 0 } channel 40, 16 bytes: the ticker
//               "clear messages" record with BOTH payload bytes zero (`stb 0 ; stb 0 ;
//               lhz ; sth`), so neither the force-fade nor the delete-challenges bit is set.
//
// ---- Update @0x824AC808 -- the per-frame pump ---------------------------------------------
//   0x824AC820  GetFirstEvent(mpInGuiEventQueue (this+0x18), &event, &size) -> id in r28
//   0x824AC8B0  `addi r11,r28,-6 ; cmplwi 0xF8` -- a 249-way jump table over ids 6..254:
//       6    HandleControllerInput(event)
//       14   nothing        (observed -- maiEventToObserve -- but no arm)
//       21   nothing        (ditto)
//       44   assert mpGuiCache (cpp:344); mpGuiCache->meLastDisconnectedError (+0x4B40) =
//            event ? *(s32*)event : 0; SendStateEvent("DISCONNECT")
//       50   assert mpGuiCache (cpp:316); GuiOverlayWaitFinishRequest::Construct("CNOnlEntGame")
//            -> { 8, 188, 16, <pad>, id } channel 40, 24 bytes; SendStateEvent("ADVANCE")
//       51   HandleInGameFailedEvent(event)
//       64   HandleGuiCacheEvent(event)
//       254  mbHasRecievedSearchResults (+0xDFF2) = 1; memcpy(&mSearchResults (+0xDFF4), event, 604)
//       else the streamed "Unexpected event received : <id> in <file> at line 353" assert
//   0x824ACF3C  GetNextEvent(...) until the event pointer is null
//   0x824ACF54  mpInGuiEventQueue->Clear()
//   0x824ACF5C  CheckForCompletedLoads()
//   0x824ACF60  component vtable slot 5 (+0x14) on this+0x2F8  == mSearchParms.Update()
//   0x824ACF74  slot 5 on this+0x4418                          == mTable.Update()
//   0x824ACF88  slot 5 on this+0x3100                          == mMessageButtons.Update()
//   0x824ACFA4  if (mbHasRecievedSearchResults) HandleSearchResults()
//
// The component slot-5 calls are spelled BY NAME per the project's flat-vtable convention
// (BrnSelectableGroup.h): the toggle group and the menu have no override of their own, so
// they resolve to SelectableGroup::Update @0x824E3FE0; the table DOES override it on the
// console (Table::Update @0x824E4890, DWARF BrnTable.cpp:135) -- that override is landed
// in BrnTable.cpp together with this file so the call binds to the right body.
//
// The streamed default-arm assert is lowered to a CGS_ASSERT with the static text, per the
// standing project rule (see BrnVehicleManager_PerFrameLeaves.cpp).
// ============================================================================


namespace BrnGui
{
    namespace
    {
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;   // mpInGuiEventQueue's real type

// (fold: an identical definition of KI_CHANNEL_GUI_OUT was dropped here -- this TU defines it once, above)

        // The observed event ids this screen's Update dispatches on (maiEventToObserve ==
        // { 14, 21, 6, 64, 254, 50, 51, 44 }). Named by their consumers; 14 and 21 are
        // observed but have no arm (the jump table sends both straight to the loop tail).
        const s32 KI_EVENT_CONTROLLER_INPUT       = 6;
        const s32 KI_EVENT_OBSERVED_NO_ARM_14     = 14;
        const s32 KI_EVENT_OBSERVED_NO_ARM_21     = 21;
        const s32 KI_EVENT_NETWORK_DISCONNECTED   = 44;
        const s32 KI_EVENT_ENTER_GAME             = 50;
        const s32 KI_EVENT_IN_GAME_FAILED         = 51;
        const s32 KI_EVENT_GUI_CACHE              = 64;
        const s32 KI_EVENT_CUSTOM_MATCH_RESULTS   = 254;   // == GuiEventNetworkCustomMatchResults::GetEventType()

        // OnLeave's apt-level clear (the inlined PlayAptMovie): the shared empty rodata
        // byte unk_820046A7 at level 3, as every sibling OnLeave spells it.
        const char* const KPC_EMPTY_STRING   = "";
// (fold: an identical definition of KI_APT_MOVIE_LEVEL was dropped here -- this TU defines it once, above)

// (fold: an identical definition of KAC_ENTER_GAME_OVERLAY_ID was dropped here -- this TU defines it once, above)
        const char KAC_ADVANCE_EVENT[]         = "ADVANCE";        // 0x824AC88C
        const char KAC_DISCONNECT_EVENT[]      = "DISCONNECT";     // 0x824AC880

        // Id 44: the server-interface error the disconnect popup shows (`lwz 0(r21)`).
        // Same shape as BrnOnlineGameRoomPlayerInfo_wH_00.cpp's.
        struct NetworkDisconnectedPayload : public CgsModule::Event
        {
            s32 meError;   // +0x00
        };

// (fold: an identical definition of CustomMatchSearchStopWire was dropped here -- this TU defines it once, above)

        // { 2, 536, 12, u16 0 }, channel 40, 16 bytes -- GuiEventTickerClearMessages with
        // both bytes CLEAR (0x82497190 `stb 0 ; stb 0`). Field names per the HUD twin
        // (BrnRaceMainHudState_wS2.cpp GuiTickerClearWire536).
        struct GuiTickerClearWire536 : public CgsGui::GuiEvent<536>
        {
            u8 mbForceFadeOut;            // +0x0C == 0
            u8 mbDeleteChallengeMessages; // +0x0D == 0
            u8 mau8Pad[2];

            GuiTickerClearWire536()
                : CgsGui::GuiEvent<536>(2, 12)
                , mbForceFadeOut(0), mbDeleteChallengeMessages(0)
            {
                mau8Pad[0] = mau8Pad[1] = 0;
            }
        };

// (fold: an identical definition of GuiOverlayWaitFinishRequestWire was dropped here -- this TU defines it once, above)

        typedef char KAC_ASSERT_STOP_WIRE_SIZE[sizeof(CustomMatchSearchStopWire) == 16 ? 1 : -1];
        typedef char KAC_ASSERT_CLEAR_WIRE_SIZE[sizeof(GuiTickerClearWire536) == 16 ? 1 : -1];
        typedef char KAC_ASSERT_WAIT_FINISH_WIRE_SIZE[sizeof(GuiOverlayWaitFinishRequestWire) == 24 ? 1 : -1];
        typedef char KAC_ASSERT_WAIT_FINISH_PAYLOAD_OFFSET[
            offsetof(GuiOverlayWaitFinishRequestWire, mRequest) == 16 ? 1 : -1];
        // The 604-byte AddOutputGuiEvent record IS the member (headerless on both sides).
        typedef char KAC_ASSERT_RESULTS_RECORD_SIZE[sizeof(GuiEventNetworkCustomMatchResults) == 604 ? 1 : -1];
    }

    // ================================================================================
    //  OnLeave  @ 0x824970D0
    // ================================================================================
    void OnlineCustomMatch::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // The X360 inlines StateInterface::PlayAptMovie here (record { 8, 18, 12, name,
        // level } on channel 41, 20 bytes). An empty name at level 3 is "clear level 3".
        mpStateInterface->PlayAptMovie(KPC_EMPTY_STRING, KI_APT_MOVIE_LEVEL);

        // Stop any custom-match search still in flight.
        const CustomMatchSearchStopWire lStop;
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lStop), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lStop)));   // X360 record size 16

        // ...and clear the ticker (no fade-out force, challenge messages kept).
        const GuiTickerClearWire536 lClear;
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lClear), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lClear)));  // X360 record size 16
    }

    // ================================================================================
    //  Update  @ 0x824AC808
    // ================================================================================
    void OnlineCustomMatch::Update()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;

        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            switch (liEventId)
            {
            case KI_EVENT_CONTROLLER_INPUT:
                HandleControllerInput(lpEvent);
                break;

            case KI_EVENT_OBSERVED_NO_ARM_14:
            case KI_EVENT_OBSERVED_NO_ARM_21:
                break;

            case KI_EVENT_NETWORK_DISCONNECTED:
                CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:344
                // 0x824ACDB0..0x824ACDD8: a null event still writes the slot (0).
                if (lpEvent != 0)
                {
                    mpGuiCache->meLastDisconnectedError =
                        reinterpret_cast<const NetworkDisconnectedPayload*>(lpEvent)->meError;
                }
                else
                {
                    mpGuiCache->meLastDisconnectedError = 0;
                }
                SendStateEvent(KAC_DISCONNECT_EVENT);
                break;

            case KI_EVENT_ENTER_GAME:
            {
                CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:316

                // Take down the "entering game" wait overlay, then advance the flow.
                const GuiOverlayWaitFinishRequestWire lRequest(KAC_ENTER_GAME_OVERLAY_ID);
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lRequest), KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(lRequest)));   // X360 record size 24

                SendStateEvent(KAC_ADVANCE_EVENT);
                break;
            }

            case KI_EVENT_IN_GAME_FAILED:
                HandleInGameFailedEvent(lpEvent);
                break;

            case KI_EVENT_GUI_CACHE:
                HandleGuiCacheEvent(lpEvent);
                break;

            case KI_EVENT_CUSTOM_MATCH_RESULTS:
                // 0x824ACD7C `stbx 1` then the 604-byte memcpy of the headerless record.
                mbHasRecievedSearchResults = true;
                std::memcpy(&mSearchResults, lpEvent, sizeof(GuiEventNetworkCustomMatchResults));
                break;

            default:
                // The streamed "Unexpected event received : <id> in <file> at line 353"
                // assert, lowered to the static text.
                CGS_ASSERT(false, "Unexpected event received");   // cpp:353
                break;
            }
        }

        lpInQueue->Clear();

        CheckForCompletedLoads();

        // Component vtable slot 5 on the three interactive components, in the console's
        // order: the parameter toggles, the found-games table, the message buttons.
        mSearchParms.Update();     // SelectableGroup::Update @0x824E3FE0 (no override)
        mTable.Update();           // Table::Update @0x824E4890 (the override, BrnTable.cpp)
        mMessageButtons.Update();  // SelectableGroup::Update @0x824E3FE0 (no override)

        if (mbHasRecievedSearchResults)
        {
            HandleSearchResults();
        }
    }

    // =====================================================================================
    // The class statics no partfile defined (the link listed all thirteen once the six
    // _wJ_ TUs were mounted). Pointer tables read from the image (headless IDA, 2026-09-02;
    // the .data slots are initialised on disk, no thunk); the char[] literals are the
    // DWARF-sized strings the header records.
    // =====================================================================================
    const char* const OnlineCustomMatch::KAPC_ANIMATION_STATES[3] =                  // @0x82F266B0
    {
        "Visible", "Invisible", "Refresh"
    };

    const char* const OnlineCustomMatch::KAPC_GAME_MODE_STRING_IDS[8] =              // @0x82F266BC
    {
        "$ONLINE_GAME_OPTION_MODE_RACE",                  // 0  (mode 10)
        "$ONLINE_GAME_OPTION_MODE_ROAD_RAGE",             // 1  (mode 11)
        "$ONLINE_GAME_OPTION_MODE_STUNT",                 // 2  (mode 12)
        "$ONLINE_GAME_OPTION_MODE_BURNING_HOME_RUN",      // 3  (mode 13)
        "$ONLINE_GAME_OPTION_MODE_STUNT_FREE_FOR_ALL",    // 4  (mode 14)
        "$ONLINE_GAME_OPTION_MODE_FREEBURN_LOBBY",        // 5  (mode 15)
        "Invalid game mode",                              // 6  (mode 16 -- the shipped literal, kept)
        "$ONLINE_GAME_OPTION_MODE_STUNT_COOP",            // 7  (mode 17)
    };

    const char* const OnlineCustomMatch::KAPC_OPPONENT_OPTION_STRING_IDS[4] =        // @0x82F266DC
    {
        "$ONLINE_GAME_SEARCH_OPTION_ANY",
        "$ONLINE_GAME_SEARCH_OPTION_FRIENDS_AND_RIVALS",
        "$ONLINE_GAME_SEARCH_OPTION_FRIENDS",
        "$ONLINE_GAME_SEARCH_OPTION_RIVALS",
    };

    const char* const OnlineCustomMatch::KAPC_YES_NO_BUTTON_STRING_ID[2] =           // @0x82F266A4
    {
        "$GENERAL_OPTION_YES", "$GENERAL_OPTION_NO"
    };

    const char* const OnlineCustomMatch::KAPC_OK_BUTTON_STRING_ID[1] =               // @0x82F266AC
    {
        "$GENERAL_OPTION_OK"
    };

    // @0x8205E964 -- three { string id, BrnNetwork::ESearchGameModes } pairs:
    // any (0) / race (1) / freeburn lobby (4).
    const OnlineCustomMatch::StringGameModeMapping
    OnlineCustomMatch::KA_GAME_MODE_SEARCH_OPTION_STRING_IDS[3] =
    {
        { "$ONLINE_GAME_SEARCH_OPTION_ANY",         0 },
        { "$ONLINE_GAME_OPTION_MODE_RACE",          1 },
        { "$ONLINE_GAME_OPTION_MODE_FREEBURN_LOBBY", 4 },
    };

    const char OnlineCustomMatch::KAC_GAME_MODE_STRING_ID[25]              = "$ONLINE_GAME_OPTION_MODE";
    const char OnlineCustomMatch::KAC_NO_GAMES_FOUND_STRING_ID[29]         = "$ONLINE_GAME_SEARCH_NO_GAMES";
    const char OnlineCustomMatch::KAC_NO_PREVIOUS_GAME_MODE_STRING_ID[33]  = "$ONLINE_GAME_SEARCH_NO_PREV_MODE";
    const char OnlineCustomMatch::KAC_NUM_GAMES_FOUND_STRING_ID[35]        = "ONLINE_GAME_SEARCH_NUM_GAMES_FOUND";
    const char OnlineCustomMatch::KAC_NUM_GAMES_FOUND_SINGULAR_STRING_ID[43] = "ONLINE_GAME_SEACH_NUM_GAMES_FOUND_SINGULAR";   // X360 typo, kept
    const char OnlineCustomMatch::KAC_NUM_PLAYERS_STRING_ID[31]            = "ONLINE_GAME_SEARCH_NUM_PLAYERS";
    const char OnlineCustomMatch::KAC_OPPONENT_OPTION_STRING_ID[30]        = "$ONLINE_GAME_SEARCH_OPPONENTS";
    const char OnlineCustomMatch::KAC_SEARCHING_STRING_ID[30]              = "$ONLINE_GAME_SEARCH_SEARCHING";

    namespace
    {
// (fold: an identical definition of KAC_APT_TRANSITION_NAME was dropped here -- this TU defines it once, above)
// (fold: an identical definition of KI_ANIMATION_STATE_VISIBLE was dropped here -- this TU defines it once, above)
// (fold: an identical definition of KI_ANIMATION_STATE_INVISIBLE was dropped here -- this TU defines it once, above)
        const s32  KI_NUM_OK_BUTTONS            = 1;
        const s32  KI_OK_BUTTON_INDEX           = 0;
    }

    // ================================================================================
    //  ShowMessage  @ 0x82484A90  (39 insns; the ledger files it under the
    //  BrnAnimationComponent.h catch-all -- it is this class's own body)
    //
    //  Hide the buttons / search form / found-games table, show the button prompts and
    //  the message body, and put lpacTextID in it. Order is the console's.
    // ================================================================================
    void OnlineCustomMatch::ShowMessage(const char* lpacTextID)
    {
        mMessageButtonsAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                       KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_INVISIBLE],
                                                       false);
        mSearchParamsAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                     KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_INVISIBLE],
                                                     false);
        mFoundGamesAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                   KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_INVISIBLE],
                                                   false);
        mButtonPromptAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                     KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_VISIBLE],
                                                     false);
        mMessageAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_VISIBLE],
                                                false);

        mMessageText.SetText(lpacTextID);
    }

    // ================================================================================
    //  ShowNoGamesFoundInGame  @ 0x8248BE68  (51 insns; same catch-all attribution)
    //
    //  The in-game flavour of "no games found": message + a single OK button over the
    //  hidden form and table, and the sub-state moves to NO_GAMES_FOUND_IN_GAME.
    // ================================================================================
    void OnlineCustomMatch::ShowNoGamesFoundInGame()
    {
        mMessageAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_VISIBLE],
                                                false);
        mMessageButtonsAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                       KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_VISIBLE],
                                                       false);
        mButtonPromptAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                     KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_VISIBLE],
                                                     false);
        mSearchParamsAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                     KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_INVISIBLE],
                                                     false);
        mFoundGamesAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                   KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_INVISIBLE],
                                                   false);

        mMessageText.SetText(KAC_NO_GAMES_FOUND_STRING_ID);

        // One live button, no wrap (X360 `li r4, 1` / `li r5, 0`), captioned OK.
        mMessageButtons.SetupMenu(KI_NUM_OK_BUTTONS, false);
        mMessageButtons.SetText(KI_OK_BUTTON_INDEX, KAPC_OK_BUTTON_STRING_ID[0]);

        meSubState = E_SUBSTATE_NO_GAMES_FOUND_IN_GAME;   // stw 7, 0x38
    }
}
