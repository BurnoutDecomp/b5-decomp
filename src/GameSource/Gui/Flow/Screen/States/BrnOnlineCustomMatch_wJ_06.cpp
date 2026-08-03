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

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineCustomMatch.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface / the out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiEventActivateCrashNav (id 191)

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
        // ---- AddEvent channels (the out-queue selector word) ----------------------------
        const s32 KI_CHANNEL_GUI_OUT = 40;   // 0x28 -- the only channel OnEnter posts on

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
