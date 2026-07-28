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
        const s32 KI_CHANNEL_GUI_OUT = 40;   // GuiEventOut

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
        const char KAC_HOST_LEAVE_OVERLAY[]           = "OnHostLvQn";
        const char KAC_LEAVE_CHALLENGE_OVERLAY[]      = "CNOnlLvChaQn";
        const char KAC_LEAVE_GAME_OVERLAY[]           = "CNOnlLvGmQn";

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
            u8 maPad[3];   // +0x0D

            GuiCommandEvent16() : CgsGui::GuiEvent<N>(1, 12), mu8Flag(0)
            {
                maPad[0] = maPad[1] = maPad[2] = 0;
            }
        };

        template <s32 N>
        void PostCommand16(CgsGui::StateInterface* lpStateInterface, s32 liChannel)
        {
            GuiCommandEvent16<N> lEvent;
            lpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent), liChannel,
                static_cast<s32>(sizeof(lEvent)));   // X360 record size 16
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
        typedef CgsGui::GuiEventWrapper<GuiEventNetworkGameParams, KI_CHANNEL_GUI_OUT>
            GuiEventNetworkGameParamsWire;

        void OutputNetworkGameParams(CgsGui::StateInterface* lpStateInterface,
                                     GuiEventNetworkGameParams& lrParams)
        {
            GuiEventNetworkGameParamsWire lWire(lrParams);
            lpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lWire), lWire.GetChannel(),
                static_cast<s32>(sizeof(lWire)));   // X360 record size 492
        }

        // The OutputGuiEvent<BrnGui::GuiOverlayRequest> wire record:
        // { 288, 184, 16, <pad>, the 288-byte request }, channel 40, 304 bytes -- the same
        // record BrnInGame.cpp's PostOverlayRequest builds. The two header words are HOST
        // expressions (sizeof / the 16-aligned payload offset), not the console immediates.
        struct GuiOverlayRequestWire : public CgsGui::GuiEvent<184>
        {
            u32               muPad0C;    // +0x0C (payload is 16-aligned past the header)
            GuiOverlayRequest mRequest;   // +0x10

            GuiOverlayRequestWire()
                : CgsGui::GuiEvent<184>(static_cast<u32>(sizeof(GuiOverlayRequest)), 16)
                , muPad0C(0)
            {
            }
        };
    }

    // ---- PerformPauseOption @0x824A5F70 -------------------------------------------
    // DWARF cpp:3956. Carry out the pause-menu action the player picked.
    void OnlineGameRoomPlayerInfo::PerformPauseOption(s32 lePauseOption)
    {
        switch (lePauseOption)
        {
        case KI_PAUSE_OPTION_LAUNCH:
            // cpp:3987. Inlined by the X360 into the 15/16 compare pair at 0x824A5FF4.
            CGS_ASSERT(!BrnGameState::GameStateModuleIO::IsOnlineFreeBurnLobby(
                           static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(
                               mpGuiCache->meOnlineGameMode)),
                       "!GsmIO::IsOnlineFreeBurnLobby(mpGuiCache->GetOnlineGameModeOptions()->meGameMode)");

            PostCommand16<KI_EVENT_LAUNCH_ONLINE_GAME>(mpStateInterface, KI_CHANNEL_GUI_OUT);
            HideMenusAndReturnToHUD();
            break;

        case KI_PAUSE_OPTION_CREATE_EVENT:
            SendStateEvent("TO_GAME_OPT");
            break;

        case KI_PAUSE_OPTION_CHANGE_TEAMS:
            if (mpGuiCache->meOnlineGameMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE)
            {
                PostCommand16<KI_EVENT_CHANGE_TEAMS>(mpStateInterface, KI_CHANNEL_GUI_OUT);
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
                       reinterpret_cast<const GuiEventNetworkGameParams*>(
                           &mpGuiCache->maOnlineGameModeOptionsStorage[0]),
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
                       reinterpret_cast<const GuiEventNetworkGameParams*>(
                           &mpGuiCache->maOnlineGameModeOptionsStorage[0]),
                       sizeof(lParams));

                lParams.meSecurity = KI_GAME_SECURITY_ALLCOMERS;
                OutputNetworkGameParams(mpStateInterface, lParams);
            }
            break;

        case KI_PAUSE_OPTION_SECURITY_FRIENDS_ONLY:
            {
                GuiEventNetworkGameParams lParams;
                memcpy(&lParams,
                       reinterpret_cast<const GuiEventNetworkGameParams*>(
                           &mpGuiCache->maOnlineGameModeOptionsStorage[0]),
                       sizeof(lParams));

                lParams.meSecurity = KI_GAME_SECURITY_FRIENDS_ONLY;
                OutputNetworkGameParams(mpStateInterface, lParams);
            }
            break;

        case KI_PAUSE_OPTION_SECURITY_INVITE_ONLY:
            {
                GuiEventNetworkGameParams lParams;
                memcpy(&lParams,
                       reinterpret_cast<const GuiEventNetworkGameParams*>(
                           &mpGuiCache->maOnlineGameModeOptionsStorage[0]),
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
            CGS_ASSERT(false, "Invalid game action to perform\n");   // cpp:4112
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
            lpacOverlayId = KAC_HOST_LEAVE_OVERLAY;
        }
        else
        {
            CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");                                   // cpp:2869
            CGS_ASSERT(mpGuiCache->GetFreeburnChallengeManager() != 0,
                       "mpGuiCache->GetFreeburnChallengeManager()");                     // cpp:2870

            // The accessor's own "mpChallengeManager" assert (BrnGuiCache.h:2390) is what
            // the X360 shows inlined before each of these two uses.
            if (mpGuiCache->GetFreeburnChallengeManager()->IsRunning())
            {
                lpacOverlayId = KAC_LEAVE_CHALLENGE_OVERLAY;
            }
            else
            {
                lpacOverlayId = KAC_LEAVE_GAME_OVERLAY;
            }
        }

        // The X360 constructs the request into a scratch stack slot and memcpy's the 288
        // bytes into the wire record (the inlined OutputGuiEvent<GuiOverlayRequest> copy);
        // constructed in place here, the same way BrnInGame.cpp posts its overlays.
        GuiOverlayRequestWire lWire;
        lWire.mRequest.Construct(lpacOverlayId);

        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lWire), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(GuiOverlayRequestWire)));   // X360 record size 304
    }

    // ---- HandleLaunchQuestion @0x82498038 -----------------------------------------
    // DWARF cpp:1587. The answer to the "launch the event?" overlay: OK posts the accept
    // command, CANCEL restores the tab title the question overwrote.
    void OnlineGameRoomPlayerInfo::HandleLaunchQuestion(s32 liLeaveMethod)
    {
        if (liLeaveMethod == GuiOverlayCompleteEvent::E_LEAVEMETHOD_OK)
        {
            PostCommand16<KI_EVENT_LAUNCH_QUESTION_ACCEPT>(mpStateInterface, KI_CHANNEL_GUI_OUT);
        }
        else if (liLeaveMethod == GuiOverlayCompleteEvent::E_LEAVEMETHOD_CANCEL)
        {
            // The X360 loads mPlayerStatsDisplay.meState (this+75684) and, on the
            // non-visible side, mRouteInfoDisplay.meState (this+84312).
            if (mPlayerStatsDisplay.IsVisible())
            {
                mMessageText.SetText(KAC_PLAYER_INFO_STRING_ID);
            }
            else if (mRouteInfoDisplay.meState != GuiNetworkRouteInfo::E_STATE_VISIBLE)
            {
                mMessageText.SetText(KAC_EVENT_MAP_STRING_ID);
            }
            else
            {
                mMessageText.SetText(KAC_EVENT_INFO_STRING_ID);
            }
        }
    }
}
