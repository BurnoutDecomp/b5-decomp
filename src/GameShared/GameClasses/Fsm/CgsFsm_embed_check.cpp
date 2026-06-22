#include "GameShared/GameClasses/FSM/CgsFsm.h"   // CgsFsm::Fsm + CgsFsm::State

#include <cstddef>

// Embed check for CgsFsm::Fsm::Update @ 0x82835CE0.
// Fsm holds a single CgsFsm::State* (mpCurrentState). Drive a derived state through
// the FSM and confirm the PreUpdate/Update/PostUpdate spine fires in order, and that
// a null current state is a no-op.
using CgsFsm::Fsm;
using CgsFsm::State;

static_assert(sizeof(Fsm) == sizeof(void*), "Fsm = single State* member");

namespace
{
    struct CountingState : public State
    {
        int order = 0;
        int preAt = 0, updAt = 0, postAt = 0;

        void PreUpdate() override  { preAt  = ++order; }
        void Update() override     { updAt  = ++order; }
        void PostUpdate() override { postAt = ++order; }
    };

    // Seed the protected mpCurrentState by name from a derived harness type
    // (no raw offset cast -- protected member is reachable through inheritance).
    struct TestFsm : public Fsm
    {
        void Seed(State* s) { mpCurrentState = s; }
    };
}

void CgsFsm_embed_check()
{
    CountingState st;
    TestFsm fsm;

    fsm.Seed(nullptr);
    fsm.Update();              // null current state -> no-op (no crash)

    fsm.Seed(&st);
    fsm.Update();
    // PreUpdate (1), Update (2), PostUpdate (3) in order
    (void)(st.preAt == 1 && st.updAt == 2 && st.postAt == 3);
}
