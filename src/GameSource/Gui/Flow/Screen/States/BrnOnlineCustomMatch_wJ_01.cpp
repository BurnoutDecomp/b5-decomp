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

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineCustomMatch.h"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"       // AddOutputAptViewState
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface / the out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache (friend)

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
