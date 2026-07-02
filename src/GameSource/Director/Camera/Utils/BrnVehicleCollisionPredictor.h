#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the predicted-collision tripwire)

// ============================================================================
// GameSource/Director/Camera/Utils/BrnVehicleCollisionPredictor.h
//
// BrnDirector::Camera::Utils::VehicleCollisionPredictor -- predicts whether the
// camera is about to hit a tracked vehicle and, if so, when. MINIMAL SLICE: only
// the has-predicted flag + time pair the VisibilityCollisionPolicy wrapper reads
// (X360 policy +0x70/+0x74, VisibilityCollisionPolicy::TimeUntilCollisionWithVehicle
// @0x821F3858 inlines the accessor below; its assert names this header's :69).
// The rest of the predictor rig lands with its own TU -- GROW in place.
// FLAG: the pair's placement INSIDE the predictor (flag @+0x00, time @+0x04) is
// inferred from the policy-relative reads; only the adjacency and order are
// attested.
// ============================================================================

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{

class VehicleCollisionPredictor
{
public:
    // A vehicle collision has been predicted this frame (the guard the time read
    // is published behind).
    bool HasPredictedCollision() const { return mbHasPredictedCollision != 0; }

    // The predicted time (seconds) until the camera hits the vehicle. Header-inline
    // in the original (the :69 assert the X360 wrapper carries); asserts a
    // collision was actually predicted, then returns the time (non-gating).
    f32 TimeUntilCollision() const
    {
        CGS_ASSERT(HasPredictedCollision(), "HasPredictedCollision()");   // :69 (non-gating)
        return mfTimeUntilCollision;
    }

private:
    u8  mbHasPredictedCollision;   // X360 predictor +0x00 (policy +0x70) -- FLAG: base inferred
    f32 mfTimeUntilCollision;      // X360 predictor +0x04 (policy +0x74)
};

}
}
}
