#include "BrnNetworkGameSearchParams.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::GameSearchParams::`scalar deleting destructor'  @ 0x82569280
//
// The X360 `scalar deleting destructor' stores the class vptr (off_8207C88C) at
// this+0 and, when its low bit flag is set, calls operator delete -- the exact
// codegen MSVC/Xenon emits for a polymorphic class with a virtual destructor.
// Reconstructed as the out-of-line virtual destructor itself; the deleting
// thunk is compiler-generated from it. No own data members are released here
// (the payload is plain bytes), matching the asm body which only resets the
// vptr and conditionally frees the object.

namespace BrnNetwork
{
    GameSearchParams::~GameSearchParams()
    {
    }
}
