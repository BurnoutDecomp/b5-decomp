#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_ROAD_RUNNER_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_ROAD_RUNNER_H

#include "types.hpp"
#include "rw/math/vpu/types.h"                        // rw::math::vpu::Matrix44Affine / Vector3
#include "GameShared/GameClasses/Core/CgsAssert.h"    // CGS_ASSERT (the mbPrepared guards)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourRoadRunner.h
//
// BrnDirector::Camera::TrafficLaneTruck -- the "road runner" behaviour's truck: a small helper
// that carries a world-space transform plus its local angular / linear velocities, prepared once
// per shot and then sampled by BehaviourRoadRunner::Update. HOME for the TrafficLaneTruck slice
// this TU bodies (the three by-value sample accessors).
//
// ----------------------------------------------------------------------------
// All three accessors are by-value getters: the X360 passes the sret (output) slot in r3 and
// `this` in r4 (the lvx128 loads come from r4 / r30; the stvx128 stores go to r3 / r31). Each
// first asserts the truck has been prepared (!!mbPrepared) before sampling:
//   GetTransform            @0x821F53D8  copies the 64-byte transform (4x 16-byte rows @+0x20)
//   GetLocalAngularVelocity @0x821F5470  copies the 16-byte angular velocity (@+0x60)
//   GetLinearVelocity       @0x821F54E0  copies the 16-byte linear velocity  (@+0x70)
// The truck's full prepare/step logic lands with the road-runner behaviour TU; this header models
// only the slice the accessors need, BY NAME, at their asm-attested offsets.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

class TrafficLaneTruck
{
public:
    // Return (by value) the truck's world-space transform. Asserts the truck is prepared.
    // @0x821F53D8: four 16-byte aligned rows copied from +0x20 (lvx128 stride 16) into the sret.
    rw::math::vpu::Matrix44Affine GetTransform() const;

    // Return (by value) the truck's local angular velocity. Asserts the truck is prepared.
    // @0x821F5470: a single 16-byte aligned vector copied from +0x60 into the sret.
    rw::math::vpu::Vector3 GetLocalAngularVelocity() const;

    // Return (by value) the truck's linear velocity. Asserts the truck is prepared.
    // @0x821F54E0: a single 16-byte aligned vector copied from +0x70 into the sret.
    rw::math::vpu::Vector3 GetLinearVelocity() const;

    // FLAG: only the members the accessors read are modelled, at their asm-attested offsets; the
    //   rest of the truck rig (prepare/step working state) lands with the road-runner behaviour TU.
    //   Reserved spans place each sampled member at its attested offset. Members are public so the
    //   file-scope offsetof pins in the .cpp can verify them (all size-stable here -- no pointers
    //   intervene, so the layout is exact).
    u8                            maReserved00[0x20];          // +0x00 .. +0x1F  truck head (not modelled)
    rw::math::vpu::Matrix44Affine mTransform;                  // +0x20  world-space transform (64B)
    rw::math::vpu::Vector3        mLocalAngularVelocity;       // +0x60  local angular velocity (16B)
    rw::math::vpu::Vector3        mLinearVelocity;             // +0x70  linear velocity (16B)
    u8                            maReserved80[0x8C - 0x80];   // +0x80 .. +0x8B  (state not modelled)
    u8                            mbPrepared;                  // +0x8C  set once the truck is prepared
};

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_ROAD_RUNNER_H
