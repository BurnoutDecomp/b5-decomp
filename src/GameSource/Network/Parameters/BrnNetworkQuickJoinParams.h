#ifndef BRN_NETWORK_QUICK_JOIN_PARAMS_H
#define BRN_NETWORK_QUICK_JOIN_PARAMS_H

#include "types.hpp"

// ===========================================================================
// BrnNetwork::QuickJoinParams
//   Home: GameSource/Network/Parameters/BrnNetworkQuickJoinParams.{h,cpp}
//
// Game-side "quick join" parameter object. The only function recovered for this
// TU is its `vector deleting destructor' (X360 @ 0x8255EB38), whose body simply
// stores the class vptr (off_8207C91C) at this+0 and conditionally calls
// operator delete -- the standard codegen for a polymorphic class with a virtual
// destructor. The asm performs no base-dtor chaining and frees no owned members,
// so the class is modelled here as a minimal vptr-only polymorphic type; no
// member layout or base derivation is asserted beyond what the asm proves.
// ===========================================================================

namespace BrnNetwork
{
    class QuickJoinParams
    {
    public:
        virtual ~QuickJoinParams();
    };
}

#endif // BRN_NETWORK_QUICK_JOIN_PARAMS_H
