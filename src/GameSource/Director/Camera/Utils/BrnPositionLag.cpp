// ============================================================================
// GameSource/Director/Camera/Utils/BrnPositionLag.cpp
//
// Bodies for BrnDirector::Camera::Utils::PositionLag.
//
// Construct / Parameters::Construct are simple resets. Update @0x821F8F08 is a KEYSTONE: see
// the documented floor at its definition below.
// ============================================================================

#include "GameSource/Director/Camera/Utils/BrnPositionLag.h"
#include "rw/math/vpu/vector3_operation.h"        // rw::math::vpu Vector3 Mult / Lerp / operator+
#include "rw/math/vpu/matrix44affine_operation.h" // InverseOfMatrixWithOrthonormal3x3 / TransformPoint / TransformVector

// Parameters::Serialise<S> drives the camera-tunings serialiser S by name; each S supplies the
// per-field scalar overloads (write=FormatName+fprintf, read=fscanf, debug-menu=Process+SetStep).
// Pull in the three serialisers this block is saved/loaded/tuned through so the explicit
// instantiations below have their full definitions available.
#include "GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h"
#include "GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.h"
#include "GameSource/Director/Camera/Behaviours/BrnDebugMenuSerialiser.h"

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{

// ----------------------------------------------------------------------------
// BrnDirector::Camera::Utils::PositionLag::Parameters::Construct
//   Seed the tunable block to its defaults: all three per-axis response rates to 1.0 and the
//   overall smoothing to 0.5. muVersion (+0x00) is left untouched (the serialiser stamps it).
//   Values recovered from the PS3 DecFIGS body
//   (._ZN11BrnDirector6Camera5Utils11PositionLag10Parameters9ConstructEv @0x16A20: stores
//   +0x04/+0x08/+0x0C = 1.0 and +0x10 = 0.5).
// ----------------------------------------------------------------------------
void PositionLag::Parameters::Construct()
{
    mfXResponse = 1.0f;   // +0x04
    mfYResponse = 1.0f;   // +0x08
    mfZResponse = 1.0f;   // +0x0C
    mfSmoothing = 0.5f;   // +0x10
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::Utils::PositionLag::Construct
//   Raise the first-frame and constructed flags so the first Update seeds the carried
//   last-offset / last-position from the live transform rather than easing from stale data.
//   The PS3 DecFIGS body
//   (._ZN11BrnDirector6Camera5Utils11PositionLag9ConstructEv @0x16A3C) stores ONLY the two
//   bools at +0x20 / +0x21 = 1; the carried Vector3s are NOT cleared here (the first-frame
//   path in Update seeds them).
// ----------------------------------------------------------------------------
void PositionLag::Construct()
{
    mbFirstFrame  = true;   // +0x20
    mbConstructed = true;   // +0x21
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::Utils::PositionLag::Update @0x821F8F08
//
// Ease the transform's translation toward its lagged position this frame. The console body
// is a hand-vectorised VMX cascade; reconstructed here as the faithful per-lane rw::math::vpu
// matrix/vector ops it computes (every lane verified against the vmrglw/vmrghw/vmaddfp/vsubfp
// stream). The stages, in asm order:
//
//   1. lInverseTransform = InverseOfMatrixWithOrthonormal3x3(lrTransform). The 3x3 transpose
//      is the vmrglw/vmrghw lane-merge permute block @0x821F8F7C..94; the inverse translation
//      is `vsubfp v13,0,Pos` (@0x821F8F70) then the vmaddfp dot-chain @0x821F8FA4..AC, whose
//      lanes are exactly -(Pos . xAxis), -(Pos . yAxis), -(Pos . zAxis).
//   2. First frame only (mbFirstFrame, lbz 0x20(r31)): seed the carried state -- store the
//      current Pos into mLastWorldPosition (stvx128 v12,r31,0x10) and the inverse-transformed
//      Pos into mLastLocalOffset (the vmaddfp accumulate @0x821F8FC4..D8, which is identically
//      the origin), then clear the latch (stb 0,0x20(r31)).
//   3. Steady state (always): TransformPoint the OLD mLastWorldPosition into the inverted local
//      frame (vmaddfp cascade @0x821F900C..28 against the carried world position read at
//      0x821F8FF0 BEFORE it is overwritten at 0x821F8FF8 with the current Pos), then ease the
//      carried local offset toward it by (1 - mfSmoothing) -- the scalar `1.0 - mfSmoothing`
//      (flt_82001C98 = 1.0f @0x821F9010, fsubs @0x821F9014) broadcast (lvlx/vspltw @0x821F901C)
//      and folded into the vmaddfp lerp @0x821F9030. Store back to mLastLocalOffset.
//   4. Scale the eased local offset component-wise by the per-axis response vector
//      (mfXResponse/mfYResponse/mfZResponse staged @0x821F9038..50 with a 0 in the 4th lane,
//      vmulfp128 @0x821F9068), TransformVector it by the rotation back to world space (vmaddfp
//      cascade @0x821F907C..84 over the original xAxis/yAxis/zAxis rows reloaded at
//      0x821F9058..60), and add onto the transform's translation (vaddfp @0x821F9088;
//      stvx128 @0x821F908C writes lrTransform.Pos()). lfTimestep (a3/r5) is unused by the body.
// ----------------------------------------------------------------------------
void PositionLag::Update(const Parameters& lrParameters, f32 lfTimestep, rw::math::vpu::Matrix44Affine& lrTransform)
{
    using namespace rw::math::vpu;

    CGS_ASSERT(mbConstructed, "mbConstructed");

    // (1) Inverse of the orthonormal affine: transpose the 3x3, inverse-transform the position.
    const Matrix44Affine lInverseTransform = InverseOfMatrixWithOrthonormal3x3(lrTransform);

    // (2) First frame: seed the carried world position and the inverse-transformed offset.
    if (mbFirstFrame)
    {
        mLastWorldPosition = lrTransform.Pos();
        mLastLocalOffset   = TransformPoint(lInverseTransform, lrTransform.Pos());
        mbFirstFrame       = false;
    }

    // (3) Project the previous world position into the current inverted local frame, then
    //     swap the carried world position to the current Pos (asm reads the old value first).
    const Vector3 lLocalOffset = TransformPoint(lInverseTransform, mLastWorldPosition);
    mLastWorldPosition = lrTransform.Pos();

    // Ease the carried local offset toward the freshly projected one by (1 - mfSmoothing).
    mLastLocalOffset = Lerp(mLastLocalOffset, lLocalOffset, 1.0f - lrParameters.mfSmoothing);

    // (4) Scale by the per-axis response, rotate the eased offset back to world space, and
    //     add it onto the transform's translation.
    const Vector3 lResponse{ lrParameters.mfXResponse, lrParameters.mfYResponse, lrParameters.mfZResponse, 0.0f };
    const Vector3 lScaledOffset = Mult(mLastLocalOffset, lResponse);
    lrTransform.Pos() = lrTransform.Pos() + TransformVector(lrTransform, lScaledOffset);

    (void)lfTimestep;
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::Utils::PositionLag::Parameters::Serialise<S> -- the ONE position-lag
// field-walk visitor body. Instantiated over the three camera-tunings serialisers:
//   - Serialise<TextFileWriteSerialiser> @0x82215F50
//   - Serialise<TextFileReadSerialiser>  @0x82203020
//   - Serialise<DebugMenuSerialiser>     @0x82215608
//
// Stamps the version tag (KU_VERSION = 1), hands the version field to S, then -- only when the
// version still reads 1 -- walks the four tunable f32 fields (ascending offset) into S by name.
// The body is uniform across S; the three asm instances differ only because each S's inlined
// scalar overloads expand differently:
//   - TextFileWriteSerialiser: version -> FormatName+fprintf "%s : %d\n" (inlined, guarded on
//     mpFile); each f32 -> out-of-line TextFileWriteSerialiser::Serialise(name, f32&) call.
//   - TextFileReadSerialiser:  version -> fscanf "%s : %d\n" (inlined, guarded on *mpFile); each
//     f32 -> fscanf "%s : %f\n" (inlined, each re-guarded on *mpFile). Here the version read can
//     genuinely change muVersion, so the `== KU_VERSION` gate is load-bearing on read.
//   - DebugMenuSerialiser: its u32 (version) overload registers nothing, so the version call and
//     the always-true gate fold away in the asm (which stores 1 then walks straight into the
//     floats); each f32 -> Process<float>(name, value) + mpDebugComponent->SetStep(&value, 0.01f)
//     (inlined). The differing per-field direction is entirely inside each S's overloads (their
//     own TUs); this visitor body is a single source template.
//
// Field/label map (from the write+read+debug-menu asm, ascending offset):
//   +0x00 muVersion    "Version Number (dont change)"   (u32, = KU_VERSION)
//   +0x04 mfXResponse  "X Response"
//   +0x08 mfYResponse  "Y Response"
//   +0x0C mfZResponse  "Z Response"
//   +0x10 mfSmoothing  "Smoothing"
// ----------------------------------------------------------------------------
template<class TSerialiser>
void PositionLag::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    const u32 KU_VERSION = 1u;                                 // li r_,1 ; stw ...,0(block)

    muVersion = KU_VERSION;
    lrSerialiser.Serialise("Version Number (dont change)", muVersion);
    if (muVersion == KU_VERSION)
    {
        lrSerialiser.Serialise("X Response", mfXResponse);     // +0x04
        lrSerialiser.Serialise("Y Response", mfYResponse);     // +0x08
        lrSerialiser.Serialise("Z Response", mfZResponse);     // +0x0C
        lrSerialiser.Serialise("Smoothing",  mfSmoothing);     // +0x10
    }
}

// Explicit instantiations -- one per serialiser this block is saved/loaded/tuned through.
// (Unqualified names resolve to the enclosing BrnDirector::Camera namespace.)
template void PositionLag::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void PositionLag::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);
template void PositionLag::Parameters::Serialise<Camera::DebugMenuSerialiser>(Camera::DebugMenuSerialiser&);

} // namespace Utils
} // namespace Camera
} // namespace BrnDirector
