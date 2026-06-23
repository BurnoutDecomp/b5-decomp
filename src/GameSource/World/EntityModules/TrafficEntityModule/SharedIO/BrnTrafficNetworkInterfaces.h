#pragma once

// Traffic network IO event payloads. Reconstructed from the DecFIGS DWARF.
#include "types.hpp"
#include "GameSource/BurnoutConstants.h"                  // EActiveRaceCarIndex
#include "GameShared/GameClasses/Module/CgsEventQueue.h"  // CgsModule::EventQueue<T,N>

namespace BrnTraffic
{
namespace BrnTrafficIO
{
    // Network event: activate a new collision hull for an active race car.
    struct ActivateHullEvent
    {
        EActiveRaceCarIndex meActiveRaceCarIndex;
        u16                 muNewActiveHull;
        u32                 muUpdateFrame;
    };

    // DWARF :71 -- the network-side input interface that queues hull-activation requests
    // arriving from a remote peer (BrnNetwork::TrafficManager::_HullSyncMessageArrivedCallback
    // calls ActivateHull). The interface is just the fixed 8-deep ActivateHull event queue plus
    // a "diverged" flag; its `this` IS the embedded queue (mActivateHullQueue at offset 0), so
    // the X360 ActivateHull (0x82558AB8) forwards `this` straight to AddEventSafe.
    struct TrafficNetworkInputInterface
    {
    public:
        typedef CgsModule::EventQueue<ActivateHullEvent, 8> ActivateHullQueue;   // DWARF :60

        void Construct();                                                        // :76
        // Validate the request (race-car index in [0,8), hull < KU_INVALID_HULL), pack an
        // ActivateHullEvent and AddEventSafe it onto the queue (X360 0x82558AB8).
        void ActivateHull(EActiveRaceCarIndex leActiveRaceCarIndex,
                          u32 luNewActiveHull, u32 luUpdateFrame);               // :83
        const ActivateHullQueue& GetActivateHullQueue() const;                   // :88
        void SetDiverged(bool lbDiverged);                                       // :92
        bool HasDiverged() const;                                                // :95

    private:
        ActivateHullQueue mActivateHullQueue;   // :100 (offset 0)
        bool              mbDiverged;           // :103
    };
}
}
