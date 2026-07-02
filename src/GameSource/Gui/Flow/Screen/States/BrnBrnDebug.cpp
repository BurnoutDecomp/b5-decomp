#include "GameSource/Gui/Flow/Screen/States/BrnBrnDebug.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX. BrnDebug (the debug GUI screen state)
// registers for the two GUI events it watches when entered, and unregisters on leave --
// the same tail-call pair as the sibling states (BrnBootAttract et al.).
//
// Bodied here (2 ledger functions, DWARF primary file
// GameSource/Gui/Flow/Screen/States/BrnBrnDebug.cpp):
//   BrnDebug::OnEnter @0x824B5150  (b CgsGui::StateInterface::RegisterForEvents)
//   BrnDebug::OnLeave @0x824B5168  (b CgsGui::StateInterface::UnRegisterForEvents)
//
// The observed-event id table lives in .data @0x82065DB0; its two big-endian dwords read
// { 14, 6 } straight out of the decrypted ARTIST XEX (file_off = 0x3000 + vaddr -
// 0x82000000). Id 6 is the controller-action GUI event (same id the boot states observe,
// see BrnBootLegal.cpp); id 14's producer is not yet named -- kept as the raw id.

namespace BrnGui
{
    const s32 BrnDebug::maiEventToObserve[] = { 14, 6 };
    const s32 BrnDebug::miNumEventsObserved = 2;

    // @ 0x824B5150
    void BrnDebug::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);
    }

    // @ 0x824B5168
    void BrnDebug::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
    }
}
