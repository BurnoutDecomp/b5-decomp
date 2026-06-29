#ifndef BRN_SERVER_INTERFACE_TELEMETRY_H
#define BRN_SERVER_INTERFACE_TELEMETRY_H

#include "types.hpp"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceTelemetry.h"

// ===========================================================================
// BrnNetwork::BrnServerInterfaceTelemetry
//   Home: GameSource/Network/Components/BrnServerInterfaceTelemetry.{h,cpp}
//
// The Burnout-side telemetry server-interface component (the
// E_PREPARESTAGE_TELEMETRY_COMPONENT / E_RELEASESTAGE_TELEMETRY_COMPONENT stage
// owner; embedded by value as BrnServerInterfaceBase::mTelemetry). It is a thin
// game-specific leaf over the committed CgsNetwork::ServerInterfaceTelemetry base
// (the DirtySock telemetry component), exactly as BrnServerInterfaceCustomCommands
// derives from CgsNetwork::ServerInterfaceCustomCommands and
// BrnServerInterfaceDownloadableConfig from CgsNetwork::ServerInterfaceComponent.
//
// FLAGGED: this leaf has no dedicated dossier in the available exports. It is
// reached here only through the aggregate's by-value embed + the polymorphic-
// teardown vtable walk, so only the inheritance edge (giving it a complete
// polymorphic layout + virtual destructor) is modelled -- no Burnout-specific data
// members are recovered, and the behavioural overrides (Construct / Prepare /
// CaptureEvent / ...) belong to their own TUs. Members GROW additively when found.
// ===========================================================================

namespace BrnNetwork
{
    class BrnServerInterfaceTelemetry : public CgsNetwork::ServerInterfaceTelemetry
    {
    public:
        BrnServerInterfaceTelemetry();

        // Polymorphic teardown slot (the X360 deleting destructor restores the shared
        // component vtable at this+0 and conditionally frees).
        virtual ~BrnServerInterfaceTelemetry();
    };
}

#endif // BRN_SERVER_INTERFACE_TELEMETRY_H
