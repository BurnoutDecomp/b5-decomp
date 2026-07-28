#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- reconstructed from BURNOUT_X360_ARTIST.XEX
// (addresses in the header). WAVE H KEYSTONE SCAFFOLD: this file carries the TU's
// static-member DEFINITIONS (every value read from the X360 image with observed table
// boundaries -- scratchpad/waveB/ogrpi_rodata.txt + scratchpad/waveH/ogrpi_rodata2.txt
// + scratchpad/waveH/OnlineGameRoomPlayerInfo.spec.md). The 50 function bodies land in
// the wave-H implementation files (BrnOnlineGameRoomPlayerInfo_wH_XX.cpp) per the
// spec's group plan.
// ===================================================================================

namespace BrnGui
{
    // ---- observed-event table (X360 @0x8205E9E8; the count word 33 sits directly
    //      before it @0x8205E9E4 -- both dumped with the surrounding window) ---------
    const s32 OnlineGameRoomPlayerInfo::maiEventToObserve[33] =
    {
        8,   6,   7,   21,  273, 275, 53,  55,  57,  58,  44,
        50,  248, 257, 64,  244, 133, 189, 134, 269, 81,  229,
        320, 516, 213, 283, 202, 199, 406, 334, 344, 581, 85,
    };
    const s32 OnlineGameRoomPlayerInfo::miNumEventsObserved = 33;

    // ---- resource-tuple tables (X360 @0x8205EA6C..0x8205EAB8, contiguous; the whole
    //      window was dumped so every count boundary is observed) --------------------
    const CgsGui::sResourceTuple OnlineGameRoomPlayerInfo::maPauseResourceTuplesToLoad[1] =
    {
        { 169, CgsGui::E_GUI_RESOURCETYPE_APT },                    // @0x8205EA6C
    };
    const s32 OnlineGameRoomPlayerInfo::miNumPauseResourcesToLoad = 1;

    const CgsGui::sResourceTuple OnlineGameRoomPlayerInfo::maLobbyResourceTuplesToLoad[2] =
    {
        { 167, CgsGui::E_GUI_RESOURCETYPE_APT }, { 190, CgsGui::E_GUI_RESOURCETYPE_APT },        // @0x8205EA78
    };
    const s32 OnlineGameRoomPlayerInfo::miNumLobbyResourcesToLoad = 2;

    const CgsGui::sResourceTuple OnlineGameRoomPlayerInfo::maChallengeResourceTuplesToLoad[1] =
    {
        { 187, CgsGui::E_GUI_RESOURCETYPE_APT },                    // @0x8205EA8C
    };
    const s32 OnlineGameRoomPlayerInfo::miNumChallengeResourcesToLoad = 1;

    const CgsGui::sResourceTuple OnlineGameRoomPlayerInfo::maSettingsResourceTuplesToLoad[1] =
    {
        { 141, CgsGui::E_GUI_RESOURCETYPE_APT },                    // @0x8205EA98
    };
    const s32 OnlineGameRoomPlayerInfo::miNumSettingsResourcesToLoad = 1;

    const CgsGui::sResourceTuple OnlineGameRoomPlayerInfo::maViewEventResourceTuplesToLoad[2] =
    {
        { 188, CgsGui::E_GUI_RESOURCETYPE_APT }, { 191, CgsGui::E_GUI_RESOURCETYPE_APT },        // @0x8205EAA4
    };
    const s32 OnlineGameRoomPlayerInfo::miNumViewEventResourcesToLoad = 2;

    // ---- apt component names (the OnEnter ctor-call literals) ----------------------
    const char OnlineGameRoomPlayerInfo::KAC_TEXTFIELD_COMPONENT[12]                 = "MessageText";
    const char OnlineGameRoomPlayerInfo::KAC_PLAYER_NAME_COMPONENT[11]               = "PlayerName";
    const char OnlineGameRoomPlayerInfo::KAC_OPTION_COMPONENT[7]                     = "Option";
    const char OnlineGameRoomPlayerInfo::KAC_PLAYER_STATS_COMPONENT[12]              = "PlayerStats";
    const char OnlineGameRoomPlayerInfo::KAC_ROUTE_INFO_COMPONENT[10]                = "RouteInfo";
    const char OnlineGameRoomPlayerInfo::KAC_MARKED_MAN_ICON_COMPONENT[13]           = "MarkedMan_%d";
    const char OnlineGameRoomPlayerInfo::KAC_REVENGE_STATUS_ICON_COMPONENT[17]       = "RevengeStatus_%d";
    const char OnlineGameRoomPlayerInfo::KAC_READY_STATUS_ICON_COMPONENT[15]         = "ReadyStatus_%d";
    const char OnlineGameRoomPlayerInfo::KAC_VIDEO_STATUS_ICON_COMPONENT[15]         = "VideoStatus_%d";
    const char OnlineGameRoomPlayerInfo::KAC_VOIP_STATUS_ICON_COMPONENT[14]          = "VoipStatus_%d";
    const char OnlineGameRoomPlayerInfo::KAC_TEAM_STATUS_ICON_COMPONENT[8]           = "Team_%d";
    const char OnlineGameRoomPlayerInfo::KAC_GAME_CONNECTION_TYPE_ICON_COMPONENT[18] = "GameConnection_%d";
    const char OnlineGameRoomPlayerInfo::KAC_VOIP_CONNECTION_TYPE_ICON_COMPONENT[18] = "VoipConnection_%d";
    const char OnlineGameRoomPlayerInfo::KAC_OPTIONS_ANIMATION_COMPONENT[17]         = "OptionsAnimation";
    const char OnlineGameRoomPlayerInfo::KAC_CHALLENGE_TOGGLE_COMPONENT[15]          = "optionToggle_0";
    const char OnlineGameRoomPlayerInfo::KAC_CHALLENGE_LIST_COMPONENT[14]            = "ChallengeList";
    const char OnlineGameRoomPlayerInfo::KAC_CHALLENGE_LIST_UP_ARROW_ANIMATION_COMPONENT[17]   = "UpArrowAnimation";
    const char OnlineGameRoomPlayerInfo::KAC_CHALLENGE_LIST_DOWN_ARROW_ANIMATION_COMPONENT[19] = "DownArrowAnimation";
    const char OnlineGameRoomPlayerInfo::KAC_PAUSE_BUTTON_PROMPTS_ANIMATION_COMPONENT[19]      = "PauseButtonPrompts";
    const char OnlineGameRoomPlayerInfo::KAC_PLAYERS_BUTTON_PROMPTS_ANIMATION_COMPONENT[21]    = "PlayersButtonPrompts";
    const char OnlineGameRoomPlayerInfo::KAC_SETTING_TOGGLE_COMPONENT[13]            = "optionToggle";
    const char OnlineGameRoomPlayerInfo::KAC_VIEW_EVENT_MENU_OPTIONS_COMPONENT[9]    = "MenuItem";

    // ---- icon state-id tables (X360 @0x8205EC18..0x8205EC80, dumped) ---------------
    // Indexing is X360-attested in ShowPlayerList @0x824A5968 (see the wave-H spec):
    // marked-man by (mbMarkedMan == 0), revenge by sign(rank delta), ready by
    // LobbyPlayerStatusData::meReadyStatus, video by meCameraStatus, voip by
    // meVOIPStatus, team by mePlayerTeam, connection by me{Game,Voip}ConnectionType.
    const char* const OnlineGameRoomPlayerInfo::KAP_MARKED_MAN_STATE_ID[2] =
    { "visible", "invisible" };
    const char* const OnlineGameRoomPlayerInfo::KAP_REVENGE_STATUS_STATE_ID[4] =
    { "ahead", "equal", "behind", "invisible" };
    const char* const OnlineGameRoomPlayerInfo::KAP_READY_STATUS_STATE_ID[5] =
    { "host", "invisible", "invisible", "racing", "invisible" };
    const char* const OnlineGameRoomPlayerInfo::KAP_VIDEO_STATUS_STATE_ID[5] =
    { "nocamera", "available", "inuse", "available", "invisible" };
    const char* const OnlineGameRoomPlayerInfo::KAP_VOIP_STATUS_STATE_ID[4] =
    { "noheadset", "available", "talking", "invisible" };
    const char* const OnlineGameRoomPlayerInfo::KAP_TEAM_STATUS_STATE_ID[3] =
    { "none", "red", "blue" };
    const char* const OnlineGameRoomPlayerInfo::KAP_CONNECTION_TYPE_STATE_ID[3] =
    { "direct", "gameserver", "invisible" };

    // ---- animation state pairs (X360 @0x82F266EC / @0x82F266F4 / @0x82F266FC) ------
    // The X360 sites load a MID-TABLE pointer for the "hide" transitions
    // (off_82F266F0 == &[1] "invisible", off_82F266F8 == &[1] "backup",
    // off_82F26700 == &[1] "invisible"); reconstruct as table[1] indexing.
    const char* const OnlineGameRoomPlayerInfo::KAPC_ANIMATION_STATES[2] =
    { "visible", "invisible" };
    const char* const OnlineGameRoomPlayerInfo::KAPC_PAUSE_BUTTON_PROMPT_ANIMATION_STATES[2] =
    { "return", "backup" };
    const char* const OnlineGameRoomPlayerInfo::KAPC_PLAYERS_BUTTON_PROMPT_ANIMATION_STATES[2] =
    { "visible", "invisible" };

    // ---- string-id tables (X360 @0x82F26704..0x82F26788, dumped as one window so
    //      every boundary is observed) -----------------------------------------------
    const char* const OnlineGameRoomPlayerInfo::KAPC_CHALLENGE_TOGGLE_ITEM_STRING_IDS[7] =
    { "2", "3", "4", "5", "6", "7", "8" };   // @0x82F26704 (player-count toggle items)

    // @0x82F26720 [14]. Indexed by GuiEventPerformOnlinePauseOption::EPauseOptions in
    // ShowPauseOptions / ShowSecurityOptions (ids 0..13; 10..12 are the security rows).
    const char* const OnlineGameRoomPlayerInfo::KPAC_PAUSE_OPTIONS_STRING_ID[14] =
    {
        "$ONLINE_LOBBY_OPTION_LAUNCH",          // 0
        "$ONLINE_LOBBY_OPTION_CREATE_EVENT",    // 1
        "$ONLINE_LOBBY_OPTION_CHANGE_TEAMS",    // 2
        "$ONLINE_LOBBY_OPTION_VIEW_PLAYERS",    // 3
        "$ONLINE_LOBBY_OPTION_LEAVE_GAME",      // 4
        "$ONLINE_LOBBY_OPTION_CHALLENGES",      // 5
        "$ONLINE_LOBBY_OPTION_VIEW_EVENT",      // 6
        "$ONLINE_LOBBY_OPTION_CANCEL_EVENT",    // 7
        "$ONLINE_LOBBY_OPTION_VIEW_MAP",        // 8
        "$ONLINE_GAME_OPTION_SECURITY",         // 9
        "$ONLINE_GAME_OPTION_SECURITY_PUBLIC",  // 10
        "$ONLINE_GAME_OPTION_SECURITY_PRIVATE", // 11
        "$ONLINE_GAME_OPTION_SECURITY_CLOSED",  // 12
        "$SettingMenu_2",                       // 13
    };

    // @0x82F26758 [7]. Indexed by the launched-event result in HandleLaunchedEvent
    // (result 0 == success never indexes; entry 0 is the empty-string sentinel).
    const char* const OnlineGameRoomPlayerInfo::KPC_LAUNCH_FAILED_STRINGIDS[7] =
    {
        "",
        "ONLINE_LAUNCHED_FAILED_PLAYERS",
        "ONLINE_LAUNCHED_FAILED_CONNECTING",
        "ONLINE_LAUNCHED_FAILED_ROUTES",
        "ONLINE_LAUNCHED_FAILED_TEAMS",
        "ONLINE_LAUNCHED_FAILED_PLAYING",
        "ONLINE_LAUNCHED_FAILED_GENERAL",
    };

    // @0x82F26774 [5]. Indexed by the left-game kick reason in HandleLeftGameEvent
    // (reason asserted <= 4; entries 0 and 2 share the "OnKicked" literal on X360).
    const char* const OnlineGameRoomPlayerInfo::KAPC_KICKED_POPUP[5] =
    {
        "OnKicked", "OnKickedGame", "OnKicked", "OnKickedComm", "OnKickedLost",
    };

    // ---- tab-title / message / option string ids (asm literals) --------------------
    const char OnlineGameRoomPlayerInfo::KAC_PLAYER_INFO_STRING_ID[34]     = "$ONLINE_GAME_ROOM_TAB_PLAYER_INFO";
    const char OnlineGameRoomPlayerInfo::KAC_EVENT_INFO_STRING_ID[33]      = "$ONLINE_GAME_ROOM_TAB_EVENT_INFO";
    const char OnlineGameRoomPlayerInfo::KAC_EVENT_MAP_STRING_ID[32]       = "$ONLINE_GAME_ROOM_TAB_EVENT_MAP";
    const char OnlineGameRoomPlayerInfo::KAC_LEAVING_GAME_STRING_ID[14]    = "$LEAVING_GAME";
    const char OnlineGameRoomPlayerInfo::KAC_MARK_PLAYER_STRING_ID[34]     = "$ONLINE_PLAYER_OPTION_MARK_PLAYER";
    const char OnlineGameRoomPlayerInfo::KAC_UNMARK_PLAYER_STRING_ID[36]   = "$ONLINE_PLAYER_OPTION_UNMARK_PLAYER";
    const char OnlineGameRoomPlayerInfo::KAC_SHOW_GAMERCARD_STRING_ID[44]  = "$ONLINE_PLAYER_OPTION_SHOW_GAMERCARD_PLAYER";
    const char OnlineGameRoomPlayerInfo::KAC_SUBMIT_FEEBDACK_STRING_ID[45] = "$ONLINE_PLAYER_OPTION_SUBMIT_FEEDBACK_PLAYER";
    const char OnlineGameRoomPlayerInfo::KAC_KICK_PLAYER_STRING_ID[34]     = "$ONLINE_PLAYER_OPTION_KICK_PLAYER";
    const char OnlineGameRoomPlayerInfo::KAC_VIEW_EVENT_MENU_ITEM_STRING_ID[36]   = "$ONLINE_LOBBY_OPTION_CHANGE_OPTIONS";
    // KAC_ASK_TO_LAUNCH_STRING_ID (cpp:225) / KAC_CHALLENGE_TOGGLE_TITLE_STRING_ID
    // (cpp:231) stay declaration-only: neither is consumed by this TU's 50 functions
    // and their rodata values were not dumped -- the foreign inlines that read them
    // define them when they land.
    // DWARF cpp:312 (18 chars incl NUL) -- the help-item accept prompt component name
    // the ShowSettingsScreen ctor-call passes ("HelpItemAccept_mc").
    const char OnlineGameRoomPlayerInfo::KAC_HELPITEM_ACCEPT[18]           = "HelpItemAccept_mc";
}
