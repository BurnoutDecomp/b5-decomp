#pragma once

// Traffic network IO event payloads. Reconstructed from the DecFIGS DWARF.
#include "types.hpp"
#include "GameSource/BurnoutConstants.h"                  // EActiveRaceCarIndex
#include "GameShared/GameClasses/Module/CgsEventQueue.h"  // CgsModule::EventQueue<T,N>
#include "SharedClasses/Traffic/BrnTrafficSharedConstants.h"  // BrnTraffic::KU_MAX_HULLS_IN_PVS

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

    // DWARF :117 -- the network-side OUTPUT interface that the local sim fills each frame so the
    // network layer can publish the traffic snapshot to peers. Holds the 8-deep ActivateHull event
    // queue (its `this` IS the embedded queue at offset 0, so ActivateHull forwards `this` straight
    // to AddEvent), the per-active-race-car active-hull table (KU_MAX_HULLS_IN_PVS == 8, seeded to
    // KU_INVALID_HULL in Construct), the cross-peer hash + the frame it was taken on, and the two
    // "valid" tripwire flags plus the hull-sync divergence flag. Member layout & types from the
    // DecFIGS DWARF, grounded against the X360 byte offsets (queue @0, table @0x6C, flags @0x7C/0x7D
    // /0x7E, hash @0x80 as a halfword, hash-frame @0x84 as a word).
    struct TrafficNetworkOutputInterface
    {
    public:
        typedef TrafficNetworkInputInterface::ActivateHullQueue ActivateHullQueue;   // DWARF :48

        void Construct();                                                            // :122
        // Queue a hull-activation event for a local active race car (X360 0x8271D238).
        void ActivateHull(EActiveRaceCarIndex leActiveRaceCarIndex,
                          u32 luNewActiveHull, u32 luUpdateFrame);                    // :129
        void SetDataHash(u32 luUpdateFrame, u16 luHash);                             // :135
        void SetActiveHull(EActiveRaceCarIndex leActiveRaceCarIndex, u16 luHull);    // :141
        void SetDetectedHullSyncDivergence(bool lbDivergence);                       // :146
        const ActivateHullQueue& GetActivateHullQueue() const;                       // :151
        bool HasHashBeenSet() const;                                                 // :155
        u16  GetDataHash() const;                                                    // :159  (X360 0x82542438)
        u32  GetUpdateFrameForHash() const;                                          // :163  (X360 0x82542490)
        const u16* GetActiveHulls() const;                                           // :167  (X360 0x825424E8)
        bool GetDetectedHullSyncDivergence() const;                                  // :171

        // X360 0x823C8FD8 -- copy-assignment. Clears then Appends the source ActivateHull queue,
        // then field-copies the active-hull table + valid/divergence flags + hash + hash-frame.
        // Callers: BrnNetworkModuleIO / BrnWorldIO output-buffer setters.
        TrafficNetworkOutputInterface& operator=(const TrafficNetworkOutputInterface& lrSource);

    private:
        ActivateHullQueue mActivateHullQueue;                       // :175 (offset 0)
        u16               mau16ActiveHulls[KU_MAX_HULLS_IN_PVS];    // :176 (offset 0x6C)
        bool              mbHashValid;                              // :178 (offset 0x7C)
        bool              mbActiveHullsValid;                       // :179 (offset 0x7D)
        bool              mbHullSyncDivergence;                     // :180 (offset 0x7E)
        u16               muHash;                                   // :181 (offset 0x80)
        u32               muHashUpdateFrame;                        // :182 (offset 0x84)
    };
}
}
