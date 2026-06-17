#pragma once

// MINIMAL SLICE for the RaceCarEntityModuleIO IO-buffer unlock; full layout reconstructed
// by VehicleInputInterface's own TU (DWARF home GameSource/Physics/VehicleManager/SharedIO/
// BrnVehicleInputInterface.h). Size 256 (NOMINAL).
//
// Per the DecFIGS DWARF, BrnPhysics::Vehicle::VehicleInputInterface is a large aggregate of
// ~17 EventQueue<...> members (line-test results, create/remove/reset race-car events,
// traffic events, impact events) plus a BitArray<8>; it is only ever returned-by-pointer
// from the RaceCarEntityModuleIO IO buffers, so a reserved-byte blob suffices here. The full
// member layout belongs to this type's own ledger TU. alignas(16) because the contained
// EventQueue payloads carry Vector3/Matrix44Affine (SIMD) elements.
#include "types.hpp"   // u8

namespace BrnPhysics
{
namespace Vehicle
{
    struct alignas(16) VehicleInputInterface
    {
        unsigned char maReserved[256];
    };
}
}
