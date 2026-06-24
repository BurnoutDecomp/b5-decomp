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

} // namespace Camera
} // namespace BrnDirector
