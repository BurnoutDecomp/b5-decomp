#ifndef CGS_SERVER_INTERFACE_H
#define CGS_SERVER_INTERFACE_H

#include "types.hpp"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/X360/CgsServerInterfaceDirtySockX360.h"

// ===========================================================================
// CgsNetwork::ServerInterface
//   Home: GameShared/GameClasses/Network/ServerInterface/CgsServerInterface.{h,cpp}
//
// The platform-neutral public server-interface facade the game derives from
// (BrnNetwork::BrnServerInterfaceBase : public CgsNetwork::ServerInterface). It
// is the top of the DirtySock server-interface inheritance chain:
//
//   ServerInterfaceDirtySock           (CgsServerInterfaceDirtySock.h)
//       <- ServerInterfaceDirtySockX360 (CgsServerInterfaceDirtySockX360.h, X360/PC leaf)
//           <- ServerInterface          (this header)
//
// (confirmed by the CgsServerInterfaceDirtySockX360.h note: "CgsNetwork::Server-
// Interface derives from this", and the CgsServerInterface.h DWARF chain
// `ServerInterface : public ServerInterfaceDirtySockPS3` on the PS3 build).
//
// This header models the inheritance edge to the platform facade (so a derived class
// is a complete polymorphic type with a virtual destructor). The component
// accessors the game-side wrappers call are the inline slot reads inherited from
// ServerInterfaceDirtySock.
// No data members are added: ServerInterface introduces none of its own in the
// recovered layout (the components are embedded by the most-derived game class).
// ===========================================================================

namespace CgsNetwork
{
    class ServerInterfaceComponent;       // forward; accessors return a base pointer
    struct ServerInterfacePrepareParams;  // Prepare-chain param block (own home)

    class ServerInterface : public ServerInterfaceDirtySockX360
    {
    public:
        // Both inline and empty: the owning game-side constructor / destructor store
        // nothing for this level but the vtable pointers. The component accessors the
        // game-side wrappers call (GetDownloadableConfigComponent / GetTelemetryComponent /
        // GetCustomCommandsComponent) are the inline slot reads inherited from
        // ServerInterfaceDirtySock.
        ServerInterface() {}
        virtual ~ServerInterface() {}
    };
}

#endif // CGS_SERVER_INTERFACE_H
