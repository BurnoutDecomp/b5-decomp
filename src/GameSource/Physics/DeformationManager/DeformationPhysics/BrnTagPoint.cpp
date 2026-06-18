#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnTagPoint.h"

#include "GameSource/Physics/DeformationManager/BrnDeformationConstants.h"  // KI_MAX_NUM_DEFORMABLE_SENSORS_PER_DEFORMABLE_OBJECT
#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT

// BrnPhysics::Deformation::TagPoint::Construct @ 0x825E7480
//
// Computes a deformation-skinned tag point's rest state by blending the two deformation
// sensors ("bones") it is bound to. The X360 build does this with VMX/AltiVec; the body
// below is the DE-SIMD'd scalar/vector equivalent (no __asm). The data flow reproduced
// from the pseudocode is:
//
//   posA   = sensorA.localSpaceSphere.centre + spec.offsetFromA   (per-lane add; the
//   posB   = sensorB.localSpaceSphere.centre + spec.offsetFromB    centre = first 16
//                                                                  bytes of the Sphere)
//   blend  = posA * spec.weightA + posB * spec.weightB            (the vmul/vmaddfp chain)
//   delta  = blend - spec.initialPosition                         (vsubfp)
//   mPos   = delta * 1.0 + spec.initialPosition  (== blend)       (final vmaddfp with a
//                                                                  ones vector; the
//                                                                  sub/add round-trips)
//
// and the combined scratch amount is
//   mfScratchAmount = sensorA.scratchAmount * spec.weightA + sensorB.scratchAmount * spec.weightB.
//
// The per-bone blend weights consumed by the position math are the w lanes of the spec's
// packed Vector3Plus offset records (vspltw ...,3), which equal mfWeightA / mfWeightB.
//
// NOTE: the X360 reads `*(sensor + 412)` (the local-space collision Sphere pointer) then
// loads its first 16 bytes — the sphere centre+radius (Vector4), NOT a transform row. We
// read that centre via a Vector4 view of the Sphere pointer so we don't depend on the
// Sphere's full layout (it has its own home; here it is forward-declared).

namespace BrnPhysics
{
namespace Deformation
{

void TagPoint::Construct(const TagPointSpec* lpType, const DeformationSensor* lpSensorArrayBase)
{
    // --- sensor-index range checks (4 asserts, lines 117-120 in BrnTagPoint.h) ------
    // The console compares the u16 index >= 0x8000u for the ">= 0" guard (a signed read
    // of the 16-bit field) and >= 20 for the upper bound. Reproduced two-sided via
    // CGS_ASSERT, with the baked d:\p4 path + line number dropped (project discards them).
    const s16 liSensorA = lpType->GetDeformationSensorA();
    const s16 liSensorB = lpType->GetDeformationSensorB();

    CGS_ASSERT(liSensorA >= 0, "lpType->GetDeformationSensorA() >= 0");
    CGS_ASSERT(liSensorA < KI_MAX_NUM_DEFORMABLE_SENSORS_PER_DEFORMABLE_OBJECT,
               "lpType->GetDeformationSensorA() < KI_MAX_NUM_DEFORMABLE_SENSORS_PER_DEFORMABLE_OBJECT");
    CGS_ASSERT(liSensorB >= 0, "lpType->GetDeformationSensorB() >= 0");
    CGS_ASSERT(liSensorB < KI_MAX_NUM_DEFORMABLE_SENSORS_PER_DEFORMABLE_OBJECT,
               "lpType->GetDeformationSensorB() < KI_MAX_NUM_DEFORMABLE_SENSORS_PER_DEFORMABLE_OBJECT");

    // --- store the back-pointers (result words 4,5,6) -------------------------------
    mpSpec    = lpType;                          // *(result + 4)
    mpSensorA = &lpSensorArrayBase[liSensorA];   // *(result + 5) ; console: base + 432*A
    mpSensorB = &lpSensorArrayBase[liSensorB];   // *(result + 6) ; console: base + 432*B

    const Vector3Plus& lInitial       = lpType->GetInitialPositionAndDetachThreshold();
    const Vector3Plus& lOffAndWeightA = lpType->GetOffsetAndWeightA(); // xyz=offsetA, w=weightA
    const Vector3Plus& lOffAndWeightB = lpType->GetOffsetAndWeightB(); // xyz=offsetB, w=weightB

    // Per-bone weights are the w lanes of the packed offset records (vspltw ...,3).
    const f32 lfWeightA = lOffAndWeightA.w;
    const f32 lfWeightB = lOffAndWeightB.w;

    // Centre of each sensor's local-space collision sphere (the lvx128 off the +412
    // sphere ptr loads its leading Vector4: centre.xyz + radius.w). Read as a Vector4
    // view so we don't depend on the Sphere's full layout.
    const Vector4& lCentreA = *reinterpret_cast<const Vector4*>(mpSensorA->GetLocalSpaceSphere());
    const Vector4& lCentreB = *reinterpret_cast<const Vector4*>(mpSensorB->GetLocalSpaceSphere());

    // posA = sphereA.centre + offsetA ; posB = sphereB.centre + offsetB (vaddfp).
    // blend = posA*weightA + posB*weightB (vmul / vmaddfp).
    Vector3 lBlend;
    lBlend.x = (lCentreA.x + lOffAndWeightA.x) * lfWeightA + (lCentreB.x + lOffAndWeightB.x) * lfWeightB;
    lBlend.y = (lCentreA.y + lOffAndWeightA.y) * lfWeightA + (lCentreB.y + lOffAndWeightB.y) * lfWeightB;
    lBlend.z = (lCentreA.z + lOffAndWeightA.z) * lfWeightA + (lCentreB.z + lOffAndWeightB.z) * lfWeightB;
    lBlend.w = (lCentreA.w + lOffAndWeightA.w) * lfWeightA + (lCentreB.w + lOffAndWeightB.w) * lfWeightB;

    // delta = blend - initialPosition ; mPos = delta*1.0 + initialPosition (== blend).
    // Reproduced verbatim (the sub/add round-trips) so the read/write data flow matches
    // the console; net effect is mPos = blend.
    mPos.x = (lBlend.x - lInitial.x) * 1.0f + lInitial.x;
    mPos.y = (lBlend.y - lInitial.y) * 1.0f + lInitial.y;
    mPos.z = (lBlend.z - lInitial.z) * 1.0f + lInitial.z;
    mPos.w = (lBlend.w - lInitial.w) * 1.0f + lInitial.w;

    // --- combined scratch amount (result word 7) ------------------------------------
    // *(result+7) = sensorA.scratch@+420 * spec.weightA@+48 + sensorB.scratch@+420 * spec.weightB@+52
    mfScratchAmount = (mpSensorA->GetScratchAmount() * lpType->GetWeightA())
                    + (mpSensorB->GetScratchAmount() * lpType->GetWeightB());
}

}
}
