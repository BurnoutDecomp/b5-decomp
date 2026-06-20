#include "GameSource/Gui/BrnGuiModule.h"

// The initial loading-screen signal (MainGameFlowStateInitialLoadingScreen raises it on entry and drops it
// at FinishLoading). The boot videos gate on its completion so the loading screen shows before the logos.
extern bool gBrnLoadingScreenShouldShow;

// BrnGui::GuiModule -- the GUI module (minimal movie-hosting slice; see BrnGuiModule.h). X360
// GuiModule::Construct (0x82518028) builds the whole GUI subsystem + the embedded MovieManager; this
// reconstructs only the MovieManager host + lifecycle. The base ModuleSingleBuffered stages (IO buffers /
// data structures) run as for any dispatched module (the BrnRendererModule pattern). (The isolated
// ProfileHost::HandleProfileTaskResult decomp lives in BrnGuiProfileHost.cpp.)

namespace BrnGui
{
    // NOTE: this minimal GuiModule is driven DIRECTLY by BrnGameModule (Construct/Prepare/Update called
    // inline), not through the module dispatch, so it does NOT run the base ModuleSingleBuffered lifecycle.
    // The base Prepare() builds the module's input/output DataStructures via CreateInputDataStructure, which
    // asserts ("new module type - can't lock/unlock") for a module that hasn't declared them -- and the movie
    // slice needs none of that IO. (The real X360 GuiModule is double-buffered with full IO; that's the
    // follow-on when the GUI runs under the real dispatch.)
    void GuiModule::Construct()
    {
        mMovieManager.Construct();
        mbBootStarted = false;
        mbLoadingHasShown = false;
    }

    bool GuiModule::Prepare()
    {
        // Load VIDEOS\VIDEOLIST.BUNDLE synchronously (English; see MovieManager::Prepare) and publish the
        // manager so the renderer draws the active movie each frame (interim render bridge; the X360 renders
        // it through the GUI's own ViewIO ImRenderers via UpdateAndRenderMovieManager 0x82511240).
        mMovieManager.Prepare(0);
        gpActiveMovieManager = &mMovieManager;

        // Stand up the boot-logo flow: BootVideos emits play/stop through mBootStateInterface and reads its
        // input events from mBootInQueue (which this module feeds cache-ready + video-finished).
        mBootInQueue.Construct();
        mBootStateInterface.Construct();
        mBootVideos.SetStateInterface(&mBootStateInterface);
        mBootVideos.SetInEventQueue(reinterpret_cast<InputBuffer::GuiEventQueue*>(&mBootInQueue));
        mBootVideos.OnEnter();
        mbBootStarted = false;
        mbLoadingHasShown = false;
        return true;
    }

    bool GuiModule::Release()
    {
        gpActiveMovieManager = 0;
        mMovieManager.Release();
        return true;
    }

    void GuiModule::Destruct()
    {
        mMovieManager.Destruct();
    }

    // X360 GuiModule::Update (0x82527A58) ticks the GUI model + the HUD/Screen flows + the MovieManager.
    // Here it runs the boot-logo flow (BootVideos) bridged to the MovieManager.
    void GuiModule::Update()
    {
        UpdateBootVideoFlow();
    }

    // Drive the boot-logo flow for one frame: feed BootVideos its events, tick it, deliver its output to
    // the MovieManager, tick the manager, and feed video-finished back. [MINIMAL boot driver -- the X360
    // runs BootVideos inside BrnHudFlow and routes via the EventObserver; this bridges the queues directly.]
    void GuiModule::UpdateBootVideoFlow()
    {
        // 1. Feed cache-ready once -- but only after the initial loading screen has been shown AND completed.
        //    This stands in for the boot FSM's BootPreload phase: in the real flow BootPreload runs first
        //    (the loading screen is up while boot resources stream), and BootVideos only plays once the
        //    cache is ready. The bundle itself loaded synchronously in Prepare, so the gate here is the
        //    initial-loading-screen completion (MainGameFlowStateInitialLoadingScreen drops
        //    gBrnLoadingScreenShouldShow at FinishLoading) -- so the user sees the loading screen first,
        //    THEN the EA/Criterion logos. [follow-on: drive this off the real BootPreload state.]
        if (gBrnLoadingScreenShouldShow)
            mbLoadingHasShown = true;
        const bool lbLoadingComplete = mbLoadingHasShown && !gBrnLoadingScreenShouldShow;
        if (!mbBootStarted && lbLoadingComplete)
        {
            CgsModule::Event lReadyEvent;
            mBootInQueue.AddEvent(&lReadyEvent, 64, static_cast<s32>(sizeof(lReadyEvent)));
            mbBootStarted = true;
        }

        // 2. Tick the boot state (reads mBootInQueue, emits play/stop onto mBootStateInterface's output).
        mBootVideos.Update();

        // 3. Deliver the state's output play(508)/stop(509) to the MovieManager's receiver queue.
        CgsGui::GuiStackEventQueue::GuiEventQueueLarge* lpOutQueue = mBootStateInterface.GetOutputEventQueue();
        if (lpOutQueue != 0)
        {
            // Drive the queue through its VariableEventQueue base (inline) -- GuiEventQueueBase's own
            // Clear/GetFirstEvent/GetNextEvent are declared-only.
            CgsModule::VariableEventQueue<65536, 16>* lpOutBase = lpOutQueue;
            const CgsModule::Event* lpEvent = 0;
            s32 liSize = 0;
            s32 liId = lpOutBase->GetFirstEvent(&lpEvent, &liSize);
            while (liId >= 0 && lpEvent != 0)
            {
                if (liId == KI_GUIEVENT_PLAY_VIDEO || liId == KI_GUIEVENT_STOP_VIDEO)
                    mMovieManager.GetReceiverQueue()->AddEvent(lpEvent, liId, liSize);
                const CgsModule::Event* lpNext = 0;
                liId = lpOutBase->GetNextEvent(lpEvent, &lpNext, &liSize);
                lpEvent = lpNext;
            }
            lpOutBase->Clear();
        }

        // 3b. Dispatch the MovieManager's receiver queue to RecvEvent. In the X360 the module event
        //     dispatcher drains the EventReceiverQueue<1024,16> into MovieManager::RecvEvent each frame;
        //     this synchronous bridge does the same (otherwise the 508/509 we just queued never reach
        //     HandlePlayVideoEvent, so nothing is ever queued and the manager sits in IDLE forever).
        {
            CgsModule::VariableEventQueue<1024, 16>* lpRecv = mMovieManager.GetReceiverQueue();
            const CgsModule::Event* lpEvent = 0;
            s32 liSize = 0;
            s32 liId = lpRecv->GetFirstEvent(&lpEvent, &liSize);
            while (liId >= 0 && lpEvent != 0)
            {
                mMovieManager.RecvEvent(lpEvent, liId);
                const CgsModule::Event* lpNext = 0;
                liId = lpRecv->GetNextEvent(lpEvent, &lpNext, &liSize);
                lpEvent = lpNext;
            }
            lpRecv->Clear();
        }

        // 4. Tick the MovieManager.
        mMovieManager.Update();

        // 5. When the manager parks in REPORTING_FINISHED (a video ended), feed video-finished (510) back to
        //    BootVideos and return the manager to IDLE so the next queued video plays. (The X360 GUI flow
        //    owns this acknowledge+reset; Update itself keeps the manager in REPORTING_FINISHED. Resetting
        //    here is what lets the EA logo be followed by Criterion -- without it the manager sticks at
        //    REPORTING_FINISHED and the queued Criterion is never picked up.)
        if (mMovieManager.HasFinishedReporting())
        {
            CgsModule::Event lFinishedEvent;
            mBootInQueue.AddEvent(&lFinishedEvent, 510, static_cast<s32>(sizeof(lFinishedEvent)));
            mMovieManager.AcknowledgeFinishedAndReturnToIdle();
        }
    }
}
