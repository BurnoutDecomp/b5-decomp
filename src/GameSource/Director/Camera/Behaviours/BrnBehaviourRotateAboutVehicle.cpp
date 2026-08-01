// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourRotateAboutVehicle.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourRotateAboutVehicle slice this TU owns:
//   * GetCollisionPolicy  @0x821FB410  (inline in the header; anchored out-of-line below)
//   * SetParameters       @0x821F55B8  (inline in the header -- its assert cites the .h)
//   * BecomeSimilarTo     @0x8224A350  (BODIED HERE, 2026-08-01)
// The rest of the behaviour (Construct @0x8222BEC0, Update @0x822493C0, GetName @0x821FB488
// and the full rig) lands with its own TU.
// ============================================================================

#include <cmath>                                 // sqrtf (the folded vrsqrtefp + 2x Newton-Raphson)

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourRotateAboutVehicle.h"
#include "GameSource/Director/Camera/Camera.h"   // Camera::mTransform (BecomeSimilarTo's only read of the source camera)

namespace BrnDirector
{
namespace Camera
{

// Out-of-line anchor: forces the +0x50 accessor to be emitted in this TU.
// ⭐ RENAMED 2026-08-01: the accessor is GetCollisionPolicy(), and the "unrecoverable opaque
// sub-object" it returned is the embedded CollisionPolicyAttachedToVehicle -- pinned by
// BehaviourRotateAboutVehicle::Construct @0x8222BEDC, which calls that policy's Construct on
// this+0x50. See the header.
CollisionPolicyAttachedToVehicle*
BehaviourRotateAboutVehicle_GetCollisionPolicyAnchor(BehaviourRotateAboutVehicle& lrBehaviour)
{
    return lrBehaviour.GetCollisionPolicy();   // addi r3, r3, 0x50 ; blr
}

namespace
{
    // ------------------------------------------------------------------------
    // The two lane-wise primitives BecomeSimilarTo's VMX block reduces to.
    // rw::math::vpu::Vector3 carries NO operators (project rule), so the multiply-accumulate
    // that the console spells `vmaddfp` over a splatted scalar is written out by lane.
    // ------------------------------------------------------------------------

    // `vmulfp128 vD, splat(lfScale), lrColumn` -- the first term of an accumulation chain.
    rw::math::vpu::Vector3 ScaleVector(f32 lfScale, const rw::math::vpu::Vector3& lrColumn)
    {
        rw::math::vpu::Vector3 lResult;
        lResult.x = lfScale * lrColumn.x;
        lResult.y = lfScale * lrColumn.y;
        lResult.z = lfScale * lrColumn.z;
        lResult.w = lfScale * lrColumn.w;
        return lResult;
    }

    // `vmaddfp vD, splat(lfScale), vD, lrColumn` -- IDA prints vmaddfp in RAW FIELD ORDER
    // (VD, VA, VB, VC) and the semantic is VD = VA * VC + VB, so e.g.
    // `vmaddfp v0, v4, v0, v9` is v0 = v4 * v9 + v0: scale-by-splat then accumulate.
    void ScaleAndAdd(rw::math::vpu::Vector3& lrAccumulator, f32 lfScale,
                     const rw::math::vpu::Vector3& lrColumn)
    {
        lrAccumulator.x += lfScale * lrColumn.x;
        lrAccumulator.y += lfScale * lrColumn.y;
        lrAccumulator.z += lfScale * lrColumn.z;
        lrAccumulator.w += lfScale * lrColumn.w;
    }

    // `vcmpeqfp. v0, v0, v0` + the CR6 "all four lanes compared equal" bit: a float compares
    // unequal to ITSELF only when it is a NaN, so the self-compare is the console's NaN test.
    // (The three `stw r10, 0x70+var_20(r1)` in among the tests are dead spills of the bool
    // accumulator to its stack home, not logic.)
    bool IsNotNaN(f32 lfValue)
    {
        return lfValue == lfValue;
    }
}

// ============================================================================
// BehaviourRotateAboutVehicle::BecomeSimilarTo @0x8224A350   (116 asm lines)
//
// Re-seat the orbit-about-car camera so that it picks up where ANOTHER behaviour's camera
// currently is: take that camera's world position, express it in the anchored car's local
// frame, flatten it onto the car's horizontal plane, and adopt that as the orbit direction.
// Then wipe the free-look rotation state so the re-seated orbit starts un-rotated.
// This is the call the junkyard / car-select states make every frame while an ICE movie is
// driving the shot, so that the later interpolation ONTO the car has no discontinuity
// (BrnArbStateCarSelect::Update @0x8226F5D0, four PC call sites).
//
// ---- asm walk (r31 = this, r30 = the source camera) ------------------------
//   0x8224A370  addi r3, r31, 0x2A0 ; bl VehicleRef::Get      -> r3 = the VehicleInfo
//   0x8224A37C  addi r11, r3, 0x1F0                            -> &mRaceCarState.mTransform
//   0x8224A38C  lvx128 v0, r30, 0x30    the SOURCE CAMERA's mTransform.wAxis (its position)
//   0x8224A39C  lvx128 v11, r11, 0      \
//   0x8224A3A8  lvx128 v10, r11, 0x10    | the vehicle's world basis + position
//   0x8224A3B0  lvx128 v12, r11, 0x20    |
//   0x8224A390  lvx128  v9, r11, 0x30   /
//   0x8224A3A4  vsubfp v0, v13, v9                             -> -vehiclePos  (v13 == 0)
//   0x8224A3B4..0x8224A3D8   SIX vmrghw/vmrglw == the 4x4 (really 3x3) TRANSPOSE:
//                            v12 = {x.x, y.x, z.x, 0}   (column 0)
//                            v9  = {x.y, y.y, z.y, 0}   (column 1)
//                            v10 = {x.z, y.z, z.z, 0}   (column 2)
//   0x8224A3E8..0x8224A3F0   v0 = col2*(-pos).z + col1*(-pos).y + col0*(-pos).x
//   0x8224A3F8..0x8224A404   v0 += col0*cam.x + col1*cam.y + col2*cam.z
//                            => v0 = {dot(cam-pos, xAxis), dot(.., yAxis), dot(.., zAxis)}
//   0x8224A410..0x8224A41C   vperm(mask @0x82CDA350) + vrlimi128 lane 2 == the standard rw
//                            `Vector3(x, y, z)` construction codegen: Vector3(local.x, 0, local.z)
//   0x8224A420..0x8224A450   vmsum3fp128 dot3 + vrsqrtefp + TWO Newton-Raphson refinements,
//                            then v11 = flat * rsqrt(|flat|^2)   -- the NORMALISE
//   0x8224A454..0x8224A4A4   three `vcmpeqfp. vN,vN,vN` self-compares on lanes 0/1/2 of the
//                            NORMALISED vector, short-circuiting to false on the first NaN
//   0x8224A4B8  stvx128 v12, r31, 0x360     <-- stores v12, the UN-normalised flat vector
//   0x8224A4C4  else: mOrbitDirection = *(Vector3*)0x82181520 == (0, 0, 1, 0)
//   0x8224A4D8..0x8224A508  the ten-store reset of mRotationController (see the header)
//
// ⚠️⚠️ SHIPPED-CONSOLE QUIRK, REPRODUCED VERBATIM -- READ BEFORE "FIXING" THIS.
//   The console computes the normalised direction into v11, validates THAT, and then stores
//   v12 -- the UN-normalised flattened offset, whose length is the current orbit RADIUS (tens
//   of metres). The value it falls back to when the test fails is the exact unit vector
//   (0, 0, 1), and Construct @0x8222C04C seeds this same member from XMMatrixRotationY's
//   at-row, which is also unit. So the two "authored" seeds are unit and the re-seat path's is
//   not: the normalise result is computed, tested, and then discarded. This is a real defect in
//   the shipped X360 code, not a transcription gap -- v12 is provably untouched between the
//   `vmulfp128 v11, v12, v0` that produces the normalised copy and the `stvx128 v12` that
//   stores the raw one.
//   IT IS BENIGN TODAY, which is why it shipped: the ONLY consumer is Update @0x822495E0,
//   which passes mOrbitDirection to Utils::CreateLookAt as the eye against a zero target and
//   then reads back only rows 0..2 of the result (the ROTATION). CreateLookAt normalises
//   internally, so the magnitude is discarded before anything uses it.
//   DO NOT "correct" this to store the normalised vector: it would change nothing visible and
//   would break parity. If a future reader of mOrbitDirection needs a unit vector, normalise
//   AT THAT READER and note it there.
//
// ⚠️ mRotationController.Construct() IS AN EMPTY STUB TODAY (DirectorLinkStubs.cpp:522), and
//   this call site is the first one on a LIVE path. See the note at the call below.
// ============================================================================
void BehaviourRotateAboutVehicle::BecomeSimilarTo(const Camera& lrSourceCamera,
                                                  const AllVehicleData& lrAllVehicleData)
{
    // ---- the anchored car's world space -------------------------------------
    const VehicleInfo&    lrVehicle      = *mVehicleRef.Get(&lrAllVehicleData);
    const Matrix44Affine& lrVehicleSpace = lrVehicle.mRaceCarState.mTransform;   // vehicle +0x1F0

    // ---- the 3x3 transpose (== the inverse rotation; the basis is orthonormal) ----
    // Columns of the vehicle basis, i.e. the rows of its transpose. Lane 3 is zero in all
    // three (the six vmrghw/vmrglw merge the basis rows against a zeroed register), which is
    // what keeps the w lane out of the accumulation below.
    const Vector3 lColumn0 = { lrVehicleSpace.xAxis.x, lrVehicleSpace.yAxis.x, lrVehicleSpace.zAxis.x, 0.0f };
    const Vector3 lColumn1 = { lrVehicleSpace.xAxis.y, lrVehicleSpace.yAxis.y, lrVehicleSpace.zAxis.y, 0.0f };
    const Vector3 lColumn2 = { lrVehicleSpace.xAxis.z, lrVehicleSpace.yAxis.z, lrVehicleSpace.zAxis.z, 0.0f };

    const Vector3& lrVehiclePosition = lrVehicleSpace.wAxis;
    const Vector3& lrCameraPosition  = lrSourceCamera.mTransform.wAxis;   // camera +0x30

    // ---- the source camera's world position, expressed in the car's local frame ----
    // Kept as the console's TWO accumulation groups (inverse translation first, then the
    // camera term) rather than folded to `transpose * (camera - vehicle)`, so the order of
    // operations matches the asm.
    Vector3 lLocalOffset = ScaleVector(-lrVehiclePosition.z, lColumn2);   // vmulfp128 v0, v8, v10
    ScaleAndAdd(lLocalOffset, -lrVehiclePosition.y, lColumn1);            // vmaddfp   v0, v4, v0, v9
    ScaleAndAdd(lLocalOffset, -lrVehiclePosition.x, lColumn0);            // vmaddfp   v0, v11, v0, v12
    ScaleAndAdd(lLocalOffset,  lrCameraPosition.x,  lColumn0);            // vmaddfp   v0, v12, v0, v7
    ScaleAndAdd(lLocalOffset,  lrCameraPosition.y,  lColumn1);            // vmaddfp   v0, v9, v0, v6
    ScaleAndAdd(lLocalOffset,  lrCameraPosition.z,  lColumn2);            // vmaddfp   v0, v10, v0, v5

    // ---- flatten onto the car's horizontal plane ----------------------------
    // Lane 1 of the local offset is the component along the car's UP axis; dropping it keeps
    // the orbit level with the car. The console leaves lLocalOffset.x in the w lane (the
    // vperm mask @0x82CDA350 broadcasts lane 0 into lanes 0/2/3 before the vrlimi128 overwrites
    // lane 2) -- that is Vector3's undefined 4th lane, written 0.0f here to match the tree's
    // established Vector3(x,y,z) reconstruction (Camera::CreateHeadingSpaceLookAt uses the
    // identical codegen with the identical mask).
    const Vector3 lFlatOffset = { lLocalOffset.x, 0.0f, lLocalOffset.z, 0.0f };

    // ---- normalise, purely to detect the degenerate case --------------------
    // The console runs vrsqrtefp + two Newton-Raphson steps, which converges to the true
    // reciprocal square root; folded to the scalar divide per the rw vpu reconstruction
    // precedent. The DEGENERATE behaviour is preserved: a zero-length offset (the camera
    // exactly above/below the car) yields an infinite scale, and the y lane's 0 * inf is a NaN
    // on both targets, so the test below fails identically.
    const f32 lfLengthSquared = lFlatOffset.x * lFlatOffset.x
                              + lFlatOffset.y * lFlatOffset.y
                              + lFlatOffset.z * lFlatOffset.z;         // vmsum3fp128 v0, v12, v12
    const f32 lfReciprocalLength = 1.0f / sqrtf(lfLengthSquared);

    const Vector3 lNormalised = { lFlatOffset.x * lfReciprocalLength,   // vmulfp128 v11, v12, v0
                                  lFlatOffset.y * lfReciprocalLength,
                                  lFlatOffset.z * lfReciprocalLength,
                                  lFlatOffset.w * lfReciprocalLength };

    if (IsNotNaN(lNormalised.x) && IsNotNaN(lNormalised.y) && IsNotNaN(lNormalised.z))
    {
        // ⚠️ the RAW flattened offset, NOT lNormalised -- see the SHIPPED-CONSOLE QUIRK banner.
        mOrbitDirection = lFlatOffset;                                 // stvx128 v12, r31, 0x360
    }
    else
    {
        mOrbitDirection = { 0.0f, 0.0f, 1.0f, 0.0f };                  // .rdata @0x82181520
    }

    // ---- wipe the free-look rotation state ----------------------------------
    // ⚠️ SILENT-DROP STUB ON A LIVE PATH: CameraSphericalRotationController::Construct is an
    // EMPTY body in GameSource/Director/DirectorLinkStubs.cpp:522. That was argued safe because
    // its only other caller (BehaviourGameplayExternal::Construct) runs on a freshly
    // placement-new'd, zero-initialised object -- but THIS caller does not. BecomeSimilarTo runs
    // on a behaviour that has been live for many frames, and the whole point of these ten stores
    // is to throw away the accumulated stick yaw / pitch / lookback state so the re-seated orbit
    // starts neutral. With the empty stub the stale rotation survives the re-seat.
    // The real body IS fully attested (three witnesses, spelled out in the header's layout
    // note) -- it just cannot be written from this file, because DirectorLinkStubs.cpp owns the
    // symbol and an inline in the canonical header would collide with it.
    mRotationController.Construct();   // stvx128 0,+0x20 / +0x30..+0x42 / +0x48 / +0x4C
}

} // namespace Camera
} // namespace BrnDirector
