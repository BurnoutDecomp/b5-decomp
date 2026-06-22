#pragma once

#include "GameShared/GameClasses/FSM/CgsState.h"   // CgsFsm::State

// CgsFsm::Fsm - the FSM base. Holds the current state and drives it: SetState swaps
// the active state, Update runs the per-frame state spine, Render forwards to the
// state. Layout (single CgsFsm::State* mpCurrentState) and method set recovered from
// the DecFIGS DWARF (CgsFsm.h). State bodies live in the committed CgsFsm::State home.
namespace CgsFsm
{
    struct Fsm
    {
        void Construct();
        bool Release();
        void SetState(State* lpState);
        void Update();
        void Render();

    protected:
        State* mpCurrentState;
    };
}
