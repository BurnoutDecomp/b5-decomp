#include "GameSource/Gui/Flow/HUD/States/BrnBootAttract.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // the state in-queue
#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Gui/Flow/HUD/States/BrnBootAttract.cpp"
// Reconstructed from BURNOUT_X360_ARTIST.XEX (OnEnter @0x82473048, OnLeave @0x82473060,
// Update @0x82476608). BootAttract registers for the single GUI event it watches when
// the attract state is entered, drains its in-queue each frame (tolerating only that
// event), and unregisters on leave.

namespace BrnGui
{
    namespace
    {
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;
    }

    const s32 BootAttract::miNumEventsObserved = 1;

    // The observed-event id table lives in .rdata (@0x8205A8EC, no exported value).
    // The Update body @0x82476608 tolerates exactly event 14 (everything else fires
    // "Unexpected event"), so the registered id is pinned from that dispatch.
    const s32 BootAttract::maiEventToObserve[] = { 14 };

    // @ 0x82473048
    void BootAttract::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);
    }

    // @ 0x82473060
    void BootAttract::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
    }

    // FLAG (unrecovered .rdata): BootAttract's resource-tuple table is data the exports
    // do not carry (no per-address export exists for this override). Empty until recovered;
    // the PC cache boundary treats boot resources as resident (synchronous loads).
    void BootAttract::GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                         u32* lpuNumberOfResources) const
    {
        *lppResourceTuples    = 0;
        *lpuNumberOfResources = 0;
    }

    // @ 0x82476608 -- drain the in-queue (only the observed event may arrive), then clear it.
    void BootAttract::Update()
    {
        // DWARF cpp:73..75 locals; the queue is read through the member at each use and is
        // not null-tested (ARTIST 0x82476620 / 0x82476668 / 0x82476684 lwz 0x18(this)).
        const CgsModule::Event* lpEvent;
        s32 liEventSize;
        for (s32 liEventId = mpInGuiEventQueue->GetFirstEvent(&lpEvent, &liEventSize);
             lpEvent != 0;
             liEventId = mpInGuiEventQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize))
        {
            if (liEventId != 14)
                CGS_ASSERT_AT(false, "Unexpected event in IntroHudState::Update", CGS_ARTIST_FILE, 91);
        }
        mpInGuiEventQueue->Clear();
    }
}
