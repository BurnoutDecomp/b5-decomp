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
//   * the AddEvent record size 20 for the {8,18,12,"",3} play-apt-movie record is not
//     written here at all -- the record carries a pointer, so its host size differs.
//     The X360 emitted it inline; the faithful de-inlined form is the real method
//     CgsGui::StateInterface::PlayAptMovie(name, level), which owns that sizeof
//     (same de-inlining the committed siblings wH_01 / wH_07 / BrnCarSelectMain_wG_01 use).
//
// HEADER-CLASH NOTE (spec's idiom section; BrnInGame.cpp precedent): the lobby
// player-list event's typed home is BrnGuiDemangledEventTypes.h, which HARD-COLLIDES
// with BrnGuiEventTypeDefs.h (pulled in transitively by the class header via
// BrnGuiCache.h). The payload therefore gets a file-local view -- and the queue hands
// out HEADER-STRIPPED payloads anyway, so a typed event pointer would mislead every
// field offset by 12.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsModule::Event
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface::PlayAptMovie
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache::UnloadResources
#include "GameSource/Gui/Flow/Screen/Components/BrnGuiNetworkPlayerStats.h" // GuiNetworkPlayerStats::Destruct
#include "GameSource/Gui/Flow/Screen/Components/BrnGuiNetworkRouteInfo.h"   // GuiNetworkRouteInfo::Destruct
#include "GameSource/Network/SharedIO/BrnNetworkModuleOnlineLobbyPlayerStatusInterface.h" // LobbyPlayerStatusData

namespace BrnGui
{
    namespace
    {
        // The apt clip the player-options popup transitions through (X360
        // aAptTransition_1, the AddOutputAptViewState first argument at 0x824AD654).
        const char KAC_APT_TRANSITION[] = "apt_Transition";

        // The empty movie name PlayAptMovie is handed when a screen's apt movie is
        // unmounted (X360 unk_820046A7, the zero-length rodata string).
        const char KAC_EMPTY_MOVIE[] = "";

        // The apt movie level the screens mount/unmount on (X360 `li r11, 3`).
        const s32 KI_APT_MOVIE_LEVEL = 3;

        // ---- in-queue payload view (event 244, GuiEventNetworkLobbyPlayerList) -------
        // The queue delivers the HEADER-STRIPPED payload: the 8 lobby rows first, then
        // the live player count (X360 payload +0x1C0 == 8 * sizeof(LobbyPlayerStatusData)).
        // Row fields come from the record's real home,
        // BrnNetworkModuleOnlineLobbyPlayerStatusInterface.h -- no forked layout here.
        struct GuiEventNetworkLobbyPlayerListPayload
        {
            BrnNetwork::BrnNetworkModuleIO::LobbyPlayerStatusData
                maPlayers[OnlineGameRoomPlayerInfo::KI_MAX_PLAYERS];   // +0x000
            s32 miNumPlayers;                                          // +0x1C0
        };
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
        const GuiEventNetworkLobbyPlayerListPayload* lpPlayerList =
            reinterpret_cast<const GuiEventNetworkLobbyPlayerListPayload*>(lpEvent);

        s32 liCurrentPlayerID = -1;
        if (lpPlayerList->miNumPlayers > 0)
        {
            // mPlayerNameComponent.miHighlightedIndex is the SelectableGroup's signed
            // highlighted-row byte (-1 == nothing highlighted); the X360 sign-extends it
            // before the compare.
            const s32 liHighlightedIndex = mPlayerNameComponent.miHighlightedIndex;
            if (liHighlightedIndex > -1)
                liCurrentPlayerID = lpPlayerList->maPlayers[liHighlightedIndex].mPlayerID;
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
                for (; liPlayerIndex < lpPlayerList->miNumPlayers; ++liPlayerIndex)
                {
                    if (lpPlayerList->maPlayers[liPlayerIndex].mPlayerID == mSelectedPlayerID)
                        break;
                }

                if (liPlayerIndex == lpPlayerList->miNumPlayers)
                {
                    ShowPlayerList(mSelectedPlayerID);
                    mOptionsAnimation.AddOutputAptViewState(KAC_APT_TRANSITION,
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

        mpStateInterface->PlayAptMovie(KAC_EMPTY_MOVIE, KI_APT_MOVIE_LEVEL);

        // X360 BrnOnlineGameRoomPlayerInfo.cpp:4135.
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        // Re-read (not the asserted copy): the X360 reloads the member and re-tests it.
        if (mpGuiCache != 0)
        {
            mpGuiCache->UnloadResources(maLobbyResourceTuplesToLoad,
                                        static_cast<u32>(miNumLobbyResourcesToLoad));
        }
    }

    // --------------------------------------------------- UnloadMapResources @ 0x8249A280
    // Tear the map page down: unmount its apt movie the same way, then hand back the
    // BASE class's map resource tuples -- CrashNavMap owns that table (protected static),
    // and this screen unloads it because the map page IS the base class.
    void OnlineGameRoomPlayerInfo::UnloadMapResources()
    {
        mpStateInterface->PlayAptMovie(KAC_EMPTY_MOVIE, KI_APT_MOVIE_LEVEL);

        // X360 BrnOnlineGameRoomPlayerInfo.cpp:4182.
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        if (mpGuiCache != 0)
        {
            mpGuiCache->UnloadResources(CrashNavMap::maResourcesToLoad,
                                        CrashNavMap::muNumResourcesToLoad);
        }
    }
}
