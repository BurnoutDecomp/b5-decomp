#include "GameSource/Gui/Flow/HUD/States/BrnBootAttract.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // the state in-queue
#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT

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
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        if (lpInQueue == 0)
            return;

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            if (liEventId != 14)
                CGS_ASSERT(false, "Unexpected event in IntroHudState::Update");
        }
        lpInQueue->Clear();
    }
}
