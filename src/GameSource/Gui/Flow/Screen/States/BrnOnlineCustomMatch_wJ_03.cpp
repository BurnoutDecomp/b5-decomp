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

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineCustomMatch.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"       // AddOutputAptViewState
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache (we are a friend)

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
