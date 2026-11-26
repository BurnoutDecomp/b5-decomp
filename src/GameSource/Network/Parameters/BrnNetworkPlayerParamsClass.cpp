#include "BrnNetworkPlayerParamsClass.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::PlayerParams::`scalar deleting destructor'  @ 0x825662D8
//
// Stores the base chain's vptr (off_8207C88C) at this+0 then conditionally calls
// operator delete. DWARF (BrnServerInterface.h:50) gives PlayerParams only implicit
// members, so the thunk is compiler-generated with the vtable in every TU that
// constructs a PlayerParams; this file defines nothing of its own.
