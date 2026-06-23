// ============================================================================
// GameSource/Director/Camera/Utils/BrnPositionLag.cpp
//
// Bodies for BrnDirector::Camera::Utils::PositionLag.
//
// Construct / Parameters::Construct are simple resets. Update @0x821F8F08 is a KEYSTONE: see
// the documented floor at its definition below.
// ============================================================================

#include "GameSource/Director/Camera/Utils/BrnPositionLag.h"

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{

// ----------------------------------------------------------------------------
// BrnDirector::Camera::Utils::PositionLag::Parameters::Construct
//   A trivial reset of the tunable block. The console body leaves the response/smoothing
//   fields for the serialiser to populate; the version tag is the only fixed seed. The
//   recovered body has no observed stores beyond the construct frame, so this mirrors the
//   no-op shape (the serialiser fills the fields). FLAG: the exact default field values are
//   not recovered from rodata; the reset is the recovered no-op shape.
// ----------------------------------------------------------------------------
void PositionLag::Parameters::Construct()
{
    // No fixed seeds recovered; the serialiser populates muVersion / responses / smoothing.
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::Utils::PositionLag::Construct
//   Reset the carried state so the first Update seeds the last-offset / last-position from
//   the live transform rather than easing from stale data.
// ----------------------------------------------------------------------------
void PositionLag::Construct()
{
    mLastLocalOffset.SetZero();
    mLastWorldPosition.SetZero();
    mbFirstFrame  = true;
    mbConstructed = true;
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::Utils::PositionLag::Update @0x821F8F08
//
// KEYSTONE -- DOCUMENTED FLOOR (body intentionally NOT reconstructed).
//
// The console body is a multi-stage hand-vectorised VMX/AltiVec pipeline that does NOT lower
// to a faithful per-lane scalar formula:
//
//   1. InverseOfMatrixWithOrthonormal3x3(lrTransform): the 3x3 rotation is inverted by a
//      register TRANSPOSE built from vmrglw/vmrghw lane-merge permutes (v8..v11), and the
//      translation is inverse-transformed via `vsubfp v13, 0, pos` then a vmaddfp dot-chain
//      against the transposed rows. There is no scalar transpose-permute equivalent that
//      reproduces the lane traffic store-for-store.
//   2. On the first frame it seeds mLastWorldPosition / mLastLocalOffset from the transformed
//      target (the vmaddfp accumulate of the transposed rows against the position).
//   3. It builds a per-axis ease vector from (1 - mfSmoothing) and the X/Y/Z response fields,
//      TransformPoints the last world position into the inverted local frame, eases the new
//      local offset toward the carried one by that vector (vmaddfp lerp), then TransformVector
//      / Pos-accumulates the eased offset back onto the transform's translation.
//
// Stages (1) and (3) are exactly the "multi-stage VMX pipeline" the reconstruction rules say
// must NOT be paraphrased or given an invented per-lane formula. Faithfully reproducing them
// needs the rw::math VMX matrix-operation vocabulary (InverseOfMatrixWithOrthonormal3x3,
// TransformPoint, TransformVector, the broadcast-VecFloat Lerp) which is not modelled in the
// reconstructed rw math home (rw/math/vpu has only the flat Vector3 op set). Rather than
// fabricate the matrix inverse / strided transforms, this is left as an honest floor: the
// carried state is seeded and the assert is reproduced, but the transform is passed through
// UNCHANGED (the zero-lag identity). This is NOT the console behaviour for the easing math.
//
// Re-home this with the real body when the rw::math VMX Matrix44Affine operation vocabulary
// (matrix inverse + TransformPoint/TransformVector + broadcast Lerp) is reconstructed; the
// member layout and the Parameters fields the pipeline reads are pinned and stable.
// ----------------------------------------------------------------------------
void PositionLag::Update(const Parameters& lrParameters, f32 lfTimestep, rw::math::vpu::Matrix44Affine& lrTransform)
{
    CGS_ASSERT(mbConstructed, "mbConstructed");

    // FLOOR: seed the carried state on the first frame so a later faithful body has correct
    // carry-in, and clear the first-frame latch (this much of the console body IS recovered).
    if (mbFirstFrame)
    {
        mLastWorldPosition = lrTransform.wAxis;   // the transform's translation row
        mbFirstFrame = false;
    }

    // KEYSTONE FLOOR: the per-axis eased lag (matrix-inverse -> TransformPoint -> per-axis
    // Lerp -> TransformVector -> Pos accumulate) is NOT reconstructed. The transform is left
    // unchanged (zero-lag). See the header note above. Do not treat this as the console math.
    (void)lrParameters;
    (void)lfTimestep;
}

} // namespace Utils
} // namespace Camera
} // namespace BrnDirector
