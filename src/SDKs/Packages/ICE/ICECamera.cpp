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
// Parameter shape: the DWARF's SetCameraMatrix(Matrix4*, float32_t). RESHAPED 2026-09-27
//   (OWNERLIST lane L5) from a single `const Matrix44Affine&`, now that its one caller --
//   ICECameraMover::UpdateFrameEnd @0x8253D988 -- is bodied: that caller passes its own
//   Matrix4 mWorldToCamera (`addi r4, r31, 0x120`) and the frame sim time in f1 (`fmuls f1,
//   f0, f31`), and this body reads only r4. The DWARF's local `Matrix44Affine lCamera` built
//   from the four rows is the copy the asm makes row by row.
//
// THE EIGHT MOVER SETTERS (2026-09-27, OWNERLIST lane L5 -- the pause camera). The DWARF homes
// every one of them in this file (references/DecFIGS/dwarfdump/SDKs/Packages/ICE/ICECamera.cpp
// :84 / :104 / :152 / :222 / :353 / :416 / :434 / :453); ARTIST keeps a symbol only for
// SetFadeColor @0x8252B500 and inlines the rest into the mover, so each body below is the store
// its inlined expansion makes (cited per function).
// ============================================================================

namespace
{
    // fsel d, t, x, y  ==  (t >= 0) ? x : y. A NaN test takes y; -0 counts as >= 0.
    inline f32 Fsel(f32 lfTest, f32 lfGreaterEqual, f32 lfLess)
    {
        return (lfTest >= 0.0f) ? lfGreaterEqual : lfLess;
    }

    // fctidz + stfiwx: the low 32 bits of the float truncated toward zero to a 64-bit integer. The
    // conversion saturates on PPC; the only inputs here are already clamped into [0, 255], and a NaN
    // cannot reach it (the clamp turns NaN into 255), so a plain truncation is the same word.
    inline u32 FctidzLow32(f32 lfValue)
    {
        return static_cast<u32>(static_cast<s64>(lfValue));
    }
}

namespace ICE
{
    rw::math::vpu::Matrix44Affine* ICECamera::SetCameraMatrix(Matrix4* lpMatrix, f32 /*lfTime*/)
    {
        // Pin the two X360-proven offsets (member fn -> has private access; type is
        // complete here). matrix copy lands at this+0x10, dirty-flags at this+0x150.
        static_assert(offsetof(ICECamera, mCamera) == 0x10,
                      "ICECamera transform (mCamera.mTransform) must land at this+0x10");
        static_assert(offsetof(ICECamera, mCamera) + 0x140 == 0x150,
                      "ICECamera dirty-flags word must land at this+0x150");

        // 1. Copy the take matrix into the director camera's transform (this+0x10): four
        //    lvx128/stvx128 row moves, 0x82531ADC..0x82531B08.
        rw::math::vpu::Matrix44Affine lCamera;
        rw::math::vpu::Vector3* const lapRows[4] = { &lCamera.xAxis, &lCamera.yAxis, &lCamera.zAxis, &lCamera.wAxis };
        for (s32 liRow = 0; liRow < 4; ++liRow)
        {
            lapRows[liRow]->x = lpMatrix->maRows[liRow].x;
            lapRows[liRow]->y = lpMatrix->maRows[liRow].y;
            lapRows[liRow]->z = lpMatrix->maRows[liRow].z;
            lapRows[liRow]->w = lpMatrix->maRows[liRow].w;
        }
        mCamera.mTransform = lCamera;

        // 2. Revalidate the freshly-installed transform; capture its result.
        rw::math::vpu::Matrix44Affine* lpValidated = mCamera.ValidateTransformWithDebugInfo();

        // 3. Mark the camera transform dirty: set bit1 of the state flags word
        //    (this+0x150). |= 2.
        mCamera.mState_uFlags |= 2;

        // 4. Forward the validate result (X360 asm; see return-type FLAG above).
        return lpValidated;
    }

    // ------------------------------------------------------------------------
    // SetSimTimeMultiplier (DWARF ICECamera.cpp:84). Inlined into ICECameraMover::UpdateSimTime:
    // `stfs f0, 0x114(r10)` @0x8252E49C, r10 = the camera -- ICECamera +0x114 == mCamera (+0x10)
    // .mEffects (+0x68) .mfSimTimeScale (+0x9C). The DWARF names the callee
    // CameraEffects::SetSimTimeScale.
    // ------------------------------------------------------------------------
    void ICECamera::SetSimTimeMultiplier(f32 lfTime)
    {
        mCamera.GetEffects().SetSimTimeScale(lfTime);
    }

    // ------------------------------------------------------------------------
    // SetFadeColor @0x8252B500 (DWARF ICECamera.cpp:104). Each channel is clamped into [0, 255]
    // (flt_82010C20 == 0x437F0000 == 255.0; flt_82001CC0 == 0.0) by two fsels -- the DWARF's
    // ICEMath::Clamp over rw::math::vpu::Max<float> / Min<float>:
    //     v = fsel(-x, 0.0, x)       0x8252B520..0x8252B52C   (Max(0, x): a NaN x stays NaN)
    //     v = fsel(255 - v, v, 255)  0x8252B534..0x8252B550   (Min(255, v): a NaN v becomes 255)
    // then truncated (fctidz / stfiwx, the DWARF's ICEMath::FloatToUInt) and packed ARGB with three
    // `insrwi rD, rS, 24, 0` (rD = rS << 8 | rD & 0xFF), alpha first:
    //     ((alpha << 8 | red) << 8 | green) << 8 | blue
    // and stored with `stw r10, 0x100(r3)` == mCamera.mEffects.muFadeColor (+0x88).
    // ------------------------------------------------------------------------
    void ICECamera::SetFadeColor(f32 lfRed, f32 lfGreen, f32 lfBlue, f32 lfAlpha)
    {
        const f32 KF_ZERO     = 0.0f;     // flt_82001CC0
        const f32 KF_MAX_BYTE = 255.0f;   // flt_82010C20

        const f32 lfAlphaLo = Fsel(-lfAlpha, KF_ZERO, lfAlpha);
        const f32 lfRedLo   = Fsel(-lfRed,   KF_ZERO, lfRed);
        const f32 lfGreenLo = Fsel(-lfGreen, KF_ZERO, lfGreen);
        const f32 lfBlueLo  = Fsel(-lfBlue,  KF_ZERO, lfBlue);

        const u32 luAlpha = FctidzLow32(Fsel(KF_MAX_BYTE - lfAlphaLo, lfAlphaLo, KF_MAX_BYTE));
        const u32 luRed   = FctidzLow32(Fsel(KF_MAX_BYTE - lfRedLo,   lfRedLo,   KF_MAX_BYTE));
        const u32 luGreen = FctidzLow32(Fsel(KF_MAX_BYTE - lfGreenLo, lfGreenLo, KF_MAX_BYTE));
        const u32 luBlue  = FctidzLow32(Fsel(KF_MAX_BYTE - lfBlueLo,  lfBlueLo,  KF_MAX_BYTE));

        u32 luColourARGB = (luAlpha << 8) | (luRed & 0xFFu);
        luColourARGB     = (luColourARGB << 8) | (luGreen & 0xFFu);
        luColourARGB     = (luColourARGB << 8) | (luBlue & 0xFFu);

        mCamera.GetEffects().SetFadeColor(luColourARGB);
    }

    // ------------------------------------------------------------------------
    // SetDepthOfField (DWARF ICECamera.cpp:152: Camera::GetDepthOfField + DepthOfField::SetParams).
    // Inlined into ICECameraMover::UpdateFocus: `addi r3, r10, 0x134` (ICECamera +0x134 == mCamera
    // +0x124 == mDepthOfField) then `bl DepthOfField::SetParams` @0x8252E748 / @0x8252E774 with the
    // five floats in f1..f5, in DepthOfField::SetParams' own order.
    // ------------------------------------------------------------------------
    void ICECamera::SetDepthOfField(f32 lfFocusStartDistanceMeters, f32 lfPerfectFocusStartDistanceMeters,
                                    f32 lfPerfectFocusEndDistanceMeters, f32 lfFocusEndDistanceMeters,
                                    f32 lfBlurriness)
    {
        mCamera.GetDepthOfField().SetParams(lfFocusStartDistanceMeters, lfPerfectFocusStartDistanceMeters,
                                            lfPerfectFocusEndDistanceMeters, lfFocusEndDistanceMeters,
                                            lfBlurriness);
    }

    // ------------------------------------------------------------------------
    // SetFieldOfView (DWARF ICECamera.cpp:222: Camera::SetFOV). Inlined into
    // ICECameraMover::UpdateLens: the "lfFOV > 0.0f" assert citing Camera.h:424 (0x8252E634..
    // 0x8252E658) and `stfs f31, 0x68(r31)` (ICECamera +0x68 == mCamera +0x58 == mfFOV) -- exactly
    // Camera::SetFOV's body.
    // ------------------------------------------------------------------------
    void ICECamera::SetFieldOfView(f32 lfScale)
    {
        mCamera.SetFOV(lfScale);
    }

    // ------------------------------------------------------------------------
    // SetBloom (DWARF ICECamera.cpp:353: Camera::GetEffects + CameraEffects::
    // SetBloomLuminanceModifier / SetBloomThresholdModifier). Inlined into
    // ICECameraMover::UpdateBloom: `stfs f0, 0x108(r11)` / `stfs f13, 0x10C(r11)` @0x8252E40C /
    // @0x8252E410 -- mEffects +0x90 mfBloomThreshold and +0x94 mfBloomLuminance.
    // ------------------------------------------------------------------------
    void ICECamera::SetBloom(f32 lfBloomThreshold, f32 lfBloomLuminance)
    {
        mCamera.GetEffects().SetBloomThresholdModifier(lfBloomThreshold);
        mCamera.GetEffects().SetBloomLuminanceModifier(lfBloomLuminance);
    }

    // ------------------------------------------------------------------------
    // SetOverlay / ClearOverlay (DWARF ICECamera.cpp:416 / :434). Inlined into
    // ICECameraMover::UpdateOverlay: `stw r3, 0(r11)` @0x8252E508 and `stw r7(=0), 0(r11)`
    // @0x8252E4FC, the camera's first member mICEOverlay.
    // ------------------------------------------------------------------------
    void ICECamera::SetOverlay(s32 liOverlay)
    {
        mICEOverlay.SetOverlay(liOverlay);
    }

    void ICECamera::ClearOverlay()
    {
        mICEOverlay.UnSetOverlay();
    }

    // ------------------------------------------------------------------------
    // SetHideOverlay (DWARF ICECamera.cpp:453, `SetHideOverlay(bool lbHide)`, onto the member
    // ICECamera.hpp:134 `bool mbShowOverlay`). Both of the console's inlined expansions store the
    // byte 1 into +0x04: ICECameraMover::Construct @0x8252B928 and ICECameraMover::UpdateOverlay
    // @0x8252E530 (`li r10, 1 ; stb r10, 4(r11)`).
    // FLAG (inferred, byte-exact either way): the body is modelled as mbShowOverlay = !lbHide, so
    // both mover sites pass false. ARTIST has no reader of +0x04 (no ICE overlay renderer is
    // linked), so only the stored byte is observable, and it is the console's.
    // ------------------------------------------------------------------------
    void ICECamera::SetHideOverlay(bool lbHide)
    {
        mbShowOverlay = !lbHide;
    }
}
