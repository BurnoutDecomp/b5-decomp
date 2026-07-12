// ===================================================================================
// BrnGui::OnlinePreEvent -- out-of-line bodies + statics for the online pre-event
// ("ready up" / fly-by) screen state.
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   OnEnter             @0x824872D8
//   OnLeave             @0x824A0EB0
//   UpdateGetCache      @0x824925C0
//   UpdateLoadResources @0x824A0F40
//   UpdatePermanent     @0x824A1020
//   UpdateRunning       @0x82487360
//
// (Update / UpdateWFInit / HandleControllerInputPressed / HandleAptTriggers are declared
//  in the header for the class shape; their bodies land with their own ledger slices.)
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlinePreEvent.h"

#include <cfloat>   // FLT_MAX (idle-time sentinel)

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::VariableEventQueue / Event
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface (register / play-apt / output)
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiOverlayRequest

namespace BrnGui
{
    // ---- statics (DWARF cpp:26-46; .rdata) ----------------------------------------
    // The 6 event ids this state registers for (X360 @0x8205F978). Values not recovered
    // from the .rdata blob; FLAG: zero-filled per the OnlineNews sibling precedent.
    const s32 OnlinePreEvent::maiEventToObserve[6] = { 0, 0, 0, 0, 0, 0 };
    const s32 OnlinePreEvent::miNumEventsObserved  = 6;

    // The single apt resource the pre-event screen loads (X360 @0x8205F994, count 1).
    const CgsGui::sResourceTuple OnlinePreEvent::maResourcesToLoad[] =
    {
        { 0u, CgsGui::E_GUI_RESOURCETYPE_APT },   // FLAG: resource id unattested in scope
    };
    const u32 OnlinePreEvent::muNumResourcesToLoad = 1;

    // The messages component name passed to GuiComponent::Construct (X360 aPreeventmessag).
    const char OnlinePreEvent::KAC_MESSAGES_COMPONENT_NAME[20] = "PreEventMessages_mc";

    // Idle sentinel for mfTimeToRemove. Must exceed any GetTime() so UpdateRunning does not
    // re-fire the hide once IsShowing() is cleared (X360 rodata flt_82001CC0). FLAG: exact
    // rodata value not recovered; FLT_MAX is the control-flow-implied "no time scheduled".
    const f32 OnlinePreEvent::KF_FLYBY_IDLE_TIME = FLT_MAX;

    // The state's in-event queue (State +0x18) is the DWARF InputBuffer::GuiEventQueue, an
    // incomplete alias for this concrete queue instantiation the X360 walks by name.
    typedef CgsModule::VariableEventQueue<18432, 16> InGuiEventQueue;

    // ---- OnEnter @ 0x824872D8 -----------------------------------------------------
    void OnlinePreEvent::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        mpGuiCache = NULL;   // +0x38 (cleared before the component is constructed)
        mMessageComponent.Construct(KAC_MESSAGES_COMPONENT_NAME, mpStateInterface, NULL);

        miCurrentFlyByIndex = 0;                          // +0x44C
        meInternalState     = E_INTERNALSTATE_GETCACHE;   // +0x3C (0)
        mfTimeToRemove      = KF_FLYBY_IDLE_TIME;          // +0x448
    }

    // ---- OnLeave @ 0x824A0EB0 -----------------------------------------------------
    void OnlinePreEvent::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // X360: an inlined PlayAptMovie (type-18 view-state event, level 3) whose movie name
        // is the rodata string @0x820046A7. FLAG: that string is not recovered in this slice.
        mpStateInterface->PlayAptMovie(/* FLAG: apt movie name @0x820046A7 not recovered */ "", 3);

        meInternalState = E_INTERNALSTATE_LEFT;   // +0x3C (4)
    }

    // ---- UpdateGetCache @ 0x824925C0 ----------------------------------------------
    // Scan the in-event queue for the GuiCache event (type 64) and latch its carried cache
    // pointer into mpGuiCache.
    void OnlinePreEvent::UpdateGetCache()
    {
        CGS_ASSERT(mpGuiCache == NULL, "NULL == mpGuiCache");

        InGuiEventQueue* lpInQueue = reinterpret_cast<InGuiEventQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = NULL;
        s32 liSize = 0;
        s32 liEventType = lpInQueue->GetFirstEvent(&lpEvent, &liSize);

        if (lpEvent != NULL)
        {
            while (liEventType != 64)
            {
                liEventType = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
                if (lpEvent == NULL)
                    break;
            }

            if (lpEvent != NULL)
            {
                // The cache event carries the GuiCache pointer in its leading word.
                GuiCache* lpCache = *reinterpret_cast<GuiCache* const*>(lpEvent);
                CGS_ASSERT(lpCache != NULL, "NULL != lpCacheEvent->mpCachePointer");
                mpGuiCache = lpCache;
            }
        }

        CGS_ASSERT(mpGuiCache != NULL, "NULL != mpGuiCache");
    }

    // ---- UpdateLoadResources @ 0x824A0F40 -----------------------------------------
    // Once the cache reports the pre-event resources loaded, play the "ON_PRE_EVENT" movie.
    bool OnlinePreEvent::UpdateLoadResources()
    {
        CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");

        if (!mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad))
            return false;

        mpStateInterface->PlayAptMovie("ON_PRE_EVENT", 3);
        return true;
    }

    // ---- UpdatePermanent @ 0x824A1020 ---------------------------------------------
    // Drain the in-event queue every frame, acting on the four handled event types.
    void OnlinePreEvent::UpdatePermanent()
    {
        InGuiEventQueue* lpInQueue = reinterpret_cast<InGuiEventQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = NULL;
        s32 liSize = 0;
        s32 liEventType = lpInQueue->GetFirstEvent(&lpEvent, &liSize);

        while (lpEvent != NULL)
        {
            switch (liEventType)
            {
                case 21:   // apt-component load notification
                {
                    if (*reinterpret_cast<const s32*>(lpEvent) == 1)
                    {
                        const char* lpacComponentName = *reinterpret_cast<const char* const*>(
                            reinterpret_cast<const u8*>(lpEvent) + 8);
                        mMessageComponent.HandleLoadNotification(lpacComponentName);
                    }
                    break;
                }
                case 44:   // network connection lost
                {
                    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");
                    // X360 reads the cache byte @+0x4B4C (the committed online-start flag) to
                    // gate the lost-connection overlay to an active online session.
                    if (mpGuiCache->IsOnlineStartInProgress())
                    {
                        GuiOverlayRequest lOverlayRequest;
                        lOverlayRequest.Construct("OnLostConn");
                        mpStateInterface->OutputGuiEvent(lOverlayRequest);
                    }
                    break;
                }
                case 160:  // show the next timed fly-by message
                {
                    // Schedule the removal at (event display-time + now); GetTime() carries the
                    // inlined "mfTimeNow != -FLT_MAX" assert (CgsGuiEventTypeDefs.h:250).
                    const f32 lfDisplayTime = *reinterpret_cast<const f32*>(lpEvent);
                    mfTimeToRemove = lfDisplayTime + mpGuiCache->GetTime();

                    const PreEventInfo* lpInfo = mpGuiCache->GetPreEventInfo(miCurrentFlyByIndex);
                    mMessageComponent.Show(lpInfo);
                    break;
                }
                case 164:  // line-up finished -> advance the flow
                {
                    SendStateEvent("ADVANCE");
                    break;
                }
                default:
                    break;
            }

            liEventType = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
        }
    }

    // ---- UpdateRunning @ 0x82487360 -----------------------------------------------
    // Hide the current fly-by message once its scheduled removal time is reached.
    void OnlinePreEvent::UpdateRunning()
    {
        if (mMessageComponent.IsShowing())
        {
            if (mfTimeToRemove <= mpGuiCache->GetTime())
            {
                mfTimeToRemove = KF_FLYBY_IDLE_TIME;
                mMessageComponent.Hide();
                ++miCurrentFlyByIndex;
            }
        }
    }
}
