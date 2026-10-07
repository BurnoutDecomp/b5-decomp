// ===================================================================================
// BrnGui::OnlineScoreboards -- wave-I partfile 02: the GuiCache handoff and the
// expected-apt-component bookkeeping the screen's internal-state machine runs before it
// can drive its components.
//
//   ClearExpectedComponent @0x82486928  (asserts cpp:430)
//   UpdateWFInit           @0x824869A0
//   UpdateGetCache         @0x824917E0  (asserts cpp:473 / 483 / 492)
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX. The committed twin for the
// cache handoff is BrnCarSelectUnlock.cpp:118-150 (same three assert strings, same queue
// walk); this file matches its idiom.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineScoreboards.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::Event / the in-queue
#include "GameSource/Gui/BrnGuiCache.h"                           // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                   // BrnGui::GuiFlow / E_GUIFLOW_SCREEN

#include <cstring>                                                // std::memset

namespace BrnGui
{
    namespace
    {
        // The state in-queue (CgsGui::State::mpInGuiEventQueue's real instantiation -- the
        // base holds it as the incomplete InputBuffer::GuiEventQueue alias).
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // The GuiCache handoff event: id 64, maiEventToObserve[5]. Its payload is the bare
        // cache pointer (the queue delivers header-stripped payloads).
        const s32 KI_EVENT_GUI_CACHE = 64;
    }

    // ---- ClearExpectedComponent @ 0x82486928 (DWARF cpp:420) ----------------------
    // Drop the flow layer's expected-apt-component watch list, then reset this screen's own
    // pending-id bookkeeping.
    void OnlineScoreboards::ClearExpectedComponent()
    {
        CGS_ASSERT(mpGuiCache, "mpGuiCache");   // cpp:430 (non-fatal -- the call below runs regardless)

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);

        // X360 emits five unrolled word stores (this+0x40..0x50): the four id slots then the
        // live count. Re-rolled; the span is the host sizeof of the array, never a console byte count.
        std::memset(mauExpectedComponentIds, 0, sizeof(mauExpectedComponentIds));
        muNumExpectedComponents = 0;
    }

    // ---- UpdateWFInit @ 0x824869A0 (DWARF cpp:497) --------------------------------
    // The WFINIT arm of the internal-state machine: hold until every expected apt component
    // on the screen flow layer has finished initialising, then release the watch list.
    bool OnlineScoreboards::UpdateWFInit()
    {
        if (!mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
        {
            return false;
        }

        ClearExpectedComponent();
        return true;
    }

    // ---- UpdateGetCache @ 0x824917E0 (DWARF cpp:459) ------------------------------
    // The GETCACHE arm of the internal-state machine: scan the in-event queue for the
    // GuiCache event (type 64) and latch its carried cache pointer into mpGuiCache.
    void OnlineScoreboards::UpdateGetCache()
    {
        CGS_ASSERT(mpGuiCache == NULL, "NULL == mpGuiCache");   // cpp:473 (the cache must still be unset here)

        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = NULL;
        s32 liSize = 0;
        s32 liEventType = lpInQueue->GetFirstEvent(&lpEvent, &liSize);

        if (lpEvent != NULL)
        {
            while (liEventType != KI_EVENT_GUI_CACHE)
            {
                liEventType = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
                if (lpEvent == NULL)
                    break;
            }

            if (lpEvent != NULL)
            {
                // The cache event carries the GuiCache pointer in its leading word.
                GuiCache* lpCache = *reinterpret_cast<GuiCache* const*>(lpEvent);
                CGS_ASSERT(lpCache != NULL, "NULL != lpCacheEvent->mpCachePointer");   // cpp:483

                // X360 stores unconditionally, after the (non-fatal) assert above.
                mpGuiCache = lpCache;
            }
        }

        CGS_ASSERT(mpGuiCache != NULL, "NULL != mpGuiCache");   // cpp:492
    }
}
