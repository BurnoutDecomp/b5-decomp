// ===================================================================================
// BrnNetwork::BrnNetworkModuleIO::StatsInputInterface -- the queue-forwarding members
//   b5-decomp/src/GameSource/Network/SharedIO/BrnNetworkModuleStatsIOInterface.cpp
//
// None of these has an out-of-line console body: PostSimulationInputBuffer::Construct
// inlines Construct as the EventQueue<StatsRequestEvent,16> Construct at this interface's
// offset, and Clear as the store of 0 to the queue's length word (+8) right after it.
//
// They live here rather than inline in the header because the header cannot see the complete
// StatsRequestEvent (see its fork note); this TU includes the real home and reaches the
// interface's storage as the real queue type.
// ===================================================================================

#include "GameSource/Network/SharedIO/BrnNetworkModuleStatsIOInterface.h"
#include "GameSource/Network/Managers/BrnStatsRequestEvent.h"   // BrnNetwork::StatsRequestEvent (the real home)

namespace BrnNetwork
{
    namespace BrnNetworkModuleIO
    {
        void StatsInputInterface::Construct()
        {
            static_assert(sizeof(StatsRequestEvent) == KI_STATS_REQUEST_EVENT_SIZE,
                          "StatsRequestEvent is the 20-byte element the storage is sized for");
            static_assert(sizeof(StatsInputQueue) == sizeof(maStatsEventQueueStorage),
                          "StatsInputQueue must fill its storage exactly");

            reinterpret_cast<StatsInputQueue*>(&maStatsEventQueueStorage[0])->Construct();
        }

        void StatsInputInterface::Clear()
        {
            reinterpret_cast<StatsInputQueue*>(&maStatsEventQueueStorage[0])->Clear();
        }

        // The read twin of StatsOutputInterface::GetStatsOutputQueue.
        const StatsInputInterface::StatsInputQueue* StatsInputInterface::GetStatsInputQueue() const
        {
            return reinterpret_cast<const StatsInputQueue*>(&maStatsEventQueueStorage[0]);
        }
    } // namespace BrnNetworkModuleIO
} // namespace BrnNetwork
