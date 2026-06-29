#pragma once

// ============================================================================
// GameSource/Director/Camera/Camera.h
//
// BrnDirector::Camera::Camera -- the director's per-frame camera state: the take
// transform, subject, FOV/aspect, the effects / depth-of-field / dirty-flag-state
// sub-blocks, the shot selection, and the near-clip / running-time scalars.
//
// This is the type's OWN ledger TU home (Camera.cpp): the layout is now recovered
// in DWARF member order with NAMED members (it was previously a minimal slice with
// reserved spans, grown for the ICE movie-player / SetCameraMatrix consumers). The
// offsets are pinned by the X360 Camera::Construct asm @0x82255E68 (which stores
// into +0x68..+0x134 and calls CameraState::Construct on the state sub-object at
// +0x138) and the ICE::ICECamera::SetCameraMatrix asm @0x82531AC8 (transform at the
// camera's +0x00, dirty-flags word at +0x140).
//
// Embedded BY VALUE in ICE::ICECamera (mCamera @+0x10) -> transform lands at
// ICECamera +0x10, flags at +0x150. Also embedded by value in the RaceCar IO headers
// (a complete sized blob suffices there).
//
// COMPAT: ICE::ICECamera::SetCameraMatrix (committed) OR-pokes the dirty-flags word
// as `mCamera.mState_uFlags |= 2`. That word is the low 32 bits of the camera-state
// current-flag set (CameraState +0x08 == camera +0x140). It is exposed here, without
// duplicating storage, via an anonymous union that overlays the real `CameraState
// mState` sub-object with a `mState_uFlags` alias at the same +0x140 -- so both the
// named sub-object (Construct calls `mState.Construct()`) and the committed
// `mState_uFlags` access resolve to the same memory.
//
// alignas(16): the camera carries Matrix44Affine + Vector3 (SIMD).
// ============================================================================

#include "types.hpp"
#include "rw/math/vpu/types.h"                            // rw::math::vpu::Matrix44Affine / Vector3
#include "GameSource/Director/Camera/BrnCameraEffects.h"  // BrnDirector::Camera::CameraEffects (by value)
#include "GameSource/Director/Camera/BrnDepthOfField.h"   // BrnDirector::Camera::DepthOfField (by value)
#include "GameSource/Director/Camera/BrnCameraState.h"    // BrnDirector::Camera::CameraState (by value)
#include <cstddef>                                        // offsetof

namespace Attrib { struct RefSpec; }

namespace BrnDirector
{
    // Forward decls for the pointer members (declaration-only; the camera never
    // dereferences these in this TU beyond the debug-behaviour name lookup).
    struct CrashAnalysis;

    namespace Camera
    {
        // The camera behaviour base (debug-info behaviour pointer). Forward-declared:
        // only its address is stored; the (debug-only) virtual name lookup is folded
        // away with the assert machinery (see Camera.cpp).
        class Behaviour;

        // DWARF: Camera.h:40 (struct BrnDirector::Camera::Camera).
        struct alignas(16) Camera
        {
            // DWARF Camera.h:185 -- the per-frame "which shot is selected" record.
            struct ShotSelectionInfo
            {
                s32 miType;   // Camera.h:189
                s32 miId;     // Camera.h:190

                void Clear();  // Camera.h:187 (declaration-only; body in its own TU)
            };

            // DWARF Camera.h:43: `typedef const Attrib::RefSpec ShotReference;`
            typedef const Attrib::RefSpec ShotReference;

            // X360-attested @0x8220A850 (DWARF Camera.h:75). Validates the transform
            // (asserts on NaN / unreasonable position) and returns the validated-
            // transform pointer that SetCameraMatrix forwards (X360 asm; see the
            // return-type note in ICECamera.cpp). Body: Camera.cpp.
            rw::math::vpu::Matrix44Affine* ValidateTransformWithDebugInfo();

            // X360-attested @0x82255E68. Body: Camera.cpp.
            void Construct();

            // FLAG: minimal-slice decl used by the ICE movie-player family. Body lands
            // with this TU's Clear (@0x8223CE70, a separate function); declaration-only.
            void Clear();

            // ---- Layout (DWARF member order; offsets X360-pinned) -----------------
            rw::math::vpu::Matrix44Affine mTransform;        // +0x00  (64B)
            rw::math::vpu::Vector3        mSubject;          // +0x40  (16B)
            const Behaviour*              mpDebugInfoBehaviour; // +0x50  (read by ValidateTransform)
            ShotReference*                mpSourceShot;      // +0x54
            f32                           mfFOV;             // +0x58
            f32                           mfAspectRatio;     // +0x5C
            f32                           mfRunningTime;     // +0x60
            const CrashAnalysis*          mpCrashAnalysis;   // +0x64
            CameraEffects                 mEffects;          // +0x68  (0xBC -> ends +0x124)
            DepthOfField                  mDepthOfField;     // +0x124 (0x14 -> ends +0x138)

            // +0x138: the dirty-flag double buffer. Overlaid with the committed
            // `mState_uFlags` alias (== mState's current-flag low word @ camera +0x140)
            // so ICECamera's `mCamera.mState_uFlags |= 2` keeps resolving to the same
            // storage.
            union
            {
                CameraState mState;                          // +0x138 (0x18)
                struct
                {
                    u8  maStateHead[0x08];                   // +0x138 (CameraState head)
                    s32 mState_uFlags;                       // +0x140 (current-flag low word)
                };
            };

            f32              mfCustomNearClipDistance;       // +0x150
            ShotSelectionInfo mShotSelectionInfo;            // +0x154 (8B)
            bool             mbHasSubject;                   // +0x15C
            bool             mbHasCustomNearClipDistance;    // +0x15D
        };

        // Pin the one pointer-size-independent offset (the transform at the head, which
        // the ICE matrix copy targets). The interior member offsets quoted in the comments
        // above are the CONSOLE (32-bit, 4-byte-pointer) offsets the X360 asm proves; on the
        // x64 compile-gate host the pointer members (mpDebugInfoBehaviour / mpSourceShot /
        // mpCrashAnalysis) widen to 8 bytes, so those interior offsets shift -- parity here
        // is BY NAMED MEMBER, not by byte offset (the x64-gate rule). The transform stays at
        // +0x00 on both, and the dirty-flags word stays NAMED `mState_uFlags` (== the low
        // word of mState's current-flag set) for the committed ICECamera consumer.
        static_assert(offsetof(Camera, mTransform) == 0x00,
                      "BrnDirector::Camera::Camera transform must be at +0x00");
    }
}
