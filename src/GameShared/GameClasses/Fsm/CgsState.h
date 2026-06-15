#pragma once

// CgsFsm::State - the root of the FSM state hierarchy. A pure interface with a
// vtable and no data members of its own. Recovered from the DecFIGS DWARF
// (CgsState.h); the virtuals are non-pure (the base supplies empty defaults),
// so derived states need only override the ones they care about. The bodies live
// in the out-of-scope CgsState.cpp and resolve at link time.
namespace CgsFsm
{
    struct State
    {
        State();

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();
        virtual void PreUpdate();
        virtual void PostUpdate();
        virtual void Render();
    };
}
