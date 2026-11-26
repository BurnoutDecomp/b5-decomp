#include "GameSource/Gui/Events/BrnGuiEventNetworkPlayerStats.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"
static const char KAC_BRN_ONLINE_GAME_ROOM_PLAYER_INFO_ASSERT_FILE[] = "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.cpp"; // ARTIST filename 0x820607A0
// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- reconstructed from BURNOUT_X360_ARTIST.XEX
// (addresses in the header). WAVE H KEYSTONE SCAFFOLD: this file carries the TU's
// static-member DEFINITIONS (every value read from the X360 image with observed table
// boundaries -- scratchpad/waveB/ogrpi_rodata.txt + scratchpad/waveH/ogrpi_rodata2.txt
// + scratchpad/waveH/OnlineGameRoomPlayerInfo.spec.md). The 50 function bodies land in
// the wave-H implementation files (BrnOnlineGameRoomPlayerInfo_wH_XX.cpp) per the
// spec's group plan.
// ===================================================================================
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.cpp"
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
    const char* OnlineGameRoomPlayerInfo::KAPC_ANIMATION_STATES[2] =
    { "visible", "invisible" };
    const char* OnlineGameRoomPlayerInfo::KAPC_PAUSE_BUTTON_PROMPT_ANIMATION_STATES[2] =
    { "return", "backup" };
    const char* OnlineGameRoomPlayerInfo::KAPC_PLAYERS_BUTTON_PROMPT_ANIMATION_STATES[2] =
    { "visible", "invisible" };

    // ---- string-id tables (X360 @0x82F26704..0x82F26788, dumped as one window so
    //      every boundary is observed) -----------------------------------------------
    const char* OnlineGameRoomPlayerInfo::KAPC_CHALLENGE_TOGGLE_ITEM_STRING_IDS[7] =
    { "2", "3", "4", "5", "6", "7", "8" };   // @0x82F26704 (player-count toggle items)

    // @0x82F26720 [14]. Indexed by GuiEventPerformOnlinePauseOption::EPauseOptions in
    // ShowPauseOptions / ShowSecurityOptions (ids 0..13; 10..12 are the security rows).
    const char* OnlineGameRoomPlayerInfo::KPAC_PAUSE_OPTIONS_STRING_ID[14] =
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
    const char* OnlineGameRoomPlayerInfo::KPC_LAUNCH_FAILED_STRINGIDS[7] =
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
    const char* OnlineGameRoomPlayerInfo::KAPC_KICKED_POPUP[5] =
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
    // KAC_ASK_TO_LAUNCH_STRING_ID stays declaration-only: nothing in this TU
    // reads it.
    // The player-count toggle's title (ShowChallengesOptions passes it to SetupMenuToggle).
    const char OnlineGameRoomPlayerInfo::KAC_CHALLENGE_TOGGLE_TITLE_STRING_ID[26] = "$ONLINE_CHALLENGE_PLAYERS";

    // Game-mode string ids for the launch overlay's mode parameter, indexed by the game
    // mode (HandleLaunchingEvent). Eighteen slots; only the online modes carry a string,
    // the offline modes and the free-burn lobby / showtime slots are null in the image.
    const char* OnlineGameRoomPlayerInfo::KPAC_MODE_STRINGS[18] =
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        "ONLINE_GAME_OPTION_MODE_RACE",
        "ONLINE_GAME_OPTION_MODE_ROAD_RAGE",
        "ONLINE_GAME_OPTION_MODE_STUNT",
        "ONLINE_GAME_OPTION_MODE_BURNING_HOME_RUN",
        "ONLINE_GAME_OPTION_MODE_STUNT_FREE_FOR_ALL",
        0,
        0,
        "ONLINE_GAME_OPTION_MODE_STUNT_COOP",
    };
    // DWARF cpp:312 (18 chars incl NUL) -- the help-item accept prompt component name
    // the ShowSettingsScreen ctor-call passes ("HelpItemAccept_mc").
    const char OnlineGameRoomPlayerInfo::KAC_HELPITEM_ACCEPT[18]           = "HelpItemAccept_mc";
}

// FOLDED FROM BrnOnlineGameRoomPlayerInfo_wH_00.cpp
// BrnGui::OnlineGameRoomPlayerInfo -- wave-H group 00: Update @0x824B0770 and
// UpdateSoundSettings @0x8249AD00.
//
// This group was reconstructed complete but parked out-of-tree by its implementer
// because it needed three shared-header declarations that implementers are not
// permitted to touch (MenuToggle::Update, and CrashNavOptionsData friendship for the
// two volume members). The conductor added those declarations; this is that
// reconstruction, unchanged apart from this banner.
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                      // CgsGui::GuiAccessPointers
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / the in-queue
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache (delta / camera / disconnect)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiEventActivateCrashNav (id 191)
#include "GameSource/Gui/SatNav/BrnMapIconManager.h"                      // BrnGui::MapIconManager::IconSizeMode

namespace BrnGui
{
    namespace
    {
        // ---- the state in-queue (CgsGui::State::mpInGuiEventQueue's real instantiation) ---
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // ---- AddEvent channels (the out-queue selector word) ---------------------------
        const s32 KI_CHANNEL_GUI_OUT      = 40;   // OutputGuiEvent

        const s32 KI_CHANNEL_GUI_INTERNAL = 42;   // OutputInternalState

        // ---- observed wire ids (the class's maiEventToObserve[33] set) ------------------
        // Named where the role is attested by the handler this arm calls or by the assert
        // string the arm fires; FLAG marks the ones whose producer-side name is not in the
        // recovered DWARF slice and which are named for what THIS screen does with them.
        const s32 KI_EVENT_CONTROLLER_PRESSED   = 6;
        const s32 KI_EVENT_CONTROLLER_RELEASED  = 7;
        const s32 KI_EVENT_NETWORK_DISCONNECTED = 44;
        const s32 KI_EVENT_IN_GAME              = 50;   // assert string "lpInGameEvent"
        const s32 KI_EVENT_LEAVING_GAME         = 53;
        const s32 KI_EVENT_ASK_TO_LAUNCH        = 55;
        const s32 KI_EVENT_ONLINE_LAUNCHING     = 57;
        const s32 KI_EVENT_ONLINE_LAUNCHED      = 58;
        const s32 KI_EVENT_GUI_CACHE            = 64;
        const s32 KI_EVENT_CAR_SELECT_START     = 81;
        const s32 KI_EVENT_RETURN_TO_HUD        = 85;   // FLAG: named for this arm's action
        const s32 KI_EVENT_INVITE_FAILED        = 133;
        const s32 KI_EVENT_INVITE_COMPLETE      = 134;  // FLAG: named for this arm's action (posts id 135)
        const s32 KI_EVENT_OVERLAY_COMPLETE     = 189;
        const s32 KI_EVENT_TO_CAR_SELECT        = 229;  // FLAG: named for this arm's action
        const s32 KI_EVENT_PLAYER_LOBBY_LIST    = 244;
        const s32 KI_EVENT_PLAYER_STATS         = 248;
        const s32 KI_EVENT_GAME_PARAMS_CHANGED  = 257;
        const s32 KI_EVENT_SPLASH_SCREEN        = 269;
        const s32 KI_EVENT_NETWORK_LEFT_GAME    = 273;
        const s32 KI_EVENT_LEAVING_GAME_FAILED  = 275;
        const s32 KI_EVENT_PERFORM_PAUSE_OPTION = 283;  // GuiEventPerformOnlinePauseOption
        const s32 KI_EVENT_UI_VISIBLE           = 516;  // assert string "lpUiVisibleEvent != NULL"
        const s32 KI_EVENT_EVERY_PLAYER_STATUS  = 581;

        // ---- car-select-start payload types (the id-81 payload word) --------------------
        // FLAG: named for the two branches this arm takes; the producer-side enum is not in
        // the recovered slice. Anything else fires the "Invalid car select type" assert.
        const s32 KI_CAR_SELECT_TYPE_FROM_LOBBY   = 1;
        const s32 KI_CAR_SELECT_TYPE_ONLINE_EVENT = 2;

        // The zero the two EA-trax timers are compared against (X360 flt_82001CC0).
        const f32 KF_ZERO_TIME = 0.0f;

        // ---- in-queue payload views -----------------------------------------------------
        // The in-queue hands the state the HEADER-STRIPPED payload, so each view starts at
        // the producer's first payload field (BrnInGame.cpp / wave-H 05 precedent).

        // Id 44: the server-interface error the disconnect popup shows (`lwz 0(r27)`).
        struct NetworkDisconnectedPayload : public CgsModule::Event
        {
            s32 meError;   // +0x00
        };

        // Id 50: read as a BYTE (`lbz 0(r27)`) -- the "entering a game" flag.
        struct InGameEventPayload : public CgsModule::Event
        {
            bool mbInGame;   // +0x00
        };

        // Id 81: the car-select type word (`lwz 0(r27)`).
        struct CarSelectStartPayload : public CgsModule::Event
        {
            s32 meCarSelectType;   // +0x00
        };

        // Id 283: the pause option to perform (`lwz 0(r27)`).
        struct PerformPauseOptionPayload : public CgsModule::Event
        {
            s32 mePauseOption;   // +0x00
        };

        // Id 516: the "UI visible" word (`lwz 0(r27)`); 0 == the UI just went away, which
        // is what pushes the free-burning screen into the pause menu.
        struct UiVisiblePayload : public CgsModule::Event
        {
            s32 miUiVisible;   // +0x00
        };

        // ---- out-queue wire records ------------------------------------------------------
        // Each is a CgsGui::GuiEvent<N> header { payload size, event id, payload offset }
        // followed by the payload, published through
        // mpStateInterface->GetOutputEventQueue()->AddEvent(record, channel, sizeof(record)).

        // The bare one-byte-payload command record { 1, N, 12, <payload byte> }, X360 record
        // size 16. The X360 stack-builds ONLY the three header words for all three ids this
        // file posts (135, 461, 533) -- the payload byte keeps whatever the reused stack slot
        // held -- so the constructor leaves it alone rather than inventing a stored value.
        template <s32 TI_EVENT_ID>
        struct GuiCommandWire16 : public CgsGui::GuiEvent<TI_EVENT_ID>
        {
            u8 mu8Payload;   // +0x0C (not written by the X360)

            GuiCommandWire16()
            {
            }
        };

        // Id 188 -- the overlay wait-finish handshake the X360 publishes through
        // OutputGuiEvent<BrnGui::GuiOverlayWaitFinishRequest> @0x82476E98: the 8-byte
        // compressed overlay id sits at the 8-aligned payload slot, X360 record size 24,
        // channel 40 (the BrnPauseScreen.cpp / BrnInGame.cpp wire twin).
        typedef CgsGui::GuiEvent<188> OverlayWaitFinishHeader;
        struct GuiOverlayWaitFinishWire
        {
            GuiOverlayWaitFinishRequest mRequest;   // the OutputGuiEvent payload

            GuiOverlayWaitFinishWire()
            {
            }
        };

        // Id 463 -- the sound-volume settings record { 8, 463, 12, music, sfx }, X360
        // record size 20, channel 40. FLAG: the producer-side type name is not in the
        // recovered slice; the two payload words are the options model's music / SFX
        // volume members, in that order.
        typedef CgsGui::GuiEvent<463> SoundVolumesHeader;
        struct GuiEventSoundVolumesWire : public SoundVolumesHeader
        {
            s32 meMusicVolume;   // +0x0C
            s32 meSFXVolume;     // +0x10

            GuiEventSoundVolumesWire(CrashNavOptionsData::EOptionsSoundVolumes leMusicVolume,
                                     CrashNavOptionsData::EOptionsSoundVolumes leSFXVolume)
                : meMusicVolume(static_cast<s32>(leMusicVolume))
                , meSFXVolume(static_cast<s32>(leSFXVolume))
            {
            }
            GuiEventSoundVolumesWire() {}
        };

        // Post the id-188 record for one overlay name -- the X360 does exactly this four
        // times across the TU (Construct the request onto the wire record, then publish).
        void PostOverlayWaitFinish(CgsGui::StateInterface* lpStateInterface,
                                   const char* lpacOverlayName)
        {
            GuiOverlayWaitFinishWire lWaitFinish;
            lWaitFinish.mRequest.Construct(lpacOverlayName);
            lpStateInterface->OutputGuiEvent(lWaitFinish.mRequest);   // X360 record size 24
        }
    }

    // ================================================================================
    //  Update  @ 0x824B0770  (cpp:464)
    // ================================================================================
    void OnlineGameRoomPlayerInfo::Update()
    {

        const CgsModule::Event* lpEvent;
        s32 liSize;

        for (s32 liEventId = mpInGuiEventQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = mpInGuiEventQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            switch (liEventId)
            {
                case KI_EVENT_CONTROLLER_PRESSED:
                    HandleControllerInputPressed(lpEvent);
                    break;

                case KI_EVENT_CONTROLLER_RELEASED:
                    HandleControllerInputReleased(lpEvent);
                    break;

                // Registered in maiEventToObserve but consumed with no action here.
                case 8:
                case 14:
                case 16:
                case 21:
                case 199:
                case 202:
                case 213:
                case 320:
                case 334:
                case 344:
                case 406:
                    break;

                case KI_EVENT_NETWORK_DISCONNECTED:
                    // While an invite is being performed the disconnect is expected, so the
                    // error word is NOT latched -- the screen just backs out. Note the X360
                    // re-walks GetAccessPointers()->GetGuiCache() for the store rather than
                    // reusing the pointer it just tested.
                    if (mpStateInterface->GetAccessPointers()->GetGuiCache()->mbPerformingInvite)
                    {
                        SendStateEvent("GO_BACK");
                    }
                    else
                    {
                        GuiCache* lpGuiCache = mpStateInterface->GetAccessPointers()->GetGuiCache();
                        if (lpEvent != 0)
                        {
                            lpGuiCache->meLastDisconnectedError =
                                reinterpret_cast<const NetworkDisconnectedPayload*>(lpEvent)->meError;
                        }
                        else
                        {
                            lpGuiCache->meLastDisconnectedError = 0;
                        }
                        SendStateEvent("GO_BACK");
                    }
                    break;

                case KI_EVENT_IN_GAME:
                {
                    CGS_ASSERT_AT(lpEvent != 0, "lpInGameEvent", CGS_ARTIST_FILE, 608);   // cpp:608 (non-fatal)
                    // Entering the game: hold the flow until the "enter game" overlay has
                    // finished showing.
                    if (reinterpret_cast<const InGameEventPayload*>(lpEvent)->mbInGame)
                    {
                        PostOverlayWaitFinish(mpStateInterface, "CNOnlEntGame");
                    }
                    break;
                }

                case KI_EVENT_LEAVING_GAME:
                    mMessageText.SetText(KAC_LEAVING_GAME_STRING_ID);
                    meSubState = E_SUBSTATE_LEAVING;
                    break;

                case KI_EVENT_ASK_TO_LAUNCH:
                    HandleAskToLaunchEvent(lpEvent);
                    break;

                case KI_EVENT_ONLINE_LAUNCHING:
                    HandleLaunchingEvent(lpEvent);
                    break;

                case KI_EVENT_ONLINE_LAUNCHED:
                    HandleLaunchedEvent(lpEvent);
                    break;

                case KI_EVENT_GUI_CACHE:
                    HandleGuiCacheEvent(lpEvent);
                    CheckForCompletedLoads();
                    break;

                case KI_EVENT_CAR_SELECT_START:
                {
                    // The HUD goes down first whichever way the car select is reached.
                    GuiEventShowHideHud lShowHideHud;
                    lShowHideHud.mbShow = false;
                    mpStateInterface->OutputInternalState(lShowHideHud);   // X360 record size 16

                    const s32 leCarSelectType =
                        reinterpret_cast<const CarSelectStartPayload*>(lpEvent)->meCarSelectType;
                    if (leCarSelectType == KI_CAR_SELECT_TYPE_FROM_LOBBY)
                    {
                        // Only leave for the car select if the player was actually out in the
                        // world (or already on the way out).
                        if (meSubState == E_SUBSTATE_FREE_BURNING ||
                            meSubState == E_SUBSTATE_LEAVING)
                        {
                            SendStateEvent("TO_CSELECT");
                            PostOverlayWaitFinish(mpStateInterface, "CNOnlLvgGame");
                        }
                    }
                    else if (leCarSelectType == KI_CAR_SELECT_TYPE_ONLINE_EVENT)
                    {
                        SendStateEvent("TO_ON_CARSEL");

                        // Hand the front-end map (CrashNav) flow back the screen.
                        GuiEventActivateCrashNav lActivateCrashNav;
                        lActivateCrashNav.muActivate = 1u;
                        lActivateCrashNav.muParam = 0;
                        mpStateInterface->OutputGuiEvent(lActivateCrashNav);   // X360 record size 20

                        GuiCommandWire16<533> lScreenFlowCommand;
                        mpStateInterface->OutputGuiEvent(lScreenFlowCommand);  // X360 record size 16
                    }
                    else
                    {
                        // cpp:701 -- streamed as "Invalid car select type : " << type << "\n";
                        // lowered to the static text per project policy. Non-fatal: the X360
                        // falls straight through into the plain car-select transition.
                        CGS_FAIL("Invalid car select type : ");
                        SendStateEvent("TO_CSELECT");
                    }
                    break;
                }

                case KI_EVENT_RETURN_TO_HUD:
                    UnloadPauseResources();
                    HideMenusAndReturnToHUD();
                    break;

                case KI_EVENT_INVITE_FAILED:
                    HandleInviteFailed(lpEvent);
                    break;

                case KI_EVENT_INVITE_COMPLETE:
                {
                    GuiCommandWire16<135> lCommand;
                    mpStateInterface->OutputGuiEvent(lCommand);   // X360 record size 16
                    break;
                }

                case KI_EVENT_OVERLAY_COMPLETE:
                    HandleOverlayComplete(lpEvent);
                    break;

                case KI_EVENT_TO_CAR_SELECT:
                    SendStateEvent("TO_CSELECT");
                    break;

                case KI_EVENT_PLAYER_LOBBY_LIST:
                    HandlePlayerLobbyListEvent(lpEvent);
                    break;

                case KI_EVENT_PLAYER_STATS:
                    HandlePlayerStatsEvent(lpEvent);
                    break;

                case KI_EVENT_GAME_PARAMS_CHANGED:
                    HandleGameParamsChangedEvent(lpEvent);
                    break;

                case KI_EVENT_SPLASH_SCREEN:
                    HandleSplashScreenRequests(lpEvent);
                    break;

                case KI_EVENT_NETWORK_LEFT_GAME:
                    HandleLeftGameEvent(lpEvent);
                    break;

                case KI_EVENT_LEAVING_GAME_FAILED:
                    HandleLeavingGameFailedEvent(lpEvent);
                    break;

                case KI_EVENT_PERFORM_PAUSE_OPTION:
                    // Only actionable while the player is driving with the menus down.
                    if (meSubState == E_SUBSTATE_FREE_BURNING)
                    {
                        PerformPauseOption(
                            reinterpret_cast<const PerformPauseOptionPayload*>(lpEvent)->mePauseOption);
                        mbShownFromPause = false;
                    }
                    break;

                case KI_EVENT_UI_VISIBLE:
                    if (meSubState == E_SUBSTATE_FREE_BURNING)
                    {
                        CGS_ASSERT_AT(lpEvent != 0, "lpUiVisibleEvent != NULL", CGS_ARTIST_FILE, 519);   // cpp:519 (non-fatal)
                        if (reinterpret_cast<const UiVisiblePayload*>(lpEvent)->miUiVisible == 0)
                        {
                            HideHUDAndEnterMenus();
                            ShowPauseScreen();
                        }
                    }
                    break;

                case KI_EVENT_EVERY_PLAYER_STATUS:
                    mChallengeListComponent.HandleEveryPlayerCompletionStatus(
                        reinterpret_cast<const BrnGameState::GameStateModuleIO::FburnChallengeEveryPlayerStatusData*>(
                            lpEvent));
                    break;

                default:
                    // cpp:748 -- streamed as "Unexpected event received : " << liEventId <<
                    // " in " << <file> << " at line " << 748 << "\n"; lowered to the static
                    // text per project policy. Non-fatal.
                    CGS_FAIL("Unexpected event received : ");
                    break;
            }
        }

        // ---- the EA-trax debounce pair (armed by the free-burn controller handler) ------
        // While the "disable next track" hold is running the skip timer is pinned at zero;
        // otherwise the skip timer runs down and fires the id-461 command as it expires.
        // Both run on the cache's frame delta (GuiCache's leading word).
        if (mfTimeToDisableNextEATrack > KF_ZERO_TIME)
        {
            mfTimeToDisableNextEATrack -= mpGuiCache->mfTimeStep;
            mfTimeUntilNextEATrack = KF_ZERO_TIME;
        }
        else if (mfTimeUntilNextEATrack > KF_ZERO_TIME)
        {
            mfTimeUntilNextEATrack -= mpGuiCache->mfTimeStep;
            // `bgt` skips the post, so an unordered compare posts too.
            if (!(mfTimeUntilNextEATrack > KF_ZERO_TIME))
            {
                GuiCommandWire16<461> lNextTrackCommand;
                mpStateInterface->OutputGuiEvent(lNextTrackCommand);   // X360 record size 16
            }
        }

        // ---- the menu pass: skipped entirely while the player is out driving ------------
        if (meSubState != E_SUBSTATE_FREE_BURNING)
        {
            mRouteInfoDisplay.Update(mpInGuiEventQueue);
            mPlayerNameComponent.Update();
            mOptionComponent.Update();
            mPauseComponent.Update();
            mPlayerStatsDisplay.Update();
            mSettingToggle.Update();
            mViewEventOptions.Update();

            switch (meSubState)
            {
                case E_SUBSTATE_MAP:
                {
                    CrashNavMap::Update();

                    // The one-shot cursor snap: the first map update after the page comes up
                    // centres the map on the local player and grows the icons.
                    if (mbFirstMapUpdate && mpGuiCache != 0 && mbIsScreenLoaded)
                    {
                        const Vector3* lpPlayerInfo = &mpGuiCache->GetPlayerInfo()->mv3PlayerCarPosition;
                        CGS_ASSERT(lpPlayerInfo != 0, "lpPlayerInfo");   // cpp:813 (non-fatal)

                        // The X360 permutes the world position into the 2-D map point with
                        // unk_82CDA450 == 00010203 18191A1B 00010203 00010203, i.e. the
                        // (X,Y,Z,W) world vector becomes (X,Z,X,X).
                        Vector2 lv2MapLocation;
                        lv2MapLocation.mV.x = lpPlayerInfo->mV.x;
                        lv2MapLocation.mV.y = lpPlayerInfo->mV.z;
                        lv2MapLocation.mV.z = lpPlayerInfo->mV.x;
                        lv2MapLocation.mV.w = lpPlayerInfo->mV.x;
                        mMainMapComponent.SnapToLocation(lv2MapLocation);

                        if (CrashNavMap::PlaceCursorOnPlayer())
                        {
                            mpIconManager->meIconSizeMode = MapIconManager::E_ICONSIZE_LARGE;
                            mbFirstMapUpdate = false;
                        }
                    }
                    break;
                }

                case E_SUBSTATE_CHALLENGES:
                    mChallengeToggle.Update();
                    mChallengeListComponent.Update();
                    break;

                case E_SUBSTATE_CONTROLLER_DISABLE_DELAY:
                    // Swallow input for KI_CONTROLLER_DISABLE_DELAY_FRAME_COUNT frames after
                    // the menus came down, then hand the player back to free burning.
                    --miDelayFrameCount;
                    if (miDelayFrameCount < 0)
                    {
                        meSubState = E_SUBSTATE_FREE_BURNING;
                    }
                    break;

                case E_SUBSTATE_SETTINGS:
                    // The accept prompt only appears once a setting has actually moved.
                    if (mbSettingsChanged)
                    {
                        mHelpItemAccept.SetItem("$CAPS_BUTTON_ACCEPT",
                                                ButtonIconComponent::E_PADBUTTON_SELECT,
                                                ButtonIconComponent::E_PADBUTTON_INVISIBLE);
                    }
                    else
                    {
                        // The empty-string rodata sentinel (&unk_820046A7).
                        mHelpItemAccept.SetItem("",
                                                ButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                                ButtonIconComponent::E_PADBUTTON_INVISIBLE);
                    }
                    break;

                default:
                    break;
            }
        }

        // Every X360 arm tails into this same Clear (the duplicated epilogues).
        mpInGuiEventQueue->Clear();
    }

    // ================================================================================
    //  UpdateSoundSettings  @ 0x8249AD00  (cpp:4590)
    //
    //  Publish the two sound-volume settings the options model currently holds. Called
    //  by HandleSettingChanged when a volume toggle moves.
    // ================================================================================
    void OnlineGameRoomPlayerInfo::UpdateSoundSettings()
    {
        CGS_ASSERT_AT(mpStateInterface != 0, "mpStateInterface", KAC_BRN_ONLINE_GAME_ROOM_PLAYER_INFO_ASSERT_FILE, 4636);

        // { 8, 463, 12, music, sfx } on channel 40, X360 record size 20. The X360 copies
        // the two adjacent option words out as one doubleword; they are the two volume
        // members in order.
        GuiEventSoundVolumesWire lSoundVolumes(mCrashNavOptionsData.meMusicVolume,
                                               mCrashNavOptionsData.meSFXVolume);
        mpStateInterface->OutputGuiEvent(lSoundVolumes);
    }
}

// FOLDED FROM BrnOnlineGameRoomPlayerInfo_wH_01.cpp
// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-H partfile 01: the lifecycle trio.
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   Construct @ 0x82484B30 (BrnOnlineGameRoomPlayerInfo.cpp:330)
//   OnEnter   @ 0x824A36E0 (BrnOnlineGameRoomPlayerInfo.cpp:353)
//   OnLeave   @ 0x824A3C18 (BrnOnlineGameRoomPlayerInfo.cpp:855)
//
// The class shape, the member map and every rodata table this file names live in
// BrnOnlineGameRoomPlayerInfo.{h,cpp} (the wave-H keystone scaffold). The sibling
// bodies land in the other BrnOnlineGameRoomPlayerInfo_wH_XX.cpp partfiles.
//
// X360 offsets appear only in comments: the host layout is name-based (pointers and
// the component vptrs widen on x64, so no console offset/stride is reproduced).
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsModule::Event, GuiEventQueueLarge
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface (Register/UnRegister/PlayAptMovie/out-queue)
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache, GuiFlow
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiFlow (E_GUIFLOW_SCREEN)
namespace BrnGui
{
    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) ------------------------

        const s32 KI_CHANNEL_VIEW_STATE   = 41;   // GuiOutViewState


        // The apt-component name buffer the OnEnter icon loop formats into. X360:
        // CgsCore::SPrintf(lacComponentName, 32, "<fmt>", liPlayer).
        const u32 KU_ICON_COMPONENT_NAME_LEN = 32;
    }

    // ------------------------------------------------ Construct @ 0x82484B30
    // Chain the map base, then clear the two map-presentation options this screen
    // overrides and the first-map-update latch.
    void OnlineGameRoomPlayerInfo::Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm)
    {
        CGS_ASSERT_AT(lpFsm != 0,
            "lpFsm",
                CGS_ARTIST_FILE, 338 /* ARTIST 0x82484B58 */);   // X360 BrnOnlineGameRoomPlayerInfo.cpp:338


        CrashNavMap::Construct(liId, lpFsm);

        mbUseRoadSigns          = false;   // base, X360 +24705 (stb)
        mbFirstMapUpdate        = false;   // X360 +87532 (stb)
        meEventIconDisplayType  = 0;       // base, X360 +24708 (stw)
    }

    // ------------------------------------------------ OnEnter @ 0x824A36E0
    // Register the screen's observed-event set, construct every apt-bound component
    // (the 8 player rows x 8 status icons included), then prime the scalar state and
    // drop straight into the hidden-lobby main sub-state.
    void OnlineGameRoomPlayerInfo::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // Components whose Construct is the component vtable's slot 0 (the X360 calls
        // them through `(**(this + off))(...)`); the MenuComponent/MenuToggle/Icon
        // families take their extra args and are called directly.
        mOptionsAnimation.Construct(KAC_OPTIONS_ANIMATION_COMPONENT, mpStateInterface, 0);

        // apt-id arg: the X360 loads `clrldi r26, -1, 32`, i.e. 32-bit -1 zero-extended
        // into the 64-bit id word -- 0xFFFFFFFF, not (u64)-1.
        mPlayerNameComponent.Construct(KAC_PLAYER_NAME_COMPONENT, mpStateInterface, 8, 0, 0xFFFFFFFFull);
        mOptionComponent.Construct(KAC_OPTION_COMPONENT, mpStateInterface, 9, 0, 0xFFFFFFFFull);
        mPauseComponent.Construct(KAC_OPTION_COMPONENT, mpStateInterface, 11, 0, 0xFFFFFFFFull);

        mMessageText.Construct(KAC_TEXTFIELD_COMPONENT, mpStateInterface, 0);
        mPlayerStatsDisplay.Construct(KAC_PLAYER_STATS_COMPONENT, mpStateInterface, 0);
        mRouteInfoDisplay.Construct(KAC_ROUTE_INFO_COMPONENT, 0, mpStateInterface, 0);

        // One status-icon set per player row; each icon binds the state-id table that
        // ShowPlayerList later indexes. NOTE: the X360 passes the SAME table
        // (off_8205EC74 == KAP_CONNECTION_TYPE_STATE_ID) for BOTH connection icons.
        for (s32 liPlayer = 0; liPlayer < KI_MAX_PLAYERS; ++liPlayer)
        {
            char lacComponentName[KU_ICON_COMPONENT_NAME_LEN];

            CgsCore::SPrintf(lacComponentName, KU_ICON_COMPONENT_NAME_LEN,
                             KAC_MARKED_MAN_ICON_COMPONENT, liPlayer);
            maIcons[liPlayer].mMarkedManIcon.Construct(lacComponentName, mpStateInterface,
                                                       KAP_MARKED_MAN_STATE_ID, 0);

            CgsCore::SPrintf(lacComponentName, KU_ICON_COMPONENT_NAME_LEN,
                             KAC_REVENGE_STATUS_ICON_COMPONENT, liPlayer);
            maIcons[liPlayer].mRevengeStatusIcon.Construct(lacComponentName, mpStateInterface,
                                                           KAP_REVENGE_STATUS_STATE_ID, 0);

            CgsCore::SPrintf(lacComponentName, KU_ICON_COMPONENT_NAME_LEN,
                             KAC_READY_STATUS_ICON_COMPONENT, liPlayer);
            maIcons[liPlayer].mReadyStatusIcon.Construct(lacComponentName, mpStateInterface,
                                                         KAP_READY_STATUS_STATE_ID, 0);

            CgsCore::SPrintf(lacComponentName, KU_ICON_COMPONENT_NAME_LEN,
                             KAC_VIDEO_STATUS_ICON_COMPONENT, liPlayer);
            maIcons[liPlayer].mVideoStatusIcon.Construct(lacComponentName, mpStateInterface,
                                                         KAP_VIDEO_STATUS_STATE_ID, 0);

            CgsCore::SPrintf(lacComponentName, KU_ICON_COMPONENT_NAME_LEN,
                             KAC_VOIP_STATUS_ICON_COMPONENT, liPlayer);
            maIcons[liPlayer].mVoipStatusIcon.Construct(lacComponentName, mpStateInterface,
                                                        KAP_VOIP_STATUS_STATE_ID, 0);

            CgsCore::SPrintf(lacComponentName, KU_ICON_COMPONENT_NAME_LEN,
                             KAC_TEAM_STATUS_ICON_COMPONENT, liPlayer);
            maIcons[liPlayer].mTeamStatusIcon.Construct(lacComponentName, mpStateInterface,
                                                        KAP_TEAM_STATUS_STATE_ID, 0);

            CgsCore::SPrintf(lacComponentName, KU_ICON_COMPONENT_NAME_LEN,
                             KAC_GAME_CONNECTION_TYPE_ICON_COMPONENT, liPlayer);
            maIcons[liPlayer].mGameConnectionTypeIcon.Construct(lacComponentName, mpStateInterface,
                                                                KAP_CONNECTION_TYPE_STATE_ID, 0);

            CgsCore::SPrintf(lacComponentName, KU_ICON_COMPONENT_NAME_LEN,
                             KAC_VOIP_CONNECTION_TYPE_ICON_COMPONENT, liPlayer);
            maIcons[liPlayer].mVoipConnectionTypeIcon.Construct(lacComponentName, mpStateInterface,
                                                                KAP_CONNECTION_TYPE_STATE_ID, 0);
        }

        mChallengeToggle.Construct(KAC_CHALLENGE_TOGGLE_COMPONENT, mpStateInterface, 0, 0xFFFFFFFFull);

        mChallengeListComponent.Construct(KAC_CHALLENGE_LIST_COMPONENT, mpStateInterface, 0);
        mChallengeListUpArrowAnimation.Construct(KAC_CHALLENGE_LIST_UP_ARROW_ANIMATION_COMPONENT,
                                                 mpStateInterface, 0);
        mChallengeListDownArrowAnimation.Construct(KAC_CHALLENGE_LIST_DOWN_ARROW_ANIMATION_COMPONENT,
                                                   mpStateInterface, 0);
        mPauseButtonPromptsAnimation.Construct(KAC_PAUSE_BUTTON_PROMPTS_ANIMATION_COMPONENT,
                                               mpStateInterface, 0);
        mPlayersButtonPromptsAnimation.Construct(KAC_PLAYERS_BUTTON_PROMPTS_ANIMATION_COMPONENT,
                                                 mpStateInterface, 0);

        mSettingToggle.Construct(KAC_SETTING_TOGGLE_COMPONENT, mpStateInterface, 4, 0, 0xFFFFFFFFull);
        mViewEventOptions.Construct(KAC_VIEW_EVENT_MENU_OPTIONS_COMPONENT, mpStateInterface, 1, 0,
                                    0xFFFFFFFFull);

        // DecFIGS lines 412-423 recover the reset order behind ARTIST 0x824A3B54-0x824A3BD8.
        mpGuiCache                 = 0;       // X360 +87516 (OUR cache ptr, not the base's)
        mSelectedPlayerID          = -1;      // X360 +87512
        miCurrentRoundDisplayed    = 0;       // X360 +87508
        mfTimeUntilNextEATrack     = 0.0f;    // X360 +87520 (flt_82001CC0)
        mfTimeToDisableNextEATrack = 0.0f;    // X360 +87524 (flt_82001CC0)
        mbShowSplashScreen         = false;   // X360 +87533
        mbKicked                   = false;   // X360 +87535
        mbShownFromPause           = false;   // X360 +87536
        mbNeedToClearSplashScreen  = false;   // X360 +87534
        mbHighlightedNewPlayer     = false;   // X360 +87537

        // Inlined CrashNavOptionsData::Construct: the nine stores at X360 +87480..+87500
        // are exactly its nine parameters in declared field order
        // (meCameraUserOption, meVoipVolume, meMusicVolume, meSFXVolume,
        //  mbSixAxisShowtime, mbSixAxisSteering, mbForceFeedback, mbTips,
        //  mbDefaultGameCamera) = {0, 8, 8, 8, false, false, true, false, true}.
        mCrashNavOptionsData.Construct(0,
                                       CrashNavOptionsData::E_OPTION_VOIP_VOLUMES_8,
                                       CrashNavOptionsData::E_OPTION_SOUND_VOLUMES_8,
                                       CrashNavOptionsData::E_OPTION_SOUND_VOLUMES_8,
                                       false, false, true, false, true);

        meSubState = E_SUBSTATE_MAIN;   // X360 +87504 = 10

        HideLobby();

    }

    // ------------------------------------------------ OnLeave @ 0x824A3C18
    // Tear the screen down: unmount the apt movie, drop the observed-event set,
    // destruct the two composed panels, turn the player-texture feed off, reset the
    // view state on both output channels, clear the expected-component list, let the
    // map base leave if it was the live page, and unload every resource group.
    void OnlineGameRoomPlayerInfo::OnLeave()
    {
        // Inlined GuiEventPlayAptMovie (type 18, channel 41, X360 size 20): the movie
        // name is the empty rodata sentinel (&unk_820046A7), level 3. Posted through
        // PlayAptMovie so the record carries the host-width name pointer.
        mpStateInterface->PlayAptMovie("", 3);

        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

        mSelectedPlayerID = -1;   // X360 +87512

        mPlayerStatsDisplay.Destruct();
        mRouteInfoDisplay.Destruct();

        CgsGui::GuiStackEventQueue::GuiEventQueueLarge* lpOutQueue =
            mpStateInterface->GetOutputEventQueue();

        // GuiEventNetworkOutputPlayerTexture (id 264) OFF:
        // { header0=8, type=264, header2=12, payload={0, 0xFFFFFFFF} } -- channel 40,
        // 20 bytes. Same record OnlinePlay::OnLeave posts (its ON form passes 1).
        u32 lauPlayerTextureRecord[5] = { 8u, 264u, 12u, 0u, 0xFFFFFFFFu };
        lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lauPlayerTextureRecord),
                             KI_CHANNEL_GUI_OUT, 20);

        // The view-state reset pair: the same GuiEvent<213> show/hide record
        // { 12, 213, 12, show=0, value=0.0f, flag=0 } posted on the view-state channel
        // and then on the internal channel (X360 size 24). The X360 writes only the
        // FLAG byte of the trailing word (stb 0) and copies the word whole, so its
        // three pad bytes carry stale stack (the previous record's header word, 12);
        // they are padding, modelled zeroed.
        u32 lauShowHideRecord[6] = { 12u, 213u, 12u, 0u, 0u, 0u };
        lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lauShowHideRecord),
                             KI_CHANNEL_VIEW_STATE, 24);
        lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lauShowHideRecord),
                             KI_CHANNEL_GUI_INTERNAL, 24);

        if (mpGuiCache != 0)
            mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);

        if (meSubState == E_SUBSTATE_MAP)
            CrashNavMap::OnLeave();

        UnloadLobbyResources();
        UnloadChallengeResources();
        UnloadMapResources();
        UnloadPauseResources();
        UnloadSettingsResources();
        UnloadViewEventResources();
    }
}

// FOLDED FROM BrnOnlineGameRoomPlayerInfo_wH_02.cpp
// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-H partfile 02: the load ladder.
//   CheckForCompletedLoads  @0x824A3FB8  (DWARF cpp:965)
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode + asm,
// scratchpad/waveB/GameSource_Gui_Flow_Screen_States_BrnOnlineGameRoomPlayerInfo.cpp.asm.txt).
//
// The screen's per-frame loading state machine: each E_SUBSTATE_LOADING_* rung either
// waits on the GUI cache's resource loader (EnsureResourcesAreLoaded over the rung's
// tuple table, then names the components it expects and starts the rung's apt movie) or
// waits on the cache's expected-apt-component list (AreAllAptComponentsInitialised, then
// clears the list and brings the rung's page up). The map rungs hand over to the
// CrashNavMap base.
//
// The two SetExpected*Components rungs this ladder calls
// (SetExpectedLobbyComponents @0x82484E78 / SetExpectedPauseComponents @0x82484FE0) are
// declared in the owning header; their bodies are in BrnOnlineGameRoomPlayerInfo_wH_17.cpp
// (they were blocked on a missing MenuComponent::AppendExpectedAptComponent declaration,
// which has since been added).

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface::PlayAptMovie
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"       // GuiComponent::AddOutputAptViewState
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache / GuiFlow
#include "GameSource/GameState/BrnGameStateSharedIO.h"                    // GameStateModuleIO::IsOnlineFreeBurnLobby

namespace BrnGui
{
    namespace
    {
        // The apt clip the options page transitions through (X360 aAptTransition_1, the
        // AddOutputAptViewState first argument at 0x824A4124).

        // The empty player-stats name the ladder pushes when it resets the panel
        // (X360 unk_820046A7, the zero-length rodata string).

        // The apt movie names PlayAptMovie is handed per rung (X360 off_82F27B7C /
        // off_82F27B84 / off_82F27BCC / off_82F27B14 / off_82F27BD0).

        // The apt level PlayAptMovie is called with on every rung (li r5, 3).
        const s32 KI_APT_MOVIE_LEVEL = 3;

        // The live row count SetupGroup activates on the 4-slot settings toggle group
        // (li r4, 4 at 0x824A4464 -- the group's item count, not a layout constant).
        const s32 KI_SETTINGS_TOGGLE_ITEMS = 4;
    }

    // @0x824A3FB8 -- DWARF cpp:965. Advance whichever load is in flight.
    void OnlineGameRoomPlayerInfo::CheckForCompletedLoads()
    {
        CGS_ASSERT_AT(mpGuiCache != 0, "mpGuiCache", CGS_ARTIST_FILE, 979);                                  // cpp:979

        switch (meSubState)
        {
        case E_SUBSTATE_LOADING_SCREEN:
            if (mpGuiCache->EnsureResourcesAreLoaded(maLobbyResourceTuplesToLoad,
                                                     static_cast<u32>(miNumLobbyResourcesToLoad)))
            {
                SetExpectedLobbyComponents();
                mpStateInterface->PlayAptMovie("ON_GR_PI", KI_APT_MOVIE_LEVEL);
                meSubState = E_SUBSTATE_LOADING_COMPONENTS;
            }
            break;

        case E_SUBSTATE_LOADING_PAUSE_SCREEN:
            if (mpGuiCache->EnsureResourcesAreLoaded(maPauseResourceTuplesToLoad,
                                                     static_cast<u32>(miNumPauseResourcesToLoad)))
            {
                SetExpectedPauseComponents();
                mpStateInterface->PlayAptMovie("ON_PAUSE", KI_APT_MOVIE_LEVEL);
                meSubState = E_SUBSTATE_LOADING_PAUSE_COMPONENTS;
            }
            break;

        case E_SUBSTATE_LOADING_PAUSE_COMPONENTS:
            if (mpGuiCache != 0 && mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
            {
                mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
                ShowPauseOptions();
                meSubState = E_SUBSTATE_PAUSE;
            }
            break;

        case E_SUBSTATE_LOADING_CHALLENGES_SCREEN:
            if (mpGuiCache->EnsureResourcesAreLoaded(maChallengeResourceTuplesToLoad,
                                                     static_cast<u32>(miNumChallengeResourcesToLoad)))
            {
                SetExpectedChallengesComponents();
                mpStateInterface->PlayAptMovie("ON_CHAL", KI_APT_MOVIE_LEVEL);
                meSubState = E_SUBSTATE_LOADING_CHALLENGES_COMPONENTS;
            }
            break;

        case E_SUBSTATE_LOADING_CHALLENGES_COMPONENTS:
            if (mpGuiCache != 0 && mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
            {
                mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
                ShowChallengesOptions();
                meSubState = E_SUBSTATE_CHALLENGES;
            }
            break;

        case E_SUBSTATE_LOADING_SETTINGS_SCREEN:
            if (mpGuiCache->EnsureResourcesAreLoaded(maSettingsResourceTuplesToLoad,
                                                     static_cast<u32>(miNumSettingsResourcesToLoad)))
            {
                SetExpectedSettingsComponents();
                mpStateInterface->PlayAptMovie("BrnCrashNavOptions", KI_APT_MOVIE_LEVEL);
                meSubState = E_SUBSTATE_LOADING_SETTINGS_COMPONENTS;
            }
            break;

        case E_SUBSTATE_LOADING_SETTINGS_COMPONENTS:
            if (mpGuiCache != 0 && mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
            {
                mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
                meSubState = E_SUBSTATE_SETTINGS;
                CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");                          // cpp:1153
                mSettingToggle.SetupGroup(KI_SETTINGS_TOGGLE_ITEMS, false);
                ShowSettingsOptions();
            }
            break;

        case E_SUBSTATE_LOADING_VIEW_EVENT_SCREEN:
            if (mpGuiCache->EnsureResourcesAreLoaded(maViewEventResourceTuplesToLoad,
                                                     static_cast<u32>(miNumViewEventResourcesToLoad)))
            {
                SetExpectedViewEventComponents();
                mpStateInterface->PlayAptMovie("ON_VWOPT", KI_APT_MOVIE_LEVEL);
                meSubState = E_SUBSTATE_LOADING_VIEW_EVENT_COMPONENTS;
            }
            break;

        case E_SUBSTATE_LOADING_VIEW_EVENT_COMPONENTS:
            if (mpGuiCache != 0 && mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
            {
                mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);

                // Inlined by the X360 into a pair of compares against 15/16 (0x824A450C).
                if (BrnGameState::GameStateModuleIO::IsOnlineFreeBurnLobby(
                        static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(mpGuiCache->GetOnlineGameModeOptions()->meGameMode)))
                {
                    if (mbShownFromPause)
                    {
                        UnloadViewEventResources();
                        HideHUDAndEnterMenus();
                        ShowPauseScreen();
                    }
                    else
                    {
                        UnloadViewEventResources();
                        HideViewEventPage();
                    }
                }
                else
                {
                    meSubState = E_SUBSTATE_VIEW_EVENT;
                    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");                      // cpp:1208
                    ShowViewEventOptions();
                }
            }
            break;

        case E_SUBSTATE_LOADING_COMPONENTS:
            if (mpGuiCache != 0 && mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
            {
                mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
                meSubState = E_SUBSTATE_MAIN;
                CGS_ASSERT_AT(mpGuiCache != 0, "mpGuiCache", CGS_ARTIST_FILE, 1011);                          // cpp:1011

                mOptionsAnimation.AddOutputAptViewState("apt_Transition",
                                                        KAPC_ANIMATION_STATES[1], false);
                mSelectedPlayerID = -1;

                // Clamp the displayed round into the game's round count (upper clamp
                // stored first, then the lower clamp -- two stores in the asm).
                if (miCurrentRoundDisplayed >= mpGuiCache->GetOnlineGameModeOptions()->miNumRounds - 1)
                {
                    miCurrentRoundDisplayed = mpGuiCache->GetOnlineGameModeOptions()->miNumRounds - 1;
                }
                if (miCurrentRoundDisplayed <= 0)
                {
                    miCurrentRoundDisplayed = 0;
                }

                mMessageText.SetText(KAC_PLAYER_INFO_STRING_ID);
                mRouteInfoDisplay.miNumComponentsLoaded = GuiNetworkRouteInfo::KI_NUM_COMPONENTS_TO_LOAD;
                mPlayerStatsDisplay.SetComponentLoaded();
                mPlayerStatsDisplay.SetState(GuiNetworkPlayerStats::E_STATE_VISIBLE);
                mRouteInfoDisplay.SetState(GuiNetworkRouteInfo::E_STATE_INVISIBLE);
                mPlayerStatsDisplay.SetInfo("", 0, 0, 0, false, mpGuiCache);
                mbHighlightedNewPlayer = false;
            }
            break;

        case E_SUBSTATE_LOADING_MAP_SCREEN:
            meSubState = E_SUBSTATE_LOADING_MAP_SCREEN_COMPONENTS;
            break;

        case E_SUBSTATE_LOADING_MAP_SCREEN_COMPONENTS:
            CrashNavMap::OnEnter();
            if (mbShownFromPause)
            {
                meMapState   = E_MAPSTATE_MAP;
                meScreenType = E_SCREEN_TYPE_ONLINE_FROM_PAUSE;
            }
            else
            {
                meScreenType = E_SCREEN_TYPE_ONLINE;
                meMapState   = E_MAPSTATE_MAP;
            }
            meSubState = E_SUBSTATE_MAP;
            break;

        default:
            break;
        }
    }
}

// FOLDED FROM BrnOnlineGameRoomPlayerInfo_wH_03.cpp
// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-H partfile 03: expect-list builders.
//   SetExpectedChallengesComponents @0x82485078  (DWARF cpp:3935, assert line 3962)
//   SetExpectedSettingsComponents   @0x8248C628  (DWARF cpp:4312, assert line 4354)
//
// Each is the "arm the loading-screen watcher for this page" step the sub-state ladder
// runs before it waits on CheckForCompletedLoads: assert the cache pointer, drop the
// SCREEN flow's previous expected-component list, then re-append exactly the components
// that page owns. Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode
// + asm; see scratchpad/waveB/...BrnOnlineGameRoomPlayerInfo.cpp.asm.txt).
//
// The X360 reaches the GuiCache name-taking face as sub_824F87C0 with r5 = <component>+4,
// i.e. the component's macName buffer -- that is CgsGui::GuiComponent::GetName(), so the
// reconstruction calls GetName() rather than reproducing the console +4 displacement (the
// host GuiComponent's name buffer is not at +4 once the vptr widens).
//
// SetExpectedViewEventComponents @0x824851A8 belongs with these two but is NOT in this
// file -- see the block comment at the end for the blocking reason.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT
#include "GameSource/Gui/BrnGuiCache.h"              // BrnGui::GuiCache (Clear/AppendExpectedAptComponent)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"      // BrnGui::GuiFlow (E_GUIFLOW_SCREEN)

// Asserts pass __FILE__; ARTIST's reads "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.cpp".

namespace BrnGui
{
    // @0x82485078 -- arm the SCREEN flow for the freeburn-challenge page: the challenge
    // mode toggle (which forwards its own row), the challenge list itself and the two
    // scroll-arrow animations. The asm passes flow 0 (E_GUIFLOW_SCREEN) to every call and
    // 1 (true) as MenuToggle::AppendExpectedAptComponent's trailing argument.
    void OnlineGameRoomPlayerInfo::SetExpectedChallengesComponents()
    {
        CGS_ASSERT_AT(mpGuiCache != 0,
            "mpGuiCache", __FILE__, 3962 /* ARTIST 0x824850AC */);   // cpp:3962

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);

        mChallengeToggle.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache, true);

        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                               mChallengeListComponent.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                               mChallengeListUpArrowAnimation.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                               mChallengeListDownArrowAnimation.GetName());
    }

    // @0x8248C628 -- arm the SCREEN flow for the settings page: the 4-slot toggle group is
    // the whole page, and its AppendExpectedAptComponent walks its own live rows.
    void OnlineGameRoomPlayerInfo::SetExpectedSettingsComponents()
    {
        CGS_ASSERT_AT(mpGuiCache != 0,
            "mpGuiCache", __FILE__, 4354 /* ARTIST 0x8248C65C */);   // cpp:4354

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);

        mSettingToggle.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache, true);
    }

    // ---------------------------------------------------------------------------------
    // SetExpectedViewEventComponents @0x824851A8 is bodied in BrnOnlineGameRoomPlayerInfo_wH_17.cpp.
}

// FOLDED FROM BrnOnlineGameRoomPlayerInfo_wH_04.cpp
// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-H partfile 04: the player-list heart.
//   ShowPlayerList     @0x824A5968  (DWARF cpp:3371)
//   HighlightNewPlayer @0x824991E0  (DWARF cpp:3494)
//   GetPlayerOptions   @0x82484CC0  (DWARF cpp:3263)
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (raw asm arbitrating; the
// Hex-Rays rendering is a hint only -- see the two notes below where it is WRONG).
// Ground truth: scratchpad/waveB/GameSource_Gui_Flow_Screen_States_
//               BrnOnlineGameRoomPlayerInfo.cpp.asm.txt
//
// ShowPlayerList repaints the whole 8-row lobby list: for each active player it walks
// TWO parallel GuiCache mirrors -- maPlayerInfo[i] (the 312-byte in-game status record)
// and mLobbyPlayerInfoList.maPlayerLobbyInfo[i] (the 56-byte lobby status record) -- and pushes eight status
// icon states per row, then blanks the unused rows and restores the highlight.
//
// TWO Hex-Rays MISREADINGS arbitrated from the assembly:
//   1. GetPlayerOptions: the pseudocode renders the unconditional option store as
//      `a2[v8] = 0xB86400000001LL`. The asm is
//          li r10, 1 ; lis r9, 0 ; ori r9, r9, 0xB864 ; stdx r10, r11, r29
//      -- r9 is the *offset* of GuiCache::mbIsOnlineHost, consumed by the `lbzx r11,
//      r3, r9` two instructions later; the value stored is r10 == 1. Hex-Rays folded
//      the unrelated `ori` into the immediate. The option id written is 1.
//   2. GetPlayerOptions: `LODWORD(v11) = *(OnlinePlayerI + 302); *a2 = v11` is the
//      compiler reusing the just-tested mbIsLocalPlayer byte (known 0 on that path) as
//      the constant 0. The pair of stores is ids[0] = 0, ids[1] = 2.
//
// De-inlining performed (AGENTS.md "Inlining reversal"), all X360-attested:
//   * The per-row `cache + 312*i + 0xAC80` address arithmetic carries GetOnlinePlayerInfo's
//     own two asserts VERBATIM (file "..\\GameSource\\Gui/BrnGuiCache.h", lines 3645/3646
//     -- exactly the pair in the committed GuiCache::GetOnlinePlayerInfo body), so the
//     compiler inlined that accessor; it is restored as the call.
//   * The two "Ended up with bad ..." asserts are the CgsDev::Assert::BeginAssert /
//     StrStream-into-gpcMessageBuffer / FireAssert / EndAssert machinery inlined at the
//     call site. Per the project convention (BrnReplayLoading.cpp / BrnImageGallery.cpp)
//     that collapses to one CGS_ASSERT carrying the verbatim rodata prefix; the streamed
//     `<< <status>` value is dropped with the machinery.
//
// X360-LITERAL AUDIT: every console offset/stride in these bodies is seeded by a member
// layout (39149 = mPlayerNameComponent.miHighlightedIndex, 87516 = mpGuiCache,
// 24928 + 1184*i = maIcons[i], 71104 = mPlayerStatsDisplay, 84880 =
// mPlayersButtonPromptsAnimation, cache+0xAC80/312*i = maPlayerInfo[i],
// cache+0xB640/56*i = mLobbyPlayerInfoList.maPlayerLobbyInfo[i], cache+0xA9DF = mGameOptions.mbRanked,
// cache+0xB864 = mbIsOnlineHost, row+0x100 = mPlayerName, row+248 =
// mLiveRevengeRelationship's +112 score word) -- so NONE of them is
// reproduced as a number here; the console values live in comments only. The two array
// strides that ARE reproduced (maPlayerInfo and maPlayerLobbyInfo row addressing) go through
// the named `u8[8][N]` cache members, whose row width is pinned host-side by
// static_assert(sizeof(InGamePlayerStatusData) == 312 / sizeof(LobbyPlayerStatusData) == 56)
// in those records' own headers.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache (friend access)
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h"      // InGamePlayerStatusData
#include "GameSource/Network/SharedIO/BrnNetworkModuleOnlineLobbyPlayerStatusInterface.h" // LobbyPlayerStatusData

namespace BrnGui
{
    namespace
    {
        // ---- AddEvent channel (the out-queue selector word) ---------------------------


        // The apt view-name every AddOutputAptViewState call on this screen targets.


        // ---- out-queue wire record (HighlightNewPlayer) --------------------------------
        // The X360 stack-builds { 4, 247, 12, <lobby mPlayerID> } and AddEvent()s it with
        // size 16 on channel 40 -- i.e. GuiEvent<247> carrying one 4-byte payload word,
        // the network player id of the row that just became highlighted.
        //
        // FLAG -- id/name mismatch, reported not resolved: the DecFIGS DWARF types
        // GuiEvent<247> as BrnGui::GuiEventNetworkFinishRound with an EMPTY payload
        // (CgsGuiEvent.h GuiEventWrapper<GuiEvent<247>,40>), which cannot be the event
        // ARTIST emits here. Neither BrnGuiEventTypeDefs.h nor BrnGuiDemangledEventTypes.h
        // declares a 247 in the committed tree, so this TU carries a file-local wire view
        // named for the role the asm gives it -- the same accommodation BrnInGame.cpp and
        // BrnCarSelectMain_wG_02.cpp make for demangled-only shapes. The record's shape is
        // asm-attested; only the NAME is consumer-derived.
        //
        // The two header words are HOST expressions, not the console's baked immediates.
        typedef CgsGui::GuiEvent<247> PlayerHighlightedHeader;

        struct GuiEventPlayerHighlightedWire : public PlayerHighlightedHeader
        {
            s32 miPlayerID;   // +0x0C

            explicit GuiEventPlayerHighlightedWire(s32 liPlayerID)
                : miPlayerID(liPlayerID)
            {
            }
            GuiEventPlayerHighlightedWire() {}
        };

        // ---- icon state indices -------------------------------------------------------
        // Each value is an index into the matching KAP_*_STATE_ID table declared on the
        // class (the state-id strings themselves are listed in
        // BrnOnlineGameRoomPlayerInfo.h and defined with their X360 rodata values in
        // BrnOnlineGameRoomPlayerInfo.cpp). Typed u32 so every SetState call binds
        // unambiguously to IconComponent::SetState(u32) rather than the const char*
        // sibling overload.
        const u32 KU_MARKED_MAN_VISIBLE        = 0;   // KAP_MARKED_MAN_STATE_ID[0]
        const u32 KU_MARKED_MAN_INVISIBLE      = 1;   //                        [1]
        const u32 KU_REVENGE_AHEAD             = 0;   // KAP_REVENGE_STATUS_STATE_ID[0]
        const u32 KU_REVENGE_EQUAL             = 1;   //                            [1]
        const u32 KU_REVENGE_BEHIND            = 2;   //                            [2]
        const u32 KU_REVENGE_INVISIBLE         = 3;   //                            [3]
        const u32 KU_READY_STATUS_INVISIBLE    = 4;   // KAP_READY_STATUS_STATE_ID[4]
        const u32 KU_VIDEO_STATUS_NO_CAMERA    = 0;   // KAP_VIDEO_STATUS_STATE_ID[0]
        const u32 KU_VIDEO_STATUS_INVISIBLE    = 4;   //                          [4]
        const u32 KU_VOIP_STATUS_NO_HEADSET    = 0;   // KAP_VOIP_STATUS_STATE_ID[0]
        const u32 KU_VOIP_STATUS_INVISIBLE     = 3;   //                         [3]
        const u32 KU_TEAM_STATUS_NONE          = 0;   // KAP_TEAM_STATUS_STATE_ID[0]
        const u32 KU_CONNECTION_TYPE_INVISIBLE = 2;   // KAP_CONNECTION_TYPE_STATE_ID[2]

        // The upper bound the VOIP-status assert compares against (`cmpwi r4, 3`). The
        // producing enum is CgsNetwork::ENetworkHeadsetPlayerStatus, which has no
        // committed home in the tree, so the X360-attested bound is spelled out here.
        const s32 KI_NETWORK_HEADSET_PLAYER_STATUS_COUNT = 3;

        // ---- player-option ids ---------------------------------------------------------
        // 64-bit SelectableGroup ids. The mapping is pinned by the sibling
        // ShowPlayerOptions @0x8248C4D0, whose stack table is indexed BY these ids:
        //   { "$ONLINE_PLAYER_OPTION_MARK_PLAYER" (or ..._UNMARK_PLAYER when the row is
        //     already a marked man), "$ONLINE_PLAYER_OPTION_SHOW_GAMERCARD_PLAYER",
        //     "$ONLINE_PLAYER_OPTION_SUBMIT_FEEDBACK_PLAYER",
        //     "$ONLINE_PLAYER_OPTION_KICK_PLAYER" }.
        const u64 KU64_PLAYER_OPTION_MARK_PLAYER     = 0;
        const u64 KU64_PLAYER_OPTION_SHOW_GAMERCARD  = 1;
        const u64 KU64_PLAYER_OPTION_SUBMIT_FEEDBACK = 2;
        const u64 KU64_PLAYER_OPTION_KICK_PLAYER     = 3;
    }

    // ================================================================================
    // ShowPlayerList @ 0x824A5968   (DWARF cpp:3371)
    //
    // Repaint the lobby player list and restore the highlight onto liSelectedPlayerID.
    // ================================================================================
    void OnlineGameRoomPlayerInfo::ShowPlayerList(s32 liSelectedPlayerID)
    {
        typedef BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData InGamePlayerStatusData;
        typedef BrnNetwork::BrnNetworkModuleIO::LobbyPlayerStatusData  LobbyPlayerStatusData;

        // X360 `lbzx this+39149 ; extsb ; cmpwi -1` -- the group's highlight cursor, with
        // "nothing highlighted" folded to row 0.
        s32 liHighlightedIndex = mPlayerNameComponent.miHighlightedIndex;
        if (liHighlightedIndex == -1)
        {
            liHighlightedIndex = 0;
        }

        // r5 == 0 -> lbWrap false.
        mPlayerNameComponent.SetupMenu(static_cast<s32>(mpGuiCache->muNumActivePlayers), false);

        // ---- the live rows -----------------------------------------------------------
        // The X360 re-loads muNumActivePlayers from the cache on every iteration (the
        // `lwzx r11, r31, 0xAC74` in the loop tail), so the bound is re-read here too.
        s32 liPlayerIndex = 0;
        for (; liPlayerIndex < static_cast<s32>(mpGuiCache->muNumActivePlayers); ++liPlayerIndex)
        {
            // GetOnlinePlayerInfo @0x8240F890, inlined by the X360 -- its two bounds
            // asserts survive verbatim (file "..\\GameSource\\Gui/BrnGuiCache.h",
            // lines 3645 / 3646).
            const InGamePlayerStatusData* lpPlayerInfo =
                mpGuiCache->GetOnlinePlayerInfoByIndex(liPlayerIndex);

            // The parallel lobby mirror row (cache +0xB640, 56-byte stride). No accessor
            // exists for it; the screen is a friend of GuiCache and reads the named
            // byte-storage member, exactly as GuiCache::GetOnlinePlayerInfo does for
            // maPlayerInfo.
            const LobbyPlayerStatusData* lpLobbyPlayerInfo =
                &mpGuiCache->GetLobbyPlayerList()->maPlayerLobbyInfo[liPlayerIndex];

            Icons& lIcons = maIcons[liPlayerIndex];

            // Row label: the online player name at record +256.
            mPlayerNameComponent.SetText(liPlayerIndex, lpPlayerInfo->mPlayerName.macName);

            // Marked-man flag at record +300: shown when set.
            lIcons.mMarkedManIcon.SetState(lpPlayerInfo->mbMarkedMan ? KU_MARKED_MAN_VISIBLE
                                                                     : KU_MARKED_MAN_INVISIBLE);

            // Revenge standing: the live-revenge score at record +248
            // (mLiveRevengeRelationship +112 == miCurrentScoreForPlayersPointOfView). The
            // X360 loads that word inline; the field is PRIVATE on LiveRevengeRelationship
            // and this screen is not a friend of it, so the original went through the
            // public accessor -- GetCurrentScoreForLocalPlayer(), the same identification
            // the committed BrnNetworkAggressiveDrivingManager.cpp:587 already makes for a
            // matching inlined +112 read. The X360 computes the <= 0 half with the
            // `cntlzw(cntlzw(x)) >> 5` sign idiom; de-optimised back to the comparison.
            const s32 liRevengeDelta =
                lpPlayerInfo->mLiveRevengeRelationship.GetCurrentScoreForLocalPlayer();
            u32 luRevengeState;
            if (liRevengeDelta > 0)
            {
                luRevengeState = KU_REVENGE_AHEAD;
            }
            else if (liRevengeDelta == 0)
            {
                luRevengeState = KU_REVENGE_EQUAL;
            }
            else
            {
                luRevengeState = KU_REVENGE_BEHIND;
            }
            lIcons.mRevengeStatusIcon.SetState(luRevengeState);

            // Ready column (lobby record +20).
            CGS_ASSERT_AT(lpLobbyPlayerInfo->meReadyStatus >= LobbyPlayerStatusData::E_READY_STATUS_HOST,
                       "lpLobbyPlayerInfo->meReadyStatus >= BrnNetwork::BrnNetworkModuleIO::LobbyPlayerStatusData::E_READY_STATUS_HOST", CGS_ARTIST_FILE, 3418);   // cpp:3418
            CGS_ASSERT_AT(lpLobbyPlayerInfo->meReadyStatus < LobbyPlayerStatusData::E_READY_STATUS_COUNT,
                       "lpLobbyPlayerInfo->meReadyStatus < BrnNetwork::BrnNetworkModuleIO::LobbyPlayerStatusData::E_READY_STATUS_COUNT", CGS_ARTIST_FILE, 3419);   // cpp:3419
            lIcons.mReadyStatusIcon.SetState(static_cast<u32>(lpLobbyPlayerInfo->meReadyStatus));

            // Video/camera column (record +284). Out-of-range falls back to "no camera"
            // AFTER firing the streamed assert -- it does not skip the SetState.
            CGS_ASSERT_AT(lpPlayerInfo->meCameraStatus >= BrnNetwork::E_CAMERA_STATUS_NONE,
                       "lpPlayerInfo->meCameraStatus >= BrnNetwork::E_CAMERA_STATUS_NONE", CGS_ARTIST_FILE, 3424);   // cpp:3424
            if (lpPlayerInfo->meCameraStatus < BrnNetwork::E_CAMERA_STATUS_COUNT)
            {
                lIcons.mVideoStatusIcon.SetState(static_cast<u32>(lpPlayerInfo->meCameraStatus));
            }
            else
            {
                // X360 streams the prefix then `<< meCameraStatus` into the assert buffer;
                // collapsed to the verbatim prefix per convention.
                CGS_FAIL("Ended up with bad meCameraStatus: ");   // cpp:3431
                lIcons.mVideoStatusIcon.SetState(KU_VIDEO_STATUS_NO_CAMERA);
            }

            // VOIP column (record +280); same shape as the camera column.
            CGS_ASSERT_AT(lpPlayerInfo->meVOIPStatus >= 0,
                       "lpPlayerInfo->meVOIPStatus >= CgsNetwork::E_NETWORK_HEADSET_PLAYER_STATUS_NONE", CGS_ARTIST_FILE, 3435);   // cpp:3435
            if (lpPlayerInfo->meVOIPStatus < KI_NETWORK_HEADSET_PLAYER_STATUS_COUNT)
            {
                lIcons.mVoipStatusIcon.SetState(static_cast<u32>(lpPlayerInfo->meVOIPStatus));
            }
            else
            {
                CGS_FAIL("Ended up with bad mVoipStatusIcon: ");   // cpp:3442
                lIcons.mVoipStatusIcon.SetState(KU_VOIP_STATUS_NO_HEADSET);
            }

            // Team column (lobby record +24).
            lIcons.mTeamStatusIcon.SetState(static_cast<u32>(lpLobbyPlayerInfo->mePlayerTeam));

            // Connection columns (lobby record +28 / +32). The local player's own row
            // carries no connection classification, so both icons go invisible.
            if (lpLobbyPlayerInfo->mbLocalPlayer)
            {
                lIcons.mGameConnectionTypeIcon.SetState(KU_CONNECTION_TYPE_INVISIBLE);
                lIcons.mVoipConnectionTypeIcon.SetState(KU_CONNECTION_TYPE_INVISIBLE);
            }
            else
            {
                lIcons.mGameConnectionTypeIcon.SetState(
                    static_cast<u32>(lpLobbyPlayerInfo->meGameConnectionType));
                lIcons.mVoipConnectionTypeIcon.SetState(
                    static_cast<u32>(lpLobbyPlayerInfo->meVoipConnectionType));
            }
        }

        // ---- blank the unused rows -----------------------------------------------------
        // Continues from the same cursor: when the list is empty the X360 branches
        // straight in here with the index still 0.
        for (; liPlayerIndex < KI_MAX_PLAYERS; ++liPlayerIndex)
        {
            Icons& lIcons = maIcons[liPlayerIndex];

            lIcons.mMarkedManIcon.SetState(KU_MARKED_MAN_INVISIBLE);
            lIcons.mRevengeStatusIcon.SetState(KU_REVENGE_INVISIBLE);
            lIcons.mReadyStatusIcon.SetState(KU_READY_STATUS_INVISIBLE);
            lIcons.mVideoStatusIcon.SetState(KU_VIDEO_STATUS_INVISIBLE);
            lIcons.mVoipStatusIcon.SetState(KU_VOIP_STATUS_INVISIBLE);
            lIcons.mTeamStatusIcon.SetState(KU_TEAM_STATUS_NONE);
            lIcons.mGameConnectionTypeIcon.SetState(KU_CONNECTION_TYPE_INVISIBLE);
            lIcons.mVoipConnectionTypeIcon.SetState(KU_CONNECTION_TYPE_INVISIBLE);
        }

        // ---- restore the highlight -----------------------------------------------------
        // Search the lobby mirror for the requested player id. The count is loaded ONCE
        // for this loop (`lwzx r10` before it, reused as the bound).
        const s32 liNumActivePlayers = static_cast<s32>(mpGuiCache->muNumActivePlayers);
        s32 liFoundIndex = 0;
        for (; liFoundIndex < liNumActivePlayers; ++liFoundIndex)
        {
            const LobbyPlayerStatusData* lpLobbyPlayerInfo =
                &mpGuiCache->GetLobbyPlayerList()->maPlayerLobbyInfo[liFoundIndex];

            if (lpLobbyPlayerInfo->mPlayerID == liSelectedPlayerID)
            {
                // Component vtable slot 12.
                mPlayerNameComponent.HighlightIndex(liFoundIndex);
                break;
            }
        }

        // The X360 re-reads the count here (it is NOT the cached bound above). The list is
        // re-highlighted when the requested player was not found (the cursor ran off the
        // end) or when no "new player" highlight has been published yet.
        const s32 liNumActivePlayersNow = static_cast<s32>(mpGuiCache->muNumActivePlayers);
        if (liNumActivePlayersNow > 0
            && (liFoundIndex == liNumActivePlayersNow || !mbHighlightedNewPlayer))
        {
            if (liHighlightedIndex >= liNumActivePlayersNow)
            {
                liHighlightedIndex = liNumActivePlayersNow - 1;
            }

            mPlayerNameComponent.HighlightIndex(liHighlightedIndex);
            HighlightNewPlayer();
        }
    }

    // ================================================================================
    // HighlightNewPlayer @ 0x824991E0   (DWARF cpp:3494)
    //
    // Publish the newly-highlighted row: announce its network player id, refresh the
    // stats panel from that player's online record, then show or hide the players-page
    // button prompts depending on whether the row has any selectable options at all.
    // ================================================================================
    void OnlineGameRoomPlayerInfo::HighlightNewPlayer()
    {
        typedef BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData InGamePlayerStatusData;
        typedef BrnNetwork::BrnNetworkModuleIO::LobbyPlayerStatusData  LobbyPlayerStatusData;

        const s32 liHighlightedIndex = mPlayerNameComponent.miHighlightedIndex;

        // The id comes from the LOBBY mirror row (cache +0xB640 + 56*index, mPlayerID at
        // row+16), not from the in-game record.
        const LobbyPlayerStatusData* lpLobbyPlayerInfo =
            &mpGuiCache->GetLobbyPlayerList()->maPlayerLobbyInfo[liHighlightedIndex];

        GuiEventPlayerHighlightedWire lPlayerHighlighted(lpLobbyPlayerInfo->mPlayerID);
        mpStateInterface->OutputGuiEvent(lPlayerHighlighted);   // X360 record size 16

        mbHighlightedNewPlayer = true;

        // Stats panel: name from the in-game record (+256), no value / no stats block,
        // the row index as the player image index, and the world-rank column gated on the
        // cached game params' "ranked" flag.
        const InGamePlayerStatusData* lpPlayerInfo =
            mpGuiCache->GetOnlinePlayerInfoByIndex(liHighlightedIndex);

        mPlayerStatsDisplay.SetInfo(lpPlayerInfo->mPlayerName.macName,
                                    0,
                                    0,
                                    liHighlightedIndex,
                                    mpGuiCache->GetOnlineGameModeOptions()->mbRanked,
                                    mpGuiCache);

        u64 lau64OptionIds[KI_MAX_PLAYER_OPTIONS];
        s32 liNumOptions;
        GetPlayerOptions(lau64OptionIds, &liNumOptions);

        mPlayersButtonPromptsAnimation.AddOutputAptViewState(
            "apt_Transition",
            liNumOptions != 0 ? KAPC_PLAYERS_BUTTON_PROMPT_ANIMATION_STATES[0]    // "visible"
                              : KAPC_PLAYERS_BUTTON_PROMPT_ANIMATION_STATES[1],   // "invisible"
            false);
    }

    // ================================================================================
    // GetPlayerOptions @ 0x82484CC0   (DWARF cpp:3263)
    //
    // Build the option-id list for the currently-highlighted player row. The ids are the
    // 64-bit SelectableGroup ids ShowPlayerOptions binds onto the option menu.
    // ================================================================================
    void OnlineGameRoomPlayerInfo::GetPlayerOptions(u64* lpau64OptionIds, s32* lpiNumOptions)
    {
        typedef BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData InGamePlayerStatusData;

        CGS_ASSERT_AT(lpiNumOptions != 0, "lpiNumOptions", CGS_ARTIST_FILE, 3279 /* ARTIST 0x82484CFC */);   // cpp:3279

        s32 liOptionIndex = 0;

        // The highlighted row's online record (mbIsLocalPlayer at +302).
        const InGamePlayerStatusData* lpPlayerInfo =
            mpGuiCache->GetOnlinePlayerInfoByIndex(mPlayerNameComponent.miHighlightedIndex);

        // Marking and feedback only apply to OTHER players.
        if (!lpPlayerInfo->mbIsLocalPlayer)
        {
            lpau64OptionIds[liOptionIndex++] = KU64_PLAYER_OPTION_MARK_PLAYER;
            lpau64OptionIds[liOptionIndex++] = KU64_PLAYER_OPTION_SUBMIT_FEEDBACK;
        }

        // The gamercard option is always offered (including on your own row).
        lpau64OptionIds[liOptionIndex++] = KU64_PLAYER_OPTION_SHOW_GAMERCARD;

        // Kicking needs host rights, and again only applies to other players. The X360
        // re-reads mpGuiCache and re-calls GetOnlinePlayerInfo here rather than reusing
        // the record above.
        if (mpGuiCache->mbIsOnlineHost)
        {
            if (!mpGuiCache->GetOnlinePlayerInfoByIndex(mPlayerNameComponent.miHighlightedIndex)
                     ->mbIsLocalPlayer)
            {
                lpau64OptionIds[liOptionIndex++] = KU64_PLAYER_OPTION_KICK_PLAYER;
            }
        }

        CGS_ASSERT_AT(liOptionIndex <= KI_MAX_PLAYER_OPTIONS,
                   "liOptionIndex <= KI_MAX_PLAYER_OPTIONS", CGS_ARTIST_FILE, 3302 /* ARTIST 0x82484DAC */);         // cpp:3302
        CGS_ASSERT_AT(liOptionIndex <= KI_MAX_DISPLAYABLE_OPTIONS,
                   "liOptionIndex <= KI_MAX_DISPLAYABLE_OPTIONS", CGS_ARTIST_FILE, 3303 /* ARTIST 0x82484DD0 */);    // cpp:3303

        *lpiNumOptions = liOptionIndex;
    }
}

// FOLDED FROM BrnOnlineGameRoomPlayerInfo_wH_05.cpp
// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-H partfile 05: the controller-input entry
// points.
//   HandleControllerInputPressed          @0x824AF018  cpp:1282 (assert cpp:1294)
//   HandleControllerInputReleased         @0x824A45B0  cpp:1361 (assert cpp:1373)
//   HandleControllerInputReleasedMapSubState @0x824982A8 cpp:2343
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode + asm). The
// first two are pure dispatchers: both open with the streamed "Invalid event sent to
// ..." assert on a null payload (the CgsDev::Assert::BeginAssert / StrStream / FireAssert
// / EndAssert idiom the X360 inlines -- written here as CGS_ASSERT, the same shape the
// sibling BrnCarSelectMain_wG_02.cpp uses for the copy-pasted twins of these asserts),
// then branch on meSubState. Note the asserts are NON-fatal in the X360 build: the
// dispatch runs whether or not the payload was null, so there is no early return here
// either.
//
// The released-map handler is the only one with a body of its own: it consumes the two
// map-zoom RELEASE actions off the controller event's payload (the file-local
// ControllerInputPayload view -- BrnBootProfile.cpp / BrnInGame.cpp precedent, since the
// in-queue hands the state the header-stripped payload) and drops the map back to the
// medium zoom / icon-selecting cursor.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent

namespace BrnGui
{
    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) --------------------------
        // OutputViewState / OutputInternalState -- the zoom-out release publishes the SAME
        // record on both. (The X360 re-stores the four identical words onto the same stack
        // slot before the second AddEvent, since AddEvent copies the record out; one local
        // reused across both calls is the same behaviour.)



        // ---- controller action ids (the payload's second word) -----------------------
        // The X360 compares the raw ids; the release pair is the map zoom-in / zoom-out
        // buttons coming back UP. FLAG: the producer-side enum name for these is not in
        // the recovered DWARF slice, so they are named after the role the handler gives
        // them (the map-substate press handler owns the matching 0x36/0x37 down ids).
        const s32 KI_ACTION_ZOOM_IN_RELEASE  = 56;   // 0x38
        const s32 KI_ACTION_ZOOM_OUT_RELEASE = 57;   // 0x39

        // ---- in-queue payload view ----------------------------------------------------
        // ARTIST controller event payload: pad id at +0x00, the button/action id at +0x04.
        // Only the action word is read here (`lwz r11, 4(r4)`).
        struct ControllerInputPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miButtonId;   // +0x04
        };

        // ---- out-queue wire record ----------------------------------------------------
        // The X360 stack-builds { 4, 558, 12, 0 } and AddEvent()s it with size 16 on
        // channels 41 and 42. Event 558 is the "set inspected event icon" view-state
        // event (a single 4-byte payload word == the event id being inspected, zero here
        // to clear it) -- its declaration lives in BrnGuiDemangledEventTypes.h, which
        // cannot be included alongside BrnGuiEventTypeDefs.h (they hard-collide on four
        // types), so this TU carries the file-local wire view, the same accommodation
        // BrnInGame.cpp and BrnCarSelectMain_wG_02.cpp make.
        //
        // The two header words are HOST expressions, not the console's baked immediates:
        // the payload size is sizeof(the payload word) and the payload offset is the host
        // sizeof of the 3-word GuiEvent header (12 on the X360, and 12 here too because
        // CgsModule::Event is empty -- but written as sizeof so it tracks the host layout).
        typedef CgsGui::GuiEvent<558> SetInspectedEventIconHeader;

        struct GuiEventSetInspectedEventIconWire : public SetInspectedEventIconHeader
        {
            u32 muEventID;   // +0x0C (X360 stores 0)

            explicit GuiEventSetInspectedEventIconWire(u32 luEventID)
                : muEventID(luEventID)
            {
            }
            GuiEventSetInspectedEventIconWire() {}
        };
    }

    // ---- HandleControllerInputPressed @ 0x824AF018 ---------------------------------
    // Route a controller-press event to the handler for the current sub-state. The X360
    // compiles this as a jump table over (meSubState - 10) with a 13-entry span; the
    // holes (12 E_SUBSTATE_LEAVING, 15/16 the map-load pair, 19 the input-disable delay)
    // and everything outside the span fall through the default and consume the press.
    void OnlineGameRoomPlayerInfo::HandleControllerInputPressed(const CgsModule::Event* lpEvent)
    {
        // cpp:1294. Streamed into the assert buffer by the X360; non-fatal.
        CGS_ASSERT_STREAM_AT(lpEvent != 0, "Invalid event sent to OnlineGameRoomPlayerInfo::HandleControllerInputPressed", CGS_ARTIST_FILE, 1294);


        switch (meSubState)
        {
            case E_SUBSTATE_MAIN:
                HandleControllerInputMainSubState(lpEvent);
                break;
            case E_SUBSTATE_FREE_BURNING:
                HandleControllerInputFreeBurnSubState(lpEvent);
                break;
            case E_SUBSTATE_PLAYER_OPTIONS:
                HandleControllerInputPlayerOptionsSubState(lpEvent);
                break;
            case E_SUBSTATE_PAUSE:
                HandleControllerInputPauseSubState(lpEvent);
                break;
            case E_SUBSTATE_MAP:
                HandleControllerInputPressedMapSubState(lpEvent);
                break;
            case E_SUBSTATE_CHALLENGES:
                HandleControllerInputChallengesSubState(lpEvent);
                break;
            case E_SUBSTATE_SECURITY_OPTIONS:
                HandleControllerInputSecurityOptionsSubState(lpEvent);
                break;
            case E_SUBSTATE_SETTINGS:
                HandleControllerInputSettingsSubState(lpEvent);
                break;
            case E_SUBSTATE_VIEW_EVENT:
                HandleControllerInputViewEventSubState(lpEvent);
                break;
            default:
                break;
        }
    }

    // ---- HandleControllerInputReleased @ 0x824A45B0 --------------------------------
    // The release counterpart. Only the map sub-state cares about button releases.
    void OnlineGameRoomPlayerInfo::HandleControllerInputReleased(const CgsModule::Event* lpEvent)
    {
        // cpp:1373. Streamed into the assert buffer by the X360; non-fatal.
        CGS_ASSERT_STREAM_AT(lpEvent != 0, "Invalid event sent to OnlineGameRoomPlayerInfo::HandleControllerInputReleased", CGS_ARTIST_FILE, 1373);


        if (meSubState == E_SUBSTATE_MAP)
        {
            HandleControllerInputReleasedMapSubState(lpEvent);
        }
    }

    // ---- HandleControllerInputReleasedMapSubState @ 0x824982A8 ---------------------
    // Both map-zoom buttons coming up return the map to the medium zoom with the cursor
    // back in icon-selecting mode. Letting go of ZOOM OUT additionally drops whatever
    // event icon was being inspected -- locally, and on the view-state and internal
    // channels via the id-558 record.
    void OnlineGameRoomPlayerInfo::HandleControllerInputReleasedMapSubState(const CgsModule::Event* lpEvent)
    {
        const s32 liAction =
            reinterpret_cast<const ControllerInputPayload*>(lpEvent)->miButtonId;

        if (liAction != KI_ACTION_ZOOM_IN_RELEASE && liAction != KI_ACTION_ZOOM_OUT_RELEASE)
        {
            return;
        }

        if (liAction == KI_ACTION_ZOOM_OUT_RELEASE)
        {
            muInspectingEventID = 0;

            GuiEventSetInspectedEventIconWire lClearInspected(0);
            mpStateInterface->OutputViewState(lClearInspected);      // X360 record size 16

            mpStateInterface->OutputInternalState(lClearInspected);      // X360 record size 16
        }

        // r4 == 1 == E_ZOOMFACTOR_MEDIUM, f1 == flt_82001CC0 == 0.0f, r6 == 0.
        mMainMapComponent.SetZoom(MainMapComponent::E_ZOOMFACTOR_MEDIUM, 0.0f, false);
        meCursorMode = E_CURSORMODE_SELECTING_ICONS;
    }
}

// FOLDED FROM BrnOnlineGameRoomPlayerInfo_wH_06.cpp
// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-H partfile 06: the main tab's controller
// handler and the player-options popup it raises.
//   b5-decomp/src/GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo_wH_06.cpp
//
//   HandleControllerInputMainSubState          @0x824A4670
//   ShowPlayerOptions                          @0x8248C4D0
//   HandleControllerInputPlayerOptionsSubState @0x82498120
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode arbitrated by
// the raw asm; scratchpad/waveB/...BrnOnlineGameRoomPlayerInfo.cpp.asm.txt). The owning
// header is BrnOnlineGameRoomPlayerInfo.h, whose banner carries the X360 member offsets
// every access below resolves through by NAME:
//   this+39149 -> mPlayerNameComponent.miHighlightedIndex (SelectableGroup +0xA5, s8)
//   this+75684 -> mPlayerStatsDisplay.meState (GuiNetworkPlayerStats +0x11E4)
//   this+75696 -> mRouteInfoDisplay          this+84320 -> mOptionsAnimation
//   this+43272 -> mOptionComponent           this+38984 -> mPlayerNameComponent
//   this+87504 -> meSubState                 this+87508 -> miCurrentRoundDisplayed
//   this+87512 -> mSelectedPlayerID          this+87516 -> mpGuiCache
//   this+87536 -> mbShownFromPause
// and on the cache side:
//   cache+0xA800 -> mGameOptions (the GuiEventNetworkGameParams
//                   payload mirror; the X360 forms it as `cache + 0x10000 - 0x5800`)
//   cache+0xA9D0 -> miOnlineNumRounds        cache+0xAC74 -> muNumActivePlayers
//   cache+0xB650 -> mLobbyPlayerInfoList.maPlayerLobbyInfo[i].mPlayerID (row stride 56 == the committed
//                   pointer-free sizeof(LobbyPlayerStatusData), so the host stride is
//                   the console stride; the row is reached by NAMED array index, the
//                   same way GuiCache::GetOnlinePlayerInfo reaches maPlayerInfo).
//
// Component vtable dispatch: the X360 calls the component virtuals through the flat
// component vtable (byte +24 slot6 Clear, +36 slot9 DisableSelectable, +52 slot13
// HighlightNext, +56 slot14 HighlightPrevious -- the keystone spec had these two BACKWARDS;
// a reviewer dumped MenuComponent's vtable (ctor @0x824FFEF8 stores &off_82074068) and read
// slot +0x38 == 0x824E4DE8 == MenuComponent::HighlightPrevious, slot +0x34 == 0x824E4DE0 ==
// the ICF-folded 0-arg HighlightNext). The x64 build lays out its own vtable,
// so every site below is a plain by-name call on the named member -- same treatment as
// the committed sibling screens (BrnOnlinePlay.cpp, BrnBootLegal.cpp).
//
// NOTE on the audio event: the X360 wire id for GuiAudioTriggerEvent is 457 (see the
// hand-built {100,457,12} record in this TU's TriggerSound), while the committed
// BrnGuiEventTypeDefs.h models it as GuiEvent<201>. That id discrepancy is a known
// cross-TU finding for the conductor; this partfile does not touch the header and calls
// the same OutputGuiEvent<GuiAudioTriggerEvent> instantiation the X360 calls.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // GuiEventWrapper
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface::OutputGuiEvent
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiAudioTriggerEvent
#include "GameSource/Gui/Events/BrnGuiEventNetworkGameParams.h"           // GuiNetworkRouteInfo::SetInfo arg
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h"      // InGamePlayerStatusData::mbMarkedMan
#include "GameSource/Network/SharedIO/BrnNetworkModuleOnlineLobbyPlayerStatusInterface.h" // LobbyPlayerStatusData::mPlayerID

#include <cstddef>                                                        // offsetof

// The canonical payload homes for the two network GUI events posted below are
// GameSource/Gui/BrnGuiDemangledEventTypes.h, but that header and BrnGuiEventTypeDefs.h
// are MUTUALLY EXCLUSIVE by construction (both define GuiAudioTriggerEvent and friends),
// and the owning header pulls BrnGuiCache.h -> BrnGuiEventTypeDefs.h in. So the two
// payloads are modelled here as file-local views with the demangled home's exact size
// and event id -- the same idiom the committed BrnInGame.cpp / BrnCarSelectMain_wG_03.cpp
// use for their in/out payloads.

namespace BrnGui
{
    namespace
    {


        // ARTIST controller event 6 payload: the pad id, then the action id the two
        // switches below dispatch on (the X360 reads the second word, `*(lpEvent + 4)`).
        struct ControllerInputPressedPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miActionId;   // +0x04
        };

        // OutputGuiEvent<BrnGui::GuiEventNetworkSelectedPlayerOption> @0x82436CA0 --
        // an 8-byte payload, event id 246 (the demangled home's size/id). The X360
        // stores the highlighted option's id word then the screen's selected player.
        struct GuiEventNetworkSelectedPlayerOptionPayload : public CgsModule::Event
        {
            s32 miOptionId;    // +0x00 (SelectableGroup::GetHighlightedId's low word)
            s32 miPlayerID;    // +0x04 (mSelectedPlayerID)

            s32 GetEventType() const { return 246; }
        };
        CGS_STATIC_ASSERT((sizeof(GuiEventNetworkSelectedPlayerOptionPayload) == 8),
                      "event 246 payload is 8 bytes");

        // OutputGuiEvent<BrnGui::GuiEventNetworkOutputPlayerTexture> @0x82436D40 --
        // an 8-byte payload, event id 264. FLAG: neither word has a recovered DWARF
        // name; the producers only pin their values (this screen writes { 0, -1 };
        // PhotoBoothComponent::Cancel / ::SendPlayerPictureEvent write { 1, -1 } and
        // { 4, -1 }), so the fields keep offset-derived names.
        struct GuiEventNetworkOutputPlayerTexturePayload : public CgsModule::Event
        {
            s32 miField_00;    // +0x00
            s32 miField_04;    // +0x04

            s32 GetEventType() const { return 264; }
        };
        CGS_STATIC_ASSERT((sizeof(GuiEventNetworkOutputPlayerTexturePayload) == 8),
                      "event 264 payload is 8 bytes");
        typedef CgsGui::GuiEventOut<GuiEventNetworkSelectedPlayerOptionPayload> GuiEventNetworkSelectedPlayerOptionOutputRecord;
        CGS_STATIC_ASSERT((sizeof(GuiEventNetworkSelectedPlayerOptionOutputRecord) == 20),
                      "event 246 channel-40 record is 12-byte header plus 8-byte payload");
        typedef CgsGui::GuiEventOut<GuiEventNetworkOutputPlayerTexturePayload> GuiEventNetworkOutputPlayerTextureOutputRecord;
        CGS_STATIC_ASSERT((sizeof(GuiEventNetworkOutputPlayerTextureOutputRecord) == 20),
                      "event 264 channel-40 record is 12-byte header plus 8-byte payload");

        // GuiAudioTriggerEvent::Construct's action argument at every site in this TU.
        const s32 KI_AUDIO_TRIGGER_ACTION = 7;

        // The apt clip the options popup transitions (the X360 literal at every
        // AddOutputAptViewState site in this file).


        // The audio labels the two sites trigger.
    }

    // ---- HandleControllerInputMainSubState @ 0x824A4670 ---------------------------
    // The player-list tab: row navigation, the route/round scroll pair, opening the
    // per-player options popup, and backing out of the lobby.
    void OnlineGameRoomPlayerInfo::HandleControllerInputMainSubState(const CgsModule::Event* lpEvent)
    {
        const s32 liAction =
            reinterpret_cast<const ControllerInputPressedPayload*>(lpEvent)->miActionId;

        switch (liAction)
        {
            case ')':   // 0x29 -- highlight the next player row.
                if (mPlayerNameComponent.miHighlightedIndex > -1)
                {
                    if (mPlayerNameComponent.HighlightPrevious())   // asm slot +0x38
                    {
                        HighlightNewPlayer();

                        GuiAudioTriggerEvent lAudio;
                        lAudio.Construct(KI_AUDIO_TRIGGER_ACTION, "",
                                         "MenuItemToggleDefault", "");
                        mpStateInterface->OutputGuiEvent<GuiAudioTriggerEvent>(lAudio);
                    }
                }
                break;

            case '*':   // 0x2A -- highlight the previous player row.
                if (mPlayerNameComponent.miHighlightedIndex > -1)
                {
                    if (mPlayerNameComponent.HighlightNext())   // asm slot +0x34
                    {
                        HighlightNewPlayer();

                        GuiAudioTriggerEvent lAudio;
                        lAudio.Construct(KI_AUDIO_TRIGGER_ACTION, "",
                                         "MenuItemToggleDefault", "");
                        mpStateInterface->OutputGuiEvent<GuiAudioTriggerEvent>(lAudio);
                    }
                }
                break;

            case '+':   // 0x2B -- scroll the displayed round DOWN (only while the
                        //         per-player stats panel is hidden; the X360 inlines
                        //         the gate as mPlayerStatsDisplay.meState != VISIBLE).
                if (!mPlayerStatsDisplay.IsVisible())
                {
                    --miCurrentRoundDisplayed;
                    if (miCurrentRoundDisplayed <= 0)
                    {
                        miCurrentRoundDisplayed = 0;
                    }

                    mRouteInfoDisplay.SetInfo(
                        miCurrentRoundDisplayed,
                        mpGuiCache->GetOnlineGameModeOptions());
                }
                break;

            case ',':   // 0x2C -- scroll the displayed round UP, clamped to the match's
                        //         round count (and then back up to zero).
                if (!mPlayerStatsDisplay.IsVisible())
                {
                    ++miCurrentRoundDisplayed;
                    if (miCurrentRoundDisplayed >= mpGuiCache->GetOnlineGameModeOptions()->miNumRounds - 1)
                    {
                        miCurrentRoundDisplayed = mpGuiCache->GetOnlineGameModeOptions()->miNumRounds - 1;
                    }
                    if (miCurrentRoundDisplayed <= 0)
                    {
                        miCurrentRoundDisplayed = 0;
                    }

                    mRouteInfoDisplay.SetInfo(
                        miCurrentRoundDisplayed,
                        mpGuiCache->GetOnlineGameModeOptions());
                }
                break;

            case '1':   // 0x31 -- open the highlighted player's options popup.
                // The X360 compares the active-player count SIGNED (cmpwi/ble).
                if (static_cast<s32>(mpGuiCache->muNumActivePlayers) > 0)
                {
                    const BrnNetwork::BrnNetworkModuleIO::LobbyPlayerStatusData* lpLobbyPlayer =
                        (&mpGuiCache->GetLobbyPlayerList()->maPlayerLobbyInfo[mPlayerNameComponent.miHighlightedIndex]);

                    mSelectedPlayerID = lpLobbyPlayer->mPlayerID;

                    if (ShowPlayerOptions())
                    {
                        meSubState = E_SUBSTATE_PLAYER_OPTIONS;
                    }
                }
                break;

            case '2':   // 0x32 -- back out of the player list.
            {
                GuiAudioTriggerEvent lAudio;
                lAudio.Construct(KI_AUDIO_TRIGGER_ACTION, "", "GO_BACK", "");
                mpStateInterface->OutputGuiEvent<GuiAudioTriggerEvent>(lAudio);

                mPlayerNameComponent.Clear();   // component slot 6

                if (mbShownFromPause)
                {
                    UnloadLobbyResources();
                    HideHUDAndEnterMenus();
                    ShowPauseScreen();

                    GuiEventNetworkOutputPlayerTexturePayload lPlayerTexture;
                    lPlayerTexture.miField_00 = 0;
                    lPlayerTexture.miField_04 = -1;
                    mpStateInterface->OutputGuiEvent(lPlayerTexture);
                }
                else
                {
                    UnloadLobbyResources();
                    HideLobby();
                }
                break;
            }

            default:
                break;
        }
    }

    // ---- ShowPlayerOptions @ 0x8248C4D0 -------------------------------------------
    // Build the per-player options popup for the highlighted row: pick the mark/unmark
    // label from the player's marked-man flag, ask GetPlayerOptions which options apply,
    // then bind them onto mOptionComponent and fade the popup in. Returns false (and
    // leaves the popup alone) when there is nothing to show.
    bool OnlineGameRoomPlayerInfo::ShowPlayerOptions()
    {
        const char* lapcOptionStrings[KI_MAX_PLAYER_OPTIONS];
        lapcOptionStrings[0] = KAC_MARK_PLAYER_STRING_ID;
        lapcOptionStrings[1] = KAC_SHOW_GAMERCARD_STRING_ID;
        lapcOptionStrings[2] = KAC_SUBMIT_FEEBDACK_STRING_ID;
        lapcOptionStrings[3] = KAC_KICK_PLAYER_STRING_ID;

        if (mpGuiCache->GetOnlinePlayerInfoByIndex(mPlayerNameComponent.GetHighlightedIndex())->mbMarkedMan)
        {
            lapcOptionStrings[0] = KAC_UNMARK_PLAYER_STRING_ID;
        }

        // GetPlayerOptions writes at most KI_MAX_PLAYER_OPTIONS ids (its own assert) but
        // SelectableGroup::SetIds below walks the group's selectable count, which
        // SetupMenu has just brought up to KI_MAX_DISPLAYABLE_OPTIONS -- so the console's
        // stack buffer spans the displayable count. (The exact declared extent is not
        // observable from the binary; the console frame reserves 128 bytes here.)
        u64 lau64OptionIds[KI_MAX_DISPLAYABLE_OPTIONS];
        s32 liNumOptions;
        GetPlayerOptions(lau64OptionIds, &liNumOptions);

        if (liNumOptions == 0)
        {
            return false;
        }

        mOptionComponent.SetupMenu(KI_MAX_DISPLAYABLE_OPTIONS, false);

        // The option id's LOW word is the label index (the X360 `ld` + `slwi 2` pair).
        s32 liOption = 0;
        for (; liOption < liNumOptions; ++liOption)
        {
            mOptionComponent.SetText(
                liOption, lapcOptionStrings[static_cast<s32>(lau64OptionIds[liOption])]);
        }

        for (; liOption < KI_MAX_DISPLAYABLE_OPTIONS; ++liOption)
        {
            mOptionComponent.Disable(liOption);
        }

        mOptionComponent.SetIds(lau64OptionIds);
        mOptionsAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[0], false);

        return true;
    }

    // ---- HandleControllerInputPlayerOptionsSubState @ 0x82498120 ------------------
    // The player-options popup: navigate the option rows, publish the chosen option for
    // the selected player, and fade the popup back out on either confirm or cancel.
    void OnlineGameRoomPlayerInfo::HandleControllerInputPlayerOptionsSubState(const CgsModule::Event* lpEvent)
    {
        const s32 liAction =
            reinterpret_cast<const ControllerInputPressedPayload*>(lpEvent)->miActionId;

        switch (liAction)
        {
            case ')':   // 0x29 -- highlight the next option row.
                mOptionComponent.HighlightPrevious();   // asm slot +0x38
                break;

            case '*':   // 0x2A -- highlight the previous option row.
                mOptionComponent.HighlightNext();       // asm slot +0x34
                break;

            case '1':   // 0x31 -- confirm: publish the highlighted option, then hide.
            {
                GuiEventNetworkSelectedPlayerOptionPayload lSelectedOption;
                lSelectedOption.miOptionId =
                    static_cast<s32>(mOptionComponent.GetHighlightedId());
                lSelectedOption.miPlayerID = mSelectedPlayerID;
                mpStateInterface->OutputGuiEvent(lSelectedOption);

                mOptionsAnimation.AddOutputAptViewState("apt_Transition",
                                                        KAPC_ANIMATION_STATES[1], false);
                mSelectedPlayerID = -1;
                meSubState = E_SUBSTATE_MAIN;
                break;
            }

            case '2':   // 0x32 -- cancel: play the back sound, then hide.
            {
                GuiAudioTriggerEvent lAudio;
                lAudio.Construct(KI_AUDIO_TRIGGER_ACTION, "", "GO_BACK", "");
                mpStateInterface->OutputGuiEvent<GuiAudioTriggerEvent>(lAudio);

                mOptionsAnimation.AddOutputAptViewState("apt_Transition",
                                                        KAPC_ANIMATION_STATES[1], false);
                mSelectedPlayerID = -1;
                meSubState = E_SUBSTATE_MAIN;
                break;
            }

            default:
                break;
        }
    }
}

// FOLDED FROM BrnOnlineGameRoomPlayerInfo_wH_07.cpp
// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-H partfile 07: the freeburn controller
// handler and the two pause/HUD transitions.
//   HandleControllerInputFreeBurnSubState @0x824A4A08  cpp:1520 (assert cpp:1532)
//   ShowPauseScreen                       @0x82484DE8  cpp:3792 (assert cpp:3826)
//   HideMenusAndReturnToHUD               @0x82499B80  cpp:3732 (assert cpp:3776)
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode + asm; see
// scratchpad/waveB/...BrnOnlineGameRoomPlayerInfo.cpp.asm.txt). Together these are the
// "player is driving in the online lobby" branch of the sub-state machine: the freeburn
// handler is the only input path live while the menus are down, ShowPauseScreen arms the
// pause load, and HideMenusAndReturnToHUD tears the whole menu stack back down.
//
// Both asserts the X360 fires here are NON-fatal (BeginAssert / FireAssert / EndAssert
// with no early-out), so control continues through them exactly as written -- the same
// shape the sibling BrnOnlineGameRoomPlayerInfo_wH_05.cpp and BrnCarSelectMain_wG_02.cpp
// use. HideMenusAndReturnToHUD's default-case assert is the streamed form (a stack
// CgsDev::StrStream over CgsDev::Assert::gpcMessageBuffer, "Substate " << meSubState <<
// " should not be leaving the menus.\n"); per project policy it is lowered to CGS_ASSERT
// with the static text and the streamed value is the switch scrutinee itself.
//
// The five out-queue posts at the tail of HideMenusAndReturnToHUD are written as typed
// records, NOT as the console's baked immediates: every payload-size / payload-offset
// header word is a host sizeof expression, and the AddEvent size argument is sizeof(the
// record). The X360's 16 / 20 byte literals are the 32-bit console record sizes.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue / PlayAptMovie
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache (freeburn gate flags)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiEventActivateCrashNav (id 191)

namespace BrnGui
{
    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) --------------------------


        // ---- controller action ids (the payload's second word) ------------------------
        // FLAG: the producer-side enum name (EGameInputActions) is not in the recovered
        // DWARF slice, so these four names are role-derived -- read off what THIS handler
        // does with each id, nothing else. All four literals appear exactly once in the
        // whole TU (this switch); no other handler in the TU compares the action word
        // against them, so their meaning outside this function is not attested here.
        const s32 KI_ACTION_PAUSE          = 0x2D;   // '-'
        const s32 KI_ACTION_MAP            = 0x2E;   // '.'
        const s32 KI_ACTION_EA_TRACK_HOLD  = 0x36;   // '6' -- arms mfTimeToDisableNextEATrack
        const s32 KI_ACTION_EA_TRACK_SKIP  = 0x37;   // '7' -- arms mfTimeUntilNextEATrack

        // The EA-trax debounce window both cases load (X360 flt_8205E5E0 == 0.30000001f);
        // the zero the skip case compares against is flt_82001CC0 == 0.0f.
        const f32 KF_EA_TRACK_DEBOUNCE_TIME = 0.3f;

        // ---- in-queue payload view ----------------------------------------------------
        // ARTIST controller event payload: pad id at +0x00, the button/action id at +0x04.
        // The in-queue hands the state the HEADER-STRIPPED payload, so this view starts at
        // CgsGui::GuiEventControllerInputPressed's miPadId (BrnInGame.cpp / wave-H 05
        // precedent). Only the action word is read here (`lwz r11, 4(r30)`).
        // ControllerInputPayload is declared above in this translation unit.





        // ---- out-queue wire records ---------------------------------------------------
        // Each is a CgsGui::GuiEvent<N> header { payload size, event id, payload offset }
        // followed by the payload, published through
        // mpStateInterface->GetOutputEventQueue()->AddEvent(record, channel, sizeof(record)).
        // BrnGuiDemangledEventTypes.h (the home of ids 65 and 148) hard-collides with
        // BrnGuiEventTypeDefs.h, which this file needs for GuiEventActivateCrashNav, so the
        // two are carried as file-local wire views -- the same accommodation BrnInGame.cpp,
        // BrnCarSelectMain_wG_02.cpp and BrnOnlineGameRoomPlayerInfo_wH_05.cpp make.

        // Id 533 -- the screen-flow command the online back-out paths post (the committed
        // BrnOnlinePlay.cpp posts the identical { 1, 533, 12, <payload byte> } record on
        // channel 40 from its own GO_BACK path). Payload size 1.
        typedef CgsGui::GuiEvent<533> ScreenFlowCommandHeader;
        struct GuiScreenFlowCommandWire : public ScreenFlowCommandHeader
        {
            // The X360 posts this record WITHOUT writing the payload byte: its block
            // (0x82499D88..0x82499DAC) stores only record+0 = 1, record+4 = 0x215 and
            // record+8 = 12 before AddEvent(..., 40, 16). The byte is still DETERMINISTIC,
            // because the preceding id-191 record left it: 0x82499D58 stw r29(=1) -> [+0],
            // 0x82499D64 stw r26 -> [+4], 0x82499D68 ld r11 <- [+0], 0x82499D78 std r11 ->
            // [+0x0C], i.e. the big-endian word 0x00000001 lands at [+0x0C..+0x0F] and the
            // byte AT +0x0C is 0x00. The published value is therefore 0.
            //
            // That 0 is an ENDIANNESS artefact of the console, not a stored constant: the
            // identical little-endian aliasing on the host would leave 0x01 there. So the
            // byte is written explicitly to reproduce what the X360 publishes, rather than
            // left to inherit whatever the host's stack reuse happens to put in it.
            u8 muPayload;   // +0x0C (never stored by the X360; inherited value is 0)

            GuiScreenFlowCommandWire()
                : muPayload(0)
            {
            }
        };










    }

    // ---- HandleControllerInputFreeBurnSubState @ 0x824A4A08 ------------------------
    // The only input path live while the player is driving with the lobby menus down.
    // Everything is gated on the cache's freeburn input flag; the pause and map entries
    // are additionally gated on the menu lock, and the two EA-trax buttons just arm their
    // debounce timers (Update runs them down).
    void OnlineGameRoomPlayerInfo::HandleControllerInputFreeBurnSubState(const CgsModule::Event* lpControllerEvent)
    {
        CGS_ASSERT_AT(lpControllerEvent != 0,
            "lpControllerEvent",
                CGS_ARTIST_FILE, 1532 /* ARTIST 0x824A4A34 */);   // cpp:1532 (non-fatal)


        if (mpGuiCache == 0 || mpGuiCache->mbFreeBurnInputDisabled)
        {
            return;
        }

        const s32 liAction =
            reinterpret_cast<const ControllerInputPayload*>(lpControllerEvent)->miButtonId;

        switch (liAction)
        {
            case KI_ACTION_PAUSE:
                if (!mpGuiCache->mbFreeBurnMenuLocked)
                {
                    HideHUDAndEnterMenus();
                    ShowPauseScreen();
                }
                break;

            case KI_ACTION_MAP:
                if (!mpGuiCache->mbFreeBurnMenuLocked)
                {
                    ShowMapScreen();
                    mbShownFromPause = false;
                }
                break;

            case KI_ACTION_EA_TRACK_HOLD:
                mfTimeToDisableNextEATrack = KF_EA_TRACK_DEBOUNCE_TIME;
                break;

            case KI_ACTION_EA_TRACK_SKIP:
                if (mfTimeUntilNextEATrack <= 0.0f)
                {
                    mfTimeUntilNextEATrack = KF_EA_TRACK_DEBOUNCE_TIME;
                }
                break;

            default:
                break;
        }
    }

    // ---- ShowPauseScreen @ 0x82484DE8 ----------------------------------------------
    // Arm the pause-screen load. Every page that can raise the pause menu is listed in
    // the assert; the state itself is the whole body.
    void OnlineGameRoomPlayerInfo::ShowPauseScreen()
    {
        // cpp:3826 -- the X360 fires the bare expression text (non-fatal).
        CGS_ASSERT_AT(meSubState == E_SUBSTATE_FREE_BURNING ||
                   meSubState == E_SUBSTATE_MAIN ||
                   meSubState == E_SUBSTATE_MAP ||
                   meSubState == E_SUBSTATE_CHALLENGES ||
                   meSubState == E_SUBSTATE_SECURITY_OPTIONS ||
                   meSubState == E_SUBSTATE_VIEW_EVENT ||
                   meSubState == E_SUBSTATE_SETTINGS,
                   "( meSubState == E_SUBSTATE_FREE_BURNING ) || ( meSubState == E_SUBSTATE_MAIN ) || "
                       "( meSubState == E_SUBSTATE_MAP ) || ( meSubState == E_SUBSTATE_CHALLENGES) || "
                       "( meSubState == E_SUBSTATE_SECURITY_OPTIONS) || ( meSubState == E_SUBSTATE_VIEW_EVENT) || "
                       "( meSubState == E_SUBSTATE_SETTINGS)",
                       CGS_ARTIST_FILE,
                       3826 /* ARTIST 0x82484E44 */);


        meSubState = E_SUBSTATE_LOADING_PAUSE_SCREEN;
    }

    // ---- HideMenusAndReturnToHUD @ 0x82499B80 --------------------------------------
    // Drop the whole menu stack and put the player back behind the wheel: leave the map
    // page if that is where we are, park the machine in the controller-disable delay for
    // KI_CONTROLLER_DISABLE_DELAY_FRAME_COUNT frames, then publish the five view/flow
    // commands that unmount the lobby movie and bring the HUD back.
    void OnlineGameRoomPlayerInfo::HideMenusAndReturnToHUD()
    {
        switch (meSubState)
        {
            case E_SUBSTATE_LOADING_PAUSE_SCREEN:       // 1
            case E_SUBSTATE_LOADING_PAUSE_COMPONENTS:   // 2
            case E_SUBSTATE_FREE_BURNING:               // 11
            case E_SUBSTATE_PAUSE:                      // 14
            case E_SUBSTATE_SECURITY_OPTIONS:           // 20
                break;

            case E_SUBSTATE_MAP:                        // 17
                CrashNavMap::OnLeave();
                break;

            default:
                // cpp:3776 -- streamed as "Substate " << meSubState << " should not be
                // leaving the menus.\n"; lowered to the static text per project policy.
                CGS_FAIL("Substate  should not be leaving the menus.\n");
                break;
        }

        meSubState        = E_SUBSTATE_CONTROLLER_DISABLE_DELAY;
        miDelayFrameCount = KI_CONTROLLER_DISABLE_DELAY_FRAME_COUNT;

        // 1. Hand car control back to the player (X360 record size 16, channel 40).
        GuiRequestCarControlChangeEvent lChangeEvent;
        lChangeEvent.mbPlayerShouldHaveControl = true;
        mpStateInterface->OutputGuiEvent(lChangeEvent);

        // 2. Unmount the lobby apt movie: the inlined CgsGui::GuiEventPlayAptMovie record
        //    { 8, 18, 12, "", 3 } on channel 41 (the empty name is the &unk_820046A7
        //    rodata sentinel). X360 record size 20.
        mpStateInterface->PlayAptMovie("", 3);

        // 3. Re-activate CrashNav (X360 record { 8, 191, 12, 1, 0 }, size 20, channel 40).
        GuiEventActivateCrashNav lActivateCrashNav;
        lActivateCrashNav.muActivate = 1u;
        lActivateCrashNav.muParam = 0;
        mpStateInterface->OutputGuiEvent(lActivateCrashNav);

        // 4. The id-533 screen-flow command (X360 record size 16, channel 40).
        GuiScreenFlowCommandWire lScreenFlowCommand;
        mpStateInterface->OutputGuiEvent(lScreenFlowCommand);

        // 5. Show the HUD again (X360 record size 16, channel 40).
        GuiEventShowHideHud lShowHideHudEvent;
        lShowHideHudEvent.mbShow = true;
        mpStateInterface->OutputGuiEvent(lShowHideHudEvent);
    }
}

// FOLDED FROM BrnOnlineGameRoomPlayerInfo_wH_08.cpp
// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-H partfile 08: the settings page.
//   HandleControllerInputSettingsSubState @0x824AD248  cpp:2004
//   ShowSettingsScreen                    @0x8249A780  cpp:4268 (assert cpp:4311)
//   TriggerSound                          @0x8249ADB0  cpp:4631 (assert cpp:4709)
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode + asm; see
// scratchpad/waveB/...BrnOnlineGameRoomPlayerInfo.cpp.asm.txt). These three are the
// lobby's "settings" page: ShowSettingsScreen arms the load, the sub-state handler runs
// the 4-row toggle group and commits/cancels it, and TriggerSound is the per-move audio
// cue both of the highlight paths raise.
//
// VTABLE SLOT -> METHOD, dumped rather than inferred. The four highlight vcalls in the
// sub-state handler go through the MenuToggleGroupVarSize<4> vtable @0x82074750, read out
// of the image (scratchpad/waveH08/dump_vtbl_out.txt):
//     +0x18 (slot  6) MenuToggleGroupVarSize<4>::Clear
//     +0x28 (slot 10) SelectableGroup::HighlightNext(bool)
//     +0x2C (slot 11) SelectableGroup::HighlightPrevious(bool)
//     +0x34 (slot 13) MenuToggleGroupVarSize<4>::HighlightNextItem()
//     +0x38 (slot 14) MenuToggleGroupVarSize<4>::HighlightPreviousItem()
// so action 0x29 moves the highlighted ROW backwards and 0x2A forwards (both are the
// one-argument base virtuals -- the asm sets r4 = 0 for them and leaves r4 untouched for
// the no-argument item virtuals), while 0x2B/0x2C change the highlighted row's VALUE and
// are the only pair that reports the change through HandleSettingChanged(). This INVERTS
// the slot map the keystone spec carried; the dumped vtable is the authority.
//
// The out-queue records are written as typed wire views with HOST sizeof/offset
// expressions -- never the console's baked 100 / 8 / 4 / 12 / 20 / 112 immediates, which
// are 32-bit-console record sizes. Ids 457 / 463 / 474 are homed only in
// BrnGuiDemangledEventTypes.h, which hard-collides with BrnGuiEventTypeDefs.h (needed
// here for GuiOverlayRequest / GuiAudioTriggerEvent / GuiEventActivateCrashNav), so they
// are carried as file-local wire views -- the same accommodation BrnInGame.cpp,
// BrnCarSelectMain_wG_02.cpp and BrnOnlineGameRoomPlayerInfo_wH_07.cpp make.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // GuiCache::GetOptionsDataProfile
#include "GameSource/Gui/BrnGuiOptionsDataProfile.h"                      // OptionsDataProfile volume getters
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiOverlayRequest / GuiAudioTriggerEvent / GuiEventActivateCrashNav

namespace BrnGui
{
    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) --------------------------


        // ---- controller action ids (the in-queue payload's second word) ----------------
        // FLAG: the producer-side enum name (EGameInputActions) is not in the recovered
        // DWARF slice, so these are named for the role the X360 code gives them. The eight
        // ids TriggerSound knows split into exactly two families by the audio label it
        // picks: 0x25/0x26/0x29/0x2A are ROW moves ("MenuToggleDefault") and
        // 0x27/0x28/0x2B/0x2C are toggle-VALUE moves ("MenuItemToggleDefault"). Only the
        // 0x29..0x2C half is dispatched by this screen's settings handler, so only that
        // half's prev/next direction is attested (by the vtable slot each case calls);
        // 0x25..0x28 are known by family only.
        const s32 KI_ACTION_MENU_MOVE_A = 0x25;   // '%'  row-move family, direction unattested
        const s32 KI_ACTION_MENU_MOVE_B = 0x26;   // '&'  row-move family, direction unattested
        const s32 KI_ACTION_ITEM_MOVE_A = 0x27;   // '\'' value-move family, direction unattested
        const s32 KI_ACTION_ITEM_MOVE_B = 0x28;   // '('  value-move family, direction unattested
        const s32 KI_ACTION_MENU_PREVIOUS = 0x29; // ')'  -> SelectableGroup::HighlightPrevious
        const s32 KI_ACTION_MENU_NEXT     = 0x2A; // '*'  -> SelectableGroup::HighlightNext
        const s32 KI_ACTION_ITEM_PREVIOUS = 0x2B; // '+'  -> HighlightPreviousItem
        const s32 KI_ACTION_ITEM_NEXT     = 0x2C; // ','  -> HighlightNextItem
        const s32 KI_ACTION_SELECT        = 0x31; // '1'  accept the page
        const s32 KI_ACTION_BACK          = 0x32; // '2'  back out of the page

        // The GuiAudioTriggerEvent::meAction value every menu cue in the GUI flow uses
        // (X360 `li r4, 7`; the same literal the committed BrnBootProfile.cpp passes).
        // FLAG: the action enum's name is not in the recovered DWARF slice.
        const s32 KI_AUDIO_ACTION_MENU_CUE = 7;

        // The two audio labels the X360 selects between (rodata literals).

        // The "cancel the settings edit?" overlay this page raises when the player backs
        // out with unsaved changes.

        // The audio label the clean back-out path plays.

        // ---- in-queue payload view ----------------------------------------------------
        // ARTIST controller event payload: pad id at +0x00, the button/action id at +0x04.
        // The in-queue hands the state the HEADER-STRIPPED payload, so this view starts at
        // the pad id (BrnInGame.cpp / wave-H 05 / wave-H 07 precedent). Only the action
        // word is read here (`lwz r30, 4(r4)`).
        // ControllerInputPayload is declared above in this translation unit.





        // ---- out-queue wire records ---------------------------------------------------

        // The record the inlined OutputGuiEvent<BrnGui::GuiAudioTriggerEvent> builds (the
        // out-of-line instantiation @0x82436890 builds the identical one):
        // { 100, 457, 12, the 100-byte payload }, channel 40, 112 bytes. The committed PC
        // GuiAudioTriggerEvent already carries that 12-byte queue header in front of its
        // 100-byte payload, so the event object IS the record -- but its GuiEvent<201>
        // base seeds the payload-size word 0 and the id 201, while the X360 wire words are
        // the payload size and id 457. This view restores them without forking the payload
        // type. (Cross-TU finding: 201 is GuiEventHideDriveThru's id; 457 is the id both
        // this record and BrnGuiDemangledEventTypes.h attest for the audio trigger.)
        const u32 KU_WIRE_ID_AUDIO_TRIGGER = 457;

        typedef CgsGui::GuiEventOut<GuiAudioTriggerEvent> GuiAudioTriggerWire;

        // Id 463 == BrnGui::GuiEventAudioSettings. The record
        // OutputGuiEvent<GuiEventAudioSettings> @0x82493728 builds: { 8, 463, 12, the two
        // volume words }, channel 40, 20 bytes. Field order is the X360 store order --
        // GetMusicVolume lands in the first word, GetSFXVolume in the second.
        typedef CgsGui::GuiEvent<463> AudioSettingsHeader;
        struct GuiEventAudioSettingsWire : public AudioSettingsHeader
        {
            s32 miMusicVolume;   // +0x0C
            s32 miSFXVolume;     // +0x10

            GuiEventAudioSettingsWire(s32 liMusicVolume, s32 liSFXVolume)
                : miMusicVolume(liMusicVolume)
                , miSFXVolume(liSFXVolume)
            {
            }
            GuiEventAudioSettingsWire() {}
        };

        // Id 474 == BrnGui::GuiEventVoipSettings. The record
        // OutputGuiEvent<GuiEventVoipSettings> @0x82493CE8 builds: { 4, 474, 12, the voip
        // volume word }, channel 40, 16 bytes. (The X360 reuses the audio-settings stack
        // slot and overwrites its FIRST word with the voip volume, so the voip volume is
        // payload word 0 and the trailing sfx word is outside this record.)
        typedef CgsGui::GuiEvent<474> VoipSettingsHeader;
        struct GuiEventVoipSettingsWire : public VoipSettingsHeader
        {
            s32 miVoipVolume;    // +0x0C

            explicit GuiEventVoipSettingsWire(s32 liVoipVolume)
                : miVoipVolume(liVoipVolume)
            {
            }
            GuiEventVoipSettingsWire() {}
        };

        // The OutputGuiEvent<BrnGui::GuiOverlayRequest> wire record: { 288, 184, 16,
        // <pad>, the 288-byte request }, channel 40, 304 bytes (identical shape to
        // BrnInGame.cpp's / BrnCarSelectMain_wG_02.cpp's; the payload is 8-aligned, so the
        // payload-offset word is 16, not 12).
        struct GuiOverlayRequestWire
        {
            GuiOverlayRequest mRequest;   // the OutputGuiEvent payload

            GuiOverlayRequestWire()
            {
            }
        };











        // Record-size pins. Every one of these payloads is pointer-free, so the X360's
        // AddEvent size immediate must survive unchanged on the x64 host; if a host
        // layout ever drifts, this is where it is caught rather than on the wire.
        CGS_STATIC_ASSERT((sizeof(GuiAudioTriggerWire) == 112), "audio trigger record is 112 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiEventAudioSettingsWire>) == 20), "audio settings record is 20 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiEventVoipSettingsWire>) == 16), "voip settings record is 16 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiOverlayRequest>) == 304), "overlay request record is 304 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiEventActivateCrashNav>) == 20), "activate crashnav record is 20 bytes");
    }

    // ---- TriggerSound @ 0x8249ADB0 -------------------------------------------------
    // Raise the menu-move audio cue for a controller action: pick the label for the
    // action's family and publish a GuiAudioTriggerEvent with it.
    void OnlineGameRoomPlayerInfo::TriggerSound(s32 leGameInputAction)
    {
        const char* lpcLabel = 0;

        switch (leGameInputAction)
        {
            case KI_ACTION_MENU_MOVE_A:
            case KI_ACTION_MENU_MOVE_B:
            case KI_ACTION_MENU_PREVIOUS:
            case KI_ACTION_MENU_NEXT:
                lpcLabel = "MenuToggleDefault";
                break;

            case KI_ACTION_ITEM_MOVE_A:
            case KI_ACTION_ITEM_MOVE_B:
            case KI_ACTION_ITEM_PREVIOUS:
            case KI_ACTION_ITEM_NEXT:
                lpcLabel = "MenuItemToggleDefault";
                break;

            default:
                break;
        }

        // cpp:4709 -- non-fatal, and the X360 sinks it into the switch's default arm
        // (the other arms provably set lpcLabel). Either placement runs the same code:
        // the event is constructed with the null label on the unrecognised path.
        CGS_ASSERT_AT(lpcLabel, "lpcLabel", CGS_ARTIST_FILE, 4709);

        GuiAudioTriggerEvent lAudioPayload;
        lAudioPayload.Construct(KI_AUDIO_ACTION_MENU_CUE, "", lpcLabel, "");

        mpStateInterface->OutputGuiEvent(lAudioPayload);
    }

    // ---- ShowSettingsScreen @ 0x8249A780 -------------------------------------------
    // Arm the settings-page load: park the machine in the loading sub-state, take the
    // front-end map and the HUD down, bring the accept help-prompt up, and start the edit
    // with no pending change.
    void OnlineGameRoomPlayerInfo::ShowSettingsScreen()
    {
        // cpp:4311 -- the X360 fires the bare expression text (non-fatal).
        CGS_ASSERT_AT((meSubState == E_SUBSTATE_PAUSE) || (meSubState == E_SUBSTATE_FREE_BURNING),
                   "(meSubState == E_SUBSTATE_PAUSE) || (meSubState == E_SUBSTATE_FREE_BURNING)", CGS_ARTIST_FILE, 4311);

        meSubState = E_SUBSTATE_LOADING_SETTINGS_SCREEN;

        // Deactivate CrashNav (X360 record { 8, 191, 12, 0, 0 }, size 20, channel 40).
        GuiEventActivateCrashNav lDeactivateCrashNav;
        lDeactivateCrashNav.muActivate = 0u;
        lDeactivateCrashNav.muParam = 0;
        mpStateInterface->OutputGuiEvent(lDeactivateCrashNav);

        // Take the HUD down (X360 record { 1, 148, 12, 0 }, size 16, channel 40).
        GuiEventShowHideHud lShowHideHudEvent;
        lShowHideHudEvent.mbShow = false;
        mpStateInterface->OutputGuiEvent(lShowHideHudEvent);

        mHelpItemAccept.Construct(KAC_HELPITEM_ACCEPT, mpStateInterface, 0);

        mbSettingsChanged = false;
    }

    // ---- HandleControllerInputSettingsSubState @ 0x824AD248 ------------------------
    // The settings page's input map. The two row moves and the two value moves each play
    // their cue only when the group actually moved; a value move additionally reports the
    // edit through HandleSettingChanged(). Accept commits the edited options back to the
    // profile and republishes them; back either raises the cancel-confirm overlay (when
    // there is an unsaved edit) or restores the profile's audio/voip volumes and leaves.
    // Both exit paths tear the toggle group down and then return to whichever page the
    // settings screen was entered from.
    void OnlineGameRoomPlayerInfo::HandleControllerInputSettingsSubState(const CgsModule::Event* lpControllerEvent)
    {
        const s32 liAction =
            reinterpret_cast<const ControllerInputPayload*>(lpControllerEvent)->miButtonId;

        switch (liAction)
        {
            case KI_ACTION_MENU_PREVIOUS:
                if (mSettingToggle.HighlightPrevious(false))
                {
                    TriggerSound(liAction);
                }
                break;

            case KI_ACTION_MENU_NEXT:
                if (mSettingToggle.HighlightNext(false))
                {
                    TriggerSound(liAction);
                }
                break;

            case KI_ACTION_ITEM_PREVIOUS:
                if (mSettingToggle.HighlightPreviousItem())
                {
                    HandleSettingChanged();
                    TriggerSound(liAction);
                }
                break;

            case KI_ACTION_ITEM_NEXT:
                if (mSettingToggle.HighlightNextItem())
                {
                    HandleSettingChanged();
                    TriggerSound(liAction);
                }
                break;

            case KI_ACTION_SELECT:
                mCrashNavOptionsData.SetToProfile(mpGuiCache->GetMenuOptionsData());
                mCrashNavOptionsData.OutputEvents(mpGuiCache->GetMenuOptionsData(),
                                                  mpStateInterface);

                mSettingToggle.Unloaded();
                mSettingToggle.Clear();

                if (mbShownFromPause)
                {
                    UnloadSettingsResources();
                    HideHUDAndEnterMenus();
                    ShowPauseScreen();
                }
                else
                {
                    UnloadSettingsResources();
                    HideSettingsPage();
                }
                break;

            case KI_ACTION_BACK:
                if (mbSettingsChanged)
                {
                    GuiOverlayRequestWire lWire;
                    lWire.mRequest.Construct("CNCnclCnfrm");

                    mpStateInterface->OutputGuiEvent(lWire.mRequest);
                }
                else
                {
                    GuiEventAudioSettings lAudioSettings;
                    lAudioSettings.miMusicVolume = mpGuiCache->GetMenuOptionsData()->GetMusicVolume();
                    lAudioSettings.miSfxVolume = mpGuiCache->GetMenuOptionsData()->GetSFXVolume();

                    GuiAudioTriggerEvent lAudioPayload;
                    lAudioPayload.Construct(KI_AUDIO_ACTION_MENU_CUE, "", "GO_BACK", "");
                    mpStateInterface->OutputGuiEvent(lAudioPayload);

                    mpStateInterface->OutputGuiEvent(lAudioSettings);

                    GuiEventVoipSettings lVoipSettings;
                    lVoipSettings.miVoipVolume = mpGuiCache->GetMenuOptionsData()->GetVoipVolume();
                    mpStateInterface->OutputGuiEvent(lVoipSettings);

                    mSettingToggle.Unloaded();
                    mSettingToggle.Clear();

                    if (mbShownFromPause)
                    {
                        UnloadSettingsResources();
                        HideHUDAndEnterMenus();
                        ShowPauseScreen();
                    }
                    else
                    {
                        UnloadSettingsResources();
                        HideSettingsPage();
                    }
                }
                break;

            default:
                break;
        }
    }
}

// FOLDED FROM BrnOnlineGameRoomPlayerInfo_wH_09.cpp
// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-H partfile 09: the view-event page and the
// game-parameters sync that feeds it.
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   HandleControllerInputViewEventSubState @ 0x824A4F28 (BrnOnlineGameRoomPlayerInfo.cpp:2130)
//   ShowViewEventOptions                   @ 0x8249AB90 (BrnOnlineGameRoomPlayerInfo.cpp:4402)
//   HandleGameParamsChangedEvent           @ 0x824A52F8 (BrnOnlineGameRoomPlayerInfo.cpp:2670)
//
// The class shape, the member map and the rodata tables live in the wave-H keystone
// scaffold BrnOnlineGameRoomPlayerInfo.{h,cpp}; the sibling bodies land in the other
// BrnOnlineGameRoomPlayerInfo_wH_XX.cpp partfiles.
//
// X360 offsets appear only in comments -- the host layout is name-based (pointers and
// component vptrs widen on x64, so no console offset/stride is reproduced here). The
// one console byte-count that IS reproduced, the 480-byte params payload copy, is
// written as sizeof(GuiEventNetworkGameParams): that record is pointer-free and its
// home header static_asserts sizeof == 480, so the host size is the console size.
//
// HEADER-CLASH NOTE (spec's idiom section; BrnInGame.cpp precedent): the
// GuiEventShowHideSatNav payload's real home is BrnGuiDemangledEventTypes.h, which
// HARD-COLLIDES with BrnGuiEventTypeDefs.h (the class header pulls the latter in via
// BrnGuiCache.h). The two view-state records this file posts therefore use a
// file-local payload view, exactly as BrnInGame.cpp does for its demangled-only
// shapes. Same reason for the controller-input payload view.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsModule::Event, GuiEventWrapper, GuiEventQueueLarge
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface (out-queue)
#include "GameSource/GameState/BrnGameStateSharedIO.h"                    // GsmIO::EGameModeType / IsOnlineFreeBurnLobby
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiAudioTriggerEvent
#include "GameSource/Gui/Events/BrnGuiEventNetworkGameParams.h"           // BrnGui::GuiEventNetworkGameParams (the 480-byte payload)
#include "GameSource/Gui/Flow/Screen/Components/BrnGuiNetworkRouteInfo.h" // BrnGui::GuiNetworkRouteInfo

#include <cstring>   // std::memcpy (the X360 params-mirror block copy)

namespace BrnGui
{
    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) ------------------------



        // ---- controller actions this sub-state answers (event-6 payload word +4) ----
        const s32 KI_ACTION_ROUND_PREVIOUS = 0x2B;   // '+'
        const s32 KI_ACTION_ROUND_NEXT     = 0x2C;   // ','



        // The GuiAudioTriggerEvent action the back press fires (X360 `li r4, 7`).
        const s32 KI_AUDIO_ACTION_BACK = 7;

        // The record OutputGuiEvent<GuiAudioTriggerEvent> builds: { 100, 457, 12, the
        // 100-byte payload } on channel 40, 112 bytes. The committed GuiAudioTriggerEvent
        // carries that 12-byte header itself but seeds it with size 0 and id 201; this
        // view restores the wire words (same view as partfiles 08 and 13). The in-tree
        // StateInterface::OutputGuiEvent posts the event on a channel equal to its id,
        // which is not the console record.
        typedef CgsGui::GuiEventOut<GuiAudioTriggerEvent> GuiAudioTriggerWire;
        CGS_STATIC_ASSERT((sizeof(GuiAudioTriggerWire) == 112), "audio trigger record is 112 bytes");
        const s32 KI_CHANNEL_AUDIO_GUI_OUT = 40;

        // ---- in-queue payload view (the queue delivers the HEADER-STRIPPED payload,
        //      so the typed event pointer the DWARF names would mislead by 12 --
        //      BrnInGame.cpp idiom) ---------------------------------------------------
        // CgsGui::GuiEventControllerInputPressed (GuiEvent<6>) minus its 12-byte queue
        // header; field names from that type's home, CgsGuiEventTypeDefs.h. The X360
        // reads only the second word (the EGameInputActions action id).
        struct GuiEventControllerInputPressedPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miButtonId;   // +0x04 (EGameInputActions)
        };

        // ---- out-queue payload view -------------------------------------------------
        // GuiEventShowHideSatNav (wire id 213, payload 12 bytes -- the
        // OutputViewState/OutputInternalState<GuiEventShowHideSatNav> instantiations
        // @0x82476DD8 / @0x82476E38 record "id 213 size 12 off12 rec 24", which is
        // exactly the record ShowViewEventOptions stack-builds). The real home
        // (BrnGuiDemangledEventTypes.h) cannot be included here -- see the banner --
        // and it models the type as an empty struct, so the three payload words are
        // named generically. FLAG: word roles not recovered; the receiving
        // CustomRendererManager keys event 213 by a sub-mode word (0 == MainMap,
        // 1 == SatNav) and a renderable flag.
        struct GuiEventShowHideSatNavPayload
        {
            s32 miSubMode;    // +0x00  X360 stores 0
            f32 mfValue;      // +0x04  X360 stores 0.0f (flt_82001CC0)
            bool mbFlag;      // +0x08  X360 stores 1
            u8  maPad[3];     // +0x09  never written by the X360; modelled zeroed


            s32 GetEventType() const { return 213; }
        };
    }

    // ------------------------------------------- HandleControllerInputViewEventSubState
    //                                             @ 0x824A4F28
    // The view-event page's input map: the two round-scroll actions step the displayed
    // round (two rounds at a time in online road rage, whose route list is paired), the
    // select action takes the HOST into the game-options screen, and the back action
    // (and a non-host select) tears the page down -- back to the pause screen when the
    // page was opened from pause, otherwise back to the lobby.
    void OnlineGameRoomPlayerInfo::HandleControllerInputViewEventSubState(const CgsModule::Event* lpEvent)
    {
        const GuiEventControllerInputPressedPayload* lpInput =
            reinterpret_cast<const GuiEventControllerInputPressedPayload*>(lpEvent);

        // The X360 falls into the hide tail from BOTH the non-host select case and the
        // back case (case '1' branches straight to case '2''s tail); modelled as the
        // shared flag below rather than a goto.
        bool lbHidePage = false;

        switch (lpInput->miButtonId)
        {
        case KI_ACTION_ROUND_PREVIOUS:
            if (miCurrentRoundDisplayed > 0)
            {
                const s32 liStep =
                    (mpGuiCache->GetOnlineGameModeOptions()->meGameMode
                         == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE) ? 2 : 1;
                miCurrentRoundDisplayed -= liStep;
                mRouteInfoDisplay.SetInfo(
                    miCurrentRoundDisplayed,
                    mpGuiCache->GetOnlineGameModeOptions());
            }
            break;

        case KI_ACTION_ROUND_NEXT:
        {
            const s32 liStep =
                (mpGuiCache->GetOnlineGameModeOptions()->meGameMode
                     == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE) ? 2 : 1;
            if (miCurrentRoundDisplayed < mpGuiCache->GetOnlineGameModeOptions()->miNumRounds - liStep)
            {
                miCurrentRoundDisplayed += liStep;
                mRouteInfoDisplay.SetInfo(
                    miCurrentRoundDisplayed,
                    mpGuiCache->GetOnlineGameModeOptions());
            }
            break;
        }

        case KI_ACTION_SELECT:
            if (mpGuiCache->mbIsOnlineHost)
            {
                UnloadViewEventResources();
                SendStateEvent("TO_GAME_OPT");
            }
            else
            {
                lbHidePage = true;   // X360: branch into the case-'2' tail
            }
            break;

        case KI_ACTION_BACK:
        {
            GuiAudioTriggerEvent lAudioEventPayload;
            lAudioEventPayload.Construct(KI_AUDIO_ACTION_BACK, "", "GO_BACK", "");
            mpStateInterface->OutputGuiEvent(lAudioEventPayload);
            lbHidePage = true;
            break;
        }

        default:
            break;
        }

        if (lbHidePage)
        {
            if (mbShownFromPause)
            {
                UnloadViewEventResources();
                HideHUDAndEnterMenus();
                ShowPauseScreen();
            }
            else
            {
                UnloadViewEventResources();
                HideViewEventPage();
            }
        }
    }

    // ------------------------------------------------ ShowViewEventOptions @ 0x8249AB90
    // Bring the view-event page up: clamp the displayed round into the current round
    // count, mark the route-info panel's apt components loaded, make it visible and
    // feed it the cached match parameters, give the host the "change options" menu row,
    // then publish the show/hide-satnav record on both the view-state and the internal
    // channels.
    void OnlineGameRoomPlayerInfo::ShowViewEventOptions()
    {
        // Two separate stores, as the X360 emits them (upper clamp, then lower clamp).
        if (miCurrentRoundDisplayed >= mpGuiCache->GetOnlineGameModeOptions()->miNumRounds - 1)
            miCurrentRoundDisplayed = mpGuiCache->GetOnlineGameModeOptions()->miNumRounds - 1;
        if (miCurrentRoundDisplayed <= 0)
            miCurrentRoundDisplayed = 0;

        // The X360 loads the count from .data (dword_8204C7A0 == 11) and stores it into
        // the ROUTE-INFO panel's loaded-component counter (abs +84304 == the panel at
        // +75696 plus its +8608 member), not into a screen member.
        mRouteInfoDisplay.miNumComponentsLoaded = GuiNetworkRouteInfo::KI_NUM_COMPONENTS_TO_LOAD;

        mRouteInfoDisplay.SetState(GuiNetworkRouteInfo::E_STATE_VISIBLE);
        mRouteInfoDisplay.SetInfo(
            miCurrentRoundDisplayed,
            mpGuiCache->GetOnlineGameModeOptions());

        if (mpGuiCache->mbIsOnlineHost)
        {
            mViewEventOptions.SetupMenu(1, true);
            mViewEventOptions.SetText(0, KAC_VIEW_EVENT_MENU_ITEM_STRING_ID);
        }

        // The X360 stack-builds the OutputViewState/OutputInternalState wrapper record
        // inline: { payload size 12, type 213, payload offset 12, payload } posted at
        // 24 bytes on channel 41 and then again on channel 42.
        CgsGui::GuiStackEventQueue::GuiEventQueueLarge* lpOutQueue =
            mpStateInterface->GetOutputEventQueue();

        GuiEventShowHideSatNavPayload lShowHideSatNav;
        lShowHideSatNav.miSubMode = 0;
        lShowHideSatNav.mfValue   = 0.0f;
        lShowHideSatNav.mbFlag    = true;

        mpStateInterface->OutputViewState(lShowHideSatNav);

        mpStateInterface->OutputInternalState(lShowHideSatNav);
    }

    // ------------------------------------------ HandleGameParamsChangedEvent @ 0x824A52F8
    // The host re-published the match parameters: refresh the cache's params mirror,
    // and if the route-info panel is up, re-clamp and re-feed the displayed round. When
    // the new mode is a free-burn lobby mode while the view-event page is showing, the
    // page has nothing left to show -- tear it down the same way the back press does.
    void OnlineGameRoomPlayerInfo::HandleGameParamsChangedEvent(const CgsModule::Event* lpEvent)
    {
        // X360 assert string, verbatim (it names HandleCreateMatchEventEvent -- the
        // handler this one was copy-pasted from), BrnOnlineGameRoomPlayerInfo.cpp:2682.
        CGS_ASSERT_STREAM_AT(lpEvent != 0,
                   "Invalid event sent to OnlineGameRoomPlayerInfo::HandleCreateMatchEventEvent",
                       CGS_ARTIST_FILE, 2682);


        if (mpGuiCache != 0)
        {
            // ARTIST 0x824A53B4 copies the whole 480-byte payload into cache+0xA800.
            std::memcpy(mpGuiCache->GetOnlineGameModeOptions(), lpEvent,
                        sizeof(GuiEventNetworkGameParams));

            if (mRouteInfoDisplay.IsComponentLoaded())
            {
                const GuiEventNetworkGameParams* lpOptions = mpGuiCache->GetOnlineGameModeOptions();
                const s32 liLastRound = lpOptions->miNumRounds - 1;
                miCurrentRoundDisplayed = miCurrentRoundDisplayed < liLastRound
                    ? miCurrentRoundDisplayed : liLastRound;
                miCurrentRoundDisplayed = miCurrentRoundDisplayed > 0 ? miCurrentRoundDisplayed : 0;

                mRouteInfoDisplay.SetInfo(
                    miCurrentRoundDisplayed,
                    lpOptions);

                // X360: the ==15 / ==16 pair, i.e. the inlined IsOnlineFreeBurnLobby.
                if (BrnGameState::GameStateModuleIO::IsOnlineFreeBurnLobby(
                        static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(
                            mpGuiCache->GetOnlineGameModeOptions()->meGameMode))
                    && meSubState == E_SUBSTATE_VIEW_EVENT)
                {
                    if (mbShownFromPause)
                    {
                        UnloadViewEventResources();
                        HideHUDAndEnterMenus();
                        ShowPauseScreen();
                    }
                    else
                    {
                        UnloadViewEventResources();
                        HideViewEventPage();
                    }
                }
            }
        }
    }
}

// FOLDED FROM BrnOnlineGameRoomPlayerInfo_wH_10.cpp
// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-H partfile 10: the pause / security menus.
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode + asm,
// scratchpad/waveB/GameSource_Gui_Flow_Screen_States_BrnOnlineGameRoomPlayerInfo.cpp.asm.txt):
//   ShowPauseOptions                            @ 0x8248BFE0 (cpp:3033)
//   ShowSecurityOptions                         @ 0x8248C2F8 (cpp:3121)
//   HandleControllerInputSecurityOptionsSubState @ 0x824AD120 (cpp:1953)
//
// All three drive the one shared pause menu component (mPauseComponent): ShowPauseOptions
// builds the top-level pause option list from the host / game-mode / challenge-manager /
// ranked state, ShowSecurityOptions replaces it with the two security settings the lobby
// is NOT currently on, and the sub-state input handler navigates and commits it.
//
// The class shape, the member map and the rodata tables live in the wave-H keystone
// scaffold BrnOnlineGameRoomPlayerInfo.{h,cpp}. X360 offsets appear only in comments --
// the host layout is name-based, and no console offset / stride / sizeof is reproduced
// as a value here.
//
// COMPONENT VTABLE (dumped from the image this pass, not assumed): MenuComponent's ctor
// @0x824FFEF8 stores &off_82074068 at this+0, so the component vtable slots are
//   +0x14 slot5  SelectableGroup::Update          +0x18 slot6  MenuComponent::Clear
//   +0x20 slot8  SelectableGroup::Enable          +0x24 slot9  SelectableGroup::Disable
//   +0x28 slot10 SelectableGroup::HighlightNext(bool)
//   +0x2C slot11 SelectableGroup::HighlightPrevious(bool)
//   +0x30 slot12 SelectableGroup::HighlightIndex
//   +0x34 slot13 0x824E4DE0 == HighlightNext(false)      (MenuComponent::HighlightNext)
//   +0x38 slot14 0x824E4DE8 == HighlightPrevious(false)  (MenuComponent::HighlightPrevious)
// So the disable loops here (vcall +0x24) are SelectableGroup::Disable -- NOT the
// non-virtual MenuComponent::DisableSelectable @0x824E2E38, which is absent from the
// table -- and the input handler's +0x38 / +0x34 pair is PREVIOUS / NEXT in that order.
// The x64 build lays out its own vtable, so every site below is a by-name call.
//
// The pause option ids are stored with a full-width `std` of a small integer, so each
// id's numeric value IS the option enum and also its KPAC_PAUSE_OPTIONS_STRING_ID row
// (`ld` + `slwi 2` at the SetText sites). SelectableGroup::GetHighlightedId likewise
// returns the whole id in one register (`ld 0x10(r3)`), and the select case narrows it
// with `clrlwi r4, r11, 0` -- the LOW word, i.e. the option enum. (Hex-Rays renders that
// narrowing as `>> 32` because it models the __int64 return in a register PAIR; the asm
// is the arbiter, per the project rule.)
//
// The X360 inlines two helpers here; both are restored to the call they were written as
// (same treatment as the committed sibling partfile 02):
//   * GameStateModuleIO::IsOnlineFreeBurnLobby -- flattened to compares against 15/16;
//   * GuiCache::GetFreeburnChallengeManager -- flattened to the null assert
//     ("mpChallengeManager", BrnGuiCache.h:2390) plus the reload of the member. The
//     out-of-line copy @0x8240F168 is instruction-for-instruction that same sequence.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsModule::Event
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface::OutputGuiEvent
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"       // GuiComponent::AddOutputAptViewState
#include "GameSource/GameState/BrnGameStateSharedIO.h"                    // GsmIO::EGameModeType / IsOnlineFreeBurnLobby
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiAudioTriggerEvent
#include "GameSource/Gui/BrnGuiFreeburnChallengeManager.h"                // BrnGui::FreeburnChallengeManager

namespace BrnGui
{
    namespace
    {
        // ---- controller actions this sub-state answers (event-6 payload word +4;
        //      EGameInputActions -- 41 GUI_UP / 42 GUI_DOWN / 49 GUI_SELECT / 50 GUI_CANCEL)





        // The GuiAudioTriggerEvent action + label the back press fires
        // (X360 `li r4, 7`, aGoBack "GO_BACK", and the empty rodata string unk_820046A7
        // passed for BOTH the component and the movie name).
        // KI_AUDIO_ACTION_BACK is declared above in this translation unit.



        // The record OutputGuiEvent<GuiAudioTriggerEvent> builds: { 100, 457, 12, the
        // 100-byte payload } on channel 40, 112 bytes. The committed GuiAudioTriggerEvent
        // carries that 12-byte header itself but seeds it with size 0 and id 201; this
        // view restores the wire words (same view as partfiles 08 and 13). The in-tree
        // StateInterface::OutputGuiEvent posts the event on a channel equal to its id,
        // which is not the console record.
        typedef CgsGui::GuiEventOut<GuiAudioTriggerEvent> GuiAudioTriggerWire;
        CGS_STATIC_ASSERT((sizeof(GuiAudioTriggerWire) == 112), "audio trigger record is 112 bytes");
        // KI_CHANNEL_AUDIO_GUI_OUT is declared above in this translation unit.

        // The apt clip the pause button prompts transition through (X360 aAptTransition_1,
        // the AddOutputAptViewState first argument at 0x8248C2D4 / 0x8248C4A4).


        // ---- in-queue payload view (the queue delivers the HEADER-STRIPPED payload, so
        //      the DWARF's typed event pointer would mislead every field by 12 --
        //      BrnInGame.cpp / BrnBootProfile.cpp idiom, used by every sibling partfile).
        // CgsGui::GuiEventControllerInputPressed (GuiEvent<6>) minus its 12-byte queue
        // header; field names from that type's home, CgsGuiEventTypeDefs.h. This handler
        // reads only the second word.
        // GuiEventControllerInputPressedPayload is declared above in this translation unit.





        // BrnGui::GuiEventPerformOnlinePauseOption::EPauseOptions -- DWARF
        // BrnGuiEventTypeDefs.h:6520. The event type's only home in this tree is
        // BrnGuiDemangledEventTypes.h, which HARD-COLLIDES with BrnGuiEventTypeDefs.h
        // (pulled in through the class header) and models the event as an opaque 4-byte
        // payload, so the enum is mirrored here as a file-local view exactly the way the
        // sibling partfiles mirror the demangled-only payload shapes. Every value below
        // is an X360 literal in ShowPauseOptions / ShowSecurityOptions, and each doubles
        // as its row index into KPAC_PAUSE_OPTIONS_STRING_ID.
        enum EPauseOptions
        {
            E_PAUSE_OPTIONS_LAUNCH                = 0,
            E_PAUSE_OPTIONS_CREATE_EVENT          = 1,
            E_PAUSE_OPTIONS_CHANGE_TEAMS          = 2,
            E_PAUSE_OPTIONS_VIEW_LOBBY            = 3,
            E_PAUSE_OPTIONS_LEAVE_GAME            = 4,
            E_PAUSE_OPTIONS_VIEW_CHALLENGES       = 5,
            E_PAUSE_OPTIONS_VIEW_EVENT            = 6,
            E_PAUSE_OPTIONS_CANCEL_EVENT          = 7,
            E_PAUSE_OPTIONS_VIEW_MAP              = 8,
            E_PAUSE_OPTIONS_CHANGE_SECURITY       = 9,
            E_PAUSE_OPTIONS_SECURITY_ALLCOMERS    = 10,
            E_PAUSE_OPTIONS_SECURITY_FRIENDS_ONLY = 11,
            E_PAUSE_OPTIONS_SECURITY_INVITE_ONLY  = 12,
            E_PAUSE_OPTIONS_SETTINGS              = 13,
            E_PAUSE_OPTIONS_COUNT                 = 14,
        };

        // The lobby's security setting (GuiCache::mGameOptions.meSecurity, X360 cache +0xA9C0 --
        // BrnNetwork::EBrnGameSecurity per its home header's annotation: 0 public /
        // 1 private / 2 closed). The three values are the unsigned compare set the X360
        // switch emits (`cmplwi 1` / `cmplwi 3`) at 0x8248C324.
        const s32 KI_SECURITY_PUBLIC  = 0;
        const s32 KI_SECURITY_PRIVATE = 1;
        const s32 KI_SECURITY_CLOSED  = 2;
    }

    // ------------------------------------------------------ ShowPauseOptions @0x8248BFE0
    // Build the online pause menu. The rows are gathered in X360 order: the host's
    // launch / create-event pair (which of the two depends on whether the session is a
    // free-burn lobby), then team change (road rage only), event cancel (host, outside a
    // lobby), the freeburn-challenge page (unless a challenge is running or showing its
    // results), the security page (unranked host only), and finally the four rows every
    // pause menu carries. The unfilled rows of the fixed 11-row menu are disabled.
    void OnlineGameRoomPlayerInfo::ShowPauseOptions()
    {
        // The console reserves the id buffer for the whole pause-option bound; the ids
        // past liOptionIndex are left as SetupMenu found them, exactly as the X360 does
        // (SelectableGroup::SetIds walks the group's selectable count, not the fill).
        u64 lau64OptionIds[KI_MAX_PAUSE_OPTIONS];
        s32 liOptionIndex = 0;

        if (mpGuiCache->mbIsOnlineHost)
        {
            // Inlined by the X360 into compares against 15/16 (0x8248C024).
            if (BrnGameState::GameStateModuleIO::IsOnlineFreeBurnLobby(
                    static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(
                        mpGuiCache->GetOnlineGameModeOptions()->meGameMode)))
            {
                lau64OptionIds[0] = E_PAUSE_OPTIONS_CREATE_EVENT;
                liOptionIndex     = 1;
            }
            else
            {
                lau64OptionIds[0] = E_PAUSE_OPTIONS_LAUNCH;
                lau64OptionIds[1] = E_PAUSE_OPTIONS_VIEW_EVENT;
                liOptionIndex     = 2;
            }
        }
        else if (!BrnGameState::GameStateModuleIO::IsOnlineFreeBurnLobby(
                     static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(
                         mpGuiCache->GetOnlineGameModeOptions()->meGameMode)))
        {
            lau64OptionIds[0] = E_PAUSE_OPTIONS_VIEW_EVENT;
            liOptionIndex     = 1;
        }

        if (mpGuiCache->GetOnlineGameModeOptions()->meGameMode
                == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE)
        {
            lau64OptionIds[liOptionIndex] = E_PAUSE_OPTIONS_CHANGE_TEAMS;
            ++liOptionIndex;
        }

        if (mpGuiCache->mbIsOnlineHost
            && !BrnGameState::GameStateModuleIO::IsOnlineFreeBurnLobby(
                   static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(
                       mpGuiCache->GetOnlineGameModeOptions()->meGameMode)))
        {
            lau64OptionIds[liOptionIndex] = E_PAUSE_OPTIONS_CANCEL_EVENT;
            ++liOptionIndex;
        }

        // The accessor carries the "mpChallengeManager" assert (BrnGuiCache.h:2390) the
        // X360 inlined here ahead of the state read.
        const FreeburnChallengeManager* lpChallengeManager =
            mpGuiCache->GetFreeburnChallengeManager();
        if (!lpChallengeManager->IsRunning() && !lpChallengeManager->IsShowingResults())
        {
            lau64OptionIds[liOptionIndex] = E_PAUSE_OPTIONS_VIEW_CHALLENGES;
            ++liOptionIndex;
        }

        if (mpGuiCache->mbIsOnlineHost && !mpGuiCache->GetOnlineGameModeOptions()->mbRanked)
        {
            lau64OptionIds[liOptionIndex] = E_PAUSE_OPTIONS_CHANGE_SECURITY;
            ++liOptionIndex;
        }

        lau64OptionIds[liOptionIndex]     = E_PAUSE_OPTIONS_VIEW_LOBBY;
        lau64OptionIds[liOptionIndex + 1] = E_PAUSE_OPTIONS_VIEW_MAP;
        lau64OptionIds[liOptionIndex + 2] = E_PAUSE_OPTIONS_SETTINGS;
        lau64OptionIds[liOptionIndex + 3] = E_PAUSE_OPTIONS_LEAVE_GAME;
        liOptionIndex += 4;

        // Both asserts fire from the one overflow test in the asm (`cmpwi r28, 0xB` at
        // 0x8248C1F0), cpp:3102 then cpp:3103. Both bounds are 11 on ARTIST.
        CGS_ASSERT_AT(liOptionIndex <= KI_MAX_DISPLAYABLE_PAUSE_OPTIONS,
                   "liOptionIndex <= KI_MAX_DISPLAYABLE_PAUSE_OPTIONS", CGS_ARTIST_FILE, 3102);
        CGS_ASSERT_AT(liOptionIndex <= KI_MAX_PAUSE_OPTIONS,
                   "liOptionIndex <= KI_MAX_PAUSE_OPTIONS", CGS_ARTIST_FILE, 3103);

        mPauseComponent.SetupMenu(KI_MAX_DISPLAYABLE_PAUSE_OPTIONS, true);

        s32 liRow = 0;
        for (; liRow < liOptionIndex; ++liRow)
        {
            mPauseComponent.SetText(
                liRow,
                KPAC_PAUSE_OPTIONS_STRING_ID[static_cast<s32>(lau64OptionIds[liRow])]);
        }
        for (; liRow < KI_MAX_DISPLAYABLE_PAUSE_OPTIONS; ++liRow)
        {
            mPauseComponent.Disable(liRow);   // component vtable slot 9
        }

        mPauseComponent.SetIds(lau64OptionIds);
        mPauseButtonPromptsAnimation.AddOutputAptViewState(
            "apt_Transition", KAPC_PAUSE_BUTTON_PROMPT_ANIMATION_STATES[0], false);
    }

    // --------------------------------------------------- ShowSecurityOptions @0x8248C2F8
    // Replace the pause menu with the security page: the two settings the lobby is not
    // currently on, then the sub-state flip and the button-prompt transition.
    void OnlineGameRoomPlayerInfo::ShowSecurityOptions()
    {
        u64 lau64OptionIds[KI_MAX_PAUSE_OPTIONS];
        s32 liOptionIndex = 0;

        switch (mpGuiCache->GetOnlineGameModeOptions()->meSecurity)
        {
        case KI_SECURITY_PUBLIC:
            lau64OptionIds[0] = E_PAUSE_OPTIONS_SECURITY_FRIENDS_ONLY;
            lau64OptionIds[1] = E_PAUSE_OPTIONS_SECURITY_INVITE_ONLY;
            liOptionIndex     = 2;
            break;

        case KI_SECURITY_PRIVATE:
            lau64OptionIds[0] = E_PAUSE_OPTIONS_SECURITY_ALLCOMERS;
            lau64OptionIds[1] = E_PAUSE_OPTIONS_SECURITY_INVITE_ONLY;
            liOptionIndex     = 2;
            break;

        case KI_SECURITY_CLOSED:
            lau64OptionIds[0] = E_PAUSE_OPTIONS_SECURITY_ALLCOMERS;
            lau64OptionIds[1] = E_PAUSE_OPTIONS_SECURITY_FRIENDS_ONLY;
            liOptionIndex     = 2;
            break;

        default:
            // cpp:3167. The X360 streams "Unknown security value of " << meSecurity
            // << "\n" into CgsDev::Assert::gpcMessageBuffer before FireAssert; modelled
            // through CGS_ASSERT per this tree's assert-machinery convention (the live
            // value is not re-streamed). No ids are written on this path.
            CGS_FAIL("Unknown security value");
            break;
        }

        mPauseComponent.SetupMenu(KI_MAX_DISPLAYABLE_PAUSE_OPTIONS, true);

        s32 liRow = 0;
        for (; liRow < liOptionIndex; ++liRow)
        {
            mPauseComponent.SetText(
                liRow,
                KPAC_PAUSE_OPTIONS_STRING_ID[static_cast<s32>(lau64OptionIds[liRow])]);
        }
        for (; liRow < KI_MAX_DISPLAYABLE_PAUSE_OPTIONS; ++liRow)
        {
            mPauseComponent.Disable(liRow);   // component vtable slot 9
        }

        mPauseComponent.SetIds(lau64OptionIds);
        meSubState = E_SUBSTATE_SECURITY_OPTIONS;
        mPauseButtonPromptsAnimation.AddOutputAptViewState(
            "apt_Transition", KAPC_PAUSE_BUTTON_PROMPT_ANIMATION_STATES[1], false);
    }

    // ------------------------- HandleControllerInputSecurityOptionsSubState @0x824AD120
    // The security page's input map: move the highlight, commit the highlighted setting
    // (its id's low word is the pause option) and go back to the pause screen, or back
    // out with the go-back sound. Every other action is ignored.
    void OnlineGameRoomPlayerInfo::HandleControllerInputSecurityOptionsSubState(
        const CgsModule::Event* lpEvent)
    {
        const GuiEventControllerInputPressedPayload* lpInput =
            reinterpret_cast<const GuiEventControllerInputPressedPayload*>(lpEvent);

        switch (lpInput->miButtonId)
        {
        case KI_ACTION_MENU_PREVIOUS:
            mPauseComponent.HighlightPrevious();   // component vtable slot 14
            break;

        case KI_ACTION_MENU_NEXT:
            mPauseComponent.HighlightNext();       // component vtable slot 13
            break;

        case KI_ACTION_SELECT:
            PerformPauseOption(static_cast<s32>(mPauseComponent.GetHighlightedId()));
            ShowPauseScreen();
            break;

        case KI_ACTION_BACK:
        {
            GuiAudioTriggerEvent lAudioEventPayload;
            lAudioEventPayload.Construct(KI_AUDIO_ACTION_BACK, "",
                                  "GO_BACK", "");
            mpStateInterface->OutputGuiEvent(lAudioEventPayload);
            ShowPauseScreen();
            break;
        }

        default:
            break;
        }
    }
}

// FOLDED FROM BrnOnlineGameRoomPlayerInfo_wH_11.cpp
// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-H partfile 11: the pause-menu actions.
//   PerformPauseOption      @0x824A5F70  (DWARF cpp:3956; asserts cpp:3987 / cpp:4112)
//   HandleLeaveGameRequest  @0x82498C98  (DWARF cpp:2844; asserts cpp:2869 / cpp:2870)
//   HandleLaunchQuestion    @0x82498038  (DWARF cpp:1587)
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode + asm,
// scratchpad/waveB/GameSource_Gui_Flow_Screen_States_BrnOnlineGameRoomPlayerInfo.cpp.asm.txt).
//
// PerformPauseOption is the pause menu's action dispatcher: a 14-arm switch over
// GuiEventPerformOnlinePauseOption::EPauseOptions. Six arms just bring a sub-page up
// and record that it was entered from the pause menu; four re-publish the cached
// network game-params payload with one field poked (the free-burn-lobby cancel and the
// three security choices); two post a bare 16-byte command record; one hands the flow a
// state event; one leaves the game.
//
// Notes taken from the asm rather than Hex-Rays:
//  * case 0's free-burn-lobby test is the compiler-inlined
//    GameStateModuleIO::IsOnlineFreeBurnLobby predicate (the asm compares the mode word
//    against 15 and 16 in line at 0x824A5FF4/0x824A5FFC, no call) -- restored to the
//    call the assert text names; the assert FIRES when the predicate holds, and the
//    X360 falls through into the launch command either way (non-fatal assert).
//  * case 9 (CHANGE_SECURITY) does NOT set mbShownFromPause -- exactly five arms store
//    it (3/5/6/8/13; the stbx at +0x155F0 == +87536), and 9 is not one of them.
//  * case 2's guard is `== 11` (E_MODE_ONLINE_ROAD_RAGE), the one online team mode.
//  * cases 7/10/11/12 memcpy the full 480-byte params mirror off the GuiCache, poke ONE
//    field, and hand the copy to StateInterface::OutputGuiEvent<GuiEventNetworkGameParams>
//    (r3 = this+28 == mpStateInterface, an out-of-line call at 0x824A61E4) -- unlike the
//    two command records, which go through the queue at mpStateInterface+12.
//  * HandleLeaveGameRequest's four asserts come in the order
//    mpGuiCache / mpChallengeManager / GetFreeburnChallengeManager() / mpChallengeManager:
//    the 1st and 3rd are this file's own (cpp:2869/2870) and the 2nd and 4th are the
//    inlined body of GuiCache::GetFreeburnChallengeManager (BrnGuiCache.h:2390), one per
//    call. Written as the two source asserts plus the two accessor calls that produce them.
//  * HandleLaunchQuestion's branch polarity is straight off the asm (0x82498060 /
//    0x8249809C): the PLAYER_INFO title is the meState == 0 (visible) side of the
//    player-stats test, and EVENT_MAP is the meState != 0 (invisible) side of the
//    route-info test.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include <cstring>                                                       // memcpy

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N> / GuiEventWrapper<T,N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue / OutputGuiEvent
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache (friend access)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiOverlayRequest / GuiOverlayCompleteEvent
#include "GameSource/Gui/BrnGuiFreeburnChallengeManager.h"                // FreeburnChallengeManager::IsRunning
#include "GameSource/Gui/Events/BrnGuiEventNetworkGameParams.h"           // BrnGui::GuiEventNetworkGameParams
#include "GameSource/GameState/BrnGameStateSharedIO.h"                    // GameStateModuleIO::IsOnlineFreeBurnLobby
namespace BrnGui
{
    namespace
    {
        // ---- AddEvent channel (the out-queue selector word) ---------------------------


        // ---- pause-menu action ids ----------------------------------------------------
        // BrnGui::GuiEventPerformOnlinePauseOption::EPauseOptions (DWARF
        // BrnGuiEventTypeDefs.h:5521). The event type itself is not declared in the tree's
        // BrnGuiEventTypeDefs.h yet and headers are frozen for this partfile, so the arms
        // are named here as constants rather than forked as a local enum.
        const s32 KI_PAUSE_OPTION_LAUNCH                = 0;
        const s32 KI_PAUSE_OPTION_CREATE_EVENT          = 1;
        const s32 KI_PAUSE_OPTION_CHANGE_TEAMS          = 2;
        const s32 KI_PAUSE_OPTION_VIEW_LOBBY            = 3;
        const s32 KI_PAUSE_OPTION_LEAVE_GAME            = 4;
        const s32 KI_PAUSE_OPTION_VIEW_CHALLENGES       = 5;
        const s32 KI_PAUSE_OPTION_VIEW_EVENT            = 6;
        const s32 KI_PAUSE_OPTION_CANCEL_EVENT          = 7;
        const s32 KI_PAUSE_OPTION_VIEW_MAP              = 8;
        const s32 KI_PAUSE_OPTION_CHANGE_SECURITY       = 9;
        const s32 KI_PAUSE_OPTION_SECURITY_ALLCOMERS    = 10;
        const s32 KI_PAUSE_OPTION_SECURITY_FRIENDS_ONLY = 11;
        const s32 KI_PAUSE_OPTION_SECURITY_INVITE_ONLY  = 12;
        const s32 KI_PAUSE_OPTION_SETTINGS              = 13;

        // ---- game-security values the three security arms write -----------------------
        // The params payload's meSecurity field (BrnNetwork::EBrnGameSecurity; the enum has
        // no home header in the tree yet, so the three X360 immediates are named here after
        // the pause options that write them -- 0 all-comers / 1 friends-only / 2 invite-only,
        // the same ordering BrnGuiCache.h records for the cached mirror field).
        const s32 KI_GAME_SECURITY_ALLCOMERS    = 0;
        const s32 KI_GAME_SECURITY_FRIENDS_ONLY = 1;
        const s32 KI_GAME_SECURITY_INVITE_ONLY  = 2;

        // ---- the leave-question overlay ids (X360 rodata literals) --------------------

        // ---- out-queue wire records ---------------------------------------------------
        // 16-byte command record { 1, N, 12 } -- the payload-less GuiEvent<N> command the
        // GUI states share (BrnInGame.cpp / BrnCarSelectMain_wG_01.cpp carry the same
        // template). The X360 stores only the three header words and hands AddEvent a size
        // of 16: the payload size word is 1 because the payload type is an EMPTY struct,
        // and the X360 never writes the byte. It is modelled zeroed here.
        template <s32 N>
        struct GuiCommandEvent16 : public CgsGui::GuiEvent<N>
        {
            u8 mu8Flag;    // +0x0C (the empty payload's byte; X360 leaves it untouched)

            GuiCommandEvent16() : mu8Flag(0)
            {
            }
        };

        template <s32 N>
        void PostCommand16(CgsGui::StateInterface* lpStateInterface)
        {
            GuiCommandEvent16<N> lEvent;
            lpStateInterface->OutputGuiEvent(lEvent);   // X360 record {1, N, 12}, size 16
        }

        // The two payload-less command ids this file posts. FLAG: neither id carries a
        // declared event struct in the recovered DWARF slice (both payloads are empty), so
        // the names are role-derived from the call sites -- the ids themselves are
        // X360-attested immediates (li r11, 0x36 / li r11, 0x38 / li r11, 0x109).
        const s32 KI_EVENT_LAUNCH_ONLINE_GAME     = 54;    // pause menu LAUNCH
        const s32 KI_EVENT_LAUNCH_QUESTION_ACCEPT = 56;    // launch question answered OK
        const s32 KI_EVENT_CHANGE_TEAMS           = 265;   // pause menu CHANGE TEAMS

        // ---- the network-game-params output path --------------------------------------
        // FLAG: stand-in for the OUT-OF-LINE instantiation
        // CgsGui::StateInterface::OutputGuiEvent<BrnGui::GuiEventNetworkGameParams>
        // @0x82436AD0 -- each params arm below is a plain `bl` to it (r3 = this+28 ==
        // mpStateInterface, r4 = the stack copy). It cannot be CALLED from here: the tree's
        // shared template body in CgsGuiStateInterface.h hands the payload straight to
        // VariableEventQueue::AddEvent(const CgsModule::Event*, s32, s32), and
        // GuiEventNetworkGameParams is not CgsModule::Event-derived, so the instantiation
        // does not compile. Repairing that template is a HEADER change, out of scope for
        // this partfile -- so the callee's body is reproduced here, read store-for-store
        // off its OWN asm (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82436AD0.json):
        //   memcpy(record+12, payload, 0x1E0); record = { 0x1E0, 0x101, 0xC };
        //   AddEvent(this+12, record, 40, 0x1EC)
        // That record IS the canonical CgsGui::GuiEventWrapper<T, 40> (CgsGuiEvent.h:71):
        // its three header words are sizeof(T) / T::GetEventType() / offsetof(mOutEvent),
        // so the console's 0x1E0 / 0x101 / 0xC immediates come out as host expressions and
        // no local record type is forked.
        typedef CgsGui::GuiEventOut<GuiEventNetworkGameParams> GuiEventNetworkGameParamsWire;

        void OutputNetworkGameParams(CgsGui::StateInterface* lpStateInterface,
                                     GuiEventNetworkGameParams& lrParams)
        {
            lpStateInterface->OutputGuiEvent(lrParams);   // X360 record size 492
        }

        // The OutputGuiEvent<BrnGui::GuiOverlayRequest> wire record:
        // { 288, 184, 16, <pad>, the 288-byte request }, channel 40, 304 bytes -- the same
        // record BrnInGame.cpp's PostOverlayRequest builds. The two header words are HOST
        // expressions (sizeof / the 16-aligned payload offset), not the console immediates.
        // GuiOverlayRequestWire is declared above in this translation unit.







    }

    // ---- PerformPauseOption @0x824A5F70 -------------------------------------------
    // DWARF cpp:3956. Carry out the pause-menu action the player picked.
    void OnlineGameRoomPlayerInfo::PerformPauseOption(s32 lePauseOption)
    {
        switch (lePauseOption)
        {
        case KI_PAUSE_OPTION_LAUNCH:
            // cpp:3987. Inlined by the X360 into the 15/16 compare pair at 0x824A5FF4.
            CGS_ASSERT_AT(!BrnGameState::GameStateModuleIO::IsOnlineFreeBurnLobby(
                           static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(
                               mpGuiCache->GetOnlineGameModeOptions()->meGameMode)),
                       "!GsmIO::IsOnlineFreeBurnLobby(mpGuiCache->GetOnlineGameModeOptions()->meGameMode)", CGS_ARTIST_FILE, 3987);

            PostCommand16<KI_EVENT_LAUNCH_ONLINE_GAME>(mpStateInterface);
            HideMenusAndReturnToHUD();
            break;

        case KI_PAUSE_OPTION_CREATE_EVENT:
            SendStateEvent("TO_GAME_OPT");
            break;

        case KI_PAUSE_OPTION_CHANGE_TEAMS:
            if (mpGuiCache->GetOnlineGameModeOptions()->meGameMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE)
            {
                PostCommand16<KI_EVENT_CHANGE_TEAMS>(mpStateInterface);
            }
            break;

        case KI_PAUSE_OPTION_VIEW_LOBBY:
            ShowLobby();
            mbShownFromPause = true;
            break;

        case KI_PAUSE_OPTION_LEAVE_GAME:
            HandleLeaveGameRequest();
            HideMenusAndReturnToHUD();
            break;

        case KI_PAUSE_OPTION_VIEW_CHALLENGES:
            ShowChallengesScreen();
            mbShownFromPause = true;
            break;

        case KI_PAUSE_OPTION_VIEW_EVENT:
            ShowViewEventScreen();
            mbShownFromPause = true;
            break;

        case KI_PAUSE_OPTION_CANCEL_EVENT:
            {
                // Re-publish the cached params with the mode dropped back to the free-burn
                // lobby -- i.e. cancel the pending event and return the room to the lobby.
                GuiEventNetworkGameParams lParams;
                memcpy(&lParams,
                       mpGuiCache->GetOnlineGameModeOptions(),
                       sizeof(lParams));   // X360 size 0x1E0 == the whole cache mirror

                lParams.meGameMode = BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY;
                OutputNetworkGameParams(mpStateInterface, lParams);

                HideMenusAndReturnToHUD();
            }
            break;

        case KI_PAUSE_OPTION_VIEW_MAP:
            ShowMapScreen();
            mbShownFromPause = true;
            break;

        case KI_PAUSE_OPTION_CHANGE_SECURITY:
            ShowSecurityOptions();
            break;

        case KI_PAUSE_OPTION_SECURITY_ALLCOMERS:
            {
                GuiEventNetworkGameParams lParams;
                memcpy(&lParams,
                       mpGuiCache->GetOnlineGameModeOptions(),
                       sizeof(lParams));

                lParams.meSecurity = KI_GAME_SECURITY_ALLCOMERS;
                OutputNetworkGameParams(mpStateInterface, lParams);
            }
            break;

        case KI_PAUSE_OPTION_SECURITY_FRIENDS_ONLY:
            {
                GuiEventNetworkGameParams lParams;
                memcpy(&lParams,
                       mpGuiCache->GetOnlineGameModeOptions(),
                       sizeof(lParams));

                lParams.meSecurity = KI_GAME_SECURITY_FRIENDS_ONLY;
                OutputNetworkGameParams(mpStateInterface, lParams);
            }
            break;

        case KI_PAUSE_OPTION_SECURITY_INVITE_ONLY:
            {
                GuiEventNetworkGameParams lParams;
                memcpy(&lParams,
                       mpGuiCache->GetOnlineGameModeOptions(),
                       sizeof(lParams));

                lParams.meSecurity = KI_GAME_SECURITY_INVITE_ONLY;
                OutputNetworkGameParams(mpStateInterface, lParams);
            }
            break;

        case KI_PAUSE_OPTION_SETTINGS:
            ShowSettingsScreen();
            mbShownFromPause = true;
            break;

        default:
            CGS_FAIL_AT("Invalid game action to perform\n", CGS_ARTIST_FILE, 4112);   // cpp:4112
            break;
        }
    }

    // ---- HandleLeaveGameRequest @0x82498C98 ---------------------------------------
    // DWARF cpp:2844. Raise the "are you sure you want to leave?" overlay, picking the
    // wording from whether the local player is hosting and, if not, whether a freeburn
    // challenge is running.
    void OnlineGameRoomPlayerInfo::HandleLeaveGameRequest()
    {
        const char* lpacOverlayId;

        if (mpGuiCache->mbIsOnlineHost)
        {
            lpacOverlayId = "OnHostLvQn";
        }
        else
        {
            CGS_ASSERT_AT(mpGuiCache != 0, "mpGuiCache", CGS_ARTIST_FILE, 2869);                                   // cpp:2869
            CGS_ASSERT_AT(mpGuiCache->GetFreeburnChallengeManager() != 0,
                       "mpGuiCache->GetFreeburnChallengeManager()", CGS_ARTIST_FILE, 2870);                     // cpp:2870

            // The accessor's own "mpChallengeManager" assert (BrnGuiCache.h:2390) is what
            // the X360 shows inlined before each of these two uses.
            if (mpGuiCache->GetFreeburnChallengeManager()->IsRunning())
            {
                lpacOverlayId = "CNOnlLvChaQn";
            }
            else
            {
                lpacOverlayId = "CNOnlLvGmQn";
            }
        }

        // The X360 constructs the request into a scratch stack slot and memcpy's the 288
        // bytes into the wire record (the inlined OutputGuiEvent<GuiOverlayRequest> copy);
        // constructed in place here, the same way BrnInGame.cpp posts its overlays.
        GuiOverlayRequestWire lWire;
        lWire.mRequest.Construct(lpacOverlayId);

        mpStateInterface->OutputGuiEvent(lWire.mRequest);   // X360 record size 304

    }

    // ---- HandleLaunchQuestion @0x82498038 -----------------------------------------
    // DWARF cpp:1587. The answer to the "launch the event?" overlay: OK posts the accept
    // command, CANCEL restores the tab title the question overwrote.
    void OnlineGameRoomPlayerInfo::HandleLaunchQuestion(GuiOverlayCompleteEvent::LeaveMethod leLeaveMethod)
    {
        switch (leLeaveMethod)
        {
        case GuiOverlayCompleteEvent::E_LEAVEMETHOD_CANCEL:
            if (mPlayerStatsDisplay.IsVisible())
            {
                mMessageText.SetText(KAC_PLAYER_INFO_STRING_ID);
            }
            else if (mRouteInfoDisplay.meState == GuiNetworkRouteInfo::E_STATE_VISIBLE)
            {
                mMessageText.SetText(KAC_EVENT_INFO_STRING_ID);
            }
            else
            {
                mMessageText.SetText(KAC_EVENT_MAP_STRING_ID);
            }
            break;

        case GuiOverlayCompleteEvent::E_LEAVEMETHOD_OK:
        {
            CgsGui::GuiEvent<KI_EVENT_LAUNCH_QUESTION_ACCEPT> lAccept;
            mpStateInterface->OutputGuiEvent(lAccept);
            break;
        }
        }
    }
}

// FOLDED FROM BrnOnlineGameRoomPlayerInfo_wH_12.cpp
// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-H partfile 12: the splash-screen pair.
//   ClearSplashScreenOverlay    @0x82498E00  (DWARF cpp:2974)
//   HandleSplashScreenRequests  @0x824A54B8  (DWARF cpp:2758)
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode + asm,
// scratchpad/waveB/GameSource_Gui_Flow_Screen_States_BrnOnlineGameRoomPlayerInfo.cpp.asm.txt).
// The class shape, the member map and the statics live in the wave-H keystone scaffold
// BrnOnlineGameRoomPlayerInfo.{h,cpp}.
//
// Both functions talk to the network splash overlay -- the "entering online" full-screen
// image the network module raises while a game mode spins up. HandleSplashScreenRequests
// is the SHOW/STOP request sink; ClearSplashScreenOverlay is the acknowledge path the
// overlay-complete handshake and the cache-arrival latch both drive.
//
// The X360 reaches the out-queue through the template instantiations
//   CgsGui::StateInterface::OutputGuiEvent<BrnGui::GuiEventNetworkSplashEvent>  @0x82493880
//   CgsGui::StateInterface::OutputGuiEvent<BrnGui::GuiOverlayWaitFinishRequest> @0x82476E98
// whose bodies were read out of the export: the first stack-builds { 4, 269, 12, state }
// and AddEvent()s it on channel 40 with size 16; the second stack-builds
// { 8, 188, 16, <hole>, CgsID } and AddEvent()s it on channel 40 with size 24. That
// { payload size, payload type, payload offset, payload } shape is exactly the canonical
// CgsGui::GuiEventWrapper<T, 40> (CgsGuiEvent.h:71), so both records are built with it --
// the header words fall out as sizeof(T) / T::GetEventType() / offsetof(mOutEvent) and the
// console's 4 / 8 / 12 / 16 / 24 literals (32-bit sizes/offsets) are never reproduced.
//
// The shared splash event is the four-byte payload in both queue directions.
//
// HandleGuiCacheEvent @0x824A3DD8 was assigned to this group too, but was blocked on a
// missing declaration at the time; it is now bodied in BrnOnlineGameRoomPlayerInfo_wH_18.cpp.
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsID / CgsIDCompress
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEventWrapper<T,N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/GameState/BrnGameStateSharedIO.h"                    // GsmIO::EGameModeType / IsOnlineFreeBurnLobby
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache (meGameModeType; we are a friend)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiOverlayWaitFinishRequest
#include "GameSource/Gui/Flow/Screen/States/Shared/BrnScreenShared.h"     // GetSplashScreenIDForGameMode

namespace BrnGui
{
    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) --------------------------


        // The upper bound both "leGameModeType < GsmIO::E_MODE_COUNT" asserts compare
        // against. ARTIST E_MODE_COUNT == 18: the X360 bakes `cmpwi 0x12` into BOTH
        // asserts, and the splash-id table it guards has 18 live entries. The committed
        // BrnGameStateSharedIO.h enum is the Dec-2007 DecFIGS one and still says 17, so
        // the bound is carried here as the attested literal rather than the stale enum.
        const s32 KI_ARTIST_MODE_COUNT = 18;

        // ---- out-queue wire records ---------------------------------------------------
        // OutputGuiEvent<BrnGui::GuiEventNetworkSplashEvent> @0x82493880: sizeof 4 /
        // type 269 / offset 12, record size 16.
        typedef CgsGui::GuiEventOut<GuiEventNetworkSplashEvent> GuiEventNetworkSplashWire;

        // OutputGuiEvent<BrnGui::GuiOverlayWaitFinishRequest> @0x82476E98: sizeof 8 /
        // type 188 / offset 16 (the 8-aligned CgsID leaves a 4-byte hole after the header
        // that the console never stores -- neither the callee nor the caller writes it --
        // so nothing is modelled there either).
        typedef CgsGui::GuiEventOut<GuiOverlayWaitFinishRequest> SplashScreenWaitFinishWire;
    }

    // ------------------------------------------------ ClearSplashScreenOverlay @0x82498E00
    // The overlay-complete acknowledge path. An overlay just finished; if it was OUR game
    // mode's splash screen and it left by the plain (method 0) route, tell the network
    // module the splash is FINISHED. Either way the "still owe a splash clear" latch drops
    // -- but only when a game mode is actually set: with meGameModeType == E_MODE_NONE the
    // X360 returns without touching anything (the whole body sits under that test).
    //
    // Called by HandleOverlayComplete (with the stored mOverlayCompleteEvent) and by
    // HandleGuiCacheEvent when the cache arrives with the latch already set.
    void OnlineGameRoomPlayerInfo::ClearSplashScreenOverlay(
        const OverlayCompleteData* lpOverlayCompleteEvent)
    {
        CGS_ASSERT_AT(mpGuiCache != 0, "mpGuiCache", KAC_BRN_ONLINE_GAME_ROOM_PLAYER_INFO_ASSERT_FILE, 2990);

        const s32 leGameModeType = mpGuiCache->meGameModeType;
        if (leGameModeType != BrnGameState::GameStateModuleIO::E_MODE_NONE)
        {
            // ARTIST E_MODE_COUNT (the X360 compares against 18) -- see KI_ARTIST_MODE_COUNT.
            CGS_ASSERT_AT(leGameModeType < KI_ARTIST_MODE_COUNT,
                "leGameModeType < GsmIO::E_MODE_COUNT", KAC_BRN_ONLINE_GAME_ROOM_PLAYER_INFO_ASSERT_FILE, 2997);

            const char* lpcSplashScreenID = GetSplashScreenIDForGameMode(
                static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(leGameModeType));
            CGS_ASSERT_AT(lpcSplashScreenID != 0, "lpcSplashScreenID != NULL", KAC_BRN_ONLINE_GAME_ROOM_PLAYER_INFO_ASSERT_FILE, 3002);

            if (lpOverlayCompleteEvent->mOverlayId == CgsIDCompress(lpcSplashScreenID) &&
                lpOverlayCompleteEvent->miLeaveMethod == 0)
            {
                GuiEventNetworkSplashEvent lFinished;
                lFinished.meSplashState = GuiEventNetworkSplashEvent::E_SPLASH_STATE_FINISHED;

                mpStateInterface->OutputGuiEvent(lFinished);   // X360 record size 16
            }

            mbNeedToClearSplashScreen = false;
        }
    }

    // ------------------------------------------------ HandleSplashScreenRequests @0x824A54B8
    // The network module's splash SHOW / STOP requests (event 269).
    //
    // SHOW: while the screen is sitting in a free-burn lobby -- or before any cache/game
    //   mode exists at all -- there is nothing to raise yet, so the request is just latched
    //   into mbShowSplashScreen (the sub-state ladder raises it later) and logged. In any
    //   other mode the request is handed straight to ShowSplashScreen.
    // STOP: if the request never got as far as an overlay (mbShowSplashScreen still set)
    //   the latch drops and the module is told FINISHED immediately; otherwise the overlay
    //   really is up, so the screen posts the wait-to-finish handshake (event 188) for this
    //   game mode's splash id and waits for the overlay-complete round trip.
    // Any other state word is ignored (the X360 `cmplwi 1` / `blt` / `bne` pair).
    //
    // DWARF param type is const GuiEventNetworkSplashEvent*; the in-queue delivers the
    // header-stripped shared payload.
    void OnlineGameRoomPlayerInfo::HandleSplashScreenRequests(const CgsModule::Event* lpNetworkSplashEvent)
    {
        CGS_ASSERT_AT(lpNetworkSplashEvent != 0, "lpNetworkSplashEvent", CGS_ARTIST_FILE, 2770);   // cpp:2770

        const u32 luSplashState =
            reinterpret_cast<const GuiEventNetworkSplashEvent*>(lpNetworkSplashEvent)->meSplashState;

        if (luSplashState == GuiEventNetworkSplashEvent::E_SPLASH_STATE_SHOW)
        {
            // The free-burn-lobby test is the compiler-inlined
            // GameStateModuleIO::IsOnlineFreeBurnLobby predicate (the asm compares the
            // mode word against 15 and 16 in line, no call), restored to the call it was
            // written as -- same de-inlining the wave-H load ladder makes.
            if ((mpGuiCache != 0 &&
                 BrnGameState::GameStateModuleIO::IsOnlineFreeBurnLobby(
                     static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(
                         mpGuiCache->meGameModeType))) ||
                mpGuiCache == 0 ||
                mpGuiCache->meGameModeType == BrnGameState::GameStateModuleIO::E_MODE_NONE)
            {
                mbShowSplashScreen = true;

                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                {
                    *CgsDev::Log::gpDebugPrint << "mbShowSplashScreen = true;\n";
                }
            }
            else
            {
                // Why this call takes no argument (the header now matches):
                // The X360 hands ShowSplashScreen NOTHING but `this`: 0x824A5650 emits only
                // `mr r3, r29` before the bl, and the event pointer (r31, loaded by
                // `mr r31, r4` @0x824A54C4) is never moved back into r4. It could not have
                // been left in r4 either -- the assert path at 0x824A54DC..0x824A54F4
                // clobbers r4 and falls through to this same call. ShowSplashScreen's own
                // body @0x82499F00 confirms it: it reads the game mode from mpGuiCache and
                // touches no second argument at all.
                //
                // The argument is passed here ONLY because the frozen keystone header
                // (the header declaration has since been corrected to take none).
                ShowSplashScreen();
            }
        }
        else if (luSplashState == GuiEventNetworkSplashEvent::E_SPLASH_STATE_STOP)
        {
            if (mbShowSplashScreen)
            {
                mbShowSplashScreen = false;

                GuiEventNetworkSplashEvent lFinished;
                lFinished.meSplashState = GuiEventNetworkSplashEvent::E_SPLASH_STATE_FINISHED;

                mpStateInterface->OutputGuiEvent(lFinished);   // X360 record size 16
            }
            else
            {
                CGS_ASSERT_AT(mpGuiCache != 0, "mpGuiCache", CGS_ARTIST_FILE, 2812);   // cpp:2812

                const s32 leGameModeType = mpGuiCache->meGameModeType;
                CGS_ASSERT_AT(leGameModeType > BrnGameState::GameStateModuleIO::E_MODE_NONE,
                           "leGameModeType > GsmIO::E_MODE_NONE", CGS_ARTIST_FILE, 2814);    // cpp:2814
                // ARTIST E_MODE_COUNT (the X360 compares against 18) -- see KI_ARTIST_MODE_COUNT.
                CGS_ASSERT_AT(leGameModeType < KI_ARTIST_MODE_COUNT,
                           "leGameModeType < GsmIO::E_MODE_COUNT", CGS_ARTIST_FILE, 2815);   // cpp:2815

                // The X360 constructs the request into its own stack slot and the
                // instantiation copies the 8 bytes into the record.
                GuiOverlayWaitFinishRequest lWaitFinish;
                lWaitFinish.Construct(GetSplashScreenIDForGameMode(
                    static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(leGameModeType)));

                mpStateInterface->OutputGuiEvent(lWaitFinish);   // X360 record size 24
            }
        }
    }

    // ---------------------------------------------------------------------------------
    // HandleGuiCacheEvent @0x824A3DD8 is bodied in BrnOnlineGameRoomPlayerInfo_wH_18.cpp.
}

// FOLDED FROM BrnOnlineGameRoomPlayerInfo_wH_13.cpp
// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-H partfile 13: the overlay plumbing.
//   b5-decomp/src/GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo_wH_13.cpp
//
//   HandleAskToLaunchEvent @0x82498698  cpp:2527 (assert cpp:2542)
//   HandleInviteFailed     @0x82498BA0  cpp:2721 (assert cpp:2735)
//   HandleOverlayComplete  @0x824A56A0  cpp:2884 (assert cpp:2899)
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode arbitrated by
// the raw asm: scratchpad/waveB/...BrnOnlineGameRoomPlayerInfo.cpp.asm.txt). The class
// shape, the member map and the rodata tables live in the wave-H keystone scaffold
// BrnOnlineGameRoomPlayerInfo.{h,cpp}; the sibling bodies land in the other
// BrnOnlineGameRoomPlayerInfo_wH_XX.cpp partfiles.
//
// These three are the screen's overlay conversation: it asks a question (the launch
// confirmation), it reports a failed invite, and it answers whatever question the
// overlay director says the player just closed.
//
// X360 offsets appear only in comments; every access below resolves by NAME (the host
// layout widens pointers and component vptrs, so no console offset or stride is
// reproduced in code). The members these bodies touch, with their console offsets:
//   this+28    -> mpStateInterface (CgsGui::State)      this+87464 -> mOverlayCompleteEvent
//   this+55584 -> mSettingToggle                        this+87516 -> mpGuiCache
//   this+87534 -> mbNeedToClearSplashScreen             this+87536 -> mbShownFromPause
// and on the cache side cache+0xB878 -> the embedded options profile
// (GuiCache::GetOptionsDataProfile, inlined by the X360 as `cache + 0xB878`).
//
// PAYLOAD VIEWS. The in-queue hands a state the HEADER-STRIPPED payload, so the DWARF's
// typed event-pointer signatures would mislead every field offset by 12; these bodies
// take `const CgsModule::Event*` and cast to a payload view, the same idiom the
// committed BrnInGame.cpp and the sibling wave-H partfiles use. The overlay-complete
// payload already has a home -- OnlineGameRoomPlayerInfo::OverlayCompleteData in the
// class header (it is also the type of the stored mOverlayCompleteEvent member and the
// parameter type of ClearSplashScreenOverlay) -- so it is reused rather than re-forked.
//
// OUT-QUEUE RECORDS. Where the X360 calls CgsGui::StateInterface::OutputGuiEvent<T> the
// callee stack-builds a { payload size, event id, payload offset, payload } record and
// posts it on channel 40; the two instantiations these bodies reach were dumped from the
// image to pin the record exactly:
//   OutputGuiEvent<BrnGui::GuiOverlayRequest>    @0x82436BE0 -> { 288, 184, 16, request },
//                                                   channel 40, 304 bytes
//   OutputGuiEvent<BrnGui::GuiAudioTriggerEvent> @0x82436890 -> { 100, 457, 12, payload },
//                                                   channel 40, 112 bytes
// The committed CgsGuiStateInterface.h OutputGuiEvent template posts the bare payload
// under channel == GetEventType() instead, which is NOT that record, so each site below
// builds the attested record and posts it through GetOutputEventQueue()->AddEvent() --
// the accommodation BrnCarSelectMain_wG_02.cpp and BrnOnlineGameRoomPlayerInfo_wH_08.cpp
// already make. Every size word is a HOST sizeof expression, never the console's baked
// 0x120 / 0x130 / 0x64 / 0x70 immediates.
//
// HEADER CLASH. BrnGuiEventTypeDefs.h (pulled in by the class header via BrnGuiCache.h,
// and needed here for GuiOverlayRequest / GuiAudioTriggerEvent) hard-collides with
// BrnGuiDemangledEventTypes.h, so the ids that are homed only in the demangled header
// (457, 463, 474, 52) are carried as file-local wire views -- as in BrnInGame.cpp and
// the sibling partfiles 07/08/09.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsID / CgsIDCompress
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // GuiCache::GetOptionsDataProfile
#include "GameSource/Gui/BrnGuiOptionsDataProfile.h"                      // OptionsDataProfile volume getters
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiOverlayRequest / GuiAudioTriggerEvent
namespace BrnGui
{
    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) ------------------------


        // ---- overlay ids these bodies name (X360 rodata literals) --------------------

        const char KAC_OVERLAY_LEAVE_CHALLENGE_QN[] = "CNOnlLvChaQn"; // "leave the challenge?"
        const char KAC_OVERLAY_HOST_LEAVE_QN[]    = "OnHostLvQn";    // "leave, as the host?"
        const char KAC_OVERLAY_LEAVING_GAME[]     = "CNOnlLvgGame";  // the "leaving..." splash
        const char KAC_OVERLAY_INVITE_SAME_GAME[] = "CNInvSmGame";   // invite target already here
        const char KAC_OVERLAY_INVITE_SAME_MACHINE[] = "CNInvSmMach";// invite target on this box
        const char KAC_OVERLAY_INVITE_HOST[]      = "CNInvHost";     // invite refused by the host

        // The overlay-complete leave method this screen acts on. FLAG: the producer-side
        // enum (GuiOverlayCompleteEvent::LeaveMethod, named by the DWARF but without
        // recovered enumerators) is not in the recovered slice; the value below is the
        // X360's own `cmpwi 1` compare, named for the role it plays at both sites.
        const s32 KI_LEAVE_METHOD_CONFIRMED = 1;

        // The audio label + action the settings cancel plays on its way out.



        // The invite-failure reasons. FLAG: the producer-side enum name is not in the
        // recovered DWARF slice; the roles come from the overlay each reason raises.
        const s32 KI_INVITE_FAILED_SAME_MACHINE = 0;
        const s32 KI_INVITE_FAILED_SAME_GAME    = 1;
        const s32 KI_INVITE_FAILED_HOST         = 2;

        // ---- in-queue payload view ---------------------------------------------------
        // The invite-failed payload: one word, the reason (`lwz r11, 0(r30)`).
        struct GuiEventInviteFailedPayload : public CgsModule::Event
        {
            s32 miReason;   // +0x00
        };

        // ---- out-queue wire records ---------------------------------------------------

        // The record OutputGuiEvent<BrnGui::GuiOverlayRequest> @0x82436BE0 builds:
        // { 288, 184, 16, <pad>, the 288-byte request }, channel 40, 304 bytes. The
        // payload is 8-aligned on the console, so its offset word is 16 rather than 12.
        // GuiOverlayRequestWire is declared above in this translation unit.








        // The record OutputGuiEvent<BrnGui::GuiAudioTriggerEvent> @0x82436890 builds:
        // { 100, 457, 12, the 100-byte payload }, channel 40, 112 bytes. The committed PC
        // GuiAudioTriggerEvent already carries that 12-byte queue header in front of its
        // 100-byte payload, so the event object IS the record -- but its GuiEvent<201>
        // base seeds the size word 0 and the id 201, while the X360 wire words are the
        // payload size and id 457. This view restores them without forking the payload
        // type. (Cross-TU finding: 201 is GuiEventHideDriveThru's id; 457 is the id both
        // this instantiation and BrnGuiDemangledEventTypes.h attest for the audio
        // trigger. Same view as BrnOnlineGameRoomPlayerInfo_wH_08.cpp's.)
        // KU_WIRE_ID_AUDIO_TRIGGER is declared above in this translation unit.

        typedef CgsGui::GuiEventOut<GuiAudioTriggerEvent> GuiAudioTriggerWire;

        // Id 463 == BrnGui::GuiEventAudioSettings: { 8, 463, 12, the two volume words },
        // channel 40, 20 bytes. Field order is the X360 store order -- GetMusicVolume
        // lands in the first payload word, GetSFXVolume in the second.
        typedef CgsGui::GuiEvent<463> AudioSettingsHeader;
        // GuiEventAudioSettingsWire is declared above in this translation unit.












        // Id 474 == BrnGui::GuiEventVoipSettings: { 4, 474, 12, the voip volume word },
        // channel 40, 16 bytes.
        typedef CgsGui::GuiEvent<474> VoipSettingsHeader;
        // GuiEventVoipSettingsWire is declared above in this translation unit.










        // Id 52 -- the "leave the online game" request: { 1, 52, 12, <one payload byte> },
        // channel 40, 16 bytes. FLAG: the producer-side type name is not in the recovered
        // DWARF slice and the id is absent from BrnGuiDemangledEventTypes.h; the role is
        // pinned by the consumer, BrnNetwork::StateManager::ProcessGuiEvents @0x82577F20
        // case 52, which starts the leave-game matchmaking process and answers with
        // CgsGui::GuiEvent<53>. The payload is a single byte that the X360 producer never
        // writes and the consumer never reads (the X360 emits only the three header
        // stores), so it is left unwritten here too.
        typedef CgsGui::GuiEvent<52> LeaveOnlineGameHeader;
        struct GuiEventLeaveOnlineGameWire : public LeaveOnlineGameHeader
        {
            u8 maPayload[1];   // +0x0C -- never written by the X360 (see above)

            GuiEventLeaveOnlineGameWire()
            {
            }
        };

        // Record-size pins. Every one of these payloads is pointer-free, so the X360's
        // AddEvent size immediate must survive unchanged on the x64 host; if a host layout
        // ever drifts, this is where it is caught rather than on the wire.
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiOverlayRequest>) == 304), "overlay request record is 304 bytes");
        CGS_STATIC_ASSERT((sizeof(GuiAudioTriggerWire) == 112), "audio trigger record is 112 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiEventAudioSettingsWire>) == 20), "audio settings record is 20 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiEventVoipSettingsWire>) == 16), "voip settings record is 16 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiEventLeaveOnlineGameWire>) == 16), "leave-game record is 16 bytes");
    }

    // ---- HandleAskToLaunchEvent @ 0x82498698 --------------------------------------
    // The host asked everyone to confirm the launch: put the launch question up.
    void OnlineGameRoomPlayerInfo::HandleAskToLaunchEvent(const CgsModule::Event* lpAskToLaunchEvent)
    {
        // cpp:2542. The X360 streams this text into the assert buffer before firing.
        CGS_ASSERT(lpAskToLaunchEvent != 0,
                   "Invalid event sent to OnlineGameRoomPlayerInfo::HandleAskToLaunchEvent");

        // The X360 constructs the request in a scratch stack slot and memcpy's it into the
        // record (the inlined OutputGuiEvent<GuiOverlayRequest> copy); constructed in
        // place here.
        GuiOverlayRequestWire lWire;
        lWire.mRequest.Construct("CNLobbyLchQn");

        mpStateInterface->OutputGuiEvent(lWire.mRequest);
    }

    // ---- HandleInviteFailed @ 0x82498BA0 -------------------------------------------
    // An outgoing invite bounced: raise the overlay that explains why. The three reasons
    // the screen answers each name their own overlay; any other reason is ignored.
    void OnlineGameRoomPlayerInfo::HandleInviteFailed(const CgsModule::Event* lpInviteFailedEvent)
    {
        CGS_ASSERT_AT(lpInviteFailedEvent != 0, "lpInviteFailedEvent", CGS_ARTIST_FILE, 2735);   // cpp:2735

        const s32 liReason =
            reinterpret_cast<const GuiEventInviteFailedPayload*>(lpInviteFailedEvent)->miReason;

        // The X360 splits this into two shapes: the same-game arm has the
        // OutputGuiEvent<GuiOverlayRequest> body inlined (construct + memcpy + AddEvent),
        // the other two call the out-of-line instantiation. Both produce the identical
        // record, so both are posted the same way here.
        const char* lpcOverlayId = 0;

        if (liReason == KI_INVITE_FAILED_SAME_GAME)
        {
            lpcOverlayId = "CNInvSmGame";
        }
        else if (liReason == KI_INVITE_FAILED_SAME_MACHINE)
        {
            lpcOverlayId = "CNInvSmMach";
        }
        else if (liReason == KI_INVITE_FAILED_HOST)
        {
            lpcOverlayId = "CNInvHost";
        }
        else
        {
            return;
        }

        GuiOverlayRequestWire lWire;
        lWire.mRequest.Construct(lpcOverlayId);

        mpStateInterface->OutputGuiEvent(lWire.mRequest);
    }

    // ---- HandleOverlayComplete @ 0x824A56A0 ----------------------------------------
    // The overlay director closed an overlay: act on whichever question it was.
    //   "CNCnclCnfrm"  -- the settings edit was discarded: restore the profile's volumes,
    //                     tear the toggle group down and go back where we came from.
    //   "CNLobbyLchQn" -- the launch question: forwarded to HandleLaunchQuestion.
    //   the three leave questions -- confirmed: raise the "leaving" splash and ask the
    //                     network layer to leave the game.
    //   anything else  -- a splash screen closing: clear it now if the cache is up,
    //                     otherwise remember it and clear it on the next Update.
    void OnlineGameRoomPlayerInfo::HandleOverlayComplete(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT_AT(lpEvent != 0, "lpOverlayCompleteEvent", CGS_ARTIST_FILE, 2899);   // cpp:2899

        // The stored member and ClearSplashScreenOverlay's parameter are both this shape
        // (id qword at +0, leave method at +8) -- see the class header.
        const OverlayCompleteData* lpOverlayCompleteEvent =
            reinterpret_cast<const OverlayCompleteData*>(lpEvent);

        if (lpOverlayCompleteEvent->mOverlayId == CgsIDCompress("CNCnclCnfrm"))
        {
            if (lpOverlayCompleteEvent->miLeaveMethod == KI_LEAVE_METHOD_CONFIRMED)
            {
                // The X360 reads both volumes into one stack qword, plays the back-out
                // cue, publishes the audio settings, then reads the voip volume into the
                // next record and publishes that.
                const s32 liMusicVolume = mpGuiCache->GetMenuOptionsData()->GetMusicVolume();
                const s32 liSFXVolume   = mpGuiCache->GetMenuOptionsData()->GetSFXVolume();

                GuiAudioTriggerEvent lAudioPayload;
                lAudioPayload.Construct(KI_AUDIO_ACTION_MENU_CUE, "", "GO_BACK", "");
                mpStateInterface->OutputGuiEvent(lAudioPayload);

                GuiEventAudioSettingsWire lAudioSettings(liMusicVolume, liSFXVolume);
                mpStateInterface->OutputGuiEvent(lAudioSettings);

                GuiEventVoipSettingsWire lVoipSettings(
                    mpGuiCache->GetMenuOptionsData()->GetVoipVolume());
                mpStateInterface->OutputGuiEvent(lVoipSettings);

                mSettingToggle.Unloaded();
                mSettingToggle.Clear();   // group vtable slot 6

                if (mbShownFromPause)
                {
                    UnloadSettingsResources();
                    HideHUDAndEnterMenus();
                    ShowPauseScreen();
                }
                else
                {
                    UnloadSettingsResources();
                    HideSettingsPage();
                }
            }
        }
        else if (lpOverlayCompleteEvent->mOverlayId == CgsIDCompress("CNLobbyLchQn"))
        {
            HandleLaunchQuestion(static_cast<GuiOverlayCompleteEvent::LeaveMethod>(lpOverlayCompleteEvent->miLeaveMethod));
        }
        else if (lpOverlayCompleteEvent->mOverlayId == CgsIDCompress("CNOnlLvGmQn") ||
                 lpOverlayCompleteEvent->mOverlayId == CgsIDCompress("CNOnlLvChaQn") ||
                 lpOverlayCompleteEvent->mOverlayId == CgsIDCompress("OnHostLvQn"))
        {
            if (lpOverlayCompleteEvent->miLeaveMethod == KI_LEAVE_METHOD_CONFIRMED)
            {
                GuiOverlayRequestWire lWire;
                lWire.mRequest.Construct("CNOnlLvgGame");
                mpStateInterface->OutputGuiEvent(lWire.mRequest);

                GuiEventLeaveOnlineGameWire lLeaveGame;
                mpStateInterface->OutputGuiEvent(lLeaveGame);

            }
        }
        else if (mpGuiCache != 0)
        {
            ClearSplashScreenOverlay(lpOverlayCompleteEvent);
        }
        else
        {
            // No cache yet -- stash the whole 16-byte record and let Update clear it.
            mOverlayCompleteEvent = *lpOverlayCompleteEvent;
            mbNeedToClearSplashScreen = true;
        }
    }
}

// FOLDED FROM BrnOnlineGameRoomPlayerInfo_wH_14.cpp
// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-H partfile 14: the three network-lifecycle
// in-queue handlers that report a game the local player is being removed from, a
// failed launch, and a refreshed stats record for the highlighted player.
//   b5-decomp/src/GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo_wH_14.cpp
//
//   HandleLeftGameEvent     @0x82498358  (cpp:2392)
//   HandleLaunchedEvent     @0x82498A10  (cpp:2599)
//   HandlePlayerStatsEvent  @0x82484BA0  (cpp:2636)
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode arbitrated by
// the raw asm, scratchpad/waveB/...BrnOnlineGameRoomPlayerInfo.cpp.asm.txt). The first
// two have VERBATIM copy-paste twins in the car-select screen -- their X360 assert
// strings still name OnlineGameRoomPlayerInfo:: -- already reconstructed as
// CarSelectMain::HandleLeftGameEvent / ::HandleLaunchedEvent in
// BrnCarSelectMain_wG_02.cpp; the wire records and the assert treatment below follow
// that committed precedent, adapted to THIS screen's members (the kicked-popup and
// launch-failed tables are class statics here, our left-game path additionally sets
// mbKicked and sends "GO_BACK" rather than "DISCONNECT").
//
// The owning header (BrnOnlineGameRoomPlayerInfo.h) carries the X360 member offsets
// every access below resolves through BY NAME:
//   this+28    -> mpStateInterface (CgsGui::State)   this+87516 -> mpGuiCache
//   this+34400 -> mMessageText (the tab title)       this+87504 -> meSubState
//   this+38984 -> mPlayerNameComponent               this+87535 -> mbKicked
//   this+39149 -> mPlayerNameComponent.miHighlightedIndex (SelectableGroup +0xA5, s8)
//   this+75684 -> mPlayerStatsDisplay.meState (GuiNetworkPlayerStats +0x11E4)
//   this+84312 -> mRouteInfoDisplay.meState   (GuiNetworkRouteInfo   +0x21A8)
// and on the cache side:
//   cache+0xB650 -> mLobbyPlayerInfoList.maPlayerLobbyInfo[i].mPlayerID (row stride 56 == the committed
//                   pointer-free sizeof(LobbyPlayerStatusData), so the console stride
//                   IS the host stride; the row is still reached by NAMED array index)
//   cache+0xA9DF -> mGameOptions.mbRanked
//
// OUT-QUEUE RECORDS. The X360 reaches the overlay wires two ways -- by inlining
// CgsGui::StateInterface::OutputGuiEvent<T> (the construct-then-memcpy pairs at
// 0x82498AE0 / 0x82498584) and by calling the out-of-line instantiation
// (HandleLeftGameEvent's lobby-deleted overlay at 0x82498444) -- but both emit the
// identical { sizeof(payload), <event id>, 16, <pad>, payload } record on channel 40.
// The committed CgsGui::StateInterface::OutputGuiEvent template queues the bare
// payload under channel == GetEventType() instead, which is NOT what the X360
// instantiation does, so every site below posts the wire record directly (identical to
// BrnCarSelectMain_wG_02.cpp / BrnOnlineGameRoomPlayerInfo_wH_08.cpp).
//
// X360 LITERALS. The console's baked AddEvent sizes (0x130 / 0x18) and payload-size
// words (0x120 / 8) are written as host sizeof() expressions; every payload involved is
// pointer-free, so the static_asserts below pin the host records back onto the console
// sizes rather than letting a widened layout drift silently onto the wire.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache / GuiFlow
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiOverlayRequest
#include "GameSource/Network/Managers/BrnNetworkPlayerStats.h"            // BrnNetwork::NetworkPlayerStats
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h"      // InGamePlayerStatusData
#include "GameSource/Network/SharedIO/BrnNetworkModuleOnlineLobbyPlayerStatusInterface.h" // LobbyPlayerStatusData
// The canonical homes for the three in-queue payload shapes below are
// GameSource/Gui/BrnGuiDemangledEventTypes.h, but that header and BrnGuiEventTypeDefs.h
// are MUTUALLY EXCLUSIVE by construction (both define GuiAudioTriggerEvent and three
// more types with different layouts) and the owning header pulls BrnGuiCache.h ->
// BrnGuiEventTypeDefs.h in. So the payloads are modelled here as file-local views --
// the same idiom the committed BrnInGame.cpp / BrnCarSelectMain_wG_02.cpp use, and the
// queue hands out HEADER-STRIPPED payloads, so the views start at the first field.

namespace BrnGui
{
    namespace
    {


        // ---- in-queue payload views ------------------------------------------------

        // Event 273 ("left game"): the reason word, then -- for the kicked reason -- the
        // kick sub-reason that indexes KAPC_KICKED_POPUP. The X360 bounds-checks the kick
        // reason SIGNED (blt on 0 then blt on 5); Hex-Rays renders it as `> 4u`.
        struct GuiEventLeftGamePayload : public CgsModule::Event
        {
            s32 miReason;        // +0x00
            s32 miKickReason;    // +0x04
        };

        // Event 58 ("launched"): one word read both as the failure test and as the
        // KPC_LAUNCH_FAILED_STRINGIDS index.
        struct GuiEventLaunchedPayload : public CgsModule::Event
        {
            s32 miResult;        // +0x00  (0 == launched OK)
        };


        // ---- out-queue wire records ------------------------------------------------

        // The OutputGuiEvent<BrnGui::GuiOverlayRequest> record: { 288, 184, 16, <pad>,
        // the 288-byte request }, channel 40, 304 bytes.
        // GuiOverlayRequestWire is declared above in this translation unit.








        // The OutputGuiEvent<BrnGui::GuiOverlayWaitFinishRequest> record:
        // { 8, 188, 16, <pad>, the compressed overlay id }, channel 40, 24 bytes.
        // GuiOverlayWaitFinishWire is declared above in this translation unit.








        // Record-size pins: both payloads are pointer-free, so the X360's AddEvent size
        // immediates must survive unchanged on the x64 host.
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiOverlayRequest>) == 304), "overlay request record is 304 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiOverlayWaitFinishRequest>) == 24), "overlay wait-finish record is 24 bytes");

        // ---- overlay / state-event vocabularies (the asm's rodata literals) --------

        const char KAC_OVERLAY_LEAVE_CHAL_QN[]   = "CNOnlLvChaQn";   // "leave the challenge?" question
        const char KAC_OVERLAY_LAUNCH_FAILED[]   = "CNOnlLchFail";
        const char KAC_STATE_EVENT_GO_BACK[]     = "GO_BACK";

        // The reason words this handler acts on. FLAG: the producer-side enum name is not
        // in the recovered DWARF slice; the roles come from the overlay ids they raise.
        const s32 KI_LEFT_GAME_REASON_KICKED         = 2;
        const s32 KI_LEFT_GAME_REASON_LOBBY_DELETED  = 3;

        // The X360's own kick-reason bound (the `>= 5` half of the signed assert range);
        // it is exactly the element count of OnlineGameRoomPlayerInfo::KAPC_KICKED_POPUP.
        const s32 KI_KICK_REASON_COUNT = 5;

        // The launch-failed message parameter slot AddMessageParam is called with.
        const CgsGui::PopupParamTypes KU_LAUNCH_FAILED_MESSAGE_PARAM = CgsGui::E_POPUPPARAMTYPES_STRING_ID;
    }

    // ---- HandleLeftGameEvent @ 0x82498358 (cpp:2392) --------------------------------
    // In-queue event 273. Raise the reason-specific overlay(s), hand the flow the
    // "GO_BACK" state event, and drop the expected-apt-component list if the screen is
    // still part-way through one of its loads.
    void OnlineGameRoomPlayerInfo::HandleLeftGameEvent(const CgsModule::Event* lpLeftGameEvent)
    {
        // The X360 fires BOTH asserts back-to-back on the null path: the bare expression
        // text (cpp:2408) then the streamed message (cpp:2409).
        CGS_ASSERT_AT(lpLeftGameEvent != 0, "lpLeftGameEvent", CGS_ARTIST_FILE, 2408);                                  // cpp:2408
        CGS_ASSERT(lpLeftGameEvent != 0,
                   "Invalid event sent to OnlineGameRoomPlayerInfo::HandleLeftGameEvent");    // cpp:2409

        const GuiEventLeftGamePayload* lpPayload =
            reinterpret_cast<const GuiEventLeftGamePayload*>(lpLeftGameEvent);

        // Two independent tests in the asm (the reason word is re-read at 0x82498448),
        // not an if/else chain.
        if (lpPayload->miReason == KI_LEFT_GAME_REASON_LOBBY_DELETED && !mbKicked)
        {
            GuiOverlayRequestWire lWire;
            lWire.mRequest.Construct("CNLobbyDlted");

            mpStateInterface->OutputGuiEvent(lWire.mRequest);
        }

        if (lpPayload->miReason == KI_LEFT_GAME_REASON_KICKED)
        {
            // Tear down both outstanding "are you sure you want to leave" questions
            // before the kick overlay goes up.
            {
                GuiOverlayWaitFinishWire lWaitGame;
                lWaitGame.mRequest.Construct("CNOnlLvGmQn");

                mpStateInterface->OutputGuiEvent(lWaitGame.mRequest);
            }
            {
                GuiOverlayWaitFinishWire lWaitChallenge;
                lWaitChallenge.mRequest.Construct("CNOnlLvChaQn");

                mpStateInterface->OutputGuiEvent(lWaitChallenge.mRequest);
            }

            // cpp:2433 -- SIGNED bound check (asm: blt on 0, then blt on 5). The X360
            // streams "Invalid kick reason of " << miKickReason << "\n" into the assert
            // buffer; collapsed to the static text per project policy.
            CGS_ASSERT(lpPayload->miKickReason >= 0 && lpPayload->miKickReason < KI_KICK_REASON_COUNT,
                       "Invalid kick reason of ");

            GuiOverlayRequestWire lWire;
            lWire.mRequest.Construct(KAPC_KICKED_POPUP[lpPayload->miKickReason]);

            mpStateInterface->OutputGuiEvent(lWire.mRequest);

            mbKicked = true;
        }

        SendStateEvent("GO_BACK");

        if (mpGuiCache != 0 &&
            (meSubState == E_SUBSTATE_LOADING_SCREEN ||
             meSubState == E_SUBSTATE_LOADING_COMPONENTS ||
             meSubState == E_SUBSTATE_LOADING_PAUSE_SCREEN ||
             meSubState == E_SUBSTATE_LOADING_PAUSE_COMPONENTS))
        {
            mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
        }
    }

    // ---- HandleLaunchedEvent @ 0x82498A10 (cpp:2599) --------------------------------
    // In-queue event 58. On a non-zero launch result, raise the "CNOnlLchFail" overlay
    // carrying the result's ONLINE_LAUNCHED_FAILED_* string id as message param 2, then
    // put the tab title back to whichever page the screen is actually showing.
    void OnlineGameRoomPlayerInfo::HandleLaunchedEvent(const CgsModule::Event* lpLaunchedEvent)
    {
        // cpp:2611 -- the X360 streams this text into the assert buffer.
        CGS_ASSERT(lpLaunchedEvent != 0,
                   "Invalid event sent to OnlineGameRoomPlayerInfo::HandleLaunchedEvent");

        const GuiEventLaunchedPayload* lpPayload =
            reinterpret_cast<const GuiEventLaunchedPayload*>(lpLaunchedEvent);

        if (lpPayload->miResult != 0)
        {
            // The X360 builds the request in a scratch stack slot and memcpy's the 288
            // bytes into the record (the inlined OutputGuiEvent<GuiOverlayRequest> copy);
            // constructed in place here.
            GuiOverlayRequestWire lWire;
            lWire.mRequest.Construct("CNOnlLchFail");
            lWire.mRequest.AddMessageParam(KU_LAUNCH_FAILED_MESSAGE_PARAM,
                                           KPC_LAUNCH_FAILED_STRINGIDS[lpPayload->miResult]);

            mpStateInterface->OutputGuiEvent(lWire.mRequest);

            // Tab title. The X360 tests the two panels' visibility state words directly
            // (mPlayerStatsDisplay.meState @+0x11E4 and mRouteInfoDisplay.meState
            // @+0x21A8, both compared against E_STATE_VISIBLE == 0); the stats panel's
            // state member is private, so its test goes through the DWARF-attested
            // IsVisible() accessor the compiler inlined here.
            if (mPlayerStatsDisplay.IsVisible())
            {
                mMessageText.SetText(KAC_PLAYER_INFO_STRING_ID);
            }
            else if (mRouteInfoDisplay.meState == GuiNetworkRouteInfo::E_STATE_VISIBLE)
            {
                mMessageText.SetText(KAC_EVENT_INFO_STRING_ID);
            }
            else
            {
                mMessageText.SetText(KAC_EVENT_MAP_STRING_ID);
            }
        }
    }

    // ---- HandlePlayerStatsEvent @ 0x82484BA0 (cpp:2636) -----------------------------
    // In-queue event 320. A refreshed stats record arrived; push it into the stats panel
    // only while it still belongs to the player the list is highlighting.
    void OnlineGameRoomPlayerInfo::HandlePlayerStatsEvent(const CgsModule::Event* lpPlayerStatsEvent)
    {
        CGS_ASSERT_STREAM_AT(lpPlayerStatsEvent != 0,
                   "Invalid event sent to OnlineGameRoomPlayerInfo::HandlePlayerStatsEvent",
                       CGS_ARTIST_FILE, 2648);


        const GuiEventNetworkPlayerStats* lpPayload =
            reinterpret_cast<const GuiEventNetworkPlayerStats*>(lpPlayerStatsEvent);

        GuiCache* lpGuiCache = mpGuiCache;
        if (lpGuiCache != 0 &&
            lpPayload->mPlayerID ==
                lpGuiCache->GetLobbyPlayerList()->maPlayerLobbyInfo[mPlayerNameComponent.GetHighlightedIndex()].mPlayerID)
        {
            const s32 liHighlightedIndex = mPlayerNameComponent.GetHighlightedIndex();

            const GuiEventNetworkGameParams* lpOptions = lpGuiCache->GetOnlineGameModeOptions();
            const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerInfo =
                lpGuiCache->GetOnlinePlayerInfoByIndex(liHighlightedIndex);

            // Name from the in-game record (+256), the event's value and its stats block,
            // the highlighted row as the player image index, and the world-rank column
            // gated on the cached game params' "ranked" flag.
            mPlayerStatsDisplay.SetInfo(lpPlayerInfo->mPlayerName.macName,
                                        lpPayload->miWorldRank,
                                        lpPayload,
                                        liHighlightedIndex,
                                        lpOptions->mbRanked,
                                        mpGuiCache);
        }
    }
}

// FOLDED FROM BrnOnlineGameRoomPlayerInfo_wH_16.cpp
// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-H partfile 16: the three remaining page
// teardowns (challenge list, pause menu, settings page).
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   UnloadChallengeResources @ 0x8249A170 (assert fires at cpp:4161)
//   UnloadPauseResources     @ 0x8249A348 (assert fires at cpp:4203)
//   UnloadSettingsResources  @ 0x8249A410 (assert fires at cpp:4224)
//
// All three share one shape: unmount the page's apt movie, assert the cache pointer,
// then hand that page's resource tuples back to the cache. UnloadChallengeResources
// additionally posts the ticker-clear record FIRST, before the movie comes down.
//
// The class shape, the member map and the rodata tuple tables live in the wave-H
// keystone scaffold BrnOnlineGameRoomPlayerInfo.{h,cpp}; the sibling bodies land in the
// other BrnOnlineGameRoomPlayerInfo_wH_XX.cpp partfiles.
//
// X360 offsets/sizes appear only in comments -- the host layout is name-based. In
// particular no console record size is written as a literal here:
//   * the {8,18,12,"",3} play-apt-movie record (X360 size 20, channel 41) is not built
//     by hand at all -- the faithful de-inlined form is the real method
//     CgsGui::StateInterface::PlayAptMovie(name, level), which owns that sizeof (the
//     record carries a pointer, so its host size differs). Same de-inlining the
//     committed siblings wH_15 / BrnCarSelectMain_wG_01 use.
//   * the ticker-clear record's X360 size 16 is emitted as sizeof(the wire view).
//
// HEADER-CLASH NOTE (spec's idiom section; BrnInGame.cpp / BrnCarSelectMain_wG_01
// precedent): the id-536 payload's typed home is BrnGuiDemangledEventTypes.h
// (GuiEventTickerClearMessages, 2 raw bytes), which HARD-COLLIDES with
// BrnGuiEventTypeDefs.h -- and the latter is pulled in transitively by the class
// header via BrnGuiCache.h. The record therefore gets a file-local wire view.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N> / CgsModule::Event
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface::PlayAptMovie / GetOutputEventQueue
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache::UnloadResources

namespace BrnGui
{
    namespace
    {
        // The empty movie name PlayAptMovie is handed when a screen's apt movie is
        // unmounted (X360 unk_820046A7, the zero-length rodata string).


        // The apt movie level the screens mount/unmount on (X360 `li r11, 3`).
        // KI_APT_MOVIE_LEVEL is declared above in this translation unit.

        // The out-queue channel the ticker-clear record goes out on (X360 `li r5, 0x28`;
        // 40 == GuiEventOut).


        // ---- out-queue wire record: the id-536 ticker-clear command -------------------
        // X360 @0x8249A184..0x8249A1C8 stack-builds { 2, 536, 12 } then stores a zeroed
        // halfword at +0x0C and calls AddEvent(&record, 40, 16). The two payload bytes are
        // written individually into a 2-byte temp and copied out as one halfword -- i.e.
        // the GuiEventWrapper<T,40> copy of a 2-byte payload -- so they are modelled as the
        // two bytes they are, not as a 16-bit word. The trailing two bytes of the record
        // are never written by the X360 (modelled zeroed). Typed home:
        // BrnGuiDemangledEventTypes.h's GuiEventTickerClearMessages (id 536, size 2); see
        // the header-clash note above for why it is re-declared file-locally.
        // The identical record (with payload bytes { 1, 0 }) appears in the committed
        // BrnCarSelectMain_wG_01.cpp as GuiEventCarSelectAccept16.
        struct GuiEventTickerClearMessages16 : public CgsGui::GuiEvent<536>
        {
            u8 mu8Byte0;   // +0x0C (X360 stores 0)
            u8 mu8Byte1;   // +0x0D (X360 stores 0)

            GuiEventTickerClearMessages16(u8 lu8Byte0, u8 lu8Byte1)
                : mu8Byte0(lu8Byte0), mu8Byte1(lu8Byte1)
            {
            }
            GuiEventTickerClearMessages16() {}
        };
    }

    // ---------------------------------------------- UnloadChallengeResources @ 0x8249A170
    // Tear the freeburn-challenge page down. Unlike its two siblings this one first pushes
    // the ticker-clear command out on the GUI-out channel (the challenge page owns the
    // ticker messages it posted), then unmounts the apt movie via the inlined
    // CgsGui::GuiEventPlayAptMovie record { 8, 18, 12, "", 3 } on the view-state channel,
    // then hands the challenge resource tuples back to the cache.
    void OnlineGameRoomPlayerInfo::UnloadChallengeResources()
    {
        GuiEventTickerClearMessages16 lClearTicker(0, 0);
        mpStateInterface->OutputGuiEvent(lClearTicker);   // X360 record size 16

        mpStateInterface->PlayAptMovie("", KI_APT_MOVIE_LEVEL);

        // X360 BrnOnlineGameRoomPlayerInfo.cpp:4161.
        CGS_ASSERT_AT(mpGuiCache != 0, "mpGuiCache", CGS_ARTIST_FILE, 4161);

        // Re-read (not the asserted copy): the X360 reloads the member and re-tests it.
        if (mpGuiCache != 0)
        {
            mpGuiCache->UnloadResources(maChallengeResourceTuplesToLoad,
                                        static_cast<u32>(miNumChallengeResourcesToLoad));
        }
    }

    // ------------------------------------------------- UnloadPauseResources @ 0x8249A348
    // Tear the pause menu down: unmount its apt movie, then hand the pause resource tuples
    // back to the cache.
    void OnlineGameRoomPlayerInfo::UnloadPauseResources()
    {
        CgsGui::GuiEventPlayAptMovie lMovie;
        lMovie.mpacMovieName = "";
        lMovie.miLevelNum = KI_APT_MOVIE_LEVEL;
        mpStateInterface->OutputViewState(lMovie);

        // X360 BrnOnlineGameRoomPlayerInfo.cpp:4203.
        CGS_ASSERT_AT(mpGuiCache != 0, "mpGuiCache", KAC_BRN_ONLINE_GAME_ROOM_PLAYER_INFO_ASSERT_FILE, 4203);

        if (mpGuiCache != 0)
        {
            mpGuiCache->UnloadResources(maPauseResourceTuplesToLoad,
                                        static_cast<u32>(miNumPauseResourcesToLoad));
        }
    }

    // ---------------------------------------------- UnloadSettingsResources @ 0x8249A410
    // Tear the settings page down: unmount its apt movie, then hand the settings resource
    // tuples back to the cache.
    void OnlineGameRoomPlayerInfo::UnloadSettingsResources()
    {
        CgsGui::GuiEventPlayAptMovie lMovie;
        lMovie.mpacMovieName = "";
        lMovie.miLevelNum = KI_APT_MOVIE_LEVEL;
        mpStateInterface->OutputViewState(lMovie);

        // X360 BrnOnlineGameRoomPlayerInfo.cpp:4224.
        CGS_ASSERT_AT(mpGuiCache != 0, "mpGuiCache", KAC_BRN_ONLINE_GAME_ROOM_PLAYER_INFO_ASSERT_FILE, 4224);

        if (mpGuiCache != 0)
        {
            mpGuiCache->UnloadResources(maSettingsResourceTuplesToLoad,
                                        static_cast<u32>(miNumSettingsResourcesToLoad));
        }
    }
}

// FOLDED FROM BrnOnlineGameRoomPlayerInfo_wH_17.cpp
// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-H partfile 17: the three remaining
// expect-list builders (previously blocked on a missing MenuComponent declaration).
//   SetExpectedLobbyComponents     @0x82484E78  (DWARF cpp:3879, assert line 3908)
//   SetExpectedPauseComponents     @0x82484FE0  (DWARF cpp:3917, assert line 3944)
//   SetExpectedViewEventComponents @0x824851A8  (DWARF cpp:4436, assert line 4482)
//
// Same shape as the two builders already landed in partfile 03
// (SetExpectedChallengesComponents / SetExpectedSettingsComponents): assert the cache
// pointer, drop the SCREEN flow's previous expected-component list, then re-append
// exactly the components that page owns, in the asm's call order. Reconstructed
// store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode + asm; see
// scratchpad/waveB/GameSource_Gui_Flow_Screen_States_BrnOnlineGameRoomPlayerInfo.cpp.asm.txt).
//
// Two console-literal displacements are deliberately NOT carried across:
//  * where the X360 reaches the GuiCache name-taking face (sub_824F87C0 == the
//    GuiCache::AppendExpectedAptComponent(GuiFlow, const char*) entry @0x824F87C0) with
//    r5 = <component>+4, the +4 is CgsGui::GuiComponent's macName buffer -- that is
//    GetName(), and the host buffer does not sit at +4 once the vptr widens, so the
//    reconstruction calls GetName() (partfile 03's convention);
//  * the lobby icon loop's r30 cursor walks a1+25080 with the console strides
//    148 (IconComponent) / 1184 (one Icons row) and biases every operand by the same
//    macName +4. Those are X360 sizeofs baked into the binary, so the loop is restored
//    as an index over maIcons[] with the eight icon members named, not as pointer
//    arithmetic over host-invalid strides.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT
#include "GameSource/Gui/BrnGuiCache.h"              // BrnGui::GuiCache (Clear/AppendExpectedAptComponent)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"      // BrnGui::GuiFlow (E_GUIFLOW_SCREEN)

// Asserts pass __FILE__; ARTIST's reads "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.cpp".

namespace BrnGui
{
    // @0x82484E78 -- arm the SCREEN flow for the main lobby page: the options animation,
    // the tab-title text field and the button-prompt animation, the player-name and
    // player-option menus, the stats panel, and then every status icon of all eight
    // player rows. The asm passes flow 0 (E_GUIFLOW_SCREEN) to every call.
    void OnlineGameRoomPlayerInfo::SetExpectedLobbyComponents()
    {
        CGS_ASSERT_AT(mpGuiCache != 0,
            "mpGuiCache", __FILE__, 3908 /* ARTIST 0x82484EA4 */);   // cpp:3908

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);

        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                               mOptionsAnimation.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                               mMessageText.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                               mPlayersButtonPromptsAnimation.GetName());

        mPlayerNameComponent.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);   // 0x82484F14
        mOptionComponent.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);       // 0x82484F28
        mPlayerStatsDisplay.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);

        // 0x82484F48..0x82484FD4: eight rows (r29 counts down from 8), each row's eight
        // icons appended in member order.
        for (s32 liPlayer = 0; liPlayer < KI_MAX_PLAYERS; ++liPlayer)
        {
            Icons& lIcons = maIcons[liPlayer];

            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                   lIcons.mMarkedManIcon.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                   lIcons.mRevengeStatusIcon.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                   lIcons.mReadyStatusIcon.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                   lIcons.mVideoStatusIcon.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                   lIcons.mVoipStatusIcon.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                   lIcons.mTeamStatusIcon.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                   lIcons.mGameConnectionTypeIcon.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                   lIcons.mVoipConnectionTypeIcon.GetName());
        }
    }

    // @0x82484FE0 -- arm the SCREEN flow for the pause page: the pause/security option
    // menu and its button-prompt animation are the whole page.
    void OnlineGameRoomPlayerInfo::SetExpectedPauseComponents()
    {
        CGS_ASSERT_AT(mpGuiCache != 0,
            "mpGuiCache", __FILE__, 3944 /* ARTIST 0x82485014 */);   // cpp:3944

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);

        mPauseComponent.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);   // 0x82485048

        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                               mPauseButtonPromptsAnimation.GetName());
    }

    // @0x824851A8 -- arm the SCREEN flow for the view-event page: the options menu and
    // the route-info panel (which walks its own owned text fields).
    void OnlineGameRoomPlayerInfo::SetExpectedViewEventComponents()
    {
        CGS_ASSERT_AT(mpGuiCache != 0,
            "mpGuiCache", __FILE__, 4482 /* ARTIST 0x824851DC */);   // cpp:4482

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);

        mViewEventOptions.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);   // 0x82485210
        mRouteInfoDisplay.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);   // 0x82485224
    }
}

// FOLDED FROM BrnOnlineGameRoomPlayerInfo_wH_18.cpp
// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-H partfile 18: the GuiCache arrival handler.
//   HandleGuiCacheEvent @0x824A3DD8  (DWARF cpp:912; asserts cpp:924 / cpp:956)
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode + asm; see
// scratchpad/waveB/GameSource_Gui_Flow_Screen_States_BrnOnlineGameRoomPlayerInfo.cpp.asm.txt).
// The class shape, the member map and the statics live in the wave-H keystone scaffold
// BrnOnlineGameRoomPlayerInfo.{h,cpp}.
//
// This is the screen's cache-arrival sink: the GUI module broadcasts the GuiCache pointer
// and every screen latches it. Three things hang off that broadcast here.
//
//  1. FIRST arrival only (mpGuiCache still null): latch the cache, drop the two "an online
//     game start is in flight" gates the menu path left set, service a splash-screen clear
//     that was requested before the cache existed, and ask the network module for the
//     current game options so the lobby page can be populated.
//  2. Every arrival while the challenges page is up: re-evaluate the select-button state.
//  3. Every arrival while free-burning: consume the "the online event has finished" flag.
//     For an ONLINE SHOWTIME event that means dropping the sat-nav tracker route and
//     driving the flow on to the post-event screen; for any other mode the flag is just
//     cleared and the screen stays where it is.
//
// This partfile was previously blocked on BrnGui::GuiTracker::ClearTracker (X360
// @0x824FA0A8) having no declaration; it is now declared on the real class in
// GameSource/Gui/SatNav/BrnGuiTracker.h and is called by name here.
//
// The out-queue post is written as a typed record, NOT as the console's baked immediates:
// the payload-size and payload-offset header words are host sizeof expressions and the
// AddEvent size argument is sizeof(the record). The X360's 1 / 12 / 16 literals are the
// 32-bit console record sizes (BrnInGame.cpp + the sibling wave-H partfiles precedent).
//
// The in-queue hands handlers the HEADER-STRIPPED payload, so the incoming cache pointer
// is read through a file-local payload view (the DWARF types the parameter
// const GuiEventCache*, whose home header hard-collides with BrnGuiEventTypeDefs.h -- the
// same accommodation BrnInGame.cpp and the sibling wave-H partfiles make).
//
// MERGE-WINDOW DELTA (rung 1 arbitrates): the Dec-2007 DecFIGS DWARF for this function
// lists GuiCache::SetWaitingForShowtimeResults / ResetStartingGameDueToPlayerJoin /
// SetEnteredOnlineViaEasyDrive calls and has no ClearSplashScreenOverlay, no
// ChallengeListComponent::ShowButton-driven challenges arm tail and no tracker/"TO_ST_POST"
// arm. ARTIST is the target and its asm is what is reconstructed here: two byte stores in
// the first-arrival arm (mbOnlineStartPending, mbIsStartingGameDueToPlayerJoin), no
// showtime-results or EasyDrive store at all.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/GameState/BrnGameStateSharedIO.h"                    // GsmIO::EGameModeType
#include "GameSource/Gui/BrnChallengeListComponent.h"                     // BrnGui::ChallengeListComponent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache (we are a friend)
#include "GameSource/Gui/SatNav/BrnGuiTracker.h"                          // BrnGui::GuiTracker::ClearTracker

namespace BrnGui
{
    namespace
    {
        // ---- AddEvent channel (the out-queue selector word) ---------------------------


        // ---- in-queue payload view -----------------------------------------------------
        // The handler reads a single word at payload +0 and treats it as the GuiCache
        // pointer (`lwz r11, 0(r27)` at 0x824A3DF0, re-read at 0x824A3E8C). DWARF param
        // type is const GuiEventCache*.
        struct GuiEventCachePayload : public CgsModule::Event
        {
            GuiCache* mpGuiCache;   // +0x00
        };

        // ---- out-queue wire record -----------------------------------------------------
        // OutputGuiEvent<BrnGui::GuiEventNetworkGameOptionsRequest>: the request carries no
        // fields, so the boxed payload is an empty type and the header's size word is 1.
        // The X360 writes { 1, 260, 12 } and AddEvent()s 16 bytes on channel 40; the
        // payload byte itself is never written (0x824A3ED0..0x824A3EEC).
        //
        // Event id: the asm bakes `li r11, 0x104` == 260. The Dec-2007 DWARF typedefs
        // GuiEventNetworkGameOptionsRequest as GuiEvent<258>; ARTIST is the later build and
        // its literal is what the queue actually carries (the same id drift the committed
        // BrnGuiEventTypeDefs.h records for GuiNewBurnoutSkillzEvent, 526 -> 541).
        struct GuiEventNetworkGameOptionsRequest
        {
        };

        typedef CgsGui::GuiEvent<260> NetworkGameOptionsRequestHeader;

        struct GuiEventNetworkGameOptionsRequestWire : public NetworkGameOptionsRequestHeader
        {
            GuiEventNetworkGameOptionsRequest mRequest;   // +0x0C (never written)

            GuiEventNetworkGameOptionsRequestWire()
            {
            }
        };

        // Layout pins for the record above: the console posts a 16-byte record whose size
        // word is 1 and whose payload offset word is 12. All three survive on the host.
        typedef char KAC_ASSERT_REQUEST_PAYLOAD_SIZE[
            sizeof(GuiEventNetworkGameOptionsRequest) == 1 ? 1 : -1];
        typedef char KAC_ASSERT_REQUEST_WIRE_SIZE[
            sizeof(CgsGui::GuiEventOut<GuiEventNetworkGameOptionsRequestWire>) == 16 ? 1 : -1];
    }

    // ------------------------------------------------------ HandleGuiCacheEvent @0x824A3DD8
    void OnlineGameRoomPlayerInfo::HandleGuiCacheEvent(const CgsModule::Event* lpEvent)
    {
        const GuiEventCachePayload* lpCacheEvent =
            reinterpret_cast<const GuiEventCachePayload*>(lpEvent);

        // Non-fatal (BeginAssert / FireAssert / EndAssert, no early-out) -- the X360 falls
        // straight through into the body, so the null cache is latched as-is.
        CGS_ASSERT(lpCacheEvent->mpGuiCache != 0,
                   "Invalid cache in HandleGuiCacheEvent::Update");   // cpp:924

        // ---- first arrival: latch the cache and open the lobby ------------------------
        if (mpGuiCache == 0)
        {
            mpGuiCache = lpCacheEvent->mpGuiCache;

            // The two "we are mid online-game-start" gates the menu path set on the way in.
            mpGuiCache->mbOnlineStartPending            = false;   // GuiCache +0x4B53
            mpGuiCache->mbIsStartingGameDueToPlayerJoin = false;   // GuiCache +0x4B4E

            // A splash clear was requested before the cache arrived; service it now that
            // there is a game mode to resolve the splash id against.
            if (mbNeedToClearSplashScreen)
            {
                ClearSplashScreenOverlay(&mOverlayCompleteEvent);
            }

            GuiEventNetworkGameOptionsRequestWire lGameOptionsRequest;
            mpStateInterface->OutputGuiEvent(lGameOptionsRequest);
        }

        // ---- challenges page: refresh the select-button state -------------------------
        // The X360 inlines ChallengeListComponent::ShowButton -- the
        // `lbz +0x981 / cmplw / stb +0x981 / stb 1, +0x980` guard at 0x824A3F0C..0x824A3F28
        // IS its body, and the DecFIGS DWARF lists ShowButton among this function's calls.
        if (meSubState == E_SUBSTATE_CHALLENGES)
        {
            mChallengeListComponent.ShowButton(ShouldShowButton());
        }

        // ---- free-burning: consume the "online event finished" flag -------------------
        if (mpGuiCache != 0 && mpGuiCache->mbOnlineEventCompleted &&
            meSubState == E_SUBSTATE_FREE_BURNING)
        {
            if (mpGuiCache->meGameModeType ==
                BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME)
            {
                // Non-fatal assert; the tracker is dereferenced either way.
                CGS_ASSERT_AT(mpGuiCache->GetGuiTracker() != 0,
                           "mpGuiCache->GetGuiTracker()", CGS_ARTIST_FILE, 956);   // cpp:956

                mpGuiCache->GetGuiTracker()->ClearTracker();
                SendStateEvent("TO_ST_POST");
            }

            mpGuiCache->mbOnlineEventCompleted = false;   // GuiCache +0x4B5A
        }
    }
}

// FOLDED FROM BrnOnlineGameRoomPlayerInfo_wN3_00.cpp
// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-N3 partfile 00: construction and the two
// remaining vtable entries.
//   OnlineGameRoomPlayerInfo::OnlineGameRoomPlayerInfo  (the screen-flow pool ctor)
//   GetResourcesToLoad                                  (vtable slot 8)
//   CheckForComponents                                  (vtable slot 13)
//
// The ctor has no statement of its own on the console: it chains CrashNavMap's ctor and
// then only runs the member default constructors (component vptrs, the four menu
// components, the toggle, the toggle group, the route-info panel). The empty body below
// is exactly that -- C++ runs the same member constructors in declaration order.
//
// GetResourcesToLoad is the image-wide folded two-store body (both outputs cleared): the
// screen hands the generic loader an empty list and loads its pages itself through the
// GuiCache (CheckForCompletedLoads).
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsHash.h"                    // CgsContainers::CgsHash::CalculateHash
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStreamBase::AppendFormat
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event
#include "GameSource/Gui/BrnGuiCache.h"                                   // GuiCache::GetWorldCameraPosition
#include "GameSource/Gui/SatNav/BrnMainMap.h"                             // MainMapComponent::GetWorldRect / GetViewRect
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"                         // MapTransform::Transform / GetDeviceRect
#include "rw/math/vpu/vector4_operation.h"                                // per-lane Vector4 product

#include <cmath>     // std::fabs
#include <cstring>   // std::strlen

namespace BrnGui
{
    namespace
    {
        // The apt component whose arrival places the cursor (the console hashes the
        // literal inline, then compares it with the event's component hash).

        // The screen-space bound beyond which the initial cursor position is reported as
        // broken (console float constant, 10000.0f).
        const f32 KF_CRAZY_CURSOR_POSITION = 10000.0f;

        // The two vector formats the debug report prints with -- the console's
        // StrStreamBase operator<<(Vector2) / operator<<(Vector4) bodies are exactly one
        // AppendFormat with these strings. Those two operators have no home in this tree
        // yet, so the lanes are pushed through AppendFormat with the same format here.

        // ---- in-queue payload view ----------------------------------------------------
        // FLAG: the event this virtual receives is typed only as CgsModule::Event; the one
        // field the console reads is the apt component's name hash at payload +0x0C.
        // The three leading words are not read.
        struct ComponentEventPayload : public CgsModule::Event
        {
            u32 mauUnread[3];        // +0x00 (not read by this handler)
            u32 muComponentHash;     // +0x0C CalculateHash of the component name
        };
    }

    // ------------------------------------------------------------ ctor
    OnlineGameRoomPlayerInfo::OnlineGameRoomPlayerInfo()
    {
    }

    // ------------------------------------------------------------ GetResourcesToLoad
    void OnlineGameRoomPlayerInfo::GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                                      u32* lpuNumberOfResources) const
    {
        *lppResourceTuples    = 0;
        *lpuNumberOfResources = 0;
    }

    // ------------------------------------------------------------ CheckForComponents
    // When the map cursor's apt component arrives, place the cursor over the player: the
    // cache's world camera position is flattened onto the map plane and taken from the
    // map's world rect into its view rect scaled to the device. A position beyond the
    // screen bound is reported on the debug log (with every rect that fed it) and
    // asserted, but the cursor is placed either way, then switched to icon selection.
    void OnlineGameRoomPlayerInfo::CheckForComponents(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, "Invalid event in OnlineGameRoomPlayerInfo::CheckForComponents");

        const u32 luCursorHash = CgsContainers::CgsHash::CalculateHash(
            const_cast<char*>("cursor_mc"),
            static_cast<s32>(std::strlen("cursor_mc")));

        if (reinterpret_cast<const ComponentEventPayload*>(lpEvent)->muComponentHash != luCursorHash)
        {
            return;
        }

        CGS_ASSERT_AT(mpGuiCache != 0, "mpGuiCache", CGS_ARTIST_FILE, 1242);

        const Vector3& lrv4CameraPosition = mpGuiCache->GetPlayerInfo()->mv3PlayerCarPosition;
        const Vector3 lv3InitialCursorPosWorldSpace(lrv4CameraPosition.mV);

        // The console streams the lane after the text; lowered to the static text.
        CGS_ASSERT(lv3InitialCursorPosWorldSpace.mV.x == lv3InitialCursorPosWorldSpace.mV.x,
                   "Crazy 3D position for the cursor : \n");
        CGS_ASSERT(lv3InitialCursorPosWorldSpace.mV.z == lv3InitialCursorPosWorldSpace.mV.z,
                   "Crazy 3D position for the cursor : \n");

        // Onto the map plane (world X/Z; the console's lane permute).
        const Vector2 lv2InitialCursorPosMapSpace = MapTransform::Flatten(lv3InitialCursorPosWorldSpace);

        const Vector2 lv2InitialCursorPosScreenSpace = MapTransform::Transform(
            lv2InitialCursorPosMapSpace,
            mMainMapComponent.GetWorldRect(),
            mMainMapComponent.GetViewRect() * MapTransform::GetDeviceRect());

        if (std::fabs(lv2InitialCursorPosScreenSpace.mV.x) > KF_CRAZY_CURSOR_POSITION ||
            std::fabs(lv2InitialCursorPosScreenSpace.mV.y) > KF_CRAZY_CURSOR_POSITION)
        {
            // Each line re-tests the log filter, as the console does.
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint << "==============================================\n";
            }
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint << "==============================================\n";
            }
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint << "Dave Addis : Something has gone wrong.!!!!\n";
            }
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint << "Please let me know...\n\n\n";
            }
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint << "MapTransform::Flatten( lv3InitialCursorPosWorldSpace ):";
                CgsDev::Log::gpDebugPrint->AppendFormat("(%f, %f)",
                                                        lv2InitialCursorPosMapSpace.mV.x,
                                                        lv2InitialCursorPosMapSpace.mV.y);
                *CgsDev::Log::gpDebugPrint << "\n";
            }
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                const Vector4 lv4WorldRect = mMainMapComponent.GetWorldRect();
                *CgsDev::Log::gpDebugPrint << "mMainMapComponent.GetWorldRect():";
                CgsDev::Log::gpDebugPrint->AppendFormat("(%f, %f, %f, %f)",
                                                        lv4WorldRect.mV.x, lv4WorldRect.mV.y,
                                                        lv4WorldRect.mV.z, lv4WorldRect.mV.w);
                *CgsDev::Log::gpDebugPrint << "\n";
            }
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                const Vector4 lv4ViewRect = mMainMapComponent.GetViewRect();
                *CgsDev::Log::gpDebugPrint << "mMainMapComponent.GetViewRect():";
                CgsDev::Log::gpDebugPrint->AppendFormat("(%f, %f, %f, %f)",
                                                        lv4ViewRect.mV.x, lv4ViewRect.mV.y,
                                                        lv4ViewRect.mV.z, lv4ViewRect.mV.w);
                *CgsDev::Log::gpDebugPrint << "\n";
            }
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                const Vector4 lv4DeviceRect = MapTransform::GetDeviceRect();
                *CgsDev::Log::gpDebugPrint << "MapTransform::GetDeviceRect():";
                CgsDev::Log::gpDebugPrint->AppendFormat("(%f, %f, %f, %f)",
                                                        lv4DeviceRect.mV.x, lv4DeviceRect.mV.y,
                                                        lv4DeviceRect.mV.z, lv4DeviceRect.mV.w);
                *CgsDev::Log::gpDebugPrint << "\n";
            }
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                const Vector4 lv4ScaledViewRect =
                    mMainMapComponent.GetViewRect() * MapTransform::GetDeviceRect();
                *CgsDev::Log::gpDebugPrint << "mMainMapComponent.GetViewRect() * MapTransform::GetDeviceRect():";
                CgsDev::Log::gpDebugPrint->AppendFormat("(%f, %f, %f, %f)",
                                                        lv4ScaledViewRect.mV.x, lv4ScaledViewRect.mV.y,
                                                        lv4ScaledViewRect.mV.z, lv4ScaledViewRect.mV.w);
                *CgsDev::Log::gpDebugPrint << "\n";
            }
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint << "lv2InitialCursorPosScreenSpace:";
                CgsDev::Log::gpDebugPrint->AppendFormat("(%f, %f)",
                                                        lv2InitialCursorPosScreenSpace.mV.x,
                                                        lv2InitialCursorPosScreenSpace.mV.y);
                *CgsDev::Log::gpDebugPrint << "\n";
            }

            // The console streams the lane after the text; lowered to the static text.
            CGS_ASSERT(lv2InitialCursorPosScreenSpace.mV.x == lv2InitialCursorPosScreenSpace.mV.x,
                       "Crazy 2D position for the cursor : \nTELL RICH GELDARD and give the output (TTY)\n");
            CGS_ASSERT(lv2InitialCursorPosScreenSpace.mV.y == lv2InitialCursorPosScreenSpace.mV.y,
                       "Crazy 2D position for the cursor : \nTELL RICH GELDARD and give the output (TTY)\n");
            CGS_ASSERT(std::fabs(lv2InitialCursorPosScreenSpace.mV.x) < KF_CRAZY_CURSOR_POSITION,
                       "Crazy position being printed in the cursor : \n");
            CGS_ASSERT(std::fabs(lv2InitialCursorPosScreenSpace.mV.y) < KF_CRAZY_CURSOR_POSITION,
                       "Crazy position being printed in the cursor : \n");
        }

        mCursor.SetPosition(lv2InitialCursorPosScreenSpace, false);
        meCursorMode = E_CURSORMODE_SELECTING_ICONS;
    }
}

// FOLDED FROM BrnOnlineGameRoomPlayerInfo_wN3_01.cpp
// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-N3 partfile 01: the page transitions.
//   ShowLobby / HideLobby
//   HideHUDAndEnterMenus
//   ShowMapScreen
//   ShowChallengesScreen / HideChallengePage
//   HideSettingsPage
//   ShowViewEventScreen / HideViewEventPage / UnloadViewEventResources
//
// Every page change is the same handful of GUI commands in a fixed order: the lobby apt
// movie is unmounted (PlayAptMovie with the empty name), the front-end map is
// (de)activated (191), the HUD is shown or hidden (148), car control is handed to or
// taken from the player (65), and the pages that return the player to driving also post
// the screen-flow command 533 and reset the main-map view state (213, on the view-state
// and internal channels). The "show" transitions only arm a loading sub-state; the load
// itself runs in CheckForCompletedLoads.
//
// Records go out on the GUI channel as the console's StateInterface::OutputGuiEvent<T>
// builds them: a GuiEventWrapper<T, 40> around the payload (size, id, payload offset,
// payload). The in-tree StateInterface::OutputGuiEvent<T> passes the event straight
// through on a channel equal to its id, so this file posts the wrapper itself.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEventWrapper
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue / PlayAptMovie
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event
#include "GameSource/GameState/Progression/BrnProfile.h"                  // Profile::HasPlayerSeenTrainingType
#include "GameSource/Gui/BrnGuiCache.h"                                   // GuiCache::UnloadResources / GetProfile
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // ShowHideHud / RequestCarControlChange / PlayerTexture / RequestTraining / ShowHideSatNav
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiEventActivateCrashNav
#include "SharedClasses/Progression/BrnTrainingTypes.h"                   // BrnProgression::ETrainingType

namespace BrnGui
{
    namespace
    {
        // The GUI-out channel every command here is posted on.
        // KI_CHANNEL_GUI_OUT is declared above in this translation unit.

        // The unmount of the page's apt movie: the empty movie name at level 3.

        // KI_APT_MOVIE_LEVEL is declared above in this translation unit.

        // The console's OutputGuiEvent<T> body: box the payload in a GuiEventWrapper<T,40>
        // and queue the wrapper on the GUI-out channel.
        template <class TEvent>
        void OutputGuiEventRecord(CgsGui::StateInterface* lpStateInterface, TEvent& lrEvent)
        {
            lpStateInterface->OutputGuiEvent(lrEvent);
        }

        // Id 533 -- the screen-flow command the "back to driving" pages post (the
        // committed BrnOnlinePlay.cpp / wave-H 07 post the same record). FLAG: an X360-only
        // event (PS3 ids 518..521 sit at X360 528..536, so 529..533 have no DWARF type);
        // the console builds only the header and never writes a payload byte
        // (HideChallengePage @0x82499778: `li r11, 0x215` straight after `stw r29`), so
        // the event is empty.
        struct GuiEventScreenFlowCommandPayload : public CgsGui::GuiEvent<533>
        {
        };

        // Id 580, payload size 1 -- posted as the challenge page opens (the game-state
        // bridge turns it into the every-player challenge-status request). FLAG: no type
        // home; payload byte not written by the console (the stale 0 of the record before).
        struct GuiEventRequestChallengeStatusPayload
        {
            u8 mu8Payload;
            s32 GetEventType() const { return 580; }
        };

        // Record-size pins: every payload here is pointer-free, so the wrapper must come
        // out at the console's AddEvent size.
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventWrapper<GuiEventShowHideHud, 40>) == 16), "148 record is 16 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventWrapper<GuiRequestCarControlChangeEvent, 40>) == 16), "65 record is 16 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventWrapper<GuiEventScreenFlowCommandPayload, 40>) == 16), "533 record is 16 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventWrapper<GuiEventRequestChallengeStatusPayload, 40>) == 16), "580 record is 16 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventWrapper<GuiEventNetworkOutputPlayerTexture, 40>) == 20), "264 record is 20 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventWrapper<GuiEventRequestTraining, 40>) == 16), "572 record is 16 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiEventActivateCrashNav>) == 20), "191 record is 20 bytes");

        // ---- the shared command sequences ---------------------------------------------
        void PostActivateCrashNav(CgsGui::StateInterface* lpStateInterface, bool lbActivate)
        {
            GuiEventActivateCrashNav lActivateCrashNav;
            lActivateCrashNav.muActivate = (lbActivate) ? 1u : 0u;
            lActivateCrashNav.muParam = 0;
            lpStateInterface->OutputGuiEvent(lActivateCrashNav);
        }

        void PostShowHideHud(CgsGui::StateInterface* lpStateInterface, bool lbShowHud)
        {
            GuiEventShowHideHud lShowHideHud;
            lShowHideHud.mbShow = lbShowHud;
            OutputGuiEventRecord(lpStateInterface, lShowHideHud);
        }

        void PostCarControlChange(CgsGui::StateInterface* lpStateInterface, bool lbEnableCarControl)
        {
            GuiRequestCarControlChangeEvent lCarControl;
            lCarControl.mbPlayerShouldHaveControl = lbEnableCarControl;
            OutputGuiEventRecord(lpStateInterface, lCarControl);
        }

        void PostScreenFlowCommand(CgsGui::StateInterface* lpStateInterface)
        {
            GuiEventScreenFlowCommandPayload lCommand;
            OutputGuiEventRecord(lpStateInterface, lCommand);
        }

        // The main-map view-state reset (213 { main map, no fade, hidden }) on the
        // view-state channel and then on the internal channel.
        void PostHideMainMap(CgsGui::StateInterface* lpStateInterface)
        {
            GuiEventShowHideSatNav lShowHideSatNav;
            lShowHideSatNav.Construct(GuiEventShowHideSatNav::E_MAPTYPE_MAIN, false, 0.0f);
            lpStateInterface->OutputViewState(lShowHideSatNav);
            lpStateInterface->OutputInternalState(lShowHideSatNav);
        }

        // Id 264: the player-texture feed; the lobby turns it to the player image on
        // entry and off on exit, with no particular player (-1).
        void PostPlayerTexture(CgsGui::StateInterface* lpStateInterface,
                               GuiEventNetworkOutputPlayerTexture::EOutput leOutput)
        {
            GuiEventNetworkOutputPlayerTexture lPlayerTexture;
            lPlayerTexture.meOutput          = leOutput;
            lPlayerTexture.mPlayerIDToOutput = -1;
            OutputGuiEventRecord(lpStateInterface, lPlayerTexture);
        }
    }

    // ------------------------------------------------------------ ShowLobby
    // Arm the lobby page load (from pause or from driving): feed the player image, take
    // the front-end map, the HUD and car control away.
    void OnlineGameRoomPlayerInfo::ShowLobby()
    {
        CGS_ASSERT_AT((meSubState == E_SUBSTATE_PAUSE) || (meSubState == E_SUBSTATE_FREE_BURNING),
                   "(meSubState == E_SUBSTATE_PAUSE) || (meSubState == E_SUBSTATE_FREE_BURNING)", CGS_ARTIST_FILE, 3547);

        meSubState = E_SUBSTATE_LOADING_SCREEN;

        PostPlayerTexture(mpStateInterface, GuiEventNetworkOutputPlayerTexture::E_OUTPUT_PLAYER_IMAGE);
        PostActivateCrashNav(mpStateInterface, false);
        PostShowHideHud(mpStateInterface, false);
        PostCarControlChange(mpStateInterface, false);
    }

    // ------------------------------------------------------------ HideLobby
    // Leave the lobby page for driving: unmount the movie, drop the selection and the two
    // lobby panels, stop the player image, and give the map, the HUD and car control back.
    void OnlineGameRoomPlayerInfo::HideLobby()
    {
        CGS_ASSERT_AT(meSubState == E_SUBSTATE_MAIN, "meSubState == E_SUBSTATE_MAIN", CGS_ARTIST_FILE, 3580);

        meSubState = E_SUBSTATE_FREE_BURNING;

        mpStateInterface->PlayAptMovie("", KI_APT_MOVIE_LEVEL);

        mSelectedPlayerID = -1;
        mPlayerStatsDisplay.Destruct();
        mRouteInfoDisplay.Destruct();

        PostPlayerTexture(mpStateInterface, GuiEventNetworkOutputPlayerTexture::E_OUTPUT_OFF);
        PostActivateCrashNav(mpStateInterface, true);
        PostScreenFlowCommand(mpStateInterface);
        PostShowHideHud(mpStateInterface, true);
        PostCarControlChange(mpStateInterface, true);
        PostHideMainMap(mpStateInterface);
    }

    // ------------------------------------------------------------ HideHUDAndEnterMenus
    // Take the front-end map, the HUD and car control away before a menu page loads; the
    // main map's view state is reset in between. The sub-state is left to the caller.
    void OnlineGameRoomPlayerInfo::HideHUDAndEnterMenus()
    {
        CGS_ASSERT_AT((meSubState == E_SUBSTATE_FREE_BURNING) || (meSubState == E_SUBSTATE_MAIN) ||
                   (meSubState == E_SUBSTATE_CHALLENGES) || (meSubState == E_SUBSTATE_VIEW_EVENT) ||
                   (meSubState == E_SUBSTATE_SETTINGS),
                   "(meSubState == E_SUBSTATE_FREE_BURNING) || (meSubState == E_SUBSTATE_MAIN) || "
                   "(meSubState == E_SUBSTATE_CHALLENGES) || (meSubState == E_SUBSTATE_VIEW_EVENT) || "
                   "(meSubState == E_SUBSTATE_SETTINGS)", CGS_ARTIST_FILE, 3721);

        PostActivateCrashNav(mpStateInterface, false);
        PostShowHideHud(mpStateInterface, false);
        PostHideMainMap(mpStateInterface);
        PostCarControlChange(mpStateInterface, false);
    }

    // ------------------------------------------------------------ ShowMapScreen
    // Arm the map page load; the first map update after it loads snaps the map onto the
    // local player.
    void OnlineGameRoomPlayerInfo::ShowMapScreen()
    {
        CGS_ASSERT_AT((meSubState == E_SUBSTATE_FREE_BURNING) || (meSubState == E_SUBSTATE_MAIN) ||
                   (meSubState == E_SUBSTATE_PAUSE),
                   "( meSubState == E_SUBSTATE_FREE_BURNING ) || ( meSubState == E_SUBSTATE_MAIN ) || "
                   "( meSubState == E_SUBSTATE_PAUSE )", CGS_ARTIST_FILE, 3844);

        PostActivateCrashNav(mpStateInterface, false);
        PostShowHideHud(mpStateInterface, false);
        PostCarControlChange(mpStateInterface, false);

        meSubState       = E_SUBSTATE_LOADING_MAP_SCREEN;
        mbFirstMapUpdate = true;
    }

    // ------------------------------------------------------------ ShowChallengesScreen
    // Arm the challenge page load, ask for the every-player challenge status, and raise
    // the first of the three challenge training tips the player has not seen yet.
    void OnlineGameRoomPlayerInfo::ShowChallengesScreen()
    {
        CGS_ASSERT_AT((meSubState == E_SUBSTATE_PAUSE) || (meSubState == E_SUBSTATE_FREE_BURNING),
                   "(meSubState == E_SUBSTATE_PAUSE) || (meSubState == E_SUBSTATE_FREE_BURNING)", CGS_ARTIST_FILE, 3672);

        meSubState = E_SUBSTATE_LOADING_CHALLENGES_SCREEN;

        PostActivateCrashNav(mpStateInterface, false);
        PostShowHideHud(mpStateInterface, false);

        GuiEventRequestChallengeStatusPayload lStatusRequest;
        lStatusRequest.mu8Payload = 0;
        OutputGuiEventRecord(mpStateInterface, lStatusRequest);

        const BrnProgression::Profile* lpProfile = mpGuiCache->GetProfile();
        BrnProgression::ETrainingType leTrainingType;
        if (!lpProfile->HasPlayerSeenTrainingType(BrnProgression::E_TRAINING_TYPE_OPEN_ONLINE_CHALLENGES))
        {
            leTrainingType = BrnProgression::E_TRAINING_TYPE_OPEN_ONLINE_CHALLENGES;
        }
        else if (!lpProfile->HasPlayerSeenTrainingType(BrnProgression::E_TRAINING_TYPE_OPEN_ONLINE_CHALLENGES_2))
        {
            leTrainingType = BrnProgression::E_TRAINING_TYPE_OPEN_ONLINE_CHALLENGES_2;
        }
        else if (!lpProfile->HasPlayerSeenTrainingType(BrnProgression::E_TRAINING_TYPE_OPEN_ONLINE_CHALLENGES_3))
        {
            leTrainingType = BrnProgression::E_TRAINING_TYPE_OPEN_ONLINE_CHALLENGES_3;
        }
        else
        {
            return;
        }

        GuiEventRequestTraining lRequestTraining;
        lRequestTraining.meTrainingType = static_cast<s32>(leTrainingType);
        OutputGuiEventRecord(mpStateInterface, lRequestTraining);
    }

    // ------------------------------------------------------------ HideChallengePage
    // Leave the challenge page for driving.
    void OnlineGameRoomPlayerInfo::HideChallengePage()
    {
        CGS_ASSERT_AT(meSubState == E_SUBSTATE_CHALLENGES, "meSubState == E_SUBSTATE_CHALLENGES", CGS_ARTIST_FILE, 3635);

        meSubState = E_SUBSTATE_FREE_BURNING;

        mpStateInterface->PlayAptMovie("", KI_APT_MOVIE_LEVEL);
        PostActivateCrashNav(mpStateInterface, true);
        PostScreenFlowCommand(mpStateInterface);
        PostShowHideHud(mpStateInterface, true);
        PostCarControlChange(mpStateInterface, true);
    }

    // ------------------------------------------------------------ HideSettingsPage
    // Leave the settings page for driving.
    void OnlineGameRoomPlayerInfo::HideSettingsPage()
    {
        CGS_ASSERT_AT(meSubState == E_SUBSTATE_SETTINGS, "meSubState == E_SUBSTATE_SETTINGS", CGS_ARTIST_FILE, 4274);

        meSubState = E_SUBSTATE_FREE_BURNING;

        mpStateInterface->PlayAptMovie("", KI_APT_MOVIE_LEVEL);
        PostActivateCrashNav(mpStateInterface, true);
        PostScreenFlowCommand(mpStateInterface);
        PostShowHideHud(mpStateInterface, true);
        PostCarControlChange(mpStateInterface, true);
    }

    // ------------------------------------------------------------ ShowViewEventScreen
    // Arm the view-event page load.
    void OnlineGameRoomPlayerInfo::ShowViewEventScreen()
    {
        CGS_ASSERT_AT((meSubState == E_SUBSTATE_PAUSE) || (meSubState == E_SUBSTATE_FREE_BURNING),
                   "(meSubState == E_SUBSTATE_PAUSE) || (meSubState == E_SUBSTATE_FREE_BURNING)", CGS_ARTIST_FILE, 4419);

        meSubState = E_SUBSTATE_LOADING_VIEW_EVENT_SCREEN;

        PostActivateCrashNav(mpStateInterface, false);
        PostShowHideHud(mpStateInterface, false);
        PostCarControlChange(mpStateInterface, false);
    }

    // ------------------------------------------------------------ HideViewEventPage
    // Leave the view-event page for driving: drop the route-info panel, unmount the
    // movie, give everything back and reset the main-map view state.
    void OnlineGameRoomPlayerInfo::HideViewEventPage()
    {
        CGS_ASSERT_AT((meSubState == E_SUBSTATE_VIEW_EVENT) ||
                   (meSubState == E_SUBSTATE_LOADING_VIEW_EVENT_COMPONENTS),
                   "(meSubState == E_SUBSTATE_VIEW_EVENT) || "
                   "(meSubState == E_SUBSTATE_LOADING_VIEW_EVENT_COMPONENTS)", CGS_ARTIST_FILE, 4372);

        meSubState = E_SUBSTATE_FREE_BURNING;

        mRouteInfoDisplay.Destruct();

        mpStateInterface->PlayAptMovie("", KI_APT_MOVIE_LEVEL);
        PostActivateCrashNav(mpStateInterface, true);
        PostScreenFlowCommand(mpStateInterface);
        PostShowHideHud(mpStateInterface, true);
        PostCarControlChange(mpStateInterface, true);
        PostHideMainMap(mpStateInterface);
    }

    // ------------------------------------------------------------ UnloadViewEventResources
    // Tear the view-event page down: reset the main-map view state, drop the route-info
    // panel, unmount the movie, and hand the page's resource tuples back to the cache.
    void OnlineGameRoomPlayerInfo::UnloadViewEventResources()
    {
        PostHideMainMap(mpStateInterface);

        mRouteInfoDisplay.Destruct();

        mpStateInterface->PlayAptMovie("", KI_APT_MOVIE_LEVEL);

        CGS_ASSERT_AT(mpGuiCache != 0, "mpGuiCache", CGS_ARTIST_FILE, 4255);

        if (mpGuiCache != 0)
        {
            mpGuiCache->UnloadResources(maViewEventResourceTuplesToLoad,
                                        static_cast<u32>(miNumViewEventResourcesToLoad));
        }
    }
}

// FOLDED FROM BrnOnlineGameRoomPlayerInfo_wN3_02.cpp
// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-N3 partfile 02: the free-burn challenge page.
//   ShouldShowButton
//   ShowChallengesOptions
//   HandleControllerInputChallengesSubState
//
// The page is a player-count toggle (2..8 players) over the challenge list component.
// Only the lobby host may start a challenge, and only when the chosen player count
// matches the lobby (or a debug "all challenges N-player" override is on) -- that is
// ShouldShowButton. Every highlight change the host makes is published to the other
// players as a GuiChallengeSelectedEvent (573); selecting starts the challenge, backing
// out withdraws the highlight.
//
// Records go out on the GUI channel as the console's StateInterface::OutputGuiEvent<T>
// builds them (a GuiEventWrapper<T, 40>); the in-tree OutputGuiEvent<T> passes the event
// straight through on a channel equal to its id, so this file posts the wrapper itself.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEventWrapper
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event
#include "GameSource/GameState/BrnGameStateSharedIO.h"                    // GsmIO::E_MODE_ONLINE_FREE_BURN_LOBBY
#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h" // the two debug player-count overrides
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // BrnGui::GuiChallengeSelectedEvent
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiAudioTriggerEvent
#include "GameSource/Gui/Events/BrnGuiEventNetworkGameParams.h"           // BrnGui::GuiEventNetworkGameParams
#include "SharedClasses/DataLists/ChallengeList.h"                        // ChallengeList::GetChallengeData
#include "SharedClasses/DataLists/ChallengeListEntry.h"                   // ChallengeListEntry::GetChallengeStyle

#include <cstring>   // std::memcpy (the audio payload re-box)

namespace BrnGui
{
    namespace
    {
        // The GUI-out channel every record here is posted on.
        // KI_CHANNEL_GUI_OUT is declared above in this translation unit.

        // ---- controller actions this sub-state answers (event-6 payload word +4) --------
        const s32 KI_ACTION_LIST_PREVIOUS   = 0x29;   // ')' challenge list up
        const s32 KI_ACTION_LIST_NEXT       = 0x2A;   // '*' challenge list down
        const s32 KI_ACTION_PLAYERS_PREVIOUS = 0x2B;  // '+' player-count toggle back
        const s32 KI_ACTION_PLAYERS_NEXT    = 0x2C;   // ',' player-count toggle forward



        // The GuiChallengeSelectedEvent selector actions. FLAG: the enum has no recovered
        // name; the values are the ones each site stores. A highlight is published with 2
        // when the list's button shows and 3 when it does not (followed by a 1 in that
        // case), backing out publishes 3 then 1, and selecting publishes 0.
        const s32 KI_SELECTOR_ACTION_SELECT        = 0;
        const s32 KI_SELECTOR_ACTION_CLEAR         = 1;
        const s32 KI_SELECTOR_ACTION_HIGHLIGHT     = 2;
        const s32 KI_SELECTOR_ACTION_UNHIGHLIGHT   = 3;

        // The player-count toggle: seven options, ids 2..8, labelled by the class's
        // KAPC_CHALLENGE_TOGGLE_ITEM_STRING_IDS. A lobby of one shows the 2-player row.
        const s32 KI_NUM_PLAYER_COUNT_OPTIONS = 7;
        const s32 KI_MIN_CHALLENGE_PLAYERS    = 2;

        // The back press's audio cue (action 7, "GO_BACK", empty component and movie).
        // KI_AUDIO_ACTION_MENU_CUE is declared above in this translation unit.



        // The console's OutputGuiEvent<T> body: box the payload in a GuiEventWrapper<T,40>
        // and queue the wrapper on the GUI-out channel.






        // The audio trigger's wire payload: the 100-byte body of GuiAudioTriggerEvent
        // under id 457 (OutputGuiEvent<GuiAudioTriggerEvent> copies the payload out and
        // boxes it as { 100, 457, 12, payload }). The committed GuiAudioTriggerEvent keeps
        // a 12-byte header of its own in front of the payload, so the payload is re-boxed.
        struct GuiAudioTriggerPayload457
        {
            char macComponent[32];
            s32  meAction;
            char macLabel[32];
            char macMovie[32];
            s32 GetEventType() const { return 457; }
        };

        void PostAudioTrigger(CgsGui::StateInterface* lpStateInterface, s32 leAction,
                              const char* lpcLabel)
        {
            GuiAudioTriggerEvent lAudioTrigger;
            lAudioTrigger.Construct(leAction, "", lpcLabel, "");

            GuiAudioTriggerPayload457 lPayload;
            std::memcpy(&lPayload, lAudioTrigger.macComponent, sizeof(lPayload));
            OutputGuiEventRecord(lpStateInterface, lPayload);
        }

        CGS_STATIC_ASSERT((sizeof(GuiAudioTriggerPayload457) == 100), "audio trigger payload is 100 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventWrapper<GuiChallengeSelectedEvent, 40>) == 32),
                      "573 record is 32 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventWrapper<GuiEventNetworkGameParams, 40>) == 492),
                      "257 record is 492 bytes");
    }

    // ------------------------------------------------------------ ShouldShowButton
    // The host may start the highlighted challenge when the toggle's player count equals
    // the lobby's (either debug override lifts that requirement).
    bool OnlineGameRoomPlayerInfo::ShouldShowButton()
    {
        bool lbShowButton = false;

        if (mpGuiCache->IsLocalPlayerHost())
        {
            if (BrnGameState::ChallengeManager::mbChallengesAreAllOnePlayer ||
                BrnGameState::ChallengeManager::mbChallengesAreAllTwoPlayer ||
                mpGuiCache->GetOnlinePlayerCount() == static_cast<s32>(mChallengeToggle.GetHighlightedId()))
            {
                lbShowButton = true;
            }
        }

        return lbShowButton;
    }

    // ------------------------------------------------------------ ShowChallengesOptions
    // Build the player-count toggle on the lobby's size, set the list up for it, show the
    // scroll arrows that apply, and (host only, when startable) publish the highlight.
    void OnlineGameRoomPlayerInfo::ShowChallengesOptions()
    {
        u64 laSelectableIDs[KI_NUM_PLAYER_COUNT_OPTIONS];
        for (s32 liIndex = 0; liIndex < KI_NUM_PLAYER_COUNT_OPTIONS; ++liIndex)
        {
            laSelectableIDs[liIndex] = static_cast<u64>(KI_MIN_CHALLENGE_PLAYERS + liIndex);
        }

        mChallengeToggle.Clear();
        mChallengeToggle.SetActive(true);
        mChallengeToggle.SetupMenuToggle(KI_NUM_PLAYER_COUNT_OPTIONS, true,
                                         KAC_CHALLENGE_TOGGLE_TITLE_STRING_ID,
                                         const_cast<const char**>(KAPC_CHALLENGE_TOGGLE_ITEM_STRING_IDS),
                                         laSelectableIDs);
        mChallengeToggle.SetHighlightable(true);
        mChallengeToggle.SetHighlighted(true);
        mChallengeToggle.SetSelectable(true);

        const s32 liNumPlayers = static_cast<s32>(mpGuiCache->GetOnlinePlayerCount());
        const s32 liToggleId   = (liNumPlayers == 1) ? KI_MIN_CHALLENGE_PLAYERS : liNumPlayers;
        if (mChallengeToggle.mItemText.HighlightId(static_cast<u64>(static_cast<s64>(liToggleId))))
        {
            mChallengeToggle.SetDirty();
        }

        mChallengeListComponent.Setup(mpGuiCache,
                                      static_cast<s32>(mChallengeToggle.GetHighlightedId()),
                                      ShouldShowButton());

        mChallengeListUpArrowAnimation.Run(mChallengeListComponent.IsAtTopOfList()
                                               ? KAPC_ANIMATION_STATES[1] : KAPC_ANIMATION_STATES[0]);
        mChallengeListDownArrowAnimation.Run(mChallengeListComponent.IsAtBottomOfList()
                                                 ? KAPC_ANIMATION_STATES[1] : KAPC_ANIMATION_STATES[0]);

        if (mChallengeListComponent.GetHighlightedChallengeID() != 0 && ShouldShowButton())
        {
            GuiChallengeSelectedEvent lChallengeSelected;
            lChallengeSelected.miSelectorAction = KI_SELECTOR_ACTION_HIGHLIGHT;
            lChallengeSelected.mChallengeID     = mChallengeListComponent.GetHighlightedChallengeID();
            lChallengeSelected.miChall = static_cast<s32>(
                mpGuiCache->GetFreeburnChallengeList()->GetChallengeData(lChallengeSelected.mChallengeID)
                    ->GetChallengeStyle());
            OutputGuiEventRecord(mpStateInterface, lChallengeSelected);
        }
    }

    // ------------------------------------------------------------ HandleControllerInputChallengesSubState
    void OnlineGameRoomPlayerInfo::HandleControllerInputChallengesSubState(const CgsModule::Event* lpEvent)
    {
        // The header-stripped GuiEventControllerInputPressed payload: pad id, action id.
        const s32 liAction = reinterpret_cast<const s32*>(lpEvent)[1];

        bool lbListChanged = false;

        switch (liAction)
        {
            case KI_ACTION_LIST_PREVIOUS:
                lbListChanged = mChallengeListComponent.HighlightPrevious();
                break;

            case KI_ACTION_LIST_NEXT:
                lbListChanged = mChallengeListComponent.HighlightNext();
                break;

            case KI_ACTION_PLAYERS_PREVIOUS:
                if (mChallengeToggle.HighlightPrevious())
                {
                    const bool lbShowButton = ShouldShowButton();
                    mChallengeListComponent.Setup(mpGuiCache,
                                                  static_cast<s32>(mChallengeToggle.GetHighlightedId()),
                                                  lbShowButton);
                    lbListChanged = true;
                }
                break;

            case KI_ACTION_PLAYERS_NEXT:
                if (mChallengeToggle.HighlightNext())
                {
                    const bool lbShowButton = ShouldShowButton();
                    mChallengeListComponent.Setup(mpGuiCache,
                                                  static_cast<s32>(mChallengeToggle.GetHighlightedId()),
                                                  lbShowButton);
                    lbListChanged = true;
                }
                break;

            case KI_ACTION_SELECT:
            {
                GuiChallengeSelectedEvent lChallengeSelected;
                lChallengeSelected.mChallengeID     = mChallengeListComponent.GetHighlightedChallengeID();
                lChallengeSelected.miSelectorAction = KI_SELECTOR_ACTION_SELECT;

                if (lChallengeSelected.mChallengeID != 0 && mChallengeListComponent.mbShowButton)
                {
                    lChallengeSelected.miChall =
                        mChallengeListComponent.GetChallengeStyle(lChallengeSelected.mChallengeID);
                    OutputGuiEventRecord(mpStateInterface, lChallengeSelected);

                    // Starting a challenge turns the lobby into the free-burn lobby mode if
                    // it is not one already.
                    if (mpGuiCache->GetOnlineGameMode() !=
                        BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY)
                    {
                        GuiEventNetworkGameParams lGameOptions =
                            *mpGuiCache->GetOnlineGameModeOptions();
                        lGameOptions.meGameMode =
                            BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY;
                        OutputGuiEventRecord(mpStateInterface, lGameOptions);
                    }

                    mChallengeToggle.Unloaded();
                    UnloadChallengeResources();
                    HideChallengePage();
                }
                break;
            }

            case KI_ACTION_BACK:
            {
                PostAudioTrigger(mpStateInterface, KI_AUDIO_ACTION_MENU_CUE, "GO_BACK");

                if (mChallengeListComponent.GetHighlightedChallengeID() != 0 &&
                    mChallengeListComponent.mbShowButton)
                {
                    GuiChallengeSelectedEvent lChallengeSelected;
                    lChallengeSelected.mChallengeID     = mChallengeListComponent.GetHighlightedChallengeID();
                    lChallengeSelected.miSelectorAction = KI_SELECTOR_ACTION_UNHIGHLIGHT;
                    lChallengeSelected.miChall =
                        mChallengeListComponent.GetChallengeStyle(lChallengeSelected.mChallengeID);
                    OutputGuiEventRecord(mpStateInterface, lChallengeSelected);

                    lChallengeSelected.miSelectorAction = KI_SELECTOR_ACTION_CLEAR;
                    OutputGuiEventRecord(mpStateInterface, lChallengeSelected);
                }

                if (mbShownFromPause)
                {
                    mChallengeToggle.Unloaded();
                    UnloadChallengeResources();
                    HideHUDAndEnterMenus();
                    ShowPauseScreen();
                }
                else
                {
                    UnloadChallengeResources();
                    HideChallengePage();
                }
                break;
            }

            default:
                break;
        }

        if (lbListChanged)
        {
            mChallengeListUpArrowAnimation.Run(mChallengeListComponent.IsAtTopOfList()
                                                   ? KAPC_ANIMATION_STATES[1] : KAPC_ANIMATION_STATES[0]);
            mChallengeListDownArrowAnimation.Run(mChallengeListComponent.IsAtBottomOfList()
                                                     ? KAPC_ANIMATION_STATES[1] : KAPC_ANIMATION_STATES[0]);

            if (mChallengeListComponent.GetHighlightedChallengeID() != 0 && mpGuiCache->IsLocalPlayerHost())
            {
                GuiChallengeSelectedEvent lChallengeSelected;
                lChallengeSelected.miSelectorAction = mChallengeListComponent.mbShowButton
                                                          ? KI_SELECTOR_ACTION_HIGHLIGHT
                                                          : KI_SELECTOR_ACTION_UNHIGHLIGHT;
                lChallengeSelected.mChallengeID = mChallengeListComponent.GetHighlightedChallengeID();
                lChallengeSelected.miChall = static_cast<s32>(
                    mpGuiCache->GetFreeburnChallengeList()->GetChallengeData(lChallengeSelected.mChallengeID)
                        ->GetChallengeStyle());
                OutputGuiEventRecord(mpStateInterface, lChallengeSelected);

                if (!mChallengeListComponent.mbShowButton)
                {
                    lChallengeSelected.miSelectorAction = KI_SELECTOR_ACTION_CLEAR;
                    OutputGuiEventRecord(mpStateInterface, lChallengeSelected);
                }
            }
        }
    }
}

// FOLDED FROM BrnOnlineGameRoomPlayerInfo_wN3_03.cpp
// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-N3 partfile 03: the remaining handlers.
//   HandleLeavingGameFailedEvent
//   HandleLaunchingEvent
//   ShowSplashScreen
//   HandleControllerInputPauseSubState
//   HandleControllerInputPressedMapSubState
//   HandleSettingChanged
//
// Overlay records go out on the GUI channel as the console's OutputGuiEvent<T> builds
// them: { payload size, id, 16, <pad>, payload } -- both overlay payloads are 8-aligned
// (they start with a compressed CgsID). Same wire views as the wave-H partfiles 08/14.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStreamBase
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N> / GuiEventWrapper
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event
#include "GameSource/GameState/BrnCgsPlayerName.h"                        // CgsNetwork::PlayerName
#include "GameSource/GameState/BrnGameStateSharedIO.h"                    // GsmIO::EGameModeType
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // GuiEventScoreboardRequestGamercardEvent
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiOverlayRequest / GuiOverlayWaitFinishRequest / GuiAudioTriggerEvent
#include "GameSource/Gui/Flow/Screen/Components/BrnCrashNavPanel.h"       // BrnGui::CrashNavPanel
#include "GameSource/Gui/Flow/Screen/States/Shared/BrnScreenShared.h"     // GetSplashScreenIDForGameMode
#include "GameSource/Gui/SatNav/BrnMainMap.h"                             // MainMapComponent::SetZoom / SetDesiredWorldCentre
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h"  // InGamePlayerStatusData

#include <cstring>   // std::memcpy (the audio payload re-box)
#include "GameSource/Gui/Flow/Screen/Components/BrnGuiNetworkPlayerStats.h" // GuiNetworkPlayerStats::Destruct

namespace BrnGui
{
    namespace
    {


        // ---- controller actions (event-6 payload word +4) -------------------------------





        const s32 KI_ACTION_GAMERCARD     = 0x33;   // '3'
        const s32 KI_ACTION_ZOOM_OUT      = 0x38;   // '8' (released by the map-release handler)

        // The pause option that stays on the pause page (the security menu reuses the
        // pause component, so its resources are not unloaded).
        const s32 KI_PAUSE_OPTION_SECURITY = 9;

        // The audio cues: action 4 "B5MenuItem" on select, action 7 "GO_BACK" on back.
        const s32  KI_AUDIO_ACTION_MENU_ITEM = 4;
        // KI_AUDIO_ACTION_MENU_CUE is declared above in this translation unit.



        // The map pull-back the zoom-out press asks for (console float constant 9000.0f)
        // and the world centre it recentres on: the 16-byte global the console loads is
        // filled at start-up by a dynamic initialiser with { -460.0f, -915.0f, 0, 0 }.
        const f32 KF_MAP_ZOOM_OUT = 9000.0f;

        Vector2 KV2_ZOOMED_OUT_MAP_CENTRE()
        {
            Vector2 lv2Centre;
            lv2Centre.mV.x = -460.0f;
            lv2Centre.mV.y = -915.0f;
            lv2Centre.mV.z = 0.0f;
            lv2Centre.mV.w = 0.0f;
            return lv2Centre;
        }

        // The road-panel score mode in which a friend row can be selected (the friend
        // scores list), as CrashNavPanel::GetRoadPanelScoreMode reports it.
        const s32 KI_ROAD_PANEL_SCORE_MODE_FRIENDS = 1;

        // The console's own game-mode bound for the splash assert: the game has eighteen
        // modes (the splash table has eighteen slots), one more than the committed
        // GameStateModuleIO::E_MODE_COUNT.
        const s32 KI_GAME_MODE_COUNT = 18;

        // ---- the settings page's toggle rows ------------------------------------------
        const s32 KI_SETTING_ROW_CAMERA = 0;
        const s32 KI_SETTING_ROW_MUSIC  = 1;
        const s32 KI_SETTING_ROW_SFX    = 2;
        const s32 KI_SETTING_ROW_TIPS   = 3;

        // The tips row's two options: id 0 is "on", id 1 is "off".
        const u64 KU_TIPS_OPTION_ON  = 0;
        const u64 KU_TIPS_OPTION_OFF = 1;

        // ---- overlay names ----------------------------------------------------------



        const char KAC_OVERLAY_HOST_LAUNCHING[]   = "CNOnlLchGmH";
        const char KAC_OVERLAY_PEER_LAUNCHING[]   = "CNOnlLchGame";

        // The launch overlay's two message parameters: the launching player's name, then
        // the game mode's string id.
        const CgsGui::PopupParamTypes KU_LAUNCH_PARAM_PLAYER_NAME = CgsGui::E_POPUPPARAMTYPES_STRING;
        const CgsGui::PopupParamTypes KU_LAUNCH_PARAM_GAME_MODE = CgsGui::E_POPUPPARAMTYPES_STRING_ID;

        // ---- in-queue payload view ----------------------------------------------------
        // Event 57 ("launching"): the mode being launched, then the launching player.
        // FLAG: the committed CgsGui::GuiEventNetworkLaunching is an opaque 8-byte blob;
        // the two words are named for what this handler reads (`lwz 0` indexes the mode
        // string table, `lwz 4` is looked up as a network player id).
        struct GuiEventNetworkLaunchingPayload : public CgsModule::Event
        {
            s32 meGameMode;   // +0x00
            s32 mPlayerID;    // +0x04
        };

        // ---- out-queue wire records -------------------------------------------------
        // { 288, 184, 16, <pad>, the 288-byte request }, 304 bytes.
        // GuiOverlayRequestWire is declared above in this translation unit.








        // { 8, 188, 16, <pad>, the compressed overlay id }, 24 bytes.
        // GuiOverlayWaitFinishWire is declared above in this translation unit.








        // The audio trigger's wire payload (the 100-byte body under id 457; see
        // BrnOnlineGameRoomPlayerInfo_wN3_02.cpp for why it is re-boxed).
        // GuiAudioTriggerPayload457 is declared above in this translation unit.








        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiOverlayRequest>) == 304), "overlay request record is 304 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiOverlayWaitFinishRequest>) == 24), "overlay wait-finish record is 24 bytes");
        CGS_STATIC_ASSERT((sizeof(GuiAudioTriggerPayload457) == 100), "audio trigger payload is 100 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiEventScoreboardRequestGamercardEvent>) == 28), "gamercard record is 28 bytes");

        void PostOverlayRequest(CgsGui::StateInterface* lpStateInterface, GuiOverlayRequestWire& lrWire)
        {
            lpStateInterface->OutputGuiEvent(lrWire.mRequest);   // {288, 184, 16} + 288 on channel 40
        }








        void PostLeavingGameFailureAudioTrigger(CgsGui::StateInterface* lpStateInterface, s32 leAction,
                              const char* lpcLabel)
        {
            GuiAudioTriggerEvent lAudioTrigger;
            lAudioTrigger.Construct(leAction, "", lpcLabel, "");

            GuiAudioTriggerPayload457 lPayload;
            std::memcpy(&lPayload, lAudioTrigger.macComponent, sizeof(lPayload));

            lpStateInterface->OutputGuiEvent(lPayload);
        }

        // OutputGuiEvent<GuiEventScoreboardRequestGamercardEvent>: the committed type
        // carries its { 16, 120, 12 } header, so the event object is the record.
        void PostGamercardRequest(CgsGui::StateInterface* lpStateInterface, const char* lpcPlayerName)
        {
            GuiEventScoreboardRequestGamercardEvent lRequest;
            lRequest.mPlayerName.Construct(lpcPlayerName);
            lpStateInterface->OutputGuiEvent(lRequest);
        }
    }

    // ------------------------------------------------------------ HandleLeavingGameFailedEvent
    // The leave request failed: stop waiting on the "leaving game" overlay.
    void OnlineGameRoomPlayerInfo::HandleLeavingGameFailedEvent(const CgsModule::Event* lpEvent)
    {
        GuiOverlayWaitFinishRequest lWaitFinish;
        lWaitFinish.Construct("CNOnlLvgGame");
        mpStateInterface->OutputGuiEvent(lWaitFinish);
    }

    // ------------------------------------------------------------ HandleLaunchingEvent
    // A game is launching: stop waiting on the leave questions and the leaving overlay,
    // then raise the launch overlay -- the host's own version, or the peers' version
    // naming the launching player -- with the mode's string id as its second parameter.
    void OnlineGameRoomPlayerInfo::HandleLaunchingEvent(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, "Invalid event sent to OnlineGameRoomPlayerInfo::HandleLaunchingEvent");
        CGS_ASSERT_AT(lpEvent != 0, "lpLaunchingEvent", CGS_ARTIST_FILE, 2567);

        const GuiEventNetworkLaunchingPayload* lpLaunchingEvent =
            reinterpret_cast<const GuiEventNetworkLaunchingPayload*>(lpEvent);

        PostOverlayWaitFinish(mpStateInterface, "CNOnlLvGmQn");
        PostOverlayWaitFinish(mpStateInterface, "CNOnlLvChaQn");
        PostOverlayWaitFinish(mpStateInterface, "CNOnlLvgGame");

        CGS_ASSERT_AT(mpGuiCache != 0, "mpGuiCache", CGS_ARTIST_FILE, 2578);

        GuiOverlayRequestWire lWire;
        if (mpGuiCache != 0 && mpGuiCache->IsLocalPlayerHost())
        {
            lWire.mRequest.Construct("CNOnlLchGmH");
        }
        else
        {
            const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerStatusData =
                mpGuiCache->GetOnlinePlayerInfo(lpLaunchingEvent->mPlayerID);
            CGS_ASSERT_AT(lpPlayerStatusData != 0, "lpPlayerStatusData", CGS_ARTIST_FILE, 2591);

            lWire.mRequest.Construct("CNOnlLchGame");
            lWire.mRequest.AddMessageParam(KU_LAUNCH_PARAM_PLAYER_NAME,
                                           lpPlayerStatusData->mPlayerName.GetPlayerName());
        }

        lWire.mRequest.AddMessageParam(KU_LAUNCH_PARAM_GAME_MODE,
                                       KPAC_MODE_STRINGS[lpLaunchingEvent->meGameMode]);

        PostOverlayRequest(mpStateInterface, lWire);
    }

    // ------------------------------------------------------------ ShowSplashScreen
    // Raise the current game mode's splash overlay and clear the pending-splash latch.
    void OnlineGameRoomPlayerInfo::ShowSplashScreen()
    {
        CGS_ASSERT_AT(mpGuiCache != 0, "mpGuiCache",
                      KAC_BRN_ONLINE_GAME_ROOM_PLAYER_INFO_ASSERT_FILE, 3877);

        const s32 leGameModeType = mpGuiCache->GetGameMode();
        CGS_ASSERT_AT(leGameModeType > BrnGameState::GameStateModuleIO::E_MODE_NONE,
                      "leGameModeType > GsmIO::E_MODE_NONE", KAC_BRN_ONLINE_GAME_ROOM_PLAYER_INFO_ASSERT_FILE, 3879);
        CGS_ASSERT_AT(leGameModeType < KI_GAME_MODE_COUNT,
                      "leGameModeType < GsmIO::E_MODE_COUNT", KAC_BRN_ONLINE_GAME_ROOM_PLAYER_INFO_ASSERT_FILE, 3880);

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "RG :: Starting an overlay for game mode : " << leGameModeType << "\n";
        }

        const char* lpcSplashScreenID = GetSplashScreenIDForGameMode(
            static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(leGameModeType));
        CGS_ASSERT_AT(lpcSplashScreenID != 0, "lpcSplashScreenID != NULL",
                      KAC_BRN_ONLINE_GAME_ROOM_PLAYER_INFO_ASSERT_FILE, 3888);

        GuiOverlayRequestWire lWire;
        lWire.mRequest.Construct(lpcSplashScreenID);
        PostOverlayRequest(mpStateInterface, lWire);

        mbShowSplashScreen = false;
    }

    // ------------------------------------------------------------ HandleControllerInputPauseSubState
    // The pause menu: move the highlight, perform the highlighted option (the security
    // option keeps the pause page loaded), or return to driving.
    void OnlineGameRoomPlayerInfo::HandleControllerInputPauseSubState(const CgsModule::Event* lpEvent)
    {
        // The header-stripped GuiEventControllerInputPressed payload: pad id, action id.
        const s32 liAction = reinterpret_cast<const s32*>(lpEvent)[1];

        switch (liAction)
        {
            case KI_ACTION_MENU_PREVIOUS:
                mPauseComponent.HighlightPrevious();
                break;

            case KI_ACTION_MENU_NEXT:
                mPauseComponent.HighlightNext();
                break;

            case KI_ACTION_SELECT:
                if (static_cast<s32>(mPauseComponent.GetHighlightedId()) != KI_PAUSE_OPTION_SECURITY)
                {
                    UnloadPauseResources();
                }
                PerformPauseOption(static_cast<s32>(mPauseComponent.GetHighlightedId()));
                PostLeavingGameFailureAudioTrigger(mpStateInterface, KI_AUDIO_ACTION_MENU_ITEM, "B5MenuItem");
                break;

            case KI_ACTION_BACK:
                PostLeavingGameFailureAudioTrigger(mpStateInterface, KI_AUDIO_ACTION_MENU_CUE, "GO_BACK");
                UnloadPauseResources();
                HideMenusAndReturnToHUD();
                break;

            case KI_ACTION_PAUSE:
                UnloadPauseResources();
                HideMenusAndReturnToHUD();
                break;

            default:
                break;
        }
    }

    // ------------------------------------------------------------ HandleControllerInputPressedMapSubState
    // The map page's presses: flip the road panel's score list, back out (to the pause
    // page or to driving), show a gamercard for the selected friend / rival, or pull the
    // map all the way out.
    void OnlineGameRoomPlayerInfo::HandleControllerInputPressedMapSubState(const CgsModule::Event* lpEvent)
    {
        const s32 liAction = reinterpret_cast<const s32*>(lpEvent)[1];

        switch (liAction)
        {
            case KI_ACTION_SELECT:
                if (mCrashNavPanel.GetPanelActiveFilterMode() == CrashNavPanel::E_PANEL_ROADSIGN)
                {
                    if (mCrashNavPanel.ToggleRoadPanelScores())
                    {
                        CrashNavMap::UpdateButtonPrompts();
                    }
                }
                break;

            case KI_ACTION_BACK:
                PostLeavingGameFailureAudioTrigger(mpStateInterface, KI_AUDIO_ACTION_MENU_CUE, "GO_BACK");
                if (mbShownFromPause)
                {
                    CrashNavMap::OnLeave();
                    UnloadMapResources();
                    ShowPauseScreen();
                }
                else
                {
                    UnloadMapResources();
                    HideMenusAndReturnToHUD();
                }
                break;

            case KI_ACTION_GAMERCARD:
                if (mCrashNavPanel.GetPanelActiveFilterMode() == CrashNavPanel::E_PANEL_ROADSIGN)
                {
                    if (mpLockedIconName != 0 &&
                        mCrashNavPanel.GetRoadPanelScoreMode() == KI_ROAD_PANEL_SCORE_MODE_FRIENDS &&
                        mCrashNavPanel.IsRoadRuleFriendSelected())
                    {
                        PostGamercardRequest(mpStateInterface,
                                             mCrashNavPanel.GetRoadRuleFriendSelectedName());
                    }
                }
                else if (mCrashNavPanel.GetPanelActiveFilterMode() == CrashNavPanel::E_PANEL_RIVALS)
                {
                    if (mPlayerName.GetPlayerName()[0] != '\0')
                    {
                        PostGamercardRequest(mpStateInterface, mPlayerName.GetPlayerName());
                    }
                }
                break;

            case KI_ACTION_ZOOM_OUT:
                mMainMapComponent.SetZoom(MainMapComponent::E_ZOOMFACTOR_CUSTOM, KF_MAP_ZOOM_OUT, false);
                mMainMapComponent.SetDesiredWorldCentre(KV2_ZOOMED_OUT_MAP_CENTRE());
                meCursorMode = E_CURSORMODE_ZOOMEDOUT;
                break;

            default:
                break;
        }
    }

    // ------------------------------------------------------------ HandleSettingChanged
    // Copy the highlighted row's new option into the options model; the two volume rows
    // publish the new volumes straight away. Every recognised row marks the edit as
    // pending, which is what arms the accept prompt and the cancel-confirm overlay.
    void OnlineGameRoomPlayerInfo::HandleSettingChanged()
    {
        const s32 liRow = mSettingToggle.GetHighlightedIndex();
        MenuToggle* lpToggle = mSettingToggle.GetSelectable(liRow);
        const u64 luSelectedId = lpToggle->mItemText.GetHighlightedId();

        switch (liRow)
        {
            case KI_SETTING_ROW_CAMERA:
                mCrashNavOptionsData.SetCameraUserOptions(static_cast<s32>(luSelectedId));
                mbSettingsChanged = true;
                break;

            case KI_SETTING_ROW_MUSIC:
                mCrashNavOptionsData.SetMusicVolume(
                    static_cast<CrashNavOptionsData::EOptionsSoundVolumes>(luSelectedId));
                UpdateSoundSettings();
                mbSettingsChanged = true;
                break;

            case KI_SETTING_ROW_SFX:
                mCrashNavOptionsData.SetSFXVolume(
                    static_cast<CrashNavOptionsData::EOptionsSoundVolumes>(luSelectedId));
                UpdateSoundSettings();
                mbSettingsChanged = true;
                break;

            case KI_SETTING_ROW_TIPS:
                mbSettingsChanged = true;
                if (luSelectedId == KU_TIPS_OPTION_ON)
                {
                    mCrashNavOptionsData.SetTips(true);
                }
                else if (luSelectedId == KU_TIPS_OPTION_OFF)
                {
                    mCrashNavOptionsData.SetTips(false);
                }
                else
                {
                    // The console streams the id after the text; lowered to the static text.
                    CGS_FAIL("Invalid ID for selected option \n");
                }
                break;

            default:
                // The console streams the row after the text; lowered to the static text.
                CGS_FAIL("Unknown option ");
                break;
        }
    }
}

// FOLDED FROM BrnOnlineGameRoomPlayerInfo_wH_15.cpp
// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-H partfile 15: the lobby player-list
// refresh and the two resource teardowns that drop the apt movie behind it.
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   HandlePlayerLobbyListEvent @ 0x824AD560 (BrnOnlineGameRoomPlayerInfo.cpp:2467)
//   UnloadLobbyResources       @ 0x8249A090 (assert fires at cpp:4135)
//   UnloadMapResources         @ 0x8249A280 (assert fires at cpp:4182)
//
// The class shape, the member map and the rodata tables live in the wave-H keystone
// scaffold BrnOnlineGameRoomPlayerInfo.{h,cpp}; the sibling bodies land in the other
// BrnOnlineGameRoomPlayerInfo_wH_XX.cpp partfiles.
//
// X360 offsets appear only in comments -- the host layout is name-based. Note in
// particular that NONE of the console's baked-in words are reproduced as literals:
//   * the 56-byte lobby-row stride (X360 `mulli r11, r11, 0x38` / `addi r11, r11, 0x38`)
//     is the sizeof of BrnNetwork::BrnNetworkModuleIO::LobbyPlayerStatusData, so the
//     payload view below indexes a real array and the host stride is whatever the host
//     record is;
//   * the +448 count offset is that array's end, i.e. the payload view's next member;
//
// HEADER-CLASH NOTE (spec's idiom section; BrnInGame.cpp precedent): the lobby
// player-list event's typed home is BrnGuiDemangledEventTypes.h, which HARD-COLLIDES
// with BrnGuiEventTypeDefs.h (pulled in transitively by the class header via
// BrnGuiCache.h). The payload therefore gets a file-local view -- and the queue hands
// out HEADER-STRIPPED payloads anyway, so a typed event pointer would mislead every
// field offset by 12.
// ===================================================================================

static const char KAC_ARTIST_FILE[] = "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.cpp";

namespace BrnGui
{
    namespace
    {
        // The apt clip the player-options popup transitions through (X360
        // aAptTransition_1, the AddOutputAptViewState first argument at 0x824AD654).

        // The empty movie name PlayAptMovie is handed when a screen's apt movie is
        // unmounted (X360 unk_820046A7, the zero-length rodata string).



    }

    // -------------------------------------------- HandlePlayerLobbyListEvent @ 0x824AD560
    // The network module republished the lobby roster. Work out which player the name
    // menu is currently sitting on (only meaningful while the roster is non-empty), then
    // re-drive the list -- but only from the sub-states that actually own the list: every
    // loading rung, the free-burn page, the controller-disable delay, the pause page and
    // the security page ignore the refresh outright, and the leaving page is already on
    // its way out.
    //
    // The player-options popup is the special case: it is pinned to one player, so the
    // roster is searched for that player. If they are gone from the new roster the popup
    // has lost its target -- redraw the list around the (now departed) selection, fade
    // the options animation out through its apt transition, and fall back to the main
    // page with no selection. If they are still there the popup stays as it is.
    void OnlineGameRoomPlayerInfo::HandlePlayerLobbyListEvent(const CgsModule::Event* lpEvent)
    {
        const GuiEventNetworkLobbyPlayerList* lpPlayerList =
            reinterpret_cast<const GuiEventNetworkLobbyPlayerList*>(lpEvent);

        s32 liCurrentPlayerID = -1;
        if (lpPlayerList->miNumberOfPlayers > 0)
        {
            // mPlayerNameComponent.miHighlightedIndex is the SelectableGroup's signed
            // highlighted-row byte (-1 == nothing highlighted); the X360 sign-extends it
            // before the compare.
            const s32 liHighlightedIndex = mPlayerNameComponent.miHighlightedIndex;
            if (liHighlightedIndex > -1)
                liCurrentPlayerID = lpPlayerList->maPlayerLobbyInfo[liHighlightedIndex].mPlayerID;
        }

        if (meSubState != E_SUBSTATE_LOADING_SCREEN
            && meSubState != E_SUBSTATE_LOADING_COMPONENTS
            && meSubState != E_SUBSTATE_LOADING_PAUSE_SCREEN
            && meSubState != E_SUBSTATE_LOADING_PAUSE_COMPONENTS
            && meSubState != E_SUBSTATE_FREE_BURNING
            && meSubState != E_SUBSTATE_CONTROLLER_DISABLE_DELAY
            && meSubState != E_SUBSTATE_PAUSE
            && meSubState != E_SUBSTATE_SECURITY_OPTIONS)
        {
            if (meSubState == E_SUBSTATE_PLAYER_OPTIONS)
            {
                s32 liPlayerIndex = 0;
                for (; liPlayerIndex < lpPlayerList->miNumberOfPlayers; ++liPlayerIndex)
                {
                    if (lpPlayerList->maPlayerLobbyInfo[liPlayerIndex].mPlayerID == mSelectedPlayerID)
                        break;
                }

                if (liPlayerIndex == lpPlayerList->miNumberOfPlayers)
                {
                    ShowPlayerList(mSelectedPlayerID);
                    mOptionsAnimation.AddOutputAptViewState("apt_Transition",
                                                            KAPC_ANIMATION_STATES[1], false);
                    meSubState        = E_SUBSTATE_MAIN;
                    mSelectedPlayerID = -1;
                }
            }
            else if (meSubState != E_SUBSTATE_LEAVING)
            {
                ShowPlayerList(liCurrentPlayerID);
            }
        }
    }

    // ------------------------------------------------- UnloadLobbyResources @ 0x8249A090
    // Tear the lobby page down: destruct the two panels that own apt components, unmount
    // the lobby apt movie (the inlined CgsGui::GuiEventPlayAptMovie record { 8, 18, 12,
    // "", 3 } on the view-state channel), then hand the lobby resource tuples back to the
    // cache.
    void OnlineGameRoomPlayerInfo::UnloadLobbyResources()
    {
        mPlayerStatsDisplay.Destruct();
        mRouteInfoDisplay.Destruct();

        CgsGui::GuiEventPlayAptMovie lMovie;
        lMovie.mpacMovieName = "";
        lMovie.miLevelNum = KI_APT_MOVIE_LEVEL;
        mpStateInterface->OutputViewState(lMovie);

        // X360 BrnOnlineGameRoomPlayerInfo.cpp:4135.
        CGS_ASSERT_AT(mpGuiCache != 0, "mpGuiCache",
                      KAC_ARTIST_FILE, 4135);

        // Re-read (not the asserted copy): the X360 reloads the member and re-tests it.
        if (mpGuiCache != 0)
        {
            mpGuiCache->UnloadResources(maLobbyResourceTuplesToLoad,
                                        sizeof(maLobbyResourceTuplesToLoad) / sizeof(maLobbyResourceTuplesToLoad[0]));
        }
    }

    // --------------------------------------------------- UnloadMapResources @ 0x8249A280
    // Tear the map page down: unmount its apt movie the same way, then hand back the
    // BASE class's map resource tuples -- CrashNavMap owns that table (protected static),
    // and this screen unloads it because the map page IS the base class.
    void OnlineGameRoomPlayerInfo::UnloadMapResources()
    {
        CgsGui::GuiEventPlayAptMovie lMovie;
        lMovie.mpacMovieName = "";
        lMovie.miLevelNum = KI_APT_MOVIE_LEVEL;
        mpStateInterface->OutputViewState(lMovie);

        // X360 BrnOnlineGameRoomPlayerInfo.cpp:4182.
        CGS_ASSERT_AT(mpGuiCache != 0, "mpGuiCache",
                      KAC_ARTIST_FILE, 4182);

        if (mpGuiCache != 0)
        {
            mpGuiCache->UnloadResources(CrashNavMap::maResourcesToLoad,
                                        CrashNavMap::muNumResourcesToLoad);
        }
    }
}

#undef CGS_ARTIST_FILE
