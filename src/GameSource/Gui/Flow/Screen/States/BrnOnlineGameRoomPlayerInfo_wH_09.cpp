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
        const s32 KI_CHANNEL_VIEW_STATE   = 41;   // GuiOutViewState
        const s32 KI_CHANNEL_GUI_INTERNAL = 42;   // internal/HUD-component channel

        // ---- controller actions this sub-state answers (event-6 payload word +4) ----
        const s32 KI_ACTION_ROUND_PREVIOUS = 0x2B;   // '+'
        const s32 KI_ACTION_ROUND_NEXT     = 0x2C;   // ','
        const s32 KI_ACTION_SELECT         = 0x31;   // '1'
        const s32 KI_ACTION_BACK           = 0x32;   // '2'

        // The GuiAudioTriggerEvent action the back press fires (X360 `li r4, 7`).
        const s32 KI_AUDIO_ACTION_BACK = 7;

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

            GuiEventShowHideSatNavPayload(s32 liSubMode, f32 lfValue, bool lbFlag)
                : miSubMode(liSubMode), mfValue(lfValue), mbFlag(lbFlag)
            {
                maPad[0] = maPad[1] = maPad[2] = 0;
            }

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
                    (mpGuiCache->meOnlineGameMode
                         == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE) ? 2 : 1;
                miCurrentRoundDisplayed -= liStep;
                mRouteInfoDisplay.SetInfo(
                    miCurrentRoundDisplayed,
                    reinterpret_cast<const GuiEventNetworkGameParams*>(
                        mpGuiCache->maOnlineGameModeOptionsStorage));
            }
            break;

        case KI_ACTION_ROUND_NEXT:
        {
            const s32 liStep =
                (mpGuiCache->meOnlineGameMode
                     == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE) ? 2 : 1;
            if (miCurrentRoundDisplayed < mpGuiCache->miOnlineNumRounds - liStep)
            {
                miCurrentRoundDisplayed += liStep;
                mRouteInfoDisplay.SetInfo(
                    miCurrentRoundDisplayed,
                    reinterpret_cast<const GuiEventNetworkGameParams*>(
                        mpGuiCache->maOnlineGameModeOptionsStorage));
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
            GuiAudioTriggerEvent lAudioEvent;
            lAudioEvent.Construct(KI_AUDIO_ACTION_BACK, "", "GO_BACK", "");
            mpStateInterface->OutputGuiEvent(lAudioEvent);
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
        if (miCurrentRoundDisplayed >= mpGuiCache->miOnlineNumRounds - 1)
            miCurrentRoundDisplayed = mpGuiCache->miOnlineNumRounds - 1;
        if (miCurrentRoundDisplayed <= 0)
            miCurrentRoundDisplayed = 0;

        // The X360 loads the count from .data (dword_8204C7A0 == 11) and stores it into
        // the ROUTE-INFO panel's loaded-component counter (abs +84304 == the panel at
        // +75696 plus its +8608 member), not into a screen member.
        mRouteInfoDisplay.miNumComponentsLoaded = GuiNetworkRouteInfo::KI_NUM_COMPONENTS_TO_LOAD;

        mRouteInfoDisplay.SetState(GuiNetworkRouteInfo::E_STATE_VISIBLE);
        mRouteInfoDisplay.SetInfo(
            miCurrentRoundDisplayed,
            reinterpret_cast<const GuiEventNetworkGameParams*>(
                mpGuiCache->maOnlineGameModeOptionsStorage));

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

        GuiEventShowHideSatNavPayload lShowHideSatNav(0, 0.0f, true);

        CgsGui::GuiEventWrapper<GuiEventShowHideSatNavPayload, KI_CHANNEL_VIEW_STATE>
            lViewStateRecord(lShowHideSatNav);
        lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lViewStateRecord),
                             lViewStateRecord.GetChannel(),
                             static_cast<s32>(sizeof(lViewStateRecord)));

        CgsGui::GuiEventWrapper<GuiEventShowHideSatNavPayload, KI_CHANNEL_GUI_INTERNAL>
            lInternalStateRecord(lShowHideSatNav);
        lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lInternalStateRecord),
                             lInternalStateRecord.GetChannel(),
                             static_cast<s32>(sizeof(lInternalStateRecord)));
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
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event sent to OnlineGameRoomPlayerInfo::HandleCreateMatchEventEvent");

        if (mpGuiCache != 0)
        {
            // The whole 480-byte payload lands on the cache's params mirror (its
            // maOnlineGameModeOptionsStorage round table plus the named scalar tail
            // that follows it -- X360 cache+0xA800..+0xA9DF). The record is
            // pointer-free, so the host block is the same 480 bytes; the console's
            // literal 0x1E0 is written as the sizeof it came from.
            std::memcpy(mpGuiCache->maOnlineGameModeOptionsStorage, lpEvent,
                        sizeof(GuiEventNetworkGameParams));

            if (mRouteInfoDisplay.IsComponentLoaded())
            {
                // Same two-store clamp as ShowViewEventOptions.
                if (miCurrentRoundDisplayed >= mpGuiCache->miOnlineNumRounds - 1)
                    miCurrentRoundDisplayed = mpGuiCache->miOnlineNumRounds - 1;
                if (miCurrentRoundDisplayed <= 0)
                    miCurrentRoundDisplayed = 0;

                mRouteInfoDisplay.SetInfo(
                    miCurrentRoundDisplayed,
                    reinterpret_cast<const GuiEventNetworkGameParams*>(
                        mpGuiCache->maOnlineGameModeOptionsStorage));

                // X360: the ==15 / ==16 pair, i.e. the inlined IsOnlineFreeBurnLobby.
                if (BrnGameState::GameStateModuleIO::IsOnlineFreeBurnLobby(
                        static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(
                            mpGuiCache->meOnlineGameMode))
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
