#include "GameSource/Gui/Flow/HUD/States/BrnBootLoading.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::StateInterface
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::VariableEventQueue (in-queue)

// Reconstructed from BURNOUT_X360_ARTIST.XEX (BootLoading OnEnter 0x82474508 / Update 0x82478DC0 /
// OnLeave 0x82478E88). BootLoading is the boot loading-screen GUI state: it registers for the two
// GUI events it watches (loading-complete 137 + cache-ready 64), shows the loading screen once the
// cache is ready, and on loading-complete asks the FSM to proceed; on leave it unregisters and stops
// the loading screen. The observed-event id table is the X360 .rdata @0x8205ABF8 = { 137, 64 }.
// The X360 inlines the show/stop loading-screen events (channel 40, GuiEventPlayAptLoadingMovie type
// 19 / GuiEventStopAptLoadingMovie type 20); StateInterface::PlayLoadingScreen/StopLoadingScreen are
// the behaviour-identical named wrappers.

namespace BrnGui
{
    // X360 .rdata @0x8205ABF8 -- the two events BootLoading observes while active.
    const s32 BootLoading::maiEventToObserve[2] = { 137, 64 };
    const s32 BootLoading::miNumEventsObserved  = 2;

    namespace
    {
        const s32 KI_EVENT_LOADING_COMPLETE = 137;   // the loading sequence finished -> proceed
        const s32 KI_EVENT_CACHE_READY      = 64;    // the boot resource/cache is ready -> show the screen
        // CgsGui::State stores its input queue as the opaque InputBuffer::GuiEventQueue; the concrete
        // type is the GUI VariableEventQueue<18432,16> (matches BootVideos / the X360 a1+24 GetFirstEvent).
        typedef CgsModule::VariableEventQueue<18432, 16> BootInQueue;
    }

    // @ 0x82474508
    void BootLoading::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);
        mbScreenPlaying = false;
    }

    // @ 0x82478DC0 -- drain the input queue: on cache-ready show the loading screen (once); on
    // loading-complete send the "BF_PROCEED" state event so the FSM advances out of BF_LOADING.
    void BootLoading::Update()
    {
        BootInQueue* lpInQueue = reinterpret_cast<BootInQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
        while (lpEvent != 0)
        {
            if (liEventId == KI_EVENT_CACHE_READY)
            {
                if (!mbScreenPlaying)
                {
                    mpStateInterface->PlayLoadingScreen();   // X360: event 40 / GuiEventPlayAptLoadingMovie (19)
                    mbScreenPlaying = true;
                }
            }
            else if (liEventId == KI_EVENT_LOADING_COMPLETE)
            {
                SendStateEvent("BF_PROCEED");
            }

            const CgsModule::Event* lpNext = 0;
            liEventId = lpInQueue->GetNextEvent(lpEvent, &lpNext, &liSize);
            lpEvent = lpNext;
        }
        lpInQueue->Clear();
    }

    // @ 0x82478E88
    void BootLoading::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
        mpStateInterface->StopLoadingScreen();
    }
}
