// ===================================================================================
// PARKED (merged drop-in form) -- BrnGui::OnlineScoreboards wave-I partfile 02:
// the GuiCache / expected-apt-component ladder.
//   ClearExpectedComponent @0x82486928  cpp:420
//   UpdateWFInit           @0x824869A0  cpp:497
//   UpdateGetCache         @0x824917E0  cpp:459
//
//   destination: b5-decomp/src/GameSource/Gui/Flow/Screen/States/BrnOnlineScoreboards_wI_02.cpp
//
// WHY PARKED: the committed leaf header BrnOnlineScoreboards.h is still the MINIMAL
// pre-wave version (29 lines: the GetResourcesToLoad inline plus the two resource
// statics). None of the spec's full-shape class has been applied, and headers are
// frozen for implementers, so none of these three bodies can be declared as members
// nor name mpGuiCache / mauExpectedComponentIds / muNumExpectedComponents. Measured
// with the cl /c gate, not assumed -- see the per-function parked banners.
//
// EXACT DECLARATION LINES THAT WOULD UNBLOCK THIS PARTFILE
// --------------------------------------------------------
// b5-decomp/src/GameSource/Gui/Flow/Screen/States/BrnOnlineScoreboards.h, inside
// `namespace BrnGui`, above the class:
//
//         class GuiCache;                      // pointer-only member (BrnGuiCache.h:198)
//
// ...and inside `struct OnlineScoreboards`, private section:
//
//         void UpdateGetCache();               // @0x824917E0  DWARF cpp:459
//         bool UpdateWFInit();                 // @0x824869A0  DWARF cpp:497
//         void ClearExpectedComponent();       // @0x82486928  DWARF cpp:420
//
//         static const u32 KU_MAX_INIT_COMPONENTS_NUM = 4;   // DWARF h:139
//         GuiCache* mpGuiCache;                              // DWARF h:130, X360 +56
//         u32 mauExpectedComponentIds[KU_MAX_INIT_COMPONENTS_NUM];  // DWARF h:140, X360 +64
//         u32 muNumExpectedComponents;                       // DWARF h:141, X360 +80
//
// No other declaration is required by THESE bodies: GuiCache::ClearExpectedAptComponentList
// (BrnGuiCache.h:251), GuiCache::AreAllAptComponentsInitialised (BrnGuiCache.h:244),
// BrnGui::E_GUIFLOW_SCREEN (BrnGuiEventTypeDefs.h:74), CgsGui::State::mpInGuiEventQueue
// (CgsGuiState.h:53, protected) and CgsModule::VariableEventQueue<18432,16>::
// GetFirstEvent / GetNextEvent (CgsVariableEventQueue.h:68-69) are all already committed.
//
// LINK NOTE for the conductor: nothing here is link-blocked. GuiCache::
// AreAllAptComponentsInitialised (BrnGuiCache.cpp:770) and GuiCache::
// ClearExpectedAptComponentList (BrnGuiCache.cpp:775) are both DEFINED in the tree, each
// forwarding to StateLoadingHelper (BrnGuiCache.cpp:615/633). GetFirstEvent / GetNextEvent
// are template bodies in CgsVariableEventQueue.h:331/361. The only repo-wide gap is the
// CgsDev::Assert trio behind CGS_ASSERT (declared CgsAssert.h, defined nowhere) -- a
// pre-existing condition shared by every committed file that asserts.
// ===================================================================================

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
