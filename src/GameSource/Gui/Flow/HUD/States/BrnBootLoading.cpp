#include "GameSource/Gui/Flow/HUD/States/BrnBootLoading.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX. BootLoading registers for the two GUI
// events it watches on enter (and clears its screen-playing flag), then unregisters
// and stops the loading screen on leave. The observed-event id table lives in .rdata
// (@0x8205ABF8) and resolves at link time. The X360 inlined StateInterface's
// StopLoadingScreen body (a type-20 event on channel 40, size 16); the named call is
// behaviour-identical.

namespace BrnGui
{
    const s32 BootLoading::miNumEventsObserved = 2;

    // @ 0x82474508
    void BootLoading::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);
        mbScreenPlaying = false;
    }

    // @ 0x82478E88
    void BootLoading::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
        mpStateInterface->StopLoadingScreen();
    }
}
