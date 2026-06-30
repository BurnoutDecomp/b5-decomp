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
// Also the HOME for a minimal slice of BrnDirector::Camera::BehaviourRoadRunner -- the "road
// runner"/picture-paradise fly-by camera behaviour itself. The crash-nav arbitrator state
// (BrnArbStateCrashNav) drives it through its handle each frame, so the named accessors it
// touches are modelled here BY NAME at their asm-attested offsets (see the slice below). The
// full behaviour (Construct/Prepare/Update + the rest of the rig) and its Behaviour base land
// with their own TUs.
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

// The director camera the behaviour produces each frame (its base Behaviour's output camera).
// Forward-declared so the produced-camera accessor below can name it; consumers that copy the
// produced camera #include Camera.h themselves.
struct Camera;

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourRoadRunner -- minimal slice (FLAG: no full reconstructed home
// yet; the full behaviour rig lands with its own TU). HOME here only for the named accessors the
// crash-nav arbitrator state drives on the live behaviour each frame; the members below are the
// ONLY ones those accessors touch, modelled at their asm-attested offsets (pinned from
// BrnArbStateCrashNav::Update @0x8226DC98). Reserved byte spans place each member exactly.
//   * mbDontSmoothNextSample (+0x08): cleared on a turnabout so the next sample is not smoothed.
//   * mbHasFinished          (+0x09): the behaviour's "finished playing" flag.
//   * mfReverseLaneA         (+0xA0): a velocity/heading lane the turnabout negates.
//   * mfReverseLaneB         (+0x294) / mfReverseLaneC (+0x29C): the other two negated lanes.
//   * mbShouldFadeBlackIn    (+0x2B4): selects the "Black_In_BW" vs "Black_Out_BW" fade.
// The produced camera is reached via the handle helper (the same camera the behaviour writes
// each frame); modelled BY NAME as GetProducedCamera(). All accessors are DECLARATION-ONLY here
// except the trivial flag/turnabout pokes, which the crash-nav .cpp drives by name.
// ----------------------------------------------------------------------------
class BehaviourRoadRunner
{
public:
    // The camera this behaviour produced this frame (the X360 reaches it through the handle
    // helper sub_821FDC58, which resolves the behaviour's produced camera). The crash-nav state
    // copies it into its own mCamera. DECLARATION-ONLY (body lands with the full behaviour TU).
    const Camera& GetProducedCamera() const;

    // The behaviour's "finished" flag (lbz +0x09). Crash-nav keeps copying the behaviour camera
    // while !HasFinished().
    bool HasFinished() const { return mbHasFinished != 0; }

    // Whether the fade this take wants is the black-IN variant (lbz +0x2B4). Crash-nav requests
    // "Black_In_BW" when set, "Black_Out_BW" otherwise.
    bool ShouldFadeBlackIn() const { return mbShouldFadeBlackIn != 0; }

    // Turnabout (X360 ACTIVE_TURNABOUT case, asm @0x8226DF7C): clear the "don't smooth the next
    // sample" gate (stb 0, +0x08), then negate the three reverse lanes so the fly-by reverses its
    // travel direction.
    void ClearSmoothingForNextSample() { mbDontSmoothNextSample = 0; }      // stb 0, +0x08
    void ReverseTravelDirection()
    {
        // asm: fmuls each by -1.0 (flt_820037C8 == -1.0) and store back. Order matches the asm
        // (loads +0x29C/+0x294/+0xA0, stores +0x29C then +0x294 then +0xA0).
        mfReverseLaneC = mfReverseLaneC * -1.0f;   // +0x29C
        mfReverseLaneB = mfReverseLaneB * -1.0f;   // +0x294
        mfReverseLaneA = mfReverseLaneA * -1.0f;   // +0xA0
    }

private:
    // FLAG: only the members the crash-nav accessors touch are modelled, at their asm-attested
    //   offsets; the rest of the road-runner rig lands with the full behaviour TU. The vtable/base
    //   head occupies +0x00.
    void* mpVTable;                              // +0x00   behaviour vtable (opaque base head)
    u8    mbDontSmoothNextSample;                // +0x08   cleared on a turnabout
    u8    mbHasFinished;                         // +0x09   "finished playing" flag
    u8    maReserved0A[0xA0 - 0x0A];             // +0x0A .. +0x9F (rig members not modelled)
    f32   mfReverseLaneA;                        // +0xA0   negated on turnabout
    u8    maReservedA4[0x294 - 0xA4];            // +0xA4 .. +0x293 (rig members not modelled)
    f32   mfReverseLaneB;                        // +0x294  negated on turnabout
    u8    maReserved298[0x29C - 0x298];          // +0x298 .. +0x29B (rig member not modelled)
    f32   mfReverseLaneC;                        // +0x29C  negated on turnabout
    u8    maReserved2A0[0x2B4 - 0x2A0];          // +0x2A0 .. +0x2B3 (rig members not modelled)
    u8    mbShouldFadeBlackIn;                   // +0x2B4  fade-direction selector
};

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_ROAD_RUNNER_H
