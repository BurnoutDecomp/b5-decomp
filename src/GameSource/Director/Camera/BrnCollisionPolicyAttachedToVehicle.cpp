// ============================================================================
// GameSource/Director/Camera/BrnCollisionPolicyAttachedToVehicle.cpp
//
// Compilation home for the BrnDirector::Camera::CollisionPolicyAttachedToVehicle slice this TU
// owns:
//   - CollisionPolicyAttachedToVehicle::SetDesiredHeight @0x821F3950
//
// Called by BrnDirector::Camera::BehaviourGyroCam::Update to seed the policy's desired camera
// height above the tracked vehicle each frame.
// ============================================================================

#include "GameSource/Director/Camera/BrnCollisionPolicy.h"

namespace BrnDirector
{
namespace Camera
{

// ----------------------------------------------------------------------------
// BrnDirector::Camera::CollisionPolicyAttachedToVehicle::SetDesiredHeight @0x821F3950
//   li    r10, 1
//   stb   r10, 0x24B(this)     ; mbHaveDesiredHeight = 1   (set BEFORE the assert)
//   lfs   f0, flt_82001CC0     ; 0.0f
//   fcmpu cr6, f31, f0         ; lfDesiredHeight vs 0.0f
//   bgt   skip                 ; assert when NOT (lfDesiredHeight > 0.0f)
//   ... Begin/Fire/End assert "lfDesiredHeight > 0.0f" ...
//   stfs  f31, 0x210(this)     ; mfDesiredHeight = lfDesiredHeight
// ----------------------------------------------------------------------------
void CollisionPolicyAttachedToVehicle::SetDesiredHeight(f32 lfDesiredHeight)
{
    mbHaveDesiredHeight = 1;                                          // stb 1, 0x24B (before assert)
    CGS_ASSERT(lfDesiredHeight > 0.0f, "lfDesiredHeight > 0.0f");     // fcmpu f31, 0.0f; bgt
    mfDesiredHeight = lfDesiredHeight;                               // stfs f31, 0x210(this)
}

} // namespace Camera
} // namespace BrnDirector
