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
// FLAGGED: this type has no dedicated dossier in the available exports -- its
// own member functions are reconstructed in their owning TUs as they are reached.
// This header models only the surface the committed callers require:
//   * the inheritance edge to ServerInterfaceDirtySockX360 (so a derived class is
//     a complete polymorphic type with a virtual destructor), and
//   * the three component accessors BrnServerInterfaceBase's inline wrappers call
//     (GetDownloadableConfigComponent / GetTelemetryComponent /
//      GetCustomCommandsComponent). They return the shared ServerInterfaceComponent
//     base pointer; the Brn wrappers reinterpret_cast it to the Brn leaf type. The
//     component objects live in the maComponents[] slot table inherited from
//     ServerInterfaceDirtySock; the accessors' bodies belong to this type's own
//     (not-yet-homed) TU, so they are declared-only here.
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
        ServerInterface();
        virtual ~ServerInterface();

        // Component accessors (declared-only; bodied in this type's own TU). They hand
        // back the shared ServerInterfaceComponent base; the game-side wrappers
        // reinterpret_cast to the concrete Brn leaf component.
        ServerInterfaceComponent* GetDownloadableConfigComponent();
        ServerInterfaceComponent* GetTelemetryComponent();
        ServerInterfaceComponent* GetCustomCommandsComponent();
    };
}

#endif // CGS_SERVER_INTERFACE_H
