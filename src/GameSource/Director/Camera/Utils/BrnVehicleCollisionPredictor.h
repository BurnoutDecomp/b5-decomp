#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                   // Vector3 (Update's arguments)
#include "GameShared/GameClasses/Core/CgsAssert.h"             // CGS_ASSERT (the predicted-collision tripwire)
#include "GameSource/Director/Utils/BrnDirectorTimestep.h"     // BrnDirector::VecFloat (Update's step) -- inside
                                                               // namespace BrnDirector the name must resolve to it,
                                                               // as CameraUtils.h explains (the two VecFloats mangle
                                                               // differently)

// ============================================================================
// GameSource/Director/Camera/Utils/BrnVehicleCollisionPredictor.h
//
// BrnDirector::Camera::Utils::VehicleCollisionPredictor -- predicts whether the camera, moving at its velocity, is
// about to run into a traffic vehicle and, if so, when. Embedded in VisibilityCollisionPolicy at +0x70 (the flag
// the policy reads at +0x70, the time at +0x74; VisibilityCollisionPolicy::TimeUntilCollisionWithVehicle @0x821F3858
// inlines GetSoonestPredictedCollision, its assert naming this header's :69).
// Declaration verbatim from the DecFIGS DWARF (BrnVehicleCollisionPredictor.h:47-74): the has-predicted flag and a
// PredictedCollision { mfTimeUntilCollision } record, Construct / Update / HasPredictedCollision /
// GetSoonestPredictedCollision.
//   Update @0x822230D8 -- BODIED 2026-09-28 (owner's list, lane L2 CAMCOLLIDE, piece 5) in
//                         BrnVehicleCollisionPredictor.cpp.
// ============================================================================

namespace BrnDirector
{
struct AllVehicleData;   // GameSource/Director/Utils/BrnDirectorAllVehicleData.h (Update's traffic records)

namespace Camera
{
namespace Utils
{

class VehicleCollisionPredictor
{
public:
    // DWARF :51 -- the one predicted collision the predictor keeps.
    struct PredictedCollision
    {
        f32 mfTimeUntilCollision;   // :52  seconds until the camera meets the vehicle
    };

    void Construct() { mbHasPredictedCollision = false; }   // :56

    // :63 @0x822230D8 -- clears the prediction, then tests the camera's line (lPosition, along its velocity relative
    // to each traffic vehicle) against every traffic vehicle's ellipsoid; see the .cpp.
    void Update(const AllVehicleData& lAllVehicleData, VecFloat lTimestep, Vector3 lPosition, Vector3 lVelocity);

    // :66 -- a vehicle collision has been predicted this frame (the guard the time read is published behind).
    bool HasPredictedCollision() const { return mbHasPredictedCollision; }

    // :69 -- header-inline in the original (the X360 wrapper @0x821F3858 carries its assert): asserts a collision
    // was predicted (non-gating), then returns the record.
    const PredictedCollision& GetSoonestPredictedCollision() const
    {
        CGS_ASSERT(HasPredictedCollision(), "HasPredictedCollision()");   // :69 (non-gating)
        return mSoonestPredictedCollision;
    }

private:
    bool               mbHasPredictedCollision;      // :73  +0x00 (stb, Update @0x82223144 / 0x82223760)
    PredictedCollision mSoonestPredictedCollision;   // :74  +0x04 (stfs 4(r11), Update @0x822237E4)
};

}
}
}
