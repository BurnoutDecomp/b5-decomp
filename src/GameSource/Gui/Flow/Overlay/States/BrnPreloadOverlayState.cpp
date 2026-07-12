#include "GameSource/Gui/Flow/Overlay/States/BrnPreloadOverlayState.h"

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::StateInterface (Register/UnRegisterForEvents, PlayAptMovie)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"         // CgsModule::VariableEventQueue<18432,16> / Event (GetFirstEvent/GetNextEvent)
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache (EnsureResourcesAreLoaded)

// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   OnEnter  @0x824B1528 -- reset the internal step state to START and clear mpGuiCache, then
//                           register for the single GUI event the overlay observes.
//   OnLeave  @0x824B1550 -- unregister from that event.
//   Update   @0x824B2988 -- the preload step machine (see the function banner below).
//   GetCache @0x824B2070 -- scan the state's in-event queue (the protected CgsGui::State
//                           mpInGuiEventQueue) for the cache-ready event (event-type id 64); on
//                           receipt, latch the GuiCache pointer the event carries in its first
//                           word into mpGuiCache and return true. Returns false until it arrives.
//
// The .rdata/.data statics carry no values in the IDA exports; their real contents were read
// straight from the decrypted XEX (see the per-static comments).

namespace BrnGui
{

// The in-event queue event-type id that carries the resolved GUI cache (X360: cmpwi r,64).
static const s32 KI_GUI_CACHE_EVENT = 64;

// .data @0x82063CA4 == { 64 } (decrypted XEX; the following word @0x82063CA8 == 1 is
// miNumEventsObserved). The single observed event is the cache-ready event GetCache drains.
const s32 PreloadOverlayState::maiEventToObserve[1] = { KI_GUI_CACHE_EVENT };

const s32 PreloadOverlayState::miNumEventsObserved = 1;

// .rdata @0x82F269F8 (values read from the decrypted XEX): the nine flapt HD bundles the
// preload overlay pins before the overlay movie starts. Tuple [0] (id 125) is the "main"
// resource Update waits on first (WFLOADMAIN); [1..8] are the WFLOAD tail. Every entry
// requests type 7 == CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE. Resource id 197 is the
// "Overlays" flapt file (the GUI resource-name table @0x82F278E0 names it; Update plays
// the apt movie by that same name).
const CgsGui::sResourceTuple PreloadOverlayState::maResourcesToLoad[] =
{
    { 125, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },
    {  22, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },
    { 197, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },
    {  29, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },
    {  56, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },
    {  63, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },
    {  61, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },
    {  59, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },
    {  36, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },
};

// .rdata @0x82F269F4 == 9 (decrypted XEX; sits immediately before the tuple table).
const u32 PreloadOverlayState::muNumResourcesToLoad = 9;

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

// @ 0x824B2988 -- the preload step machine, one step per GUI-module tick with
// same-tick fallthrough on success:
//   START      -> GETCACHE   : wait for the cache-ready event (GetCache)
//   GETCACHE   -> WFLOADMAIN : wait for the main resource (tuple [0]) to load
//   WFLOADMAIN -> WFLOAD     : wait for the remaining muNumResourcesToLoad-1 tuples
//   WFLOAD     -> DONE       : play the "Overlays" apt movie on level 6 and send
//                              the "DONE" state event to the overlay FSM
// Every path (progress, stall or done) drains the state's in-event queue. The X360
// reads the movie name out of the GUI resource-name table (off_82F27BF4 == entry 197,
// "Overlays"); reconstructed as the literal that entry holds.
void PreloadOverlayState::Update()
{
    typedef CgsModule::VariableEventQueue<18432, 16> GuiInEventQueue;

    switch (meInternalState)
    {
    case E_OVERLAYINTERNALSTATE_START:
        meInternalState = E_OVERLAYINTERNALSTATE_START;
        // fall through
    case E_OVERLAYINTERNALSTATE_GETCACHE:
        meInternalState = E_OVERLAYINTERNALSTATE_GETCACHE;
        if (!GetCache())
        {
            break;
        }
        // fall through
    case E_OVERLAYINTERNALSTATE_WFLOADMAIN:
        meInternalState = E_OVERLAYINTERNALSTATE_WFLOADMAIN;
        if (!mpGuiCache->EnsureResourcesAreLoaded(&maResourcesToLoad[0], 1))
        {
            break;
        }
        // fall through
    case E_OVERLAYINTERNALSTATE_WFLOAD:
        meInternalState = E_OVERLAYINTERNALSTATE_WFLOAD;
        if (!mpGuiCache->EnsureResourcesAreLoaded(&maResourcesToLoad[1],
                                                  muNumResourcesToLoad - 1))
        {
            break;
        }
        mpStateInterface->PlayAptMovie("Overlays", 6);
        SendStateEvent("DONE");
        // fall through
    case E_OVERLAYINTERNALSTATE_DONE:
        meInternalState = E_OVERLAYINTERNALSTATE_DONE;
        break;
    default:
        // cpp:141 -- the X360 streams "Invalid State : " + meInternalState + " !";
        // folded static per convention.
        CGS_ASSERT(false, "Invalid State : ");
        break;
    }

    reinterpret_cast<GuiInEventQueue*>(mpInGuiEventQueue)->Clear();
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
