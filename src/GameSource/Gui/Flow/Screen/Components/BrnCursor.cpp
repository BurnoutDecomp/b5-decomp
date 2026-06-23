// BrnCursor.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   BrnGui::GuiCursor::Update @ 0x824941A8
//
//   if (!mpStateInterface) <fire "mpStateInterface" assert>   (non-fatal, DWARF :380)
//   build a 48-byte cursor event on the stack:
//       event.muHeader0   = 32      (li 0x20)
//       event.muEventType = 560     (li 0x230)
//       event.muHeader2   = 16      (li 0x10)
//       event.maPosition  = *(this + 0xA0)   (lvx128 -> stvx128, 16-byte lane move)
//       event.miState0    = *(this + 0xE4)
//       event.miState1    = *(this + 0xE8)
//   mpStateInterface->mOutEventQueue.AddEvent(&event, 40, 48)   (channel id 40 = GuiEventOut)
//
// The X360 reaches the queue as `mpStateInterface + 0xC` (the StateInterface's
// mOutEventQueue, a VariableEventQueue<65536,16>) and calls its 3-arg AddEvent. We reach
// it BY NAME through GetOutputEventQueue(); the queue's AddEvent is the inherited
// VariableEventQueue<65536,16>::AddEvent. The header/state words and position lane are
// copied into named event fields (the guest's lvx128/stvx128 is a plain 16-byte block
// move, lowered to scalar lane copies -- endian-independent, not a VMX pipeline). The
// X360-baked assert file/line are discarded per project convention.

#include "GameSource/Gui/Flow/Screen/Components/BrnCursor.h"

namespace BrnGui
{

// @ 0x824941A8
bool GuiCursor::Update()
{
    CGS_ASSERT( mpStateInterface != nullptr, "mpStateInterface" );

    GuiEventCursorUpdate lEvent;
    lEvent.muHeader0   = 32;
    lEvent.muEventType = 560;
    lEvent.muHeader2   = 16;

    // 16-byte position lane (X360 lvx128 from this+0xA0 -> stvx128 into the event).
    lEvent.maPosition[0] = maPosition[0];
    lEvent.maPosition[1] = maPosition[1];
    lEvent.maPosition[2] = maPosition[2];
    lEvent.maPosition[3] = maPosition[3];

    lEvent.miState0 = miState0;   // *(this + 0xE4)
    lEvent.miState1 = miState1;   // *(this + 0xE8)

    // Push onto the owning state's large output queue with channel id 40 (GuiEventOut),
    // record size 48 bytes (the X360 li r5=0x28=40 type, li r6=0x30=48 size).
    return mpStateInterface->GetOutputEventQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>( &lEvent ), 40, 48 );
}

} // namespace BrnGui
