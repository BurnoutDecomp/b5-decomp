#include "GameShared/GameClasses/FSM/CgsFsm.h"   // CgsFsm::Fsm + CgsFsm::State

// CgsFsm::Fsm::Update @ 0x82835CE0
// Per-frame drive of the current state. When a state is set, runs the three-call
// spine in vtable order: PreUpdate (State vtable +12), Update (+8), PostUpdate (+16);
// no-op when there is no current state. Behaviour-faithful to the X360 pseudocode,
// which reads mpCurrentState and dispatches the three State virtuals through its
// vtable. Reuses the committed CgsFsm::State (CgsState.h) by name.
//
// FLAG: mpCurrentState is RE-READ from the member before EACH of the three calls.
// ARTIST asm (0x82835CE0) reloads `lwz r3, 0(r31)` before every dispatch, and the
// DecFIGS asm (0xB52500) does the same (calls 2/3 do `lwz r11, 0(this)`). The
// original source therefore writes `mpCurrentState->...` each time, NOT a cached
// local -- a state transition triggered inside PreUpdate/Update must affect the
// subsequent calls. Caching in a local (the previous reconstruction) was a silent
// behavioural bug.

namespace CgsFsm
{
    void Fsm::Update()
    {
        if (mpCurrentState)
        {
            mpCurrentState->PreUpdate();
            mpCurrentState->Update();
            mpCurrentState->PostUpdate();
        }
    }
}
