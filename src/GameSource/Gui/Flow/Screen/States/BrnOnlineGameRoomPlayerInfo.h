#pragma once

// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h
//   TU: GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.cpp (50 functions)
//
// The online game-room "player info" screen state: the in-lobby tabbed screen that
// shows the player list (8 rows x 8 status icons), the per-player stats panel, the
// route/event info panel, the pause menu, the freeburn-challenge list, the settings
// toggles and the view-event page -- one ESubState machine over all of them. Derives
// from CrashNavMap (the map page IS the base class; E_SUBSTATE_MAP forwards to it).
//
// CLASS SHAPE + MEMBER ORDER: DecFIGS DWARF
//   references/DecFIGS/dwarfdump/GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h
//   (struct BrnGui::OnlineGameRoomPlayerInfo : public BrnGui::CrashNavMap)
// MEMBER PLACEMENT: X360 ARTIST asm (this TU's dossier + raw asm, wave H). X360
// byte offsets (documentation; the x64 host layout is name-based):
//   +24928 maIcons[8]           (8 icon rows; IconComponent stride 148, row 1184)
//   +34400 mMessageText         (TextField, 296)     "MessageText"
//   +34696 mViewEventOptions    (MenuComponent, 4288) "MenuItem"
//   +38984 mPlayerNameComponent (MenuComponent)      "PlayerName"
//   +43272 mOptionComponent     (MenuComponent)      "Option" (player options popup)
//   +47560 mPauseComponent      (MenuComponent)      "Option" (pause / security menu)
//   +51848 mChallengeToggle     (MenuToggle, 3736)   "optionToggle_0"
//   +55584 mSettingToggle       (MenuToggleGroupVarSize<4>, 15520) "optionToggle"
//   +71104 mPlayerStatsDisplay  (GuiNetworkPlayerStats, 4592) "PlayerStats"
//   +75696 mRouteInfoDisplay    (GuiNetworkRouteInfo, 8624)   "RouteInfo"
//   +84320 mOptionsAnimation                (AnimationComponent, 140) "OptionsAnimation"
//   +84460 mChallengeListUpArrowAnimation   (140) "UpArrowAnimation"
//   +84600 mChallengeListDownArrowAnimation (140) "DownArrowAnimation"
//   +84740 mPauseButtonPromptsAnimation     (140) "PauseButtonPrompts"
//   +84880 mPlayersButtonPromptsAnimation   (140) "PlayersButtonPrompts"
//   +85024 mChallengeListComponent (ChallengeListComponent, 2440) "ChallengeList"
//   +87464 mOverlayCompleteEvent (stored overlay-complete PAYLOAD: CgsID + s32)
//   +87480 mCrashNavOptionsData  (CrashNavOptionsData, 24)
//   +87504 meSubState            +87508 miCurrentRoundDisplayed
//   +87512 mSelectedPlayerID     +87516 mpGuiCache
//   +87520 mfTimeUntilNextEATrack  +87524 mfTimeToDisableNextEATrack
//   +87528 miDelayFrameCount
//   +87532 mbFirstMapUpdate  +87533 mbShowSplashScreen  +87534 mbNeedToClearSplashScreen
//   +87535 mbKicked  +87536 mbShownFromPause  +87537 mbHighlightedNewPlayer
//   +87538 mbSettingsChanged
//   +87540 mHelpItemAccept (HelpItem, 428)  "HelpItemAccept_mc"
//   X360 sizeof == 87968 (BrnScreenFlow's ON_GAME_ROOM pool-state size).
//
// Statics (tables/string ids) are defined in BrnOnlineGameRoomPlayerInfo.cpp with the
// X360 rodata values (dumped, boundaries observed: scratchpad/waveB/ogrpi_rodata.txt +
// scratchpad/waveH/ogrpi_rodata2.txt).
//
// In-queue handler params are `const CgsModule::Event*` and the bodies cast to
// file-local payload views -- the queue delivers HEADER-STRIPPED payloads, so the
// DWARF's typed event-pointer signatures (noted per method) would mislead field
// offsets by 12; same idiom as the committed BrnInGame.cpp / BrnCarSelectMain waves.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h" // CgsGui::sResourceTuple
#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h"          // BrnGui::CrashNavMap (base) + AnimationComponent stopgap
#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavOptions.h"      // BrnGui::CrashNavOptionsData (by value)
#include "GameSource/Gui/BrnGuiTextField.h"                            // BrnGui::TextField (by value)
#include "GameSource/Gui/BrnChallengeListComponent.h"                  // BrnGui::ChallengeListComponent (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnIcon.h"             // BrnGui::IconComponent (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuComponent.h"    // BrnGui::MenuComponent (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuToggle.h"       // BrnGui::MenuToggle (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuToggleGroup.h"  // BrnGui::MenuToggleGroupVarSize<4> (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnHelpItem.h"         // BrnGui::HelpItem (by value)
#include "GameSource/Gui/Flow/Screen/Components/BrnGuiNetworkPlayerStats.h" // BrnGui::GuiNetworkPlayerStats (by value)
#include "GameSource/Gui/Flow/Screen/Components/BrnGuiNetworkRouteInfo.h"   // BrnGui::GuiNetworkRouteInfo (by value)
#include "GameSource/Gui/BrnGuiCache.h"                                // BrnGui::GuiCache (ShowSettingsOptions inline)

#include <cstddef>   // offsetof (layout pins in _AssertLayout)

namespace BrnGui
{
    // DWARF BrnOnlineGameRoomPlayerInfo.h:54.
    struct OnlineGameRoomPlayerInfo : public CrashNavMap
    {
        // ---- ESubState (DWARF h:113; the +87504 machine) ----------------------------
        enum ESubState
        {
            E_SUBSTATE_LOADING_SCREEN                = 0,
            E_SUBSTATE_LOADING_PAUSE_SCREEN          = 1,
            E_SUBSTATE_LOADING_PAUSE_COMPONENTS      = 2,
            E_SUBSTATE_LOADING_CHALLENGES_SCREEN     = 3,
            E_SUBSTATE_LOADING_CHALLENGES_COMPONENTS = 4,
            E_SUBSTATE_LOADING_SETTINGS_SCREEN       = 5,
            E_SUBSTATE_LOADING_SETTINGS_COMPONENTS   = 6,
            E_SUBSTATE_LOADING_VIEW_EVENT_SCREEN     = 7,
            E_SUBSTATE_LOADING_VIEW_EVENT_COMPONENTS = 8,
            E_SUBSTATE_LOADING_COMPONENTS            = 9,
            E_SUBSTATE_MAIN                          = 10,
            E_SUBSTATE_FREE_BURNING                  = 11,
            E_SUBSTATE_LEAVING                       = 12,
            E_SUBSTATE_PLAYER_OPTIONS                = 13,
            E_SUBSTATE_PAUSE                         = 14,
            E_SUBSTATE_LOADING_MAP_SCREEN            = 15,
            E_SUBSTATE_LOADING_MAP_SCREEN_COMPONENTS = 16,
            E_SUBSTATE_MAP                           = 17,
            E_SUBSTATE_CHALLENGES                    = 18,
            E_SUBSTATE_CONTROLLER_DISABLE_DELAY      = 19,
            E_SUBSTATE_SECURITY_OPTIONS              = 20,
            E_SUBSTATE_SETTINGS                      = 21,
            E_SUBSTATE_VIEW_EVENT                    = 22,
            E_SUBSTATE_COUNT                         = 23,
        };

        // ---- one player row's status icons (DWARF h:143; X360 row stride 1184) ------
        struct Icons
        {
            IconComponent mMarkedManIcon;           // h:145  row+0    "MarkedMan_%d"
            IconComponent mRevengeStatusIcon;       // h:146  row+148  "RevengeStatus_%d"
            IconComponent mReadyStatusIcon;         // h:147  row+296  "ReadyStatus_%d"
            IconComponent mVideoStatusIcon;         // h:148  row+444  "VideoStatus_%d"
            IconComponent mVoipStatusIcon;          // h:149  row+592  "VoipStatus_%d"
            IconComponent mTeamStatusIcon;          // h:150  row+740  "Team_%d"
            IconComponent mGameConnectionTypeIcon;  // h:151  row+888  "GameConnection_%d"
            IconComponent mVoipConnectionTypeIcon;  // h:152  row+1036 "VoipConnection_%d"
        };

        // The stored overlay-complete record @X360 +87464. DWARF member type is
        // GuiOverlayCompleteEvent, but what the X360 stores/compares is the
        // HEADER-STRIPPED payload {CgsID overlay id @+0, leave method @+8} (the
        // `std`/`lwz 8` pair in HandleOverlayComplete; same payload view the committed
        // BrnInGame.cpp uses for event 189).
        struct OverlayCompleteData
        {
            CgsID mOverlayId;      // +0  (CgsIDCompress'd overlay name)
            s32   miLeaveMethod;   // +8  (GuiOverlayCompleteEvent::LeaveMethod)
        };

        static const s32 KI_MAX_PLAYERS = 8;   // icon rows / lobby slots (asserted in ShowPlayerList)

        OnlineGameRoomPlayerInfo();   // DWARF-attested; X360 body is compiler-synthesised
                                      // (the screen-flow pool ctor). Not one of this TU's
                                      // 50 ledger functions -- no definition here.

        // ---- lifecycle virtuals (this TU's ledger functions) ------------------------
        virtual void Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm);   // @0x82484B30 cpp:330
        virtual void OnEnter();                                           // @0x824A36E0 cpp:353
        virtual void OnLeave();                                           // @0x824A3C18 cpp:855
        virtual void Update();                                            // @0x824B0770 cpp:464
        // DWARF h:104 -- hands out the LOBBY tuple table (the base-class override slot).
        // Not in this TU's ledger (folded/foreign); declared for the vtable shape.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const;

    private:
        // DWARF cpp:1223 -- virtual per the DWARF. Body owned by a FOREIGN ledger TU
        // (`reviewed` there; defined nowhere yet -- see the wave-H spec's absent list).
        virtual void CheckForComponents(const CgsModule::Event* lpEvent);

        // ---- this TU's 50 ledger functions (X360 addresses; cpp:<line> = DWARF) -----
        bool ShowPlayerOptions();                                   // @0x8248C4D0 cpp:3303
        void ShowPlayerList(s32 liSelectedPlayerID);                // @0x824A5968 cpp:3371 (NetworkPlayerID)
        void HighlightNewPlayer();                                  // @0x824991E0 cpp:3494
        void CheckForCompletedLoads();                              // @0x824A3FB8 cpp:965
        void HandleControllerInputPressed(const CgsModule::Event* lpEvent);   // @0x824AF018 cpp:1282
        void HandleControllerInputReleased(const CgsModule::Event* lpEvent);  // @0x824A45B0 cpp:1361
        // (DWARF param: const GuiEventControllerInputPressed* -- see the header banner)
        void HandleControllerInputMainSubState(const CgsModule::Event* lpEvent);            // @0x824A4670 cpp:1392
        void HandleControllerInputFreeBurnSubState(const CgsModule::Event* lpEvent);        // @0x824A4A08 cpp:1520
        void HandleControllerInputPlayerOptionsSubState(const CgsModule::Event* lpEvent);   // @0x82498120 cpp:1632
        void HandleControllerInputSecurityOptionsSubState(const CgsModule::Event* lpEvent); // @0x824AD120 cpp:1953
        void HandleControllerInputSettingsSubState(const CgsModule::Event* lpEvent);        // @0x824AD248 cpp:2004
        void HandleControllerInputViewEventSubState(const CgsModule::Event* lpEvent);       // @0x824A4F28 cpp:2130
        void HandleControllerInputReleasedMapSubState(const CgsModule::Event* lpEvent);     // @0x824982A8 cpp:2343
        void HandleLaunchQuestion(s32 liLeaveMethod);               // @0x82498038 cpp:1587 (GuiOverlayCompleteEvent::LeaveMethod)
        void HandleLeftGameEvent(const CgsModule::Event* lpEvent);          // @0x82498358 cpp:2392
        void HandlePlayerLobbyListEvent(const CgsModule::Event* lpEvent);   // @0x824AD560 cpp:2467
        void HandleAskToLaunchEvent(const CgsModule::Event* lpEvent);       // @0x82498698 cpp:2527
        void HandleLaunchedEvent(const CgsModule::Event* lpEvent);          // @0x82498A10 cpp:2599
        void HandlePlayerStatsEvent(const CgsModule::Event* lpEvent);       // @0x82484BA0 cpp:2636
        void HandleGameParamsChangedEvent(const CgsModule::Event* lpEvent); // @0x824A52F8 cpp:2670
        void HandleGuiCacheEvent(const CgsModule::Event* lpEvent);          // @0x824A3DD8 cpp:912  (DWARF: const GuiEventCache*)
        void HandleInviteFailed(const CgsModule::Event* lpEvent);           // @0x82498BA0 cpp:2721 (DWARF: const GuiEventInviteFailed*)
        void HandleOverlayComplete(const CgsModule::Event* lpEvent);        // @0x824A56A0 cpp:2884 (DWARF: const GuiOverlayCompleteEvent*)
        void HandleSplashScreenRequests(const CgsModule::Event* lpEvent);   // @0x824A54B8 cpp:2758 (DWARF: const GuiEventNetworkSplashEvent*)
        void HandleLeaveGameRequest();                              // @0x82498C98 cpp:2844
        void SetExpectedLobbyComponents();                          // @0x82484E78 cpp:3879
        void SetExpectedPauseComponents();                          // @0x82484FE0 cpp:3917
        void ShowPauseOptions();                                    // @0x8248BFE0 cpp:3033
        void ShowSecurityOptions();                                 // @0x8248C2F8 cpp:3121
        void HideMenusAndReturnToHUD();                             // @0x82499B80 cpp:3732
        void ShowPauseScreen();                                     // @0x82484DE8 cpp:3792
        void SetExpectedChallengesComponents();                     // @0x82485078 cpp:3935
        void ShowSettingsScreen();                                  // @0x8249A780 cpp:4268
        void SetExpectedSettingsComponents();                       // @0x8248C628 cpp:4312
        void SetExpectedViewEventComponents();                      // @0x824851A8 cpp:4436
        void ShowViewEventOptions();                                // @0x8249AB90 cpp:4402
        void PerformPauseOption(s32 lePauseOption);                 // @0x824A5F70 cpp:3956 (GuiEventPerformOnlinePauseOption::EPauseOptions)
        void UnloadLobbyResources();                                // @0x8249A090 cpp:4090
        void UnloadChallengeResources();                            // @0x8249A170 cpp:4114
        void UnloadMapResources();                                  // @0x8249A280 cpp:4140
        void UnloadPauseResources();                                // @0x8249A348 cpp:4161
        void UnloadSettingsResources();                             // @0x8249A410 cpp:4182
        void ClearSplashScreenOverlay(const OverlayCompleteData* lpOverlayCompleteEvent); // @0x82498E00 cpp:2974
        void UpdateSoundSettings();                                 // @0x8249AD00 cpp:4590
        void TriggerSound(s32 leGameInputAction);                   // @0x8249ADB0 cpp:4631 (EGameInputActions)
        void GetPlayerOptions(u64* lpau64OptionIds, s32* lpiNumOptions);   // @0x82484CC0 cpp:3263

        // ---- methods of THIS class owned by FOREIGN ledger TUs (all `reviewed` in the
        //      ledger, none defined anywhere in the tree yet -- the wave-H spec's
        //      absent-callee list). Declared so this TU's bodies can call them. --------
        void HideOptions();                                          // cpp:3357
        bool ShouldShowButton();                                     // cpp:1757 (BrnSelectableGroup.h TU)
        void HandleControllerInputPauseSubState(const CgsModule::Event* lpEvent);      // cpp:1693 (BrnSelectableGroup.h TU)
        void HandleControllerInputPressedMapSubState(const CgsModule::Event* lpEvent); // cpp:2238 (CgsStrStream.h TU)
        void HandleControllerInputChallengesSubState(const CgsModule::Event* lpEvent); // cpp:1779 (ChallengeList.h TU)
        void HandleLaunchingEvent(const CgsModule::Event* lpEvent);  // cpp:2547 (BrnGuiEventTypeDefs.h TU)
        void HandleLeavingGameFailedEvent(const CgsModule::Event* lpEvent);   // cpp:2451 (DWARF: const GuiEventNetworkLeavingGameFailed*)
        void HandlePerformOnlinePauseOption(const CgsModule::Event* lpEvent); // cpp:3016 (DWARF: const GuiEventPerformOnlinePauseOption*)
        void ShowLobby();                                            // cpp:3532 (CgsGuiEvent.h TU)
        void HideLobby();                                            // cpp:3565 (CgsGuiEvent.h TU)
        void HideChallengePage();                                    // cpp:3616 (CgsGuiEvent.h TU)
        void HideSettingsPage();                                     // cpp:4236 (CgsGuiEvent.h TU)
        void HideViewEventPage();                                    // cpp:4329 (CgsGuiEvent.h TU)
        // cpp:3846 (CgsStrStream.h TU). NO parameters: 0x824A5650 emits only `mr r3, r29`
        // before the bl -- r4 is never set on that path and the callee @0x82499F00 never
        // reads it. DWARF agrees. (The old comment here claimed the payload was passed
        // through; both the asm and DWARF contradict it.)
        void ShowSplashScreen();
        void HideHUDAndEnterMenus();                                 // cpp:3694 (CgsGuiEvent.h TU)
        void ShowMapScreen();                                        // cpp:3814 (CgsGuiEvent.h TU)
        void ShowChallengesScreen();                                 // cpp:3648 (CgsGuiEvent.h TU)
        void ShowChallengesOptions();                                // cpp:3191 (CgsStrStream.h TU)
        void ShowViewEventScreen();                                  // cpp:4372 (CgsGuiEvent.h TU)
        void HandleSettingChanged();                                 // cpp:4455 (CgsStrStream.h TU)
        void UpdateVoipSettings();                                   // cpp:4610 (no X360 ledger entry; folded)
        void UnloadViewEventResources();                             // cpp:4203 (CgsGuiEvent.h TU)

        // @0x82485140 cpp:4297 -- ledger home is the BrnCrashNavOptions.h TU (its old
        // file-local "slice" class is retired in favour of this real home; the inline
        // body is kept VERBATIM at the class it belongs to). Refresh the options model
        // from the profile and bind it onto the 4-slot toggle group.
        void ShowSettingsOptions()
        {
            mCrashNavOptionsData.SetFromProfile(mpGuiCache->GetOptionsDataProfile());
            mCrashNavOptionsData.SetUpComponent<4>(&mSettingToggle);
        }

        // ---- statics (DWARF names; definitions + X360 rodata values in the .cpp) ----
        static const s32 maiEventToObserve[33];             // cpp:41  @0x8205E9E8
        static const s32 miNumEventsObserved;               // cpp:81  == 33 (@0x8205E9E4)
        static const CgsGui::sResourceTuple maPauseResourceTuplesToLoad[1];     // cpp:83  @0x8205EA6C
        static const s32 miNumPauseResourcesToLoad;                             // == 1
        static const CgsGui::sResourceTuple maLobbyResourceTuplesToLoad[2];     // cpp:90  @0x8205EA78
        static const s32 miNumLobbyResourcesToLoad;                             // == 2
        static const CgsGui::sResourceTuple maChallengeResourceTuplesToLoad[1]; // cpp:98  @0x8205EA8C
        static const s32 miNumChallengeResourcesToLoad;                         // == 1
        static const CgsGui::sResourceTuple maSettingsResourceTuplesToLoad[1];  // cpp:105 @0x8205EA98
        static const s32 miNumSettingsResourcesToLoad;                          // == 1
        static const CgsGui::sResourceTuple maViewEventResourceTuplesToLoad[2]; // cpp:112 @0x8205EAA4
        static const s32 miNumViewEventResourcesToLoad;                         // == 2

        // Option-count bounds (DWARF cpp:296..301). The two PAUSE bounds are 11 on
        // ARTIST (the assert compare @0x8248C0xx fires on liOptionIndex > 11); the
        // Dec-07 DWARF said 10 -- the merge window added the security option row.
        static const s32 KI_MAX_PLAYER_OPTIONS            = 4;
        static const s32 KI_MAX_PAUSE_OPTIONS             = 11;   // DWARF 10; X360-attested 11
        static const s32 KI_MAX_DISPLAYABLE_OPTIONS       = 9;
        static const s32 KI_MAX_DISPLAYABLE_PAUSE_OPTIONS = 11;   // DWARF 10; X360-attested 11
        static const s32 KI_CONTROLLER_DISABLE_DELAY_FRAME_COUNT = 5;

        // Apt component names (DWARF cpp:122..144; the OnEnter ctor-call literals).
        static const char KAC_TEXTFIELD_COMPONENT[12];                  // "MessageText"
        static const char KAC_PLAYER_NAME_COMPONENT[11];                // "PlayerName"
        static const char KAC_OPTION_COMPONENT[7];                      // "Option"
        static const char KAC_PLAYER_STATS_COMPONENT[12];               // "PlayerStats"
        static const char KAC_ROUTE_INFO_COMPONENT[10];                 // "RouteInfo"
        static const char KAC_MARKED_MAN_ICON_COMPONENT[13];            // "MarkedMan_%d"
        static const char KAC_REVENGE_STATUS_ICON_COMPONENT[17];        // "RevengeStatus_%d"
        static const char KAC_READY_STATUS_ICON_COMPONENT[15];          // "ReadyStatus_%d"
        static const char KAC_VIDEO_STATUS_ICON_COMPONENT[15];          // "VideoStatus_%d"
        static const char KAC_VOIP_STATUS_ICON_COMPONENT[14];           // "VoipStatus_%d"
        static const char KAC_TEAM_STATUS_ICON_COMPONENT[8];            // "Team_%d"
        static const char KAC_GAME_CONNECTION_TYPE_ICON_COMPONENT[18];  // "GameConnection_%d"
        static const char KAC_VOIP_CONNECTION_TYPE_ICON_COMPONENT[18];  // "VoipConnection_%d"
        static const char KAC_OPTIONS_ANIMATION_COMPONENT[17];          // "OptionsAnimation"
        static const char KAC_CHALLENGE_TOGGLE_COMPONENT[15];           // "optionToggle_0"
        static const char KAC_CHALLENGE_LIST_COMPONENT[14];             // "ChallengeList"
        static const char KAC_CHALLENGE_LIST_UP_ARROW_ANIMATION_COMPONENT[17];   // "UpArrowAnimation"
        static const char KAC_CHALLENGE_LIST_DOWN_ARROW_ANIMATION_COMPONENT[19]; // "DownArrowAnimation"
        static const char KAC_PAUSE_BUTTON_PROMPTS_ANIMATION_COMPONENT[19];      // "PauseButtonPrompts"
        static const char KAC_PLAYERS_BUTTON_PROMPTS_ANIMATION_COMPONENT[21];    // "PlayersButtonPrompts"
        static const char KAC_SETTING_TOGGLE_COMPONENT[13];             // "optionToggle"
        static const char KAC_VIEW_EVENT_MENU_OPTIONS_COMPONENT[9];     // "MenuItem"

        // Icon state-id tables (DWARF cpp:147..212; X360 @0x8205EC18.. -- dumped).
        static const char* const KAP_MARKED_MAN_STATE_ID[2];        // {visible, invisible}
        static const char* const KAP_REVENGE_STATUS_STATE_ID[4];    // {ahead, equal, behind, invisible}
        static const char* const KAP_READY_STATUS_STATE_ID[5];      // {host, invisible, invisible, racing, invisible}
        static const char* const KAP_VIDEO_STATUS_STATE_ID[5];      // {nocamera, available, inuse, available, invisible}
        static const char* const KAP_VOIP_STATUS_STATE_ID[4];       // {noheadset, available, talking, invisible}
        static const char* const KAP_TEAM_STATUS_STATE_ID[3];       // {none, red, blue}
        static const char* const KAP_CONNECTION_TYPE_STATE_ID[3];   // {direct, gameserver, invisible}

        // Animation state pairs (DWARF cpp:194..206; X360 @0x82F266EC.. -- dumped).
        static const char* const KAPC_ANIMATION_STATES[2];                       // {visible, invisible}
        static const char* const KAPC_PAUSE_BUTTON_PROMPT_ANIMATION_STATES[2];   // {return, backup}
        static const char* const KAPC_PLAYERS_BUTTON_PROMPT_ANIMATION_STATES[2]; // {visible, invisible}

        // String-id tables (DWARF cpp:234..303; X360 @0x82F26704.. -- dumped).
        static const char* const KAPC_CHALLENGE_TOGGLE_ITEM_STRING_IDS[7];  // {"2".."8"} @0x82F26704
        static const char* const KPAC_PAUSE_OPTIONS_STRING_ID[14];          // @0x82F26720
        static const char* const KPC_LAUNCH_FAILED_STRINGIDS[7];            // @0x82F26758
        static const char* const KAPC_KICKED_POPUP[5];                      // @0x82F26774

        // DWARF cpp:274 KPAC_MODE_STRINGS[17] / h:227 KPAC_HOST_STATUS_STRING_IDS:
        // consumed only by the foreign-owned inlines (ShowChallengesOptions et al), not
        // by this TU's 50 -- left undeclared until those bodies land with their values.

        // Tab-title / message string ids (DWARF cpp:212..232; literals in the asm).
        static const char KAC_PLAYER_INFO_STRING_ID[34];      // "$ONLINE_GAME_ROOM_TAB_PLAYER_INFO"
        static const char KAC_EVENT_INFO_STRING_ID[33];       // "$ONLINE_GAME_ROOM_TAB_EVENT_INFO"
        static const char KAC_EVENT_MAP_STRING_ID[32];        // "$ONLINE_GAME_ROOM_TAB_EVENT_MAP"
        static const char KAC_LEAVING_GAME_STRING_ID[14];     // "$LEAVING_GAME"
        static const char KAC_ASK_TO_LAUNCH_STRING_ID[24];    // (cpp:225; the launch-question id)
        static const char KAC_MARK_PLAYER_STRING_ID[34];      // "$ONLINE_PLAYER_OPTION_MARK_PLAYER"
        static const char KAC_UNMARK_PLAYER_STRING_ID[36];    // "$ONLINE_PLAYER_OPTION_UNMARK_PLAYER"
        static const char KAC_SHOW_GAMERCARD_STRING_ID[44];   // "$ONLINE_PLAYER_OPTION_SHOW_GAMERCARD_PLAYER"
        static const char KAC_SUBMIT_FEEBDACK_STRING_ID[45];  // "$ONLINE_PLAYER_OPTION_SUBMIT_FEEDBACK_PLAYER" (sic DWARF)
        static const char KAC_KICK_PLAYER_STRING_ID[34];      // "$ONLINE_PLAYER_OPTION_KICK_PLAYER"
        static const char KAC_CHALLENGE_TOGGLE_TITLE_STRING_ID[26];  // (cpp:231)
        static const char KAC_VIEW_EVENT_MENU_ITEM_STRING_ID[36];    // "$ONLINE_LOBBY_OPTION_CHANGE_OPTIONS"
        static const char KAC_HELPITEM_ACCEPT[18];            // "$CAPS_BUTTON_ACCEPT" family (cpp:312)

        // ---- data members (DWARF order; X360 offsets in the header banner) ----------
        Icons              maIcons[KI_MAX_PLAYERS];          // h:233  X360 +24928
        TextField          mMessageText;                     // h:234  X360 +34400
        MenuComponent      mViewEventOptions;                // h:235  X360 +34696
        MenuComponent      mPlayerNameComponent;             // h:236  X360 +38984
        MenuComponent      mOptionComponent;                 // h:237  X360 +43272
        MenuComponent      mPauseComponent;                  // h:238  X360 +47560
        MenuToggle         mChallengeToggle;                 // h:239  X360 +51848
        MenuToggleGroupVarSize<4> mSettingToggle;            // h:240  X360 +55584 (DWARF type
                                                             //   MenuToggleGroup; the X360 calls the
                                                             //   VarSize<4> instantiation throughout)
        GuiNetworkPlayerStats mPlayerStatsDisplay;           // h:241  X360 +71104
        GuiNetworkRouteInfo   mRouteInfoDisplay;             // h:242  X360 +75696
        AnimationComponent mOptionsAnimation;                // h:243  X360 +84320
        AnimationComponent mChallengeListUpArrowAnimation;   // h:244  X360 +84460
        AnimationComponent mChallengeListDownArrowAnimation; // h:245  X360 +84600
        AnimationComponent mPauseButtonPromptsAnimation;     // h:246  X360 +84740
        AnimationComponent mPlayersButtonPromptsAnimation;   // h:247  X360 +84880
        ChallengeListComponent mChallengeListComponent;      // h:248  X360 +85024
        OverlayCompleteData mOverlayCompleteEvent;           // h:249  X360 +87464
        CrashNavOptionsData mCrashNavOptionsData;            // h:251  X360 +87480
        ESubState          meSubState;                       // h:253  X360 +87504
        s32                miCurrentRoundDisplayed;          // h:255  X360 +87508
        s32                mSelectedPlayerID;                // h:257  X360 +87512 (GuiEventNetworkLaunching::NetworkPlayerID, typedef s32; -1 == none)
        GuiCache*          mpGuiCache;                       // h:259  X360 +87516
        f32                mfTimeUntilNextEATrack;           // h:261  X360 +87520
        f32                mfTimeToDisableNextEATrack;       // h:262  X360 +87524
        s32                miDelayFrameCount;                // h:264  X360 +87528
        bool               mbFirstMapUpdate;                 // h:266  X360 +87532
        bool               mbShowSplashScreen;               // h:267  X360 +87533
        bool               mbNeedToClearSplashScreen;        // h:268  X360 +87534
        bool               mbKicked;                         // h:269  X360 +87535
        bool               mbShownFromPause;                 // h:270  X360 +87536
        bool               mbHighlightedNewPlayer;           // h:271  X360 +87537
        bool               mbSettingsChanged;                // h:273  X360 +87538
        HelpItem           mHelpItemAccept;                  // h:274  X360 +87540

        // Never called -- complete-class context so offsetof works on the private
        // members. Pins are RELATIVE deltas inside pointer-free scalar runs (pointers
        // widen on the x64 gate, so absolute X360 offsets cannot hold host-side).
        static void _AssertLayout()
        {
            // run A: overlay record .. selected player id (X360 +87464..+87512; the
            // deltas below are the exact X360 deltas -- no pointer inside the run)
            static_assert(offsetof(OnlineGameRoomPlayerInfo, mCrashNavOptionsData) - offsetof(OnlineGameRoomPlayerInfo, mOverlayCompleteEvent) == 16, "run A order");
            static_assert(offsetof(OnlineGameRoomPlayerInfo, meSubState)              - offsetof(OnlineGameRoomPlayerInfo, mOverlayCompleteEvent) == 40, "run A order");
            static_assert(offsetof(OnlineGameRoomPlayerInfo, miCurrentRoundDisplayed) - offsetof(OnlineGameRoomPlayerInfo, mOverlayCompleteEvent) == 44, "run A order");
            static_assert(offsetof(OnlineGameRoomPlayerInfo, mSelectedPlayerID)       - offsetof(OnlineGameRoomPlayerInfo, mOverlayCompleteEvent) == 48, "run A order");
            // run B: the scalar tail after the (widening) mpGuiCache pointer
            // (X360 +87520..+87538; exact X360 deltas within the run)
            static_assert(offsetof(OnlineGameRoomPlayerInfo, mfTimeToDisableNextEATrack) - offsetof(OnlineGameRoomPlayerInfo, mfTimeUntilNextEATrack) == 4,  "run B order");
            static_assert(offsetof(OnlineGameRoomPlayerInfo, miDelayFrameCount)          - offsetof(OnlineGameRoomPlayerInfo, mfTimeUntilNextEATrack) == 8,  "run B order");
            static_assert(offsetof(OnlineGameRoomPlayerInfo, mbFirstMapUpdate)           - offsetof(OnlineGameRoomPlayerInfo, mfTimeUntilNextEATrack) == 12, "run B order");
            static_assert(offsetof(OnlineGameRoomPlayerInfo, mbShowSplashScreen)         - offsetof(OnlineGameRoomPlayerInfo, mfTimeUntilNextEATrack) == 13, "run B order");
            static_assert(offsetof(OnlineGameRoomPlayerInfo, mbNeedToClearSplashScreen)  - offsetof(OnlineGameRoomPlayerInfo, mfTimeUntilNextEATrack) == 14, "run B order");
            static_assert(offsetof(OnlineGameRoomPlayerInfo, mbKicked)                   - offsetof(OnlineGameRoomPlayerInfo, mfTimeUntilNextEATrack) == 15, "run B order");
            static_assert(offsetof(OnlineGameRoomPlayerInfo, mbShownFromPause)           - offsetof(OnlineGameRoomPlayerInfo, mfTimeUntilNextEATrack) == 16, "run B order");
            static_assert(offsetof(OnlineGameRoomPlayerInfo, mbHighlightedNewPlayer)     - offsetof(OnlineGameRoomPlayerInfo, mfTimeUntilNextEATrack) == 17, "run B order");
            static_assert(offsetof(OnlineGameRoomPlayerInfo, mbSettingsChanged)          - offsetof(OnlineGameRoomPlayerInfo, mfTimeUntilNextEATrack) == 18, "run B order");
            // component ORDER pins (relative ordering only -- component widths widen)
            static_assert(offsetof(OnlineGameRoomPlayerInfo, mMessageText) > offsetof(OnlineGameRoomPlayerInfo, maIcons),            "component order");
            static_assert(offsetof(OnlineGameRoomPlayerInfo, mSettingToggle) > offsetof(OnlineGameRoomPlayerInfo, mChallengeToggle), "component order");
            static_assert(offsetof(OnlineGameRoomPlayerInfo, mHelpItemAccept) > offsetof(OnlineGameRoomPlayerInfo, mbSettingsChanged), "component order");
        }
    };
}
