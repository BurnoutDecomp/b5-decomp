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
//   cache+0xA800 -> maOnlineGameModeOptionsStorage (the GuiEventNetworkGameParams
//                   payload mirror; the X360 forms it as `cache + 0x10000 - 0x5800`)
//   cache+0xA9D0 -> miOnlineNumRounds        cache+0xAC74 -> muNumActivePlayers
//   cache+0xB650 -> maLobbyPlayerInfo[i].mPlayerID (row stride 56 == the committed
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

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface::OutputGuiEvent
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiAudioTriggerEvent
#include "GameSource/Gui/Events/BrnGuiEventNetworkGameParams.h"           // GuiNetworkRouteInfo::SetInfo arg
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h"      // InGamePlayerStatusData::mbMarkedMan
#include "GameSource/Network/SharedIO/BrnNetworkModuleOnlineLobbyPlayerStatusInterface.h" // LobbyPlayerStatusData::mPlayerID

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
        static_assert(sizeof(GuiEventNetworkSelectedPlayerOptionPayload) == 8,
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
        static_assert(sizeof(GuiEventNetworkOutputPlayerTexturePayload) == 8,
                      "event 264 payload is 8 bytes");

        // GuiAudioTriggerEvent::Construct's action argument at every site in this TU.
        const s32 KI_AUDIO_TRIGGER_ACTION = 7;

        // The apt clip the options popup transitions (the X360 literal at every
        // AddOutputAptViewState site in this file).
        const char KAC_APT_TRANSITION[] = "apt_Transition";

        // The audio labels the two sites trigger.
        const char KAC_SOUND_MENU_ITEM_TOGGLE_DEFAULT[] = "MenuItemToggleDefault";
        const char KAC_SOUND_GO_BACK[]                  = "GO_BACK";
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
                                         KAC_SOUND_MENU_ITEM_TOGGLE_DEFAULT, "");
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
                                         KAC_SOUND_MENU_ITEM_TOGGLE_DEFAULT, "");
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
                        reinterpret_cast<const GuiEventNetworkGameParams*>(
                            mpGuiCache->maOnlineGameModeOptionsStorage));
                }
                break;

            case ',':   // 0x2C -- scroll the displayed round UP, clamped to the match's
                        //         round count (and then back up to zero).
                if (!mPlayerStatsDisplay.IsVisible())
                {
                    ++miCurrentRoundDisplayed;
                    if (miCurrentRoundDisplayed >= mpGuiCache->miOnlineNumRounds - 1)
                    {
                        miCurrentRoundDisplayed = mpGuiCache->miOnlineNumRounds - 1;
                    }
                    if (miCurrentRoundDisplayed <= 0)
                    {
                        miCurrentRoundDisplayed = 0;
                    }

                    mRouteInfoDisplay.SetInfo(
                        miCurrentRoundDisplayed,
                        reinterpret_cast<const GuiEventNetworkGameParams*>(
                            mpGuiCache->maOnlineGameModeOptionsStorage));
                }
                break;

            case '1':   // 0x31 -- open the highlighted player's options popup.
                // The X360 compares the active-player count SIGNED (cmpwi/ble).
                if (static_cast<s32>(mpGuiCache->muNumActivePlayers) > 0)
                {
                    const BrnNetwork::BrnNetworkModuleIO::LobbyPlayerStatusData* lpLobbyPlayer =
                        reinterpret_cast<const BrnNetwork::BrnNetworkModuleIO::LobbyPlayerStatusData*>(
                            mpGuiCache->maLobbyPlayerInfo[mPlayerNameComponent.miHighlightedIndex]);

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
                lAudio.Construct(KI_AUDIO_TRIGGER_ACTION, "", KAC_SOUND_GO_BACK, "");
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
                    mpStateInterface->OutputGuiEvent<GuiEventNetworkOutputPlayerTexturePayload>(
                        lPlayerTexture);
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

        if (mpGuiCache->GetOnlinePlayerInfo(mPlayerNameComponent.miHighlightedIndex)->mbMarkedMan)
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
            mOptionComponent.DisableSelectable(liOption);   // component slot 9
        }

        mOptionComponent.SetIds(lau64OptionIds);
        mOptionsAnimation.AddOutputAptViewState(KAC_APT_TRANSITION, KAPC_ANIMATION_STATES[0], false);

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
                mpStateInterface->OutputGuiEvent<GuiEventNetworkSelectedPlayerOptionPayload>(
                    lSelectedOption);

                mOptionsAnimation.AddOutputAptViewState(KAC_APT_TRANSITION,
                                                        KAPC_ANIMATION_STATES[1], false);
                mSelectedPlayerID = -1;
                meSubState = E_SUBSTATE_MAIN;
                break;
            }

            case '2':   // 0x32 -- cancel: play the back sound, then hide.
            {
                GuiAudioTriggerEvent lAudio;
                lAudio.Construct(KI_AUDIO_TRIGGER_ACTION, "", KAC_SOUND_GO_BACK, "");
                mpStateInterface->OutputGuiEvent<GuiAudioTriggerEvent>(lAudio);

                mOptionsAnimation.AddOutputAptViewState(KAC_APT_TRANSITION,
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
