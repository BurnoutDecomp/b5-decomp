#include "GameSource/Gui/Flow/BrnBaseFlow.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnGui::BrnBaseFlow::SetInEventQueue, reconstructed from BURNOUT_X360_ARTIST.XEX
// @ 0x827E28A8 (semantic parity, not byte match).
//
// X360 body:
//   cmplwi lpInEventQueue, 0 / bne ...          ; assert lpInEventQueue != NULL (BrnBaseFlow.h:162)
//   addis  r3, this, 1 ; addi r3, r3, 0x20      ; r3 = &mStateMachine (this + 0x10020)
//   bl     CgsGui::StateMachine::SetInEventQueue ; forward (this->mStateMachine, lpInEventQueue)
// A thin forwarder: validate the incoming GUI input-event queue (non-gating assert tripwire),
// then hand it to the flow's owned state machine. Under CGS_ASSERT the original's d:\p4-baked
// file/line is dropped per policy; the plain condition string ("lpInEventQueue") is forwarded.
//
// The +0x10020 reach is the EventObserver base size; member-by-name access (mStateMachine) through
// the real base reproduces it. Caller's StateMachine::SetInEventQueue is the GameShared out-of-line
// member already declared in CgsGuiStateMachine.h.

namespace BrnGui
{
    void BrnBaseFlow::SetInEventQueue(InputBuffer::GuiEventQueue* lpInEventQueue)
    {
        CGS_ASSERT(lpInEventQueue != 0, "lpInEventQueue");
        mStateMachine.SetInEventQueue(lpInEventQueue);
    }
}
