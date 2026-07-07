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
    // Update's per-frame input payload (owned by GameSource/Network/BrnNetworkModuleIO.{h,cpp});
    // reached only by pointer here, so a forward declaration keeps this header light.
    namespace BrnNetworkModuleIO
    {
        struct PostSimulationInputBuffer;
    }

    class BrnServerInterfaceTelemetry : public CgsNetwork::ServerInterfaceTelemetry
    {
    public:
        BrnServerInterfaceTelemetry();

        // Polymorphic teardown slot (the X360 deleting destructor restores the shared
        // component vtable at this+0 and conditionally frees).
        virtual ~BrnServerInterfaceTelemetry();

        // Prepare @ 0x82583650 -- run the base telemetry Prepare with the Burnout country-block
        // list + buffer caps, then latch the game's event-id -> keys mapping table.
        virtual bool Prepare(CgsNetwork::ServerInterfaceDirtySock* lpServerInterface,
                             bool lbConnectImmediately);

        // Release @ 0x825836C8 -- run the base telemetry Release, then wipe the mapping table.
        virtual bool Release();

        // Update @ 0x8258EF90 -- pump the base telemetry feed, then drain the inbound post-sim
        // network-event queue, capturing each telemetry event (type 16) into the component.
        void Update(const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInput);
    };
}

#endif // BRN_SERVER_INTERFACE_TELEMETRY_H
