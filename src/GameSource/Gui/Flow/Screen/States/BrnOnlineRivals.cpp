// GameSource/Gui/Flow/Screen/States/BrnOnlineRivals.cpp
//
// BrnGui::OnlineRivals -- the online "rivals" screen (ON_RIVAL). The seven bodies, read off
// the console asm:
//
//   OnEnter / OnLeave / Update                 the lifecycle
//   CheckForCompletedLoads                     load ON_RIVAL, then wait for its components
//   HandleGuiCacheEvent                        adopt the first cache offered
//   HandleControllerInput(+SelectParams)       the back button leaves the screen

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineRivals.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStreamBase (unexpected-event log)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // CgsDev::Log::gpDebugPrint / Message::gxMessageFilterFlags
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // the state in-queue
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiShared.h"                                  // gGuiResourceIdentifier
#include "GameSource/Input/GameInputActions.h"                            // EGameInputActions

namespace BrnGui
{
    // ================================================================================
    //  Class statics (values read from the image)
    // ================================================================================

    // The events the screen observes: 14, apt ONLOAD, controller press, gui cache,
    // disconnected.
    const s32 OnlineRivals::maiEventToObserve[5] = { 14, 21, 6, 64, 44 };
    const s32 OnlineRivals::miNumEventsObserved  = 5;

    // The one apt package the screen loads (ON_RIVAL).
    const CgsGui::sResourceTuple OnlineRivals::maResourceTuplesToLoad[] =
    {
        { 180, CgsGui::E_GUI_RESOURCETYPE_APT }
    };
    const s32 OnlineRivals::miNumResourcesToLoad = 1;

    namespace
    {
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;   // mpInGuiEventQueue's real type

        // The observed events Update dispatches on.
        const s32 KI_EVENT_CONTROLLER_INPUT     = 6;
        const s32 KI_EVENT_OBSERVED_NO_ARM_14   = 14;
        const s32 KI_EVENT_APT_ONLOAD           = 21;   // observed, no arm
        const s32 KI_EVENT_NETWORK_DISCONNECTED = 44;
        const s32 KI_EVENT_GUI_CACHE            = 64;

        // The movie CheckForCompletedLoads plays: the screen's own package.
        const u32 KU_RIVALS_MOVIE_RESOURCE = 180;
        const s32 KI_APT_MOVIE_LEVEL       = 3;
        const char* const KPC_EMPTY_STRING = "";

        const char KAC_GO_BACK_EVENT[] = "GO_BACK";

        // ---- in-queue payload views (the queue hands out the header-stripped payload) ----
        struct ControllerButtonPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miButtonId;   // +0x04 (the input action id)
        };

        struct GuiCachePayload : public CgsModule::Event
        {
            GuiCache* mpCache;   // +0x00
        };
    }

    // ================================================================================
    //  OnEnter
    // ================================================================================
    void OnlineRivals::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        meSubState = E_SUBSTATE_LOADING_SCREEN;
        mpGuiCache = 0;
    }

    // ================================================================================
    //  OnLeave
    // ================================================================================
    void OnlineRivals::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // The console inlines StateInterface::PlayAptMovie here: the empty name at level 3
        // clears the level.
        mpStateInterface->PlayAptMovie(KPC_EMPTY_STRING, KI_APT_MOVIE_LEVEL);
    }

    // ================================================================================
    //  Update
    // ================================================================================
    void OnlineRivals::Update()
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

            case KI_EVENT_NETWORK_DISCONNECTED:
                SendStateEvent(KAC_GO_BACK_EVENT);
                break;

            case KI_EVENT_GUI_CACHE:
                HandleGuiCacheEvent(lpEvent);
                break;

            default:
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "Unexpected event received : " << liEventId
                        << " in "
                        << "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnOnlineRivals.cpp"
                        << " at line " << 131 << "\n";
                }
                break;
            }
        }

        lpInQueue->Clear();

        CheckForCompletedLoads();
    }

    // ================================================================================
    //  CheckForCompletedLoads -- wait for the apt package, then for its components.
    // ================================================================================
    void OnlineRivals::CheckForCompletedLoads()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        if (meSubState == E_SUBSTATE_LOADING_SCREEN)
        {
            if (mpGuiCache->EnsureResourcesAreLoaded(maResourceTuplesToLoad,
                                                     static_cast<u32>(miNumResourcesToLoad)))
            {
                mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[KU_RIVALS_MOVIE_RESOURCE],
                                               KI_APT_MOVIE_LEVEL);
                meSubState = E_SUBSTATE_LOADING_COMPONENTS;
            }
        }
        else if (meSubState == E_SUBSTATE_LOADING_COMPONENTS)
        {
            // The console re-tests the cache after the assert (the assert does not stop).
            if (mpGuiCache != 0 && mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
            {
                meSubState = E_SUBSTATE_SELECTING_PARAMS;
            }
        }
    }

    // ================================================================================
    //  HandleControllerInput -- only the parameter page takes input.
    // ================================================================================
    void OnlineRivals::HandleControllerInput(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, "Invalid event sent to OnlineRivals::HandleControllerInput");

        if (meSubState == E_SUBSTATE_SELECTING_PARAMS)
        {
            HandleControllerInputSelectParams(lpEvent);
        }
    }

    // ================================================================================
    //  HandleControllerInputSelectParams -- cancel goes back.
    // ================================================================================
    void OnlineRivals::HandleControllerInputSelectParams(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event sent to OnlineRivals::HandleControllerInputSelectParams");

        const ControllerButtonPayload* lpInput =
            static_cast<const ControllerButtonPayload*>(lpEvent);
        if (lpInput->miButtonId == E_GAMEINPUTACTIONS_GUI_CANCEL)
        {
            SendStateEvent(KAC_GO_BACK_EVENT);
        }
    }

    // ================================================================================
    //  HandleGuiCacheEvent -- adopt the first cache offered.
    // ================================================================================
    void OnlineRivals::HandleGuiCacheEvent(const CgsModule::Event* lpEvent)
    {
        const GuiCachePayload* lpPayload = static_cast<const GuiCachePayload*>(lpEvent);

        CGS_ASSERT(lpPayload->mpCache != 0, "Invalid cache in HandleGuiCacheEvent::Update");

        if (mpGuiCache == 0)
        {
            mpGuiCache = lpPayload->mpCache;
        }
    }
}
