#pragma once

#include <cstddef>   // offsetof (_AssertLayout)

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"   // GuiEventNetworkCustomMatchSearch / ...Results (payload homes)
#include "GameSource/Gui/BrnGuiTextField.h"             // BrnGui::TextField (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h"  // AnimationComponent (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnIcon.h"                // IconComponent (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuComponent.h"       // MenuComponent (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuToggleGroup.h"     // MenuToggleGroupVarSize<3> (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnTable.h"               // Table / TableDataSet (by value; brings BrnTableRow.h)
#include "GameSource/Gui/Flow/Shared/Components/BrnTableCell.h"           // TableCell::TableCellComponentTypes

// BrnGui::OnlineCustomMatch - the online "custom match" search/join screen state: pick a
// game-mode + opponents filter, fire a network search, list up to 10 found games in a
// 5-row scrolling table, join the highlighted one. Class shape from the DecFIGS DWARF
// (BrnOnlineCustomMatch.h), gated on the X360 ledger; the sub-state machine and all
// screen logic live in BrnOnlineCustomMatch.cpp (wave J).
//
// MEMBER PLACEMENT: X360 ARTIST asm (wave-J dossier + raw asm). The X360 byte offsets in
// the trailing comments are DOCUMENTATION ONLY -- the x64 host layout is name-based and
// several members contain pointers that widen. X360 object size 57952 (corroborated by the
// BrnScreenFlow.h pool comment).
namespace BrnGui
{
    class GuiCache;

    // DWARF BrnOnlineCustomMatch.h:36/37 (namespace-scope consts sizing the cell arrays:
    // 5 rows x 4 text columns / 5 rows x 3 icon columns).
    const s8 KI_NUM_ONLINE_FOUND_GAMES_TEXT_FIELDS = 20;
    const s8 KI_NUM_ONLINE_FOUND_GAMES_ICONS       = 15;

    struct OnlineCustomMatch : public CgsGui::State
    {
        // @ 0x82508CE8 - hands the custom-match screen's static resource list to the loader
        // (X360: *r4 = &maResourceTuplesToLoad; *r5 = miNumResourcesToLoad, count = 1).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = (u32)miNumResourcesToLoad;
        }

        // DWARF h:163.
        enum ESubState
        {
            E_SUBSTATE_LOADING_SCREEN        = 0,
            E_SUBSTATE_LOADING_COMPONENTS    = 1,
            E_SUBSTATE_SELECTING_PARAMS      = 2,
            E_SUBSTATE_SEARCHING             = 3,
            E_SUBSTATE_SELECTING_GAME        = 4,
            E_SUBSTATE_JOINING               = 5,
            E_SUBSTATE_NO_GAMES_FOUND        = 6,
            E_SUBSTATE_NO_GAMES_FOUND_IN_GAME = 7,
            E_SUBSTATE_COUNT                 = 8,
        };

        // DWARF h:147. One search-option row: the toggle's display string id and the
        // BrnNetwork::ESearchGameModes value it stands for. That enum has no committed home
        // yet, so the mode is carried as s32 (measured values in the table: 0 = any,
        // 1 = race, 4 = freeburn lobby -- rodata @0x8205E964).
        struct StringGameModeMapping
        {
            const char* mpcStringID;
            s32         meGameMode;   // DWARF type BrnNetwork::ESearchGameModes
        };

        static const s32 KI_MAX_CREATE_GAME_OPTIONS = 6;    // DWARF h:178 (value from DWARF)
        static const s32 KI_MAX_DISPLAYABLE_GAMES   = 10;   // measured: all six mSearchResults arrays stride 10

        virtual void OnEnter();   // @0x82496C10 (this TU)
        virtual void OnLeave();   // @0x824970D0 -- FOREIGN TU (ledger `reviewed`, defined nowhere yet)
        virtual void Update();    // @0x824AC808 -- FOREIGN TU (ditto)

        // DWARF-only methods NOT in the X360 ledger (GetCurrentlySelectedGame cpp:380,
        // CheckPrivileges cpp:1333) are deliberately NOT declared -- inlined/dropped on X360.

    private:
        // ---- this TU's 16 functions -------------------------------------------------
        void HandleControllerInput(const CgsModule::Event* lpEvent);              // @0x824A35B8 (DWARF: const GuiEventControllerInputPressed*)
        void HandleControllerInputSelectParams(const CgsModule::Event* lpEvent);  // @0x824972D0
        void HandleControllerInputSelectGame(const CgsModule::Event* lpEvent);    // @0x82497570
        void HandleControllerInputNoGames(const CgsModule::Event* lpEvent);       // @0x82497880
        void HandleControllerInputNoGamesInGame(const CgsModule::Event* lpEvent); // @0x82497A68
        void HandleSearchResults();                                               // @0x8248FD30
        void HandleGuiCacheEvent(const CgsModule::Event* lpEvent);                // @0x82497C50 (DWARF: const GuiEventCache*)
        void HandleInGameFailedEvent(const CgsModule::Event* lpEvent);            // @0x82497F50 (DWARF: const GuiEventNetworkInGameFailed*)
        void CheckForCompletedLoads();                                            // @0x824A34A0
        void ShowInitialScreen();                                                 // @0x824971D0
        void ShowParamSelection();                                                // @0x8248FE30
        void ShowFoundGames();                                                    // @0x8248E738
        void ShowNoGamesFound();                                                  // @0x8248BD78
        void FillInTable();                                                       // @0x8248BA90
        void HighlightLastSearchParams();                                         // @0x8248E920

        // ---- methods of THIS class owned by FOREIGN ledger TUs. Declared for shape;
        //      deliberately NOT stubbed (same treatment as the wave-H/I keystones).
        void ShowMessage(const char* lpacTextID);                                 // @0x82484A90 (BrnAnimationComponent.h TU)
        void ShowNoGamesFoundInGame();                                            // @0x8248BE68 (BrnAnimationComponent.h TU)

        // ---- statics (values dumped this wave -> scratchpad/waveJ/ocm_rodata.txt;
        //      definitions live in the .cpp except the two resource ones, which are already
        //      defined in BrnScreenStatesDataLinkStubs.cpp) ---------------------------------
        static const CgsGui::sResourceTuple maResourceTuplesToLoad[]; // @ 0x8205E77C == { { 175, E_GUI_RESOURCETYPE_APT } }
        static const s32                    miNumResourcesToLoad;     // @ 0x8205E784 == 1

        static const s32  maiEventToObserve[8];                  // @0x8205E758 == { 14, 21, 6, 64, 254, 50, 51, 44 }
        static const s32  miNumEventsObserved;                   // == 8

        static const char KAC_SEARCH_PARAMS_COMPONENT[12];       // "SearchParam"
        static const char KAC_FOUND_GAMES_COMPONENT[10];         // DWARF char[10]; used only by the foreign Update/OnLeave
        static const char KAC_MESSAGE_BUTTONS_COMPONENT[7];      // "Button"
        static const char KAC_MESSAGE_TEXT_COMPONENT[12];        // "MessageText"
        static const char KAC_NUM_GAMES_FOUND_TEXT_COMPONENT[14];// "NumGamesFound"
        static const char KAC_MESSAGE_ANIMATION_COMPONENT[22];   // "MessageTextTransition"
        static const char KAC_MESSAGE_BUTTONS_ANIMATION_COMPONENT[25]; // "MessageButtonsTransition"
        static const char KAC_SEARCH_PARAMS_ANIMATION_COMPONENT[23];   // "SearchParamsTransition"
        static const char KAC_FOUND_GAMES_ANIMATION_COMPONENT[21];     // "FoundGamesTransition"
        static const char KAC_BUTTON_PROMPT_ANIMATION_COMPONENT[24];   // "ButtonPromptsTransition"

        static const char* const KAPC_ANIMATION_STATES[3];       // @0x82F266B0 == { "Visible", "Invisible", "Refresh" }

        static const char KAC_NO_GAMES_FOUND_STRING_ID[29];      // "$ONLINE_GAME_SEARCH_NO_GAMES"
        static const char KAC_RANKED_TITLE_STRING_ID[27];        // DWARF char[27]; used only by the foreign Update/OnLeave
        static const char KAC_SEARCHING_STRING_ID[30];           // "$ONLINE_GAME_SEARCH_SEARCHING"
        static const char KAC_JOINING_STRING_ID[28];             // "$ONLINE_GAME_SEARCH_JOINING"
        static const char KAC_GAME_MODE_STRING_ID[25];           // "$ONLINE_GAME_OPTION_MODE"
        static const char KAC_OPPONENT_OPTION_STRING_ID[30];     // "$ONLINE_GAME_SEARCH_OPPONENTS"
        static const char KAC_NUM_GAMES_FOUND_STRING_ID[35];     // "ONLINE_GAME_SEARCH_NUM_GAMES_FOUND"
        static const char KAC_NUM_GAMES_FOUND_SINGULAR_STRING_ID[43]; // "ONLINE_GAME_SEACH_NUM_GAMES_FOUND_SINGULAR" (X360 typo, kept)
        static const char KAC_NUM_PLAYERS_STRING_ID[31];         // "ONLINE_GAME_SEARCH_NUM_PLAYERS"
        static const char KAC_NO_PREVIOUS_GAME_MODE_STRING_ID[33]; // "$ONLINE_GAME_SEARCH_NO_PREV_MODE"

        static const char* const KPAC_RANKED_OPTION_STRING_IDS[2]; // @0x82F2669C (foreign Update/OnLeave)
        static const char* const KAPC_YES_NO_BUTTON_STRING_ID[2];  // @0x82F266A4 == { "$GENERAL_OPTION_YES", "$GENERAL_OPTION_NO" }
        static const char* const KAPC_OK_BUTTON_STRING_ID[1];      // @0x82F266AC == { "$GENERAL_OPTION_OK" }

        static const StringGameModeMapping KA_GAME_MODE_SEARCH_OPTION_STRING_IDS[3]; // @0x8205E964 (values in the .cpp)

        // DWARF declares [7]; the X360 body accepts modes 10..17 (`cmpwi 0xA / 0x12`), i.e.
        // EIGHT reachable rodata slots @0x82F266BC -- slot 6 (mode 16) really is the literal
        // "Invalid game mode" (shipped quirk, kept).
        static const char* const KAPC_GAME_MODE_STRING_IDS[8];
        static const char* const KAPC_OPPONENT_OPTION_STRING_IDS[4]; // @0x82F266DC

        static const char macTableName[11];                      // "FoundGames"
        static const s8   miTableNumRows;                        // == 5
        static const s8   miTableNumColumns;                     // == 7
        static const TableCell::TableCellComponentTypes maTableRowComponentTypes[7]; // @0x8205E9AC == { ICON, TEXT, TEXT, TEXT, TEXT, ICON, ICON }
        static const char* const maIconStates[6];                // @0x8205E9C8 == { "Top", "Middle", "Bottom", "Friends", "Rivals", "Empty" }

        // ---- data members (DWARF order; the X360 offsets are documentation) ---------
        ESubState                 meSubState;               // X360 +56
        AnimationComponent        mMessageAnimation;        // +60    "MessageTextTransition"
        AnimationComponent        mMessageButtonsAnimation; // +200   "MessageButtonsTransition"
        AnimationComponent        mSearchParamsAnimation;   // +340   "SearchParamsTransition"
        AnimationComponent        mFoundGamesAnimation;     // +480   "FoundGamesTransition"
        AnimationComponent        mButtonPromptAnimation;   // +620   "ButtonPromptsTransition"
        // DWARF types this MenuToggleGroup; the X360 ctor @0x82500DB8 installs the
        // MenuToggleGroupVarSize<3> vtable (off_82074790) and the member spans 11784 bytes
        // == 0x238 + 3*0xE98 + 8, so it is the <3> instantiation.
        MenuToggleGroupVarSize<3> mSearchParms;              // +760   "SearchParam" (sic DWARF spelling)
        MenuComponent             mMessageButtons;           // +12544 "Button"
        TextField                 mMessageText;              // +16832 "MessageText"
        TextField                 mNumGamesFoundText;        // +17128 "NumGamesFound"
        GuiCache*                 mpGuiCache;                // +17424
        Table                     mTable;                    // +17432 "FoundGames"
        TextField                 maTextFields[KI_NUM_ONLINE_FOUND_GAMES_TEXT_FIELDS]; // +30424 (X360 stride 296)
        IconComponent             maIcons[KI_NUM_ONLINE_FOUND_GAMES_ICONS];            // +36344 (X360 stride 148)
        CgsGui::GuiComponent*     maapTableCellComponentPtrs[16][16];                  // +38564 (5x7 used)
        TableDataSet              mTableData;                // +39588
        TableRowDataSet           maTableRowDataSets[16];    // +39656 (X360 stride 1104)
        bool                      mabPositionHandled[8];     // +57320
        s8                        miFirstRow;                // +57328 (found-games scroll window first row)
        s8                        miLastRow;                 // +57329 (rows visible)
        bool                      mbHasRecievedSearchResults; // +57330 (sic DWARF spelling)
        // The two event-payload members. On the X360 both are HEADERLESS payload structs
        // (604 / 12 bytes -- mSearchResults' six arrays start at the member base); on the
        // host the homed event types carry the 12-byte CgsGui::GuiEvent<N> base in front,
        // which is fine because every access is by name.
        GuiEventNetworkCustomMatchResults mSearchResults;     // +57332 (console payload 604 B)
        GuiEventNetworkCustomMatchSearch  mLastSearchParams;  // +57936 (console payload 12 B)

        // Compile-time pins on RELATIVE deltas inside the pointer-free scalar tail run
        // (never absolute console offsets -- pointers widen on the host).
        void _AssertLayout()
        {
            static_assert(offsetof(OnlineCustomMatch, miFirstRow)
                              - offsetof(OnlineCustomMatch, mabPositionHandled) == 8,
                          "mabPositionHandled[8] runs straight into miFirstRow");
            static_assert(offsetof(OnlineCustomMatch, miLastRow)
                              - offsetof(OnlineCustomMatch, miFirstRow) == 1,
                          "miLastRow follows miFirstRow");
            static_assert(offsetof(OnlineCustomMatch, mbHasRecievedSearchResults)
                              - offsetof(OnlineCustomMatch, miLastRow) == 1,
                          "mbHasRecievedSearchResults follows miLastRow");
        }
    };
}
