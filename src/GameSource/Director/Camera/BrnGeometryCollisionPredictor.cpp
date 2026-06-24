// ============================================================================
// GameSource/Director/Camera/BrnGeometryCollisionPredictor.cpp
//
// Compilation home for the BrnDirector::Camera::GeometryCollisionPredictor slice this TU owns:
//   - GeometryCollisionPredictor::GetTimeUntilCollision @0x821F36C0
//
// Called by BrnDirector::Camera::VisibilityCollisionPolicy::ProcessSceneQueryResults after a
// scene query reports the camera ray hit geometry.
// ============================================================================

#include "GameSource/Director/Camera/BrnCollisionPolicy.h"

namespace BrnDirector
{
namespace Camera
{

// ----------------------------------------------------------------------------
// BrnDirector::Camera::GeometryCollisionPredictor::GetTimeUntilCollision @0x821F36C0
//   lbz   r11, 0x64(this)      ; mbWillCollide
//   cmplwi r11, 0              ; assert it is set (a collision was predicted)
//   bne   skip                 ; (asm asserts when CLEAR, i.e. !mbWillCollide)
//   ... Begin/Fire/End assert ...
//   lfs   f1, 0x60(this)       ; return mfTimeUntilCollision
// ----------------------------------------------------------------------------
f32 GeometryCollisionPredictor::GetTimeUntilCollision() const
{
    CGS_ASSERT(mbWillCollide, "mbWillCollide");   // lbz 0x64; assert set
    return mfTimeUntilCollision;                  // lfs f1, 0x60(this)
}

} // namespace Camera
} // namespace BrnDirector
