#ifndef BRN_NETWORK_QUICK_JOIN_PARAMS_H
#define BRN_NETWORK_QUICK_JOIN_PARAMS_H

#include "types.hpp"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceQuickJoinParams.h" // CgsNetwork::ServerInterfaceQuickJoinParams (base)

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
//
// Base: the platform quick-join record (its Prepare is the first thing this Prepare
// calls, and SetMatchmakingParameter writes the base's parameter list: the value at
// +0x08 + 4 * parameter, then bumps the count at +0x04).
// ===========================================================================

namespace BrnNetwork
{
    class QuickJoinParams : public CgsNetwork::ServerInterfaceQuickJoinParams
    {
    public:
        virtual ~QuickJoinParams();

        // Platform prepare, then publish the ranked / unranked matchmaking contexts in the
        // platform payload. Declared only (the payload is still opaque bytes in the base).
        virtual bool Prepare() override;

        // Store one matchmaking parameter (asserted in [0, 5)) and count it (at most 16).
        // The parameter enum has no home yet, so it travels as s32.
        void SetMatchmakingParameter(s32 leParameter, s32 liValue);
    };
}

#endif // BRN_NETWORK_QUICK_JOIN_PARAMS_H
