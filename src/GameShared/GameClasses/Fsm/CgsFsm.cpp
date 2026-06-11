#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x... (CgsFsm::Fsm::Update)
// First-pass reconstruction: behaviour-faithful to the X360 pseudocode. Update
// dispatches three virtual calls on the current state. The three slots are named
// by their vtable position (offsets 8/12/16 -> slots 2/3/4); their semantics are
// not yet recovered, hence the placeholder names.

namespace CgsFsm
{
    struct IState
    {
        virtual void slot0() = 0;
        virtual void slot1() = 0;
        virtual void slot2() = 0;  // vtable offset 8
        virtual void slot3() = 0;  // vtable offset 12
        virtual int  slot4() = 0;  // vtable offset 16
    };

    struct Fsm
    {
        IState* current;  // [0]

        int Update();
    };

    int Fsm::Update()
    {
        IState* state = current;
        if (state)
        {
            state->slot3();
            state->slot2();
            return state->slot4();
        }
        return 0;
    }
}
