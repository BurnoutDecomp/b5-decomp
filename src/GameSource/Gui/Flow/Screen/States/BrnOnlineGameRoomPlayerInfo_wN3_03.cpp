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

namespace BrnGui
{
    namespace
    {
        const s32 KI_CHANNEL_GUI_OUT = 40;   // the GUI-out channel every record here uses

        // ---- controller actions (event-6 payload word +4) -------------------------------
        const s32 KI_ACTION_MENU_PREVIOUS = 0x29;   // ')'
        const s32 KI_ACTION_MENU_NEXT     = 0x2A;   // '*'
        const s32 KI_ACTION_PAUSE         = 0x2D;   // '-' resume from the pause menu
        const s32 KI_ACTION_SELECT        = 0x31;   // '1'
        const s32 KI_ACTION_BACK          = 0x32;   // '2'
        const s32 KI_ACTION_GAMERCARD     = 0x33;   // '3'
        const s32 KI_ACTION_ZOOM_OUT      = 0x38;   // '8' (released by the map-release handler)

        // The pause option that stays on the pause page (the security menu reuses the
        // pause component, so its resources are not unloaded).
        const s32 KI_PAUSE_OPTION_SECURITY = 9;

        // The audio cues: action 4 "B5MenuItem" on select, action 7 "GO_BACK" on back.
        const s32  KI_AUDIO_ACTION_MENU_ITEM = 4;
        const s32  KI_AUDIO_ACTION_MENU_CUE  = 7;
        const char KAC_SOUND_MENU_ITEM[]     = "B5MenuItem";
        const char KAC_SOUND_GO_BACK[]       = "GO_BACK";
        const char KAC_EMPTY_STRING[]        = "";

        // The map pull-back the zoom-out press asks for (console float constant 9000.0f)
        // and the world centre it recentres on: the 16-byte global the console loads is
        // filled at start-up by a dynamic initialiser with { -460.0f, -915.0f, 0, 0 }.
        const f32 KF_MAP_ZOOM_OUT = 9000.0f;

        Vector2 KV2_ZOOMED_OUT_MAP_CENTRE()
        {
            Vector2 lv2Centre;
            lv2Centre.x = -460.0f;
            lv2Centre.y = -915.0f;
            lv2Centre.z = 0.0f;
            lv2Centre.w = 0.0f;
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
        const char KAC_OVERLAY_LEAVE_GAME_QN[]    = "CNOnlLvGmQn";
        const char KAC_OVERLAY_LEAVE_CHAL_QN[]    = "CNOnlLvChaQn";
        const char KAC_OVERLAY_LEAVING_GAME[]     = "CNOnlLvgGame";
        const char KAC_OVERLAY_HOST_LAUNCHING[]   = "CNOnlLchGmH";
        const char KAC_OVERLAY_PEER_LAUNCHING[]   = "CNOnlLchGame";

        // The launch overlay's two message parameters: the launching player's name, then
        // the game mode's string id.
        const u32 KU_LAUNCH_PARAM_PLAYER_NAME = 1;
        const u32 KU_LAUNCH_PARAM_GAME_MODE   = 2;

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
        struct GuiOverlayRequestWire : public CgsGui::GuiEvent<184>
        {
            u32               muPad0C;
            GuiOverlayRequest mRequest;

            GuiOverlayRequestWire()
                : CgsGui::GuiEvent<184>(static_cast<u32>(sizeof(GuiOverlayRequest)), 16)
                , muPad0C(0)
            {
            }
        };

        // { 8, 188, 16, <pad>, the compressed overlay id }, 24 bytes.
        struct GuiOverlayWaitFinishWire : public CgsGui::GuiEvent<188>
        {
            u32                         muPad0C;
            GuiOverlayWaitFinishRequest mRequest;

            GuiOverlayWaitFinishWire()
                : CgsGui::GuiEvent<188>(static_cast<u32>(sizeof(GuiOverlayWaitFinishRequest)), 16)
                , muPad0C(0)
            {
            }
        };

        // The audio trigger's wire payload (the 100-byte body under id 457; see
        // BrnOnlineGameRoomPlayerInfo_wN3_02.cpp for why it is re-boxed).
        struct GuiAudioTriggerPayload457
        {
            char macComponent[32];
            s32  meAction;
            char macLabel[32];
            char macMovie[32];
            s32 GetEventType() const { return 457; }
        };

        static_assert(sizeof(GuiOverlayRequestWire) == 304, "overlay request record is 304 bytes");
        static_assert(sizeof(GuiOverlayWaitFinishWire) == 24, "overlay wait-finish record is 24 bytes");
        static_assert(sizeof(GuiAudioTriggerPayload457) == 100, "audio trigger payload is 100 bytes");
        static_assert(sizeof(GuiEventScoreboardRequestGamercardEvent) == 28, "gamercard record is 28 bytes");

        void PostOverlayRequest(CgsGui::StateInterface* lpStateInterface, GuiOverlayRequestWire& lrWire)
        {
            lpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lrWire), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lrWire)));
        }

        void PostOverlayWaitFinish(CgsGui::StateInterface* lpStateInterface, const char* lpacOverlayName)
        {
            GuiOverlayWaitFinishWire lWaitFinish;
            lWaitFinish.mRequest.Construct(lpacOverlayName);
            lpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lWaitFinish), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lWaitFinish)));
        }

        void PostAudioTrigger(CgsGui::StateInterface* lpStateInterface, s32 leAction,
                              const char* lpcLabel)
        {
            GuiAudioTriggerEvent lAudioTrigger;
            lAudioTrigger.Construct(leAction, KAC_EMPTY_STRING, lpcLabel, KAC_EMPTY_STRING);

            GuiAudioTriggerPayload457 lPayload;
            std::memcpy(&lPayload, lAudioTrigger.macComponent, sizeof(lPayload));

            CgsGui::GuiEventWrapper<GuiAudioTriggerPayload457, KI_CHANNEL_GUI_OUT> lRecord(lPayload);
            lpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lRecord), lRecord.GetChannel(),
                static_cast<s32>(sizeof(lRecord)));
        }

        // OutputGuiEvent<GuiEventScoreboardRequestGamercardEvent>: the committed type
        // carries its { 16, 120, 12 } header, so the event object is the record.
        void PostGamercardRequest(CgsGui::StateInterface* lpStateInterface, const char* lpcPlayerName)
        {
            GuiEventScoreboardRequestGamercardEvent lRequest;
            lRequest.mPlayerName.Construct(lpcPlayerName);
            lpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lRequest), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lRequest)));
        }
    }

    // ------------------------------------------------------------ HandleLeavingGameFailedEvent
    // The leave request failed: stop waiting on the "leaving game" overlay.
    void OnlineGameRoomPlayerInfo::HandleLeavingGameFailedEvent(const CgsModule::Event* lpEvent)
    {
        PostOverlayWaitFinish(mpStateInterface, KAC_OVERLAY_LEAVING_GAME);
    }

    // ------------------------------------------------------------ HandleLaunchingEvent
    // A game is launching: stop waiting on the leave questions and the leaving overlay,
    // then raise the launch overlay -- the host's own version, or the peers' version
    // naming the launching player -- with the mode's string id as its second parameter.
    void OnlineGameRoomPlayerInfo::HandleLaunchingEvent(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, "Invalid event sent to OnlineGameRoomPlayerInfo::HandleLaunchingEvent");
        CGS_ASSERT(lpEvent != 0, "lpLaunchingEvent");

        const GuiEventNetworkLaunchingPayload* lpLaunchingEvent =
            reinterpret_cast<const GuiEventNetworkLaunchingPayload*>(lpEvent);

        PostOverlayWaitFinish(mpStateInterface, KAC_OVERLAY_LEAVE_GAME_QN);
        PostOverlayWaitFinish(mpStateInterface, KAC_OVERLAY_LEAVE_CHAL_QN);
        PostOverlayWaitFinish(mpStateInterface, KAC_OVERLAY_LEAVING_GAME);

        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        GuiOverlayRequestWire lWire;
        if (mpGuiCache != 0 && mpGuiCache->IsLocalPlayerHost())
        {
            lWire.mRequest.Construct(KAC_OVERLAY_HOST_LAUNCHING);
        }
        else
        {
            const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerStatusData =
                mpGuiCache->GetOnlinePlayerInfoFromPlayerId(lpLaunchingEvent->mPlayerID);
            CGS_ASSERT(lpPlayerStatusData != 0, "lpPlayerStatusData");

            lWire.mRequest.Construct(KAC_OVERLAY_PEER_LAUNCHING);
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
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        const s32 leGameModeType = mpGuiCache->GetGameMode();
        CGS_ASSERT(leGameModeType > BrnGameState::GameStateModuleIO::E_MODE_NONE,
                   "leGameModeType > GsmIO::E_MODE_NONE");
        CGS_ASSERT(leGameModeType < KI_GAME_MODE_COUNT, "leGameModeType < GsmIO::E_MODE_COUNT");

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "RG :: Starting an overlay for game mode : ";
            *CgsDev::Log::gpDebugPrint << leGameModeType;
            *CgsDev::Log::gpDebugPrint << "\n";
        }

        const char* lpcSplashScreenID = GetSplashScreenIDForGameMode(
            static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(leGameModeType));
        CGS_ASSERT(lpcSplashScreenID != 0, "lpcSplashScreenID != NULL");

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
                PostAudioTrigger(mpStateInterface, KI_AUDIO_ACTION_MENU_ITEM, KAC_SOUND_MENU_ITEM);
                break;

            case KI_ACTION_BACK:
                PostAudioTrigger(mpStateInterface, KI_AUDIO_ACTION_MENU_CUE, KAC_SOUND_GO_BACK);
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
                PostAudioTrigger(mpStateInterface, KI_AUDIO_ACTION_MENU_CUE, KAC_SOUND_GO_BACK);
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
                    CGS_ASSERT(false, "Invalid ID for selected option \n");
                }
                break;

            default:
                // The console streams the row after the text; lowered to the static text.
                CGS_ASSERT(false, "Unknown option ");
                break;
        }
    }
}
