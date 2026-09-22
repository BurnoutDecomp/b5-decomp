// ===================================================================================
// BrnNetwork::BrnNetworkModuleIO::NetworkToGuiInterface -- owning header
//   b5-decomp/src/GameSource/Network/SharedIO/BrnNetworkToGuiIOInterfaces.h
//
// The network module's GUI-bound interface: a four-slot queue of live-revenge status
// updates. It sits in BrnNetworkModuleIO::OutputBuffer at console +172376 (76 bytes:
// the 12-byte queue header + 4 x 16-byte NetworkToGuiLiveRevengeUpdate), directly before
// StatsOutputInterface at +172452. OutputBuffer::Construct runs the queue's Construct and
// zeroes its length word (+8) at that offset.
//
// The console has no out-of-line body for any of the methods below; they are header
// inlines folded into their callers (OutputBuffer::Construct, the LiveRevengeManager
// display paths, BrnGameModule::TranslateNetworkInterfaceToGuiEvents). Declared here with
// the reference shapes; Construct and Clear are bodied inline, the rest land with their callers.
// ===================================================================================
#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsEventQueue.h"          // CgsModule::EventQueue<T,N>
#include "GameSource/Network/SharedIO/BrnNetworkToGuiEvents.h"    // BrnNetwork::NetworkToGuiLiveRevengeUpdate

namespace BrnNetwork
{
    namespace BrnNetworkModuleIO
    {
        const s32 KI_MAX_NETWORK_TO_GUI_QUEUE_LENGTH = 4;

        struct NetworkToGuiInterface
        {
            typedef CgsModule::EventQueue<NetworkToGuiLiveRevengeUpdate, KI_MAX_NETWORK_TO_GUI_QUEUE_LENGTH>
                LiveRevengeUpdateQueue;

            // Header inlines: OutputBuffer::Construct emits the queue's Construct, then its
            // length reset, at this interface's offset.
            void Construct() { mLiveRevengeUpdateQueue.Construct(); }
            void Clear()     { mLiveRevengeUpdateQueue.Clear(); }
            void AddLiveRevengeUpdate(EActiveRaceCarIndex leAggressorActiveRaceCarIndex,
                                      EActiveRaceCarIndex leVictimActiveRaceCarIndex,
                                      NetworkToGuiLiveRevengeUpdate::LiveRevengeStatus leNewStatus,
                                      s32 liDifference);
            const LiveRevengeUpdateQueue* GetLiveRevengeUpdateQueue() const { return &mLiveRevengeUpdateQueue; }

        private:
            LiveRevengeUpdateQueue mLiveRevengeUpdateQueue;   // console +0 (76 bytes)
        };
    } // namespace BrnNetworkModuleIO
} // namespace BrnNetwork
