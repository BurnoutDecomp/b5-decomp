#pragma once

// MINIMAL SLICE for the RaceCarEntityModuleIO IO-buffer unlock; full layout reconstructed
// by VehicleOutputInterface / VehicleManagerOutputInterface's own TU (DWARF home
// GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h). Size 256 each
// (NOMINAL).
//
// Per the DecFIGS DWARF this header homes several BrnPhysics::Vehicle aggregates. The two
// referenced by the RaceCarEntityModuleIO IO buffers (InputBuffer_PostPhysics) are:
//   VehicleOutputInterface        : BitArray<8> + RaceCarState[8] + ImpactEventQueue +
//                                   PhysicalTrafficStateQueue + GameEventQueue +
//                                   AggressiveDrivingFlags.
//   VehicleManagerOutputInterface : seven traffic/race-car EventQueue<...> members +
//                                   VehicleGuiOutputMessages + CgsInput WheelFFSpring.
// Both are returned-by-pointer only, so reserved-byte blobs suffice here; the full member
// layouts belong to this type's own ledger TU. alignas(16) because the contained
// RaceCarState/EventQueue/Vector members are SIMD-aligned.
#include "types.hpp"   // u8
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                          // CgsModule::EventQueue<T,N>
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"          // TrafficRemovedEvent

namespace BrnPhysics
{
namespace Vehicle
{
    struct alignas(16) VehicleOutputInterface
    {
        unsigned char maReserved[256];
    };

    struct alignas(16) VehicleManagerOutputInterface
    {
        // DWARF BrnVehicleOutputInterface.h:189/429 -- the recycled/removed-traffic event queue
        // type the manager output interface publishes (EventQueue<TrafficRemovedEvent,25>). The
        // crash module embeds one as CrashModule::mRecycledTrafficQueue. ADDITIVE GROW: a nested
        // typedef only; does not change this blob's reserved layout (its own ledger TU owns the
        // full member set).
        typedef CgsModule::EventQueue<TrafficRemovedEvent, 25> RemovedTrafficEventQueue;

        unsigned char maReserved[256];
    };
}
}
