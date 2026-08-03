// ===================================================================================
// BrnGui::CarSelectLivery -- wave-J partfile 01.
//   HandleLobbyPlayerList @0x824B5190
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (asm arbitrates; Hex-Rays is
// a hint only). The five CarSelectOnlinePlayerList methods this body calls are declared in
// that component's header and link from its own TU.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnCarSelectLivery.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameSource/Gui/BrnGuiCache.h"                                   // GetOnlinePlayerInfoFromPlayerId
#include "GameSource/GameState/BrnGameStateSharedIO.h"                    // GsmIO::ECarSelectType
#include "GameSource/Gui/Flow/Screen/Components/BrnCarSelectOnlinePlayerList.h"           // the list component
#include "GameSource/Network/SharedIO/BrnNetworkModuleOnlineLobbyPlayerStatusInterface.h" // LobbyPlayerStatusData
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h"      // InGamePlayerStatusData

namespace BrnGui
{
    namespace
    {
        // ---- in-queue payload view (event 244, GuiEventNetworkLobbyPlayerList) -------
        // The queue delivers the RAW record -- this event carries no CgsGui::GuiEvent
        // header (AddGuiEvent<GuiEventNetworkLobbyPlayerList> @0x823CF4C8 hands
        // VariableEventQueue::AddEvent the record with id 244 and length 456 passed
        // out-of-band). The 456 bytes are the eight lobby rows then the live count.
        // Row fields come from the record's real home,
        // BrnNetworkModuleOnlineLobbyPlayerStatusInterface.h -- no forked layout here.
        // Same view as the wave-H consumer, BrnOnlineGameRoomPlayerInfo_wH_15.cpp:60-70.
        struct GuiEventNetworkLobbyPlayerListPayload
        {
            BrnNetwork::BrnNetworkModuleIO::LobbyPlayerStatusData
                maPlayers[CarSelectOnlinePlayerList::KI_MAX_PLAYERS];   // +0x000
            s32 miNumPlayers;                                           // X360 +0x1C0 (448)
        };
    }

    // -------------------------------------------- HandleLobbyPlayerList @ 0x824B5190
    // The network module republished the lobby roster while this screen is up. Bring the
    // eight-row player table in line with it: show and name any row that is not up yet,
    // re-drive every live row's car and final-selection tick, then take down the tail.
    //
    // The whole body is behind an ONLINE-ONLY guard (X360 `lwz 0x288` / `0x7C8 == 2` /
    // `0x7CC == 2`): a GuiCache must be bound, this must be the online-event-start car
    // select, and the screen must be in its interactive state. The offline Junkyard flow
    // observes event 244 too and falls straight out here.
    void CarSelectLivery::HandleLobbyPlayerList(const GuiEventNetworkLobbyPlayerList* lpEvent)
    {
        if (mpGuiCache == 0
            || meCarSelectType != BrnGameState::GameStateModuleIO::E_CAR_SELECT_TYPE_ONLINE_EVENT_START
            || meCurrentState != E_CARSELECT_VISIBLE_INTERACTIVE)
        {
            return;
        }

        const GuiEventNetworkLobbyPlayerListPayload* lpPlayerList =
            reinterpret_cast<const GuiEventNetworkLobbyPlayerListPayload*>(lpEvent);

        // liPlayer is carried OUT of this loop into the hide sweep below (X360 r31), which
        // is why it is declared here and the second loop has no initialiser. The count is a
        // signed compare (`cmpw` / `blt`), and the main loop is bounded by the count alone
        // -- exactly as the console does it (the producer clamps the roster to eight).
        s32 liPlayer = 0;
        for (; liPlayer < lpPlayerList->miNumPlayers; ++liPlayer)
        {
            const BrnNetwork::BrnNetworkModuleIO::LobbyPlayerStatusData& lrPlayer =
                lpPlayerList->maPlayers[liPlayer];

            // A row that is not up yet is shown and named once; the name is only ever
            // fetched on that transition.
            if (!mOnlinePlayerList.IsShowing(liPlayer))
            {
                mOnlinePlayerList.Show(liPlayer);

                // The console fetches the record twice around the assert -- that is the
                // assert macro expanding, not two separate reads -- and dereferences it
                // unguarded either way. Fetch once, assert, use.
                const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerInfo =
                    mpGuiCache->GetOnlinePlayerInfoFromPlayerId(lrPlayer.mPlayerID);

                CGS_ASSERT(lpPlayerInfo != 0,
                           "Trying to show the name of a player who isn't in our game");  // cpp:1177

                // X360 `addi r5, r3, 0x100` -- the record's name field at console +256.
                mOnlinePlayerList.SetPlayerName(liPlayer, lpPlayerInfo->mPlayerName.macName);
            }

            // Driven every refresh, shown or not: the tick and the car can change under a
            // row that is already up.
            mOnlinePlayerList.SetFinalSelection(liPlayer, lrPlayer.mbFinalSelection);  // lbz row+50
            mOnlinePlayerList.SetPlayerCar(liPlayer, lrPlayer.mSelectedCarID);         // ld  row+0
        }

        // Take down the tail, starting from the first row the roster did not fill. The rows
        // are contiguous, so the sweep stops at the first one that is already hidden rather
        // than running to the end of the bank. Entered even when the roster is empty (the
        // console's `ble` jumps straight in with the index still zero).
        for (; liPlayer < CarSelectOnlinePlayerList::KI_MAX_PLAYERS
               && mOnlinePlayerList.IsShowing(liPlayer); ++liPlayer)
        {
            mOnlinePlayerList.Hide(liPlayer);
        }
    }
}
