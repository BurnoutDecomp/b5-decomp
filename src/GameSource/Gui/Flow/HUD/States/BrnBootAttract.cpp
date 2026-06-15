#include "GameSource/Gui/Flow/HUD/States/BrnBootAttract.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX. BootAttract registers for the single
// GUI event it watches when the attract state is entered, and unregisters on leave.
// The observed-event id table lives in .rdata (@0x8205A8EC); it carries no value in
// the IDA export, so it is declared in the header and resolved at link time.

namespace BrnGui
{
    const s32 BootAttract::miNumEventsObserved = 1;

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
}
