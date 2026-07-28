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

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
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
        const s32 KI_CHANNEL_GUI_OUT      = 40;   // GuiEventOut
        const s32 KI_CHANNEL_VIEW_STATE   = 41;   // GuiOutViewState
        const s32 KI_CHANNEL_GUI_INTERNAL = 42;   // internal/HUD-component channel

        // The apt-component name buffer the OnEnter icon loop formats into. X360:
        // CgsCore::SPrintf(lacComponentName, 32, "<fmt>", liPlayer).
        const u32 KU_ICON_COMPONENT_NAME_LEN = 32;
    }

    // ------------------------------------------------ Construct @ 0x82484B30
    // Chain the map base, then clear the two map-presentation options this screen
    // overrides and the first-map-update latch.
    void OnlineGameRoomPlayerInfo::Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm)
    {
        CGS_ASSERT(lpFsm != 0, "lpFsm");   // X360 BrnOnlineGameRoomPlayerInfo.cpp:338

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

        mpGuiCache                 = 0;       // X360 +87516 (OUR cache ptr, not the base's)
        mSelectedPlayerID          = -1;      // X360 +87512
        mfTimeToDisableNextEATrack = 0.0f;    // X360 +87524 (flt_82001CC0)
        miCurrentRoundDisplayed    = 0;       // X360 +87508
        mbShownFromPause           = false;   // X360 +87536
        mfTimeUntilNextEATrack     = 0.0f;    // X360 +87520 (flt_82001CC0)
        mbKicked                   = false;   // X360 +87535
        mbNeedToClearSplashScreen  = false;   // X360 +87534
        mbHighlightedNewPlayer     = false;   // X360 +87537
        mbShowSplashScreen         = false;   // X360 +87533

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
