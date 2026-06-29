// ============================================================================
// GameSource/Director/Camera/Camera.cpp
//
// Compilation home for the BrnDirector::Camera::Camera member functions this TU owns:
//   - Camera::Construct                    @0x82255E68  (default-init the camera)
//   - Camera::ValidateTransformWithDebugInfo @0x8220A850 (debug transform validation)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (offset/behaviour authority) + the DecFIGS
// DWARF (declaration shape). The X360 inlines the sub-block constructors (CameraState /
// DepthOfField / CameraEffects) into Construct as flat store runs; they are de-inlined
// here to the named sub-object Construct calls, matching the asm store set field-for-field.
//
// FLAG (assert machinery): both ValidateTransformWithDebugInfo asserts originally built a
//   dynamic message through CgsDev::StrStream over CgsDev::Assert::gpcMessageBuffer (the
//   off_82000D00 / BasePriorityQueue::Clear / off_82000D08 sink dance), streaming the
//   camera name from mpDebugInfoBehaviour->GetName() ("Camera name unknown" when null) and
//   a transform dump. Per the project rule that replaces the gpcMessageBuffer machinery with
//   CGS_ASSERT, the dynamic message build (and with it the debug-only mpDebugInfoBehaviour
//   name lookup, a virtual call reached only on the failure path) is folded to a static
//   assert message. The validity/position predicates -- the only non-debug side effects --
//   are preserved exactly.
// ============================================================================

#include "GameSource/Director/Camera/Camera.h"
#include "rw/math/vpu/vector3_operation.h"          // rw::math::vpu::IsValid / Abs (Vector3)
#include "rw/math/vpu/matrix44affine_operation.h"   // rw::math::vpu::IsValid (Matrix44Affine)
#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT (gpcMessageBuffer substitute)
#include <cstddef>                                  // offsetof

namespace BrnDirector
{
namespace Camera
{

// Pointer-size-independent facts the X360 asm pins (these hold on the x64 gate too).
// CameraEffects has no pointer members, so its 0xBC stride -- the gap the Construct asm
// proves between the camera's effects block (+0x68) and depth-of-field (+0x124) -- is the
// same on console and host. The transform stays at the head on both. (The interior member
// offsets that ride behind the 4-vs-8-byte pointer members are NOT asserted; parity there
// is by named member -- see Camera.h.)
static_assert(sizeof(CameraEffects) == 0xBC, "CameraEffects must be 0xBC (camera +0x68..+0x124)");
static_assert(offsetof(Camera, mTransform) == 0x00, "transform @ +0x00");

// ----------------------------------------------------------------------------
// BrnDirector::Camera::Camera::Construct @0x82255E68
//
// Default-construct the camera. The asm:
//   1. CameraState::Construct(this+0x138)               -> mState.Construct()
//   2. five stfs into the DOF sub-object at +0x124..+0x134 (0.1/0.2/0.3/0.4/0.0,
//      no range asserts) -> the inlined DepthOfField default-init -> mDepthOfField.Construct()
//   3. the inlined CameraEffects default-init (zeroes the hook-name heads, the motion-blur
//      block, the background-effect/fade/post-fx scalars, sets the two blend amounts at
//      +0x9C/+0xB0 to 1.0) over the +0x68 block -> mEffects.Construct()
//   4. tail-call Camera::Clear(this)                    -> Clear()
// (The asm interleaves 2 and 3 across the shared base register r11=this+0x68, but the
//  effects/DOF blocks are disjoint, so the de-inlined order is behaviourally identical.)
// ----------------------------------------------------------------------------
void Camera::Construct()
{
    mState.Construct();          // CameraState::Construct(this+0x138)
    mDepthOfField.Construct();   // inlined DOF default-init (+0x124..+0x134)
    mEffects.Construct();        // inlined CameraEffects default-init (+0x68 block)
    Clear();                     // tail call -> Camera::Clear(this)
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::Camera::ValidateTransformWithDebugInfo @0x8220A850
//
// Debug-build transform validation, run after each transform install. Two checks:
//   1. Every row of mTransform is finite (the asm runs a per-row, per-lane vcmpeqfp
//      self-equality NaN test over xAxis/yAxis/zAxis/wAxis, ANDs the four results) ->
//      IsValid(mTransform). If invalid, fire "Camera has invalid transform".
//   2. The position (mTransform.Pos(), the wAxis row) is "reasonable": the asm Abs's the
//      row (vandc against the sign-bit mask) and compares each xyz lane against a cached
//      broadcast 1,000,000 (vcmpgefp); a lane >= 1,000,000 makes the position unreasonable
//      -> fire "Camera has unreasonable position". (The broadcast constant is a function-
//      local one-time-initialised static -- the dword_82FAAD20 & 1 guard; modelled as the
//      static sOneMillion below.)
//
// Returns the validated-transform pointer the X360 forwards to ICECamera::SetCameraMatrix.
// The asm's r3/r24 ("result") is the camera `this`, and the transform sits at this+0x00,
// so the validated-transform pointer is &mTransform. (On a failing assert the X360 returns
// EndAssert()'s pointer instead; that is part of the dropped gpcMessageBuffer machinery, so
// the validated-transform pointer is returned unconditionally here -- see the file FLAG.)
// ----------------------------------------------------------------------------
rw::math::vpu::Matrix44Affine* Camera::ValidateTransformWithDebugInfo()
{
    // 1. No NaN/Inf in any transform row.
    CGS_ASSERT(rw::math::vpu::IsValid(mTransform),
               "Camera has invalid transform, originated from: ");

    // 2. Position within +/- 1,000,000 on every axis. One-time-initialised broadcast
    //    bound (the X360 dword_82FAAD20 & 1 cache guard).
    static const rw::math::vpu::Vector3 sOneMillion = { 1000000.0f, 1000000.0f, 1000000.0f, 1000000.0f };
    const rw::math::vpu::Vector3 lAbsPosition = rw::math::vpu::Abs(mTransform.Pos());
    CGS_ASSERT(lAbsPosition.x < sOneMillion.x
            && lAbsPosition.y < sOneMillion.y
            && lAbsPosition.z < sOneMillion.z,
               "Camera has unreasonable position, originated from: ");

    return &mTransform;
}

} // namespace Camera
} // namespace BrnDirector
