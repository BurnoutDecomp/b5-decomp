#include "GameSource/Gui/Flow/HUD/States/BrnIdleHudState.h"

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // CgsGui::StateInterface::RegisterForEvents

// Reconstructed from BURNOUT_X360_ARTIST.XEX. The idle HUD state's entry wiring.
//
//   OnEnter @0x824759C0 -- mpStateInterface->RegisterForEvents(maiEventToObserve, 1), then
//                          reset the load bookkeeping (meCurrentState = E_IDLE_HUD_RESOURCES,
//                          mpGuiCache = 0, mbIsLoaded = false) and construct the embedded text
//                          field against the "text_txt" apt component on this state's interface.
//                          The X360 reaches mTextField at this+0x3C and dispatches its first
//                          virtual (TextField::Construct) with (name, mpStateInterface, 0); here
//                          we call Construct by name. The trailing argument is the parent-name
//                          pointer (NULL: this field has no parent clip).
//
// The observed-event id table (maiEventToObserve) lives in .data and carries no value in the
// IDA export, so it is declared in the header and resolved at link time (as BrnGui::BootAttract).

namespace BrnGui
{

// One observed GUI event id (value resolved at link time; see header).
const s32 IdleHudState::miNumEventsObserved = 1;

// @ 0x824759C0
void IdleHudState::OnEnter()
{
    mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

    meCurrentState = E_IDLE_HUD_RESOURCES;   // this+0x38 = 0
    mpGuiCache     = 0;                       // this+0x164 = 0
    mbIsLoaded     = false;                   // this+0x168 = 0

    mTextField.Construct("text_txt", mpStateInterface, 0);
}

} // namespace BrnGui
