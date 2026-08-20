// ============================================================================
// GameSource/Director/Camera/BrnDepthOfField.cpp
//
// Compilation home for the BrnDirector::Camera::DepthOfField slice this TU owns:
//   - DepthOfField::SetParams @0x821F1AC8
//
// Called by BrnDirector::KeyAnimController::UpdateFocus, BehaviourIceAnim::Update and
// ICE::ICECameraMover::UpdateFocus to drive the camera's focus band each frame.
// ============================================================================

#include "GameSource/Director/Camera/BrnDepthOfField.h"

namespace BrnDirector
{
namespace Camera
{

// ----------------------------------------------------------------------------
// BrnDirector::Camera::DepthOfField::SetParams @0x821F1AC8
//
// The asm validates the band ordering + the blurriness range with a chain of fcmpu/assert
// guards (each a Begin/Fire/End sequence on failure), then commits the five f32 stores:
//   stfs f30, 0x10(r30)  ; mfBlurriness
//   stfs f29, 0(r30)     ; mfFocusStartDistanceMeters
//   stfs f28, 4(r30)     ; mfPerfectFocusStartDistanceMeters
//   stfs f27, 8(r30)     ; mfPerfectFocusEndDistanceMeters
//   stfs f26, 0xC(r30)   ; mfFocusEndDistanceMeters
//
// Guards (asm fcmpu predicate -> asserted condition):
//   f30 >= 0.0f                         -> "lfBlurriness >= 0.0f"
//   f30 <= 1.0f                         -> "lfBlurriness <= 1.0f"
//   f29 >= 0.0f                         -> "lfFocusStartDistanceMeters >= 0.0f"
//   f28 >= f29                          -> "lfPerfectFocusStartDistanceMeters >= lfFocusStartDistanceMeters"
//   f27 >= f28                          -> "lfPerfectFocusEndDistanceMeters >= lfPerfectFocusStartDistanceMeters"
//   f26 >= f27                          -> "lfFocusEndDistanceMeters >= lfPerfectFocusEndDistanceMeters"
// ----------------------------------------------------------------------------
void DepthOfField::SetParams(f32 lfFocusStartDistanceMeters,
                             f32 lfPerfectFocusStartDistanceMeters,
                             f32 lfPerfectFocusEndDistanceMeters,
                             f32 lfFocusEndDistanceMeters,
                             f32 lfBlurriness)
{
    CGS_ASSERT(lfBlurriness >= 0.0f, "lfBlurriness >= 0.0f");
    CGS_ASSERT(lfBlurriness <= 1.0f, "lfBlurriness <= 1.0f");
    CGS_ASSERT(lfFocusStartDistanceMeters >= 0.0f, "lfFocusStartDistanceMeters >= 0.0f");
    CGS_ASSERT(lfPerfectFocusStartDistanceMeters >= lfFocusStartDistanceMeters,
               "lfPerfectFocusStartDistanceMeters >= lfFocusStartDistanceMeters");
    CGS_ASSERT(lfPerfectFocusEndDistanceMeters >= lfPerfectFocusStartDistanceMeters,
               "lfPerfectFocusEndDistanceMeters >= lfPerfectFocusStartDistanceMeters");
    CGS_ASSERT(lfFocusEndDistanceMeters >= lfPerfectFocusEndDistanceMeters,
               "lfFocusEndDistanceMeters >= lfPerfectFocusEndDistanceMeters");

    mfBlurriness                      = lfBlurriness;                      // stfs f30, 0x10
    mfFocusStartDistanceMeters        = lfFocusStartDistanceMeters;        // stfs f29, 0x00
    mfPerfectFocusStartDistanceMeters = lfPerfectFocusStartDistanceMeters; // stfs f28, 0x04
    mfPerfectFocusEndDistanceMeters   = lfPerfectFocusEndDistanceMeters;   // stfs f27, 0x08
    mfFocusEndDistanceMeters          = lfFocusEndDistanceMeters;          // stfs f26, 0x0C
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::DepthOfField::Construct (destub wave 2026-07-26)
//
// Default-initialise the focus band. INLINED on the X360: both director-camera
// bring-up paths (Camera::Construct @0x82255E68 and Camera::Clear @0x8223CE70)
// emit the same five raw stfs into the DOF block at camera +0x124..+0x134 --
// flt_82004014(0.1) / flt_82004744(0.2) / flt_82004740(0.3) / flt_8200473C(0.4)
// / 0.0 -- with NO SetParams range asserts, so this is a plain default-init.
// ----------------------------------------------------------------------------
void DepthOfField::Construct()
{
    mfFocusStartDistanceMeters        = 0.1f;   // +0x00
    mfPerfectFocusStartDistanceMeters = 0.2f;   // +0x04
    mfPerfectFocusEndDistanceMeters   = 0.3f;   // +0x08
    mfFocusEndDistanceMeters          = 0.4f;   // +0x0C
    mfBlurriness                      = 0.0f;   // +0x10
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::DepthOfField::GetBlurriness / ::SetBlurriness
//
// The single-lane read/write of mfBlurriness (+0x10). INLINED on the X360 everywhere it is
// used -- BrnLooker's Zoom path ramps the lane (`lfs f0, 0x134(camera)` / `stfs f0, 0x134`,
// i.e. DOF block +0x10 with the block at camera +0x124), ArbStateCrashNav stores its
// crash-nav constant into the same word, and DepthOfField::SetParams above writes it from
// its own fifth argument. There is therefore no standalone console body to walk; the
// operation is a plain field access on the member SetParams already pins store-for-store.
//
// ⚠️ NOT wrapped in SetParams' range asserts on purpose: the console's inlined single-lane
// stores carry no assert prologue (SetParams' six fcmpu guards are inside SetParams), and
// adding one here would fire on callers the console lets through.
// ----------------------------------------------------------------------------
f32 DepthOfField::GetFocusStartDistanceMeters() const
{
    return mfFocusStartDistanceMeters;          // +0x00
}

f32 DepthOfField::GetPerfectFocusStartDistanceMeters() const
{
    return mfPerfectFocusStartDistanceMeters;   // +0x04
}

f32 DepthOfField::GetPerfectFocusEndDistanceMeters() const
{
    return mfPerfectFocusEndDistanceMeters;     // +0x08
}

f32 DepthOfField::GetFocusEndDistanceMeters() const
{
    return mfFocusEndDistanceMeters;            // +0x0C
}

f32 DepthOfField::GetBlurriness() const
{
    return mfBlurriness;
}

void DepthOfField::SetBlurriness(f32 lfBlurriness)
{
    mfBlurriness = lfBlurriness;
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::DepthOfField::SetStaticParams @0x82204528 (sub_82204528)
//
// The BrnLooker mbUseStaticDOF leg: turn a FOCAL DISTANCE and an authored DOF setting into a
// focus band centred on that distance, at full blurriness. It had NO body anywhere in the
// tree -- declared at BrnDepthOfField.h:74, called once (BrnLooker.cpp:393) -- so the moment
// BrnLooker.cpp joins the link it is an LNK2019.
//
// IDENTIFICATION. The export is unnamed, but its three asserts carry the console __FILE__
// "d:/p4/b5_main/burnout/main/code/gamesource/director/camera/BrnDepthOfField.h"
// __LINE__ 178 / 179 / 180 (`li r5, 0xB2 / 0xB3 / 0xB4`) and the texts "lfFocalDistance >= 0.0f",
// "lfClearDepth >= 0.0f", "lfDepthOfField >= lfClearDepth"; r3 is the DOF block and the four
// stores land on +0x00/+0x04/+0x08/+0x0C with +0x10 forced to 1.0f. That is this class, this
// header, and the setter BrnLooker's `sub_82204528(camera+0x124, focalLength, dof)` call site
// names. (On the console it is a header inline, which is why the asserts cite the .h; it is
// bodied here because this TU is the class's compilation home.)
//
// THE ASM, LINE FOR LINE (0x82204528-0x82204688). f1 == lfFocalLength, f2 == lfDepthOfField
// (PPC float args; both are FPRs, neither burns a GPR that this body reads):
//   fadds f12, f2, f27(1.0) / fmuls f0, f12, flt_8200426C(5.0)   ; lfDepthOfField = (dof+1)*5
//   fcmpu f0, flt_82004A20(10.0) / ble ...                        ; only past 10 does the
//   fsubs f12, f0, 10.0 / fmuls f13, f12, flt_82004744(0.2)       ;   PERFECT (clear) span open
//   fsel f12, -f0,  0.0, f0    -> max(lfDepthOfField, 0)
//   fsel f13, -f13, 0.0, f13   -> max(lfClearDepth,   0)
//   fsel f11, -f1,  0.0, f1    -> max(lfFocalLength,  0)
//   fsel f29, 10000-f12, f12, flt_82005D9C(10000.0)  -> lfDepthOfField clamped to [0, 10000]
//   fsel f30, 10000-f11, f11, 10000.0                -> lfFocalDistance clamped to [0, 10000]
//   fsel f28, f29-f13, f13, f29                      -> lfClearDepth = min(lfClearDepth, span)
//   ... the three asserts ...
//   stfs f27,  0x10(r30)   ; mfBlurriness = 1.0f  -- UNCONDITIONAL, this leg is always full blur
//   stfs f30 - f29*0.5, 0x00 / f30 - f28*0.5, 0x04 / f30 + f28*0.5, 0x08 / f30 + f29*0.5, 0x0C
// i.e. a band SYMMETRIC about the focal distance: outer half-width lfDepthOfField/2, inner
// half-width lfClearDepth/2 (flt_82001DA0 == 0.5f).
//
// CONSTANT PROVENANCE -- every one has a second witness, none is a Hex-Rays literal alone:
//   flt_82001C98 == 1.0f      / flt_82001CC0 == 0.0f    (DATA_DUMP.md PART 2, 0x3F800000 / 0)
//   flt_8200426C == 5.0f      -- `lfs f8` / `stfs f8, 0x3C(r3)` in Looker::Parameters::Construct
//                                @0x821F8D80, whose pseudocode prints `*(result + 60) = 5.0`
//   flt_82004A20 == 10.0f     -- BrnPostFx.cpp:224 KF_DOF_FOCAL_PLANE_1
//   flt_82004744 == 0.2f      -- this file's own Construct banner (camera +0x128 default)
//   flt_82005D9C == 10000.0f  -- Utils::PointWillLeaveFrustrum @0x8220D150 stores the same slot
//                                into two stack floats its pseudocode prints as 10000.0
//   flt_82001DA0 == 0.5f      -- dumped (BrnPostFx.cpp's composite banner, DATA_DUMP.md:1552)
//
// The `fsel` idioms are written as the tree's rw-style Min/Max expressions (the same de-fsel the
// rest of the camera family uses); they differ only for NaN, which the asserts below already
// exclude.
//
// ⚠️ NOTE FOR THE READER CHASING DEPTH OF FIELD: this setter is currently UNREACHABLE, because
// BrnLooker.cpp -- its only caller -- is not in the link (build_game_exe.bat:2174-2178: that TU
// does not compile, BrnLooker.cpp:189 calls a retired 3-argument rw::math::vpu::SLerp). Landing
// the body removes one of the two blockers on mounting it; the SLerp re-fit is the other.
// ----------------------------------------------------------------------------
void DepthOfField::SetStaticParams(f32 lfFocalLength, f32 lfDepthOfField)
{
    // The authored DOF setting is remapped before anything else: (dof + 1) * 5 metres of total
    // span, and the PERFECTLY-focused core only starts to open once that span passes 10 m.
    const f32 KF_DOF_SPAN_BIAS        = 1.0f;      // flt_82001C98
    const f32 KF_DOF_SPAN_SCALE       = 5.0f;      // flt_8200426C
    const f32 KF_CLEAR_SPAN_THRESHOLD = 10.0f;     // flt_82004A20
    const f32 KF_CLEAR_SPAN_SCALE     = 0.2f;      // flt_82004744
    const f32 KF_MAX_DISTANCE_METERS  = 10000.0f;  // flt_82005D9C
    const f32 KF_HALF                 = 0.5f;      // flt_82001DA0
    const f32 KF_FULL_BLURRINESS      = 1.0f;      // flt_82001C98 -> +0x10

    f32 lfSpanMeters = (lfDepthOfField + KF_DOF_SPAN_BIAS) * KF_DOF_SPAN_SCALE;

    f32 lfClearDepth = 0.0f;                       // fmr f13, f31 -- zero unless the branch runs
    if (lfSpanMeters > KF_CLEAR_SPAN_THRESHOLD)
        lfClearDepth = (lfSpanMeters - KF_CLEAR_SPAN_THRESHOLD) * KF_CLEAR_SPAN_SCALE;

    // The three fsel floors, then the two fsel ceilings, then the clear-depth cap.
    if (lfSpanMeters < 0.0f)  lfSpanMeters = 0.0f;
    if (lfClearDepth < 0.0f)  lfClearDepth = 0.0f;
    f32 lfFocalDistance = (lfFocalLength < 0.0f) ? 0.0f : lfFocalLength;

    if (lfSpanMeters    > KF_MAX_DISTANCE_METERS) lfSpanMeters    = KF_MAX_DISTANCE_METERS;
    if (lfFocalDistance > KF_MAX_DISTANCE_METERS) lfFocalDistance = KF_MAX_DISTANCE_METERS;
    if (lfClearDepth    > lfSpanMeters)           lfClearDepth    = lfSpanMeters;

    CGS_ASSERT(lfFocalDistance >= 0.0f, "lfFocalDistance >= 0.0f");              // .h:178
    CGS_ASSERT(lfClearDepth    >= 0.0f, "lfClearDepth >= 0.0f");                 // .h:179
    CGS_ASSERT(lfSpanMeters    >= lfClearDepth, "lfDepthOfField >= lfClearDepth"); // .h:180

    mfBlurriness                      = KF_FULL_BLURRINESS;                       // stfs f27, 0x10
    mfFocusStartDistanceMeters        = lfFocalDistance - (lfSpanMeters * KF_HALF);   // 0x00
    mfPerfectFocusStartDistanceMeters = lfFocalDistance - (lfClearDepth * KF_HALF);   // 0x04
    mfPerfectFocusEndDistanceMeters   = lfFocalDistance + (lfClearDepth * KF_HALF);   // 0x08
    mfFocusEndDistanceMeters          = lfFocalDistance + (lfSpanMeters * KF_HALF);   // 0x0C
}

} // namespace Camera
} // namespace BrnDirector
