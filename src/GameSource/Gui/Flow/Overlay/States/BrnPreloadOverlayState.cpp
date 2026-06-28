#include "GameSource/Gui/Flow/Overlay/States/BrnPreloadOverlayState.h"

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::StateInterface (Register/UnRegisterForEvents)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"         // CgsModule::VariableEventQueue<18432,16> / Event (GetFirstEvent/GetNextEvent)
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT

// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   OnEnter  @0x824B1528 -- reset the internal step state to START and clear mpGuiCache, then
//                           register for the single GUI event the overlay observes.
//   OnLeave  @0x824B1550 -- unregister from that event.
//   GetCache @0x824B2070 -- scan the state's in-event queue (the protected CgsGui::State
//                           mpInGuiEventQueue) for the cache-ready event (event-type id 64); on
//                           receipt, latch the GuiCache pointer the event carries in its first
//                           word into mpGuiCache and return true. Returns false until it arrives.

namespace BrnGui
{

// The in-event queue event-type id that carries the resolved GUI cache (X360: cmpwi r,64).
static const s32 KI_GUI_CACHE_EVENT = 64;

const s32 PreloadOverlayState::miNumEventsObserved = 1;

// @ 0x824B1528
void PreloadOverlayState::OnEnter()
{
    meInternalState = E_OVERLAYINTERNALSTATE_START;
    mpGuiCache      = NULL;
    mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);
}

// @ 0x824B1550
void PreloadOverlayState::OnLeave()
{
    mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
}

// @ 0x824B2070
bool PreloadOverlayState::GetCache()
{
    CGS_ASSERT(mpGuiCache == NULL, "mpGuiCache == NULL");

    // CgsGui::State stores the in-event queue as an opaque InputBuffer::GuiEventQueue* (a
    // forward-decl, to avoid a header cascade), but the X360 in-event queue is concretely a
    // VariableEventQueue<18432,16> -- proven by the recovered VariableEventQueue<18432,16>::
    // GetFirstEvent callee. Recover that real type to read the queue by its API.
    typedef CgsModule::VariableEventQueue<18432, 16> GuiInEventQueue;
    GuiInEventQueue* lpQueue = reinterpret_cast<GuiInEventQueue*>(mpInGuiEventQueue);

    const CgsModule::Event* lpEvent = NULL;
    s32 liEventSize = 0;
    s32 liEventType = lpQueue->GetFirstEvent(&lpEvent, &liEventSize);

    if (lpEvent == NULL)
    {
        return false;
    }

    while (liEventType != KI_GUI_CACHE_EVENT)
    {
        liEventType = lpQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize);
        if (lpEvent == NULL)
        {
            return false;
        }
    }

    // The cache-ready event carries the resolved GuiCache pointer in its first word.
    GuiCache* lpGuiCache = *reinterpret_cast<GuiCache* const*>(lpEvent);
    CGS_ASSERT(lpGuiCache != NULL, "Invalid gui cached");
    mpGuiCache = lpGuiCache;
    return true;
}

} // namespace BrnGui
