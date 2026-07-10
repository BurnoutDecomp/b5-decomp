#include "GameSource/Gui/Flow/HUD/States/BrnPausedHudState.h"

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // CgsGui::StateInterface, GetOutputEventQueue
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                      // CgsGui::GuiEvent, CgsModule::Event

// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX.
//
//   OnEnter @0x8247CBE8 -- RegisterForEvents(maiEventToObserve, 3), then post a GuiEvent<532>
//                          { muHeader0 = 1, muEventType = 532, muHeader2 = 12 } onto the state's
//                          large output queue with channel id 40 (GuiEventOut), record size 16.
//                          The X360 reaches the queue as `mpStateInterface + 0xC` (the
//                          StateInterface's mOutEventQueue); we reach it by name through
//                          GetOutputEventQueue(). The trailing word of the 16-byte record is left
//                          uninitialised by the X360 (it builds only the 12-byte event header).
//   OnLeave @0x82475390 -- UnRegisterForEvents(maiEventToObserve, 3) (tail call).

namespace BrnGui
{

// The GUI event PausedHudState publishes when it is entered. The X360 fills a GuiEvent<532>
// header { muHeader0 = 1, muEventType = 532, muHeader2 = 12 } and pushes a 16-byte record
// (channel id 40 = GuiEventOut); the trailing payload word is left uninitialised. Type id 532
// is the X360 GuiEvent<N> template id; the payload semantics beyond the header are not recovered.
struct GuiEventPausedHudEnter : public CgsGui::GuiEvent<532>
{
    u32 muReserved;   // +0x0C (X360 leaves this gap word uninitialised; record size is 16)

    GuiEventPausedHudEnter() : CgsGui::GuiEvent<532>(1, 12) {}
};

// 3 observed event ids. FLAG (unrecovered .rdata): the exports carry no values; 0 is a
// never-posted placeholder id until the table is recovered (the paused HUD is in-game
// territory, not on the boot path).
const s32 PausedHudState::maiEventToObserve[3] = { 0, 0, 0 };
const s32 PausedHudState::miNumEventsObserved = 3;

// @ 0x8247CBE8
void PausedHudState::OnEnter()
{
    mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

    GuiEventPausedHudEnter lEvent;
    mpStateInterface->GetOutputEventQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lEvent), 40, 16);
}

// @ 0x82475390
void PausedHudState::OnLeave()
{
    mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
}

} // namespace BrnGui
