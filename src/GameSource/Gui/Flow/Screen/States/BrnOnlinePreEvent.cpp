// GameSource/Gui/Flow/Screen/States/BrnOnlinePreEvent.cpp
//
// BrnGui::OnlinePreEvent -- the online pre-event screen (ON_PRE_EVENT). The seven bodies,
// read off the console asm:
//
//   OnEnter / OnLeave / Update      the lifecycle and the internal-state chain
//   UpdateGetCache                  adopt the cache from the in-queue
//   UpdateLoadResources             load ON_PRE_EVENT and play it
//   UpdateRunning                   hide the current message once its time is up
//   UpdatePermanent                 the fly-by event shows the next message; apt ONLOAD
//                                   re-pushes text; a lost connection raises its overlay;
//                                   GUI 164 advances
//
// Out-queue records are the console's own wire records, posted on channel 40 through
// GetOutputEventQueue()->AddEvent with host sizeof sizes.

#include "GameSource/Gui/Flow/Screen/States/BrnOnlinePreEvent.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h" // GuiEventAptTriggerPayload
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // the state in-queue / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiOverlayRequest
#include "GameSource/Gui/BrnGuiShared.h"                                  // gGuiResourceIdentifier

namespace BrnGui
{
    // ================================================================================
    //  Class statics (values read from the image)
    // ================================================================================

    // The events the screen observes: controller press, apt ONLOAD, disconnected, gui cache,
    // advance (164) and the fly-by message (160).
    const s32 OnlinePreEvent::maiEventToObserve[6] = { 6, 21, 44, 64, 164, 160 };
    const s32 OnlinePreEvent::miNumEventsObserved  = 6;

    // The one apt package the screen loads (ON_PRE_EVENT).
    const CgsGui::sResourceTuple OnlinePreEvent::maResourcesToLoad[] =
    {
        { 185, CgsGui::E_GUI_RESOURCETYPE_APT }
    };
    const u32 OnlinePreEvent::muNumResourcesToLoad = 1;

    const char OnlinePreEvent::KAC_MESSAGES_COMPONENT_NAME[20] = "PreEventMessages_mc";

    namespace
    {
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;   // mpInGuiEventQueue's real type

        const s32 KI_CHANNEL_GUI_OUT = 40;

        const s32 KI_EVENT_APT_TRIGGER          = 21;
        const s32 KI_EVENT_NETWORK_DISCONNECTED = 44;
        const s32 KI_EVENT_GUI_CACHE            = 64;
        const s32 KI_EVENT_PRE_EVENT_FLY_BY     = 160;
        const s32 KI_EVENT_PRE_EVENT_ADVANCE    = 164;

        // The movie UpdateLoadResources plays: the screen's own package.
        const u32 KU_PRE_EVENT_MOVIE_RESOURCE = 185;
        const s32 KI_APT_MOVIE_LEVEL          = 3;
        const char* const KPC_EMPTY_STRING    = "";

        // The overlay a lost connection raises.
        const char KAC_LOST_CONNECTION_OVERLAY_ID[] = "OnLostConn";

        const char KAC_ADVANCE_EVENT[] = "ADVANCE";

        // ---- in-queue payload views (the queue hands out the header-stripped payload) ----
        // The gui-cache event (the assert text names its field mpCachePointer).
        struct GuiEventCachePayload : public CgsModule::Event
        {
            GuiCache* mpCachePointer;   // +0x00
        };

        // The fly-by event: how long the next message stays up.
        struct FlyByPayload : public CgsModule::Event
        {
            f32 mfDisplayTime;   // +0x00
        };

        // ---- out-queue wire records ------------------------------------------------------
        // { 288, 184, 16, <pad>, the 288-byte request }, channel 40, 304 bytes.
        struct GuiOverlayRequestWire : public CgsGui::GuiEvent<184>
        {
            u32               muPad0C;    // +0x0C
            GuiOverlayRequest mRequest;   // +0x10

            GuiOverlayRequestWire()
                : CgsGui::GuiEvent<184>(static_cast<u32>(sizeof(GuiOverlayRequest)), 16)
                , muPad0C(0)
            {
            }
        };

        static_assert(sizeof(GuiOverlayRequestWire) == 304, "overlay request record is 304 bytes");
    }

    // ================================================================================
    //  OnEnter
    // ================================================================================
    void OnlinePreEvent::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        mpGuiCache = 0;
        mMessageComponent.Construct(KAC_MESSAGES_COMPONENT_NAME, mpStateInterface, 0);

        miCurrentFlyByIndex = 0;
        meInternalState     = E_INTERNALSTATE_GETCACHE;
        mfTimeToRemove      = 0.0f;
    }

    // ================================================================================
    //  OnLeave
    // ================================================================================
    void OnlinePreEvent::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // The console inlines StateInterface::PlayAptMovie here: the empty name at level 3
        // clears the level.
        mpStateInterface->PlayAptMovie(KPC_EMPTY_STRING, KI_APT_MOVIE_LEVEL);

        meInternalState = E_INTERNALSTATE_LEFT;
    }

    // ================================================================================
    //  Update -- each step stores its state on entry and drops into the next one once it
    //  completes; the permanent handlers then run and the in-queue is cleared.
    // ================================================================================
    void OnlinePreEvent::Update()
    {
        switch (meInternalState)
        {
        case E_INTERNALSTATE_GETCACHE:
            UpdateGetCache();
            // fall through

        case E_INTERNALSTATE_LOADRESOURCES:
            meInternalState = E_INTERNALSTATE_LOADRESOURCES;
            if (!UpdateLoadResources())
            {
                break;
            }
            // fall through

        case E_INTERNALSTATE_WFINIT:
            meInternalState = E_INTERNALSTATE_WFINIT;
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

        reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue)->Clear();
    }

    // ================================================================================
    //  UpdateGetCache -- take the cache from the first gui-cache event in the in-queue.
    // ================================================================================
    void OnlinePreEvent::UpdateGetCache()
    {
        CGS_ASSERT(0 == mpGuiCache, "NULL == mpGuiCache");

        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            if (liEventId == KI_EVENT_GUI_CACHE)
            {
                const GuiEventCachePayload* lpCacheEvent =
                    static_cast<const GuiEventCachePayload*>(lpEvent);
                CGS_ASSERT(0 != lpCacheEvent->mpCachePointer,
                           "NULL != lpCacheEvent->mpCachePointer");
                mpGuiCache = lpCacheEvent->mpCachePointer;
                break;
            }
        }

        CGS_ASSERT(0 != mpGuiCache, "NULL != mpGuiCache");
    }

    // ================================================================================
    //  UpdateLoadResources -- load the package, then play it.
    // ================================================================================
    bool OnlinePreEvent::UpdateLoadResources()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        if (!mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad))
        {
            return false;
        }

        mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[KU_PRE_EVENT_MOVIE_RESOURCE],
                                       KI_APT_MOVIE_LEVEL);
        return true;
    }

    // ================================================================================
    //  UpdateRunning -- once the shown message's time is up, hide it and move on.
    // ================================================================================
    void OnlinePreEvent::UpdateRunning()
    {
        if (mMessageComponent.IsShowing())
        {
            if (!(mfTimeToRemove > mpGuiCache->GetTime()))
            {
                mfTimeToRemove = 0.0f;
                mMessageComponent.Hide();
                ++miCurrentFlyByIndex;
            }
        }
    }

    // ================================================================================
    //  UpdatePermanent
    // ================================================================================
    void OnlinePreEvent::UpdatePermanent()
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
            case KI_EVENT_APT_TRIGGER:
            {
                const CgsGui::GuiEventAptTriggerPayload* lpTrigger =
                    reinterpret_cast<const CgsGui::GuiEventAptTriggerPayload*>(lpEvent);
                if (lpTrigger->meEventType == CgsGui::GuiEventAptTrigger::E_APT_EVENT_ONLOAD)
                {
                    mMessageComponent.HandleLoadNotification(lpTrigger->mpacComponentName);
                }
                break;
            }

            case KI_EVENT_NETWORK_DISCONNECTED:
                CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");
                if (mpGuiCache->IsOnline())
                {
                    GuiOverlayRequestWire lLostConnectionOverlay;
                    lLostConnectionOverlay.mRequest.Construct(KAC_LOST_CONNECTION_OVERLAY_ID);
                    mpStateInterface->GetOutputEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lLostConnectionOverlay),
                        KI_CHANNEL_GUI_OUT, static_cast<s32>(sizeof(lLostConnectionOverlay)));
                }
                break;

            case KI_EVENT_PRE_EVENT_FLY_BY:
            {
                const FlyByPayload* lpFlyBy = static_cast<const FlyByPayload*>(lpEvent);
                mfTimeToRemove = lpFlyBy->mfDisplayTime + mpGuiCache->GetTime();
                mMessageComponent.Show(mpGuiCache->GetPreEventInfo(miCurrentFlyByIndex));
                break;
            }

            case KI_EVENT_PRE_EVENT_ADVANCE:
                SendStateEvent(KAC_ADVANCE_EVENT);
                break;

            default:
                break;
            }
        }
    }
}
