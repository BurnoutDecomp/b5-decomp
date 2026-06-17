#pragma once

// MINIMAL SLICE for the RaceCarEntityModuleIO IO-buffer unlock; full layout reconstructed
// by VehicleDriverInputInterface's own TU (DWARF home GameSource/Physics/VehicleManager/
// SharedIO/BrnVehicleDriverInputInterface.h). Size 256 (NOMINAL).
//
// Per the DecFIGS DWARF, BrnPhysics::Vehicle::VehicleDriverInputInterface holds a
// VariableEventQueue<4920,16> (mDriverUpdateQueue) plus Vector3[8] target-assist positions,
// EntityId[8] ids and a count. It is only returned-by-pointer from the IO buffers, so a
// reserved-byte blob suffices here; the full member layout belongs to this type's own ledger
// TU. alignas(16) because it carries a VariableEventQueue and Vector3 array (SIMD).
#include "types.hpp"   // u8

namespace BrnPhysics
{
namespace Vehicle
{
    struct alignas(16) VehicleDriverInputInterface
    {
        unsigned char maReserved[256];
    };
}
}
