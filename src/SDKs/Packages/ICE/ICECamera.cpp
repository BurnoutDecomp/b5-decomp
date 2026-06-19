#include "SDKs/Packages/ICE/ICECamera.hpp"
#include "GameSource/Director/Camera/Camera.h"   // BrnDirector::Camera::Camera
#include "rw/math/vpu/types.h"                    // rw::math::vpu::Matrix44Affine

// ============================================================================
// SDKs/Packages/ICE/ICECamera.cpp
//
// ICE::ICECamera::SetCameraMatrix @0x82531AC8 -- install a new take matrix on the
// ICE camera. From the X360 asm:
//   1. Copy the 64-byte Matrix44Affine arg into the director camera's transform at
//      this+0x10 (== mCamera.mTransform). The asm does this as four lvx128/stvx128
//      pairs over 16-byte rows at offsets 0x10/0x20/0x30/0x40 of `this` -- i.e. a
//      whole-matrix copy. Reconstructed as the structured value assignment.
//   2. result = mCamera.ValidateTransformWithDebugInfo()  (bl ...; the call is on
//      the camera sub-object at this+0x10).
//   3. Mark the camera dirty: OR bit1 (|=2) into the flags word at this+0x150
//      (== mCamera.mState_uFlags). The DWARF renders this as the
//      BrnDirector::Camera::CameraState::SetFlag / CgsContainers::BitArray::
//      SetBitToBool pair; the asm is a plain `ld r11,0x150(r31); ori r11,r11,2;
//      std r11,0x150(r31)`, so we model it as the OR on the named flags member.
//   4. return result.
//
// FLAG: return type. The DWARF (PS3-internal build) declares both
//   ICECamera::SetCameraMatrix and Camera::ValidateTransformWithDebugInfo as
//   `void` (ICECamera.hpp:123 / Camera.h:75). The X360 build (the spine, offset
//   authority) instead CAPTURES ValidateTransformWithDebugInfo's return in r3 and
//   forwards it as SetCameraMatrix's own return (pseudocode:
//   `result = ...ValidateTransformWithDebugInfo(this+0x10); ...; return result;`).
//   Reconstructed faithfully to the X360 asm: Validate returns the validated-
//   transform pointer and SetCameraMatrix forwards it. Both home declarations were
//   shaped to match (pointer-returning) so the .cpp compiles against them. Revisit
//   if the camera's own X360-attested TU proves a different return.
//
// FLAG: parameter shape. The DWARF declares SetCameraMatrix(Matrix4*, float32_t)
//   (a pointer + a float). The X360 asm uses only r4 (the matrix pointer) and never
//   reads a second/float argument, so the second parameter is unused on this spine.
//   Reconstructed with the single `const Matrix44Affine&` the asm actually consumes
//   (matching the task's SetCameraMatrix(const Matrix44Affine&) shape).
// ============================================================================

namespace ICE
{
    rw::math::vpu::Matrix44Affine* ICECamera::SetCameraMatrix(const rw::math::vpu::Matrix44Affine& lrMatrix)
    {
        // Pin the two X360-proven offsets (member fn -> has private access; type is
        // complete here). matrix copy lands at this+0x10, dirty-flags at this+0x150.
        static_assert(offsetof(ICECamera, mCamera) == 0x10,
                      "ICECamera transform (mCamera.mTransform) must land at this+0x10");
        static_assert(offsetof(ICECamera, mCamera) + 0x140 == 0x150,
                      "ICECamera dirty-flags word must land at this+0x150");

        // 1. Copy the take matrix into the director camera's transform (this+0x10).
        mCamera.mTransform = lrMatrix;

        // 2. Revalidate the freshly-installed transform; capture its result.
        rw::math::vpu::Matrix44Affine* lpValidated = mCamera.ValidateTransformWithDebugInfo();

        // 3. Mark the camera transform dirty: set bit1 of the state flags word
        //    (this+0x150). |= 2.
        mCamera.mState_uFlags |= 2;

        // 4. Forward the validate result (X360 asm; see return-type FLAG above).
        return lpValidated;
    }
}
