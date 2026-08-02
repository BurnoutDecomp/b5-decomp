#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/Gui/BrnGuiTextField.h"
#include "GameSource/Gui/Events/BrnGuiEventNetworkGameParams.h"
#include "GameSource/Gui/Flow/Screen/Components/BrnGuiNetworkRouteInfo.h"
#include "GameSource/Gui/Flow/Screen/States/BrnCreateMatchOption.h"
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h"
#include "GameSource/Gui/Flow/Shared/Components/BrnButtonIcon.h"
#include "GameSource/Gui/Flow/Shared/Components/BrnHelpBar.h"
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuComponent.h"
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuToggleGroup.h"

// BrnGui::OnlineGameOptions - the online "create match / game options" screen state. This
// leaf header carries the class shape and the one inline resource accessor attributed to
// the header (the single ledger function for this TU). The create-match option set,
// help-bar items, menu/toggle component wiring and the out-of-line state and virtual
// machinery are reconstructed with the class:BrnGui::OnlineGameOptions TU.
// Layout/virtuals and the CgsGui::State derivation are from the DecFIGS DWARF
// (BrnOnlineGameOptions.h).
//
// MEMBER PLACEMENT: X360 ARTIST asm (this TU's dossier + raw asm, wave I). The X360 byte
// offsets in the trailing comments are DOCUMENTATION ONLY -- the x64 host layout is
// name-based and several of these members contain pointers that widen.
namespace BrnGui
{
    class GuiCache;

    struct OnlineGameOptions : public CgsGui::State
    {
        // @ 0x8251AFA8 - hands the game-options screen's static resource list to the loader
        // (X360: *r4 = &maResourceTuplesToLoad; *r5 = miNumResourcesToLoad, count = 2).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = (u32)miNumResourcesToLoad;
        }

        // DWARF BrnOnlineGameOptions.h:185 / :204
        enum EOptionsToLoad { E_OPTIONS_TO_LOAD_SAVED = 0, E_OPTIONS_TO_LOAD_RECENT = 1,
                              E_OPTIONS_TO_LOAD_COUNT = 2 };
        enum ESubState { E_SUBSTATE_LOADING_SCREEN = 0, E_SUBSTATE_LOADING_COMPONENTS = 1,
                         E_SUBSTATE_SELECTING_PARAMS = 2, E_SUBSTATE_LOAD_OPTIONS = 3,
                         E_SUBSTATE_WAIT_IN_GAME = 4, E_SUBSTATE_COUNT = 5 };

        // DWARF h:154. meButton is the pad-button id (4/5/6 here). The DWARF type
        // BrnGui::ButtonIconComponent::EPadButton DOES have a home -- it is declared at
        // GameSource/Gui/Flow/Shared/Components/BrnButtonIcon.h:29 -- so the member is
        // typed as the DWARF types it. (An earlier wave-I draft claimed no home existed;
        // that was wrong.)
        struct HelpBarItem { const char* mpcText; ButtonIconComponent::EPadButton meButton; };

        static const s32 KI_MAX_CREATE_GAME_OPTIONS = 5;   // DWARF h:242
        static const s32 KI_MAX_LOAD_OPTIONS        = 6;   // DWARF h:243
        static const s32 KI_MAX_HELP_BAR_ITEMS      = 3;   // DWARF h:244

        virtual void OnEnter();   // @0x8249C188 (this TU)
        virtual void OnLeave();   // cpp:443 -- FOREIGN TU (ledger `reviewed`, defined nowhere yet)
        virtual void Update();    // @0x824AF688 -- FOREIGN TU (ditto)

    private:
        // ---- this TU's 22 functions -------------------------------------------------
        void HandleControllerInput(const CgsModule::Event* lpEvent);            // @0x824AD758
        void HandleControllerInputCreateGame(const CgsModule::Event* lpEvent);  // @0x824A7878
        void HandleInGameEvent(const CgsModule::Event* lpEvent);                // @0x8249C9D0
        void HandleInGameFailedEvent(const CgsModule::Event* lpEvent);          // @0x8249CAE0
        void HandleGuiCacheEvent(const CgsModule::Event* lpEvent);              // @0x824A85E8
        void CheckForCompletedLoads();                                          // @0x824A7758
        void SetupCommonCreateGameOptions();                                    // @0x82490598
        void SetupGameModeOptions();                                            // @0x82490630
        void SetupOptions(s32 liToggleIndex, CreateMatchOption::EOption leOption); // @0x8248EB60
        s32  GetOptionIndex(CreateMatchOption::EOption leOption);               // @0x8248ED50
        s32  GetSelectedGameMode() const;                                       // @0x82485A88
        s32  GetNumberOptions();                                                // @0x82485B08
        void HighlightCreateGameOptions();                                      // @0x82490740
        void StoreCreateGameOptions();                                          // @0x82492BD8
        void StoreGameMode();                                                   // @0x82490C68
        bool CheckPrivileges();                                                 // @0x82485C18
        void SetupHelpBar(bool lbUnused);                                       // @0x82485C78 (DWARF bool param; the body ignores it)
        void ShowGameOptionsScreen();                                           // @0x8249C5C8
        void ShowLoadScreen();                                                  // @0x8249C760
        void ShowLoadOptions();                                                 // @0x8248C878
        void TriggerSound(s32 leGameInputAction);                               // @0x8249CCF0

        // ---- methods of THIS class owned by FOREIGN ledger TUs. All are ledger
        //      `reviewed` but defined nowhere in the tree yet, so the screen will not
        //      link until those TUs land. Declared for shape; deliberately NOT stubbed
        //      (same treatment as the wave-H keystone).
        void BuildGameOptions();                                                // @0x8248CA98
        void HandleControllerInputLoadOptions(const CgsModule::Event* lpEvent);  // @0x824A7E48
        void HandleCarInfoResponseEvent(const CgsModule::Event* lpEvent);
        CreateMatchOption::EOption GetHighlightedOption(CreateMatchOption::EOption leOption); // @0x8248EC98
        void ResetGameOptions();                                                // @0x82485B98
        void RequestPresetEvents();                                             // @0x8249CBF8

        // ---- statics (values dumped in wave I; definitions live in the .cpp) --------
        static const CgsGui::sResourceTuple maResourceTuplesToLoad[]; // @ 0x8205F004 (unk_8205F004, .rdata)
        static const s32                    miNumResourcesToLoad;     // @ 0x8205F014 (dword_8205F014, .rdata) == 2

        static const s32  maiEventToObserve[11];                 // @0x8205EFD4
        static const s32  miNumEventsObserved;                   // == 11
        static const char KAC_MENU_OPTIONS_COMPONENT[9];         // "MenuItem"
        static const char KAC_CREATE_GAME_TOGGLE_COMPONENT[17];  // "CreateGameToggle"
        static const char KAC_ROUTE_INFO_NAME[10];               // "RouteInfo"
        static const char KAC_UP_ARROW_COMPONENT[13];            // "ArrowUp_anim"
        static const char KAC_DOWN_ARROW_COMPONENT[15];          // "ArrowDown_anim"
        static const char KAC_LOAD_HEADER_COMPONENT[16];         // "LoadHeader_anim"
        static const char KAC_LOAD_HEADER_TEXT_COMPONENT[15];    // "LoadHeaderText"
        static const char KAC_TITLE_TEXT_COMPONENT[11];          // "Title_text"
        static const char KAC_HELP_BAR_COMPONENT[7];             // "Button"
        static const char KAC_MAP_BORDER_ANIMATION_COMPONENT[10];// "Dirt_anim"
        static const char KAC_GAME_OPTIONS_TITLE_STRING_ID[27];  // "$PAGE_HEADING_CREATE_EVENT"
        static const char KAC_LOAD_OPTIONS_TITLE_STRING_ID[32];  // "$PAGE_HEADING_LOAD_GAME_OPTIONS"
        static const char KAC_CREATED_OPTIONS_STRING_ID[28];     // "$ONLINE_GAME_OPTION_CREATED"
        static const char KAC_RECENT_OPTIONS_STRING_ID[27];      // "$ONLINE_GAME_OPTION_RECENT"
        static const char KPC_SLOT_STRING_FORMAT_ID[24];         // "ONLINE_GAME_OPTION_SLOT"
        static const char KPC_SLOT_STRING_ID[28];                // "$ONLINE_GAME_OPTION_SLOT_%d"
        static const char* const KPC_ARROW_ANIMATION_STATES[3];        // @0x82F2683C
        static const char* const KPC_LOAD_HEADER_ANIMATION_STATES[3];  // @0x82F26848
        static const char* const KPC_MAP_BORDER_ANIMATION_STATES[2];   // @0x82F26854
        static const CreateMatchOption::EOption KAE_COMMON_OPTIONS[2];                // @0x8205F14C
        static const CreateMatchOption::EOption KAE_RACE_MODE_OPTIONS[5];             // @0x8205F154
        static const CreateMatchOption::EOption KAE_ROAD_RAGE_MODE_OPTIONS[6];        // @0x8205F168
        static const CreateMatchOption::EOption KAE_BURNING_HOME_RUN_MODE_OPTIONS[8]; // @0x8205F180
        static const CreateMatchOption::EOption KAE_DEFAULT_MODE_OPTIONS[5];          // @0x8205F1A0 (no DWARF name; modes 12/14/17)
        static const CreateMatchOption::EOption* const KAP_GAME_MODE_OPTION_DATA[6];  // @0x82F26824
        static const HelpBarItem KA_HELPBAR_ITEMS[4];                                 // @0x8205F1B4

        // ---- data members (DWARF order; the X360 offsets are documentation) ---------
        ESubState                 meSubState;            // X360 +56
        MenuComponent             mMenuOptions;          // +64    "MenuItem" (6 load slots)
        MenuToggleGroupVarSize<5> mCreateGameToggles;    // +4352  "CreateGameToggle"
        HelpBar                   mHelpBar;              // +23616 "Button" (3 items)
        AnimationComponent        mUpArrowAnimator;      // +31376 "ArrowUp_anim"
        AnimationComponent        mDownArrowAnimator;    // +31516 "ArrowDown_anim"
        AnimationComponent        mLoadHeaderAnimator;   // +31656 "LoadHeader_anim"
        AnimationComponent        mMapBorderAnimator;    // +31796 "Dirt_anim"
        TextField                 mLoadHeaderText;       // +31936 "LoadHeaderText"
        TextField                 mTitleText;            // +32232 "Title_text"
        GuiNetworkRouteInfo       mRouteInfoDisplay;     // +32528 "RouteInfo"
        GuiEventNetworkGameParams mGameOptions;          // +41152 (480 B, pointer-free)
        Array<CreateMatchOption, 75> maOptions;          // +41632 (count word @ +42232)
        const CreateMatchOption::EOption* mpCommonOptions; // +42236
        GuiCache*                 mpGuiCache;            // +42240
        EOptionsToLoad            meOptionsToLoad;       // +42244
        s32                       miCurrentRound;        // +42248
        s32                       miStartItem;           // +42252
        bool                      mbIsCreating;          // +42256 (byte)
    };
}
