// ===================================================================================
// BrnNetwork::BrnNetworkModuleIO::StatsInputInterface / StatsOutputInterface -- owning header
//   b5-decomp/src/GameSource/Network/SharedIO/BrnNetworkModuleStatsIOInterface.h
//
// The stats request/response interfaces the network module exchanges with the game:
//   StatsInputInterface  -- PostSimulationInputBuffer console +72952, 332 bytes
//                           (EventQueue<StatsRequestEvent,16>: 12-byte header + 16 x 20).
//   StatsOutputInterface -- OutputBuffer console +172452, 2124 bytes
//                           (EventQueue<NetworkPlayerStats,16>: 12-byte header + 16 x 132).
// The buffers' Construct runs each queue's Construct and zeroes its length word (+8).
//
// The console has no out-of-line body for any of these methods (header inlines folded into
// the buffer Construct and the NetworkPlayerStatsManager paths). Declared with the reference
// shapes; GetStatsOutputQueue is a pure member read and is defined here. The buffers'
// Construct runs each interface's Construct then its Clear: StatsOutputInterface's pair is
// inline below, StatsInputInterface's lives in BrnNetworkModuleStatsIOInterface.cpp.
//
// ODR FORK, so StatsInputInterface keeps its queue as storage: BrnNetwork::StatsRequestEvent
// has two definitions in the tree -- the real home Managers/BrnStatsRequestEvent.h
// (char[16] name + s32 id) and a five-word copy in Managers/BrnNetworkStatsRequestEventQueue.h.
// BrnNetworkPlayerStatsManager.h includes the copy and also reaches this header through
// BrnGameActions.h -> BrnNetworkModuleIO.h, so including the real home here would be a
// redefinition in that TU. The queue is held as storage of the host queue's size (the 20-byte
// element is the same on both targets; the header carries a pointer) until the copy is
// retired; then type it as StatsInputQueue and define the methods inline on the member.
// ===================================================================================
#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsEventQueue.h"         // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/Core/CgsAssert.h"               // CGS_ASSERT (AppendStatsEvent)
#include "GameSource/Network/Managers/BrnNetworkPlayerStats.h"   // BrnNetwork::NetworkPlayerStats (132 bytes)

namespace BrnNetwork
{
    struct StatsRequestEvent;   // real home: Managers/BrnStatsRequestEvent.h (see the fork note above)

    namespace BrnNetworkModuleIO
    {
        const s32 KI_STATS_EVENT_QUEUE_SIZE   = 16;
        const s32 KI_STATS_OUTPUT_BUFFER_SIZE = 16;

        // sizeof(StatsRequestEvent): 20 bytes on both targets (checked against the real type in
        // BrnNetworkModuleStatsIOInterface.cpp).
        const s32 KI_STATS_REQUEST_EVENT_SIZE = 20;

        struct StatsInputInterface
        {
            typedef CgsModule::EventQueue<StatsRequestEvent, KI_STATS_EVENT_QUEUE_SIZE> StatsInputQueue;

            void AppendStatsEvent(StatsRequestEvent* lpEvent);
            const StatsInputQueue* GetStatsInputQueue() const;
            void Construct();   // body in BrnNetworkModuleStatsIOInterface.cpp
            void Clear();       // body in BrnNetworkModuleStatsIOInterface.cpp

        private:
            // EventQueue<StatsRequestEvent,16>: console 332 bytes (12-byte header + 16 x 20).
            // Storage of the host queue's size (see the fork note); the .cpp reaches it as the
            // real queue type.
            alignas(8) u8 maStatsEventQueueStorage[sizeof(CgsModule::BaseEventQueue<StatsRequestEvent>)
                                                   + KI_STATS_EVENT_QUEUE_SIZE * KI_STATS_REQUEST_EVENT_SIZE];
        };

        struct StatsOutputInterface
        {
            typedef CgsModule::EventQueue<NetworkPlayerStats, KI_STATS_OUTPUT_BUFFER_SIZE> StatsOutputQueue;

            // Inlined by the stats manager's posting paths: a bounds-gated append whose failure
            // trips the assert.
            void AppendStatsEvent(NetworkPlayerStats* lpStats)
            {
                CGS_ASSERT(mStatsEventQueue.AddEventSafe(*lpStats), "mStatsEventQueue.AddEventSafe(*lpStats)");
            }
            const StatsOutputQueue* GetStatsOutputQueue() const { return &mStatsEventQueue; }
            // Header inlines: OutputBuffer::Construct emits the queue's Construct, then its
            // length reset, at this interface's offset.
            void Construct() { mStatsEventQueue.Construct(); }
            void Clear()     { mStatsEventQueue.Clear(); }

        private:
            StatsOutputQueue mStatsEventQueue;   // console +0
        };
    } // namespace BrnNetworkModuleIO
} // namespace BrnNetwork
