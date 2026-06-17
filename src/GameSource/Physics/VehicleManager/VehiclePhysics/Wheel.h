#pragma once

// BrnPhysics::Vehicle::Wheel -- MINIMAL reconstruction.
//
// The full Wheel class (suspension/inertia VectorIntrinsic blobs, TireAttribs,
// TireGripCurve, ~150 methods) is a separate future TU. Here we reconstruct ONLY the
// nested RoadContact struct, which WheelLite (and therefore RaceCarState) embeds by
// value and which must be a COMPLETE type for those aggregates to be complete.
//
// Shape recovered from references/DecFIGS/dwarfdump/GameSource/Physics/VehicleManager/
// VehiclePhysics/Wheel.h : RoadContact is nested in `struct Wheel` (Wheel.h:71), members
// in DWARF order. The Wheel TU that owns the full layout will GROW this struct later.
#include "BrnCommonTypes.h"   // Vector3, CollisionTag
#include "types.hpp"          // f32

namespace BrnPhysics
{
namespace Vehicle
{
    // Minimal Wheel: only the nested RoadContact is reconstructed here.
    struct Wheel
    {
        // One wheel/road line-test contact result (position, surface normal, distance,
        // collision tag, on/near-ground flags). Embedded by value in WheelLite.
        struct RoadContact
        {
            Vector3      mPosition;
            Vector3      mNormal;
            f32          mfLineDistanceToRoad;
            CollisionTag mCollisionTag;
            bool         mbIsOnGround;
            bool         mbWasOnGroundLastUpdate;
            bool         mbIsCloseToGround;
            bool         mbLineTestIsValid;

            // Owned by the Wheel TU -- declare only (no body) to avoid an ODR clash.
            void operator=(const RoadContact&);
        };
    };
}
}
