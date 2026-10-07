// BrnGui::OnlineTeamSelection -- the "ON_TEAMS" online team-selection screen state,
// reconstructed from the console build.

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineTeamSelection.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                        // CgsGui::GuiAccessPointers
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"    // StateInterface
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"            // VariableEventQueue<18432,16>
#include "GameSource/Gui/BrnGuiCache.h"                                     // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                             // GuiAudioTriggerEvent / GuiFlow
#include "GameSource/Gui/BrnGuiShared.h"                                    // gGuiResourceIdentifier
#include "GameSource/Input/GameInputActions.h"                              // EGameInputActions
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h"   // InGamePlayerStatusData
#include "GameSource/Network/SharedIO/BrnNetworkModuleOnlineLobbyPlayerStatusInterface.h" // LobbyPlayerStatusData

namespace BrnGui
{
namespace
{
    typedef CgsModule::VariableEventQueue<18432, 16> InGuiEventQueue;
    typedef BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData InGamePlayerStatusData;
    typedef BrnNetwork::BrnNetworkModuleIO::LobbyPlayerStatusData  LobbyPlayerStatusData;

    const s32 KI_CHANNEL_GUI_OUT = 40;

    const s32 KI_EVENT_CONTROLLER_PRESSED = 6;
    const s32 KI_EVENT_APT_TRIGGER        = 21;
    const s32 KI_EVENT_DISCONNECT         = 44;
    const s32 KI_EVENT_GUI_CACHE          = 64;
    const s32 KI_EVENT_LOBBY_PLAYER_LIST  = 244;

    // gGuiResourceIdentifier[179] == "ON_TEAMS", played on level 3.
    const u32 KU_ON_TEAMS_RESOURCE = 179;
    const s32 KI_SCREEN_MOVIE_LEVEL = 3;

    const s32 KI_MAX_PLAYERS = 8;   // the player-name menu rows / the lobby table size

    // The audio action and sound the highlight move plays.
    const s32 KI_AUDIO_ACTION_MENU_TOGGLE = 7;
    const u32 KU_AUDIO_TRIGGER_EVENT_ID   = 457;   // the console's queued id for the audio record

    // The in-queue delivers the payloads without their GuiEvent header.
    struct ControllerButtonPayload : public CgsModule::Event
    {
        s32 miPadId;      // +0x00
        s32 miButtonId;   // +0x04 (EGameInputActions)
    };

    struct GuiCachePayload : public CgsModule::Event
    {
        GuiCache* mpCache;   // +0x00
    };

    // Event 244: the eight lobby rows, then the live count (+0x1C0).
    struct GuiEventNetworkLobbyPlayerListPayload
    {
        LobbyPlayerStatusData maPlayers[KI_MAX_PLAYERS];   // +0x000
        s32                   miNumPlayers;                // +0x1C0
    };
}

const s32 OnlineTeamSelection::maiEventToObserve[5] =
{
    KI_EVENT_CONTROLLER_PRESSED, KI_EVENT_APT_TRIGGER, KI_EVENT_DISCONNECT,
    KI_EVENT_GUI_CACHE, KI_EVENT_LOBBY_PLAYER_LIST,
};
const s32 OnlineTeamSelection::miNumEventsObserved = 5;

const CgsGui::sResourceTuple OnlineTeamSelection::maResourcesToLoad[1] =
{
    { KU_ON_TEAMS_RESOURCE, CgsGui::E_GUI_RESOURCETYPE_APT },
};

OnlineTeamSelection::OnlineTeamSelection()
{
}

void OnlineTeamSelection::Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm)
{
    CGS_ASSERT(lpFsm != 0, "lpFsm");
    CgsGui::State::Construct(liId, lpFsm);
}

void OnlineTeamSelection::OnEnter()
{
    mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);
    mPlayerNameComponent.Construct("PlayerName", mpStateInterface, KI_MAX_PLAYERS, 0,
                                   Selectable::K_INVALID_ID);
    mpGuiCache        = 0;
    mSelectedPlayerID = -1;
    meSubState        = E_SUBSTATE_LOADING_SCREEN;
}

void OnlineTeamSelection::OnLeave()
{
    mpStateInterface->PlayAptMovie("", KI_SCREEN_MOVIE_LEVEL);
    mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

    mSelectedPlayerID = -1;
    if (mpGuiCache != 0)
        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);

    UnloadLobbyResources();
}

void OnlineTeamSelection::Update()
{
    InGuiEventQueue* lpInQueue = reinterpret_cast<InGuiEventQueue*>(mpInGuiEventQueue);

    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
         lpEvent != 0;
         liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
    {
        switch (liEventId)
        {
        case KI_EVENT_CONTROLLER_PRESSED:
            HandleControllerInputPressed(lpEvent);
            break;

        case KI_EVENT_APT_TRIGGER:
            break;

        case KI_EVENT_DISCONNECT:
        {
            // The console re-reads the cache through the access pointers here (not the
            // latched mpGuiCache): a disconnect can arrive before the cache event.
            if (!mpStateInterface->GetAccessPointers()->GetGuiCache()->IsPerformInviteReceived())
                mpStateInterface->GetAccessPointers()->GetGuiCache()->SetDoDisconnectPopup(lpEvent);
            SendStateEvent("DISCONNECT");
            break;
        }

        case KI_EVENT_GUI_CACHE:
            HandleGuiCacheEvent(lpEvent);
            CheckForCompletedLoads();
            break;

        case KI_EVENT_LOBBY_PLAYER_LIST:
        {
            const GuiEventNetworkLobbyPlayerListPayload* lpPlayerList =
                reinterpret_cast<const GuiEventNetworkLobbyPlayerListPayload*>(lpEvent);

            s32 liSelectedPlayerID = -1;
            if (lpPlayerList->miNumPlayers > 0)
            {
                const s32 liHighlightedIndex = mPlayerNameComponent.GetHighlightedIndex();
                if (liHighlightedIndex > -1)
                    liSelectedPlayerID = lpPlayerList->maPlayers[liHighlightedIndex].mPlayerID;
            }
            if (static_cast<u32>(meSubState) >= static_cast<u32>(E_SUBSTATE_MAIN))
                ShowPlayerList(liSelectedPlayerID);
            break;
        }

        default:
            CGS_ASSERT(false, "Unexpected event received : ");
            break;
        }
    }

    if (meSubState == E_SUBSTATE_MAIN)
        mPlayerNameComponent.Update();

    lpInQueue->Clear();
}

void OnlineTeamSelection::HandleGuiCacheEvent(const CgsModule::Event* lpEvent)
{
    const GuiCachePayload* lpPayload = static_cast<const GuiCachePayload*>(lpEvent);
    CGS_ASSERT(lpPayload->mpCache != 0, "Invalid cache in HandleGuiCacheEvent::Update");

    if (mpGuiCache == 0)
    {
        mpGuiCache = lpPayload->mpCache;
        SendStateEvent("TO_ON_CARSEL");
    }
}

void OnlineTeamSelection::CheckForCompletedLoads()
{
    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

    switch (meSubState)
    {
    case E_SUBSTATE_LOADING_SCREEN:
        if (mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, 1))
        {
            SetExpectedLobbyComponents();
            mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[KU_ON_TEAMS_RESOURCE],
                                           KI_SCREEN_MOVIE_LEVEL);
            meSubState = E_SUBSTATE_LOADING_COMPONENTS;
        }
        break;

    case E_SUBSTATE_LOADING_COMPONENTS:
        if (mpGuiCache != 0 && mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
        {
            mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
            meSubState = E_SUBSTATE_MAIN;
            CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");
            mSelectedPlayerID           = -1;
            mbHighlightOnSelectedPlayer = false;
        }
        break;

    default:
        break;
    }
}

void OnlineTeamSelection::SetExpectedLobbyComponents()
{
    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");
    mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
    mPlayerNameComponent.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);
}

void OnlineTeamSelection::UnloadLobbyResources()
{
    mpStateInterface->PlayAptMovie("", KI_SCREEN_MOVIE_LEVEL);

    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");
    if (mpGuiCache != 0)
        mpGuiCache->UnloadResources(maResourcesToLoad, 1);
}

void OnlineTeamSelection::ShowPlayerList(s32 liSelectedPlayerID)
{
    s32 liPreviousIndex = mPlayerNameComponent.GetHighlightedIndex();
    if (liPreviousIndex == -1)
        liPreviousIndex = 0;

    mPlayerNameComponent.SetupMenu(static_cast<s32>(mpGuiCache->muNumActivePlayers), false);

    // GetOnlinePlayerInfo is inlined by the console (its two bounds asserts included); the
    // row label is the player's online name.
    for (s32 liPlayerIndex = 0;
         liPlayerIndex < static_cast<s32>(mpGuiCache->muNumActivePlayers);
         ++liPlayerIndex)
    {
        const InGamePlayerStatusData* lpPlayerInfo = mpGuiCache->GetOnlinePlayerInfo(liPlayerIndex);
        mPlayerNameComponent.SetText(liPlayerIndex, lpPlayerInfo->mPlayerName.macName);
    }

    // Find the lobby row of the selected player and highlight it.
    const s32 liNumPlayers = static_cast<s32>(mpGuiCache->muNumActivePlayers);
    s32 liFoundIndex = 0;
    if (liNumPlayers > 0)
    {
        for (; liFoundIndex < liNumPlayers; ++liFoundIndex)
        {
            const LobbyPlayerStatusData* lpLobbyPlayer =
                reinterpret_cast<const LobbyPlayerStatusData*>(mpGuiCache->maLobbyPlayerInfo[liFoundIndex]);
            if (lpLobbyPlayer->mPlayerID == liSelectedPlayerID)
            {
                mPlayerNameComponent.HighlightIndex(liFoundIndex);
                break;
            }
        }
    }

    // Not found (or not following the selection): keep the previous row, clamped.
    const s32 liNumRows = static_cast<s32>(mpGuiCache->muNumActivePlayers);
    if (liNumRows > 0 && (liFoundIndex == liNumRows || !mbHighlightOnSelectedPlayer))
    {
        if (liPreviousIndex >= liNumRows)
            liPreviousIndex = liNumRows - 1;
        mPlayerNameComponent.HighlightIndex(liPreviousIndex);
    }
}

void OnlineTeamSelection::HandleControllerInputPressed(const CgsModule::Event* lpEvent)
{
    CGS_ASSERT(lpEvent != 0, "Invalid event sent to OnlineTeamSelection::HandleControllerInputPressed");

    if (meSubState == E_SUBSTATE_MAIN)
        HandleControllerInputMainSubState(lpEvent);
}

void OnlineTeamSelection::HandleControllerInputMainSubState(const CgsModule::Event* lpEvent)
{
    const ControllerButtonPayload* lpPress = static_cast<const ControllerButtonPayload*>(lpEvent);

    bool lbMoved = false;
    if (lpPress->miButtonId == E_GAMEINPUTACTIONS_GUI_UP)
    {
        if (mPlayerNameComponent.GetHighlightedIndex() <= -1)
            return;
        lbMoved = mPlayerNameComponent.HighlightPrevious();
    }
    else if (lpPress->miButtonId == E_GAMEINPUTACTIONS_GUI_DOWN)
    {
        if (mPlayerNameComponent.GetHighlightedIndex() <= -1)
            return;
        lbMoved = mPlayerNameComponent.HighlightNext();
    }

    if (lbMoved)
    {
        // OutputGuiEvent<GuiAudioTriggerEvent>: { 100, 457, 12, the 100-byte record },
        // 112 bytes on channel 40.
        GuiAudioTriggerEvent lAudio;
        lAudio.Construct(KI_AUDIO_ACTION_MENU_TOGGLE, "", "MenuItemToggleDefault", "");
        lAudio.muHeader0   = 100u;
        lAudio.muEventType = KU_AUDIO_TRIGGER_EVENT_ID;
        lAudio.muHeader2   = 12u;
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            &lAudio, KI_CHANNEL_GUI_OUT, static_cast<s32>(sizeof(lAudio)));
    }
}
}
