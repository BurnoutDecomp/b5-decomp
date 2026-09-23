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
// (the DirtySock telemetry component). It adds no instance data; its only state is
// the class-static event-id -> keys table. Construct / Destruct / Update() / OnEvent
// are the base's (the component vtable carries the base entries); Prepare is a new
// virtual with its own slot, Release overrides the base's.
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

    private:
        static const s32 KI_NUM_EVENT_DATA_KEYS = 50;

        // The event-id -> telemetry-keys table Prepare latches and Release zeroes.
        static CgsNetwork::EventDataKeys maEventDataKeys[KI_NUM_EVENT_DATA_KEYS];
    };
}

#endif // BRN_SERVER_INTERFACE_TELEMETRY_H
