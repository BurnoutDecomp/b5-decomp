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

#include "BrnCommonTypes.h"   // Vector3
#include "GameShared/GameClasses/Graphics/CgsCamera.h"   // CgsGraphics::Camera
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
            // ---- near/far-clip distance class constants (DWARF Camera.h:200-202) ----
            // Static data members the camera's near-clip selection reads. GetNearClipDistance
            // returns KF_SMALL_NEAR_CLIP_DISTANCE when the camera-state "small near clip"
            // flag (mState_uFlags & 0x10000) is set, else KF_DEFAULT_NEAR_CLIP_DISTANCE.
            // FLAG (un-recovered rodata): the X360 leaf floats (flt_82CDA55C /
            // flt_82CDA560 / the far-clip constant) are NOT in any available rodata dump,
            // so their VALUES are not reconstructed here. They are defined in Camera.cpp
            // with a flagged placeholder (see the FLAG there) -- the SELECTION logic in
            // GetNearClipDistance is fully X360-attested; only the two leaf magnitudes are
            // unknown. Do NOT treat the placeholder magnitudes as ground truth.
            static const f32 KF_SMALL_NEAR_CLIP_DISTANCE;    // Camera.h:200 (flt_82CDA55C)
            static const f32 KF_DEFAULT_NEAR_CLIP_DISTANCE;  // Camera.h:201 (flt_82CDA560)
            static const f32 KF_DEFAULT_FAR_CLIP_DISTANCE;   // Camera.h:202
            // DWARF Camera.h:185 -- the per-frame "which shot is selected" record.
            struct ShotSelectionInfo
            {
                s32 miType;   // Camera.h:189
                s32 miId;     // Camera.h:190

                void Clear();  // Camera.h:187 (declaration-only; body in its own TU)
            };

            // DWARF Camera.h:43: `typedef const Attrib::RefSpec ShotReference;`
            typedef const Attrib::RefSpec ShotReference;

            // Default-constructible (the camera is brought up via Construct(), and is
            // embedded by value in several owners that zero/Construct it themselves).
            Camera() = default;

            // X360-attested @0x821F3B88. The copy constructor -- a flat memberwise copy of
            // the whole camera (body: Camera.cpp). Declared explicitly so the X360-attested
            // copy has a named home; defaulting it keeps the default ctor available too.
            Camera(const Camera& lrOther);

            // BrnDirector::Camera::Camera::operator= -- the copy-assignment counterpart of the
            // X360-attested copy constructor above (`Camera::Camera::operator_` in the ARTIST
            // pseudocode; every arbitrator-state Update copies a behaviour-produced camera into
            // its live `mCamera` with it, e.g. `lrCamera = mSomeHandle.GetProducedCamera();`).
            // No separate asm export exists for it in the available dumps (Hex-Rays elides
            // trivial memberwise-copy specials), but it is the same flat field-by-field copy as
            // the copy constructor by construction (a POD-of-PODs aggregate with no owning
            // resources), so the compiler-generated default is exact parity. Declared explicitly
            // (rather than left implicit) so it has a named, documented home.
            Camera& operator=(const Camera& lrOther) = default;

            // X360-attested @0x8220A850 (DWARF Camera.h:75). Validates the transform
            // (asserts on NaN / unreasonable position) and returns the validated-
            // transform pointer that SetCameraMatrix forwards (X360 asm; see the
            // return-type note in ICECamera.cpp). Body: Camera.cpp.
            rw::math::vpu::Matrix44Affine* ValidateTransformWithDebugInfo();

            // X360-attested @0x82255E68. Body: Camera.cpp.
            void Construct();

            // ---- ADDITIVE (WorldModule::GenerateFrustumQueries @0x827DADF8 copies
            //      the director's frame camera into the graphics camera the scene
            //      frustum queries are built from). Declaration-only; body with the
            //      director-camera TU. ----
            void CopyToCgsCamera( CgsGraphics::Camera* lpOutCamera ) const;

            // ---- ADDITIVE (WorldModule::GenerateDispatchLists @0x827D1CE8) ----
            // The camera flag word's junkyard bit (X360 +0x140 & 0x400000) and the
            // frame LOD zoom factor. Declaration-only; bodies with the camera TU.
            bool IsInJunkyard() const;
            f32  GetLodZoomFactor() const;
            // The camera's world position / view direction rows (X360 +48 / +32).
            Vector3 GetPosition() const;
            Vector3 GetDirection() const;

            // ---- effect-request helpers the arbitrator states drive -----------------------
            // These write the camera's mEffects request sub-block. The X360 inlines the field
            // writes into the calling state (ArbStateRoaming::ProcessPossibleFX); de-inlined
            // here to named Camera operations so callers never poke mEffects by offset.
            // DECLARATION-ONLY (bodies land with the Camera / CameraEffects TU); the precise
            // mEffects offsets are this type's own concern. FLAG: the committed mEffects field
            // layout is nominal past +0x44 -- these setters are the offset-authoritative API.

            // Set the per-frame impact-shake (amplitude, frequency, shake-type id).
            void SetImpactShake(f32 lfAmplitude, f32 lfFrequency, u8 lu8ShakeType);
            // Multiply the live shake amplitude (used to decay it when the requested amount
            // is non-positive).
            void ScaleImpactShake(f32 lfScale);
            // Set the showtime-intro motion-blur + black-bars request amounts.
            void SetShowtimeBlurAndBars(f32 lfBlur, f32 lfBars, bool lbHasStart, bool lbHasStop);
            // Set the requested post-FX (full-screen) amount.
            void SetRequestedPostFX(f32 lfAmount);

            // ---- crash-mode effect requests (BrnArbStateCrashMode::Update / ::DoCloseup) ----
            // These four de-inline the per-frame motion-blur / borders post-FX / time-dilation
            // pokes the crash-mode arbitrator state makes into mEffects, so that state never
            // touches the effects block by offset. DECLARATION-ONLY (bodies land with the Camera /
            // CameraEffects TU); the mEffects offsets quoted are this type's own concern.

            // Request the per-frame motion-blur amounts (the two motion-blur fields at mEffects
            // +0x44 / +0x48, with both blur-enable flags at +0x4C / +0x4D set true).
            void RequestMotionBlur(f32 lfBlurAmountA, f32 lfBlurAmountB);

            // Request the motion-blur "shake/strobe" overlay (mEffects +0xAC amount, +0xB0 set to
            // 1.0, the shake-type byte at +0xB4). Only issued while mbBlurShake is set.
            void RequestMotionBlurShake(f32 lfAmount, f32 lfBlend, u8 lu8ShakeType);

            // Set the requested full-screen border post-FX amount (mEffects +0xA8). Crash mode
            // drives it from its borders timer.
            void SetRequestedBorderPostFX(f32 lfAmount);

            // Set the game time-dilation (slow-mo) factor the camera requests this frame (mEffects
            // +0x9C). Crash mode requests its close-up / impact-time slow-mo through this.
            void SetRequestedTimeDilation(f32 lfTimeScale);

            // ---- crash-nav (picture-paradise) effect request (BrnArbStateCrashNav::Update) ------
            // Clear the crash-nav effect gate byte the crash-nav state zeroes on its hand-off to
            // WAITING_TO_STOP (X360 stb 0 at mEffects +0xB7 == camera +0x11F). DECLARATION-ONLY
            // (body lands with the Camera / CameraEffects TU); the mEffects offset is this type's
            // own concern. FLAG: the +0xB7 byte's precise role is not recovered (modelled as a
            // crash-nav effect gate; the byte WRITE is asm-attested).
            void ClearCrashNavEffectGate();

            // FLAG: minimal-slice decl used by the ICE movie-player family. Body lands
            // with this TU's Clear (@0x8223CE70, a separate function); declaration-only.
            void Clear();

            // ---- FOV / transform / depth-of-field accessors --------------------------------
            // The look-at + zoom post-process (BrnLooker) reads/writes the camera FOV, reads
            // the live transform, and drives the depth-of-field band through these. The X360
            // inlines them as direct field loads/stores (mfFOV @+0x58, mTransform @+0x00,
            // mDepthOfField @+0x124); de-inlined to named Camera operations so BrnLooker never
            // pokes those fields by offset. DECLARATION-ONLY here (bodies land with the Camera
            // TU; the per-TU `cl /c` gate does not link). SetFOV asserts the FOV stays > 0
            // (Camera.h:424 in the X360 source). FLAG: these are Camera's own offset-authoritative
            // API; declared additively for the BrnLooker consumer.
            f32  GetFOV() const;
            void SetFOV(f32 lfFOV);

            // X360-attested @0x82205B68 (DWARF Camera.h:174). The active near-clip distance:
            // the per-camera custom override when one is set, else the small/default constant
            // selected by the camera-state flag. Body: Camera.cpp. (CameraInterpolationController
            // ::Update reads it.)
            f32  GetNearClipDistance() const;

            const rw::math::vpu::Matrix44Affine& GetTransform() const;
            void SetTransform(const rw::math::vpu::Matrix44Affine& lrTransform);

            // The per-frame effects block (DWARF-named accessor: the PS3 hint for
            // PerlinShakeController::Update calls Camera::GetEffects() then
            // CameraEffects::GetShakeAmplitude()).
            const CameraEffects& GetEffects() const { return mEffects; }
            CameraEffects&       GetEffects()       { return mEffects; }
            DepthOfField& GetDepthOfField();
            const DepthOfField& GetDepthOfField() const;

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
