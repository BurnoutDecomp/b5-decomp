#pragma once

// MINIMAL SLICE of BrnDirector::Camera::Camera (full layout reconstructed by this
// type's own TU; DWARF home GameSource/Director/Camera/Camera.h:40). Grown in place
// from the earlier 256-byte opaque payload (RaceCarEntityModuleIO IO-buffer unlock)
// to expose the two offsets the X360 ICE::ICECamera::SetCameraMatrix asm
// (@0x82531AC8) proves: the take transform at the camera's +0x00 and the dirty-flags
// word at the camera's +0x140. Embedded BY VALUE in ICE::ICECamera (mCamera @+0x10),
// so ICECamera lands matrix@+0x10 / flags@+0x150.
//
// Opaque camera payload also embedded BY VALUE in RaceCarEntityModuleIO::
// InputBuffer_PreScene / _GenerateDispatchLists (mCameraInput); those IO headers only
// return &member / copy into member, so a complete sized blob suffices there too.
//
// DWARF (Camera.h:40) member order (offsets NOT given by DWARF; the two pinned ones
// below come from the X360 asm, the rest stay an opaque reserved cascade):
//   Matrix44Affine mTransform;            // +0x00  (64B, four 16B SIMD rows)
//   Vector3 mSubject; const Behaviour* mpDebugInfoBehaviour; ShotReference* mpSourceShot;
//   float32_t mfFOV/mfAspectRatio/mfRunningTime; const CrashAnalysis* mpCrashAnalysis;
//   CameraEffects mEffects; DepthOfField mDepthOfField;
//   CameraState mState;                   // dirty-flags live here; SetCameraMatrix
//                                         // OR-sets bit1 of the word at camera +0x140
//   float32_t mfCustomNearClipDistance; ShotSelectionInfo mShotSelectionInfo;
//   bool mbHasSubject; bool mbHasCustomNearClipDistance;
// CameraEffects/DepthOfField/CameraState/ShotSelectionInfo are Director-internal
// cascades, collapsed to opaque reserved spans here; the real members (and the rest
// of the method set) land with this type's own ledger TU.
//
// alignas(16): the camera carries Matrix44Affine + Vector3 (SIMD).

#include "types.hpp"
#include "rw/math/vpu/types.h"   // rw::math::vpu::Matrix44Affine
#include <cstddef>               // offsetof

namespace BrnDirector
{
    namespace Camera
    {
        // DWARF: Camera.h:40 (struct BrnDirector::Camera::Camera). The director's
        // per-frame camera state (transform, FOV, effects, shot selection).
        struct alignas(16) Camera
        {
            // +0x00: the take transform. X360 SetCameraMatrix copies four 16-byte
            // rows here via lvx128/stvx128 at stride 16.
            rw::math::vpu::Matrix44Affine mTransform;

            // +0x40 .. +0x140: the opaque Director-internal cascade (mSubject, the
            // behaviour/shot/analysis pointers, FOV/aspect/running-time, mEffects,
            // mDepthOfField, and the head of mState). NOMINAL span -- the real members
            // land with this type's own TU. 0x100 bytes.
            unsigned char maReserved0[0x100];

            // +0x140: the camera-state dirty-flags word (inside mState). The X360
            // SetCameraMatrix sets bit1 (|=2) here after validating the transform;
            // modelled as the BrnDirector::Camera::CameraState::SetFlag target.
            s32 mState_uFlags;

            // +0x144 .. tail: remainder of mState + the trailing scalars/flags,
            // 16-byte aligned. NOMINAL.
            unsigned char maReserved1[0xC];

            // X360-attested @0x8220A850 (DWARF Camera.h:75: `void ... const`).
            // DECLARATION-ONLY -- the per-TU `cl /c` gate does not link. See the
            // return-type note in ICECamera.cpp; reconstructed here as returning the
            // validated-transform pointer that SetCameraMatrix forwards (X360 asm),
            // not the DWARF's PS3-internal `void`.
            rw::math::vpu::Matrix44Affine* ValidateTransformWithDebugInfo();
        };

        // Pin the two X360-proven offsets.
        static_assert(offsetof(Camera, mTransform) == 0x00,
                      "BrnDirector::Camera::Camera transform must be at +0x00");
        static_assert(offsetof(Camera, mState_uFlags) == 0x140,
                      "BrnDirector::Camera::Camera dirty-flags word must be at +0x140");
    }
}
