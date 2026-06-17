#pragma once

// BrnPhysics::Vehicle::AboveGroundTestResult -- the above-ground (down-ray) test result
// embedded by value in RaceCarState (and in SimpleVehiclePhysics). Namespace-scope struct,
// NOT nested, per references/DecFIGS/dwarfdump/.../BrnSimpleVehiclePhysics.h (line :71).
//
// Only this result struct is reconstructed; the SimpleVehiclePhysics class that also owns
// it (and its methods Reset/SetFrom/SetValidResult) is a separate future TU. Those methods
// are declared-only here so that TU can define them later without an ODR clash.
#include "BrnCommonTypes.h"   // Vector3, CollisionTag
#include "types.hpp"          // f32, u16

namespace BrnPhysics
{
namespace Vehicle
{
    struct AboveGroundTestResult
    {
        Vector3      mIntersectionPosition;
        Vector3      mIntersectionNormal;
        f32          mfVerticalDistance;
        CollisionTag mCollisionTag;
        bool         mbValid;

        // Owned by the BrnSimpleVehiclePhysics TU -- declare only (no body).
        void Reset();
        void SetFrom(Vector3, const AboveGroundTestResult*);
        void SetValidResult(Vector3, Vector3, f32, u16, u16);
    };
}
}
