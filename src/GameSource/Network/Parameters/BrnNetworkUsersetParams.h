#ifndef BRN_NETWORK_USERSET_PARAMS_H
#define BRN_NETWORK_USERSET_PARAMS_H

#include "types.hpp"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/X360/CgsServerInterfaceUsersetParamsX360.h" // CgsNetwork::ServerInterfaceUsersetParamsX360 (base)

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
//
// Base: the platform userset record (Prepare chains to it). The replicated pattern is
// the empty string.
// ===========================================================================

namespace BrnNetwork
{
    class UsersetParams : public CgsNetwork::ServerInterfaceUsersetParamsX360
    {
    public:
        virtual ~UsersetParams();

        // Chain to the platform Prepare; true on success.
        virtual bool Prepare() override;

        // The (empty) serialisation pattern.
        virtual const char* GetPattern() const override;
    };
}

#endif // BRN_NETWORK_USERSET_PARAMS_H
