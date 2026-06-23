#ifndef BRN_NETWORK_USERSET_PARAMS_H
#define BRN_NETWORK_USERSET_PARAMS_H

#include "types.hpp"

// ===========================================================================
// BrnNetwork::UsersetParams
//   Home: GameSource/Network/Parameters/BrnNetworkUsersetParams.{h,cpp}
//
// Game-side userset (group/party) parameter object. The recovered function is
// its `vector deleting destructor' (X360 @ 0x8255EAF0): store the class vptr
// (off_8207C88C) at this+0, then conditionally call operator delete -- the
// codegen of a virtual destructor. No base-dtor chaining or owned-member release
// appears in the asm, so the class is modelled as a minimal vptr-only
// polymorphic type.
// ===========================================================================

namespace BrnNetwork
{
    class UsersetParams
    {
    public:
        virtual ~UsersetParams();
    };
}

#endif // BRN_NETWORK_USERSET_PARAMS_H
