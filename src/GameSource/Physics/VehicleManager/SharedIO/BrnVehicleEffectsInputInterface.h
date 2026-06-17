#pragma once

// MINIMAL SLICE for the RaceCarEntityModuleIO IO-buffer unlock; full layout reconstructed
// by VehicleEffectsInputInterface's own TU (DWARF home GameSource/Physics/VehicleManager/
// SharedIO/BrnVehicleEffectsInputInterface.h). Size 256 (NOMINAL).
//
// Per the DecFIGS DWARF, BrnPhysics::Vehicle::VehicleEffectsInputInterface holds an
// EventQueue<CreateAirRamEvent,20> (mAirRamQueue) and an EventQueue<CreateSpinEvent,10>
// (mSpinQueue). It is only returned-by-pointer from the IO buffers, so a reserved-byte blob
// suffices here; the full member layout belongs to this type's own ledger TU. alignas(16)
// because the air-ram/spin events carry Vector3 (SIMD) members.
#include "types.hpp"   // u8

namespace BrnPhysics
{
namespace Vehicle
{
    struct alignas(16) VehicleEffectsInputInterface
    {
        unsigned char maReserved[256];
    };
}
}
