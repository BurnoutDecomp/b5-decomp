#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3 / Vector3Plus aliases

// BrnPhysics::Deformation::TagPointSpec — the streamed/authored description of one
// deformation-skinned tag point: which two deformation sensors ("bones") it is bound
// to, the local offset from each sensor, the per-bone blend weights, and its rest
// (initial) position. Homed at the mirrored DWARF path
// (SharedClasses/Physics/Deformation/BrnTagPointSpec.h).
//
// Layout reconstructed from the X360 TagPoint::Construct pseudocode (the authority for
// offsets) cross-checked against the DWARF member list. Offsets (bytes), all confirmed
// by the pseudocode's field reads:
//
//   +0  Vector3Plus mOffsetFromAAndWeightA             (offset-from-sensor-A in xyz, weightA in w)
//   +16 Vector3Plus mOffsetFromBAndWeightB             (offset-from-sensor-B in xyz, weightB in w)
//   +32 Vector3Plus mInitialPositionAndDetachThreshold (rest position in xyz)
//   +48 f32         mfWeightA                          (scalar weightA; *(spec+48))
//   +52 f32         mfWeightB                          (scalar weightB; *(spec+52))
//   +56 f32         mfDetachThresholdSquared
//   +60 s16         miDeformationSensorA               (*(spec+60); asserted in [0,20))
//   +62 s16         miDeformationSensorB               (*(spec+62); asserted in [0,20))
//   +64 s8          miJointIndex
//   +65 bool        mbSkinnedPoint
//
// Only the members/accessors the current TU needs are bodied; the rest are declared so
// callers compile (COMPILE-ONLY gate — callee bodies not required). GROW this header as
// further TagPointSpec TUs are reconstructed.

namespace BrnPhysics
{
namespace Deformation
{
    struct TagPointSpec
    {
    private:
        Vector3Plus mOffsetFromAAndWeightA;              // +0
        Vector3Plus mOffsetFromBAndWeightB;              // +16
        Vector3Plus mInitialPositionAndDetachThreshold;  // +32
        f32         mfWeightA;                            // +48
        f32         mfWeightB;                            // +52
        f32         mfDetachThresholdSquared;            // +56
        s16         miDeformationSensorA;                 // +60
        s16         miDeformationSensorB;                 // +62
        s8          miJointIndex;                         // +64
        bool        mbSkinnedPoint;                       // +65

    public:
        // ---- accessors used by TagPoint::Construct (bodied) -----------------------
        s16 GetDeformationSensorA() const { return miDeformationSensorA; }
        s16 GetDeformationSensorB() const { return miDeformationSensorB; }

        const Vector3& GetOffsetFromDeformationSensorA() const
        {
            return reinterpret_cast<const Vector3&>(mOffsetFromAAndWeightA);
        }
        const Vector3& GetOffsetFromDeformationSensorB() const
        {
            return reinterpret_cast<const Vector3&>(mOffsetFromBAndWeightB);
        }
        const Vector3& GetInitialPosition() const
        {
            return reinterpret_cast<const Vector3&>(mInitialPositionAndDetachThreshold);
        }

        f32 GetWeightA() const { return mfWeightA; }
        f32 GetWeightB() const { return mfWeightB; }

        // The whole packed Vector3Plus (xyz offset + w weight) for each bone — the form
        // the SIMD blend actually consumes (it adds the xyz and splats the w weight).
        const Vector3Plus& GetOffsetAndWeightA() const { return mOffsetFromAAndWeightA; }
        const Vector3Plus& GetOffsetAndWeightB() const { return mOffsetFromBAndWeightB; }
        const Vector3Plus& GetInitialPositionAndDetachThreshold() const
        {
            return mInitialPositionAndDetachThreshold;
        }

        // ⭐ HEADER-INLINE (2026-08-14, walls wave): no X360/PS3 export symbol exists for it (the
        // console kept it inline), and UpdateSkinningOffsets' reference was unresolved in the
        // walls-wave trial link. It is the mbSkinnedPoint read.
        bool IsSkinned() const { return mbSkinnedPoint; }

        // ---- remaining authored API (declared only; bodies elsewhere) -------------
        void Construct(s32, f32, Vector3, s32, f32, Vector3, Vector3, f32, bool);
        void SetInitialPosition(const Vector3&);
        void FixUp(void*);
        void FixDown(void*);
        void SetOffsetFromA(const Vector3&);
        void SetOffsetFromB(const Vector3&);
        void SetWeightA(f32);
        void SetWeightB(f32);
        void SetDetachThreshold(f32);
        f32  GetDetachThresholdSquared() const;
        s32  GetIKPartJointIndex() const;
    };
}
}
