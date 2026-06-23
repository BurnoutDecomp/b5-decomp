// ---- GameSource/Network/SharedIO/BrnNetworkToGuiEvents.h ----
// Network->GUI event records that the network module pushes into its OutputBuffer's
// per-event queues for the front-end to consume. Layout (member names / types / order /
// enum values) recovered from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Network/SharedIO/BrnNetworkToGuiEvents.h).
//
// Only NetworkToGuiLiveRevengeUpdate is modelled here so far -- it is the element type of
// the EventQueue<NetworkToGuiLiveRevengeUpdate,4> embedded in the OutputBuffer, whose
// Construct (CgsModule::EventQueue<...,4>::Construct @ 0x82592518, called by
// BrnNetwork::BrnNetworkModuleIO::OutputBuffer::Construct) points the base queue at the
// inline maEvents[4] buffer at this+0xC. The element is four int32 fields == 16 bytes.
#pragma once

#include "types.hpp"
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"   // BrnNetwork::Event, EActiveRaceCarIndex

namespace BrnNetwork
{
    // DWARF BrnNetworkToGuiEvents.h:41 -- NetworkToGuiLiveRevengeUpdate. Derives from the
    // empty BrnNetwork::Event spine (no data members on the base). Four int32 members ==
    // 16 bytes, which matches the inline EventQueue<...,4> stride (buffer base at this+0xC).
    struct NetworkToGuiLiveRevengeUpdate : public Event
    {
        // DWARF BrnNetworkToGuiEvents.h:45
        enum LiveRevengeStatus
        {
            E_LIVEREVENGESTATUS_NEWRIVALRY = 0,
            E_LIVEREVENGESTATUS_REIGNITED  = 1,
            E_LIVEREVENGESTATUS_SETTLED    = 2,
            E_LIVEREVENGESTATUS_AHEAD      = 3,
            E_LIVEREVENGESTATUS_BEHIND     = 4,
            E_LIVEREVENGESTATUS_COUNT      = 5,
        };

        EActiveRaceCarIndex meAggressorActiveRaceCarIndex; // +0  BrnNetworkToGuiEvents.h:56
        EActiveRaceCarIndex meVictimActiveRaceCarIndex;    // +4  BrnNetworkToGuiEvents.h:57
        LiveRevengeStatus   meNewStatus;                   // +8  BrnNetworkToGuiEvents.h:58
        s32                 miDifference;                  // +12 BrnNetworkToGuiEvents.h:59
    };
}
