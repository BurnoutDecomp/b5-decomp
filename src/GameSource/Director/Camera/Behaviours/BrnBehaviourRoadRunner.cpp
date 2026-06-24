// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourRoadRunner.cpp
//
// Compilation home for the BrnDirector::Camera::TrafficLaneTruck accessor slice this TU owns:
//   - TrafficLaneTruck::GetTransform            @0x821F53D8
//   - TrafficLaneTruck::GetLocalAngularVelocity @0x821F5470
//   - TrafficLaneTruck::GetLinearVelocity       @0x821F54E0
//
// All three are by-value samplers (sret in r3, `this` in r4) read by BehaviourRoadRunner::Update.
// Each asserts the truck has been prepared before sampling, then performs a plain aligned copy of
// the requested member -- not a multi-stage VMX pipeline, so reproduced as by-name assignments.
// The truck's full prepare/step logic lands with the road-runner behaviour TU.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourRoadRunner.h"
#include <cstddef>   // offsetof

namespace BrnDirector
{
namespace Camera
{

// Pin the asm-attested member offsets the accessors sample.
static_assert(offsetof(TrafficLaneTruck, mTransform)            == 0x20, "transform @ +0x20");
static_assert(offsetof(TrafficLaneTruck, mLocalAngularVelocity) == 0x60, "angular velocity @ +0x60");
static_assert(offsetof(TrafficLaneTruck, mLinearVelocity)       == 0x70, "linear velocity @ +0x70");
static_assert(offsetof(TrafficLaneTruck, mbPrepared)            == 0x8C, "mbPrepared @ +0x8C");

// ----------------------------------------------------------------------------
// BrnDirector::Camera::TrafficLaneTruck::GetTransform @0x821F53D8
// Asserts the truck is prepared, then copies the 64-byte transform (four 16-byte aligned rows,
// lvx128/stvx128 at base this+0x20, stride 16) into the by-value return slot.
// ----------------------------------------------------------------------------
rw::math::vpu::Matrix44Affine TrafficLaneTruck::GetTransform() const
{
    CGS_ASSERT(mbPrepared, "mbPrepared");
    return mTransform;                                 // 4x lvx128 (this+0x20) -> 4x stvx128 (sret)
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::TrafficLaneTruck::GetLocalAngularVelocity @0x821F5470
// Asserts the truck is prepared, then copies the 16-byte local angular velocity (@this+0x60).
// ----------------------------------------------------------------------------
rw::math::vpu::Vector3 TrafficLaneTruck::GetLocalAngularVelocity() const
{
    CGS_ASSERT(mbPrepared, "mbPrepared");
    return mLocalAngularVelocity;                      // lvx128 (this+0x60) -> stvx128 (sret)
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::TrafficLaneTruck::GetLinearVelocity @0x821F54E0
// Asserts the truck is prepared, then copies the 16-byte linear velocity (@this+0x70).
// ----------------------------------------------------------------------------
rw::math::vpu::Vector3 TrafficLaneTruck::GetLinearVelocity() const
{
    CGS_ASSERT(mbPrepared, "mbPrepared");
    return mLinearVelocity;                            // lvx128 (this+0x70) -> stvx128 (sret)
}

} // namespace Camera
} // namespace BrnDirector
