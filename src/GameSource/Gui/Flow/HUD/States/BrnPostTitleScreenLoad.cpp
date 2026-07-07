#include "GameSource/Gui/Flow/HUD/States/BrnPostTitleScreenLoad.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameSource/Gui/BrnGuiVideoEvents.h"

// BrnGui::PostTitleScreenLoad::GetResourcesToLoad, reconstructed from BURNOUT_X360_ARTIST.XEX
// @ 0x825080B0 (semantic parity, not byte match).
//
// X360 body: the function ignores both out-params and unconditionally runs the assert sequence
//   BeginAssert(); FireAssert("Should not get here", "<...>/BrnPostTitleScreenLoad.h", 63); EndAssert();
// then returns -- a "Should not get here" tripwire. This state is driven by its own
// mpGuiCache / video bookkeeping and is never asked for resources through the FSM's
// sResourceTuple loading path, so the override exists only to flag misuse. (Contrast
// BrnGui::BootLegal::GetResourcesToLoad @ 0x82508090, which returns a real .rdata table.)
//
// The Hex-Rays render drops the two pointer args because the body never touches them; the asm
// signature is the CgsGui::State virtual `void GetResourcesToLoad(const sResourceTuple**, u32*)`.
// Under CGS_ASSERT the original's d:\p4-baked file/line is dropped per policy; the plain
// condition string is forwarded. The assert is unconditional, so it is modelled as CGS_ASSERT(false, ...).

namespace BrnGui
{
    namespace
    {
        enum
        {
            KI_EVENT_UNLOAD_OR_STOP = 6,
            KI_EVENT_GUI_CACHE      = 64,
            KI_EVENT_VIDEO_FINISHED = 510,
        };

        struct GuiEventCache : public CgsModule::Event
        {
            GuiCache* mpGuiCache;
        };

        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;
    }

    const s32 PostTitleScreenLoad::maiEventToObserve[PostTitleScreenLoad::miNumEventsObserved] =
    {
        KI_EVENT_UNLOAD_OR_STOP,
        KI_EVENT_GUI_CACHE,
        KI_EVENT_VIDEO_FINISHED,
    };

    void PostTitleScreenLoad::OnEnter()
    {
        mpGuiCache = 0;
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);
        meState = E_IDLE;
    }

    void PostTitleScreenLoad::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
    }

    void PostTitleScreenLoad::HandleIncomingEvents()
    {
        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        StateInputQueue* lpQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);

        for (s32 liEventId = lpQueue->GetFirstEvent(&lpEvent, &liEventSize);
             lpEvent != 0;
             liEventId = lpQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize))
        {
            switch (liEventId)
            {
            case KI_EVENT_UNLOAD_OR_STOP:
                if (meState == E_PLAYING_VIDEO)
                {
                    GuiEventStopVideo lStopVideoEvent;
                    mpStateInterface->OutputGuiEvent(lStopVideoEvent);
                }
                break;

            case KI_EVENT_GUI_CACHE:
                if (mpGuiCache == 0)
                    mpGuiCache = static_cast<const GuiEventCache*>(lpEvent)->mpGuiCache;
                break;

            case KI_EVENT_VIDEO_FINISHED:
                mbVideoFinished = true;
                break;

            default:
                CGS_ASSERT(false, "Unhandled event");
                break;
            }
        }
    }

    void PostTitleScreenLoad::GetResourcesToLoad(const CgsGui::sResourceTuple** /*lppResourceTuples*/,
                                                 u32* /*lpuNumberOfResources*/) const
    {
        CGS_ASSERT(false, "Should not get here");
    }
}
