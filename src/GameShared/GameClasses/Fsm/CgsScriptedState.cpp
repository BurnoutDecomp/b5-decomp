#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82835FF0
//   (CgsFsm::ScriptedState::Construct)
//
// The X360 pseudocode is mangled by a Hex-Rays ABI artifact: it models the object
// pointer as the high half of a 64-bit `a1` (PPC register pairing), so the body
// reads:
//     *(HIDWORD(a1) + 8)  = a1;     // store the object pointer back-reference
//     *(HIDWORD(a1) + 16) = a2;     // store the second argument
//     return HIDWORD(a1);           // return `this`
//
// Recovered intent: a Construct that takes `this` plus one argument, writes a
// self/back-reference at +8 and the argument at +16, and returns `this`. The
// stores are 32-bit (pointer-width on the X360 32-bit ABI).
//
// NOTE: low confidence on the +8 store — the artifact collapses `this` and the
// stored value into one 64-bit `a1`. Flagged for review.

namespace CgsFsm
{
    struct ScriptedState
    {
        u8             mPad0[8];     // [0x00] opaque
        ScriptedState* mpSelf;       // [0x08] self/back-reference
        u8             mPad1[4];     // [0x0C] opaque
        s32            miArg;        // [0x10] second constructor argument

        ScriptedState* Construct(s32 liArg);
    };

    ScriptedState* ScriptedState::Construct(s32 liArg)
    {
        mpSelf = this;
        miArg  = liArg;
        return this;
    }
}
