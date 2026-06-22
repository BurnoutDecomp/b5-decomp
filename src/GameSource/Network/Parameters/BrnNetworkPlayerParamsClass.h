#ifndef BRN_NETWORK_PLAYER_PARAMS_CLASS_H
#define BRN_NETWORK_PLAYER_PARAMS_CLASS_H

#include "types.hpp"

// ===========================================================================
// BrnNetwork::PlayerParams
//   Home: GameSource/Network/Parameters/BrnNetworkPlayerParamsClass.{h,cpp}
//
// Game-side player-parameter object. (The accessor/prepare logic of the related
// BrnNetwork::PlayerParamsBase lives in BrnNetworkPlayerParams.cpp; this TU owns
// only the PlayerParams leaf's destructor.) The recovered function is the
// `scalar deleting destructor' (X360 @ 0x825662D8): it stores the class vptr
// (off_8207C88C) at this+0 and conditionally calls operator delete -- the codegen
// of a virtual destructor. No base-dtor chaining or owned-member release appears
// in the asm, so the class is modelled as a minimal vptr-only polymorphic type.
// (Named ...PlayerParamsClass to avoid colliding with the existing
// BrnNetworkPlayerParams.cpp PlayerParamsBase home.)
// ===========================================================================

namespace BrnNetwork
{
    class PlayerParams
    {
    public:
        virtual ~PlayerParams();
    };
}

#endif // BRN_NETWORK_PLAYER_PARAMS_CLASS_H
