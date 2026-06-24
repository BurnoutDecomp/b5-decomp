#pragma once

// ===================================================================================
// BrnNetwork::HullSyncMessage (+ HullToActivateInfo) -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnHullSyncMessage.h
//
// SHAPE from DecFIGS DWARF (BrnHullSyncMessage.h:38/45 + BrnNetworkTrafficManager.h)
// gated against the X360 binary. The message's only payload is a fixed Array of hulls
// to activate, embedded right after the 0x28-byte ReliableMessage base:
//   +0x00  (CgsNetwork::ReliableMessage base, size 0x28)
//   +0x28  BufferedHullsToActivate mBufferedHullActivates  (Array<HullToActivateInfo,7>)
// Array<T,7> = 7 * HullToActivateInfo (7 * 6 = 42 bytes) + a trailing s32 live-count
// (miCount, padded to +0x2C within the array = absolute +0x54), sizeof 48 -- which is
// exactly the 48-byte block PrepareForSend/Retrieve memcpy in/out (@ 0x8257DC78 /
// 0x8257DD58) and the +0x54 count word Construct (@ 0x8257DC38) and Release
// (@ 0x8257DD40) clear. This message is embedded by value (send/recv copies) inside
// BrnNetwork::TrafficManager::TrafficSyncData, so the layout must be exact.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"

namespace BrnNetwork
{
    // DWARF BrnHullSyncMessage.h:38 -- one traffic hull queued for activation, with the
    // frame it was requested and the traffic update it should activate on.
    struct HullToActivateInfo
    {
        u16 muHull;                 // +0x00
        u16 muFramesSinceStart;     // +0x02
        u16 muTrafUpdateToActivate; // +0x04

        // Additive memberwise equality. The X360 only emitted Ge/Grow for the
        // Array<HullToActivateInfo,7> instance, but forcing the explicit class
        // instantiation (CgsArrayHullToActivateInfo7.cpp) pulls in the generic
        // equality-based members (FindFirstInstanceOf/Contains), which need a plain
        // operator== over the three u16 lanes. Does not change layout/sizeof.
        bool operator==(const HullToActivateInfo& lrOther) const
        {
            return muHull == lrOther.muHull
                && muFramesSinceStart == lrOther.muFramesSinceStart
                && muTrafUpdateToActivate == lrOther.muTrafUpdateToActivate;
        }
    };

    // DWARF BrnHullSyncMessage.h:45 (typedef Array<HullToActivateInfo,7u>).
    typedef Array<HullToActivateInfo, 7> BufferedHullsToActivate;

    // Reliable message syncing the set of traffic hulls a peer should activate.
    struct HullSyncMessage : CgsNetwork::ReliableMessage
    {
        BufferedHullsToActivate mBufferedHullActivates;     // +0x28

        void Construct();
        void PrepareForSend(u16 lu16FrameCount, BufferedHullsToActivate* laBufferedHullActivates);
        bool Retrieve(BufferedHullsToActivate* laBufferedHullActivates);
        void Release();
    };
} // namespace BrnNetwork
