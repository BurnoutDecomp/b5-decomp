#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                              // Vector3 alias
#include "SharedClasses/Physics/Deformation/BrnTagPointSpec.h"
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformationSensor.h"

// BrnPhysics::Deformation::TagPoint — a deformation-skinned vertex/locator on a
// vehicle. Each tag point is skinned to TWO deformation sensors ("bones") with per-bone
// weights (carried by its TagPointSpec); Construct() computes its rest state by blending
// the two sensor transforms applied to the spec's per-bone local offsets.
//
// Layout reconstructed from the X360 TagPoint::Construct pseudocode (offset authority),
// cross-checked against the DWARF member list. Console offsets (32 bytes total):
//
//   +0x00 Vector3                  mPos             (computed rest position; result+0)
//   +0x10 const TagPointSpec*      mpSpec           (result word 4; *(result+4) = lpType)
//   +0x14 const DeformationSensor* mpSensorA        (result word 5; base + 432*sensorA)
//   +0x18 const DeformationSensor* mpSensorB        (result word 6; base + 432*sensorB)
//   +0x1C f32                      mfScratchAmount  (result word 7; blended sensor weight)
//
// Asserts in Construct reference this header at lines 117-120 (the four sensor-index
// range checks). GROW this header — do not fork it — for further TagPoint TUs.

namespace BrnPhysics
{
namespace Deformation
{
    struct TagPoint
    {
    private:
        Vector3                   mPos;            // +0x00
        const TagPointSpec*       mpSpec;          // +0x10
        const DeformationSensor*  mpSensorA;       // +0x14
        const DeformationSensor*  mpSensorB;       // +0x18
        f32                       mfScratchAmount; // +0x1C

    public:
        // Compute the tag point's rest state from its spec and the deformation-sensor
        // array. lpSensorArrayBase points at element 0 of the owning DeformableObject's
        // sensor array (maDeformationSensors); the spec's sensor indices select the two
        // bones this tag point is skinned to.
        void Construct(const TagPointSpec* lpType, const DeformationSensor* lpSensorArrayBase);

        // ---- remaining authored API (declared only; bodies elsewhere) -------------
        const Vector3&           GetPosition() const;
        Vector3                  GetInitialPosition() const;
        const DeformationSensor* GetDeformationSensorA() const;
        const DeformationSensor* GetDeformationSensorB() const;
        Vector3                  GetOffsetFromInitialPosition() const;
        f32                      GetScratchAmount() const;
        void                     SetPosition(Vector3);
        f32                      GetDetachThresholdSquared() const;
        f32                      GetJointDetachThresholdSquared() const;
        s32                      GetJointIndex() const;
        const TagPointSpec*      GetSpec();
        bool                     HasJoint() const;
    };
}
}
