#ifndef BRN_NETWORK_PLAYER_INFO_DATA_H
#define BRN_NETWORK_PLAYER_INFO_DATA_H

#include "types.hpp"

// ===========================================================================
// BrnNetwork::PlayerInfoData
//   Home: GameSource/Network/Parameters/BrnNetworkPlayerInfoData.{h,cpp}
//
// Game-side per-player info object. The recovered function is its
// `scalar deleting destructor' (X360 @ 0x8255EA38): store the class vptr
// (off_8207C88C) at this+0, then conditionally call operator delete -- the
// codegen of a virtual destructor. The asm chains to no base dtor and frees no
// owned members, so the class is modelled as a minimal vptr-only polymorphic
// type; no member layout is asserted beyond what the asm proves.
// ===========================================================================

namespace BrnNetwork
{
    class PlayerInfoData
    {
    public:
        virtual ~PlayerInfoData();
    };
}

#endif // BRN_NETWORK_PLAYER_INFO_DATA_H
