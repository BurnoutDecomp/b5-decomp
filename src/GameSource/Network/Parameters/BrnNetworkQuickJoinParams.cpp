#include "BrnNetworkQuickJoinParams.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::QuickJoinParams::`vector deleting destructor'  @ 0x8255EB38
//
// The vector deleting destructor stores the class vptr (off_8207C91C) at this+0
// and conditionally calls operator delete. That is precisely the thunk a virtual
// destructor compiles to; reconstructed here as the out-of-line virtual dtor.

namespace BrnNetwork
{
    QuickJoinParams::~QuickJoinParams()
    {
    }
}
