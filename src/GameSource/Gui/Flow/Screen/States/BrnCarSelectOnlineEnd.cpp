// GameSource/Gui/Flow/Screen/States/BrnCarSelectOnlineEnd.cpp
//
// BrnGui::CarSelectOnlineEnd -- the online car-select "ready / countdown" screen. The nine
// bodies, read off the console asm:
//
//   OnEnter / OnLeave / Update          the lifecycle and the internal-state chain; entry and
//                                       exit post the online car-select action (GUI 192)
//   UpdateLoadResources / UpdateWFInit  load BrnCarSelectOnlineEnd + its package; a client of
//                                       a host-chooses game shows the "host choosing" clip
//   UpdateRunning                       the countdown (GUI 82) and the lobby roster (GUI 244)
//   UpdatePermanent                     apt ONLOAD, disconnected (GUI 44), advance (GUI 280)
//   HandleLobbyPlayerList               the player table; a client follows the host's pick,
//                                       and keeps its own if the host has dropped
//
// Out-queue records are the console's own wire records, posted on channel 40 through
// GetOutputEventQueue()->AddEvent with host sizeof sizes.

#include "GameSource/Gui/Flow/Screen/States/BrnCarSelectOnlineEnd.h"

#include <cstddef>                                                        // offsetof (wire records)
#include <cstring>                                                        // std::strstr
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStreamBase (debug print)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                      // CgsGui::GuiAccessPointers
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h" // GuiEventAptTriggerPayload
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // the state in-queue / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiShared.h"                                  // gGuiResourceIdentifier
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h"      // InGamePlayerStatusData
#include "GameSource/Network/SharedIO/BrnNetworkModuleOnlineLobbyPlayerStatusInterface.h" // LobbyPlayerStatusData

namespace BrnGui
{
    // ================================================================================
    //  Class statics (values read from the image)
    // ================================================================================

    // The events the screen observes: apt ONLOAD, disconnected, gui cache, the countdown,
    // advance (280) and the lobby roster.
    const s32 CarSelectOnlineEnd::maiEventToObserve[6] = { 21, 44, 64, 82, 280, 244 };
    const s32 CarSelectOnlineEnd::miNumEventsObserved  = 6;

    // BrnCarSelectOnlineEnd and the online car-select components package.
    const CgsGui::sResourceTuple CarSelectOnlineEnd::maResourcesToLoad[] =
    {
        { 151, CgsGui::E_GUI_RESOURCETYPE_APT },
        {  94, CgsGui::E_GUI_RESOURCETYPE_APT }
    };
    const u32 CarSelectOnlineEnd::muNumResourcesToLoad = 2;

    const char CarSelectOnlineEnd::KAC_ONLINE_COUNTDOWN_NAME[13]       = "Countdown_mc";
    const char CarSelectOnlineEnd::KAC_ONLINE_PLAYER_LIST[15]          = "PlayerTable_mc";
    const char CarSelectOnlineEnd::KAC_HOST_CHOOSING_ANIMATOR_NAME[16] = "HostChoice_anim";

    namespace
    {
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;   // mpInGuiEventQueue's real type

        const s32 KI_CHANNEL_GUI_OUT = 40;

        const s32 KI_EVENT_APT_TRIGGER          = 21;
        const s32 KI_EVENT_NETWORK_DISCONNECTED = 44;
        const s32 KI_EVENT_COUNTDOWN_TIME       = 82;
        const s32 KI_EVENT_LOBBY_PLAYER_LIST    = 244;
        const s32 KI_EVENT_ADVANCE              = 280;

        // The movie UpdateLoadResources plays: the screen's own package.
        const u32 KU_CAR_SELECT_ONLINE_END_MOVIE_RESOURCE = 151;
        const s32 KI_APT_MOVIE_LEVEL                      = 3;
        const char* const KPC_EMPTY_STRING                = "";

        // The online-host-game state (GuiCache::GetOnlineHostGameState) of a game whose host
        // picks the car for everyone.
        const s32 KI_ONLINE_HOST_GAME_HOST_CHOOSES = 1;

        // ---- in-queue payload views (the queue hands out the header-stripped payload) ----
        struct CountdownTimePayload : public CgsModule::Event
        {
            f32 mfTimeLeft;   // +0x00
        };

        // Event 244: the eight lobby rows then the player count (+0x1C0).
        struct GuiEventNetworkLobbyPlayerListPayload
        {
            BrnNetwork::BrnNetworkModuleIO::LobbyPlayerStatusData
                maPlayers[CarSelectOnlinePlayerList::KI_MAX_PLAYERS];   // +0x000
            s32 miNumPlayers;                                           // +0x1C0
        };

        // ---- out-queue wire records ------------------------------------------------------
        // { 8, 415, 16, <pad>, the car id }, channel 40, 24 bytes: change the local car.
        struct GuiChangeCarWire : public CgsGui::GuiEvent<415>
        {
            CgsID mCarID;    // +0x10

            explicit GuiChangeCarWire(CgsID lCarID)
                : mCarID(lCarID)
            {
            }
            GuiChangeCarWire() {}
        };

        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiChangeCarWire>) == 24), "change-car record is 24 bytes");
        CGS_STATIC_ASSERT((sizeof(GuiEventNetworkLobbyPlayerListPayload) >= 0x1C4),
                      "lobby roster count sits at +0x1C0");
    }

    // ================================================================================
    //  OnEnter
    // ================================================================================
    void CarSelectOnlineEnd::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        mpGuiCache = mpStateInterface->GetAccessPointers()->GetGuiCache();

        if (CgsDev::Message::gxMessageFilterFlags & 1)
        {
            *CgsDev::Log::gpDebugPrint << "RG :: CSE : Entering Car Select - "
                                       << mpGuiCache->GetCurrentCarSelectType() << "\n";
        }

        mOnlineCountdown.Construct(KAC_ONLINE_COUNTDOWN_NAME, mpStateInterface, 0);
        mOnlinePlayerList.Construct(KAC_ONLINE_PLAYER_LIST, mpStateInterface, 0);
        mHostChoosingAnimator.Construct(KAC_HOST_CHOOSING_ANIMATOR_NAME, mpStateInterface, 0);

        // DWARF local GuiEventActivateCarSelect lActivateCarSelect (cpp:82); ARTIST stores the
        // type word (+0x54) before the state word (+0x50) @0x824966EC.
        GuiEventActivateCarSelect lActivateCarSelect;
        lActivateCarSelect.meCarSelectType = BrnGameState::GameStateModuleIO::E_CAR_SELECT_TYPE_ONLINE_EVENT_START;
        lActivateCarSelect.meState         = BrnGameState::GameStateModuleIO::E_CAR_SELECT_STATE_ACCEPT_CAR;
        mpStateInterface->OutputGuiEvent(lActivateCarSelect);

        meInternalState = E_INTERNALSTATE_LOADRESOURCES;
        mLastHostCarID  = 0;
    }

    // ================================================================================
    //  OnLeave
    // ================================================================================
    void CarSelectOnlineEnd::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // The console inlines StateInterface::PlayAptMovie here: the empty name at level 3
        // clears the level.
        mpStateInterface->PlayAptMovie(KPC_EMPTY_STRING, KI_APT_MOVIE_LEVEL);

        // DWARF local GuiEventActivateCarSelect lActivateEvent (cpp:175).
        GuiEventActivateCarSelect lActivateEvent;
        lActivateEvent.meCarSelectType = BrnGameState::GameStateModuleIO::E_CAR_SELECT_TYPE_ONLINE_EVENT_START;
        lActivateEvent.meState         = BrnGameState::GameStateModuleIO::E_CAR_SELECT_STATE_EXIT;
        mpStateInterface->OutputGuiEvent(lActivateEvent);

        meInternalState = E_INTERNALSTATE_LEFT;
    }

    // ================================================================================
    //  Update -- each step stores its state on entry and drops into the next one once it
    //  completes; the permanent handlers then run and the in-queue is cleared.
    // ================================================================================
    void CarSelectOnlineEnd::Update()
    {
        switch (meInternalState)
        {
        case E_INTERNALSTATE_LOADRESOURCES:
            meInternalState = E_INTERNALSTATE_LOADRESOURCES;
            if (!UpdateLoadResources())
            {
                break;
            }
            // fall through

        case E_INTERNALSTATE_WFINIT:
            meInternalState = E_INTERNALSTATE_WFINIT;
            if (!UpdateWFInit())
            {
                break;
            }
            // fall through

        case E_INTERNALSTATE_RUNNING:
            meInternalState = E_INTERNALSTATE_RUNNING;
            UpdateRunning();
            break;

        case E_INTERNALSTATE_LEFT:
            break;

        default:
            CGS_ASSERT(false, "Invalid internal state : ");
            break;
        }

        UpdatePermanent();

        mpInGuiEventQueue->Clear();
    }

    // ================================================================================
    //  UpdateLoadResources -- load the packages, play the screen, expect the host clip.
    // ================================================================================
    bool CarSelectOnlineEnd::UpdateLoadResources()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        if (!mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad))
        {
            return false;
        }

        mpStateInterface->PlayAptMovie(
            gGuiResourceIdentifier[KU_CAR_SELECT_ONLINE_END_MOVIE_RESOURCE], KI_APT_MOVIE_LEVEL);

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mHostChoosingAnimator.GetName());
        return true;
    }

    // ================================================================================
    //  UpdateWFInit -- once the clip is up, a client of a host-chooses game shows it.
    // ================================================================================
    bool CarSelectOnlineEnd::UpdateWFInit()
    {
        if (!mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
        {
            return false;
        }

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);

        if (mpGuiCache->GetOnlineHostGameState() == KI_ONLINE_HOST_GAME_HOST_CHOOSES)
        {
            mHostChoosingAnimator.AddOutputAptViewState("apt_Transition", "visible",
                                                        false);
        }

        return true;
    }

    // ================================================================================
    //  UpdateRunning -- the countdown and the lobby roster.
    // ================================================================================
    void CarSelectOnlineEnd::UpdateRunning()
    {


        const CgsModule::Event* lpEvent;
        s32 liSize;
        for (s32 liEventId = mpInGuiEventQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = mpInGuiEventQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            if (liEventId == KI_EVENT_COUNTDOWN_TIME)
            {
                mOnlineCountdown.SetTimeLeft(
                    static_cast<const CountdownTimePayload*>(lpEvent)->mfTimeLeft);
            }
            else if (liEventId == KI_EVENT_LOBBY_PLAYER_LIST)
            {
                HandleLobbyPlayerList(reinterpret_cast<const GuiEventNetworkLobbyPlayerList*>(lpEvent));
            }
        }
    }

    // ================================================================================
    //  UpdatePermanent
    // ================================================================================
    void CarSelectOnlineEnd::UpdatePermanent()
    {


        const CgsModule::Event* lpEvent;
        s32 liSize;
        for (s32 liEventId = mpInGuiEventQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = mpInGuiEventQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            if (liEventId == KI_EVENT_APT_TRIGGER)
            {
                // A player-table row finished loading: let the table re-push it.
                const CgsGui::GuiEventAptTriggerPayload* lpTrigger =
                    reinterpret_cast<const CgsGui::GuiEventAptTriggerPayload*>(lpEvent);
                if (lpTrigger->meEventType == CgsGui::GuiEventAptTrigger::E_APT_EVENT_ONLOAD
                    && std::strstr(lpTrigger->mpacComponentName, mOnlinePlayerList.GetName()) != 0)
                {
                    mOnlinePlayerList.HandleLoadNotification(lpTrigger->mpacComponentName);
                }
            }
            else if (liEventId == KI_EVENT_NETWORK_DISCONNECTED)
            {
                CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");
                mpGuiCache->SetDoDisconnectPopup(lpEvent);

                if (CgsDev::Message::gxMessageFilterFlags & 1)
                {
                    *CgsDev::Log::gpDebugPrint << "RG :: CSE : SendStateEvent( \"DISCONNECT\" )\n";
                }
                SendStateEvent("DISCONNECT");
            }
            else if (liEventId == KI_EVENT_ADVANCE)
            {
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                {
                    *CgsDev::Log::gpDebugPrint << "RG :: CSE : SendStateEvent( \"ADVANCE\" )\n";
                }
                SendStateEvent("ADVANCE");
            }
        }
    }

    // ================================================================================
    //  HandleLobbyPlayerList -- bring the player table in line with the roster. In a game
    //  whose host picks the car, a client follows the host's pick once it is final; if the
    //  host has dropped out, the client keeps its own car.
    // ================================================================================
    void CarSelectOnlineEnd::HandleLobbyPlayerList(const GuiEventNetworkLobbyPlayerList* lpEvent)
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache != NULL");

        const GuiEventNetworkLobbyPlayerListPayload* lpPlayerList =
            reinterpret_cast<const GuiEventNetworkLobbyPlayerListPayload*>(lpEvent);

        CgsID lLocalPlayerCarID   = 0;
        bool  lbHostDisconnected  = false;

        s32 liPlayer = 0;
        for (; liPlayer < lpPlayerList->miNumPlayers; ++liPlayer)
        {
            const BrnNetwork::BrnNetworkModuleIO::LobbyPlayerStatusData& lrPlayer =
                lpPlayerList->maPlayers[liPlayer];

            if (!mOnlinePlayerList.IsShowing(liPlayer))
            {
                mOnlinePlayerList.Show(liPlayer);

                const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerInfo =
                    mpGuiCache->GetOnlinePlayerInfoFromPlayerId(lrPlayer.mPlayerID);
                CGS_ASSERT(lpPlayerInfo != 0,
                           "Trying to show the name of a player who isn't in our game");
                mOnlinePlayerList.SetPlayerName(liPlayer, lpPlayerInfo->mPlayerName.macName);
            }

            if (lrPlayer.mbLocalPlayer)
            {
                lLocalPlayerCarID = lrPlayer.mSelectedCarID;
            }
            else if (mpGuiCache->GetOnlineHostGameState() == KI_ONLINE_HOST_GAME_HOST_CHOOSES
                     && lrPlayer.mbIsHost)
            {
                if (mpGuiCache->GetOnlinePlayerDisconnected(
                        mpGuiCache->GetActiveRaceCarFromNetworkId(lrPlayer.mPlayerID)))
                {
                    lbHostDisconnected = true;
                }
                else
                {
                    if (lrPlayer.mSelectedCarID != mLastHostCarID)
                    {
                        mLastHostCarID = lrPlayer.mSelectedCarID;
                    }

                    if (lrPlayer.mbFinalSelection)
                    {
                        CGS_ASSERT(mLastHostCarID != 0, "Trying to select a car will null id");
                        GuiChangeCarWire lChangeCar(mLastHostCarID);
                        mpStateInterface->OutputGuiEvent(lChangeCar);
                    }
                }
            }

            mOnlinePlayerList.SetFinalSelection(liPlayer, lrPlayer.mbFinalSelection);
            mOnlinePlayerList.SetPlayerCar(liPlayer, lrPlayer.mSelectedCarID);
        }

        if (lbHostDisconnected)
        {
            CGS_ASSERT(lLocalPlayerCarID != 0, "Trying to select a car will null id");
            GuiChangeCarWire lChangeCar(lLocalPlayerCarID);
            mpStateInterface->OutputGuiEvent(lChangeCar);
        }

        // Take down the tail, from the first row the roster did not fill up to the first
        // row already hidden.
        for (; liPlayer < CarSelectOnlinePlayerList::KI_MAX_PLAYERS
               && mOnlinePlayerList.IsShowing(liPlayer); ++liPlayer)
        {
            mOnlinePlayerList.Hide(liPlayer);
        }
    }
}
