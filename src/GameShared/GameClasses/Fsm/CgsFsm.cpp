#include "GameShared/GameClasses/FSM/CgsFsm.h"   // CgsFsm::Fsm + CgsFsm::State

// CgsFsm::Fsm::Update @ 0x82835CE0
// Per-frame drive of the current state. When a state is set, runs the three-call
// spine in vtable order: PreUpdate (State vtable +12), Update (+8), PostUpdate (+16);
// no-op when there is no current state. Behaviour-faithful to the X360 pseudocode,
// which reads mpCurrentState and dispatches the three State virtuals through its
// vtable. Reuses the committed CgsFsm::State (CgsState.h) by name.

namespace CgsFsm
{
    void Fsm::Update()
    {
        State* lpState = mpCurrentState;
        if (lpState)
        {
            lpState->PreUpdate();
            lpState->Update();
            lpState->PostUpdate();
        }
    }
}
