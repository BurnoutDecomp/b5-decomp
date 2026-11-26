// GameSource/Gui/Flow/Screen/States/BrnOnlineCreateFreeburn.cpp
//
// BrnGui::OnlineCreateFreeburn -- the online "create freeburn" screen (ON_CREATE_FB): Easy
// Drive's Freeburn -> Create lands here after the sign-in screen. The eight bodies, read
// store for store off the console asm:
//
//   OnEnter / OnLeave / Update     the lifecycle
//   CheckForCompletedLoads         load ON_CREA + the route-info package, then post the
//                                  create-game request (GUI 256: a free-burn lobby, unranked)
//                                  behind the "entering game" overlay
//   HandleGuiCacheEvent            adopt the cache; leave when multiplayer is not allowed
//   HandleInGameEvent              GUI 50: finish the overlay wait and advance (game room)
//   HandleInGameFailedEvent        GUI 51: back out
//   HandleControllerInput          the screen takes no input (only the null-event assert)
//
// Out-queue records are the console's own wire records, posted on channel 40 through
// GetOutputEventQueue()->AddEvent with host sizeof sizes (the committed OutputGuiEvent
// template does not use channel 40; see CgsGuiStateInterface.h). The sat-nav record goes
// through OutputViewState / OutputInternalState, which build the console's wrapped records.
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnOnlineCreateFreeburn.cpp"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineCreateFreeburn.h"

#include <cstddef>                                                        // offsetof (wire records)
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N> / GuiEventWrapper
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface / OutputViewState
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // the state in-queue / AddEvent
#include "GameSource/GameState/BrnGameStateSharedIO.h"                    // GameStateModuleIO::EGameModeType
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // GuiEventShowHideSatNav
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // overlay requests, GuiEventActivateCrashNav, GuiFlow
#include "GameSource/Gui/Events/BrnGuiEventNetworkCreateGame.h"           // BrnGui::GuiEventNetworkCreateGame

namespace BrnGui
{
    // ================================================================================
    //  Class statics (values read from the image)
    // ================================================================================

    // The events the screen observes: 14, apt ONLOAD, controller press, gui cache,
    // disconnected, in-game, in-game failed.
    const s32 OnlineCreateFreeburn::maiEventToObserve[7] = { 14, 21, 6, 64, 44, 50, 51 };
    const s32 OnlineCreateFreeburn::miNumEventsObserved  = 7;

    // ON_CREA and the route-info package.
    const CgsGui::sResourceTuple OnlineCreateFreeburn::maResourceTuplesToLoad[] =
    {
        { 176, CgsGui::E_GUI_RESOURCETYPE_APT },
        { 191, CgsGui::E_GUI_RESOURCETYPE_APT }
    };
    const s32 OnlineCreateFreeburn::miNumResourcesToLoad = 2;

    namespace
    {
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;   // mpInGuiEventQueue's real type

        const s32 KI_CHANNEL_GUI_OUT = 40;

        // The observed events Update dispatches on.
        const s32 KI_EVENT_CONTROLLER_INPUT      = 6;
        const s32 KI_EVENT_OBSERVED_NO_ARM_14    = 14;
        const s32 KI_EVENT_APT_ONLOAD            = 21;   // observed, no arm
        const s32 KI_EVENT_NETWORK_DISCONNECTED  = 44;
        const s32 KI_EVENT_IN_GAME               = 50;
        const s32 KI_EVENT_IN_GAME_FAILED        = 51;
        const s32 KI_EVENT_GUI_CACHE             = 64;

        const s32  KI_APT_MOVIE_LEVEL        = 3;
        const char* const KPC_EMPTY_STRING   = "";

        // The "entering game" overlay the create request runs behind.


        // The create request's security word: the first option, open to all comers
        // (the network's E_GAMESECURITY_ALLCOMERS).
        const s32 KI_GAME_SECURITY_ALLCOMERS = 0;

        struct GuiCachePayload : public CgsModule::Event
        {
            GuiCache* mpCache;   // +0x00
        };

        // ---- out-queue wire records ------------------------------------------------------
        // { 480, 256, 12, the 480-byte request }, channel 40, 492 bytes.
        typedef CgsGui::GuiEventOut<GuiEventNetworkCreateGame> GuiEventNetworkCreateGameWire;

        // { 288, 184, 16, <pad>, the 288-byte request }, channel 40, 304 bytes.
        struct GuiOverlayRequestWire
        {
            GuiOverlayRequest mRequest;   // the OutputGuiEvent payload

            GuiOverlayRequestWire()
            {
            }
        };

        // { 8, 188, 16, <pad>, the compressed overlay id }, channel 40, 24 bytes.
        struct GuiOverlayWaitFinishWire
        {
            GuiOverlayWaitFinishRequest mRequest;   // the OutputGuiEvent payload

            explicit GuiOverlayWaitFinishWire(const char* lpcOverlayName)
            {
                mRequest.Construct(lpcOverlayName);
            }
        };

        // { 1, 533, 12, <one byte> }, channel 40, 16 bytes -- the record the screens post
        // when they hand the front end back. The console never writes the payload byte.
        struct GuiEvent533Wire : public CgsGui::GuiEvent<533>
        {
            u8 muUnwrittenPayload;   // +0x0C

            GuiEvent533Wire()
                : muUnwrittenPayload(0)
            {
            }
        };

        CGS_STATIC_ASSERT((sizeof(GuiEventNetworkCreateGame) == 480), "create-game payload is 480 bytes");
        CGS_STATIC_ASSERT((sizeof(GuiEventNetworkCreateGameWire) == 492), "create-game record is 492 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiOverlayRequest>) == 304), "overlay request record is 304 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiOverlayWaitFinishRequest>) == 24), "wait-finish record is 24 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiEvent533Wire>) == 16), "id-533 record is 16 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiEventActivateCrashNav>) == 20), "activate-crashnav record is 20 bytes");
    }

    // ================================================================================
    //  OnEnter
    // ================================================================================
    void OnlineCreateFreeburn::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        meSubState = E_SUBSTATE_LOADING_SCREEN;
        mpGuiCache = 0;

        // The create page owns the screen: CrashNav down, HUD hidden.
        GuiEventActivateCrashNav lDeactivateCrashNav;
        lDeactivateCrashNav.muActivate = 0u;
        lDeactivateCrashNav.muParam = 0;
        mpStateInterface->OutputGuiEvent(lDeactivateCrashNav);

        GuiEventShowHideHud lShowHideHudEvent;
        lShowHideHudEvent.mbShow = false;
        mpStateInterface->OutputGuiEvent(lShowHideHudEvent);
    }

    // ================================================================================
    //  OnLeave
    // ================================================================================
    void OnlineCreateFreeburn::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // The console inlines StateInterface::PlayAptMovie here: the empty name at level 3
        // clears the level ({ 8, 18, 12, name, level } on channel 41).
        mpStateInterface->PlayAptMovie(KPC_EMPTY_STRING, KI_APT_MOVIE_LEVEL);
    }

    // ================================================================================
    //  Update
    // ================================================================================
    void OnlineCreateFreeburn::Update()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            switch (liEventId)
            {
            case KI_EVENT_CONTROLLER_INPUT:
                HandleControllerInput(lpEvent);
                break;

            case KI_EVENT_OBSERVED_NO_ARM_14:
            case KI_EVENT_APT_ONLOAD:
                break;

            case KI_EVENT_GUI_CACHE:
                HandleGuiCacheEvent(lpEvent);
                break;

            case KI_EVENT_NETWORK_DISCONNECTED:
                CGS_ASSERT_AT(mpGuiCache != 0, "mpGuiCache", CGS_ARTIST_FILE, 151);
                // A null event still latches (0).
                mpGuiCache->SetDoDisconnectPopup(lpEvent);
                SendStateEvent("DISCONNECT");
                break;

            case KI_EVENT_IN_GAME:
                HandleInGameEvent(lpEvent);
                break;

            case KI_EVENT_IN_GAME_FAILED:
                HandleInGameFailedEvent(lpEvent);
                break;

            default:
                // The console's "Unexpected event received : <id> in <file> at line 172"
                // debug message, printed only behind the message filter flags.
                break;
            }
        }

        lpInQueue->Clear();

        CheckForCompletedLoads();
    }

    // ================================================================================
    //  CheckForCompletedLoads
    //
    //  Two sub-states: wait for the two apt packages, then for the apt components. Once both
    //  are in, the free-burn lobby is created behind the "entering game" overlay and the
    //  screen waits for the network's in-game answer.
    // ================================================================================
    void OnlineCreateFreeburn::CheckForCompletedLoads()
    {
        CGS_ASSERT_AT(mpGuiCache != 0, "mpGuiCache", CGS_ARTIST_FILE, 221);

        if (meSubState == E_SUBSTATE_LOADING_SCREEN)
        {
            if (mpGuiCache->EnsureResourcesAreLoaded(maResourceTuplesToLoad,
                                                     static_cast<u32>(miNumResourcesToLoad)))
            {
                mpStateInterface->PlayAptMovie("ON_CREA", KI_APT_MOVIE_LEVEL);
                meSubState = E_SUBSTATE_LOADING_COMPONENTS;
            }
        }
        else if (meSubState == E_SUBSTATE_LOADING_COMPONENTS)
        {
            // The console re-tests the cache after the assert (the assert does not stop).
            if (mpGuiCache != 0 && mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
            {
                mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);

                // The default online game, turned into an unranked free-burn lobby open to
                // all comers.
                GuiEventNetworkCreateGame lCreateGame;
                lCreateGame.Construct();
                lCreateGame.meGameMode = BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY;
                lCreateGame.mbRanked   = false;
                lCreateGame.meSecurity = KI_GAME_SECURITY_ALLCOMERS;

                mpStateInterface->OutputGuiEvent(lCreateGame);

                GuiOverlayRequestWire lEnterGameOverlay;
                lEnterGameOverlay.mRequest.Construct("CNOnlEntGame");
                mpStateInterface->OutputGuiEvent(lEnterGameOverlay.mRequest);

                meSubState = E_SUBSTATE_WAIT_IN_GAME;

                // Clear apt level 3, hide the main map on both the view and the internal
                // channels, raise the CrashNav flow again and hand the front end back.
                mpStateInterface->PlayAptMovie(KPC_EMPTY_STRING, KI_APT_MOVIE_LEVEL);

                GuiEventShowHideSatNav lHideSatNav;
                lHideSatNav.Construct(GuiEventShowHideSatNav::E_MAPTYPE_MAIN, false, 0.0f);
                mpStateInterface->OutputViewState(lHideSatNav);
                mpStateInterface->OutputInternalState(lHideSatNav);

                GuiEventActivateCrashNav lActivateCrashNav;
                lActivateCrashNav.muActivate = 1u;
                lActivateCrashNav.muParam = 0;
                mpStateInterface->OutputGuiEvent(lActivateCrashNav);

                GuiEvent533Wire lHandBack;
                mpStateInterface->OutputGuiEvent(lHandBack);
            }
        }
    }

    // ================================================================================
    //  CheckPrivileges -- inlined into HandleGuiCacheEvent on the console.
    // ================================================================================
    bool OnlineCreateFreeburn::CheckPrivileges()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");
        return mpGuiCache->IsMultiplayerAllowed();
    }

    // ================================================================================
    //  HandleGuiCacheEvent -- adopt the first cache offered; without multiplayer privileges
    //  the screen backs out.
    // ================================================================================
    void OnlineCreateFreeburn::HandleGuiCacheEvent(const CgsModule::Event* lpEvent)
    {
        const GuiCachePayload* lpPayload = reinterpret_cast<const GuiCachePayload*>(lpEvent);

        // Streamed on the console; non-fatal.
        CGS_ASSERT_AT(lpPayload->mpCache != 0, "Invalid cache in HandleGuiCacheEvent::Update", CGS_ARTIST_FILE, 376);

        if (mpGuiCache != 0)
        {
            return;
        }

        mpGuiCache = lpPayload->mpCache;

        if (!CheckPrivileges())
        {
            SendStateEvent("GO_BACK");
        }
    }

    // ================================================================================
    //  HandleInGameEvent -- GUI 50: the lobby exists and the player is in it.
    // ================================================================================
    void OnlineCreateFreeburn::HandleInGameEvent(const CgsModule::Event* lpInGameEvent)
    {
        // Both asserts fire on a null event (the streamed one, then the plain one).
        CGS_ASSERT(lpInGameEvent != 0, "Invalid event sent to OnlineCreateFreeburn::HandleInGameEvent");
        CGS_ASSERT_AT(lpInGameEvent != 0, "lpInGameEvent", CGS_ARTIST_FILE, 339);

        GuiOverlayWaitFinishWire lWaitEnterGame("CNOnlEntGame");
        mpStateInterface->OutputGuiEvent(lWaitEnterGame.mRequest);

        SendStateEvent("ADVANCE");
    }

    // ================================================================================
    //  HandleInGameFailedEvent -- GUI 51: the create failed; back out.
    // ================================================================================
    void OnlineCreateFreeburn::HandleInGameFailedEvent(const CgsModule::Event* lpEvent)
    {
        // The console's text names HandleInGameEvent (the same pooled string).
        CGS_ASSERT(lpEvent != 0, CGS_ASSERT_STREAM_MESSAGE_AT("Invalid event sent to OnlineCreateFreeburn::HandleInGameEvent", CGS_ARTIST_FILE, 361));

        SendStateEvent("GO_BACK");
    }

    // ================================================================================
    //  HandleControllerInput -- the screen reacts to no button; only the null-event assert.
    // ================================================================================
    void OnlineCreateFreeburn::HandleControllerInput(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, CGS_ASSERT_STREAM_MESSAGE_AT("Invalid event sent to OnlineCreateFreeburn::HandleControllerInput", CGS_ARTIST_FILE, 297));
    }
}

#undef CGS_ARTIST_FILE
